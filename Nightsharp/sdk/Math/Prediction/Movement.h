#pragma once

#include "GamePath.h"
#include "../Geometry.h"
#include "../../Events/Dash.h"
#include "../../../core/CoreBuffs.h"
#include "../../../core/CoreNavGrid.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace SDK::Prediction::Movement {

using AoEPredictionCallback = PredictionOutput(*)(const PredictionInput&);
inline AoEPredictionCallback g_aoePredictionCallback = nullptr;

namespace detail {

constexpr int kBuffTypeStun = 5;
constexpr int kBuffTypeSnare = 12;
constexpr int kBuffTypeSuppression = 20;
constexpr int kBuffTypeCharm = 24;
constexpr int kBuffTypeKnockup = 26;

inline std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline bool SegmentIntersectsWithPadding(const Vec2& a1,
                                         const Vec2& a2,
                                         const Vec2& b1,
                                         const Vec2& b2,
                                         float padding) {
    if (Vec2_SegmentIntersection(a1, a2, b1, b2).Intersects) {
        return true;
    }

    const float safePadding = std::max(0.0f, padding);
    return SDK::Geometry::PointToSegmentDistance(a1, b1, b2) <= safePadding ||
           SDK::Geometry::PointToSegmentDistance(a2, b1, b2) <= safePadding ||
           SDK::Geometry::PointToSegmentDistance(b1, a1, a2) <= safePadding ||
           SDK::Geometry::PointToSegmentDistance(b2, a1, a2) <= safePadding;
}

inline bool IsYasuoWallName(const std::string& lowerName) {
    return lowerName.find("yasuowmovingwall") != std::string::npos ||
           (lowerName.find("windwall") != std::string::npos &&
            lowerName.find("yasuo") != std::string::npos) ||
           (lowerName.find("_w_windwall_enemy_") != std::string::npos &&
            lowerName.find("yasuo") != std::string::npos);
}

inline bool IsBraumShieldBuffActive(const AIHeroClient& hero) {
    if (!hero.IsValid()) {
        return false;
    }

    const std::string champion = ToLowerCopy(hero.CharacterName());
    if (champion != "braum") {
        return false;
    }

    return hero.HasBuff("BraumShieldRaise") ||
           hero.HasBuff("braumeshieldbuff") ||
           hero.HasBuff("BraumE");
}

inline bool HasYasuoWallCollision(const Vec2& lineStart, const Vec2& lineEnd, float radius) {
    for (const auto& object : GameObjects::AllGameObjects()) {
        if (!object.IsValid()) {
            continue;
        }

        const std::string lowerName = ToLowerCopy(object.Name());
        if (!IsYasuoWallName(lowerName)) {
            continue;
        }

        const Vec2 center = object.Position().To2D();
        if (!center.IsValid()) {
            continue;
        }

        Vec2 wallAxis = object.Direction().To2D();
        if (wallAxis.LengthSqr() <= 0.0001f) {
            wallAxis = (lineEnd - lineStart).Perpendicular();
        } else {
            wallAxis = wallAxis.Perpendicular();
        }

        if (wallAxis.LengthSqr() <= 0.0001f) {
            continue;
        }

        wallAxis = wallAxis.Normalized();

        int wallLevel = 1;
        const std::string& name = object.Name();
        if (name.size() >= 2) {
            const char c1 = name[name.size() - 2];
            const char c2 = name[name.size() - 1];
            if (c2 >= '1' && c2 <= '5') {
                wallLevel = c2 - '0';
            } else if (c1 >= '1' && c1 <= '5') {
                wallLevel = c1 - '0';
            }
        }
        const float wallWidth = static_cast<float>(250 + 50 * wallLevel);
        const float halfWidth = wallWidth * 0.5f;

        const Vec2 wallStart = center + (wallAxis * halfWidth);
        const Vec2 wallEnd = center - (wallAxis * halfWidth);
        if (SegmentIntersectsWithPadding(lineStart, lineEnd, wallStart, wallEnd, radius + 35.0f)) {
            return true;
        }
    }

    return false;
}

inline bool HasBraumShieldCollision(const Vec2& lineStart, const Vec2& lineEnd, float radius) {
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        if (!IsBraumShieldBuffActive(hero)) {
            continue;
        }

        Vec2 facing = hero.Direction().To2D();
        if (facing.LengthSqr() <= 0.0001f) {
            const Vec2 towardOrder = hero.OrderPosition().To2D() - hero.Position().To2D();
            if (towardOrder.LengthSqr() > 0.0001f) {
                facing = towardOrder;
            }
        }

        if (facing.LengthSqr() <= 0.0001f) {
            continue;
        }

        facing = facing.Normalized();
        const Vec2 center = hero.Position().To2D() + (facing * 85.0f);
        const Vec2 shieldAxis = facing.Perpendicular().Normalized();
        const float halfWidth = 175.0f;
        const Vec2 shieldStart = center + (shieldAxis * halfWidth);
        const Vec2 shieldEnd = center - (shieldAxis * halfWidth);
        if (SegmentIntersectsWithPadding(lineStart, lineEnd, shieldStart, shieldEnd, radius + 45.0f)) {
            return true;
        }
    }

