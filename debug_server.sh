#!/bin/bash

echo "=== 服务器启动问题调试 ==="
echo ""

# 1. 编译检查
echo "1. 编译检查..."
make clean > /dev/null
if make > /tmp/compile.log 2>&1; then
    echo "✅ 编译成功"
else
    echo "❌ 编译失败"
    cat /tmp/compile.log | grep -i "error\|undefined" | head -10
    exit 1
fi

# 2. 检查文件依赖
echo ""
echo "2. 检查可执行文件..."
if [ -f ./server ]; then
    echo "✅ server可执行文件存在"
    file ./server
    ldd ./server 2>/dev/null | head -5
else
    echo "❌ server可执行文件不存在"
    exit 1
fi

# 3. 使用strace跟踪系统调用
echo ""
echo "3. 跟踪系统调用..."
timeout 2 strace -o /tmp/strace.log ./server -p 9123 > /tmp/server_output.log 2>&1 &
PID=$!
sleep 1

if ps -p $PID > /dev/null 2>&1; then
    echo "✅ 服务器启动成功 (PID: $PID)"
    kill $PID 2>/dev/null
    echo "查看最后几行输出:"
    tail -10 /tmp/server_output.log
else
    echo "❌ 服务器启动失败"
    echo "查看错误输出:"
    cat /tmp/server_output.log
    
    echo ""
    echo "查看系统调用错误:"
    grep -i "error\|fail\|exit" /tmp/strace.log | tail -10
fi

# 4. 测试不同端口
echo ""
echo "4. 测试不同端口..."
TEST_PORTS="9124 9125 9126 9127"
for port in $TEST_PORTS; do
    echo -n "测试端口 $port: "
    timeout 2 ./server -p $port > /tmp/port_$port.log 2>&1 &
    PID=$!
    sleep 1
    if ps -p $PID > /dev/null 2>&1; then
        echo "✅ 成功"
        kill $PID 2>/dev/null
    else
        echo "❌ 失败"
    fi
done

echo ""
echo "=== 调试完成 ==="
echo "详细日志:"
echo "编译日志: /tmp/compile.log"
echo "服务器输出: /tmp/server_output.log"
echo "系统调用跟踪: /tmp/strace.log"
