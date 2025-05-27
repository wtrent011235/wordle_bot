#pragma once

#include <chrono>
#include <stdexcept>

namespace timing {

template <typename Rep = double, typename Period = std::ratio<1>>
struct Timer {
private:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<Rep, Period>;

    Clock::time_point tp;
    Duration elapsed = Duration::zero();
    bool running = false;

public:
    constexpr Timer() noexcept = default;

    void start() {
        if (running) throw std::runtime_error("start() called while timer is already running");
        running = true;
        tp = Clock::now();
    }

    void stop() {
        if (!running) throw std::runtime_error("stop() called without corresponding start()");
        elapsed += std::chrono::duration_cast<Duration>(Clock::now() - tp);
        running = false;
    }

    Duration end() {
        if (running) {
            elapsed += std::chrono::duration_cast<Duration>(Clock::now() - tp);
            running = false; // Optional: "freeze" the timer after end
        }
        return elapsed;
    }

    void reset() {
        elapsed = Duration::zero();
        running = false;
    }
};

}