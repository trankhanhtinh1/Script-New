#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Aphelios::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::kPi;

inline constexpr float kCalibrumAttackRange = 650.0f;
inline constexpr float kCalibrumQRange = 1450.0f;
inline constexpr float kCalibrumQWidth = 60.0f;
inline constexpr float kCalibrumQCastSeconds = 0.35f;
inline constexpr float kCalibrumQSpeed = 1800.0f;
inline constexpr float kCalibrumMarkRange = 1800.0f;
inline constexpr float kSeverumQSeconds = 1.75f;
inline constexpr float kInfernumQRange = 850.0f;
inline constexpr float kInfernumQOuterRadius = 375.0f;
inline constexpr float kCrescendumSentryRange = 475.0f;
inline constexpr float kCrescendumSentryAttackRange = 500.0f;
inline constexpr float kCrescendumSentryIdleSeconds = 20.0f;
inline constexpr float kCrescendumSentryActiveSeconds = 4.0f;
inline constexpr float kMoonlightVigilRange = 1300.0f;
inline constexpr float kMoonlightVigilRadius = 210.0f;
inline constexpr float kMoonlightVigilWidth = 110.0f;
inline constexpr float kMoonlightVigilCastSeconds = 0.50f;
inline constexpr float kMoonlightVigilSpeed = 1000.0f;
inline constexpr int kWeaponAmmo = 50;
inline constexpr int kAbilityAmmoCost = 10;

enum class Weapon : std::uint8_t {
    Unknown,
    Calibrum,
    Severum,
    Gravitum,
    Infernum,
    Crescendum,
};

inline constexpr std::array<Weapon, 5> AllWeapons = {
    Weapon::Calibrum,
    Weapon::Severum,
    Weapon::Gravitum,
    Weapon::Infernum,
    Weapon::Crescendum,
};

inline constexpr bool IsWeapon(Weapon weapon) {
    return weapon >= Weapon::Calibrum && weapon <= Weapon::Crescendum;
}

inline constexpr int WeaponIndex(Weapon weapon) {
    return IsWeapon(weapon) ? static_cast<int>(weapon) - 1 : -1;
}

inline constexpr const char* WeaponName(Weapon weapon) {
    switch (weapon) {
    case Weapon::Calibrum: return "Calibrum";
    case Weapon::Severum: return "Severum";
    case Weapon::Gravitum: return "Gravitum";
    case Weapon::Infernum: return "Infernum";
    case Weapon::Crescendum: return "Crescendum";
    default: return "Unknown";
    }
}

inline constexpr std::uint32_t WeaponColor(Weapon weapon) {
    switch (weapon) {
    case Weapon::Calibrum: return 0xFF76E5D8u;
    case Weapon::Severum: return 0xFFE65A66u;
    case Weapon::Gravitum: return 0xFFB074E8u;
    case Weapon::Infernum: return 0xFFFF8A45u;
    case Weapon::Crescendum: return 0xFFF4E6A2u;
    default: return 0xFFB8B8B8u;
    }
}

inline bool ContainsInsensitive(const char* value, const char* token) {
    if (!value || !token || !token[0]) return false;
    for (const char* start = value; *start; ++start) {
        const char* left = start;
        const char* right = token;
        while (*left && *right) {
            char a = *left++;
            char b = *right++;
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
            if (a != b) break;
        }
        if (!*right) return true;
    }
    return false;
}

inline Weapon WeaponFromRuntimeName(const char* name) {
    if (ContainsInsensitive(name, "calibrum")) return Weapon::Calibrum;
    if (ContainsInsensitive(name, "severum")) return Weapon::Severum;
    if (ContainsInsensitive(name, "gravitum")) return Weapon::Gravitum;
    if (ContainsInsensitive(name, "infernum")) return Weapon::Infernum;
    if (ContainsInsensitive(name, "crescendum")) return Weapon::Crescendum;
    return Weapon::Unknown;
}

struct WeaponState {
    Weapon Main = Weapon::Calibrum;
    Weapon Offhand = Weapon::Severum;
    std::array<Weapon, 3> Queue = {
        Weapon::Gravitum, Weapon::Infernum, Weapon::Crescendum,
    };
    std::array<int, 5> Ammo = { 50, 50, 50, 50, 50 };
    std::array<bool, 5> AmmoKnown = { true, true, true, true, true };
    bool QueueKnown = true;
};

