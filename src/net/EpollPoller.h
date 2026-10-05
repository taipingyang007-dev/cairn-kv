#pragma once
#include "Poller.h"

class EpollPoller : public Poller
{
public:
    EpollPoller();
    ~EpollPoller();

    EpollPoller(const EpollPoller &) = delete;
    EpollPoller &operator=(const EpollPoller &) = delete;

    // 注册
    void updateChannel(Channel *ch) override;
    // 摘除
    void removeChannel(Channel *ch) override;
    // wait
    void poll(int timeout_ms, std::vector<Channel *> &active_channels) override;

private:
    int epoll_fd_;
    void closeEpollFd();
};