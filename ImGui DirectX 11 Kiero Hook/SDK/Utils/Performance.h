#pragma once
#include <chrono>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <numeric>

// ============================================================================
// Performance — Block-based performance profiling utility
// Source: EnsoulSharp.SDK/Core/Utils/Performance.cs
//
// Usage:
//   // Scoped measurement (RAII):
//   {
//       SDK::Performance perf("MyFunction");
//       // ... code to measure ...
//   }  // Auto-prints elapsed time on scope exit
//
//   // Manual measurement:
//   SDK::Performance perf("Test", false);  // Don't print on dispose
//   // ... do work ...
//   long ms = perf.GetMilliseconds();
//   long ticks = perf.GetTickCount();
//
//   // Static helper:
//   long elapsed = SDK::Performance::Measure([]() {
//       // code to benchmark
//   });
//
//   // Performance tracker (aggregate stats):
//   SDK::PerfTracker::Begin("Orbwalker");
//   // ... orbwalker code ...
//   SDK::PerfTracker::End("Orbwalker");
//   SDK::PerfStats stats = SDK::PerfTracker::GetStats("Orbwalker");
// ============================================================================

namespace SDK {

    // ========================================================================
    // PerformanceType — What unit to report
    // ========================================================================
    enum class PerformanceType {
        Ticks,
        Milliseconds,
        Microseconds
    };

    // ========================================================================
    // Performance — RAII scoped profiler (EnsoulSharp.SDK/Core/Utils/Performance.cs)
    // ========================================================================
    class Performance {
    public:
        /// Create a scoped performance measurement
        /// @param name         Name for logging
        /// @param printDispose Whether to print elapsed time when scope exits
        /// @param type         What unit to report (Ticks, Milliseconds, Microseconds)
        Performance(const std::string& name = "Block",
                    bool printDispose = true,
                    PerformanceType type = PerformanceType::Milliseconds)
            : m_name(name), m_printDispose(printDispose), m_type(type),
              m_start(std::chrono::high_resolution_clock::now()), m_stopped(false) {}

        ~Performance() {
            Stop();
        }

        /// Manually stop the timer (called automatically on destruction)
        void Stop() {
            if (m_stopped) return;
            m_stopped = true;
            m_end = std::chrono::high_resolution_clock::now();

            if (m_printDispose) {
                PrintResult();
            }
        }

        /// Get elapsed ticks (high_resolution_clock ticks)
        long long GetTickCount() const {
            auto end = m_stopped ? m_end : std::chrono::high_resolution_clock::now();
            return (end - m_start).count();
        }

        /// Get elapsed milliseconds
        long long GetMilliseconds() const {
            auto end = m_stopped ? m_end : std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start).count();
        }

        /// Get elapsed microseconds
        long long GetMicroseconds() const {
            auto end = m_stopped ? m_end : std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count();
        }

        /// Get elapsed as double (seconds)
        double GetElapsedSeconds() const {
            auto end = m_stopped ? m_end : std::chrono::high_resolution_clock::now();
            return std::chrono::duration<double>(end - m_start).count();
        }

        // ====================================================================
        // Static helpers
        // ====================================================================

        /// Measure a function and return elapsed milliseconds
        static long long Measure(std::function<void()> func) {
            auto start = std::chrono::high_resolution_clock::now();
            func();
            auto end = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        }

        /// Measure a function and return elapsed microseconds
        static long long MeasureMicro(std::function<void()> func) {
            auto start = std::chrono::high_resolution_clock::now();
            func();
            auto end = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }

    private:
        void PrintResult() const {
            // Use OutputDebugString for in-game; can be redirected to Logging.h
            char buf[512];
            switch (m_type) {
            case PerformanceType::Ticks:
                snprintf(buf, sizeof(buf), "[Performance] %s: %lld ticks",
                         m_name.c_str(), GetTickCount());
                break;
            case PerformanceType::Milliseconds:
                snprintf(buf, sizeof(buf), "[Performance] %s: %lld ms",
                         m_name.c_str(), GetMilliseconds());
                break;
            case PerformanceType::Microseconds:
                snprintf(buf, sizeof(buf), "[Performance] %s: %lld us",
                         m_name.c_str(), GetMicroseconds());
                break;
            }
            OutputDebugStringA(buf);
            OutputDebugStringA("\n");
        }

