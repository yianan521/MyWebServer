#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <string.h>
#include <sys/uio.h>
#include <map>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"
#include "../CGImysql/sql_connection_pool.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;        // 文件名的最大长度
    static const int READ_BUFFER_SIZE = 2048;   // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;  // 写缓冲区的大小
    
    // HTTP请求方法，这里只支持GET
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH };
    
    // 解析客户端请求时，主状态机的状态
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    
    // 从状态机的三种可能状态，即行的读取状态
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
    
    // 服务器处理HTTP请求的可能结果
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };

    static int m_epollfd;       // 所有socket上的事件都被注册到同一个epoll对象中
    static int m_user_count;    // 统计用户的数量



public:
    http_conn() {
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
        m_file_address = nullptr;
        m_iv_count = 0;
        bytes_to_send = 0;
        bytes_have_send = 0;
        cgi = 0;
        m_string = nullptr;
        timer_flag = 0;
        improv = 0;
        m_TRIGMode = 0;
        m_close_log = 0;
        mysql = nullptr;
        doc_root = nullptr;

        // 初始化字符数组
        memset(m_read_buf, 0, READ_BUFFER_SIZE);
        memset(m_write_buf, 0, WRITE_BUFFER_SIZE);
        memset(m_real_file, 0, FILENAME_LEN);
        memset(sql_user, 0, sizeof(sql_user));
        memset(sql_passwd, 0, sizeof(sql_passwd));
        memset(sql_name, 0, sizeof(sql_name));
           std::cout << "DEBUG: http_conn constructor called at address: " << this << std::endl;
    }
    ~http_conn() {
        // 确保释放内存映射
        unmap();
    }
    
    // 初始化连接
    void init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
              int close_log, std::string user, std::string passwd, std::string sqlname);
              
    // 重置连接状态（供连接池使用）
    void reset();
    
    // 关闭连接
    void close_conn(bool real_close = true);
    
    // 处理客户端请求
    void process();
    
    // 非阻塞读
    bool read_once();
    
    // 非阻塞写
    bool write();
    
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    MYSQL* get_mysql() { return mysql; }
    void set_mysql(MYSQL* conn) { mysql = conn; }
    // 初始化数据库结果集
    void initmysql_result(connection_pool *connPool);
    
    // 定时器相关
    int timer_flag;
    int improv;

private:
    // 初始化连接
    void init();
    
    // 从m_read_buf读取，并处理请求
    HTTP_CODE process_read();
    
    // 向m_write_buf写入响应
    bool process_write(HTTP_CODE ret);
    
    // 主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line(char *text);
    
    // 主状态机解析报文中的请求头数据
    HTTP_CODE parse_headers(char *text);
    
    // 主状态机解析报文中的请求内容
    HTTP_CODE parse_content(char *text);
    
    // 生成响应报文
    HTTP_CODE do_request();
    
    // m_start_line是已经解析的字符
    // get_line用于将指针向后偏移，指向未处理的字符
    char *get_line() { return m_read_buf + m_start_line; };
    
    // 从状态机，用于解析出一行内容
    LINE_STATUS parse_line();
    
    // 释放内存映射
    void unmap();
    
    // 根据响应报文返回状态描述
    const char *get_stateinfo(int code);
    
    // 添加状态行
    bool add_response(const char *format, ...);
    
    // 添加消息报头
    bool add_headers(int content_length);
    
    // 添加响应正文
    bool add_content(const char *content);
    
    // 添加状态行、消息报头、空行
    bool add_status_line(int status, const char *title);
    
    // 添加Content-Length
    bool add_content_length(int content_length);
    
    // 添加连接状态
    bool add_linger();
    bool add_content_type();
    // 添加空行
    bool add_blank_line();
    // 添加这两个成员变量
    connection_pool* m_connPool;  // 数据库连接池指针
    HTTP_CODE m_ret_code;         // 处理结果代码

public:
    int m_state;  // 读为0, 写为1
    int m_sockfd;
    sockaddr_in m_address;
    void set_conn_pool(connection_pool* pool) {
        m_connPool = pool;
    }
     MYSQL *mysql;  // MySQL连接指针
    void set_sql_num(int sql_num) { m_sql_num = sql_num; }
    int get_sql_num() const { return m_sql_num; }

private:
    char m_read_buf[READ_BUFFER_SIZE];   // 读缓冲区
    int m_read_idx;                      // 标识读缓冲区中已经读入的客户端数据的最后一个字节的下一个位置
    int m_checked_idx;                   // 当前正在分析的字符在读缓冲区中的位置
    int m_start_line;                    // 当前正在解析的行的起始位置
    
    char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲区
    int m_write_idx;                     // 写缓冲区中待发送的字节数
    
    CHECK_STATE m_check_state;           // 主状态机当前所处的状态
    METHOD m_method;                     // 请求方法
    
    char m_real_file[FILENAME_LEN];      // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    char *m_url;                         // 客户请求的目标文件的文件名
    char *m_version;                     // HTTP协议版本号，仅支持HTTP/1.1
    char *m_host;                        // 主机名
    int m_content_length;                // HTTP请求的消息总长度
    bool m_linger;                       // HTTP请求是否要求保持连接
    
    char *m_file_address;                // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;             // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2];                // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量
    int m_iv_count;
    
    int cgi;                             // 是否启用的POST
    char *m_string;                      // 存储请求头数据
    int bytes_to_send;                   // 剩余将要发送的数据大小
    int bytes_have_send;                 // 已经发送的数据大小
    
    char *doc_root;                      // 网站根目录
    
    map<string, string> m_users;         // 存储数据库用户名和密码
    int m_TRIGMode;
    int m_close_log;
    
    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
        int m_sql_num;  // 数据库连接数
};

#endif
