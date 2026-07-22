#pragma once

#include "../Detection/Threat.h"
#include "EvadeRoutingPolicy.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace ZDEvade {

enum class PlannerCandidateSource {
    Unknown,
    LineLeft,
    LineRight,
    CircleExit,
    ConeSide,
    ArcExit,
    Intersection,
    Cursor,
    Ring,
    EvadeSpell
};

struct CandidateBudget {
    int analytical = 0;
    int cursor = 0;
    int singleThreatDetour = 0;
    int exactIntersections = 0;
    int radialFallback = 0;

    static CandidateBudget ForMax(int maxCandidates) {
        CandidateBudget result;
        const int capacity = std::max(0, maxCandidates);
        if (capacity == 0) return result;

        result.cursor = 1;
        int remaining = capacity - result.cursor;
        result.radialFallback = std::min(
            remaining,
            capacity >= 32 ? std::max(8, capacity / 4) : capacity / 4);
        remaining -= result.radialFallback;
        result.analytical = std::min(remaining, capacity / 4);
        remaining -= result.analytical;
        result.singleThreatDetour = std::min(remaining, capacity / 8);
        remaining -= result.singleThreatDetour;
        result.exactIntersections = remaining;
        return result;
    }

    int Total() const {
        return analytical + cursor + singleThreatDetour +
               exactIntersections + radialFallback;
    }
};

struct CandidateSeed {
    Vec2 position = {};
    PlannerCandidateSource source = PlannerCandidateSource::Unknown;
    int threatId = -1;
    int stabilityBranchKey = StabilityBranch::Unknown;
};

enum class PlannerRejectReason {
    None,
    Invalid,
    Wall,
    EndpointDanger,
    PathDanger,
    Reentry,
    Late,
    NoExit
};

enum class EvadeControllerState {
    Idle,
    Assessing,
    StrictEvade,
    FallbackEvade,
    ReroutingPath,
    Release
};

struct ThreatRule {
    bool enabled = true;
    int danger = 1;
    float dodgeHealthPercent = 100.0f;
};

using ThreatRuleMap = std::unordered_map<std::string, ThreatRule>;

struct EvadeSettings {
    float endpointBuffer = kDefaultEndpointMargin;
    float pathBuffer = 8.0f;
    float releaseBuffer = 48.0f;
    float pathStep = 22.0f;
    float temporalStepMs = 18.0f;
    float ringStep = 35.0f;
    float maxSearchRadius = 760.0f;
    float inputDelayMs = 55.0f;
    float minimumTimeMarginMs = 25.0f;
    float preferredClearance = 18.0f;
    float maxThreatHorizonMs = 1800.0f;
    float targetSwitchDistance = 40.0f;
    float directionBlend = 0.40f;
    float lockedDirectionBlend = 0.25f;
    int targetLockMs = 160;
    int nearWallHoldMinimumMs = 240;
    int nearWallHoldMaximumMs = 520;
    int maxCandidates = 320;
};

inline constexpr float kMaximumAnalysisHorizonMs = 10000.0f;
inline constexpr int kMaximumAnalysisSamples = 4096;

