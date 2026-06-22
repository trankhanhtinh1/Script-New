#pragma once

#include <cstdint>

namespace SDK {

enum class SpellTags : std::int32_t {
    Damage,
    AoE,
    AppliesOnHitEffects,
    CrowdControl,
    Shield,
    Heal,
    Stasis,
    LeavesMark,
    CanDetonateMark,
    Transformation,
    Dash,
    Blink,
    Teleport,
    DamageAmplifier,
    DefensiveBuff,
    MovementSpeedAmplifier,
    AttackSpeedAmplifier,
    AttackRangeModifier,
    SpellShield,
    RemoveCrowdControl,
    GrantsVision,
    Interruptable,
};

} // namespace SDK
