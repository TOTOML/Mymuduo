// pragma once 可以保证该头文件，在源文件当中，只被包含一次
#pragma once

/*
noncopyable,能够使得派生类，能够正常的构造和析构，但是不能被拷贝和赋值
*/

class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};