inline EvadeSettings NormalizeEvadeSettings(const EvadeSettings& input) {
    const EvadeSettings defaults;
    const auto finiteOr = [](float value, float fallback) {
        return std::isfinite(value) ? value : fallback;
    };
    EvadeSettings result = input;
    result.endpointBuffer = std::clamp(
        finiteOr(input.endpointBuffer, defaults.endpointBuffer), 0.0f, 1000.0f);
    result.pathBuffer = std::clamp(
        finiteOr(input.pathBuffer, defaults.pathBuffer), 0.0f, 1000.0f);
    result.releaseBuffer = std::clamp(
        finiteOr(input.releaseBuffer, defaults.releaseBuffer), 0.0f, 1000.0f);
    result.pathStep = std::clamp(
        finiteOr(input.pathStep, defaults.pathStep), 4.0f, 500.0f);
    result.temporalStepMs = std::clamp(
        finiteOr(input.temporalStepMs, defaults.temporalStepMs), 6.0f, 24.0f);
    result.ringStep = std::clamp(
        finiteOr(input.ringStep, defaults.ringStep), 20.0f, 1000.0f);
    result.maxSearchRadius = std::clamp(
        finiteOr(input.maxSearchRadius, defaults.maxSearchRadius), 0.0f, 5000.0f);
    result.inputDelayMs = std::clamp(
        finiteOr(input.inputDelayMs, defaults.inputDelayMs),
        0.0f,
        kMaximumAnalysisHorizonMs);
    result.minimumTimeMarginMs = std::clamp(
        finiteOr(input.minimumTimeMarginMs, defaults.minimumTimeMarginMs),
        0.0f,
        kMaximumAnalysisHorizonMs);
    result.preferredClearance = std::clamp(
        finiteOr(input.preferredClearance, defaults.preferredClearance),
        0.0f,
        5000.0f);
    result.maxThreatHorizonMs = std::clamp(
        finiteOr(input.maxThreatHorizonMs, defaults.maxThreatHorizonMs),
        0.0f,
        kMaximumAnalysisHorizonMs);
    result.targetSwitchDistance = std::clamp(
        finiteOr(input.targetSwitchDistance, defaults.targetSwitchDistance),
        0.0f,
        5000.0f);
    result.directionBlend = std::clamp(
        finiteOr(input.directionBlend, defaults.directionBlend), 0.0f, 1.0f);
    result.lockedDirectionBlend = std::clamp(
        finiteOr(input.lockedDirectionBlend, defaults.lockedDirectionBlend),
        0.0f,
        1.0f);
    result.maxCandidates = std::clamp(input.maxCandidates, 0, 320);
    return result;
}

inline int AnalysisSampleCount(float durationMs, float stepMs) {
    const float duration = std::clamp(
        std::isfinite(durationMs) ? durationMs : kMaximumAnalysisHorizonMs,
        0.0f,
        kMaximumAnalysisHorizonMs);
    const float step = std::clamp(
        std::isfinite(stepMs) ? stepMs : 18.0f,
        1.0f,
        kMaximumAnalysisHorizonMs);
    const double requested = std::ceil(
        static_cast<double>(duration) / static_cast<double>(step));
    return std::clamp(
        static_cast<int>(requested),
        2,
        kMaximumAnalysisSamples);
}

struct CollisionIdentity {
    bool initializedId = false;
    std::size_t value = 0;

    bool operator==(const CollisionIdentity& other) const {
        return initializedId == other.initializedId &&
               value == other.value;
    }
};

inline CollisionIdentity MakeCollisionIdentity(
    int threatId,
    std::size_t inputRecordIndex) {
    return threatId >= 0
        ? CollisionIdentity{true, static_cast<std::size_t>(threatId)}
        : CollisionIdentity{false, inputRecordIndex};
}

inline constexpr std::size_t kInlineCollisionIdentityCapacity = 16;

// Active collision sets normally fit inline. Heap storage is used only after
// the seventeenth distinct collision identity.
class CollisionIdentitySet {
public:
    bool Add(const CollisionIdentity& identity) {
        if (Contains(identity)) return false;
        if (inlineSize < inlineValues.size()) {
            inlineValues[inlineSize++] = identity;
        } else {
            overflowValues.push_back(identity);
        }
        return true;
    }

    void AddAll(const CollisionIdentitySet& other) {
        for (std::size_t index = 0; index < other.inlineSize; ++index)
            Add(other.inlineValues[index]);
        for (const CollisionIdentity& identity : other.overflowValues)
            Add(identity);
    }

    std::size_t Size() const {
        return inlineSize + overflowValues.size();
    }

private:
    bool Contains(const CollisionIdentity& identity) const {
        for (std::size_t index = 0; index < inlineSize; ++index) {
            if (inlineValues[index] == identity) return true;
        }
        for (const CollisionIdentity& existing : overflowValues) {
            if (existing == identity) return true;
        }
        return false;
    }

    std::array<CollisionIdentity, kInlineCollisionIdentityCapacity>
        inlineValues = {};
    std::size_t inlineSize = 0;
    std::vector<CollisionIdentity> overflowValues;
};