        std::string m_name;
        bool m_printDispose;
        PerformanceType m_type;
        std::chrono::high_resolution_clock::time_point m_start;
        std::chrono::high_resolution_clock::time_point m_end;
        bool m_stopped;
    };

    // ========================================================================
    // PerfStats — Aggregate statistics for a named measurement
    // ========================================================================
    struct PerfStats {
        std::string Name;
        long long TotalMicroseconds = 0;
        long long MinMicroseconds = LLONG_MAX;
        long long MaxMicroseconds = 0;
        int SampleCount = 0;

        double AverageMs() const {
            return SampleCount > 0 ? (TotalMicroseconds / (double)SampleCount) / 1000.0 : 0.0;
        }
        double MinMs() const { return MinMicroseconds / 1000.0; }
        double MaxMs() const { return MaxMicroseconds / 1000.0; }
        double TotalMs() const { return TotalMicroseconds / 1000.0; }
    };

    // ========================================================================
    // PerfTracker — Named performance tracking with aggregated statistics
    // ========================================================================
    // Usage:
    //   PerfTracker::Begin("Orbwalker");
    //   // ... code ...
    //   PerfTracker::End("Orbwalker");
    //
    //   PerfStats stats = PerfTracker::GetStats("Orbwalker");
    //   printf("Orbwalker avg: %.2f ms (min: %.2f, max: %.2f, samples: %d)\n",
    //          stats.AverageMs(), stats.MinMs(), stats.MaxMs(), stats.SampleCount);
    //
    //   PerfTracker::Reset("Orbwalker");  // Clear stats
    //   PerfTracker::ResetAll();          // Clear all
    // ========================================================================
    class PerfTracker {
    public:
        /// Start a named timer
        static void Begin(const std::string& name) {
            s_active[name] = std::chrono::high_resolution_clock::now();
        }

        /// End a named timer and record the sample
        static void End(const std::string& name) {
            auto it = s_active.find(name);
            if (it == s_active.end()) return;

            auto end = std::chrono::high_resolution_clock::now();
            long long us = std::chrono::duration_cast<std::chrono::microseconds>(
                end - it->second).count();

            auto& stats = s_stats[name];
            stats.Name = name;
            stats.TotalMicroseconds += us;
            stats.SampleCount++;
            if (us < stats.MinMicroseconds) stats.MinMicroseconds = us;
            if (us > stats.MaxMicroseconds) stats.MaxMicroseconds = us;

            s_active.erase(it);
        }

        /// Get stats for a named measurement
        static PerfStats GetStats(const std::string& name) {
            auto it = s_stats.find(name);
            if (it != s_stats.end()) return it->second;
            return PerfStats{name};
        }

        /// Get all stats
        static const std::unordered_map<std::string, PerfStats>& GetAllStats() {
            return s_stats;
        }

        /// Reset stats for a named measurement
        static void Reset(const std::string& name) {
            s_stats.erase(name);
        }

        /// Reset all stats
        static void ResetAll() {
            s_stats.clear();
            s_active.clear();
        }

        /// Convenience: scope-based measurement that auto-records
        class Scope {
        public:
            Scope(const std::string& name) : m_name(name) {
                PerfTracker::Begin(m_name);
            }
            ~Scope() {
                PerfTracker::End(m_name);
            }
        private:
            std::string m_name;
        };

    private:
        static inline std::unordered_map<std::string,
            std::chrono::high_resolution_clock::time_point> s_active;
        static inline std::unordered_map<std::string, PerfStats> s_stats;
    };

} // namespace SDK
