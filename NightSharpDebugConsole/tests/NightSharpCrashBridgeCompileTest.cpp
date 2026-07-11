#include "../../NightSharp/CrashBridge.h"

#include <type_traits>

void RunNightSharpCrashBridgeCompileTest() {
    static_assert(std::is_same_v<
                  decltype(&NightSharpDebug::CrashBridge::Install),
                  bool (*)(HMODULE)>);
    static_assert(std::is_same_v<
                  decltype(&NightSharpDebug::CrashBridge::PublishPhase),
                  void (*)(const char*)>);
    static_assert(std::is_same_v<
                  decltype(&NightSharpDebug::CrashBridge::CaptureException),
                  bool (*)(
                      nscrash::CrashKind,
                      const char*,
                      EXCEPTION_POINTERS*)>);
}
