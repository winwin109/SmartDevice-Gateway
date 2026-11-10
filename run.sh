#!/usr/bin/bash

make clean

make

if [ -f "server" ] && [ -f "client" ]
then
    echo "编译完成，启动服务器..."
    ./server
else
    echo "没有生成可执行程序"
fi