inline bool HasUniqueWeapons(const WeaponState& state) {
    std::array<int, 5> seen{};
    const std::array<Weapon, 5> order = {
        state.Main, state.Offhand,
        state.Queue[0], state.Queue[1], state.Queue[2],
    };
    for (Weapon weapon : order) {
        const int index = WeaponIndex(weapon);
        if (index < 0 || seen[index]++ != 0) return false;
    }
    return true;
}

inline int AmmoOf(const WeaponState& state, Weapon weapon) {
    const int index = WeaponIndex(weapon);
    return index >= 0 ? std::clamp(state.Ammo[index], 0, kWeaponAmmo) : 0;
}

inline void SetAmmo(WeaponState& state, Weapon weapon, int ammo, bool known = true) {
    const int index = WeaponIndex(weapon);
    if (index < 0) return;
    state.Ammo[index] = std::clamp(ammo, 0, kWeaponAmmo);
    state.AmmoKnown[index] = known;
}

inline bool PairContains(const WeaponState& state, Weapon weapon) {
    return state.Main == weapon || state.Offhand == weapon;
}

inline void SwapHands(WeaponState& state) {
    std::swap(state.Main, state.Offhand);
}

struct AmmoTransition {
    Weapon SpentWeapon = Weapon::Unknown;
    Weapon IncomingWeapon = Weapon::Unknown;
    int Before = 0;
    int After = 0;
    bool Depleted = false;
    bool QueueWasTrusted = false;
};

// Weapon disposal is deterministic: the off-hand moves to main, the first
// queued gun moves to off-hand, and the empty gun returns at the queue tail
// with 50 ammo.  Runtime reconciliation may later override an unknown queue.
inline AmmoTransition ConsumeMainAmmo(WeaponState& state, int amount) {
    AmmoTransition result{};
    if (!IsWeapon(state.Main) || amount <= 0) return result;
    result.SpentWeapon = state.Main;
    result.Before = AmmoOf(state, state.Main);
    result.QueueWasTrusted = state.QueueKnown && HasUniqueWeapons(state);
    const int remaining = std::max(0, result.Before - amount);
    SetAmmo(state, state.Main, remaining);
    result.After = remaining;
    if (remaining > 0 || !result.QueueWasTrusted) return result;

    const Weapon exhausted = state.Main;
    state.Main = state.Offhand;
    state.Offhand = state.Queue[0];
    state.Queue[0] = state.Queue[1];
    state.Queue[1] = state.Queue[2];
    state.Queue[2] = exhausted;
    SetAmmo(state, exhausted, kWeaponAmmo);
    result.IncomingWeapon = state.Offhand;
    result.Depleted = true;
    return result;
}

inline std::array<Weapon, 5> CycleOrder(const WeaponState& state) {
    return { state.Main, state.Offhand,
             state.Queue[0], state.Queue[1], state.Queue[2] };
}

inline bool SameCycleModuloRotation(const std::array<Weapon, 5>& left,
                                    const std::array<Weapon, 5>& right) {
    for (int offset = 0; offset < 5; ++offset) {
        bool equal = true;
        for (int i = 0; i < 5; ++i) {
            if (left[i] != right[(i + offset) % 5]) {
                equal = false;
                break;
            }
        }
        if (equal) return true;
    }
    return false;
}

inline constexpr std::array<Weapon, 5> StandardCycle = {
    Weapon::Calibrum, Weapon::Gravitum, Weapon::Infernum,
    Weapon::Severum, Weapon::Crescendum,
};

inline constexpr std::array<Weapon, 5> GreenBlueCycle = {
    Weapon::Calibrum, Weapon::Infernum, Weapon::Gravitum,
    Weapon::Severum, Weapon::Crescendum,
};

enum class RotationPlan : std::uint8_t {
    PreserveCurrent,
    BuildStandard,
    BuildGreenBlue,
    HoldSurvivalGun,
    HoldObjectiveGun,
    EmergencyFreestyle,
};

