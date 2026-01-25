#ifndef HTTP_CONN_POOL_H
#define HTTP_CONN_POOL_H

#include "../http/http_conn.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

class http_conn_pool {
public:
    static http_conn_pool* get_instance() {
        static http_conn_pool instance;
        return &instance;
    }

    void init(int max_size, int timeout_sec = 15);

    http_conn* get_conn();
    void release_conn(http_conn* conn);

    size_t get_created_count() const { return m_created_count; }
    size_t get_cur_size() const { return m_cur_size; }
    size_t get_pool_size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_pool.size();
    }

private:
    http_conn_pool() = default;
    ~http_conn_pool();

    std::queue<http_conn*> m_pool;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;

    size_t m_max_size;
    size_t m_cur_size;
    size_t m_created_count;
    int m_timeout_sec;
};

#endif // HTTP_CONN_POOL_H