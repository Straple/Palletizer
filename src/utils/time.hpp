#pragma once

#include <chrono>
#include <iostream>

using TimePoint = std::chrono::steady_clock::time_point;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;
using Nanoseconds = std::chrono::nanoseconds;

TimePoint get_now();

class Timer {
    TimePoint start;

public:
    Timer();

    [[nodiscard]] double get() const;

    [[nodiscard]] uint64_t get_ms() const;

    [[nodiscard]] uint64_t get_us() const;

    [[nodiscard]] uint64_t get_ns() const;

    void reset();
};

std::ostream &operator<<(std::ostream &output, const Timer &time);
