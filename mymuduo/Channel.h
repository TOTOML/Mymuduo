#pragma once
#include "noncopyable.h"
#include "Timestamp.h"
#include <functional>
#include<memory>
/*
 * 理清楚 Eventloop、Channel、Poller之间的关系
 * Channel 理解为通道， 封装了sockfd和其感兴趣的事件event，如EPOLLIN、EPOLLOUT事件
 * 还绑定了poller返回的具体发生的事件
 */

class EventLoop;

class Channel : noncopyable
{
public:
    using EventCallBack = std::function<void()>;
    using ReadEventCallBack = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到Poller通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallBack cb){readCallback_=std::move(cb);}
    void setWriteCallback(EventCallBack cb){writeCallback_=std::move(cb);}
    void setCloseCallback(EventCallBack cb) {closeCallback_=std::move(cb);}
    void setErrorCallback(EventCallBack cb) {errorCallback_=std::move(cb);}

    //防止当channel被手动remove掉后，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd(){return fd_;}
    int events(){return events_;}
    void setrevents(int revents){revent_=revents;}

    //设置fd相应的事件状态
    void enableReading(){ events_ |= kReadEvent; update();}
    void disableReading() { events_ &= ~kReadEvent; update();}
    void enableWriting() {events_ |= kWriteEvent; update();}
    void disableWriting() { events_ &= ~kWriteEvent; update();}
    void disableAll() { events_ = kNoneEvent; update();}

    //返回fd当前的事件状态
    bool isNoneEvent() const {return events_ == kNoneEvent;}
    bool isWriting() const {return events_ & kWriteEvent;}
    bool isReading() const {return events_ & kReadEvent;}

    int index() {return index_;}
    void set_index(int idx) { index_ = idx; }

    //one loop per thread
    EventLoop* ownerloop() {return loop_;}
    void remove();

private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    // 以下三个变量，表示fd的状态
    static const int kNoneEvent;  // 没有对任何事件感性
    static const int kReadEvent;  // 只对读事件感兴趣
    static const int kWriteEvent; // 只对写事件感兴趣
 
    EventLoop *loop_;    //事件循环
    const int fd_;      //fd, Poller上监听的事件
    int events_;        //注册fd感兴趣的事件
    int revent_;        //Poller返回的具体发生事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

// 因为channel里面能够获知fd最终发生的具体时间revents，所以它负责调用具体时间的回调操作
    ReadEventCallBack readCallback_;
    EventCallBack writeCallback_;
    EventCallBack closeCallback_;
    EventCallBack errorCallback_;

};