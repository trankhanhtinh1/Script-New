#pragma once

// Native model of Skillshot.cs from the supplied Evade project.  It wraps the
// NightSharp SDK shape (kept for special-spell geometry and ImGui drawing) and
// owns the timing/path-safety behaviour used by the replacement engine.

#include "../Config/EvadeConfig.h"
#include "Geometry.h"
#include "../Database/SpellData.h"
#include "../SpecialSpells/SpecialSpellCommon.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace Plugins::KuroEvade {

enum class SourceDetectionType {
    ProcessSpell,
    MissileCreate,
    ObjectCreate,
    Simulated,
};

struct SourcePathIntersection {
    Vec2 ComingFrom;
    Vec2 Point;
    float Distance = 0.0f;
    int Time = 0;
    bool Valid = false;
};

struct SourceSafePathResult {
    bool IsSafe = true;
    SourcePathIntersection Intersection;
};

class SourceSkillshot final {
public:
    using NativePtr = std::shared_ptr<SDK::Skillshot>;

    SourceSkillshot(NativePtr native,
                    const Database::SpellData& data,
                    SourceDetectionType detectionType,
                    int id)
        : Native(std::move(native)),
          Data(data),
          DetectionType(detectionType),
          Id(id) {
        if (Native) {
            OriginalEnd = Native->EndPosition;
            CollisionEnd = Native->EndPosition;
        }
    }

    NativePtr Native;
    Database::SpellData Data;
    SourceDetectionType DetectionType = SourceDetectionType::ProcessSpell;
    Vec2 OriginalEnd;
    Vec2 CollisionEnd;
    int Id = 0;
    int TrapObjectId = 0;
    int MissileNetworkId = 0;
    bool ForceDisabled = false;
    bool Persistent = false;
    bool FromFog = false;
    float EvadeTime = -FLT_MAX;
    float SpellHitTime = -FLT_MAX;

    bool IsValid() const {
        return Native != nullptr;
    }

    int StartTick() const {
        return Native ? Native->StartTime : 0;
    }

    const SDK::SpellDatabaseEntry& SpellData() const {
        static const SDK::SpellDatabaseEntry empty;
        return Native ? Native->SData : empty;
    }

    Vec2 Start() const {
        return Native ? Native->StartPosition : Vec2();
    }

    Vec2 End() const {
        return Native ? Native->EndPosition : Vec2();
    }

    Vec2 Direction() const {
        return Native ? Native->Direction : Vec2();
    }

    bool HasMissile() const {
        return Native && Native->HasMissile();
    }

    bool IsLine() const {
        return Native && SDK::IsLineSpellType(Native->SData.SpellType);
    }

    bool IsCircle() const {
        return Native && SDK::IsCircleSpellType(Native->SData.SpellType);
    }

    bool IsCone() const {
        return Native &&
            (Native->SData.SpellType == SDK::SpellType::SkillshotCone ||
             Native->SData.SpellType == SDK::SpellType::SkillshotMissileCone);
    }

    bool IsRing() const {
        return Native && Native->SData.SpellType == SDK::SpellType::SkillshotRing;
    }

    bool IsArc() const {
        return Native &&
            (Native->SData.SpellType == SDK::SpellType::SkillshotArc ||
             Native->SData.SpellType == SDK::SpellType::SkillshotMissileArc);
    }

    bool IsFiniteMissile() const {
        return Native && Native->SData.MissileSpeed > 0 &&
            Native->SData.MissileSpeed != INT_MAX && HasMissile();
    }

    int ExtraDurationMs() const {
        return std::max(0, static_cast<int>(Data.ExtraEndTime));
    }

    float RawRadius() const {
        if (!Native) {
            return 0.0f;
        }
        return static_cast<float>(std::max(0, Native->SData.Radius));
    }

    float EffectiveRadius(const EvadeSettings& settings,
                          float unitRadius = 0.0f) const {
        return RawRadius() + std::max(0.0f, unitRadius) +
            static_cast<float>(std::max(0, settings.SkillShotsExtraRadius));
    }

    Vec2 EffectiveEnd(const EvadeSettings& settings) const {
        if (!Native) return {};
        const Vec2 end = CollisionEnd.IsZero() ? Native->EndPosition : CollisionEnd;
        const bool collisionShortened = !OriginalEnd.IsZero() &&
            !CollisionEnd.IsZero() && CollisionEnd.DistanceSqr(OriginalEnd) > 1.0f;
        if (!IsLine() || collisionShortened || settings.SkillShotsExtraRange <= 0) {
            return end;
        }
        Vec2 direction = Native->Direction;
        if (direction.IsZero()) direction = (end - Native->StartPosition).Normalized();
        return end + direction * static_cast<float>(settings.SkillShotsExtraRange);
    }

