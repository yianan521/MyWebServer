TARGET = server
OBJS = main.o timer/lst_timer.o http/http_conn.o log/log.o CGImysql/sql_connection_pool.o webserver.o config.o

CC = g++
CFLAGS = -g -Wall -std=c++11

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) -lpthread -lmysqlclient

main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp -o main.o

timer/lst_timer.o: timer/lst_timer.cpp
	$(CC) $(CFLAGS) -c timer/lst_timer.cpp -o timer/lst_timer.o

http/http_conn.o: http/http_conn.cpp
	$(CC) $(CFLAGS) -c http/http_conn.cpp -o http/http_conn.o

log/log.o: log/log.cpp
	$(CC) $(CFLAGS) -c log/log.cpp -o log/log.o

CGImysql/sql_connection_pool.o: CGImysql/sql_connection_pool.cpp
	$(CC) $(CFLAGS) -c CGImysql/sql_connection_pool.cpp -o CGImysql/sql_connection_pool.o

webserver.o: webserver.cpp
	$(CC) $(CFLAGS) -c webserver.cpp -o webserver.o

config.o: config.cpp
	$(CC) $(CFLAGS) -c config.cpp -o config.o

pool/http_conn_pool.o: pool/http_conn_pool.cpp
	$(CC) $(CFLAGS) -c pool/http_conn_pool.cpp -o pool/http_conn_pool.o

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
CXXFLAGS = -g -Wall -std=c++11 -pthread