    return false;
}

inline bool IsInstantSpeed(float speed) {
    return speed <= 0.0f || speed >= FLT_MAX * 0.5f;
}

inline bool IsMaxRange(float range) {
    return range >= FLT_MAX * 0.5f;
}

inline Vec3 UnitPosition(const AIBaseClient& target) {
    const auto server = target.ServerPosition();
    return server.IsZero() ? target.Position() : server;
}

inline Vec2 UnitPosition2D(const AIBaseClient& target) {
    return UnitPosition(target).To2D();
}

inline HitChance GetPathHitchance(const AIBaseClient& target) {
    const auto current = SDK::GamePath::PathTracker::GetCurrentPath(target);
    return current.Time() < 0.1 ? HitChance::VeryHigh : HitChance::High;
}

inline std::vector<Vec3> BuildCurrentPath(const AIBaseClient& target) {
    auto path = target.GetWaypoints();
    if (path.empty()) {
        const Vec3 start = target.ServerPosition().IsZero() ? UnitPosition(target) : target.ServerPosition();
        path.push_back(start);
        const Vec3 end = target.PathEnd();
        if (!end.IsZero() && end.Distance2D(start) > 1.0f) {
            path.push_back(end);
        }
    }
    return path;
}

inline double UnitIsImmobileUntil(const AIBaseClient& target) {
    if (!target.IsValid()) {
        return -1.0;
    }

    uintptr_t buffs[128] = {};
    const int count = CoreBuffs::Enumerate(target.Address(), buffs, static_cast<int>(std::size(buffs)));
    const float now = Game::Time();
    float maxEnd = -1.0f;

    for (int i = 0; i < count; ++i) {
        const CoreBuffs::BuffRef buff{ buffs[i] };
        if (!buff.IsActive(now)) {
            continue;
        }

        const int type = buff.GetType();
        if (type != kBuffTypeCharm &&
            type != kBuffTypeKnockup &&
            type != kBuffTypeStun &&
            type != kBuffTypeSuppression &&
            type != kBuffTypeSnare) {
            continue;
        }

        maxEnd = std::max(maxEnd, buff.GetEndTime());
    }

    return static_cast<double>(maxEnd - now);
}

inline Vec2 PositionAfter(const AIBaseClient& target, float timeSeconds, float moveSpeedOverride = FLT_MAX) {
    const float speed = (moveSpeedOverride >= FLT_MAX * 0.5f)
        ? std::max(target.MoveSpeed(), 0.0f)
        : std::max(moveSpeedOverride, 0.0f);
    const float travelDistance = std::max(timeSeconds, 0.0f) * speed;

    std::vector<Vec2> path = {};
    for (const auto& point : BuildCurrentPath(target)) {
        path.push_back(point.To2D());
    }

    if (path.empty()) {
        return UnitPosition2D(target);
    }
    return SDK::Geometry::PositionAlongPath(path, travelDistance);
}

inline float GetRealRadius(const PredictionInput& input, const AIBaseClient& target) {
    const float baseRadius = std::max(input.Radius, 0.0f);
    return input.UseBoundingRadius ? (baseRadius + target.BoundingRadius()) : baseRadius;
}

} // namespace detail

inline Vec3 ResolveFrom(const PredictionInput& input) {
    if (!input.From.IsZero()) {
        return input.From;
    }

    const auto player = ObjectManager::Player();
    return player.IsValid() ? player.Position() : Vec3();
}

inline Vec3 ResolveRangeCheckFrom(const PredictionInput& input, const Vec3& fallback) {
    return input.RangeCheckFrom.IsZero() ? fallback : input.RangeCheckFrom;
}

inline Vec3 ResolveUnitPosition(const AIBaseClient& target) {
    const auto server = target.ServerPosition();
    return server.IsZero() ? target.Position() : server;
}

