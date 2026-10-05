#include "net/TimerQueue.h"
#include <sys/timerfd.h> // timerfd_create / timerfd_settime / itimerspec
#include <cstdio>        // perror
#include <cstdlib>       // abort
#include <sys/epoll.h>
#include <unistd.h>
#include <cerrno>

namespace
{
    int createTimerFd()
    {
        int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (fd == -1)
        {
            perror("timerfd_create failed:");
            abort();
        }
        return fd;
    }
}
TimerQueue::TimerQueue() : fd_(createTimerFd()), channel_(fd_.getFd(), EPOLLIN)
{
    channel_.setReadCb([this]()
                       { handleRead(); });
}

void TimerQueue::resetTimerfd(Timestamp when)
{
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(when - Clock::now()).count();
    /*如果 ns <= 0（已经过期了），拆出来是 {0, 0} —— 而 it_value = {0,0} 的含义是「关掉定时器」，它就永远不响了。
    所以要兜底：if (ns <= 0) ns = 1;（muduo 兜到 100 微秒）。 */
    if (ns <= 0)
        ns = 1;

    struct itimerspec ts{};
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    ts.it_value.tv_sec = ns / 1'000'000'000;  // 整秒部分
    ts.it_value.tv_nsec = ns % 1'000'000'000; // 余下的纳秒
    if (timerfd_settime(fd_.getFd(), 0, &ts, nullptr) == -1)
    {
        perror("timerfd_settime failed:");
        abort();
    }
}

// 向 timerqueue 中添加 Timer，只循环一次
TimerId TimerQueue::runAfter(Duration delay, std::function<void()> cb) { return addTimer(delay, Duration::zero(), cb); }
// 向 timerqueue 中添加 Timer，永久循环
TimerId TimerQueue::runEvery(Duration interval, std::function<void()> cb)
{
    if (interval <= Duration::zero())
    {
        fprintf(stderr, "runEvery: interval must be positive\n");
        abort();
    }

    return addTimer(interval, interval, cb);
}
// 向 timerqueue 中添加 Timer
TimerId TimerQueue::addTimer(Duration delay, Duration interval, std::function<void()> cb)
{
    Timestamp when = Clock::now() + delay; // 在不同时机调用now()函数，结果不同
    Timer timer(when, interval, next_id_, cb);

    // 当timers_为空，或者新添加的到期时间<最近的到期时间，则resetTimerfd
    bool earliest_changed = timers_.empty() || when < timers_.begin()->first;
    if (earliest_changed)
        resetTimerfd(when);
    timers_.emplace(when, next_id_);
    active_.emplace(next_id_, timer);
    return next_id_++;
}

// 取消 TimerQueue 内的 Timer
void TimerQueue::cancel(TimerId timerid)
{
    auto pos = active_.find(timerid);
    if (pos != active_.end())
    {
        timers_.erase(std::make_pair(pos->second.expiration, pos->second.id));
        active_.erase(timerid);
    }
}

Channel &TimerQueue::getChannel() { return channel_; }

// 获取已经到期的定时任务并清理
std::vector<Timer> TimerQueue::getExpired(Timestamp when)
{
    std::vector<Timer> ans;
    for (auto it = timers_.begin(); it != timers_.end();)
    {
        // 如果定时任务的到期时间 <= 到期时刻
        if (it->first <= when)
        {
            Timer &t = active_.at(it->second);
            // 将符合条件的Timer添加进ans
            ans.emplace_back(t);

            // 检查是不是 Duration::zero()
            if (t.interval == Duration::zero())
            {
                // 如果是，将已经添加过的Timer进行删除
                active_.erase(it->second);
                it = timers_.erase(it); // erase的返回值是下一个迭代器
            }
            else
            {
                // 如果不是，需要重新计算到期时间，删除再写入
                Timestamp new_Expiration = when + t.interval;
                TimerId new_Id = it->second;

                // 删除
                it = timers_.erase(it); // erase的返回值是下一个迭代器

                // 写入
                timers_.emplace(new_Expiration, new_Id);
                t.expiration = new_Expiration;
            }
        }
        else
        {
            // 如果已经触及到了边界（定时任务的到期时间>现在的时刻）就结束循环
            break;
        }
    }
    return ans;
}

// 设置新的到期时间
void TimerQueue::resetTimerfdToEarliest()
{
    // 设置新的到期时间
    if (!timers_.empty())
        resetTimerfd(timers_.begin()->first);
}

void TimerQueue::handleRead()
{
    uint64_t exp = 0;
    ssize_t n = read(fd_.getFd(), &exp, sizeof(exp));
    fprintf(stderr, "[handleRead] 醒了，exp=%llu\n", (unsigned long long)exp);

    if (n == -1 && (errno == EAGAIN || errno == EINTR))
    {
        // 下次再来
        return;
    }
    else if (n == -1)
    {
        perror("read TimerFd failed:");
        return;
    }

    // 响应之后先检查对应的到期事件
    std::vector<Timer> timers = getExpired(Clock::now());
    // 将对应的Timer的回调进行执行
    for (auto &timer : timers)
        timer.cb();

    // 设置新的到期时间
    resetTimerfdToEarliest();
}