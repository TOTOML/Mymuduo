#!/bin/bash

set -e

#如果没有buld目录，则船舰该目录
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*

cd `pwd`/build &&
    cmake .. &&
    make

# 回到项目根目录
cd ..

# 把头文件拷贝到 /usr/include/mymuduo    so库拷贝到 /usr/lib   PATH
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

# `ls *.h`表示，罗列出当前目录下的所有头文件
for header in `ls *.h`
do
    # $header 表示获取header的值
    cp $header /usr/include/mymuduo
done

cp `pwd`/lib/libmymuduo.so /usr/lib

# 如果我们更新了so库，调用这个命令可以刷新共享库缓存
ldconfig