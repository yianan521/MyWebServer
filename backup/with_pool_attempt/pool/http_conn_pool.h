#ifndef HTTP_CONN_POOL_H
#define HTTP_CONN_POOL_H

#include "../http/http_conn.h"
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
    void init(int max_size, int timeout = 15);

    // 获取一个HTTP连接
    http_conn* get_conn();

    // 释放HTTP连接回池中
    void release_conn(http_conn* conn);

    // 获取当前池大小
    size_t get_pool_size();

    // 获取当前总连接数
    int get_total_conn();

    // 获取统计信息
    struct PoolStats {
        size_t pool_size;
        int total_conn;
        int max_size;
        int timeout;
        long long created_count;
        long long reused_count;
    };
    
    PoolStats get_stats();

    // 清理连接池
    void destroy();

private:
    http_conn_pool();
    ~http_conn_pool();

    std::list<http_conn*> m_conn_list;  // 连接池
    int m_max_size;                     // 最大连接数
    int m_cur_size;                     // 当前总连接数
    int m_timeout;                      // 连接超时时间（秒）
    pthread_mutex_t m_mutex;            // 互斥锁
    pthread_cond_t m_cond;              // 条件变量
    long long m_created_count;          // 统计创建次数
    long long m_reused_count;           // 统计重用次数
};

#endif