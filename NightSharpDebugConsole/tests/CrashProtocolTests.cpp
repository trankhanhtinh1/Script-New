#include "../../NightSharpCrashProtocol/CrashProtocol.h"

#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

} // namespace

void RunCrashProtocolTests() {
    nscrash::PacketHeader header{};
    nscrash::InitializeHeader(
        header,
        nscrash::MessageType::Hello,
        sizeof(nscrash::HelloPacket),
        4321,
        7);
    Require(
        nscrash::ValidateHeader(
            header,
            nscrash::MessageType::Hello,
            sizeof(nscrash::HelloPacket)),
        "valid hello header rejected");

    ++header.version;
    Require(
        !nscrash::ValidateHeader(
            header,
            nscrash::MessageType::Hello,
            sizeof(nscrash::HelloPacket)),
        "version mismatch accepted");

    char name[96] = {};
    nscrash::FormatMappingName(4321, name, sizeof(name));
    Require(
        std::string(name) == "Local\\NightSharpCrashState-4321",
        "mapping name mismatch");

    nscrash::FormatCrashReadyEventName(4321, name, sizeof(name));
    Require(
        std::string(name) == "Local\\NightSharpCrashReady-4321",
        "ready event name mismatch");

    nscrash::FormatDumpCompleteEventName(4321, name, sizeof(name));
    Require(
        std::string(name) == "Local\\NightSharpDumpComplete-4321",
        "complete event name mismatch");

    static_assert(std::is_trivially_copyable_v<nscrash::SharedState>);
    static_assert(sizeof(nscrash::SharedState) <= 64 * 1024);
}
