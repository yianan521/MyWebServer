#ifndef SIMPLE_CONN_POOL_H
#define SIMPLE_CONN_POOL_H

#include "../http/http_conn.h"
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

class SimpleConnPool {
public:
    static SimpleConnPool* get_instance() {
        static SimpleConnPool instance;
        return &instance;
    }
    
    // 初始化连接池
    void init(int pool_size) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pool_size = pool_size;
        
        // 创建初始连接
        for (int i = 0; i < pool_size; ++i) {
            http_conn* conn = new http_conn();
            m_free_list.push(conn);
        }
        
        printf("SimpleConnPool: 初始化完成，池大小: %d\n", pool_size);
    }
    
    // 获取连接
    http_conn* get_conn() {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        if (m_free_list.empty()) {
            // 池为空，等待
            m_cond.wait(lock, [this]() { return !m_free_list.empty(); });
        }
        
        http_conn* conn = m_free_list.front();
        m_free_list.pop();
        m_in_use_count++;
        
        printf("SimpleConnPool: 获取连接，空闲: %lu, 使用中: %d\n", 
               m_free_list.size(), m_in_use_count);
        return conn;
    }
    
    // 释放连接
    void release_conn(http_conn* conn) {
        if (!conn) return;
        
        // 重置连接状态
        conn->reset();
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_free_list.push(conn);
        m_in_use_count--;
        
        printf("SimpleConnPool: 释放连接，空闲: %lu, 使用中: %d\n", 
               m_free_list.size(), m_in_use_count);
        
        // 通知等待的线程
        m_cond.notify_one();
    }
    
    // 获取统计信息
    void print_stats() {
        std::lock_guard<std::mutex> lock(m_mutex);
        printf("SimpleConnPool统计 - 空闲: %lu, 使用中: %d, 总大小: %d\n",
               m_free_list.size(), m_in_use_count, m_pool_size);
    }
    
    ~SimpleConnPool() {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_free_list.empty()) {
            http_conn* conn = m_free_list.front();
            m_free_list.pop();
            delete conn;
        }
    }

private:
    SimpleConnPool() : m_pool_size(0), m_in_use_count(0) {}
    SimpleConnPool(const SimpleConnPool&) = delete;
    SimpleConnPool& operator=(const SimpleConnPool&) = delete;
    
    std::queue<http_conn*> m_free_list;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    int m_pool_size;
    int m_in_use_count;
};

#endif
