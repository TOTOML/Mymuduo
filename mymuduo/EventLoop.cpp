#include"EventLoop.h"
#include<sys/eventfd.h>
#include<unistd.h>
#include<fcntl.h>
#include"Logger.h"
#include<errno.h>
#include"Poller.h"
#include"Channel.h"
#include<memory>

//防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;  //thread local变量

//定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs= 10000;   //10s

//创建wakeupfd，用来notify唤醒subReactor，处理mainReactor发过来的新用户连接channel
int createEventfd()
{
    int evtfd=::eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0)
    {
        LOG_FATAL("eventfd error: %d\n",errno);
    }
    return evtfd;
}
 
EventLoop::EventLoop()
    :looping_(false),quit_(false),callingPendingFunctors_(false),threadId_(CurrentThread::tid()),poller_(Poller::newDefaultPoller(this)),
    wakeupfd_(createEventfd()),wakeupChannel_(new Channel(this,wakeupfd_)),currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d\n",this,threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n",t_loopInThisThread,threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd的感兴趣事件类型  以及  发生事件后的回调函数操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    //每一个EventLoop都将监听wakeupchannel的EPOLLIN读事件了。
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupfd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    looping_ =true;
    quit_=false;

    LOG_INFO("EventLoop %p start looping \n",this);
    while(!quit_)
    {
        activeChannels_.clear();
        //这里其实监听两类fd：一类是clientfd，一类是wakeupfd
        pollReturnTime_=poller_->poll(kPollTimeMs,&activeChannels_);
        for(Channel *channel:activeChannels_)
        {
            //Poller监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前EventLoop事件循环需要处理的回调操作
        /*
        1. IO线程——mainReactor主要是完成accept新用户连接的工作，然后把新连接fd打包成channel，
        发送给subReactor。
        2. 因此，当mainReactor通过wakeupfd唤醒一个subReactor后，是需要subReactor执行某些行为的，
        所以mainReactor需要注册一个回调函数cb给subReactor来执行。
        3.因此，在mainReactor当中，通过一个vector，把要执行的函数都存了起来（即成员变量pendingFunctors_）

        */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping.\n",this);
    looping_=false;
}

//退出事件循环
/*   退出事件循环分两种情况：
    1. mainReactor的loop在自己所在的线程中调用quit()
    2. 在其他线程中（比如subReactor），调用了mainReactor的quit()
*/
void EventLoop::quit()
{
    quit_=true;
    if(!isInLoopThread())
    {
        wakeup();   //这是第二类退出情况，此时就需要wakeup mainReactor，使
                    //其能够在poller_->poll(kPollTimeMs,&activeChannels_)中返回，然后退出循环
    }
}


//在当前loop所在的线程中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if(isInLoopThread())  //判断是否在当前loop所在的线程中执行cb
    {
        cb();
    }
    else  //在非当前loop线程中执行cb，就需要唤醒loop所在的线程，执行cb
    {
        queueInLoop(cb);
    }
}

// 把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面回调函数的loop所在线程
    if(!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();  //唤醒loop所在线程
    }
}



// wakeupfd 可读事件发生时，需要调用的函数
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n =read(wakeupfd_,&one,sizeof one);
    if( n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8",n);
    }
}


// 用来唤醒loop所在的线程，往wakeupfd_写一个数据,wakeupchannel
//就发生读事件，当前loop所在线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one =1;
    ssize_t n = write(wakeupfd_,&one,sizeof one);
    if( n != sizeof one)
    {
        LOG_FATAL("EventLoop::wakeup() write %lu bytes instead of 8 \n",n);
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()  //执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for( const Functor &functor : functors)
    {
        functor();  //执行当前loop需要执行的回调函数
    }

    callingPendingFunctors_ =false;
}