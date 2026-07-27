#pragma once

#include "../Detection/Threat.h"
#include "../../../SDK/SDK.h"
#include <string>
#include <vector>

namespace ZDEvade {

enum class SelfSkillDebugPhase : std::uint8_t {
    Pending,
    Live,
    Terminal
};

struct SelfSkillDebugVisibility {
    bool masterEnabled = false;
    bool drawPending = true;
    bool drawLive = true;
};

inline bool ShouldDrawSelfSkillPhase(
        const SelfSkillDebugVisibility& visibility,
        SelfSkillDebugPhase phase) {
    if (!visibility.masterEnabled) return false;
    return phase == SelfSkillDebugPhase::Pending
        ? visibility.drawPending
        : visibility.drawLive;
}

struct SelfSkillDebugSnapshot {
    std::uint64_t id = 0;
    SelfSkillDebugPhase phase = SelfSkillDebugPhase::Pending;
    Threat visual = {};
    std::uint32_t missileNetworkId = 0;
};

struct SelfSkillDebugCounters {
    unsigned long long processSeen = 0;
    unsigned long long processMatched = 0;
    unsigned long long processRejected = 0;
    unsigned long long processUnmatched = 0;
    unsigned long long missileCreateMatched = 0;
    unsigned long long missileCreateOrphan = 0;
    unsigned long long missileCreateDuplicate = 0;
    unsigned long long missileCreateRejected = 0;
    unsigned long long missileCreateUnmatched = 0;
    unsigned long long missileDeleteMatched = 0;
    unsigned long long missileDeleteUnmatched = 0;
    unsigned long long timeouts = 0;
    unsigned long long capacityDrops = 0;
};

class SelfSkillDebug {
public:
    static constexpr std::size_t kCapacity = 32;

    struct Diagnostics {
        char champion[64] = {};
        char lastMatchedProcess[96] = {};
        char lastUnmatchedProcess[96] = {};
        char lastMatchedMissile[96] = {};
        char lastUnmatchedMissile[96] = {};
        std::size_t databaseEntries = 0;
        std::size_t pending = 0;
        std::size_t live = 0;
        std::size_t terminal = 0;
        SelfSkillDebugCounters counters = {};
    };

    void Configure(std::uint32_t localPlayerNetworkId,
                   const char* championName) {}

    void Clear() {}

    void OnProcessSpell(
            const SDK::Events::ProcessSpellEventArgs& args,
            int now) {}

    void OnMissileCreate(
            const SDK::Events::ObjectEventArgs& args,
            int now) {}

    void OnMissileDelete(
            const SDK::Events::ObjectEventArgs& args,
            int now,
            int terminalHoldMs) {}

    void OnGameUpdate(int now) {}

    std::vector<SelfSkillDebugSnapshot> Snapshot() const {
        return {};
    }

    Diagnostics ReadDiagnostics() const {
        return {};
    }

    static void Draw(const SelfSkillDebugSnapshot& snapshot,
                     int now,
                     float planeY,
                     bool drawLabels,
                     bool drawEndpoints) {}
};

} // namespace ZDEvade