struct CombatContext {
    float PlayerHealthPercent = 100.0f;
    float TargetDistance = 600.0f;
    int NearbyEnemies = 1;
    int GroupedEnemies = 1;
    int Chakrams = 0;
    bool EnemyDiving = false;
    bool NeedCatch = false;
    bool NeedPeel = false;
    bool ObjectiveSoon = false;
    bool ObjectiveActive = false;
    bool OpenMap = false;
    bool CanCommitClose = false;
    bool TargetEscaping = false;
    bool ProjectileWall = false;
    bool PlayerChasingReturn = false;
    bool EarlyGame = false;
};

inline float WeaponTacticalScore(Weapon weapon,
                                 const CombatContext& context) {
    float score = 0.0f;
    switch (weapon) {
    case Weapon::Calibrum:
        score = context.TargetDistance > 700.0f ? 5.0f : 2.1f;
        if (context.NeedCatch) score += 1.1f;
        if (context.ProjectileWall) score -= 4.5f;
        break;
    case Weapon::Severum:
        score = context.PlayerHealthPercent < 45.0f ? 6.2f : 2.4f;
        if (context.EnemyDiving || context.NeedPeel) score += 2.8f;
        if (context.ProjectileWall) score += 1.3f;
        break;
    case Weapon::Gravitum:
        score = 2.0f;
        if (context.NeedCatch || context.NeedPeel) score += 4.4f;
        if (context.OpenMap || context.TargetEscaping) score += 1.7f;
        break;
    case Weapon::Infernum:
        score = 2.8f + static_cast<float>(std::max(0, context.GroupedEnemies - 1)) * 2.0f;
        if (context.ObjectiveActive || context.ObjectiveSoon) score += 1.7f;
        break;
    case Weapon::Crescendum:
        score = context.CanCommitClose || context.TargetDistance < 450.0f
            ? 5.3f : 1.4f;
        score += static_cast<float>(std::clamp(context.Chakrams, 0, 12)) * 0.34f;
        if (context.ObjectiveActive) score += 2.2f;
        if (context.TargetDistance > 600.0f && !context.PlayerChasingReturn) score -= 2.7f;
        break;
    default:
        return -1000.0f;
    }
    return score;
}

inline float PairSynergyScore(Weapon first,
                              Weapon second,
                              const CombatContext& context) {
    if (!IsWeapon(first) || !IsWeapon(second) || first == second) return -1000.0f;
    float score = WeaponTacticalScore(first, context) +
                  WeaponTacticalScore(second, context) * 0.72f;
    const auto pair = [=](Weapon a, Weapon b) {
        return (first == a && second == b) || (first == b && second == a);
    };
    if (pair(Weapon::Severum, Weapon::Crescendum)) score += 5.8f;
    if (pair(Weapon::Calibrum, Weapon::Crescendum)) score += 5.1f;
    if (pair(Weapon::Calibrum, Weapon::Infernum)) score += 4.0f;
    if (pair(Weapon::Calibrum, Weapon::Gravitum)) score += 3.4f;
    if (pair(Weapon::Gravitum, Weapon::Infernum)) score += 3.0f;
    if (pair(Weapon::Severum, Weapon::Infernum)) score += 2.8f;
    if (pair(Weapon::Severum, Weapon::Gravitum)) {
        score += (context.EnemyDiving || context.NeedPeel) ? 4.5f : 0.8f;
    }
    if (pair(Weapon::Gravitum, Weapon::Crescendum)) score -= 2.2f;
    if (pair(Weapon::Calibrum, Weapon::Severum)) score -= 1.7f;
    if (pair(Weapon::Infernum, Weapon::Crescendum)) {
        score += context.ObjectiveActive ? 1.8f : -0.4f;
    }
    return score;
}