inline Vec2 ResolveUnitPosition2D(const AIBaseClient& target) {
    return ResolveUnitPosition(target).To2D();
}

inline bool InRange(const Vec3& from, const Vec3& to, float range, float radius) {
    if (detail::IsMaxRange(range)) {
        return true;
    }
    return from.Distance2D(to) <= (range + radius);
}

inline std::vector<GameObject> CollectLineCollisions(const Vec3& from,
                                                     const Vec3& to,
                                                     float radius,
                                                     const AIBaseClient& target,
                                                     CollisionableObjects mask) {
    std::vector<GameObject> out = {};
    if (from.IsZero() || to.IsZero()) {
        return out;
    }

    const Vec2 lineStart = from.To2D();
    const Vec2 lineEnd = to.To2D();

    const auto collides = [&](const AIBaseClient& unit) {
        if (!unit.IsValid() || unit.IsDead()) {
            return false;
        }
        if (target.IsValid() && unit.NetworkId() == target.NetworkId()) {
            return false;
        }

        const float hitRadius = radius + unit.BoundingRadius();
        return SDK::Geometry::PointToSegmentDistance(ResolveUnitPosition2D(unit), lineStart, lineEnd) <= hitRadius;
    };

    if (HasFlag(mask, CollisionableObjects::Minions)) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (collides(minion)) {
                out.emplace_back(minion);
            }
        }

        for (const auto& minion : GameObjects::Jungle()) {
            if (collides(minion)) {
                out.emplace_back(minion);
            }
        }
    }

    if (HasFlag(mask, CollisionableObjects::Heroes)) {
        for (const auto& hero : GameObjects::EnemyHeroes()) {
            if (collides(hero)) {
                out.emplace_back(hero);
            }
        }
    }

    if (HasFlag(mask, CollisionableObjects::Walls)) {
        const auto start = Vec3::From2D(lineStart, from.y);
        const auto end = Vec3::From2D(lineEnd, to.y);
        if (CoreNavGrid::IsWallBetween(start, end)) {
            out.emplace_back(ObjectManager::Player());
        }
    }

    if (HasFlag(mask, CollisionableObjects::YasuoWall) &&
        detail::HasYasuoWallCollision(lineStart, lineEnd, radius)) {
        out.emplace_back(ObjectManager::Player());
    }

    if (HasFlag(mask, CollisionableObjects::BraumShield) &&
        detail::HasBraumShieldCollision(lineStart, lineEnd, radius)) {
        out.emplace_back(ObjectManager::Player());
    }

    return out;
}

