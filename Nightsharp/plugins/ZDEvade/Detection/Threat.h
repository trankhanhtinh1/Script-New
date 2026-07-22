#pragma once

#include "../Database/SpellData.h"
#include "../../../Core/Vector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace ZDEvade {

inline int ClampTick(std::int64_t tick) {
    return static_cast<int>(std::clamp(
        tick,
        static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

inline int ClampTickOffset(double offset) {
    if (std::isnan(offset)) return 0;
    return static_cast<int>(std::clamp(
        offset,
        static_cast<double>(std::numeric_limits<int>::min()),
        static_cast<double>(std::numeric_limits<int>::max())));
}

inline int SaturatingTickAdd(int tick, std::int64_t duration) {
    const std::int64_t base = tick;
    const std::int64_t minimum = std::numeric_limits<int>::min();
    const std::int64_t maximum = std::numeric_limits<int>::max();
    if (duration > maximum - base) return std::numeric_limits<int>::max();
    if (duration < minimum - base) return std::numeric_limits<int>::min();
    return static_cast<int>(base + duration);
}

inline std::int64_t TickDifference(int left, int right) {
    return static_cast<std::int64_t>(left) - static_cast<std::int64_t>(right);
}

inline bool TickAfter(int left, int right) {
    return TickDifference(left, right) > 0;
}

inline int SaturatingDurationAdd(int first, int second) {
    return ClampTick(
        std::max<std::int64_t>(0, first) +
        std::max<std::int64_t>(0, second));
}

inline int EffectiveEndExplosionDelay(const SpellData& data) {
    return std::max(0, data.endExplosionDelay > 0
        ? data.endExplosionDelay
        : data.extraDelay);
}

inline int EffectiveEndExplosionDuration(const SpellData& data) {
    return std::max(0, data.endExplosionDuration > 0
        ? data.endExplosionDuration
        : data.extraEndTime);
}

inline int ThreatLifecycleLinger(int extraEndTime,
                                 bool hasEndExplosion,
                                 int explosionDelay,
                                 int explosionDuration) {
    const int explosionLinger = hasEndExplosion
        ? SaturatingDurationAdd(explosionDelay, explosionDuration)
        : 0;
    return std::max(std::max(0, extraEndTime), explosionLinger);
}

inline int CalculateThreatEndTick(const SpellData& data,
                                  const Vec2& start,
                                  const Vec2& end,
                                  int startTick,
                                  int launchTick,
                                  int delayOverride = -1) {
    const int delay = delayOverride >= 0
        ? delayOverride
        : std::max(0, data.spellDelay);
    const int linger = ThreatLifecycleLinger(
        data.extraEndTime,
        data.hasEndExplosion,
        EffectiveEndExplosionDelay(data),
        EffectiveEndExplosionDuration(data));
    int result = SaturatingTickAdd(
        SaturatingTickAdd(startTick, delay), linger);
    // Arc travel is not modeled as a straight start/end chord.
    if (data.spellType == ZDSpellType::Arc) return result;
    if (std::isfinite(data.projectileSpeed) &&
        data.projectileSpeed > 1.0f &&
        data.projectileSpeed < 100000.0f) {
        const double travelMs = std::ceil(
            1000.0 * static_cast<double>(start.Distance(end)) /
            static_cast<double>(data.projectileSpeed));
        const int travel = ClampTick(static_cast<std::int64_t>(std::clamp(
            travelMs,
            0.0,
            static_cast<double>(std::numeric_limits<int>::max()))));
        const int travelBase = launchTick > 0
            ? launchTick
            : SaturatingTickAdd(startTick, delay);
        result = SaturatingTickAdd(
            SaturatingTickAdd(travelBase, travel), linger);
    }
    return result;
}

enum class ZDCollisionKind : std::uint8_t {
    None,
    Unit,
    ProjectileWall,
    Terrain
};

struct Threat {
    int id = -1;
    int revision = 0;
    const SpellData* data = nullptr;
    int dangerOverride = 0;
    Vec2 startPos = {};
    Vec2 endPos = {};
    Vec2 authoredEndPos = {};
    Vec2 direction = {};
    Vec2 collisionUnitCenter = {};
    Vec2 collisionExplosionCenter = {};
    Vec2 lastConsumedCollisionPoint = {};
    int startTick = 0;
    int launchTick = 0;
    int endTick = 0;
    std::uintptr_t castIdentity = 0;
    std::uint32_t casterNetworkId = 0;
    std::uint32_t missileNetworkId = 0;
    int slot = -1;
    Vec2 observedHead = {};
    int observedTick = 0;
    float observedSpeed = 0.0f;
    float speedOverride = 0.0f;
    float positionUncertainty = 0.0f;
    int delayOverride = -1;
    float collisionEndExplosionRadius = 0.0f;
    int collisionEndExplosionDelay = -1;
    int collisionHitCount = 0;
    int collisionUnitNetworkId = 0;
    int projectileTerminationTick = 0;
    int missileMissingSinceTick = -1;
    int trapObjectId = 0;
    ZDCollisionKind collisionKind = ZDCollisionKind::None;
    std::vector<std::pair<int, float>> pendingUnitCollisions;
    std::vector<int> consumedCollisionUnits;
    bool collisionStopped = false;
    bool projectileTerminated = false;
    bool missingMissileTermination = false;
    bool persistent = false;
    bool missileBound = false;
    bool expired = false;

    bool HasData() const { return data != nullptr; }
    float Radius() const { return data ? std::max(0.0f, data->radius) : 0.0f; }
    float InnerRadius() const {
        return data ? std::clamp(data->innerRadius, 0.0f, Radius()) : 0.0f;
    }
    float Range() const { return data ? std::max(0.0f, data->range) : 0.0f; }
    float Speed() const {
        if (std::isfinite(speedOverride) && speedOverride > 1.0f && speedOverride < 100000.0f)
            return speedOverride;
        if (std::isfinite(observedSpeed) && observedSpeed > 1.0f && observedSpeed < 100000.0f)
            return observedSpeed;
        return data ? data->projectileSpeed : 0.0f;
    }
    float PositionUncertainty() const {
        return std::isfinite(positionUncertainty)
            ? std::clamp(positionUncertainty, 0.0f, 80.0f)
            : 0.0f;
    }
    int Danger() const {
        return std::max(1, dangerOverride > 0
            ? dangerOverride
            : data ? data->dangerlevel : 1);
    }
    ZDSpellType Type() const { return data ? data->spellType : ZDSpellType::Line; }
    bool HasValidGeometry() const {
        return data != nullptr && data->HasValidGeometryFields();
    }
    bool ArcSupported() const {
        return false;
    }
    MissileRouteMode RouteMode() const {
        return data ? data->missileRouteMode : MissileRouteMode::Straight;
    }
    int Delay() const {
        return delayOverride >= 0
            ? delayOverride
            : data ? std::max(0, data->spellDelay) : 0;
    }
    int ExtraEndTime() const { return data ? std::max(0, data->extraEndTime) : 0; }
    float Angle() const { return data ? data->coneAngleDegrees : 0.0f; }
    float ConeEdgePadding() const {
        return data && std::isfinite(data->coneEdgePadding)
            ? std::max(0.0f, data->coneEdgePadding)
            : 0.0f;
    }
    bool HasValidConeAngle() const {
        const float angle = Angle();
        return std::isfinite(angle) && angle > 0.0f && angle <= 360.0f;
    }
    bool DefaultOff() const { return data && data->defaultOff; }
    const std::string& SpellName() const {
        static const std::string empty;
        return data ? data->spellName : empty;
    }
    Vec2 AuthoredEnd() const {
        return authoredEndPos.IsValid() && !authoredEndPos.IsZero()
            ? authoredEndPos
            : endPos;
    }
    int CollisionTargetLimit() const {
        return data ? std::max(1, data->collisionTargetLimit) : 1;
    }
    int EndExplosionDelay() const {
        if (collisionEndExplosionDelay >= 0) return collisionEndExplosionDelay;
        return data ? EffectiveEndExplosionDelay(*data) : 0;
    }
    int EndExplosionDuration() const {
        return data ? EffectiveEndExplosionDuration(*data) : 0;
    }
    float EndExplosionRadius() const {
        if (collisionEndExplosionRadius > 0.0f) return collisionEndExplosionRadius;
        return data ? std::max(0.0f, data->secondaryRadius) : 0.0f;
    }
    bool HasEndExplosionArea() const {
        if (!data || !data->hasEndExplosion || EndExplosionRadius() <= 0.0f) return false;
        if (collisionKind == ZDCollisionKind::ProjectileWall &&
            !data->endExplosionOnProjectileWall) return false;
        const float travel = startPos.Distance(
            data->endExplosionAtUnitCenter && !collisionUnitCenter.IsZero()
                ? collisionUnitCenter
                : endPos);
        if (travel + 0.01f < std::max(0.0f, data->endExplosionMinimumTravelDistance))
            return false;
        if (missingMissileTermination) return true;
        if (data->endExplosionRequiresUnitCollision)
            return collisionStopped && collisionKind == ZDCollisionKind::Unit;
        if (data->endExplosionRequiresCollision)
            return collisionStopped && collisionKind != ZDCollisionKind::None;
        return true;
    }
    Vec2 EndExplosionCenter() const {
        Vec2 center = !collisionExplosionCenter.IsZero()
            ? collisionExplosionCenter
            : data && data->endExplosionAtUnitCenter && !collisionUnitCenter.IsZero()
                ? collisionUnitCenter
                : endPos;
        if (data && data->endExplosionCenterOffset != 0.0f)
            center = center + direction * data->endExplosionCenterOffset;
        return center;
    }
    int ArrivalTickAt(const Vec2& routePoint) const {
        const int baseTick = missileBound && launchTick > 0
            ? launchTick
            : SaturatingTickAdd(startTick, Delay());
        if (Type() == ZDSpellType::Arc) return baseTick;
        if (!HasTravelSpeed()) return baseTick;

        const Vec2 routeEnd =
            Type() == ZDSpellType::Circular && missileBound
                ? AuthoredEnd()
                : endPos;
        Vec2 routeDirection = direction.Normalized();
        if (observedTick > 0 && observedHead.IsValid() && !observedHead.IsZero()) {
            if (RouteMode() == MissileRouteMode::Steering) {
                if (!routeDirection.IsValid() || routeDirection.IsZero())
                    return observedTick;
                const float remainingRoute = std::max(
                    0.0f,
                    (routeEnd - observedHead).Dot(routeDirection));
                const float remainingDistance = std::clamp(
                    (routePoint - observedHead).Dot(routeDirection),
                    0.0f,
                    remainingRoute);
                return SaturatingTickAdd(observedTick, ClampTickOffset(std::ceil(
                    1000.0f * remainingDistance / std::max(1.0f, Speed()))));
            }
            if (routeDirection.IsZero())
                routeDirection = (routeEnd - startPos).Normalized();
            if (routeDirection.IsZero()) return baseTick;
            const float routeDistance = std::max(
                0.0f,
                (routeEnd - startPos).Dot(routeDirection));
            const float targetDistance = std::clamp(
                (routePoint - startPos).Dot(routeDirection),
                0.0f,
                routeDistance);
            const float observedDistance = std::clamp(
                (observedHead - startPos).Dot(routeDirection),
                0.0f,
                routeDistance);
            const float remainingDistance = std::max(
                0.0f,
                targetDistance - observedDistance);
            return SaturatingTickAdd(observedTick, ClampTickOffset(std::ceil(
                1000.0f * remainingDistance / std::max(1.0f, Speed()))));
        }
        if (routeDirection.IsZero())
            routeDirection = (routeEnd - startPos).Normalized();
        if (routeDirection.IsZero()) return baseTick;
        const float routeDistance = std::max(
            0.0f,
            (routeEnd - startPos).Dot(routeDirection));
        const float targetDistance = std::clamp(
            (routePoint - startPos).Dot(routeDirection),
            0.0f,
            routeDistance);
        return SaturatingTickAdd(baseTick, ClampTickOffset(std::ceil(
            1000.0f * targetDistance / std::max(1.0f, Speed()))));
    }
    int ArrivalTick() const {
        return ArrivalTickAt(
            Type() == ZDSpellType::Circular && missileBound
                ? AuthoredEnd()
                : endPos);
    }
    int EndExplosionStartTick() const {
        if (projectileTerminated && projectileTerminationTick > 0)
            return SaturatingTickAdd(projectileTerminationTick, EndExplosionDelay());
        return SaturatingTickAdd(ArrivalTick(), EndExplosionDelay());
    }
    int EndExplosionEndTick() const {
        return SaturatingTickAdd(EndExplosionStartTick(), EndExplosionDuration());
    }
    int MovingLineTerminalActiveEndTick() const {
        const int terminalLinger = std::max(80, ExtraEndTime());
        return std::min(
            SaturatingTickAdd(endTick, 100),
            SaturatingTickAdd(ArrivalTick(), terminalLinger));
    }

    bool HasTravelSpeed() const {
        const float speed = Speed();
        return data && std::isfinite(speed) && speed > 1.0f && speed < 100000.0f;
    }

    Vec2 HeadAtTick(int tick) const {
        if (Type() == ZDSpellType::Arc) return startPos;
        if (projectileTerminated) return endPos;
        const bool movingCircular =
            Type() == ZDSpellType::Circular && missileBound;
        if (Type() != ZDSpellType::Line && !movingCircular) return startPos;
        if (!HasTravelSpeed()) return startPos;
        const Vec2 routeEnd = movingCircular ? AuthoredEnd() : endPos;
        Vec2 routeDirection = direction.Normalized();
        if (observedTick > 0 && observedHead.IsValid() && !observedHead.IsZero()) {
            if (RouteMode() == MissileRouteMode::Steering) {
                if (!routeDirection.IsValid() || routeDirection.IsZero())
                    return observedHead;
                const float remainingDistance = std::max(
                    0.0f,
                    (routeEnd - observedHead).Dot(routeDirection));
                if (remainingDistance <= 0.0f) return routeEnd;
                const float elapsedSeconds = static_cast<float>(
                    std::max<std::int64_t>(0, TickDifference(tick, observedTick))) / 1000.0f;
                const float distance = std::clamp(
                    elapsedSeconds * Speed(),
                    0.0f,
                    remainingDistance);
                if (distance >= remainingDistance) return routeEnd;
                return observedHead + routeDirection * distance;
            }
            if (routeDirection.IsZero())
                routeDirection = (routeEnd - startPos).Normalized();
            if (routeDirection.IsZero()) return startPos;
            const float totalDistance = std::max(
                0.0f,
                (routeEnd - startPos).Dot(routeDirection));
            const float observedDistance = std::clamp(
                (observedHead - startPos).Dot(routeDirection),
                0.0f,
                totalDistance);
            const float elapsedSeconds = static_cast<float>(
                std::max<std::int64_t>(0, TickDifference(tick, observedTick))) / 1000.0f;
            const float distance = std::clamp(
                observedDistance + elapsedSeconds * Speed(),
                0.0f,
                totalDistance);
            return startPos + routeDirection * distance;
        }
        if (routeDirection.IsZero())
            routeDirection = (routeEnd - startPos).Normalized();
        if (routeDirection.IsZero()) return startPos;
        const float totalDistance = std::max(
            0.0f,
            (routeEnd - startPos).Dot(routeDirection));
        const int baseTick = missileBound && launchTick > 0
            ? launchTick
            : SaturatingTickAdd(startTick, Delay());
        const float elapsedSeconds = static_cast<float>(
            std::max<std::int64_t>(0, TickDifference(tick, baseTick))) / 1000.0f;
        const float distance = std::clamp(elapsedSeconds * Speed(), 0.0f, totalDistance);
        return startPos + routeDirection * distance;
    }

    bool IsExpiredAt(int tick) const {
        if (expired) return true;
        if (persistent) return false;
        const int expiryTick = SaturatingTickAdd(endTick, 250);
        return TickAfter(tick, expiryTick) ||
            (expiryTick == std::numeric_limits<int>::max() &&
             tick == std::numeric_limits<int>::max());
    }
};

}
