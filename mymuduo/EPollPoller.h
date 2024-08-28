#pragma once
#include"Poller.h"
#include<vector>
#include<sys/epoll.h>
#include"Timestamp.h"

class Channel;

class EPollPoller:public Poller
{
    public:
        EPollPoller(EventLoop *loop);
        ~EPollPoller() override;

        // 重写基类Poller的抽象方法
        Timestamp poll(int timeoutMs,ChannelList *activeChannels) override;
        void updateChannel(Channel *channel) override;
        void removeChannel(Channel *channel) override;
    private:
        static const int kInitEventListSize=16;  //用于初始化EventList的大小
        using EventList=std::vector<epoll_event>;
        //填写活跃的连接
        void fillActiveChannels(int numEvents,ChannelList *) const;
        //更新channel通道
        void update(int operation, Channel *channel);

        int epollfd_;
        EventList events_;
};
