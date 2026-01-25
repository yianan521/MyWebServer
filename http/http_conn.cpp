#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> users;

void http_conn::initmysql_result(connection_pool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

//对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
        std::cout << "DEBUG: addfd() called - epollfd=" << epollfd << ", fd=" << fd << std::endl;
    
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    
    std::cout << "DEBUG: Calling epoll_ctl()" << std::endl;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

//关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

//初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)
{
        std::cout << "DEBUG: http_conn::init() - Starting init for fd=" << sockfd << std::endl;
    std::cout << "DEBUG: m_epollfd=" << m_epollfd << std::endl;
    std::cout << "DEBUG: root pointer=" << (void*)root << std::endl;
    
    m_sockfd = sockfd;
    m_address = addr;
    m_TRIGMode = TRIGMode;
    
    std::cout << "DEBUG: Before addfd()" << std::endl;
    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    std::cout << "DEBUG: After addfd()" << std::endl;
    
    m_user_count++;

    doc_root = root;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    std::cout << "DEBUG: Before calling internal init()" << std::endl;
    init();
    std::cout << "DEBUG: After init()" << std::endl;
}

//初始化新接受的连接
//check_state默认为分析请求行状态
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    //LT读取数据
    if (0 == m_TRIGMode)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if (bytes_read <= 0)
        {
            return false;
        }

        return true;
    }
    //ET读数据
    else
    {
        while (true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}

//解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，显示判断界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    std::cout << "DEBUG: do_request() called, m_url=" << (m_url ? m_url : "null") << std::endl;
    
    if (!doc_root || !m_url) {
        std::cout << "ERROR: doc_root or m_url is null!" << std::endl;
        return INTERNAL_ERROR;
    }
    
    // 清空并复制文档根目录
    memset(m_real_file, '\0', FILENAME_LEN);
    strncpy(m_real_file, doc_root, FILENAME_LEN - 1);
    
    // 获取最后一个 '/' 的位置
    const char *p = strrchr(m_url, '/');
    if (!p) {
        std::cout << "ERROR: No '/' found in URL: " << m_url << std::endl;
        return BAD_REQUEST;
    }
    
    char next_char = *(p + 1); // p+1 指向 '/' 后面的字符
    
    // 处理CGI请求（登录/注册）
    if (cgi == 1 && (next_char == '2' || next_char == '3')) {
        std::cout << "DEBUG: Processing CGI request" << std::endl;
        
        // 检查数据库连接
        if (!mysql) {
            std::cout << "ERROR: MySQL connection is null!" << std::endl;
            if (next_char == '3') {
                strcpy(m_url, "/registerError.html");
            } else {
                strcpy(m_url, "/logError.html");
            }
        } else {
            // 提取用户名和密码
            char name[100] = {0};
            char password[100] = {0};
            
            // 解析 POST 数据
            if (m_string) {
                std::cout << "DEBUG: Raw POST data: [" << m_string << "]" << std::endl;

                // 解析 user=
                char* user_start = strstr(m_string, "user=");
                if (user_start) {
                    user_start += 5; // skip "user="
                    char* user_end = strchr(user_start, '&');
                    int len = user_end ? (user_end - user_start) : strlen(user_start);
                    if (len > 0 && len < 100) {
                        strncpy(name, user_start, len);
                        name[len] = '\0';
                    }
                }

                // 解析 passwd=
                char* pass_start = strstr(m_string, "passwd=");
                if (pass_start) {
                    pass_start += 7; // skip "passwd="
                    char* pass_end = strchr(pass_start, '&');
                    int len = pass_end ? (pass_end - pass_start) : strlen(pass_start);
                    if (len > 0 && len < 100) {
                        strncpy(password, pass_start, len);
                        password[len] = '\0';
                    }
                }

                // 辅助函数：去除末尾 \r \n 空格
                auto trim_crlf = [](char* s) {
                    int len = strlen(s);
                    while (len > 0 && (s[len-1] == '\r' || s[len-1] == '\n' || s[len-1] == ' ')) {
                        s[--len] = '\0';
                    }
                };
                trim_crlf(name);
                trim_crlf(password);
            }

            std::cout << "DEBUG: Extracted - name='" << name << "', password='" << password << "'" << std::endl;
            
            // 注册（3）
            if (next_char == '3') {
                if (strlen(name) == 0 || strlen(password) == 0) {
                    std::cout << "ERROR: Empty username or password" << std::endl;
                    strcpy(m_url, "/registerError.html");
                } else {
                    // 检查用户是否已存在
                    char sql_check[256];
                    snprintf(sql_check, sizeof(sql_check), 
                             "SELECT username FROM user WHERE username='%s'", name);
                    
                    std::cout << "DEBUG: Checking user: " << sql_check << std::endl;
                    
                    if (mysql_query(mysql, sql_check) == 0) {
                        MYSQL_RES* result = mysql_store_result(mysql);
                        if (result) {
                            if (mysql_num_rows(result) > 0) {
                                std::cout << "DEBUG: User already exists: " << name << std::endl;
                                strcpy(m_url, "/registerError.html");
                                mysql_free_result(result);
                            } else {
                                mysql_free_result(result);
                                
                                // 插入新用户
                                m_lock.lock();
                                char sql_insert[256];
                                snprintf(sql_insert, sizeof(sql_insert), 
                                         "INSERT INTO user(username, passwd) VALUES('%s', '%s')",
                                         name, password);
                                
                                std::cout << "DEBUG: Executing SQL: " << sql_insert << std::endl;
                                
                                int res = mysql_query(mysql, sql_insert);
                                if (res == 0) {
                                    users[name] = password;
                                    strcpy(m_url, "/log.html");
                                    std::cout << "DEBUG: Registration successful for user: " << name << std::endl;
                                } else {
                                    std::cout << "MySQL Error: " << mysql_error(mysql) << std::endl;
                                    strcpy(m_url, "/registerError.html");
                                    std::cout << "DEBUG: Registration failed for user: " << name << std::endl;
                                }
                                m_lock.unlock();
                            }
                        }
                    } else {
                        std::cout << "MySQL Error (check): " << mysql_error(mysql) << std::endl;
                        strcpy(m_url, "/registerError.html");
                    }
                }
            }
            // 登录（2）
            else if (next_char == '2') {
                if (strlen(name) == 0 || strlen(password) == 0) {
                    std::cout << "ERROR: Empty username or password" << std::endl;
                    strcpy(m_url, "/logError.html");
                } else {
                    char sql_query[256];
                    snprintf(sql_query, sizeof(sql_query), 
                             "SELECT passwd FROM user WHERE username='%s'", name);
                    
                    std::cout << "DEBUG: Querying user: " << sql_query << std::endl;
                    
                    if (mysql_query(mysql, sql_query) == 0) {
                        MYSQL_RES* result = mysql_store_result(mysql);
                        if (result) {
                            MYSQL_ROW row = mysql_fetch_row(result);
                            if (row && row[0]) {
                                if (strcmp(row[0], password) == 0) {
                                    strcpy(m_url, "/welcome.html");
                                    std::cout << "DEBUG: Login successful for user: " << name << std::endl;
                                } else {
                                    strcpy(m_url, "/logError.html");
                                    std::cout << "DEBUG: Password incorrect for user: " << name << std::endl;
                                }
                            } else {
                                strcpy(m_url, "/logError.html");
                                std::cout << "DEBUG: User not found: " << name << std::endl;
                            }
                            mysql_free_result(result);
                        } else {
                            strcpy(m_url, "/logError.html");
                            std::cout << "DEBUG: Query result is null" << std::endl;
                        }
                    } else {
                        std::cout << "MySQL Error: " << mysql_error(mysql) << std::endl;
                        strcpy(m_url, "/logError.html");
                    }
                }
            }
            
            // 重新获取 next_char 因为 m_url 可能被修改
            p = strrchr(m_url, '/');
            next_char = p ? *(p + 1) : '\0';
        }
    }
    
    // 构建实际文件路径
    if (next_char == '0') {
        strncat(m_real_file, "/register.html", FILENAME_LEN - strlen(m_real_file) - 1);
    }
    else if (next_char == '1') {
        strncat(m_real_file, "/log.html", FILENAME_LEN - strlen(m_real_file) - 1);
    }
    else if (next_char == '5') {
        strncat(m_real_file, "/picture.html", FILENAME_LEN - strlen(m_real_file) - 1);
    }
    else if (next_char == '6') {
        strncat(m_real_file, "/video.html", FILENAME_LEN - strlen(m_real_file) - 1);
    }
    else if (next_char == '7') {
        strncat(m_real_file, "/fans.html", FILENAME_LEN - strlen(m_real_file) - 1);
    }
    else {
        strncat(m_real_file, m_url, FILENAME_LEN - strlen(m_real_file) - 1);
    }
    
    std::cout << "DEBUG: Final file path: " << m_real_file << std::endl;
    
    // 检查文件状态
    if (stat(m_real_file, &m_file_stat) < 0) {
        std::cout << "ERROR: stat failed for file: " << m_real_file 
                  << ", errno: " << errno << std::endl;
        return NO_RESOURCE;
    }
    
    if (!(m_file_stat.st_mode & S_IROTH)) {
        std::cout << "ERROR: File not readable: " << m_real_file << std::endl;
        return FORBIDDEN_REQUEST;
    }
    
    if (S_ISDIR(m_file_stat.st_mode)) {
        std::cout << "ERROR: Path is a directory: " << m_real_file << std::endl;
        return BAD_REQUEST;
    }
    
    // 打开文件并内存映射
    int fd = open(m_real_file, O_RDONLY);
    if (fd < 0) {
        std::cout << "ERROR: Failed to open file: " << m_real_file 
                  << ", errno: " << errno << std::endl;
        return INTERNAL_ERROR;
    }
    
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (m_file_address == MAP_FAILED) {
        std::cout << "ERROR: mmap failed for file: " << m_real_file 
                  << ", errno: " << errno << std::endl;
        close(fd);
        return INTERNAL_ERROR;
    }
    
    close(fd);
    std::cout << "DEBUG: File mapped successfully, size: " << m_file_stat.st_size << " bytes" << std::endl;
    return FILE_REQUEST;
}
void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
bool http_conn::write()
{
    int temp = 0;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}
bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    
    m_ret_code = read_ret;  // 保存处理结果
    
    // 如果是 CGI 请求，需要数据库连接
    if (cgi == 1) {
        // 如果有数据库连接池，从池中获取连接
        if (m_connPool && m_sql_num > 0 && !mysql) {
            connectionRAII mysqlcon(&mysql, m_connPool);
            
            if (!mysql) {
                LOG_ERROR("Failed to get MySQL connection for CGI request");
                m_ret_code = INTERNAL_ERROR;
            } else {
                LOG_INFO("MySQL connection acquired for CGI request");
            }
        } else if (!mysql) {
            // 没有数据库连接池，记录错误
            LOG_ERROR("No database connection available for CGI request");
            m_ret_code = INTERNAL_ERROR;
        }
    }
    
    bool write_ret = process_write(m_ret_code);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}

// 重置连接状态（供连接池使用）
void http_conn::reset() {
    std::cout << "DEBUG: http_conn::reset() called" << std::endl;
       m_sockfd = -1;
    m_state = 0;
    m_read_idx = 0;
    m_checked_idx = 0;
    m_start_line = 0;
    m_write_idx = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_method = GET;
    m_url = nullptr;
    m_version = nullptr;
    m_host = nullptr;
    m_content_length = 0;
    m_linger = false;
    bytes_to_send = 0;
    bytes_have_send = 0;
    cgi = 0;
    m_string = nullptr;
    timer_flag = 0;
    improv = 0;
    
    // 重置缓冲区
    memset(m_read_buf, 0, READ_BUFFER_SIZE);
    memset(m_write_buf, 0, WRITE_BUFFER_SIZE);
    memset(m_real_file, 0, FILENAME_LEN);
    
    // 释放内存映射
    unmap();
    
    m_file_address = nullptr;
    m_iv_count = 0;
    
    // 重置数据库连接
    mysql = nullptr;
    
    std::cout << "DEBUG: Connection reset" << std::endl;
}
