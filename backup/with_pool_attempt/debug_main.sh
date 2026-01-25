#!/bin/bash

# 在 main.cpp 中添加调试信息
sed -i '/int main(int argc, char \*argv\[\]) {/a\
    printf("MAIN_DEBUG: 程序开始\\n");' main.cpp

sed -i '/Config config;/a\
    printf("MAIN_DEBUG: Config 对象创建\\n");' main.cpp

sed -i '/config.parse_arg(argc, argv);/a\
    printf("MAIN_DEBUG: 参数解析完成\\n");' main.cpp

sed -i '/WebServer server;/a\
    printf("MAIN_DEBUG: WebServer 对象创建\\n");' main.cpp

sed -i '/server.init(/a\
    printf("MAIN_DEBUG: 开始 server.init\\n");' main.cpp

sed -i '/server.log_write();/a\
    printf("MAIN_DEBUG: 开始 server.log_write\\n");' main.cpp

sed -i '/server.sql_pool();/a\
    printf("MAIN_DEBUG: 开始 server.sql_pool\\n");' main.cpp

sed -i '/server.thread_pool();/a\
    printf("MAIN_DEBUG: 开始 server.thread_pool\\n");' main.cpp

sed -i '/server.http_conn_pool_init();/a\
    printf("MAIN_DEBUG: 开始 server.http_conn_pool_init\\n");' main.cpp

sed -i '/server.trig_mode();/a\
    printf("MAIN_DEBUG: 开始 server.trig_mode\\n");' main.cpp

sed -i '/server.eventListen();/a\
    printf("MAIN_DEBUG: 开始 server.eventListen\\n");' main.cpp

sed -i '/server.eventLoop();/a\
    printf("MAIN_DEBUG: 开始 server.eventLoop\\n");' main.cpp

sed -i '/return 0;/a\
    printf("MAIN_DEBUG: 程序正常结束\\n");' main.cpp
