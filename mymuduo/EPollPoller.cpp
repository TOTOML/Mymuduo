#include "EPollPoller.h"
#include "Logger.h"
#include"Channel.h"
#include <error.h>
#include<string.h>  //memset函数所在
#include<unistd.h>  //close函数所在

const int kNew = -1;    // 表示channel还没有被添加到Poller中
const int kAdded = 1;   // 表示channel已经被添加到Poller中
const int kDeleted = 2; // 表示channel已经重复了，要把它删除掉

EPollPoller::EPollPoller(EventLoop *loop) : Poller(loop),
                                            epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
                                            events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d\n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

/*
 updateChannel和removeChannel什么时候被调用？
 在Channel类中，通过update和remove去调用EventLoop的updateChannel和removeChannel，然后EvenLoop的updateChannel和removeChannel
 就去调用EPollPoller中的updateChannel和removeChannel   
*/

/*
Channel中的index_初始化，是跟kNew、kAdd、kDeleted有关的。
index_ = -1，表示该Channel还没有添加到Poller当中。
相当于 index_=kNew
*/

void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d  events=%d  index=%d  \n",__FUNCTION__,channel->fd(),channel->events(),channel->index());

    if(index == kNew || index == kDeleted)
    {
        if(index == kNew)
        {
            int fd= channel->fd();
            channels_[fd]=channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD,channel);
    }
    else   // 表示channel已经在Poller中注册过了
    {
        int fd=channel->fd();
        if(channel->isNoneEvent()) //表示这个fd对事件不感兴趣
        {
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD,channel);
        }
    }
}

//在Poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd=channel->fd();
    int index=channel->index();
    
    channels_.erase(fd);   //去除Poller中ChannelMap里面相应的channel
    LOG_INFO("func = %s => fd = %d",__FUNCTION__,fd);
    if(index == kAdded)
    {
        update(EPOLL_CTL_DEL,channel);  //去除epoll上的fd
    }
    channel->set_index(kNew);

}


// 更新channel通道 epoll_ctl add/mod/del
void EPollPoller::update(int operation,Channel *channel)
{
    epoll_event event;
    memset(&event,0,sizeof event);
    int fd=channel->fd();
    event.data.fd=fd;
    event.events=channel->events();
    event.data.ptr=channel;

    if(::epoll_ctl(epollfd_,operation,fd,&event) < 0)
    {   
        /*
          如果操作是DEL，即使没有成功删除，也不影响程序的往后执行，因为
          本来也不打算继续关注这个fd了。
         */
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_INFO("epoll_ctl del errno : %d\n",errno);
        }
        /*
        如果是ADD或者MOD，没有成功的话，会影响程序往后的执行，因为该fd是
        我们需要关注的，所以应该coredump掉
        */
        else
        {
            //LOG_FATAL里面已经调用了exit函数退出，这里就不需要调用了。
            LOG_FATAL("epoll_ctl add/mod errno : %d\n",errno);
        }
    }
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    //实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu\n",__FUNCTION__,channels_.size());
    int numEvents=::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);

    int saveErrno=errno;
    Timestamp now(Timestamp::now());

    if(numEvents > 0)
    {
        LOG_INFO("%d events happened \n",numEvents);
        fillActiveChannels(numEvents,activeChannels);
        if(numEvents == events_.size()) //如果events中关注的事件全部活跃，则扩容
        {
            events_.resize(events_.size()*2);
        }
    }
    else if(numEvents == 0)
    {
        //如果为0，表示epoll_wait超时了
        LOG_DEBUG("%s timeout ! \n",__FUNCTION__);
    }
    else
    {
        if(saveErrno != EINTR)  //如果errno不是由中断引起的话
        {
            errno = saveErrno; // 因为在整个进程中，可能由其他线程发生了错误，覆盖了本线程中的
                                // errno,所以要重新复制一次
            LOG_ERROR("EPollPoller::poll() err, errno : %d\n",errno);
        }
    }
    return now;
}

void EPollPoller::fillActiveChannels(int numEvents,ChannelList *activeChannels) const
{
    for(int i=0;i<numEvents;i++)
    {
        Channel *channel=static_cast<Channel*>(events_[i].data.ptr);
        channel->setrevents(events_[i].events);
        activeChannels->push_back(channel); //EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表了      
    }

}