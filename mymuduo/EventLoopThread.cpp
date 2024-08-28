#include "EventLoopThread.h"
#include"EventLoop.h"
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb , const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb)
{

}

EventLoopThread::~EventLoopThread()
{
    exiting_=true;
    if(loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startloop()
{
    thread_.start();   //启动一个独立的新线程，新线程调用的函数就是EventLoopThread::threadFunc

    EventLoop *loop=nullptr;  //这里用作条件变量的条件判断，因为新线程可能还没创建好loop
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(nullptr == loop_)
        {
            cond_.wait(lock);  //当新线程没有通知的时候，会阻塞在这，直到新线程通知（cond_.notify)
        }
        loop=loop_;
    }
    return loop;
}

// 该方法是独立的新线程要调用的函数
void EventLoopThread::threadFunc()
{
    EventLoop loop; //创建一个独立的EventLoop,和上面的独立线程一一对应，实现one loop per thread

    if(callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_=&loop;
        cond_.notify_one();
    }

    loop.loop();  //开启EventLoop，EventLoop会开启Poller的EPOLL，进入阻塞等待事件发生。

    //如果可以执行到这一步，就说明当前的loop循环要关闭掉了，即某个subReactor线程要结束了
    std::unique_lock<std::mutex> lock(mutex_);
    loop_=nullptr;
}