inline PredictionOutput GetPositionOnPath(PredictionInput input,
                                          const std::vector<Vec3>& path3d,
                                          float speed = -1.0f) {
    PredictionOutput result = {};
    if (!input.Unit.IsValid()) {
        return result;
    }

    const AIBaseClient target = input.Unit;
    const float targetSpeed = (speed < 0.0f) ? target.MoveSpeed() : speed;
    std::vector<Vec2> path = {};
    path.reserve(path3d.size());
    for (const auto& point : path3d) {
        path.push_back(point.To2D());
    }

    if (path.size() <= 1) {
        const Vec3 pos = path3d.empty() ? ResolveUnitPosition(target) : path3d.front();
        result.Input = input;
        result.CastPosition = pos;
        result.UnitPosition = pos;
        result.Hitchance = HitChance::VeryHigh;
        return result;
    }

    const float realRadius = detail::GetRealRadius(input, target);
    const float pathLength = SDK::Geometry::PathLength(path);
    const float triggerDistance = std::max(0.0f, input.Delay * targetSpeed - realRadius);

    if (pathLength >= triggerDistance && detail::IsInstantSpeed(input.Speed)) {
        float remaining = triggerDistance;
        for (size_t i = 0; i + 1 < path3d.size(); ++i) {
            const Vec3 a = path3d[i];
            const Vec3 b = path3d[i + 1];
            const float segLen = a.Distance2D(b);

            if (segLen >= remaining) {
                const Vec3 dir = (b - a).Normalized2D();
                const Vec3 castPos = a + (dir * remaining);
                const float push = (i == path3d.size() - 2)
                    ? std::min(remaining + realRadius, segLen)
                    : (remaining + realRadius);
                const Vec3 unitPos = a + (dir * push);

                result.Input = input;
                result.CastPosition = castPos;
                result.UnitPosition = unitPos;
                result.Hitchance = detail::GetPathHitchance(target);
                return result;
            }

            remaining -= segLen;
        }
    }

    if (pathLength >= triggerDistance && !detail::IsInstantSpeed(input.Speed) && input.Speed > 0.0f) {
        float distance = triggerDistance;
        if ((input.Type == SkillshotType::SkillshotLine || input.Type == SkillshotType::SkillshotCone) &&
            input.From.DistanceSqr2D(path3d.front()) < (200.0f * 200.0f)) {
            distance = input.Delay * targetSpeed;
        }

        path = SDK::Geometry::CutPath(path, std::max(0.0f, distance));
        float timeOffset = 0.0f;

        for (size_t i = 0; i + 1 < path.size(); ++i) {
            const Vec2 start = path[i];
            const Vec2 end = path[i + 1];
            const float segLen = start.Distance(end);
            const float segTime = segLen / std::max(targetSpeed, 1.0f);
            const Vec2 dir = (end - start).Normalized();

            const auto solution = SDK::Geometry::VectorMovementCollision(
                start, end, targetSpeed, input.From.To2D(), input.Speed, timeOffset);

            if (solution.IsValid() &&
                solution.Time >= timeOffset &&
                solution.Time <= (timeOffset + segTime)) {
                if (solution.Position.Distance(end) < 20.0f) {
                    timeOffset += segTime;
                    continue;
                }

                Vec3 castPos = Vec3::From2D(solution.Position, ResolveUnitPosition(target).y);
                const Vec2 unitPos2 = solution.Position + (dir * realRadius);
                Vec3 unitPos = Vec3::From2D(unitPos2, ResolveUnitPosition(target).y);

                if (input.Type == SkillshotType::SkillshotLine) {
                    const Vec2 from2 = input.From.To2D();
                    const Vec2 p2 = unitPos2;
                    const float alpha = std::fabs(SDK::Geometry::RadToDeg((from2 - p2).AngleBetween(start - end)));
                    if (alpha > 30.0f && alpha < 150.0f) {
                        const float dist = std::max(p2.Distance(from2), 1.0f);
                        const float sinBeta = realRadius / dist;
                        if (sinBeta > -1.0f && sinBeta < 1.0f) {
                            const float beta = std::asin(sinBeta);
                            const Vec2 cp1 = from2 + (p2 - from2).Rotated(beta);
                            const Vec2 cp2 = from2 + (p2 - from2).Rotated(-beta);
                            castPos = (cp1.DistanceSqr(solution.Position) < cp2.DistanceSqr(solution.Position))
                                ? Vec3::From2D(cp1, castPos.y)
                                : Vec3::From2D(cp2, castPos.y);
                        }
                    }
                }

                result.Input = input;
                result.CastPosition = castPos;
                result.UnitPosition = unitPos;
                result.Hitchance = detail::GetPathHitchance(target);
                return result;
            }

            timeOffset += segTime;
        }
    }

    result.Input = input;
    result.CastPosition = path3d.back();
    result.UnitPosition = path3d.back();
    result.Hitchance = HitChance::Medium;
    return result;
}

inline PredictionOutput GetAdvancedPrediction(PredictionInput input, float additionalSpeed = 0.0f) {
    PredictionOutput result = {};
    if (!input.Unit.IsValid()) {
        return result;
    }

    const AIBaseClient target = input.Unit;

    float arrival = std::max(input.Delay, 0.0f);
    if (!detail::IsInstantSpeed(input.Speed) && input.Speed > 1.0f) {
        const Vec3 from = ResolveFrom(input);
        arrival += from.Distance2D(ResolveUnitPosition(target)) / input.Speed;
    }

    float moveSpeed = target.MoveSpeed();
    if (std::fabs(additionalSpeed) > FLT_EPSILON) {
        moveSpeed = std::max(moveSpeed * additionalSpeed, 0.0f);
    }

    const Vec2 position = detail::PositionAfter(target, arrival, moveSpeed);
    const Vec3 predicted(position.x, ResolveUnitPosition(target).y, position.y);

    result.Input = input;
    result.UnitPosition = predicted;
    result.CastPosition = predicted;
    result.Hitchance = target.IsMoving() ? HitChance::Medium : HitChance::High;
    return result;
}

