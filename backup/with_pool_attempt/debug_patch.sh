#!/bin/bash

# 在 http_conn_pool_init 方法开始处添加
sed -i '/http_conn_pool_init() {/a\
    printf("DEBUG: 开始连接池初始化\\n");\
    printf("DEBUG: m_http_pool = %p\\n", m_http_pool);\
    printf("DEBUG: 大小=%d, 超时=%d\\n", m_http_conn_pool_size, m_http_conn_timeout);' webserver.cpp

# 在构造函数结束时添加
sed -i '/m_http_pool = NULL;/a\
    printf("DEBUG: WebServer构造函数完成\\n");' webserver.cpp

# 在 main 中连接池初始化前后添加
sed -i '/server.http_conn_pool_init();/i\
    printf("DEBUG: 开始初始化连接池\\n");' main.cpp
sed -i '/server.http_conn_pool_init();/a\
    printf("DEBUG: 连接池初始化完成\\n");' main.cpp
