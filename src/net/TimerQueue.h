#pragma once
#include "FdGuard.h"
#include "Channel.h"
#include "Timer.h"
#include <set>
#include <unordered_map>

class TimerQueue
{
public:
    TimerQueue();
    Channel &getChannel();

    TimerQueue(const TimerQueue &) = delete;
    TimerQueue &operator=(const TimerQueue &) = delete;

    // 向 timerqueue 中添加 Timer，只循环一次
    TimerId runAfter(Duration delay, std::function<void()> cb); 
    // 向 timerqueue 中添加Timer，永久循环
    TimerId runEvery(Duration interval, std::function<void()> cb);
    // 取消 TimerQueue 内的 Timer
    void cancel(TimerId timerid);

private:
    FdGuard fd_;
    Channel channel_;
    TimerId next_id_ = 1;
    TimerId addTimer(Duration delay, Duration interval, std::function<void()> cb);
    void resetTimerfd(Timestamp when);
    // 記錄每一個定時任務的到期时间和Id （到期的絕對時間，TimerId）
    std::set<std::pair<Timestamp, TimerId>> timers_;
    // 記錄每一個定時任務 （TimerId，Timer）
    std::unordered_map<TimerId, Timer> active_;

    // 获取已经到期的定时任务
    std::vector<Timer> getExpired(Timestamp when);
    // 设置新的到期时间
    void  resetTimerfdToEarliest();
    // 回调处理函数
    void handleRead();
};