#!/bin/bash

# 在连接池构造函数中添加调试
sed -i '/http_conn_pool::http_conn_pool() {/a\
    printf("POOL_DEBUG: 连接池构造函数开始\\n");\
    printf("POOL_DEBUG: 初始化前: m_max_size=%d, m_cur_size=%d\\n", m_max_size, m_cur_size);' pool/http_conn_pool.cpp

sed -i '/m_created_count = 0;/a\
    printf("POOL_DEBUG: 成员变量初始化完成\\n");' pool/http_conn_pool.cpp

sed -i '/^}/i\
    printf("POOL_DEBUG: 连接池构造函数结束\\n");' pool/http_conn_pool.cpp

# 在 init 方法中添加调试
sed -i '/void http_conn_pool::init(/a\
    printf("POOL_DEBUG: init 方法开始，max_size=%d, timeout=%d\\n", max_size, timeout);' pool/http_conn_pool.cpp

sed -i '/pthread_mutex_init(/a\
    printf("POOL_DEBUG: 互斥锁初始化完成\\n");' pool/http_conn_pool.cpp

sed -i '/pthread_cond_init(/a\
    printf("POOL_DEBUG: 条件变量初始化完成\\n");' pool/http_conn_pool.cpp

sed -i '/^}/i\
    printf("POOL_DEBUG: init 方法结束\\n");' pool/http_conn_pool.cpp
