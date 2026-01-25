#ifndef HTTP_CONN_POOL_H
#define HTTP_CONN_POOL_H

#include "../http/http_conn.h"
#include "../log/log.h"
#include <list>
#include <pthread.h>

// HTTP连接池类
class http_conn_pool {
public:
    // 获取单例实例
    static http_conn_pool* get_instance() {
        static http_conn_pool instance;
        return &instance;
    }

    // 初始化连接池
    void init(int max_size, int timeout = 15) {
        m_max_size = max_size;
        m_timeout = timeout;
        m_cur_size = 0;
        
        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init(&m_cond, NULL);
        
        Log::get_instance()->write_log(0, "HTTP连接池初始化，最大大小：%d，超时时间：%d秒", 
                                      max_size, timeout);
    }

    // 获取一个HTTP连接
    http_conn* get_conn() {
        pthread_mutex_lock(&m_mutex);
        
        // 如果池为空且未达到最大限制，创建新连接
        if (m_conn_list.empty() && m_cur_size < m_max_size) {
            http_conn* conn = new http_conn();
            m_cur_size++;
            pthread_mutex_unlock(&m_mutex);
            
            Log::get_instance()->write_log(0, "创建新HTTP连接，当前连接数：%d", m_cur_size);
            return conn;
        }
        
        // 等待池中有可用连接
        while (m_conn_list.empty()) {
            if (m_cur_size >= m_max_size) {
                Log::get_instance()->write_log(1, "HTTP连接池已满，等待可用连接...");
                pthread_cond_wait(&m_cond, &m_mutex);
            } else {
                // 创建新连接
                http_conn* conn = new http_conn();
                m_cur_size++;
                pthread_mutex_unlock(&m_mutex);
                
                Log::get_instance()->write_log(0, "创建新HTTP连接，当前连接数：%d", m_cur_size);
                return conn;
            }
        }
        
        // 从池中获取连接
        http_conn* conn = m_conn_list.front();
        m_conn_list.pop_front();
        
        pthread_mutex_unlock(&m_mutex);
        
        // 检查连接是否超时
        if (difftime(time(NULL), conn->last_activity_time) > m_timeout) {
            Log::get_instance()->write_log(0, "HTTP连接超时，销毁并创建新连接");
            delete conn;
            conn = new http_conn();
        }
        
        return conn;
    }

    // 释放HTTP连接回池中
    void release_conn(http_conn* conn) {
        if (!conn) return;
        
        // 重置连接状态（但不关闭socket）
        conn->reset();
        
        pthread_mutex_lock(&m_mutex);
        
        // 如果池已满，直接销毁连接
        if (m_conn_list.size() >= (size_t)m_max_size) {
            delete conn;
            m_cur_size--;
            Log::get_instance()->write_log(0, "HTTP连接池已满，销毁连接，当前连接数：%d", m_cur_size);
        } else {
            // 放回池中
            m_conn_list.push_back(conn);
            Log::get_instance()->write_log(0, "HTTP连接放回池中，池大小：%lu", m_conn_list.size());
            
            // 通知等待的线程
            pthread_cond_signal(&m_cond);
        }
        
        pthread_mutex_unlock(&m_mutex);
    }

    // 获取当前池大小
    size_t get_pool_size() {
        pthread_mutex_lock(&m_mutex);
        size_t size = m_conn_list.size();
        pthread_mutex_unlock(&m_mutex);
        return size;
    }

    // 获取当前总连接数
    int get_total_conn() {
        pthread_mutex_lock(&m_mutex);
        int total = m_cur_size;
        pthread_mutex_unlock(&m_mutex);
        return total;
    }

    // 清理连接池
    void destroy() {
        pthread_mutex_lock(&m_mutex);
        
        while (!m_conn_list.empty()) {
            http_conn* conn = m_conn_list.front();
            m_conn_list.pop_front();
            delete conn;
        }
        
        m_cur_size = 0;
        pthread_mutex_unlock(&m_mutex);
        
        pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_cond);
        
        Log::get_instance()->write_log(0, "HTTP连接池已销毁");
    }

private:
    http_conn_pool() {}
    ~http_conn_pool() { destroy(); }
    
    std::list<http_conn*> m_conn_list;  // 连接池
    int m_max_size;                     // 最大连接数
    int m_cur_size;                     // 当前总连接数
    int m_timeout;                      // 连接超时时间（秒）
    pthread_mutex_t m_mutex;            // 互斥锁
    pthread_cond_t m_cond;              // 条件变量
};

#endif