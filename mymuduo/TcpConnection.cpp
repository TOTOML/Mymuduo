#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include <functional>
#include"EventLoop.h"
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include <netinet/tcp.h>   //用于添加TCP_NODELAY，不然会报错
#include<string>

// 静态函数只能被当前文件访问，不能被其他文件访问
static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024) // 64M
{
    // 下面给channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会回调相应的操作函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string &buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(),buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()
            ));
        }
    }
}

/*
 * 发送数据。因为应用写的快，而内核发送数据慢，因此需要把待发送数据写入缓冲区，
 * 而且设置了水位回调，防止写太快 
*/
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len; //表示还没有发送的数据长度
    bool faultError = false; //用来记录是否发生错误

    // 之前调用过该connection的shutdown，不能再进行发送了
    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    /*因为一开始只对读事件感兴趣，写事件不一定有注册。
      所以该判断条件意思是，表示channel_第一次开始写数据 且 缓冲区没有待发送的数据
    */
    if(!channel_->isWriting() && outputBuffer_.readableByte() == 0)
    {
        //那就直接发送
        nwrote = ::write(channel_->fd(),data,len);
        if( nwrote >=0 )
        {
            remaining = len-nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                //既然在这里数据全部发送完成，就不用再给channel设置epollout事件了
                loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
            }
        }
        else  //表示nwrote < 0 ，出错
        {
            nwrote = 0;
            if(errno !=EWOULDBLOCK)  //EWOULDBLOCK,表示因为非阻塞没有数据的正常返回
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET) //表示收到SIGPIPE和SIGRESET的错误请求
                {
                    faultError = true;
                }
            }
        }
    }

    //说明当前这一次write, 并没有把数据全部发送出去，剩余的数据需要保存到缓冲区当中，
    //然后给channel注册epollout事件，poller发现TCP的发送缓冲区有空间，会通知相应的sock-channel，调用handleWrite回调方法
    //也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据全部发送完成
    if(!faultError && remaining > 0 )  
    {
        //表示目前发送缓冲区剩余的待发送数据的长度
        size_t oldlen = outputBuffer_.readableByte();
        
        //表示 加起来后会超过高水位，不加就不会超过高水位的状态
        if(oldlen + remaining >= highWaterMark_ && oldlen < highWaterMark_ &&highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_,shared_from_this(),oldlen+remaining));
        }

        outputBuffer_.append((char*)data + nwrote,remaining);
        if(!channel_->isWriting())
        {
            channel_->enableReading();//这里一定要注册channel的些时间，否则Epoll不会给channel注册EPOLLOUT
        }

    }
}

// 关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop,this)
        );
    }
}

void TcpConnection::shutdownInLoop()
{
    //如果channel没有注册写事件，说明outputBuffer中的数据已经全部发送完成
    if(!channel_->isWriting())  
    {
        socket_->shutdownWrite();  //关闭写端
    }
}


// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  //向poller注册channel的epollin事件

    // 新连接建立，执行回调
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)   //只有建立成功，才能删除建立
    {
        setState(kDisconnected);
        channel_->disableAll();  //把channel的所有感兴趣的事件，从poller中delete掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove();  //把channel从Poller中删除掉 
}



void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(),&savedErrno);
        if(n > 0)
        {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableByte() == 0)  //表示没数据可写了
            {
                channel_->disableWriting();
                if(writeCompleteCallback_)
                {
                    //唤醒loop_对应的thread线程，执行回调函数
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_,shared_from_this())
                    );
                }
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else  //表示数据不可写
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n",channel_->fd());
    }
}

// Poller通知channel调用channel::closeCallback，实际上最终是调用TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose  fd=%d  state=%d \n",channel_->fd(),(int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   //执行连接关闭的回调
    closeCallback_(connPtr);  //关闭连接的回调  执行的是TcpServer::removeConnection回调方法
} 
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    
    //返回值：若调用成功则返回0，若出错则返回-1
    if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n",name_.c_str(),err);
}


