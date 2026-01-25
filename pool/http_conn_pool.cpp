#include "http_conn_pool.h"
#include <iostream>
#include <unistd.h>

void http_conn_pool::init(int max_size, int timeout_sec) {
    if (max_size <= 0) max_size = 10;
    m_max_size = max_size;
    m_timeout_sec = timeout_sec;
    m_cur_size = 0;
    m_created_count = 0;
}

http_conn* http_conn_pool::get_conn() {
    std::unique_lock<std::mutex> lock(m_mutex);

    // 等待直到有可用连接或可以创建新连接
    m_cond.wait(lock, [this]() {
        return !m_pool.empty() || m_cur_size < m_max_size;
    });

    http_conn* conn = nullptr;

    if (!m_pool.empty()) {
        conn = m_pool.front();
        m_pool.pop();

        // 检查是否超时（简单策略：直接复用，不检查）
        // 若需严格超时，可记录 last_used 时间并判断
    } else if (m_cur_size < m_max_size) {
        conn = new http_conn;
        ++m_cur_size;
        ++m_created_count;
    }
    std::cout << "连接池[获取] 当前池大小: " << m_pool.size()
              << ", 总创建: " << m_created_count << std::endl;

    return conn;
}

void http_conn_pool::release_conn(http_conn* conn) {
    std::cout << "连接池[释放] 连接 " << conn << std::endl;
    if (!conn) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_pool.push(conn);
    m_cond.notify_one();
}

http_conn_pool::～http_conn_pool() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_pool.empty()) {
        delete m_pool.front();
        m_pool.pop();
    }
    // 注意：正在使用的连接由外部负责 delete（但正常应已全部释放）
}
