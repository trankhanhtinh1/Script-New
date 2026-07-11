#pragma once

#include "../Detection/Threat.h"

#include <cfloat>
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
    Release
};

struct ThreatRule {
    bool enabled = true;
    int danger = 1;
    float dodgeHealthPercent = 100.0f;
};

using ThreatRuleMap = std::unordered_map<std::string, ThreatRule>;

struct EvadeSettings {
    float endpointBuffer = 24.0f;
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
    int maxCandidates = 480;
};

struct CandidateEvaluation {
    Vec2 position = {};
    PlannerCandidateSource source = PlannerCandidateSource::Unknown;
    int sourceThreatId = -1;
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
    float cursorDistance = FLT_MAX;
    float enemyDistance = FLT_MAX;
    float turretPenalty = 0.0f;
    PlannerRejectReason rejectReason = PlannerRejectReason::Invalid;
};

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
