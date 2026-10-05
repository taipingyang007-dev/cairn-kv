#pragma once
#include "Channel.h"
#include <vector>
class Poller
{
public:
    Poller() = default;
    virtual ~Poller() = default;

    // 注册
    virtual void updateChannel(Channel *ch) = 0;
    // 摘除
    virtual void removeChannel(Channel *ch) = 0;
    // wait
    virtual void poll(int timeout_ms, std::vector<Channel *> &active_channels) = 0;
};