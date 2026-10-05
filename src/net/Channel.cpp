#include "Channel.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <utility>

Channel::Channel(int fd, uint32_t events) : fd_(fd), events_(events) {}
void Channel::setRevents(uint32_t revents) { revents_ = revents; }
void Channel::setReadCb(std::function<void()> func) { read_cb_ = std::move(func); }
void Channel::setWriteCb(std::function<void()> func) { write_cb_ = std::move(func); }
void Channel::setCloseCb(std::function<void()> func) { close_cb_ = std::move(func); }
void Channel::setPollState(Channel::PollState ps) { pstate_ = ps; }
void Channel::setEvents(std::uint32_t ev) { events_ = ev; }
std::uint32_t Channel::getEvents() const { return events_; }
Channel::PollState Channel::getPollState() const { return pstate_; }
int Channel::getFd() const { return fd_; }
void Channel::handleEvent()
{
    if (revents_ & EPOLLERR)
    {
        if (close_cb_)
        {
            this->close_cb_();
        }
    }
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (close_cb_)
        {
            this->close_cb_();
        }
    }
    if (revents_ & EPOLLIN)
    {
        if (read_cb_)
            this->read_cb_();
    }

    if (revents_ & EPOLLOUT)
    {
        if (write_cb_)
            this->write_cb_();
    }
}