inline PredictionOutput GetImmobilePrediction(PredictionInput input, double remainingImmobileT) {
    PredictionOutput result = {};
    if (!input.Unit.IsValid()) {
        return result;
    }

    const AIBaseClient target = input.Unit;
    const float moveSpeed = std::max(target.MoveSpeed(), 1.0f);
    float timeToReach = input.Delay;
    if (!detail::IsInstantSpeed(input.Speed)) {
        timeToReach += target.Distance(ResolveFrom(input)) / input.Speed;
    }

    result.Input = input;
    result.CastPosition = ResolveUnitPosition(target);
    result.UnitPosition = ResolveUnitPosition(target);
    result.Hitchance = (timeToReach <= static_cast<float>(remainingImmobileT) +
                                       detail::GetRealRadius(input, target) / moveSpeed)
        ? HitChance::Immobile
        : HitChance::High;
    return result;
}

inline PredictionOutput GetStandardPrediction(PredictionInput input) {
    PredictionOutput result = {};
    if (!input.Unit.IsValid()) {
        return result;
    }

    const AIBaseClient target = input.Unit;
    float speed = target.MoveSpeed();
    if (target.Position().DistanceSqr2D(ResolveFrom(input)) < (200.0f * 200.0f)) {
        speed /= 1.5f;
    }

    return GetPositionOnPath(input, detail::BuildCurrentPath(target), speed);
}

inline PredictionOutput GetDashingPrediction(PredictionInput input) {
    PredictionOutput result = {};
    if (!input.Unit.IsValid()) {
        return result;
    }

    const AIBaseClient target = input.Unit;
    const auto dashData = SDK::Events::Dash::GetDashInfo(target);
    if (!dashData.IsDash || dashData.EndTick <= dashData.StartTick) {
        return result;
    }

    input.Delay += 0.1f;

    std::vector<Vec3> dashPath = {};
    dashPath.reserve(dashData.PathCount);
    for (int i = 0; i < dashData.PathCount; ++i) {
        dashPath.push_back(dashData.Path[i]);
    }
    if (dashPath.size() < 2) {
        dashPath.insert(dashPath.begin(), ResolveUnitPosition(target));
    }

    auto dashPrediction = GetPositionOnPath(input, dashPath, dashData.Speed);
    if (static_cast<int>(dashPrediction.Hitchance) >= static_cast<int>(HitChance::High)) {
        dashPrediction.CastPosition = dashPrediction.UnitPosition;
        dashPrediction.Hitchance = HitChance::Dashing;
        return dashPrediction;
    }

    const Vec3 dashEnd = dashPath.back();
    if (SDK::Geometry::PathLength(dashPath) > 200.0f) {
        float timeToPoint = input.Delay;
        if (!detail::IsInstantSpeed(input.Speed)) {
            timeToPoint += ResolveFrom(input).Distance2D(dashEnd) / input.Speed;
        }

        const float arrivalTime = target.Distance(dashEnd) / std::max(dashData.Speed, 1.0f) +
                                  detail::GetRealRadius(input, target) / std::max(target.MoveSpeed(), 1.0f);
        if (timeToPoint <= arrivalTime) {
            result.Input = input;
            result.CastPosition = dashEnd;
            result.UnitPosition = dashEnd;
            result.Hitchance = HitChance::Dashing;
            return result;
        }
    }

    result.Input = input;
    result.CastPosition = dashEnd;
    result.UnitPosition = dashEnd;
    return result;
}

