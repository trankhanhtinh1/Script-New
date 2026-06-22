#pragma once

#include <cstdint>

namespace SDK {

enum class SkillshotDetectionType : std::int32_t {
    CreateObject,
    ProcessSpell,
    MissileCreate,
};

} // namespace SDK
