#pragma once
#include "sdk/EzEvade/Utils/EvadeUtils.h"
#include "sdk/Utils/DebugConsole.h"
#include <string>

namespace EzEvade {
namespace ConsolePrinter {

inline float LastPrintTime = 0.0f;

inline void Print(const std::string& str) {
    const float timeDiff = EvadeUtils::TickCount() - LastPrintTime;
    DebugConsole::Log("[%d] %s", (int)timeDiff, str.c_str());
    LastPrintTime = EvadeUtils::TickCount();
}

} // namespace ConsolePrinter
} // namespace EzEvade
