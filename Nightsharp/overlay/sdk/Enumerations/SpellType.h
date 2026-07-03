#pragma once

#include <cstdint>

#include "SkillshotType.h"

namespace SDK {

enum class SpellType : std::int32_t {
    // Runtime EnsoulSharp.SDK.dll shape used by PredictionInput.Type.
    Line = 0,
    Circle = 1,
    Cone = 2,
    Arc = 3,
    None = 4,

    // NightSharp compatibility aliases used by the spell database/tracker.
    SkillshotLine = Line,
    SkillshotCircle = Circle,
    SkillshotCone = Cone,
    SkillshotArc = Arc,

    SkillshotMissileLine = 10,
    SkillshotMissileCircle = 11,
    SkillshotMissileCone = 12,
    SkillshotMissileArc = 13,
    SkillshotRing = 14,

    Targeted = 20,
    TargetedMissile = 21,
    Toggled = 22,
    Activated = 23,
    Passive = 24,
    Position = 25,
    Vector = 26,
};

constexpr SpellType ToSpellType(SkillshotType type) {
    switch (type) {
    case SkillshotType::SkillshotLine:
        return SpellType::Line;
    case SkillshotType::SkillshotCircle:
        return SpellType::Circle;
    case SkillshotType::SkillshotCone:
        return SpellType::Cone;
    case SkillshotType::SkillshotArc:
        return SpellType::Arc;
    case SkillshotType::SkillshotNone:
    default:
        return SpellType::None;
    }
}

constexpr SkillshotType ToSkillshotType(SpellType type) {
    if (type == SpellType::Line || type == SpellType::SkillshotMissileLine) {
        return SkillshotType::SkillshotLine;
    }
    if (type == SpellType::Circle || type == SpellType::SkillshotMissileCircle) {
        return SkillshotType::SkillshotCircle;
    }
    if (type == SpellType::Cone || type == SpellType::SkillshotMissileCone) {
        return SkillshotType::SkillshotCone;
    }
    if (type == SpellType::Arc || type == SpellType::SkillshotMissileArc) {
        return SkillshotType::SkillshotArc;
    }
    return SkillshotType::SkillshotNone;
}

constexpr bool IsLineSpellType(SpellType type) {
    return type == SpellType::Line || type == SpellType::SkillshotMissileLine;
}

constexpr bool IsCircleSpellType(SpellType type) {
    return type == SpellType::Circle || type == SpellType::SkillshotMissileCircle;
}

constexpr bool IsConeSpellType(SpellType type) {
    return type == SpellType::Cone || type == SpellType::SkillshotMissileCone;
}

constexpr bool IsArcSpellType(SpellType type) {
    return type == SpellType::Arc || type == SpellType::SkillshotMissileArc;
}

} // namespace SDK
