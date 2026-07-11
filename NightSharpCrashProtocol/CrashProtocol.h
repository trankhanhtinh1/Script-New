#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <type_traits>

namespace nscrash {

inline constexpr std::uint32_t kMagic = 0x5243534Eu; // NSCR
inline constexpr std::uint16_t kVersion = 1;
inline constexpr char kPipeName[] = R"(\\.\pipe\NightSharpDebugConsole)";
inline constexpr std::size_t kStageLength = 160;
inline constexpr std::size_t kPhaseLength = 128;
inline constexpr std::size_t kLogLength = 512;
inline constexpr std::size_t kRecentExceptionCount = 16;
inline constexpr std::size_t kRecentLogCount = 64;

enum class MessageType : std::uint16_t {
    Hello = 1,
    Log = 2,
};

enum class CrashKind : std::uint32_t {
    None = 0,
    Handled = 1,
    Unhandled = 2,
};

enum class CaptureSource : std::uint32_t {
    None = 0,
    Bridge = 1,
    Wer = 2,
    ExitOnly = 3,
};

struct PacketHeader {
    std::uint32_t magic{};
    std::uint16_t version{};
    MessageType type{};
    std::uint32_t size{};
    std::uint32_t pid{};
    std::uint64_t sequence{};
};

struct HelloPacket {
    PacketHeader header{};
    std::uint64_t moduleBase{};
    std::uint32_t moduleSize{};
    char mappingName[96]{};
    char crashReadyEventName[96]{};
    char dumpCompleteEventName[96]{};
};

struct LogPacket {
    PacketHeader header{};
    std::uint32_t length{};
    char text[kLogLength]{};
};

struct FirstChanceSummary {
    std::uint64_t serial{};
    std::uint64_t timestamp100ns{};
    std::uint32_t threadId{};
    std::uint32_t code{};
    std::uint64_t address{};
};

struct RecentLogLine {
    std::uint64_t serial{};
    char text[kLogLength]{};
};

struct CrashRecord {
    volatile LONG claimed{};
    CrashKind kind{CrashKind::None};
    std::uint32_t threadId{};
    std::uint32_t exceptionCode{};
    std::uint64_t exceptionAddress{};
    std::uint64_t exceptionPointers{};
    char stage[kStageLength]{};
    volatile LONG dumpFinished{};
    volatile LONG dumpSucceeded{};
    std::uint32_t dumpError{};
};

struct SharedState {
    std::uint32_t magic{};
    std::uint16_t version{};
    std::uint16_t reserved{};
    std::uint32_t size{};
    volatile LONG sequence{};
    std::uint32_t pid{};
    std::uint64_t moduleBase{};
    std::uint32_t moduleSize{};
    std::uint64_t heartbeatTick{};
    char phase[kPhaseLength]{};
    char lastStage[kStageLength]{};
    volatile LONG firstChanceSerial{};
    FirstChanceSummary firstChance[kRecentExceptionCount]{};
    volatile LONG logSerial{};
    volatile LONG droppedLogs{};
    RecentLogLine recentLogs[kRecentLogCount]{};
    CrashRecord crash{};
};

inline void InitializeHeader(
    PacketHeader& header,
    MessageType type,
    std::uint32_t size,
    std::uint32_t pid,
    std::uint64_t sequence) {
    header.magic = kMagic;
    header.version = kVersion;
    header.type = type;
    header.size = size;
    header.pid = pid;
    header.sequence = sequence;
}

inline bool ValidateHeader(
    const PacketHeader& header,
    MessageType type,
    std::uint32_t size) {
    return header.magic == kMagic &&
           header.version == kVersion &&
           header.type == type &&
           header.size == size &&
           header.pid != 0;
}

inline bool ValidateSharedState(const SharedState& state, DWORD expectedPid) {
    return state.magic == kMagic &&
           state.version == kVersion &&
           state.size == sizeof(SharedState) &&
           state.pid == expectedPid &&
           expectedPid != 0;
}

inline void FormatName(
    const char* prefix,
    DWORD pid,
    char* out,
    std::size_t outSize) {
    if (!out || outSize == 0) {
        return;
    }
    std::snprintf(
        out,
        outSize,
        "Local\\%s-%lu",
        prefix,
        static_cast<unsigned long>(pid));
    out[outSize - 1] = '\0';
}

inline void FormatMappingName(DWORD pid, char* out, std::size_t outSize) {
    FormatName("NightSharpCrashState", pid, out, outSize);
}

inline void FormatCrashReadyEventName(DWORD pid, char* out, std::size_t outSize) {
    FormatName("NightSharpCrashReady", pid, out, outSize);
}

inline void FormatDumpCompleteEventName(DWORD pid, char* out, std::size_t outSize) {
    FormatName("NightSharpDumpComplete", pid, out, outSize);
}

static_assert(std::is_trivially_copyable_v<SharedState>);
static_assert(sizeof(SharedState) <= 64 * 1024);

} // namespace nscrash
