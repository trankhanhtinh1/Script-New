#pragma once

#include "Globals.h"
#include "offset.h"

#include <cstdint>

namespace CoreAttackableUnit {

struct Snapshot {
    uintptr_t address = 0;
    float health = 0.0f;
    float maxHealth = 0.0f;
    float healthMaxPenalty = 0.0f;
    float allShield = 0.0f;
    float physicalShield = 0.0f;
    float magicalShield = 0.0f;
    float championSpecific = 0.0f;
    float incomingHealAlly = 0.0f;
    float incomingHealEnemy = 0.0f;
    float incomingDamage = 0.0f;
    float stopShieldFade = 0.0f;
    bool isTargetable = false;
    std::uint32_t targetableFlags = 0;
    std::uint32_t actionState1 = 0;
    std::uint32_t actionState2 = 0;

    bool IsValid() const {
        return Globals::IsValidPtr(address);
    }

    float TotalShield() const {
        return allShield + physicalShield + magicalShield;
    }

    float EffectiveHealth(bool includeShields = true) const {
        return health + (includeShields ? TotalShield() : 0.0f);
    }
};

template <typename T>
inline T ReadField(uintptr_t object, uintptr_t offset) {
    if (!Globals::IsValidPtr(object)) {
        return T{};
    }
    return Globals::Read<T>(object + offset);
}

inline bool ReadBool(uintptr_t object, uintptr_t offset) {
    return ReadField<std::uint8_t>(object, offset) != 0;
}

inline float Health(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::HP);
}

inline float MaxHealth(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::MaxHP);
}

inline float HealthMaxPenalty(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::HPMaxPenalty);
}

inline float AllShield(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::AllShield);
}

inline float PhysicalShield(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::PhysicalShield);
}

inline float MagicalShield(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::MagicalShield);
}

inline float ChampionSpecific(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::ChampSpecific);
}

inline float IncomingHealAlly(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::InHealAllied);
}

inline float IncomingHealEnemy(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::InHealEnemy);
}

inline float IncomingDamage(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::InDamage);
}

inline float StopShieldFade(uintptr_t object) {
    return ReadField<float>(object, Offset::AttackableUnit::StopShieldFade);
}

inline bool IsTargetable(uintptr_t object) {
    return ReadBool(object, Offset::AttackableUnit::IsTargetable);
}

inline std::uint32_t TargetableFlags(uintptr_t object) {
    return ReadField<std::uint32_t>(object, Offset::AttackableUnit::TargetableFlags);
}

inline std::uint32_t ActionState1(uintptr_t object) {
    return ReadField<std::uint32_t>(object, Offset::AttackableUnit::ActionState1);
}

inline std::uint32_t ActionState2(uintptr_t object) {
    return ReadField<std::uint32_t>(object, Offset::AttackableUnit::ActionState2);
}

inline bool CanAttack(uintptr_t object) {
    return (ActionState1(object) & 0x1u) != 0u;
}

inline bool CanCast(uintptr_t object) {
    return (ActionState1(object) & 0x2u) != 0u;
}

inline bool CanMove(uintptr_t object) {
    const std::uint32_t state = ActionState1(object);
    return (state & 0x200u) == 0u && (state & 0x4u) != 0u;
}

inline bool CanWalk(uintptr_t object) {
    return (ActionState1(object) & 0x8u) != 0u;
}

inline float TotalShield(uintptr_t object) {
    return AllShield(object) + PhysicalShield(object) + MagicalShield(object);
}

inline float EffectiveHealth(uintptr_t object, bool includeShields = true) {
    return Health(object) + (includeShields ? TotalShield(object) : 0.0f);
}

inline float IncomingHealthDelta(uintptr_t object, bool alliedSource = true) {
    return (alliedSource ? IncomingHealAlly(object) : IncomingHealEnemy(object)) -
           IncomingDamage(object);
}

inline Snapshot Read(uintptr_t object) {
    Snapshot snapshot{};
    snapshot.address = object;
    if (!Globals::IsValidPtr(object)) {
        return snapshot;
    }

    snapshot.health = Health(object);
    snapshot.maxHealth = MaxHealth(object);
    snapshot.healthMaxPenalty = HealthMaxPenalty(object);
    snapshot.allShield = AllShield(object);
    snapshot.physicalShield = PhysicalShield(object);
    snapshot.magicalShield = MagicalShield(object);
    snapshot.championSpecific = ChampionSpecific(object);
    snapshot.incomingHealAlly = IncomingHealAlly(object);
    snapshot.incomingHealEnemy = IncomingHealEnemy(object);
    snapshot.incomingDamage = IncomingDamage(object);
    snapshot.stopShieldFade = StopShieldFade(object);
    snapshot.isTargetable = IsTargetable(object);
    snapshot.targetableFlags = TargetableFlags(object);
    snapshot.actionState1 = ActionState1(object);
    snapshot.actionState2 = ActionState2(object);
    return snapshot;
}

} // namespace CoreAttackableUnit
