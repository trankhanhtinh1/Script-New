#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Rumble::Geometry {

inline constexpr float kMaximumHeat = 150.0f;
inline constexpr float kDangerZoneHeat = 50.0f;
inline constexpr float kSpellHeat = 20.0f;
inline constexpr float kHeatDecayPerSecond = 10.0f;
inline constexpr float kOverheatDurationSeconds = 4.0f;
inline constexpr float kQRange = 600.0f;
inline constexpr float kQConeDistance = 625.0f;
inline constexpr float kQConeHalfAngleRadians =
    32.0f * SharedGeometry::kPi / 180.0f;
inline constexpr float kERange = 850.0f;
inline constexpr float kEWidth = 90.0f;
inline constexpr float kESpeed = 1200.0f;
inline constexpr float kRRange = 1700.0f;
inline constexpr float kRLineLength = 1050.0f;
inline constexpr float kRHalfWidth = 100.0f;
inline constexpr float kRTrailSeconds = 4.5f;

// 16.15 raised Rumble's overheat threshold to 150. Danger Zone remains 50.
enum class HeatBand : std::uint8_t {
    Cold,
    DangerZone,
    Overheated,
};

inline HeatBand ClassifyHeat(float heat, bool overheatBuff = false) {
    if (overheatBuff || heat >= kMaximumHeat) return HeatBand::Overheated;
    return heat >= kDangerZoneHeat ? HeatBand::DangerZone : HeatBand::Cold;
}

inline bool IsDangerZone(float heat, bool overheatBuff = false) {
    return ClassifyHeat(heat, overheatBuff) == HeatBand::DangerZone;
}

inline float ClampHeat(float heat) {
    if (!std::isfinite(heat)) return 0.0f;
    return std::clamp(heat, 0.0f, kMaximumHeat);
}

// Electro Harpoon's first shot spends 20 Heat; its paired second shot spends 0.
inline float HeatCostForSlot(int slot, bool secondHarpoon = false) {
    if (slot == 3) return 0.0f;
    if (slot == 2 && secondHarpoon) return 0.0f;
    return slot >= 0 && slot <= 2 ? kSpellHeat : 0.0f;
}

inline float HeatAfterCast(float heat, int slot, bool secondHarpoon = false) {
    return ClampHeat(ClampHeat(heat) + HeatCostForSlot(slot, secondHarpoon));
}

inline bool CastWouldOverheat(float heat, int slot, bool secondHarpoon = false) {
    return ClampHeat(heat) + HeatCostForSlot(slot, secondHarpoon) >=
        kMaximumHeat - 0.01f;
}

inline bool CastEntersDangerZone(float heat, int slot, bool secondHarpoon = false) {
    return ClampHeat(heat) < kDangerZoneHeat &&
        HeatAfterCast(heat, slot, secondHarpoon) >= kDangerZoneHeat;
}

struct OverheatContext {
    bool AllowIntentional = false;
    bool TargetInAutoRange = false;
    bool TargetKillable = false;
    bool JungleTarget = false;
    bool DefensiveEmergency = false;
    bool FollowupSpellRequired = false;
};

inline bool IntentionalOverheatAllowed(const OverheatContext& context) {
    if (!context.AllowIntentional || context.FollowupSpellRequired) return false;
    if (context.DefensiveEmergency) return true;
    return context.TargetInAutoRange &&
        (context.TargetKillable || context.JungleTarget);
}

inline bool HeatPolicyAllows(float heat,
                             int slot,
                             bool secondHarpoon,
                             const OverheatContext& context) {
    return !CastWouldOverheat(heat, slot, secondHarpoon) ||
        IntentionalOverheatAllowed(context);
}

inline float QRawDamage(int rank,
                        float abilityPower,
                        float targetMaximumHealth,
                        bool dangerZone,
                        bool minion = false) {
    static constexpr std::array<float, 6> base = { 0, 50, 75, 100, 125, 150 };
    static constexpr std::array<float, 6> healthPercent =
        { 0.0f, 0.060f, 0.065f, 0.070f, 0.075f, 0.080f };
    float damage = SharedGeometry::RankValue(base, rank) +
        1.05f * std::max(0.0f, abilityPower) +
        SharedGeometry::RankValue(healthPercent, rank) *
            std::max(0.0f, targetMaximumHealth);
    if (dangerZone) damage *= 1.5f;
    if (minion) damage *= 0.70f;
    return std::max(0.0f, damage);
}

inline float WShield(int rank,
                     float abilityPower,
                     float maximumHealth,
                     bool dangerZone) {
    static constexpr std::array<float, 6> base = { 0, 25, 55, 85, 115, 145 };
    float shield = SharedGeometry::RankValue(base, rank) +
        0.30f * std::max(0.0f, abilityPower) +
        0.04f * std::max(0.0f, maximumHealth);
    return std::max(0.0f, shield * (dangerZone ? 1.5f : 1.0f));
}

inline float WMoveSpeedPercent(int rank, bool dangerZone) {
    static constexpr std::array<float, 6> speed =
        { 0.0f, 0.10f, 0.15f, 0.20f, 0.25f, 0.30f };
    const float value = SharedGeometry::RankValue(speed, rank);
    return value * (dangerZone ? 1.5f : 1.0f);
}

