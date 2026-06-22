#pragma once

#include "../Enumerations/PerformanceType.h"
#include "Logging.h"

#include <chrono>
#include <string>

namespace SDK::Core::Utils {

class Performance {
public:
    explicit Performance(PerformanceType performanceType = PerformanceType::TickCount,
                         bool printDispose = true,
                         const char* memberName = "")
        : memberName_(memberName ? memberName : ""),
          performanceType_(performanceType),
          printDispose_(printDispose),
          start_(Clock::now()) {}

    ~Performance() {
        Dispose();
    }

    void Dispose() {
        if (disposed_) {
            return;
        }
        disposed_ = true;

        if (!printDispose_) {
            return;
        }

        switch (performanceType_) {
        case PerformanceType::Milliseconds:
            Logging::Write()(LogLevel::Info,
                             "%s has taken %lld elapsed milliseconds to execute, and was executed successfuly.",
                             memberName_.c_str(),
                             GetMilliseconds());
            break;
        case PerformanceType::TimeSpan:
            Logging::Write()(LogLevel::Info,
                             "%s has taken %lld elapsed nanoseconds to execute, and was executed successfuly.",
                             memberName_.c_str(),
                             static_cast<long long>(GetTimeSpan().count()));
            break;
        case PerformanceType::TickCount:
        default:
            Logging::Write()(LogLevel::Info,
                             "%s has taken %lld elapsed ticks to execute, and was executed successfuly.",
                             memberName_.c_str(),
                             GetTickCount());
            break;
        }
    }

    long long GetMilliseconds() const {
        return static_cast<long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_).count());
    }

    long long GetTickCount() const {
        return static_cast<long long>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_).count());
    }

    std::chrono::nanoseconds GetTimeSpan() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_);
    }

private:
    using Clock = std::chrono::high_resolution_clock;

    std::string memberName_;
    PerformanceType performanceType_ = PerformanceType::TickCount;
    bool printDispose_ = true;
    bool disposed_ = false;
    Clock::time_point start_;
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using Performance = ::SDK::Core::Utils::Performance;
} // namespace SDK::Utils
