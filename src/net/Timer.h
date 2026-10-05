#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
using TimerId = std::int64_t;
using Clock = std::chrono::steady_clock;
using Timestamp = Clock::time_point;
using Duration = Clock::duration;

struct Timer
{
    Timestamp expiration; // 什么时候到期
    Duration interval;    // 周期多长
    TimerId id;
    std::function<void()> cb;
};