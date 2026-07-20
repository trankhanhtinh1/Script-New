#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Annie::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::kPi;

inline constexpr float kDisintegrateRange = 625.0f;
inline constexpr float kDisintegrateCastSeconds = 0.25f;
inline constexpr float kDisintegrateSpeed = 1400.0f;
inline constexpr float kIncinerateRange = 600.0f;
inline constexpr float kIncinerateCastSeconds = 0.25f;
inline constexpr float kIncinerateHalfAngleDegrees = 24.76f;
inline constexpr float kMoltenShieldRange = 800.0f;
inline constexpr float kTibbersCastRange = 600.0f;
inline constexpr float kTibbersRadius = 250.0f;
inline constexpr float kTibbersAuraRadius = 350.0f;
inline constexpr float kTibbersLifetimeSeconds = 45.0f;
inline constexpr float kTibbersAuraTickSeconds = 0.25f;
inline constexpr float kTibbersEnrageSeconds = 3.0f;
inline constexpr float kTibbersDeathEnrageSeconds = 10.0f;

enum class PassiveSpell : std::uint8_t {
    None,
    Q,
    W,
    E,
    R,
};

enum class PassiveEventKind : std::uint8_t {
    GainOnCast,
    DamageLanding,
    QDamageLanding,
};

struct PassiveEvent {
    int Tick = 0;
    int Order = 0;
    PassiveSpell Spell = PassiveSpell::None;
    PassiveEventKind Kind = PassiveEventKind::DamageLanding;
    // Spell shields consume a primed Pyromania proc even though they block
    // the stun.  Other immunity states can instead leave the event invalid.
    bool CanConsume = true;
    bool CanApplyStun = true;
};

struct PassiveResolution {
    int FinalStacks = 0;
    PassiveSpell Consumer = PassiveSpell::None;
    int ConsumeTick = 0;
    bool StunApplied = false;
    bool StunBlocked = false;
};

inline bool IsDamagingPassiveSpell(PassiveSpell spell) {
    return spell == PassiveSpell::Q || spell == PassiveSpell::W ||
           spell == PassiveSpell::R;
}

// Simulates the actual distinction that makes advanced Annie sequencing
// possible: W/E/R add their stack on cast, Q adds its stack only on missile
// hit, and the first primed damaging spell to land owns the stun.  A Q which
// consumes Pyromania does not immediately rebuild a stack from that same hit.
inline PassiveResolution SimulatePyromania(
    int startingStacks,
    std::vector<PassiveEvent> events) {
    std::stable_sort(events.begin(), events.end(),
        [](const PassiveEvent& left, const PassiveEvent& right) {
            if (left.Tick != right.Tick) return left.Tick < right.Tick;
            return left.Order < right.Order;
        });

    PassiveResolution result{};
    result.FinalStacks = std::clamp(startingStacks, 0, 4);
    for (const auto& event : events) {
        if (event.Kind == PassiveEventKind::GainOnCast) {
            result.FinalStacks = std::min(4, result.FinalStacks + 1);
            continue;
        }
        if (!IsDamagingPassiveSpell(event.Spell) || !event.CanConsume) {
            if (event.Kind == PassiveEventKind::QDamageLanding) {
                result.FinalStacks = std::min(4, result.FinalStacks + 1);
            }
            continue;
        }

        if (result.Consumer == PassiveSpell::None &&
            result.FinalStacks >= 4) {
            result.Consumer = event.Spell;
            result.ConsumeTick = event.Tick;
            result.StunApplied = event.CanApplyStun;
            result.StunBlocked = !event.CanApplyStun;
            result.FinalStacks = 0;
            continue;
        }
        if (event.Kind == PassiveEventKind::QDamageLanding) {
            result.FinalStacks = std::min(4, result.FinalStacks + 1);
        }
    }
    return result;
}

inline float DisintegrateImpactSeconds(float distance) {
    return kDisintegrateCastSeconds +
           std::clamp(distance, 0.0f, kDisintegrateRange) /
               kDisintegrateSpeed;
}

inline float DisintegrateRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 80.0f, 125.0f, 170.0f, 215.0f, 260.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, abilityPower) * 0.80f;
}

inline float DisintegrateManaCost(int rank) {
    static constexpr std::array<float, 6> cost = {
        0.0f, 60.0f, 65.0f, 70.0f, 75.0f, 80.0f,
    };
    return cost[std::clamp(rank, 0, 5)];
}

struct DisintegrateResult {
    float ManaAfter = 0.0f;
    float CooldownSeconds = 0.0f;
};

