#include "http_conn_pool.h"
#include <iostream>
#include <ctime>

http_conn_pool::http_conn_pool() : m_max_size(0), m_cur_size(0), m_timeout(15), m_created_count(0), m_reused_count(0) {
    std::cout << "POOL_DEBUG: 连接池构造函数开始\n";
}

http_conn_pool::~http_conn_pool() {
    destroy();
    std::cout << "POOL_DEBUG: 连接池析构函数结束\n";
}

void http_conn_pool::init(int max_size, int timeout) {
    std::cout << "POOL_DEBUG: init 方法开始，max_size=" << max_size << ", timeout=" << timeout << "\n";
    m_max_size = max_size;
    m_timeout = timeout;
    m_cur_size = 0;
    m_created_count = 0;
    m_reused_count = 0;

    pthread_mutex_init(&m_mutex, NULL);
    pthread_cond_init(&m_cond, NULL);

    std::cout << "POOL_DEBUG: init 方法结束\n";
}

http_conn* http_conn_pool::get_conn() {
    pthread_mutex_lock(&m_mutex);

    while (m_conn_list.empty() && m_cur_size >= m_max_size) {
        pthread_cond_wait(&m_cond, &m_mutex);
    }

    http_conn* conn = nullptr;

    if (!m_conn_list.empty()) {
        conn = m_conn_list.front();
        m_conn_list.pop_front();
        m_reused_count++;

        if (std::time(nullptr) - conn->last_activity_time > m_timeout) {
            delete conn;
            conn = new http_conn();
            m_created_count++;
        }
    } else {
        conn = new http_conn();
        m_cur_size++;
        m_created_count++;
    }

    pthread_mutex_unlock(&m_mutex);
    return conn;
}

void http_conn_pool::release_conn(http_conn* conn) {
    if (!conn) return;

    conn->reset();

    pthread_mutex_lock(&m_mutex);

    if (m_conn_list.size() >= static_cast<size_t>(m_max_size)) {
        delete conn;
        m_cur_size--;
    } else {
        m_conn_list.push_back(conn);
        pthread_cond_signal(&m_cond);
    }

    pthread_mutex_unlock(&m_mutex);

    std::cout << "POOL_DEBUG: 连接释放回池中\n";
}

size_t http_conn_pool::get_pool_size() {
    pthread_mutex_lock(&m_mutex);
    size_t size = m_conn_list.size();
    pthread_mutex_unlock(&m_mutex);
    return size;
}

int http_conn_pool::get_total_conn() {
    pthread_mutex_lock(&m_mutex);
    int total = m_cur_size;
    pthread_mutex_unlock(&m_mutex);
    return total;
}

http_conn_pool::PoolStats http_conn_pool::get_stats() {
    pthread_mutex_lock(&m_mutex);
    PoolStats stats;
    stats.pool_size = m_conn_list.size();
    stats.total_conn = m_cur_size;
    stats.max_size = m_max_size;
    stats.timeout = m_timeout;
    stats.created_count = m_created_count;
    stats.reused_count = m_reused_count;
    pthread_mutex_unlock(&m_mutex);
    return stats;
}

void http_conn_pool::destroy() {
    pthread_mutex_lock(&m_mutex);
    
    for (auto& conn : m_conn_list) {
        delete conn;
    }
    m_conn_list.clear();

    m_cur_size = 0;
    m_created_count = 0;
    m_reused_count = 0;

    pthread_mutex_unlock(&m_mutex);

    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_cond);

    std::cout << "POOL_DEBUG: 连接池已销毁\n";
}