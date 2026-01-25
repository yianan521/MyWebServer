#ifndef SIMPLE_CONN_POOL_H
#define SIMPLE_CONN_POOL_H

#include <queue>
#include <mutex>
#include <iostream>
#include "../http/http_conn.h"

class SimpleConnPool {
private:
    std::queue<http_conn*> pool_;
    std::mutex mutex_;
    int max_size_;
    int used_count_;

public:
    SimpleConnPool() : max_size_(0), used_count_(0) {}

    static SimpleConnPool* getInstance() {
        static SimpleConnPool instance;
        return &instance;
    }

    void init(int size) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_size_ = size;
        for (int i = 0; i < size; ++i) {
            pool_.push(new http_conn());
        }
        std::cout << "SimpleConnPool: 初始化 " << size << " 个连接" << std::endl;
    }

    http_conn* acquire() {  // 无参数版本
        std::lock_guard<std::mutex> lock(mutex_);

        if (pool_.empty()) {
            std::cout << "SimpleConnPool: 池空，创建新连接" << std::endl;
            http_conn* conn = new http_conn();
            used_count_++;
            return conn;
        }

        http_conn* conn = pool_.front();
        pool_.pop();
        used_count_++;

        std::cout << "SimpleConnPool: 获取连接，空闲=" << pool_.size()
                  << ", 使用中=" << used_count_ << std::endl;
        return conn;
    }

    void release(http_conn* conn) {
        if (!conn) return;

        conn->reset();

        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(conn);
        used_count_--;

        std::cout << "SimpleConnPool: 释放连接，空闲=" << pool_.size()
                  << ", 使用中=" << used_count_ << std::endl;
    }

    void getStats(int& free, int& used, int& total) {
        std::lock_guard<std::mutex> lock(mutex_);
        free = pool_.size();
        used = used_count_;
        total = max_size_;
    }

    ~SimpleConnPool() {
        while (!pool_.empty()) {
            delete pool_.front();
            pool_.pop();
        }
    }
};

#endif
