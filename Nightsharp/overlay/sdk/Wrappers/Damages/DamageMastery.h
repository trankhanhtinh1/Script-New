#pragma once

#include "../../Enumerations/DamageType.h"
#include "../../Core/Objects.h"

#include <algorithm>

namespace SDK::DamageMastery {

namespace detail {

inline constexpr int RunePressTheAttack = 8005;
inline constexpr int RuneCoupDeGrace = 8014;
inline constexpr int RuneCutDown = 8017;
inline constexpr int RuneLastStand = 8299;
inline constexpr int RuneFirstStrike = 8369;

inline bool HasRune(const AIHeroClient& source, int runeId) {
    return source.IsValid() && source.HasRune(runeId);
}

inline bool HasAnyBuff(const AIBaseClient& unit, const char* const* names, int count) {
    for (int i = 0; i < count; ++i) {
        if (names[i] && unit.HasBuff(names[i])) {
            return true;
        }
    }
    return false;
}

inline bool HasFirstStrikeActiveBuff(const AIHeroClient& source) {
    static constexpr const char* kNames[] = {
        "FirstStrike",
        "firststrike",
        "FirstStrikeBuff",
        "firststrikebuff",
        "FirstStrikeDamage",
        "firststrikedamage",
        "FirstStrikeAvailable",
        "firststrikeavailable",
        "FirstStrikeReady",
        "firststrikeready",
    };
    return HasAnyBuff(source, kNames, static_cast<int>(sizeof(kNames) / sizeof(kNames[0])));
}

inline bool HasPressTheAttackDamageAmp(const AIHeroClient& source,
                                       const AIBaseClient& target) {
    static constexpr const char* kSourceNames[] = {
        "PressTheAttack",
        "presstheattack",
        "PressTheAttackBuff",
        "presstheattackbuff",
        "PressTheAttackDamageAmp",
        "presstheattackdamageamp",
    };
    static constexpr const char* kTargetNames[] = {
        "PressTheAttackDebuff",
        "presstheattackdebuff",
        "PressTheAttackExpose",
        "presstheattackexpose",
        "PressTheAttackExposed",
        "presstheattackexposed",
    };
    return HasAnyBuff(source, kSourceNames, static_cast<int>(sizeof(kSourceNames) / sizeof(kSourceNames[0]))) ||
           HasAnyBuff(target, kTargetNames, static_cast<int>(sizeof(kTargetNames) / sizeof(kTargetNames[0])));
}

inline float LastStandBonus(const AIHeroClient& source) {
    const float hp = std::clamp(source.HealthPercent(), 0.0f, 100.0f);
    if (hp >= 60.0f) {
        return 0.0f;
    }
    if (hp <= 30.0f) {
        return 0.11f;
    }
    const float t = (60.0f - hp) / 30.0f;
    return 0.05f + t * 0.06f;
}

} // namespace detail

inline float GetOutgoingDamageModifier(const AIHeroClient& source,
                                       const AIBaseClient& target,
                                       DamageType damageType) {
    if (!source.IsValid() || !target.IsValid()) {
        return 1.0f;
    }

    float modifier = 1.0f;
    const bool targetIsHero = target.IsHero();

    if (targetIsHero) {
        if (detail::HasRune(source, detail::RuneCoupDeGrace) &&
            target.HealthPercent() < 40.0f) {
            modifier *= 1.08f;
        }

        if (detail::HasRune(source, detail::RunePressTheAttack) &&
            detail::HasPressTheAttackDamageAmp(source, target)) {
            modifier *= 1.08f;
        }

        if (detail::HasRune(source, detail::RuneCutDown) &&
            target.HealthPercent() > 60.0f) {
            modifier *= 1.08f;
        }

        if (detail::HasRune(source, detail::RuneLastStand)) {
            modifier *= 1.0f + detail::LastStandBonus(source);
        }

        if (detail::HasRune(source, detail::RuneFirstStrike) &&
            detail::HasFirstStrikeActiveBuff(source)) {
            modifier *= 1.07f;
        }
    }

    (void)damageType;
    return modifier;
}

inline float GetIncomingDamageModifier(const AIBaseClient& target, DamageType) {
    return target.IsInvulnerable() ? 0.0f : 1.0f;
}

inline float ApplyOutgoing(const AIHeroClient& source,
                           const AIBaseClient& target,
                           DamageType damageType,
                           float damage) {
    return damage * GetOutgoingDamageModifier(source, target, damageType);
}

inline float ApplyIncoming(const AIBaseClient& target,
                           DamageType damageType,
                           float damage) {
    return damage * GetIncomingDamageModifier(target, damageType);
}

inline float Apply(const AIHeroClient& source,
                   const AIBaseClient& target,
                   DamageType damageType,
                   float damage) {
    return ApplyIncoming(target, damageType, ApplyOutgoing(source, target, damageType, damage));
}

} // namespace SDK::DamageMastery