inline PredictionOutput GetPredictionCore(PredictionInput input,
                                          bool adjustDelay,
                                          bool checkCollision,
                                          bool checkRange) {
    PredictionOutput result = {};
    if (!input.Unit.IsValid() || input.Unit.IsDead()) {
        return result;
    }

    input.From = ResolveFrom(input);
    input.RangeCheckFrom = ResolveRangeCheckFrom(input, input.From);
    result.Input = input;

    if (adjustDelay) {
        input.Delay += (Game::Ping() / 2000.0f) + 0.06f;

        if (input.AoE && g_aoePredictionCallback) {
            return g_aoePredictionCallback(input);
        }
    }

    if (!detail::IsMaxRange(input.Range) &&
        input.Unit.Position().DistanceSqr2D(input.RangeCheckFrom) > std::pow(input.Range * 1.5f, 2.0f)) {
        return result;
    }

    if (input.Unit.IsDashing()) {
        result = GetDashingPrediction(input);
    } else {
        const double immobileUntil = detail::UnitIsImmobileUntil(input.Unit);
        if (immobileUntil >= 0.0) {
            result = GetImmobilePrediction(input, immobileUntil);
        }
    }

    if (!result.IsValid()) {
        result = GetAdvancedPrediction(input);
    }

    const float radius = detail::GetRealRadius(input, input.Unit);
    if (checkRange && !detail::IsMaxRange(input.Range)) {
        if (static_cast<int>(result.Hitchance) >= static_cast<int>(HitChance::High) &&
            input.RangeCheckFrom.DistanceSqr2D(ResolveUnitPosition(input.Unit)) > std::pow(input.Range + (radius * 0.75f), 2.0f)) {
            result.Hitchance = HitChance::Medium;
        }

        const float unitAllowance = input.Type == SkillshotType::SkillshotCircle ? radius : 0.0f;
        if (input.RangeCheckFrom.DistanceSqr2D(result.UnitPosition) > std::pow(input.Range + unitAllowance, 2.0f)) {
            result.Hitchance = HitChance::OutOfRange;
        }

        if (input.RangeCheckFrom.DistanceSqr2D(result.CastPosition) > std::pow(input.Range, 2.0f)) {
            if (result.Hitchance != HitChance::OutOfRange) {
                result.CastPosition = input.RangeCheckFrom.Extend(result.UnitPosition, input.Range);
            } else {
                result.Hitchance = HitChance::OutOfRange;
            }
        }
    }

    if (checkCollision && input.Collision && input.Type == SkillshotType::SkillshotLine) {
        result.CollisionObjects = CollectLineCollisions(input.From, result.CastPosition, radius, input.Unit, input.CollisionObjects);
        result.Hitchance = result.CollisionObjects.empty() ? result.Hitchance : HitChance::Collision;
    }

    return result;
}

inline PredictionOutput GetPrediction(const PredictionInput& input, const AIBaseClient& target) {
    PredictionInput local = input;
    local.Unit = target;
    return GetPredictionCore(local, true, true, true);
}

inline PredictionOutput GetPrediction(const AIBaseClient& target, const PredictionInput& input) {
    return GetPrediction(input, target);
}

inline PredictionOutput GetPrediction(PredictionInput input, bool checkCollision, bool checkRange) {
    return GetPredictionCore(input, false, checkCollision, checkRange);
}

inline PredictionOutput GetPrediction(const AIBaseClient& target,
                                      PredictionInput input,
                                      bool checkCollision,
                                      bool checkRange) {
    input.Unit = target;
    return GetPredictionCore(input, false, checkCollision, checkRange);
}

inline PredictionOutput GetPrediction(const PredictionInput& input) {
    return GetPredictionCore(input, true, true, true);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit, float delay) {
    PredictionInput input = {};
    input.Unit = unit;
    input.Delay = delay;
    return GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit, float delay, float radius) {
    PredictionInput input = {};
    input.Unit = unit;
    input.Delay = delay;
    input.Radius = radius;
    return GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit, float delay, float radius, float speed) {
    PredictionInput input = {};
    input.Unit = unit;
    input.Delay = delay;
    input.Radius = radius;
    input.Speed = speed;
    return GetPrediction(input);
}

inline bool WillHit(const AIBaseClient& target, const PredictionInput& input, HitChance minimum = HitChance::High) {
    const auto result = GetPrediction(target, input);
    return static_cast<int>(result.Hitchance) >= static_cast<int>(minimum);
}

} // namespace SDK::Prediction::Movement

namespace SDK::Movement {

inline PredictionOutput GetPrediction(const AIBaseClient& unit, float delay) {
    return Prediction::Movement::GetPrediction(unit, delay);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit, float delay, float radius) {
    return Prediction::Movement::GetPrediction(unit, delay, radius);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit, float delay, float radius, float speed) {
    return Prediction::Movement::GetPrediction(unit, delay, radius, speed);
}

inline PredictionOutput GetPrediction(const PredictionInput& input) {
    return Prediction::Movement::GetPrediction(input);
}

inline PredictionOutput GetPrediction(PredictionInput input, bool checkCollision, bool checkRange) {
    return Prediction::Movement::GetPrediction(input, checkCollision, checkRange);
}

inline PredictionOutput GetPrediction(const AIBaseClient& target, const PredictionInput& input) {
    return Prediction::Movement::GetPrediction(target, input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& target,
                                      PredictionInput input,
                                      bool checkCollision,
                                      bool checkRange) {
    return Prediction::Movement::GetPrediction(target, input, checkCollision, checkRange);
}

inline bool WillHit(const AIBaseClient& target, const PredictionInput& input, HitChance minimum = HitChance::High) {
    return Prediction::Movement::WillHit(target, input, minimum);
}

} // namespace SDK::Movement
