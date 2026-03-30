#pragma once

namespace SDK {

enum class SpellType : int {
    Unknown = 0,
    Targeted = 1,
    Line = 2,
    Circle = 3,
    Cone = 4,
    SkillshotLine = 2,
    SkillshotCircle = 3,
    SkillshotCone = 4
};

} // namespace SDK
