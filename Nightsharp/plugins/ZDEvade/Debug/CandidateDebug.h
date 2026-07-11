#pragma once

#include "../../../SDK/SDK.h"

#include <cfloat>
#include <cstdint>

namespace ZDEvade {

enum class CandidateSource {
    Unknown,
    DirectSide,
    FarSide,
    ClosestEdge,
    Backstep,
    Diagonal,
    HeadOn,
    Ring,
    Mouse,
    EvadeSpell
};

enum class CandidateSide {
    None,
    Left,
    Right,
    Center
};

struct CandidateDebugPoint {
    Vec2 position = {};
    CandidateSource source = CandidateSource::Unknown;
    CandidateSide side = CandidateSide::None;
    int spellId = -1;
    int rank = -1;
    bool selected = false;
    bool rejectPosition = false;
    bool isDangerousPos = false;
    int posDangerLevel = 0;
    int posDangerCount = 0;
    float travelDistance = FLT_MAX;
    float travelTimeMs = FLT_MAX;
    float escapeDistance = FLT_MAX;
    float closestDistance = FLT_MAX;
    float timeMargin = FLT_MAX;
    float exitDistance = FLT_MAX;
    float arrivalTimeMs = FLT_MAX;
    bool endpointSafe = true;
    bool pathSafe = true;
    bool collisionSafe = true;
    int rejectReason = 0;
};

inline const char* CandidateSourceName(CandidateSource source) {
    switch (source) {
    case CandidateSource::DirectSide: return "direct";
    case CandidateSource::FarSide: return "far";
    case CandidateSource::ClosestEdge: return "edge";
    case CandidateSource::Backstep: return "back";
    case CandidateSource::Diagonal: return "diag";
    case CandidateSource::HeadOn: return "head";
    case CandidateSource::Ring: return "ring";
    case CandidateSource::Mouse: return "mouse";
    case CandidateSource::EvadeSpell: return "spell";
    default: return "unknown";
    }
}

inline const char* CandidateSideName(CandidateSide side) {
    switch (side) {
    case CandidateSide::Left: return "L";
    case CandidateSide::Right: return "R";
    case CandidateSide::Center: return "C";
    default: return "";
    }
}

} // namespace ZDEvade
