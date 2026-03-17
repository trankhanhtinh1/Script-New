#pragma once
#include <cstdio>
#include <string>
#include "EvadeUtils.h"

// ============================================================================
// ConsolePrinter
//   C# original: ezEvade.ConsolePrinter (ConsolePrinter.cs, 34 lines)
//   Line-by-line port preserving original logic
//
//   Simple debug printer that prefixes each message with elapsed time
//   since last print call.
// ============================================================================

namespace EzEvade {

    class ConsolePrinter {
    public:
        // C# line 13: private static float lastPrintTime = 0
        static inline float lastPrintTime = 0;                          // C# line 13

        // C# lines 15-18: static constructor — empty
        // (no-op in C++)

        // ====================================================================
        // Print
        //   C# lines 20-31
        //   public static void Print(string str)
        // ====================================================================
        static void Print(const std::string& str) {
            // C# line 24: var timeDiff = EvadeUtils.TickCount - lastPrintTime;
            float timeDiff = EvadeUtils::TickCount() - lastPrintTime;   // C# line 24

            // C# line 26: var finalStr = "[" + timeDiff + "] " + str;
            char buf[512];
            snprintf(buf, sizeof(buf), "[%.0f] %s", timeDiff, str.c_str());

            // C# line 28: Console.WriteLine(finalStr);
            printf("%s\n", buf);                                        // C# line 28

            // C# line 30: lastPrintTime = EvadeUtils.TickCount;
            lastPrintTime = EvadeUtils::TickCount();                    // C# line 30
        }
    };

} // namespace EzEvade
