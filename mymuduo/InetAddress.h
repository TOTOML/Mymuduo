#pragma once

#include <string>
#include <netinet/in.h>
#include<arpa/inet.h>
// 封装socket地址类型
class InetAddress
{
public:
    explicit InetAddress(uint16_t port=0,std::string ip="127.0.0.1");
    explicit InetAddress(const struct sockaddr_in &addr):addr_(addr){}

    std::string toIp() const;
    uint16_t toPort() const;
    std::string toIpPort() const;

    //返回const修饰的指针，接收变量也要是const的
    const sockaddr_in* getSockAddr() const {return &addr_;}

    void setSockAddr(const sockaddr_in &addr){ addr_ = addr;}
private:
    struct sockaddr_in addr_;
};