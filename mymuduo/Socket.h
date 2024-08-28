#pragma once
#include"noncopyable.h"

class InetAddress;

class Socket : noncopyable
{
    public:
        explicit Socket(int sockfd):sockfd_(sockfd){}
        ~Socket();

        int fd() const {return sockfd_;}
        void bindAddress(const InetAddress &localaddr);
        void listen();
        int accept(InetAddress *peeraddr);

        void shutdownWrite();

        //立即输出，不在缓冲区中停留
        void setTcpNoDelay(bool on);

        // SO_REUSEADDR
        void setReuseAddr(bool on);

        // SO_REUSEPORT
        void setReusePort(bool on);

        // SO_KEEALIVE
        void setKeepAlive(bool on);

    private:
    const int sockfd_;
};