inline float ERawDamage(int rank, float abilityPower, bool dangerZone) {
    static constexpr std::array<float, 6> base = { 0, 55, 80, 105, 130, 155 };
    const float damage = SharedGeometry::RankValue(base, rank) +
        0.50f * std::max(0.0f, abilityPower);
    return std::max(0.0f, damage * (dangerZone ? 1.5f : 1.0f));
}

inline float RDamagePerSecond(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base = { 0, 120, 200, 280 };
    return std::max(0.0f, SharedGeometry::RankValue(base, rank) +
        0.35f * std::max(0.0f, abilityPower));
}

inline int ReconciledHarpoonAmmo(int observedAmmo,
                                 int observedMaximum,
                                 int fallbackAmmo) {
    if (observedMaximum == 2 && observedAmmo >= 0 &&
        observedAmmo <= observedMaximum) {
        return observedAmmo;
    }
    return std::clamp(fallbackAmmo, 0, 2);
}

inline bool PreserveHarpoonCharge(int ammo,
                                  bool killSecure,
                                  bool fleeing,
                                  bool secondHarpoonWindow) {
    if (ammo <= 0) return true;
    if (killSecure || fleeing || secondHarpoonWindow) return false;
    return ammo <= 1;
}

inline bool PointInQConicalFlame(const Vec3& origin,
                                 const Vec3& facingPoint,
                                 const Vec3& point,
                                 float targetRadius = 0.0f) {
    const Vec3 facing = SharedGeometry::Direction2D(origin, facingPoint);
    Vec3 offset = point - origin;
    offset.y = 0.0f;
    const float distance = offset.Length2D();
    const float radius = std::max(0.0f, targetRadius);
    if (facing.IsZero() || distance > kQConeDistance + radius) return false;
    if (distance <= radius + 0.001f) return true;
    const Vec3 direction = offset / distance;
    const float angularPadding = std::asin(std::clamp(
        radius / std::max(distance, radius + 0.001f), 0.0f, 1.0f));
    const float minimumDot = std::cos(kQConeHalfAngleRadians + angularPadding);
    return facing.Dot(direction) >= minimumDot;
}

struct RLine {
    Vec3 Start = {};
    Vec3 End = {};
    bool Valid = false;
};

inline RLine CenteredRLine(const Vec3& caster,
                           const Vec3& center,
                           const Vec3& desiredDirection) {
    Vec3 direction = desiredDirection;
    direction.y = 0.0f;
    const float length = direction.Length2D();
    if (length <= 0.001f || !std::isfinite(length) ||
        !caster.IsValid() || !center.IsValid()) return {};
    direction = direction / length;
    Vec3 start = center - direction * (kRLineLength * 0.5f);
    Vec3 end = center + direction * (kRLineLength * 0.5f);
    if (caster.Distance2D(start) > kRRange) {
        const Vec3 towardStart = SharedGeometry::Direction2D(caster, start);
        if (towardStart.IsZero()) return {};
        start = caster + towardStart * kRRange;
        end = start + direction * kRLineLength;
    }
    return { start, end, start.Distance2D(end) <= kRLineLength + 0.5f };
}

inline bool RLineContacts(const RLine& line,
                          const Vec3& point,
                          float targetRadius = 0.0f) {
    if (!line.Valid || !point.IsValid()) return false;
    return SharedGeometry::ProjectPointToSegment2D(
        point, line.Start, line.End).Distance <=
        kRHalfWidth + std::max(0.0f, targetRadius);
}

inline int RLineHitCount(const RLine& line,
                         const std::vector<Vec3>& points,
                         float targetRadius = 0.0f) {
    int hits = 0;
    for (const auto& point : points) {
        if (RLineContacts(line, point, targetRadius)) ++hits;
    }
    return hits;
}

inline float RLineScore(int hits,
                        bool includesPrimary,
                        float primaryProjectionDistance,
                        bool followsEscapePath) {
    if (!includesPrimary) return -10000.0f;
    return static_cast<float>(std::max(0, hits)) * 420.0f -
        std::max(0.0f, primaryProjectionDistance) * 1.5f +
        (followsEscapePath ? 120.0f : 0.0f);
}

struct RSafetyContext {
    bool PlayerUnderEnemyTurret = false;
    bool PrimaryUnderEnemyTurret = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Automatic = false;
    bool AlliedFollowup = false;
    int HitCount = 0;
};

inline bool RCastSafe(const RSafetyContext& context) {
    if (context.PlayerUnderEnemyTurret &&
        !context.Defensive && !context.Lethal) return false;
    if (context.PrimaryUnderEnemyTurret && !context.Lethal &&
        !context.Defensive && !context.AlliedFollowup && context.HitCount < 2) {
        return false;
    }
    if (context.Automatic && !context.Defensive && !context.Lethal &&
        context.HitCount < 2) return false;
    return context.Lethal || context.Defensive || context.HitCount >= 2 ||
        context.AlliedFollowup;
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.Engage &&
        (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Rumble::Geometry