inline RotationPlan ChooseRotationPlan(const WeaponState& state,
                                       const CombatContext& context) {
    if (context.PlayerHealthPercent < 31.0f && PairContains(state, Weapon::Severum)) {
        return RotationPlan::HoldSurvivalGun;
    }
    if ((context.NeedCatch || context.OpenMap) && PairContains(state, Weapon::Gravitum)) {
        return RotationPlan::HoldSurvivalGun;
    }
    if (context.ObjectiveSoon &&
        (PairContains(state, Weapon::Infernum) || PairContains(state, Weapon::Crescendum))) {
        return RotationPlan::HoldObjectiveGun;
    }
    if (!state.QueueKnown || !HasUniqueWeapons(state)) {
        return RotationPlan::EmergencyFreestyle;
    }
    const auto order = CycleOrder(state);
    if (SameCycleModuloRotation(order, StandardCycle) ||
        SameCycleModuloRotation(order, GreenBlueCycle)) {
        return RotationPlan::PreserveCurrent;
    }
    if (context.EarlyGame && context.GroupedEnemies <= 2) {
        return RotationPlan::BuildGreenBlue;
    }
    return RotationPlan::BuildStandard;
}

inline bool ShouldHoldWeapon(Weapon weapon,
                             const CombatContext& context,
                             int ammo) {
    if (ammo <= 0) return false;
    if (weapon == Weapon::Severum &&
        (context.PlayerHealthPercent < 48.0f || context.EnemyDiving)) return true;
    if (weapon == Weapon::Gravitum &&
        (context.NeedCatch || context.NeedPeel || context.OpenMap)) return true;
    if ((weapon == Weapon::Infernum || weapon == Weapon::Crescendum) &&
        context.ObjectiveSoon && ammo >= 12) return true;
    return false;
}

inline bool LowAmmoAbilitySwaps(int ammo) {
    return ammo > 0 && ammo <= kAbilityAmmoCost;
}

inline int LevelBreakpointIndex(int championLevel) {
    return std::clamp((std::clamp(championLevel, 1, 13) - 1) / 2, 0, 6);
}

inline float CalibrumQRawDamage(int championLevel,
                                float bonusAttackDamage,
                                float abilityPower) {
    const int index = LevelBreakpointIndex(championLevel);
    return 70.0f + static_cast<float>(index) * 15.0f +
           std::max(0.0f, bonusAttackDamage) *
               (0.42f + static_cast<float>(index) * 0.03f) +
           std::max(0.0f, abilityPower);
}

inline float CalibrumMarkRawDamage(float bonusAttackDamage,
                                   int marksConsumed = 1,
                                   float ultimateBonus = 0.0f) {
    return std::max(1, marksConsumed) *
               (15.0f + std::max(0.0f, bonusAttackDamage) * 0.15f) +
           std::max(0.0f, ultimateBonus);
}

inline float SeverumQPerHitRawDamage(int championLevel,
                                     float totalAttackDamage) {
    const int index = LevelBreakpointIndex(championLevel);
    return std::max(0.0f, totalAttackDamage) *
           (0.20f + static_cast<float>(index) * 0.035f);
}

inline int SeverumQAttackCount(float bonusAttackSpeedPercent) {
    return 6 + static_cast<int>(std::floor(
        std::max(0.0f, bonusAttackSpeedPercent) / 50.0f + 0.0001f));
}

inline float GravitumQRawDamage(int championLevel,
                                float bonusAttackDamage,
                                float abilityPower) {
    const int index = LevelBreakpointIndex(championLevel);
    return 50.0f + static_cast<float>(index) * 15.0f +
           std::max(0.0f, bonusAttackDamage) *
               (0.32f + static_cast<float>(index) * 0.03f) +
           std::max(0.0f, abilityPower) * 0.70f;
}

inline float InfernumQRawDamage(int championLevel,
                                float bonusAttackDamage,
                                float abilityPower) {
    const int index = LevelBreakpointIndex(championLevel);
    return 20.0f + static_cast<float>(index) * 15.0f +
           std::max(0.0f, bonusAttackDamage) *
               (0.15f + static_cast<float>(index) * 0.01f) +
           std::max(0.0f, abilityPower) * 0.70f;
}

inline float CrescendumSentryRawDamage(int championLevel,
                                       float bonusAttackDamage,
                                       float abilityPower) {
    const int index = LevelBreakpointIndex(championLevel);
    return 35.0f + static_cast<float>(index) * 15.0f +
           std::max(0.0f, bonusAttackDamage) *
               (0.34f + static_cast<float>(index) * 0.03f) +
           std::max(0.0f, abilityPower) * 0.50f;
}

