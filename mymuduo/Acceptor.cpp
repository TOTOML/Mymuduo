#include"Acceptor.h"
#include<sys/types.h>
#include<sys/socket.h> 
#include"Logger.h"
#include<errno.h>
#include<netinet/tcp.h>
#include"InetAddress.h"
#include<unistd.h>

static int createNonblocking()
{
    int sockfd=::socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,IPPROTO_TCP);
    if(sockfd<0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
:loop_(loop),
acceptSocket_(createNonblocking()),
acceptChannel_(loop,acceptSocket_.fd())
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);   //bind绑定套接字 
    //TcpServer::start() 内部调用Acceptor::listen() ,进行套接字监听
    //有新用户的连接时，需要执行一个回调函数，将connfd打包成一个channel，发送给subloop
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead,this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listening_=true;
    acceptSocket_.listen();   //开始listen
    acceptChannel_.enableReading();  //把读事件注册到Poller当中
}



// 当listenfd有读事件发生了，即有新用户连接到了，就调用该函数
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd=acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd,peerAddr); //轮询找到subloop，唤醒，然后分发当前的新客户端Channel 
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
         LOG_ERROR("%s:%s:%d accept err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
         if(errno == EMFILE) //表示该进程能使用的文件描述符个数达到了上限
         {
            LOG_ERROR("%s:%s:%d sockfd resource reached limit\n",__FILE__,__FUNCTION__,__LINE__);
         }
    }
}
