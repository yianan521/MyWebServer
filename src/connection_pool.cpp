#include "../include/connection_pool.h"
#include <iostream>

ConnectionPool* ConnectionPool::instance_ = nullptr;
std::mutex ConnectionPool::instanceMutex_;

ConnectionPool::ConnectionPool() : maxSize_(0) {
    std::cout << "ConnectionPool created" << std::endl;
}

ConnectionPool::~ConnectionPool() {
    destroy();
    std::cout << "ConnectionPool destroyed" << std::endl;
}

ConnectionPool* ConnectionPool::getInstance() {
    if (instance_ == nullptr) {
        std::lock_guard<std::mutex> lock(instanceMutex_);
        if (instance_ == nullptr) {
            instance_ = new ConnectionPool();
        }
    }
    return instance_;
}

void ConnectionPool::init(int maxSize) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    maxSize_ = maxSize;
    
    // 预创建连接对象
    for (int i = 0; i < maxSize; ++i) {
        auto conn = std::shared_ptr<http_conn>(new http_conn(), 
            [this](http_conn* p) {
                delete p;
                --usedCount_;
            });
        pool_.push(conn);
    }
    
    std::cout << "ConnectionPool initialized with " << maxSize << " connections" << std::endl;
}

std::shared_ptr<http_conn> ConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(poolMutex_);
    
    // 等待直到有可用连接
    cond_.wait(lock, [this]() { return !pool_.empty(); });
    
    auto conn = pool_.front();
    pool_.pop();
    ++usedCount_;
    
    // 重置连接状态
    conn->reset();
    
    std::cout << "Connection acquired. Free: " << pool_.size() 
              << ", Used: " << usedCount_ << std::endl;
    
    return conn;
}

void ConnectionPool::returnConnection(std::shared_ptr<http_conn> conn) {
    if (!conn) return;
    
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    // 重置连接状态
    conn->reset();
    
    pool_.push(conn);
    --usedCount_;
    
    std::cout << "Connection returned. Free: " << pool_.size() 
              << ", Used: " << usedCount_ << std::endl;
    
    cond_.notify_one();
}

void ConnectionPool::getStats(int& freeCount, int& usedCount, int& totalCount) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    freeCount = pool_.size();
    usedCount = usedCount_;
    totalCount = maxSize_;
}

void ConnectionPool::destroy() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    while (!pool_.empty()) {
        pool_.pop();
    }
    usedCount_ = 0;
}
