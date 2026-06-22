#pragma once

#include <cstdint>

namespace SDK {

enum class SpellType : std::int32_t {
    SkillshotCircle,
    SkillshotMissileCircle,
    SkillshotLine,
    SkillshotMissileLine,
    SkillshotCone,
    SkillshotMissileCone,
    SkillshotMissileArc,
    SkillshotRing,
    SkillshotArc,
    Targeted,
    TargetedMissile,
    Toggled,
    Activated,
    Passive,
    Position,
    Vector,
};

} // namespace SDK