inline DisintegrateResult ResolveDisintegrate(
    float manaAfterCast,
    int rank,
    float ordinaryCooldownSeconds,
    bool killedTarget) {
    return {
        std::max(0.0f, manaAfterCast) +
            (killedTarget ? DisintegrateManaCost(rank) : 0.0f),
        std::max(0.0f, ordinaryCooldownSeconds) *
            (killedTarget ? 0.5f : 1.0f),
    };
}

inline float IncinerateRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 70.0f, 110.0f, 150.0f, 190.0f, 230.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, abilityPower) * 0.80f;
}

inline Vec3 IncinerateResolveOrigin(const Vec3& castOrigin,
                                    const Vec3& relocatedOrigin,
                                    int relocationTick,
                                    int resolveTick) {
    return relocatedOrigin.IsValid() && !relocatedOrigin.IsZero() &&
           relocationTick > 0 && relocationTick <= resolveTick
        ? relocatedOrigin
        : castOrigin;
}

// Circle-versus-sector test.  Target radius expands both the radial edge and
// the angular edge; a center just behind Annie is hit only when its gameplay
// circle overlaps the cone's apex.
inline bool IncinerateHits(const Vec3& origin,
                           const Vec3& aim,
                           const Vec3& target,
                           float targetRadius = 0.0f) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) {
        return false;
    }
    const Vec3 direction = Direction2D(origin, aim);
    Vec3 offset = target - origin;
    offset.y = 0.0f;
    const float distance = offset.Length2D();
    const float radius = std::clamp(targetRadius, 0.0f, 250.0f);
    if (direction.IsZero()) return false;
    if (distance <= radius) return true;
    if (distance > kIncinerateRange + radius || distance <= 0.001f) {
        return false;
    }
    const Vec3 targetDirection = offset / distance;
    const float angularExpansion = std::asin(
        std::clamp(radius / distance, 0.0f, 1.0f));
    const float halfAngle = kIncinerateHalfAngleDegrees * kPi / 180.0f +
                            angularExpansion;
    return direction.Dot(targetDirection) >= std::cos(halfAngle);
}

struct ConeUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Priority = 1.0f;
    bool Primary = false;
    bool HardCrowdControlled = false;
    bool Dashing = false;
    bool Valid = false;
};

inline float IncinerateScore(const Vec3& origin,
                             const Vec3& aim,
                             const std::vector<ConeUnit>& units) {
    float score = 0.0f;
    for (const auto& unit : units) {
        if (!unit.Valid || !IncinerateHits(
                origin, aim, unit.Position, unit.Radius)) {
            continue;
        }
        score += std::max(0.1f, unit.Priority) * 1.4f;
        if (unit.Primary) score += 1.15f;
        if (unit.HardCrowdControlled) score += 0.25f;
        if (unit.Dashing) score += 0.60f;
    }
    return score;
}

inline float MoltenShieldAmount(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 60.0f, 95.0f, 130.0f, 165.0f, 200.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, abilityPower) * 0.40f;
}

inline float MoltenShieldReactionRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 25.0f, 35.0f, 45.0f, 55.0f, 65.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, abilityPower) * 0.40f;
}

inline float MoltenShieldMoveSpeedPercent(int championLevel) {
    const float level = static_cast<float>(std::clamp(championLevel, 1, 18));
    return 20.0f + (level - 1.0f) * (30.0f / 17.0f);
}

inline float MoltenShieldMoveSpeedAt(float elapsedSeconds,
                                     int championLevel) {
    const float full = MoltenShieldMoveSpeedPercent(championLevel);
    return full * (1.0f - std::clamp(
        elapsedSeconds / 1.5f, 0.0f, 1.0f));
}

inline float SummonTibbersRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base = {
        0.0f, 150.0f, 275.0f, 400.0f,
    };
    return base[std::clamp(rank, 0, 3)] +
           std::max(0.0f, abilityPower) * 0.75f;
}

inline float TibbersAuraRawDamagePerSecond(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base = {
        0.0f, 8.0f, 12.0f, 16.0f,
    };
    return base[std::clamp(rank, 0, 3)] +
           std::max(0.0f, abilityPower) * 0.04f;
}

inline float TibbersAuraRawDamagePerTick(int rank, float abilityPower) {
    return TibbersAuraRawDamagePerSecond(rank, abilityPower) *
           kTibbersAuraTickSeconds;
}