inline float MoonlightVigilRawDamage(int rank,
                                     float bonusAttackDamage,
                                     float abilityPower) {
    static constexpr std::array<float, 4> base = {
        0.0f, 125.0f, 175.0f, 225.0f,
    };
    return base[std::clamp(rank, 0, 3)] +
           std::max(0.0f, bonusAttackDamage) * 0.20f +
           std::max(0.0f, abilityPower);
}

inline float UltimateWeaponBonus(Weapon weapon,
                                 int rank,
                                 float bonusAttackDamage) {
    rank = std::clamp(rank, 1, 3);
    switch (weapon) {
    case Weapon::Calibrum:
        return 20.0f + 30.0f * static_cast<float>(rank);
    case Weapon::Severum:
        return 150.0f + 100.0f * static_cast<float>(rank);
    case Weapon::Infernum:
        return 50.0f * static_cast<float>(rank) +
               std::max(0.0f, bonusAttackDamage) * 0.25f;
    case Weapon::Crescendum:
        return 5.0f;
    case Weapon::Gravitum:
        return 1.35f;
    default:
        return 0.0f;
    }
}

inline float CalibrumQImpactSeconds(float distance) {
    return kCalibrumQCastSeconds +
           std::clamp(distance, 0.0f, kCalibrumQRange) / kCalibrumQSpeed;
}

inline bool LineSkillshotHits(const Vec3& origin,
                              const Vec3& end,
                              const Vec3& target,
                              float halfWidth,
                              float targetRadius) {
    if (!origin.IsValid() || !end.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, end);
    return projection.Distance <= std::max(0.0f, halfWidth) +
                                  std::max(0.0f, targetRadius);
}

inline bool CalibrumQHits(const Vec3& origin,
                          const Vec3& end,
                          const Vec3& target,
                          float targetRadius) {
    if (origin.Distance2D(end) > kCalibrumQRange + 1.0f) return false;
    return LineSkillshotHits(origin, end, target,
                             kCalibrumQWidth * 0.5f, targetRadius);
}

inline bool InfernumQHits(const Vec3& origin,
                          const Vec3& aim,
                          const Vec3& target,
                          float targetRadius) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) return false;
    const Vec3 direction = Direction2D(origin, aim);
    Vec3 relative = target - origin;
    relative.y = 0.0f;
    const float distance = relative.Length2D();
    const float radius = std::clamp(targetRadius, 0.0f, 250.0f);
    if (direction.IsZero()) return false;
    if (distance <= radius) return true;
    if (distance > kInfernumQRange + radius) return false;
    const float halfAngle = std::atan2(kInfernumQOuterRadius,
                                       kInfernumQRange) +
        std::asin(std::clamp(radius / distance, 0.0f, 1.0f));
    return direction.Dot(relative / distance) >= std::cos(halfAngle);
}

struct AreaUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Priority = 1.0f;
    bool Primary = false;
    bool Dashing = false;
    bool HardCrowdControlled = false;
    bool SpellShielded = false;
    bool Valid = false;
};

inline float InfernumQScore(const Vec3& origin,
                            const Vec3& aim,
                            const std::vector<AreaUnit>& units) {
    float score = 0.0f;
    for (const auto& unit : units) {
        if (!unit.Valid || !InfernumQHits(
                origin, aim, unit.Position, unit.Radius)) continue;
        score += std::max(0.2f, unit.Priority) * 1.35f;
        if (unit.Primary) score += 1.4f;
        if (unit.Dashing) score += 0.35f;
        if (unit.SpellShielded) score -= 0.45f;
    }
    return score;
}

inline float MiniChakramBonusRatio(int chakrams) {
    const int count = std::clamp(chakrams, 0, 20);
    float ratio = 0.0f;
    for (int i = 0; i < count; ++i) {
        ratio += std::max(0.05f, 0.15f - static_cast<float>(i) * 0.015f);
    }
    return ratio;
}

inline float CrescendumRoundTripSeconds(float distance,
                                        float projectileSpeed = 600.0f) {
    return 2.0f * std::max(0.0f, distance) /
           std::max(1.0f, projectileSpeed);
}

