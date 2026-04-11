#pragma once

#include "../../core/CoreRuntime.h"

#include <cstdio>
#include <cstdarg>

struct DebugLogState {
    static constexpr int kMaxLines = 100;
    static constexpr int kMaxLineLen = 512;

    char Lines[kMaxLines][kMaxLineLen];
    int WriteIndex = 0;
    int Count = 0;
    int Generation = 0;

    static DebugLogState& Get() {
        static DebugLogState s;
        return s;
    }
};

inline void print(const char* fmt, ...) {
    auto& s = DebugLogState::Get();
    float gameTime = CoreRuntime::GetContext().gameTime;
    int offset = snprintf(s.Lines[s.WriteIndex], DebugLogState::kMaxLineLen, "[%.2f] ", gameTime);
    if (offset < 0) offset = 0;
    va_list args;
    va_start(args, fmt);
    vsnprintf(s.Lines[s.WriteIndex] + offset, DebugLogState::kMaxLineLen - offset, fmt, args);
    va_end(args);
    s.WriteIndex = (s.WriteIndex + 1) % DebugLogState::kMaxLines;
    if (s.Count < DebugLogState::kMaxLines) s.Count++;
    s.Generation++;
}

inline void printClear() {
    auto& s = DebugLogState::Get();
    s.WriteIndex = 0;
    s.Count = 0;
    s.Generation++;
}
