#pragma once
#include "Poller.h"
#include <memory>
class EventLoop
{
public:
    EventLoop();
    EventLoop(const EventLoop &) = delete;
    EventLoop &operator=(const EventLoop &) = delete;

    void loop();
    void updateChannel(Channel *ch);
    void removeChannel(Channel *ch);
    void setAfterLoopCb(std::function<void()> qp);

private:
    std::unique_ptr<Poller> poller_;
    std::vector<Channel *> active_channels_;
    std::function<void()> queuePending_;
};