inline float CrescendumDpsScore(float distance,
                                int chakrams,
                                bool chasingReturn) {
    const float effectiveDistance = chasingReturn
        ? std::max(120.0f, distance * 0.72f)
        : std::max(120.0f, distance);
    const float cycle = std::max(0.24f,
        CrescendumRoundTripSeconds(effectiveDistance));
    return (1.0f + MiniChakramBonusRatio(chakrams)) / cycle;
}

inline bool ShouldUseCrescendumAuto(float distance,
                                    int chakrams,
                                    bool chasingReturn,
                                    float alternativeScore) {
    return CrescendumDpsScore(distance, chakrams, chasingReturn) >=
           std::max(0.01f, alternativeScore);
}

struct SentryContext {
    Vec3 Player = {};
    Vec3 Position = {};
    Vec3 PredictedTarget = {};
    Vec3 RetreatDirection = {};
    float TargetRadius = 0.0f;
    int ExpectedTargets = 0;
    bool ObjectiveChoke = false;
    bool BushOrFogEdge = false;
    bool UnderEnemyTurret = false;
    bool GivesEnemyDashTarget = false;
    bool PlayerFleeing = false;
    bool CalibrumOffhand = false;
    bool GravitumOffhand = false;
};

inline float SentryPlacementScore(const SentryContext& context) {
    if (!context.Player.IsValid() || !context.Position.IsValid() ||
        context.Player.Distance2D(context.Position) > kCrescendumSentryRange + 1.0f ||
        context.UnderEnemyTurret || context.GivesEnemyDashTarget) {
        return -1000.0f;
    }
    float score = static_cast<float>(std::max(0, context.ExpectedTargets)) * 2.2f;
    if (context.PredictedTarget.IsValid() &&
        context.Position.Distance2D(context.PredictedTarget) <=
            kCrescendumSentryAttackRange + context.TargetRadius) {
        score += 3.2f;
    }
    if (context.ObjectiveChoke) score += 2.8f;
    if (context.BushOrFogEdge) score += 1.1f;
    if (context.CalibrumOffhand) score += 2.1f;
    if (context.GravitumOffhand) score += 1.6f;
    if (context.PlayerFleeing) {
        const Vec3 away = Direction2D(context.Player,
                                      context.Player + context.RetreatDirection);
        const Vec3 toSentry = Direction2D(context.Player, context.Position);
        if (!away.IsZero() && !toSentry.IsZero() &&
            away.Dot(toSentry) < -0.15f) score += 1.8f;
    }
    return score;
}

inline bool MoonlightVigilExplosionHits(const Vec3& center,
                                        const Vec3& target,
                                        float targetRadius) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <=
               kMoonlightVigilRadius + std::max(0.0f, targetRadius);
}

inline bool MoonlightVigilPathHits(const Vec3& origin,
                                   const Vec3& end,
                                   const Vec3& target,
                                   float targetRadius) {
    return origin.Distance2D(end) <= kMoonlightVigilRange + 1.0f &&
           LineSkillshotHits(origin, end, target,
                             kMoonlightVigilWidth * 0.5f, targetRadius);
}

inline float MoonlightVigilImpactSeconds(float distance) {
    return kMoonlightVigilCastSeconds +
           std::clamp(distance, 0.0f, kMoonlightVigilRange) /
               kMoonlightVigilSpeed;
}

struct UltimateContext {
    float PlayerHealthPercent = 100.0f;
    float TargetHealthPercent = 100.0f;
    float TargetDistance = 800.0f;
    int HitCount = 1;
    int PriorityHits = 1;
    int Chakrams = 0;
    int GravitumMarked = 0;
    bool EnemyDiving = false;
    bool NeedPeel = false;
    bool NeedCatch = false;
    bool CanFollowClose = false;
    bool TargetEscaping = false;
    bool ObjectiveFight = false;
    bool ProjectileWall = false;
    bool SpellShieldedPrimary = false;
};

