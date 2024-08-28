#include"TcpServer.h"
#include"Logger.h"
#include<functional>
#include<strings.h>
#include"TcpConnection.h"


//静态函数只能被当前文件访问，不能被其他文件访问
static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop,
                    const InetAddress &listenAddr,
                    const std::string &nameArg,
                    Option option)
                    :loop_(CheckLoopNotNull(loop)),
                    ipPort_(listenAddr.toIpPort()),
                    name_(nameArg),
                    acceptor_(new Acceptor(loop,listenAddr,option == kReusePort)),
                    threadPool_(new EventLoopThreadPool(loop,name_)),
                    connectionCallback_(),
                    messageCallback_(),
                    nextConnId_(1),
                    started_(0)
{
    // 当有新用户连接时，会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,
            std::placeholders::_1,std::placeholders::_2));
}

TcpServer::~TcpServer()
{
     for(auto& item:connections_)
     {
        //通过用item.second初始化一个conn局部变量，这两个都是shared_ptr变量
        //这样就可以释放掉item.second智能指针，而conn这个智能指针会在离开作用域时自动释放
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        //销毁连接
        conn->getloop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed,conn)
        );
     }
}

//设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听  
void TcpServer::start()
{
    if(started_++ == 0)  //防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_);  //启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get()));
    }
}

// 有一个新的客户端的连接，acceptor会执行这个回调操作 
void TcpServer::newConnection(int sockfd,const InetAddress &peerAddr)
{
    // 轮询算法，选择一个subLoop，来管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf,sizeof buf,"-%s#%d",ipPort_.c_str(),nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
    name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());

    //通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local,sizeof local);
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd,(sockaddr*)&local,&addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    // 根据连接成功的sockfd，创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(
        ioLoop,
        connName,
        sockfd,  //Socket Channel
        localAddr,
        peerAddr
    ));

    connections_[connName] = conn;

    //下面的回调，都是用户设置给TcpServer的，
    //而TcpServer又设置给TcpConnection，TcpConnection设置给Channel，channel在Poller中注册
    //当有事件发生，就通知Channel调用这些回调函数
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    
    //设置了如何关闭连接的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection,this,std::placeholders::_1)
    );

    //只要有一个新的客户端连接，那么最终就会直接调用该函数
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop,this,conn)
    );
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
   LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s \n",name_.c_str(),conn->name().c_str());

   connections_.erase(conn->name());
   EventLoop *ioLoop = conn->getloop();
   ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed,conn)
   );

}