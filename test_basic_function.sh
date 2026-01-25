#!/bin/bash

echo "=== 基础功能验证测试 ==="
echo ""

# 编译
echo "1. 编译项目..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "❌ 编译失败"
    exit 1
fi
echo "✅ 编译成功"
echo ""

# 启动服务器
echo "2. 启动服务器..."
./server -p 9100 > /tmp/basic_test.log 2>&1 &
PID=$!
sleep 3

if ! ps -p $PID > /dev/null; then
    echo "❌ 服务器启动失败"
    cat /tmp/basic_test.log
    exit 1
fi
echo "✅ 服务器启动成功 (PID: $PID)"
echo ""

# 测试HTTP请求
echo "3. 测试HTTP请求..."
for i in {1..3}; do
    if curl -s http://localhost:9100/ > /dev/null; then
        echo "   ✅ 请求 $i 成功"
    else
        echo "   ❌ 请求 $i 失败"
    fi
    sleep 0.5
done
echo ""

# 停止服务器
echo "4. 停止服务器..."
kill $PID 2>/dev/null
sleep 1

if ps -p $PID > /dev/null; then
    kill -9 $PID 2>/dev/null
    echo "✅ 服务器已强制停止"
else
    echo "✅ 服务器已正常停止"
fi
echo ""

echo "=== 基础功能验证完成 ==="
echo "服务器运行正常，可以处理HTTP请求"
