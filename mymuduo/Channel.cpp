#include "Channel.h"
#include <sys/epoll.h>
#include"EventLoop.h"
#include"Logger.h"
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// EventLoop: ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revent_(0), index_(-1), tied_(false) {}

Channel::~Channel(){}

/* channel的tie方法什么时候调用? 在TcpConnection连接创建的时候。
 * 因为TcpConnection底层管理着channel
 * 而channel在Epoll中也是注册着的，所以如果channel对应的TcpConnection被释放了怎么办？那对应的回调函数也调用不了了
 * channel的回调函数，都是TcpConnection提供的，而且都绑定着TcpConnection的this
*/
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_=obj;
    tied_=true;
}
/*
 * 当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件，即epoll_ctl
 * EventLoop 相当于 包含一个 ChannelList 和 Poller
 * Channel不能直接调用Poller，因为没有包含Poller对象。因此只能通过EventLoop去调用Poller
*/
void Channel::update()
{
    //通过channel所属的EventLoop，调用Poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

/*
    在channel所属的EventLoop中，把当前的channel删除掉。
    EventlLoop中，使用一个vector<Channel*> 来存储所有的channel，然后命名为ChannelList
*/ 
void Channel::remove()
{
    loop_->removeChannel(this);
}

//fd得到poller通知以后，处理事件
void Channel::handleEvent(Timestamp receiveTime)
{
    //如果weak_ptr有绑定对象的话
    if(tied_)
    {
        std::shared_ptr<void> guard=tie_.lock();  //获取weak_ptr的指向
        if(guard) //如果获取成功
        {
            handleEventWithGuard(receiveTime);
        }
        else
        {
            handleEventWithGuard(receiveTime);
        }
    }
}


//根据Poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{

    LOG_INFO("channel handleEvent revents:%d\n",revent_);
    if((revent_ & EPOLLHUP) && !(revent_ & EPOLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
    }

    if(revent_ & EPOLLERR)
    {
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    if(revent_ & (EPOLLIN | EPOLLPRI))
    {
        if(readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if(revent_ & EPOLLOUT)
    {
        if(writeCallback_)
        {
            writeCallback_();
        }
    }
}