#pragma once
#include <functional>
#include <cstdint>
class Channel
{
public:
    enum class PollState
    {
        kNew,    // 从没注册过        → updateChannel 用 ADD
        kAdded,  // 已经在 poller 里  → 用 MOD
        kDeleted // 曾经注册、已摘除  → 用 ADD
    };

public:
    Channel(int fd, uint32_t events);

    Channel(Channel &&) = delete;
    Channel &operator=(Channel &&) = delete;

    void setReadCb(std::function<void()> func);
    void setWriteCb(std::function<void()> func);
    void setCloseCb(std::function<void()> func);
    void setPollState(PollState ps);
    PollState getPollState() const;
    std::uint32_t getEvents() const;
    void setEvents(std::uint32_t ev);
    int getFd() const;
    void handleEvent();
    void setRevents(uint32_t revents);

private:
    int fd_{};                           // fd
    PollState pstate_ = PollState::kNew; // 监听状态
    std::uint32_t events_ = 0;           // 监听事件
    std::uint32_t revents_ = 0;          // 监听到的实际事件

    // 监听到事件之后的回调函数，可读之后的回调
    std::function<void()> read_cb_ = {};
    std::function<void()> write_cb_ = {};
    std::function<void()> close_cb_ = {};
};