inline float UltimateVariantScore(Weapon weapon,
                                  const UltimateContext& context) {
    if (!IsWeapon(weapon) || context.ProjectileWall || context.HitCount <= 0) {
        return -1000.0f;
    }
    float score = static_cast<float>(context.HitCount) * 2.1f +
                  static_cast<float>(context.PriorityHits) * 1.0f;
    switch (weapon) {
    case Weapon::Calibrum:
        score += context.TargetDistance > 750.0f ? 4.5f : 1.4f;
        score += context.TargetHealthPercent < 32.0f ? 3.2f : 0.0f;
        if (context.TargetEscaping) score += 1.7f;
        break;
    case Weapon::Severum:
        score += context.PlayerHealthPercent < 35.0f ? 8.0f : -0.4f;
        if (context.EnemyDiving || context.NeedPeel) score += 3.1f;
        break;
    case Weapon::Gravitum:
        score += (context.NeedCatch || context.NeedPeel) ? 5.2f : 1.2f;
        score += static_cast<float>(context.GravitumMarked) * 0.7f;
        if (context.TargetEscaping) score += 2.2f;
        break;
    case Weapon::Infernum:
        score += static_cast<float>(std::max(0, context.HitCount - 1)) * 3.4f;
        if (context.ObjectiveFight) score += 2.0f;
        break;
    case Weapon::Crescendum:
        score += context.CanFollowClose ? 4.8f : -1.8f;
        score += static_cast<float>(std::max(0, 6 - context.Chakrams)) * 0.55f;
        if (context.TargetDistance > 700.0f) score -= 2.1f;
        break;
    default:
        break;
    }
    if (context.SpellShieldedPrimary) score -= 2.7f;
    return score;
}

inline Weapon ChooseUltimateWeapon(Weapon main,
                                   Weapon offhand,
                                   const UltimateContext& context,
                                   float swapPenalty = 0.65f) {
    const float mainScore = UltimateVariantScore(main, context);
    const float offhandScore = UltimateVariantScore(offhand, context) -
                               std::max(0.0f, swapPenalty);
    return offhandScore > mainScore ? offhand : main;
}

enum class LowAmmoCombo : std::uint8_t {
    None,
    CalibrumIntoIncoming,
    SeverumGravitumRootIntoIncoming,
    GravitumRootIntoIncoming,
    InfernumIntoIncoming,
    SentryIntoIncoming,
};

inline LowAmmoCombo ChooseLowAmmoCombo(Weapon main,
                                       Weapon offhand,
                                       Weapon incoming,
                                       int ammo,
                                       const CombatContext& context) {
    if (!LowAmmoAbilitySwaps(ammo) || !IsWeapon(incoming)) {
        return LowAmmoCombo::None;
    }
    if (main == Weapon::Severum && offhand == Weapon::Gravitum &&
        (context.NeedCatch || context.EnemyDiving)) {
        return LowAmmoCombo::SeverumGravitumRootIntoIncoming;
    }
    if (main == Weapon::Gravitum &&
        (context.NeedCatch || context.NeedPeel)) {
        return LowAmmoCombo::GravitumRootIntoIncoming;
    }
    if (main == Weapon::Infernum &&
        (incoming == Weapon::Crescendum || context.GroupedEnemies >= 2)) {
        return LowAmmoCombo::InfernumIntoIncoming;
    }
    if (main == Weapon::Calibrum && context.TargetDistance > 650.0f) {
        return LowAmmoCombo::CalibrumIntoIncoming;
    }
    if (main == Weapon::Crescendum && context.ObjectiveActive) {
        return LowAmmoCombo::SentryIntoIncoming;
    }
    return LowAmmoCombo::None;
}

struct MarkAttackTiming {
    float OrdinaryAttackFinishSeconds = 0.0f;
    float MarkAvailableSeconds = 0.0f;
    float MarkExpiresSeconds = 4.5f;
};

inline bool CanFitPreMarkAuto(const MarkAttackTiming& timing) {
    return timing.OrdinaryAttackFinishSeconds > 0.0f &&
           timing.OrdinaryAttackFinishSeconds + 0.025f <
               timing.MarkAvailableSeconds &&
           timing.MarkAvailableSeconds < timing.MarkExpiresSeconds;
}

inline float GravitumRootValue(int markedTargets,
                               int priorityTargets,
                               bool interrupt,
                               bool peel) {
    if (markedTargets <= 0) return -1000.0f;
    return static_cast<float>(markedTargets) * 1.8f +
           static_cast<float>(priorityTargets) * 1.2f +
           (interrupt ? 4.0f : 0.0f) +
           (peel ? 3.1f : 0.0f);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Aphelios::Geometry
