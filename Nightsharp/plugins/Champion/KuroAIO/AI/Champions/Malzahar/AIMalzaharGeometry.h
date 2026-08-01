#pragma once

// Pure Malzahar mechanics: portal hitboxes, voidling charge/target policy,
// Malefic Visions spread and Nether Grasp channel safety. Runtime state and
// SDK events remain in AIMalzaharController.h.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Malzahar::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kPassiveCooldownSeconds = 30.0f;
inline constexpr float kQRange = 900.0f;
inline constexpr float kQPortalHalfWidth = 85.0f;
inline constexpr float kQDelay = 0.75f;
inline constexpr float kWRange = 450.0f;
inline constexpr float kWRadius = 100.0f;
inline constexpr float kWLifetimeSeconds = 8.0f;
inline constexpr float kWRechargeSeconds = 20.0f;
inline constexpr int kWMaximumAmmo = 2;
inline constexpr int kWMaximumVoidlings = 3;
inline constexpr float kERange = 650.0f;
inline constexpr float kESpreadRadius = 550.0f;
inline constexpr float kEDurationSeconds = 4.0f;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kRRange = 700.0f;
inline constexpr float kRChannelSeconds = 2.5f;
inline constexpr float kRRadius = 95.0f;

inline float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline bool PassiveReady(int nowTick, int cooldownEndTick) {
    return nowTick >= 0 && cooldownEndTick <= nowTick;
}

inline int PassiveCooldownEnd(int triggerTick, float cooldownSeconds = kPassiveCooldownSeconds) {
    if (triggerTick < 0 || !std::isfinite(cooldownSeconds)) return 0;
    return triggerTick + static_cast<int>(std::max(0.0f, cooldownSeconds) * 1000.0f);
}

inline bool QLineHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                      float targetRadius = 0.0f) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) return false;
    if (origin.Distance2D(aim) > kQRange + std::max(0.0f, targetRadius)) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, aim);
    return projection.Distance <= kQPortalHalfWidth + std::max(0.0f, targetRadius);
}

inline float QSilenceSeconds(int rank) {
    return RankValue(rank, {1.5f, 1.75f, 2.0f, 2.25f, 2.5f});
}

inline float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {70.0f, 100.0f, 130.0f, 160.0f, 190.0f}) +
        0.55f * std::max(0.0f, abilityPower);
}

inline int ValidatedVoidlingAmmo(int ammo, int maximum) {
    return maximum == kWMaximumAmmo && ammo >= 0 && ammo <= maximum ? ammo : -1;
}

inline int SpawnableVoidlings(int ammo, int activeVoidlings) {
    return ammo > 0 && activeVoidlings >= 0
        ? std::min({ammo, kWMaximumVoidlings - activeVoidlings, kWMaximumVoidlings})
        : 0;
}

enum class VoidlingTarget : unsigned char {
    MaleficVisions,
    NetherGrasp,
    Champion,
    Minion,
    Monster,
    None,
};

inline VoidlingTarget ChooseVoidlingTarget(bool eMarked, bool rChanneling,
                                           bool championInRange,
                                           bool minionInRange,
                                           bool monsterInRange) {
    if (eMarked && championInRange) return VoidlingTarget::MaleficVisions;
    if (rChanneling && championInRange) return VoidlingTarget::NetherGrasp;
    if (championInRange) return VoidlingTarget::Champion;
    if (minionInRange) return VoidlingTarget::Minion;
    if (monsterInRange) return VoidlingTarget::Monster;
    return VoidlingTarget::None;
}

inline bool ECanSpread(bool marked, bool targetDied, bool nearbyTarget,
                       bool spellReady, bool protectedTarget) {
    return marked && targetDied && nearbyTarget && spellReady && !protectedTarget;
}

inline bool ESpreadHits(const Vec3& deadTarget, const Vec3& candidate,
                        float candidateRadius = 0.0f) {
    return deadTarget.IsValid() && candidate.IsValid() &&
        deadTarget.Distance2D(candidate) <=
            kESpreadRadius + std::max(0.0f, candidateRadius);
}

inline float ERawDamagePerSecond(int rank, float abilityPower) {
    return RankValue(rank, {15.0f, 20.0f, 25.0f, 30.0f, 35.0f}) +
        0.2f * std::max(0.0f, abilityPower);
}

struct NetherGraspGate {
    bool Ready = false;
    bool TargetValid = false;
    bool TargetProtected = false;
    bool InRange = false;
    bool Channeling = false;
    bool Interrupted = false;
    bool IncomingHardCrowdControl = false;
    bool UnderTurret = false;
    bool TooManyEnemies = false;
    bool Lethal = false;
    bool SuppressionTarget = false;
};

inline bool CanStartNetherGrasp(const NetherGraspGate& gate) {
    return gate.Ready && gate.TargetValid && gate.InRange &&
        !gate.TargetProtected && !gate.Channeling && !gate.Interrupted &&
        !gate.IncomingHardCrowdControl && !gate.UnderTurret &&
        !gate.TooManyEnemies && gate.SuppressionTarget;
}

inline bool ShouldContinueNetherGrasp(const NetherGraspGate& gate,
                                      float elapsedSeconds) {
    if (!gate.Channeling || gate.Interrupted || gate.IncomingHardCrowdControl ||
        !gate.TargetValid || gate.TargetProtected || !gate.InRange) return false;
    return elapsedSeconds < kRChannelSeconds;
}

inline bool ShouldReleaseNetherGrasp(const NetherGraspGate& gate,
                                     float elapsedSeconds) {
    if (!gate.Channeling || gate.Interrupted || gate.IncomingHardCrowdControl ||
        !gate.TargetValid || gate.TargetProtected) return false;
    return elapsedSeconds >= kRChannelSeconds;
}

inline float RRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {125.0f, 200.0f, 275.0f, 350.0f, 425.0f}) +
        0.8f * std::max(0.0f, abilityPower);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Malzahar::Geometry
