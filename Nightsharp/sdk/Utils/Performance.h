#pragma once

#include "../Enumerations/PerformanceType.h"
#include "Logging.h"

#include <chrono>

namespace SDK::Utils {

class Performance {
public:
    explicit Performance(PerformanceType performanceType = PerformanceType::Ticks,
                         bool printOnDispose = true,
                         const char* memberName = "")
        : m_memberName(memberName ? memberName : "")
        , m_type(performanceType)
        , m_printOnDispose(printOnDispose)
        , m_started(std::chrono::high_resolution_clock::now()) {}

    ~Performance() {
        Dispose();
    }

    void Dispose() {
        if (m_disposed) {
            return;
        }
        m_disposed = true;

        if (!m_printOnDispose) {
            return;
        }

        switch (m_type) {
        case PerformanceType::Milliseconds:
            Logging::Write()(LogLevel::Info, "%s took %lld ms.", m_memberName.c_str(), GetMilliseconds());
            break;
        case PerformanceType::TimeSpan:
            Logging::Write()(LogLevel::Info, "%s took %lld us.", m_memberName.c_str(), GetMicroseconds());
            break;
        case PerformanceType::Ticks:
        default:
            Logging::Write()(LogLevel::Info, "%s took %lld ticks.", m_memberName.c_str(), GetTickCount());
            break;
        }
    }

    long long GetMilliseconds() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Elapsed()).count();
    }

    long long GetMicroseconds() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(Elapsed()).count();
    }

    long long GetTickCount() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Elapsed()).count();
    }

private:
    std::chrono::high_resolution_clock::duration Elapsed() const {
        return std::chrono::high_resolution_clock::now() - m_started;
    }

    std::string m_memberName = {};
    PerformanceType m_type = PerformanceType::Ticks;
    bool m_printOnDispose = true;
    bool m_disposed = false;
    std::chrono::high_resolution_clock::time_point m_started;
};

} // namespace SDK::Utils