    float TravelDistance() const {
        return Native ? Native->StartPosition.Distance(CollisionEnd.IsZero()
            ? Native->EndPosition : CollisionEnd) : 0.0f;
    }

    int EndTick() const {
        if (!Native) {
            return 0;
        }
        if (Persistent) {
            return INT_MAX;
        }
        if (Native->SData.MissileAccel != 0) {
            return Native->StartTime + 5000 + ExtraDurationMs();
        }
        const float speed = Native->SData.MissileSpeed <= 0 ||
                            Native->SData.MissileSpeed == INT_MAX
            ? 100000000.0f
            : static_cast<float>(Native->SData.MissileSpeed);
        return Native->StartTime + std::max(0, Native->SData.Delay) +
            static_cast<int>(1000.0f * TravelDistance() / speed) +
            ExtraDurationMs() + 100;
    }

    bool IsActive(int now = SDK::Variables::TickCount()) const {
        return Native && (Persistent || now <= EndTick());
    }

    Vec2 MissilePosition(int afterTimeMs = 0) const {
        if (!Native) {
            return {};
        }
        if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(Native.get())) {
            if (Native->SData.MissileAccel != 0) {
                const Vec2 predicted = missile->GetMissilePosition(afterTimeMs);
                return Native->StartPosition + Native->Direction * std::clamp(
                    Native->StartPosition.Distance(predicted),
                    0.0f, TravelDistance());
            }
            if (missile->Missile.IsValid()) {
                const Vec2 current = missile->Missile.Position().To2D();
                const Vec2 end = missile->Missile.EndPosition().To2D();
                const float speed = std::max(
                    1.0f, static_cast<float>(Native->SData.MissileSpeed));
                const float distance = std::min(current.Distance(end),
                    speed * std::max(0, afterTimeMs) / 1000.0f);
                const Vec2 direction = (end - current).Normalized();
                if (!direction.IsZero()) {
                    return current + direction * distance;
                }
            }
            return missile->GetMissilePosition(afterTimeMs);
        }

