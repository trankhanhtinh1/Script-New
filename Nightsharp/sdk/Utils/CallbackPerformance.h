#pragma once

#include "Logging.h"

#include <chrono>
#include <functional>

namespace SDK::Core::Utils {

class CallbackPerformance {
public:
    static long long MeasureMilliseconds(const std::function<void()>& callback,
                                         int iterations = 1,
                                         const char* memberName = "") {
        return Measure(callback, iterations, memberName, Mode::Milliseconds);
    }

    static long long MeasureTicks(const std::function<void()>& callback,
                                  int iterations = 1,
                                  const char* memberName = "") {
        return Measure(callback, iterations, memberName, Mode::Ticks);
    }

    static std::chrono::nanoseconds MeasureTimeSpan(const std::function<void()>& callback,
                                                    int iterations = 1,
                                                    const char* memberName = "") {
        const auto start = std::chrono::high_resolution_clock::now();
        if (!Run(callback, iterations, memberName)) {
            return {};
        }
        const auto elapsed = std::chrono::high_resolution_clock::now() - start;
        Logging::Write()(LogLevel::Info,
                         "%s has taken %lld elapsed nanoseconds to execute, and was executed successfuly.",
                         memberName ? memberName : "",
                         static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()));
        return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
    }

private:
    enum class Mode {
        Milliseconds,
        Ticks,
    };

    static bool Run(const std::function<void()>& callback, int iterations, const char* memberName) {
        try {
            for (int i = 0; i < iterations; ++i) {
                if (callback) {
                    callback();
                }
            }
            return true;
        } catch (...) {
            Logging::Write()(LogLevel::Error,
                             "%s had an error during execution and was unable to be measured.",
                             memberName ? memberName : "");
            return false;
        }
    }

    static long long Measure(const std::function<void()>& callback, int iterations, const char* memberName, Mode mode) {
        const auto start = std::chrono::high_resolution_clock::now();
        if (!Run(callback, iterations, memberName)) {
            return -1;
        }
        const auto elapsed = std::chrono::high_resolution_clock::now() - start;
        if (mode == Mode::Milliseconds) {
            const auto value = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            Logging::Write()(LogLevel::Info,
                             "%s has taken %lld elapsed milliseconds to execute, and was executed successfuly.",
                             memberName ? memberName : "",
                             static_cast<long long>(value));
            return static_cast<long long>(value);
        }

        const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        Logging::Write()(LogLevel::Info,
                         "%s has taken %lld elapsed ticks to execute, and was executed successfuly.",
                         memberName ? memberName : "",
                         static_cast<long long>(value));
        return static_cast<long long>(value);
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using CallbackPerformance = ::SDK::Core::Utils::CallbackPerformance;
} // namespace SDK::Utils
