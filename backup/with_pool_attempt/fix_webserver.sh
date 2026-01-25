#!/bin/bash
FILE="webserver.cpp"

# 修复 init 方法中的 config 错误
sed -i 's/m_http_conn_pool_size = config.http_conn_pool_size;/\/\/ 连接池参数通过构造函数设置，这里不需要赋值/g' $FILE
sed -i 's/m_http_conn_timeout = config.http_conn_timeout;//g' $FILE

# 修复 eventLoop 方法中的变量错误
sed -i "s/conn->init(sockfd, m_root, m_CONNTrigmode, m_close_log, user, passwd, databaseName);/conn->init(sockfd, *(users_timer[sockfd].address), m_root, m_CONNTrigmode, m_close_log, m_user, m_passWord, m_databaseName);/g" $FILE

# 修复 dealwithread 和 dealwithwrite 方法的调用
sed -i 's/dealwithread(conn, sockfd);/dealwithread(sockfd);/g' $FILE
sed -i 's/dealwithwrite(conn, sockfd);/dealwithwrite(sockfd);/g' $FILE

echo "修复完成"
