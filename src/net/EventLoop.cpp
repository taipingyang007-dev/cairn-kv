#include "EventLoop.h"
#include "EpollPoller.h"
EventLoop::EventLoop()
{
    poller_ = std::make_unique<EpollPoller>();
}
void EventLoop::loop()
{
    while (1)
    {
        poller_->poll(-1, active_channels_);
        for (auto &active_channel : active_channels_)
        {
            active_channel->handleEvent();
        }
        if (queuePending_)
            queuePending_();
    }
}
void EventLoop::updateChannel(Channel *ch) { poller_->updateChannel(ch); }
void EventLoop::removeChannel(Channel *ch) { poller_->removeChannel(ch); }
void EventLoop::setAfterLoopCb(std::function<void()> qp) { queuePending_ = std::move(qp); }