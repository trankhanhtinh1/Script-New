#pragma once

#include "Logging.h"

#include <chrono>
#include <functional>
#include <utility>

namespace SDK::Utils {

class CallbackPerformance {
public:
    using Callback = std::function<void(long long microseconds)>;

    explicit CallbackPerformance(Callback callback = {})
        : m_callback(std::move(callback))
        , m_started(std::chrono::high_resolution_clock::now()) {}

    ~CallbackPerformance() {
        if (m_callback) {
            m_callback(GetMicroseconds());
        }
    }

    long long GetMicroseconds() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - m_started).count();
    }

    template<typename Func>
    static long long MeasureMilliseconds(Func&& funcCallback,
                                         int iterations = 1,
                                         const char* memberName = "") {
        return Measure<std::chrono::milliseconds>(
            std::forward<Func>(funcCallback),
            iterations,
            memberName,
            "elapsed milliseconds");
    }

    template<typename Func>
    static long long MeasureTicks(Func&& funcCallback,
                                  int iterations = 1,
                                  const char* memberName = "") {
        return Measure<std::chrono::nanoseconds>(
            std::forward<Func>(funcCallback),
            iterations,
            memberName,
            "elapsed ticks");
    }

    template<typename Func>
    static std::chrono::microseconds MeasureTimeSpan(Func&& funcCallback,
                                                     int iterations = 1,
                                                     const char* memberName = "") {
        try {
            const auto started = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                funcCallback();
            }
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - started);
            Logging::Write()(LogLevel::Info,
                             "%s has taken %lld us to execute, and was executed successfully.",
                             memberName ? memberName : "",
                             static_cast<long long>(elapsed.count()));
            return elapsed;
        } catch (...) {
            Logging::Write()(LogLevel::Error,
                             "%s had an error during execution and was unable to be measured.",
                             memberName ? memberName : "");
            return std::chrono::microseconds::zero();
        }
    }

private:
    template<typename Duration, typename Func>
    static long long Measure(Func&& funcCallback,
                             int iterations,
                             const char* memberName,
                             const char* unitName) {
        try {
            const auto started = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                funcCallback();
            }
            const auto elapsed = std::chrono::duration_cast<Duration>(std::chrono::high_resolution_clock::now() - started).count();
            Logging::Write()(LogLevel::Info,
                             "%s has taken %lld %s to execute, and was executed successfully.",
                             memberName ? memberName : "",
                             static_cast<long long>(elapsed),
                             unitName ? unitName : "");
            return static_cast<long long>(elapsed);
        } catch (...) {
            Logging::Write()(LogLevel::Error,
                             "%s had an error during execution and was unable to be measured.",
                             memberName ? memberName : "");
            return -1LL;
        }
    }

    Callback m_callback = {};
    std::chrono::high_resolution_clock::time_point m_started;
};

} // namespace SDK::Utils