        const int elapsed = std::max(0,
            SDK::Variables::TickCount() + afterTimeMs - Native->StartTime -
            std::max(0, Native->SData.Delay));
        const float speed = Native->SData.MissileSpeed <= 0 ||
                            Native->SData.MissileSpeed == INT_MAX
            ? 0.0f
            : static_cast<float>(Native->SData.MissileSpeed);
        const float distance = std::clamp(
            speed * static_cast<float>(elapsed) / 1000.0f,
            0.0f, TravelDistance());
        return Native->StartPosition + Native->Direction * distance;
    }

    std::vector<Vec2> PolygonPoints() const {
        return Native ? SourceGeometry::ToPoints(Native->Path) : std::vector<Vec2>();
    }

    bool ContainsStatic(const Vec2& point,
                        float unitRadius,
                        const EvadeSettings& settings) const {
        if (!Native) {
            return false;
        }
        const float radius = EffectiveRadius(settings, unitRadius);
        if (IsLine()) {
            const Vec2 lineStart = IsFiniteMissile() ? MissilePosition(0) : Native->StartPosition;
            return SourceGeometry::PointSegmentDistance(
                point, lineStart, EffectiveEnd(settings)) <= radius;
        }
        if (IsCircle()) {
            return point.Distance(CollisionEnd.IsZero() ? Native->EndPosition : CollisionEnd) <= radius;
        }
        if (IsRing()) {
            const float distance = point.Distance(Native->EndPosition);
            const float padding = unitRadius + settings.SkillShotsExtraRadius;
            const float outer = RawRadius() +
                static_cast<float>(Native->SData.RingRadius) + padding;
            const float inner = std::max(0.0f,
                RawRadius() - static_cast<float>(Native->SData.RingRadius) - padding);
            return distance >= inner && distance <= outer;
        }

        const auto polygon = PolygonPoints();
        return SourceGeometry::DistanceToPolygon(point, polygon) <=
            std::max(0.0f, unitRadius + settings.SkillShotsExtraRadius);
    }

    float HitTime(const Vec2& point,
                  const EvadeSettings& settings,
                  int now = SDK::Variables::TickCount()) const {
        if (!Native) {
            return FLT_MAX;
        }
        const float latency = static_cast<float>(std::max(0, SDK::Game::Ping())) * 0.5f;
        if (IsLine() && IsFiniteMissile()) {
            const Vec2 missile = MissilePosition(0);
            const Vec2 projection = SourceGeometry::ProjectOn(
                point, missile, EffectiveEnd(settings)).SegmentPoint;
            const float forward = (projection - missile).Dot(Native->Direction);
            if (forward < -RawRadius()) {
                return FLT_MAX;
            }
            return std::max(0.0f,
                1000.0f * std::max(0.0f, forward) /
                    static_cast<float>(std::max(1, Native->SData.MissileSpeed)) - latency);
        }
        return std::max(0.0f, static_cast<float>(EndTick() - now) - latency);
    }

    bool CanHeroEvade(const SDK::AIHeroClient& hero,
                      const EvadeSettings& settings,
                      float* evadeTimeOut = nullptr,
                      float* hitTimeOut = nullptr) const {
        if (!Native || !hero.IsValid()) {
            return false;
        }
        const Vec2 heroPos = hero.ServerPosition().To2D();
        const float speed = std::max(50.0f, hero.MoveSpeed());
        float distanceOutside = 0.0f;
        if (IsLine()) {
            distanceOutside = std::max(0.0f,
                EffectiveRadius(settings, hero.BoundingRadius()) -
                SourceGeometry::PointSegmentDistance(heroPos, Native->StartPosition,
                    CollisionEnd.IsZero() ? Native->EndPosition : CollisionEnd));
        } else if (IsCircle()) {
            distanceOutside = std::max(0.0f,
                EffectiveRadius(settings, hero.BoundingRadius()) -
                heroPos.Distance(CollisionEnd.IsZero() ? Native->EndPosition : CollisionEnd));
        } else {
            const auto polygon = PolygonPoints();
            distanceOutside = SourceGeometry::PointInPolygon(heroPos, polygon)
                ? std::max(15.0f, hero.BoundingRadius())
                : 0.0f;
        }
        const float evadeTime = 1000.0f * distanceOutside / speed;
        const float hitTime = HitTime(heroPos, settings);
        if (evadeTimeOut) {
            *evadeTimeOut = evadeTime;
        }
        if (hitTimeOut) {
            *hitTimeOut = hitTime;
        }
        return hitTime > evadeTime;
    }

    bool ContainsAt(const Vec2& point,
                    float unitRadius,
                    int afterTimeMs,
                    const EvadeSettings& settings) const {
        if (!Native || afterTimeMs < 0) {
            return false;
        }
        const int absoluteTick = SDK::Variables::TickCount() + afterTimeMs;
        if (!Persistent && absoluteTick > EndTick()) {
            return false;
        }

        if (IsLine() && IsFiniteMissile()) {
            const int launchTick = Native->StartTime + std::max(0, Native->SData.Delay);
            if (absoluteTick < launchTick) {
                return false;
            }
            const Vec2 missile = MissilePosition(afterTimeMs);
            return point.Distance(missile) <= EffectiveRadius(
                settings, unitRadius);
        }

        const int impactTick = EndTick() - ExtraDurationMs() - 100;
        const int tolerance = 20;
        if (!Persistent && ExtraDurationMs() <= 0 &&
            std::abs(absoluteTick - impactTick) > tolerance + 35) {
            return false;
        }
        if (absoluteTick + tolerance < impactTick) {
            return false;
        }
        return ContainsStatic(point, unitRadius, settings);
    }

    SourceSafePathResult IsSafePath(const std::vector<Vec2>& path,
                                    int timeOffset,
                                    float speed,
                                    int delay,
                                    float unitRadius,
                                    const EvadeSettings& settings) const {
        SourceSafePathResult result;
        if (!Native || path.size() < 2) {
            result.IsSafe = !ContainsStatic(path.empty() ? Vec2() : path.front(),
                                            unitRadius, settings);
            return result;
        }

        speed = std::max(50.0f, speed);
        delay = std::max(0, delay);
        timeOffset = std::max(0, timeOffset) + std::max(0, SDK::Game::Ping()) / 2;
        const float length = SourceGeometry::PathLength(path);
        const int arrival = delay + static_cast<int>(1000.0f * length / speed);
        int horizon = std::min(6000, std::max(arrival + timeOffset,
            std::min(INT_MAX / 2, EndTick() - SDK::Variables::TickCount() + timeOffset)));
        horizon = std::max(horizon, delay);

        const int step = IsFiniteMissile() ? 18 : 25;
        Vec2 previous = path.front();
        for (int t = 0; t <= horizon; t += step) {
            const float travelled = t <= delay
                ? 0.0f
                : speed * static_cast<float>(t - delay) / 1000.0f;
            const Vec2 position = SourceGeometry::PositionAfter(path, travelled);
            if (ContainsAt(position, unitRadius, t, settings)) {
                result.IsSafe = false;
                result.Intersection.ComingFrom = previous;
                result.Intersection.Point = position;
                result.Intersection.Distance = std::min(length, travelled);
                result.Intersection.Time = t;
                result.Intersection.Valid = !position.IsZero();
                return result;
            }
            previous = position;
        }

        // Preserve the source's conservative route-change offset by testing
        // both sides of the predicted collision window.
        for (int delta : { -timeOffset, timeOffset }) {
            const int t = std::clamp(arrival + delta, 0, horizon);
            const float travelled = t <= delay
                ? 0.0f
                : speed * static_cast<float>(t - delay) / 1000.0f;
            if (ContainsAt(SourceGeometry::PositionAfter(path, travelled),
                           unitRadius, t, settings)) {
                result.IsSafe = false;
                return result;
            }
        }
        return result;
    }

    bool IsAboutToHit(int timeMs,
                      const SDK::AIBaseClient& unit,
                      const EvadeSettings& settings) const {
        if (!Native || !unit.IsValid()) {
            return false;
        }
        const Vec2 position = unit.ServerPosition().To2D();
        if (IsLine() && IsFiniteMissile()) {
            const Vec2 from = MissilePosition(0);
            const Vec2 to = MissilePosition(std::max(0, timeMs));
            return SourceGeometry::PointSegmentDistance(position, from, to) <=
                EffectiveRadius(settings, unit.BoundingRadius());
        }
        return HitTime(position, settings) <= static_cast<float>(std::max(0, timeMs)) &&
            ContainsStatic(position, unit.BoundingRadius(), settings);
    }

    std::vector<std::vector<Vec2>> EvadeBoundaries(float unitRadius,
                                                    float extraEvadeDistance,
                                                    const EvadeSettings& settings) const {
        std::vector<std::vector<Vec2>> result;
        if (!Native) {
            return result;
        }
        const float padding = std::max(0.0f, unitRadius) +
            std::max(0.0f, extraEvadeDistance) +
            static_cast<float>(std::max(0, settings.SkillShotsExtraRadius));
        if (IsLine()) {
            result.push_back(SourceGeometry::RectanglePoints(
                IsFiniteMissile() ? MissilePosition(0) : Native->StartPosition,
                EffectiveEnd(settings),
                RawRadius() + padding));
        } else if (IsCircle()) {
            result.push_back(SourceGeometry::CirclePoints(
                CollisionEnd.IsZero() ? Native->EndPosition : CollisionEnd,
                RawRadius() + padding));
        } else if (IsRing()) {
            result.push_back(SourceGeometry::CirclePoints(
                Native->EndPosition, RawRadius() +
                    static_cast<float>(Native->SData.RingRadius) + padding));
            const float inner = std::max(5.0f,
                RawRadius() - static_cast<float>(Native->SData.RingRadius) - padding);
            result.push_back(SourceGeometry::CirclePoints(Native->EndPosition, inner));
        } else if (IsCone()) {
            result.push_back(SourceGeometry::SectorPoints(
                Native->StartPosition, Native->Direction,
                static_cast<float>(std::max(1, Native->SData.Angle)) *
                    SourceGeometry::Pi / 180.0f,
                static_cast<float>(std::max(1, Native->SData.Range)), padding));
        } else {
            auto polygon = PolygonPoints();
            if (!polygon.empty()) {
                // Arc and other uncommon SDK polygons are already built by
                // the retained special-spell geometry. Push vertices away
                // from their centroid to obtain the source evade polygon.
                Vec2 center;
                for (const Vec2& point : polygon) {
                    center = center + point;
                }
                center = center / static_cast<float>(polygon.size());
                for (Vec2& point : polygon) {
                    Vec2 direction = (point - center).Normalized();
                    if (!direction.IsZero()) {
                        point = point + direction * padding;
                    }
                }
                result.push_back(std::move(polygon));
            }
        }
        return result;
    }
};

using SourceSkillshotPtr = std::shared_ptr<SourceSkillshot>;
using SourceSkillshotList = std::vector<SourceSkillshotPtr>;

} // namespace Plugins::KuroEvade
