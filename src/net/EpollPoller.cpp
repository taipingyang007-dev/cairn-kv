#include "EpollPoller.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <cstdlib>
#include <stdio.h>
EpollPoller::EpollPoller()
{
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1)
    {
        perror("epoll_create1 failed:");
        abort();
    }
}
EpollPoller::~EpollPoller() { closeEpollFd(); }
void EpollPoller::closeEpollFd()
{
    if (epoll_fd_ >= 0)
        ::close(epoll_fd_);
    epoll_fd_ = -1;
}
// 注册
void EpollPoller::updateChannel(Channel *ch)
{
    // 添加
    if (ch->getPollState() == Channel::PollState::kNew || ch->getPollState() == Channel::PollState::kDeleted)
    {
        epoll_event ev = {};
        ev.events = ch->getEvents();
        ev.data.ptr = ch;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, ch->getFd(), &ev) == -1)
        {
            // 添加失败，下次再来
            perror("epoll_ctl EPOLL_CTL_ADD failed:");
            return;
        }
        ch->setPollState(Channel::PollState::kAdded);
    }
    else
    { // 修改，ch->getPollState() == Channel::PollState::kAdded
        epoll_event ev = {};
        ev.events = ch->getEvents();
        ev.data.ptr = ch;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, ch->getFd(), &ev) == -1)
        {
            // 修改失败，下次再来
            perror("epoll_ctl EPOLL_CTL_MOD failed:");
            return;
        }
    }
}

// 摘除
void EpollPoller::removeChannel(Channel *ch)
{
    if (ch->getPollState() == Channel::PollState::kAdded)
    {
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, ch->getFd(), nullptr) == -1)
        {
            // 摘除失败，下次再来
            perror("epoll_ctl EPOLL_CTL_DEL failed:");
            return;
        }
        ch->setPollState(Channel::PollState::kDeleted);
    }
    else
    {
        // 监听状态不是Channel::PollState::kAdded，说明不在监听红黑树上
        return;
    }
}

// wait，监听时间，channel数组为传入传出参数，为ev.data.ptr
void EpollPoller::poll(int timeout_ms, std::vector<Channel *> &active_channels)
{
    active_channels.clear();
    epoll_event evs[1024];
    int ret = epoll_wait(epoll_fd_, evs, 1024, timeout_ms);
    if (ret == -1)
    {
        // 监听失败，下次再来
        perror("epoll_wait failed:");
        return;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            Channel *channel = static_cast<Channel *>(evs[i].data.ptr);
            channel->setRevents(evs[i].events);
            active_channels.push_back(channel);
        }
    }
}