inline bool TibbersAuraHits(const Vec3& tibbersPosition,
                            const Vec3& targetPosition,
                            float targetRadius = 0.0f) {
    return tibbersPosition.IsValid() && targetPosition.IsValid() &&
           tibbersPosition.Distance2D(targetPosition) <=
               kTibbersAuraRadius +
                   std::clamp(targetRadius, 0.0f, 250.0f);
}

inline int TibbersAuraTickCount(float contactSeconds,
                                bool includeInitialTick = false) {
    if (contactSeconds < 0.0f) return 0;
    const int elapsed = static_cast<int>(std::floor(
        contactSeconds / kTibbersAuraTickSeconds + 0.0001f));
    return elapsed + (includeInitialTick ? 1 : 0);
}

inline float TibbersAttackRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base = {
        0.0f, 30.0f, 45.0f, 60.0f,
    };
    return base[std::clamp(rank, 0, 3)] +
           std::max(0.0f, abilityPower) * 0.10f;
}

// The live pet exposes five discrete attack-speed stages while enraged.  The
// values below are the observed PC values corresponding to zero through four
// attacks consumed; after the fifth attack Tibbers returns to 0.625.
inline float TibbersEnrageAttackSpeed(int attacksAlreadyMade) {
    static constexpr std::array<float, 6> speed = {
        1.736f, 1.536f, 1.307f, 1.043f, 0.739f, 0.625f,
    };
    return speed[std::clamp(attacksAlreadyMade, 0, 5)];
}

inline float TibbersEnrageAttackSpeedMultiplier(int attacksAlreadyMade) {
    return TibbersEnrageAttackSpeed(attacksAlreadyMade) / 0.625f;
}

inline float TibbersEnrageAttackInterval(float ordinaryIntervalSeconds,
                                         int attacksAlreadyMade) {
    return std::max(0.0f, ordinaryIntervalSeconds) /
           std::max(1.0f,
                    TibbersEnrageAttackSpeedMultiplier(attacksAlreadyMade));
}

struct CircleUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Priority = 1.0f;
    bool Primary = false;
    bool HardCrowdControlled = false;
    bool Dashing = false;
    bool SpellShielded = false;
    bool Valid = false;
};

inline bool TibbersSummonHits(const Vec3& center,
                              const Vec3& target,
                              float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= kTibbersRadius +
               std::clamp(targetRadius, 0.0f, 250.0f);
}

inline float TibbersSummonScore(const Vec3& center,
                                const std::vector<CircleUnit>& units,
                                bool stunPrimed) {
    float score = 0.0f;
    for (const auto& unit : units) {
        if (!unit.Valid || !TibbersSummonHits(
                center, unit.Position, unit.Radius)) {
            continue;
        }
        float value = std::max(0.1f, unit.Priority) * 1.55f;
        if (unit.Primary) value += 1.20f;
        if (unit.HardCrowdControlled) value += stunPrimed ? -0.15f : 0.35f;
        if (unit.Dashing) value += 0.55f;
        if (unit.SpellShielded) value -= stunPrimed ? 1.35f : 0.30f;
        score += std::max(0.0f, value);
    }
    return score;
}

enum class PetCommand : std::uint8_t {
    Hold,
    Attack,
    MoveToOwner,
    MoveToZone,
};

struct PetCommandInput {
    bool PetAlive = false;
    bool ManualLock = false;
    bool TargetValid = false;
    bool TargetUnderEnemyTurret = false;
    bool DiveAllowed = false;
    bool ZoneRequested = false;
    bool Enraged = false;
    float PetHealthPercent = 100.0f;
    float DistanceToOwner = 0.0f;
    float DistanceToTarget = 0.0f;
};

inline PetCommand ChoosePetCommand(const PetCommandInput& input,
                                   float returnDistance = 1100.0f,
                                   float criticalDistance = 1450.0f) {
    if (!input.PetAlive || input.ManualLock) return PetCommand::Hold;
    if (input.DistanceToOwner >= std::max(returnDistance, criticalDistance) ||
        input.PetHealthPercent <= 12.0f) {
        return PetCommand::MoveToOwner;
    }
    if (input.TargetValid &&
        (!input.TargetUnderEnemyTurret || input.DiveAllowed) &&
        (input.Enraged || input.DistanceToTarget <= 900.0f)) {
        return PetCommand::Attack;
    }
    if (input.DistanceToOwner >= returnDistance) {
        return PetCommand::MoveToOwner;
    }
    return input.ZoneRequested ? PetCommand::MoveToZone : PetCommand::Hold;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Annie::Geometry