class ExposureDangerSet {
public:
    void AddOrMax(const CollisionIdentity& identity, int danger) {
        const int safeDanger = std::max(0, danger);
        for (Entry& entry : entries) {
            if (entry.identity == identity) {
                entry.danger = std::max(entry.danger, safeDanger);
                return;
            }
        }
        entries.push_back({identity, safeDanger});
    }

    void AddAll(const ExposureDangerSet& other) {
        for (const Entry& entry : other.entries)
            AddOrMax(entry.identity, entry.danger);
    }

    int SummedDanger() const {
        std::int64_t sum = 0;
        for (const Entry& entry : entries)
            sum += entry.danger;
        return static_cast<int>(std::min<std::int64_t>(
            sum,
            std::numeric_limits<int>::max()));
    }

private:
    struct Entry {
        CollisionIdentity identity;
        int danger = 0;
    };

    std::vector<Entry> entries;
};

struct CandidateEvaluation {
    Vec2 position = {};
    PlannerCandidateSource source = PlannerCandidateSource::Unknown;
    int sourceThreatId = -1;
    int stabilityBranchKey = StabilityBranch::Unknown;
    bool valid = false;
    bool walkable = false;
    bool endpointSafe = false;
    bool pathSafe = false;
    bool timingSafe = false;
    bool strictSafe = false;
    bool reenteredDanger = false;
    int endpointDanger = 0;
    int pathDanger = 0;
    int maxDanger = 0;
    int collisionCount = 0;
    float travelDistance = FLT_MAX;
    float exitDistance = FLT_MAX;
    float arrivalTimeMs = FLT_MAX;
    float timeMarginMs = -FLT_MAX;
    float minimumClearance = FLT_MAX;
    float firstCollisionTimeMs = FLT_MAX;
    float dangerExposureMs = 0.0f;
    int summedExposureDanger = 0;
    float cursorDistance = FLT_MAX;
    float enemyDistance = FLT_MAX;
    float turretPenalty = 0.0f;
    PlannerRejectReason rejectReason = PlannerRejectReason::Invalid;
};

inline void CarryStabilityBranchKey(
    CandidateEvaluation& refreshed,
    const CandidateEvaluation& established) {
    refreshed.stabilityBranchKey =
        established.stabilityBranchKey;
}

struct PlannerResult {
    bool found = false;
    bool strictSafe = false;
    CandidateEvaluation selected;
    std::vector<CandidateEvaluation> candidates;
};

inline const char* ControllerStateName(EvadeControllerState state) {
    switch (state) {
    case EvadeControllerState::Idle: return "idle";
    case EvadeControllerState::Assessing: return "assessing";
    case EvadeControllerState::StrictEvade: return "strict";
    case EvadeControllerState::FallbackEvade: return "fallback";
    case EvadeControllerState::ReroutingPath: return "reroute";
    case EvadeControllerState::Release: return "release";
    default: return "unknown";
    }
}

inline const char* CandidateSourceName(PlannerCandidateSource source) {
    switch (source) {
    case PlannerCandidateSource::LineLeft: return "line-left";
    case PlannerCandidateSource::LineRight: return "line-right";
    case PlannerCandidateSource::CircleExit: return "circle";
    case PlannerCandidateSource::ConeSide: return "cone";
    case PlannerCandidateSource::ArcExit: return "arc";
    case PlannerCandidateSource::Intersection: return "intersection";
    case PlannerCandidateSource::Cursor: return "cursor";
    case PlannerCandidateSource::Ring: return "ring";
    case PlannerCandidateSource::EvadeSpell: return "spell";
    default: return "unknown";
    }
}

inline const char* RejectReasonName(PlannerRejectReason reason) {
    switch (reason) {
    case PlannerRejectReason::None: return "none";
    case PlannerRejectReason::Invalid: return "invalid";
    case PlannerRejectReason::Wall: return "wall";
    case PlannerRejectReason::EndpointDanger: return "endpoint";
    case PlannerRejectReason::PathDanger: return "path";
    case PlannerRejectReason::Reentry: return "reentry";
    case PlannerRejectReason::Late: return "late";
    case PlannerRejectReason::NoExit: return "no-exit";
    default: return "unknown";
    }
}

}
