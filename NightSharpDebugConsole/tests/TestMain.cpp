#include <cstdio>
#include <exception>

void RunCrashProtocolTests();
void RunIncidentModelTests();
void RunWerLocalDumpsTests();
void RunCrashReportTests();
void RunNightSharpCrashBridgeCompileTest();

int main() {
    try {
        RunCrashProtocolTests();
        RunIncidentModelTests();
        RunWerLocalDumpsTests();
        RunCrashReportTests();
        RunNightSharpCrashBridgeCompileTest();
        std::puts("NightSharpDebugConsoleTests: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "NightSharpDebugConsoleTests: FAIL: %s\n", ex.what());
        return 1;
    }
}
