#!/bin/bash

# 在 WebServer 构造函数的关键点添加调试
sed -i '/users = new http_conn\[MAX_FD\];/a\
    printf("DEBUG: 分配 users 数组完成\\n");' webserver.cpp

sed -i '/m_root = (char \*)malloc(strlen(server_path) + strlen(root) + 1);/a\
    printf("DEBUG: 分配 m_root 内存完成\\n");' webserver.cpp

sed -i '/strcpy(m_root, server_path);/a\
    printf("DEBUG: 复制 server_path 完成\\n");' webserver.cpp

sed -i '/m_http_pool = NULL;/a\
    printf("DEBUG: 设置 m_http_pool = NULL 完成\\n");' webserver.cpp

sed -i '/users_timer = new client_data\[MAX_FD\];/a\
    printf("DEBUG: 分配 users_timer 完成\\n");' webserver.cpp

# 在 http_conn_pool_init 中添加调试
sed -i '/http_conn_pool_init() {/a\
    printf("DEBUG: 进入 http_conn_pool_init\\n");\
    printf("DEBUG: 调用 get_instance() 前\\n");' webserver.cpp

sed -i '/m_http_pool = http_conn_pool::get_instance();/a\
    printf("DEBUG: get_instance() 返回: %p\\n", m_http_pool);' webserver.cpp

sed -i '/m_http_pool->init(/a\
    printf("DEBUG: 调用 init 前\\n");' webserver.cpp
