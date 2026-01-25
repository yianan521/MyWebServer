#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include "../http/http_conn.h"

class ConnectionPool {
public:
    // 获取单例实例
    static ConnectionPool* getInstance();
    
    // 初始化连接池
    void init(int maxSize);
    
    // 获取连接
    std::shared_ptr<http_conn> getConnection();
    
    // 归还连接
    void returnConnection(std::shared_ptr<http_conn> conn);
    
    // 获取统计信息
    void getStats(int& freeCount, int& usedCount, int& totalCount);
    
    // 销毁连接池
    void destroy();
    
private:
    ConnectionPool();
    ~ConnectionPool();
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    
    static ConnectionPool* instance_;
    static std::mutex instanceMutex_;
    
    std::queue<std::shared_ptr<http_conn>> pool_;
    std::mutex poolMutex_;
    std::condition_variable cond_;
    int maxSize_;
    std::atomic<int> usedCount_{0};
};

#endif
