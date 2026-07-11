#include "../src/CrashReport.h"

#include <stdexcept>
#include <string>

namespace {

void RequireContains(const std::string& text, const char* expected) {
    if (text.find(expected) == std::string::npos) {
        throw std::runtime_error(std::string("report missing: ") + expected);
    }
}

} // namespace

void RunCrashReportTests() {
    nscrash::SharedState state{};
    state.pid = 4321;
    state.moduleBase = 0x00007FF700000000ull;
    strcpy_s(state.phase, "d3d11hook-plugins");
    strcpy_s(state.lastStage, "Core::Events::OnGameUpdate/callback[3]");

    nsmonitor::RemoteCrashData crash{};
    crash.valid = true;
    crash.threadId = 77;
    crash.exceptionCode = EXCEPTION_ACCESS_VIOLATION;
    crash.exceptionAddress = 0x00007FF700001234ull;
    crash.rip = crash.exceptionAddress;
    crash.rsp = 0x1000;
    crash.rbp = 0x2000;
    crash.moduleName = "NightSharp.dll";
    crash.moduleBase = state.moduleBase;
    crash.moduleOffset = 0x1234;
    crash.stack.push_back("#00 NightSharp.dll!CrashSite+0x4");

    nsmonitor::DumpResult dump{true, ERROR_SUCCESS, 4096};
    const std::string report = nsmonitor::FormatIncidentReport(
        nsmonitor::IncidentClass::UnhandledException,
        nscrash::CaptureSource::Bridge,
        state,
        crash,
        dump,
        L"C:\\Users\\Public\\NightSharpDumps\\sample.dmp",
        0xC0000005u);

    RequireContains(report, "classification=unhandled-exception");
    RequireContains(report, "capture_source=external-bridge");
    RequireContains(report, "exception_code=0xC0000005");
    RequireContains(report, "rip=0x00007FF700001234");
    RequireContains(report, "fault_module=NightSharp.dll");
    RequireContains(report, "fault_offset=0x1234");
    RequireContains(report, "phase=d3d11hook-plugins");
    RequireContains(report, "stage=Core::Events::OnGameUpdate/callback[3]");
    RequireContains(report, "stack=#00 NightSharp.dll!CrashSite+0x4");
}
