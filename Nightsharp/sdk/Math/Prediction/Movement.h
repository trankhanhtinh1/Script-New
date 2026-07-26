#pragma once

// ============================================================================
// Movement.h - Movement prediction ported 1:1 from EnsoulSharp.SDK
// ----------------------------------------------------------------------------
// Source: EnsoulSharp.SDK/Core/Math/Prediction/Movement.cs
// GamePath.cs lives in Prediction/GamePath.h. PredictionInput /
// PredictionOutput are still defined at the bottom of this header to match
// their original placement in Movement.cs.
//
// Helper math (Polar, Rotated, AngleBetween, Perpendicular, ProjectOn,
// Intersection, VectorMovementCollision, CutPath, PathLength, SetZ) is
// ported from Vector2Extensions.cs / Vector3Extensions.cs.
// ============================================================================

#include "../../Core/Objects.h"
#include "../../Core/Game.h"
#include "../../Enumerations/HitChance.h"
#include "../../Enumerations/SpellType.h"
#include "../../Enumerations/CollisionableObjects.h"
#include "../../Events/Dash.h"
#include "../../Extensions/Unit.h"
#include "../../Utils/MathUtils.h"
#include "../../GameObjects/GameObjects.h"
#include "../../GameObjects/ObjectManager.h"
#include "GamePath.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>
#include <string>

// ============================================================================
// PredictionInput / PredictionOutput are defined in SDK namespace
// (matching EnsoulSharp.SDK namespace in the original C# source).
// They are forward-declared here and defined after the helper namespaces.
// ============================================================================
namespace SDK {
class Spell;
struct PredictionInput;
struct PredictionOutput;
} // namespace SDK

// Forward declaration — defined in Cluster.h (included via Prediction.h)
namespace SDK::Prediction::Cluster {
    PredictionOutput GetAoEPrediction(const PredictionInput& input);
}

namespace SDK::Collision {
    std::vector<AIBaseClient> GetCollision(const std::vector<Vector3>& positions, PredictionInput input);
}

namespace SDK::Prediction {

// ============================================================================
// BuffType constants (from EnsoulSharp.BuffType enum)
// Used by UnitIsImmobileUntil to check CC buffs.
// ============================================================================
namespace BuffType {
    constexpr int Charm        = 23;
    constexpr int Knockup      = 30;
    constexpr int Stun         = 5;
    constexpr int Suppression  = 25;
    constexpr int Snare        = 12;
    constexpr int Fear         = 22;
    constexpr int Taunt        = 8;
    constexpr int Knockback    = 31;
    constexpr int Asleep       = 35;
} // namespace BuffType

// ============================================================================
// Vector2 helper extensions (ported from Vector2Extensions.cs)
// ============================================================================
namespace Vec2Ext {

inline float Polar(const Vec2& v) {
    if (std::abs(v.x) < 0.0001f) {
        return v.y > 0 ? 90.0f : (v.y < 0 ? 270.0f : 0.0f);
    }
    float theta = std::atan2(v.y, v.x) * 180.0f / 3.14159265358979323846f;
    if (theta < 0) theta += 360.0f;
    return theta;
}

inline float AngleBetween(const Vec2& v1, const Vec2& v2) {
    float theta = Polar(v1) - Polar(v2);
    if (theta < 0) theta += 360.0f;
    if (theta > 180) theta = 360.0f - theta;
    return theta;
}

inline Vec2 Rotated(const Vec2& v, float angle) {
    float cos = std::cos(angle);
    float sin = std::sin(angle);
    return Vec2(
        static_cast<float>(v.x * cos - v.y * sin),
        static_cast<float>(v.y * cos + v.x * sin));
}

inline Vec2 Perpendicular(const Vec2& v, int offset = 0) {
    float c = std::cos(static_cast<float>(offset) * 3.14159265358979323846f / 2.0f);
    float s = std::sin(static_cast<float>(offset) * 3.14159265358979323846f / 2.0f);
    return Vec2(-v.x * s + v.y * c, v.x * c + v.y * s);
}

// ProjectionInfo (from Vector2Extensions.cs)
struct ProjectionInfo {
    bool IsOnSegment = false;
    Vec2 LinePoint = {};
    Vec2 SegmentPoint = {};

    ProjectionInfo() = default;
    ProjectionInfo(bool isOnSegment, const Vec2& segmentPoint, const Vec2& linePoint)
        : IsOnSegment(isOnSegment), LinePoint(linePoint), SegmentPoint(segmentPoint) {}
};

inline ProjectionInfo ProjectOn(const Vec2& point, const Vec2& segmentStart, const Vec2& segmentEnd) {
    const Vec2 segment = segmentEnd - segmentStart;
    const float lengthSqr = segment.LengthSqr();
    if (lengthSqr < 0.0001f) {
        return { false, segmentStart, segmentStart };
    }

    const float projection = (point - segmentStart).Dot(segment) / lengthSqr;
    const Vec2 linePoint = segmentStart + segment * projection;
    const float segmentProjection = std::clamp(projection, 0.0f, 1.0f);
    const bool isOnSegment = segmentProjection == projection;
    const Vec2 segmentPoint = isOnSegment
        ? linePoint
        : segmentStart + segment * segmentProjection;
    return { isOnSegment, segmentPoint, linePoint };
}

// IntersectionResult (from Vector2Extensions.cs)
struct IntersectionResult {
    bool Valid = false;
    Vec2 Point = {};

    IntersectionResult() = default;
    IntersectionResult(bool valid, const Vec2& point) : Valid(valid), Point(point) {}
};

inline IntersectionResult Intersection(const Vec2& lineSegmentStart, const Vec2& lineSegmentEnd,
                                        const Vec2& line2SegmentStart, const Vec2& line2SegmentEnd) {
    double deltaAC_y = lineSegmentStart.y - line2SegmentStart.y;
    double deltaDC_x = lineSegmentEnd.x - line2SegmentStart.x;
    double deltaBA_x = -lineSegmentEnd.x + lineSegmentStart.x;

    double denominator = deltaBA_x * (line2SegmentEnd.y - line2SegmentStart.y)
                         - (lineSegmentStart.y - lineSegmentEnd.y) * (line2SegmentEnd.x - line2SegmentStart.x);

    if (std::abs(denominator) < 0.0001) {
        return { false, {} };
    }

    double deltaCB_x = -line2SegmentEnd.x + line2SegmentStart.x;
    double deltaAC_x = -lineSegmentStart.x + line2SegmentStart.x;

    double s = (deltaAC_y * deltaCB_x - deltaAC_x * (line2SegmentEnd.y - line2SegmentStart.y)) / denominator;
    double t = (deltaAC_y * deltaDC_x - deltaAC_x * (lineSegmentStart.y - lineSegmentEnd.y)) / denominator;

    if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
        return { true, Vec2(
            static_cast<float>(lineSegmentStart.x + s * (lineSegmentEnd.x - lineSegmentStart.x)),
            static_cast<float>(lineSegmentStart.y + s * (lineSegmentEnd.y - lineSegmentStart.y))) };
    }

    return { false, {} };
}

// MovementCollisionInfo (from Vector2Extensions.cs)
struct MovementCollisionInfo {
    Vec2 CollisionPosition = {};
    float CollisionTime = 0.0f;

    MovementCollisionInfo() = default;
    MovementCollisionInfo(float collisionTime, const Vec2& collisionPosition)
        : CollisionPosition(collisionPosition), CollisionTime(collisionTime) {}
};

inline MovementCollisionInfo VectorMovementCollision(
    const Vec2& pointStartA, const Vec2& pointEndA, float pointVelocityA,
    const Vec2& pointB, float pointVelocityB, float delay = 0.0f) {
    const float x = pointStartA.x;
    const float y = pointStartA.y;
    const float x2 = pointEndA.x;
    const float y2 = pointEndA.y;
    const float x3 = pointB.x;
    const float y3 = pointB.y;

    const float dx = x2 - x;
    const float dy = y2 - y;
    const float distanceA = std::sqrt(dx * dx + dy * dy);
    float collisionTime = std::numeric_limits<float>::quiet_NaN();

    const float velocityAx = std::abs(distanceA) > FLT_EPSILON
        ? pointVelocityA * dx / distanceA
        : 0.0f;
    const float velocityAy = std::abs(distanceA) > FLT_EPSILON
        ? pointVelocityA * dy / distanceA
        : 0.0f;

    const float relX = x3 - x;
    const float relY = y3 - y;
    const float relDistanceSqr = relX * relX + relY * relY;

    if (distanceA > 0.0f) {
        if (std::abs(pointVelocityA - FLT_MAX) < FLT_EPSILON) {
            const float time = distanceA / pointVelocityA;
            collisionTime = pointVelocityB * time >= 0.0f
                ? time
                : std::numeric_limits<float>::quiet_NaN();
        } else if (std::abs(pointVelocityB - FLT_MAX) < FLT_EPSILON) {
            collisionTime = 0.0f;
        } else {
            const float a = velocityAx * velocityAx + velocityAy * velocityAy
                - pointVelocityB * pointVelocityB;
            const float b = -relX * velocityAx - relY * velocityAy;

            if (std::abs(a) < FLT_EPSILON) {
                if (std::abs(b) < FLT_EPSILON) {
                    collisionTime = std::abs(relDistanceSqr) < FLT_EPSILON
                        ? 0.0f
                        : std::numeric_limits<float>::quiet_NaN();
                } else {
                    const float time = -relDistanceSqr / (2.0f * b);
                    collisionTime = pointVelocityB * time >= 0.0f
                        ? time
                        : std::numeric_limits<float>::quiet_NaN();
                }
            } else {
                const float discriminant = b * b - a * relDistanceSqr;
                if (discriminant >= 0.0f) {
                    const float sqrtDiscriminant = std::sqrt(discriminant);
                    const float time1 = (-sqrtDiscriminant - b) / a;
                    const float candidate1 = pointVelocityB * time1 >= 0.0f
                        ? time1
                        : std::numeric_limits<float>::quiet_NaN();
                    const float time2 = (sqrtDiscriminant - b) / a;
                    const float candidate2 = pointVelocityB * time2 >= 0.0f
                        ? time2
                        : std::numeric_limits<float>::quiet_NaN();

                    collisionTime = candidate1;
                    if (!std::isnan(candidate2) && !std::isnan(collisionTime)) {
                        if (collisionTime >= delay && candidate2 >= delay) {
                            collisionTime = std::min(collisionTime, candidate2);
                        } else if (candidate2 >= delay) {
                            collisionTime = candidate2;
                        }
                    }
                }
            }
        }
    } else if (std::abs(distanceA) < FLT_EPSILON) {
        collisionTime = 0.0f;
    }

    const Vec2 collisionPosition = !std::isnan(collisionTime)
        ? Vec2(x + velocityAx * collisionTime, y + velocityAy * collisionTime)
        : Vec2();
    return { collisionTime, collisionPosition };
}

} // namespace Vec2Ext

// ============================================================================
// Vector3 helper extensions (ported from Vector3Extensions.cs)
// ============================================================================
namespace Vec3Ext {

inline Vec3 SetZ(const Vec3& v, float value = -1.0f) {
    Vec3 result = v;
    if (value < 0) {
        result.y = SDK::Game::CursorPosRaw().y;
    } else {
        result.y = value;
    }
    return result;
}

inline Vec3 Rotated(const Vec3& v, float angle) {
    float cos = std::cos(angle);
    float sin = std::sin(angle);
    return Vec3(
        static_cast<float>(v.x * cos - v.z * sin),
        v.y,
        static_cast<float>(v.z * cos + v.x * sin));
}

} // namespace Vec3Ext

} // namespace SDK::Prediction

// ============================================================================
// PredictionInput (ported from Movement.cs lines 508-621)
// Defined in SDK namespace to match EnsoulSharp.SDK namespace.
// ============================================================================
namespace SDK {
struct PredictionInput {
    AIBaseClient Unit = GameObjects::Player();
    float Delay = 0.0f;
    float Radius = 1.0f;
    float Speed = FLT_MAX;
    Vector3 From = {};
    float Range = FLT_MAX;
    bool Collision = false;
    SpellType Type = SpellType::None;
    const class Spell* Spell = nullptr;
    Vector3 RangeCheckFrom = {};
    bool AoE = false;
    CollisionObjectsBridge CollisionObjects;
    bool AddHitBox = true;
    float MaxCollisionCount = 0.0f;
    bool ChoiceCloserPosition = false;

    void SetType(SpellType type) {
        Type = type;
    }

    void SetType(SkillshotType type) {
        Type = ToSpellType(type);
    }

    void SetCollisionObjects(CollisionableObjects flags) {
        CollisionObjects = flags;
    }

    void SetCollisionObjects(const std::vector<CollisionableObjects>& objects) {
        CollisionObjects = objects;
    }

    CollisionableObjects CollisionObjectFlags() const {
        return CollisionObjects.ToFlags();
    }

    const std::vector<CollisionableObjects>& CollisionObjectArray() const {
        return CollisionObjects.ToArray();
    }

    Vector3 ResolveFrom() const {
        return From.IsValid() && !From.IsZero()
            ? From
            : PlayerServerPosition();
    }

    Vector3 ResolveRangeCheckFrom() const {
        if (RangeCheckFrom.IsValid() && !RangeCheckFrom.IsZero()) {
            return RangeCheckFrom;
        }
        const auto from = ResolveFrom();
        return from.To2D().IsValid() && !from.To2D().IsZero()
            ? from
            : PlayerServerPosition();
    }

    float RealRadius() const {
        return AddHitBox ? Radius + Unit.BoundingRadius() : Radius;
    }

private:
    static Vector3 ServerPositionOrPosition(const AIBaseClient& unit) {
        if (!unit.IsValid()) {
            return Vector3();
        }
        const auto serverPosition = unit.ServerPosition();
        return serverPosition.IsValid() && !serverPosition.IsZero()
            ? serverPosition
            : unit.Position();
    }

    static Vector3 PlayerServerPosition() {
        const auto player = GameObjects::Player();
        return ServerPositionOrPosition(player);
    }
};

// ============================================================================
// PredictionOutput (ported from Movement.cs lines 626-712)
// Defined in SDK namespace to match EnsoulSharp.SDK namespace.
// ============================================================================
struct PredictionOutput {
private:
    Vector3 _castPosition = {};
    Vector3 _unitPosition = {};
    HitChance _originHitchance = HitChance::None;

public:
    HitChance Hitchance = HitChance::None;
    int AoeTargetsHitCount = 0;
    std::vector<AIHeroClient> AoeTargetsHit;
    std::vector<AIBaseClient> CollisionObjects;
    PredictionInput Input;
    std::string Idle;

    // CastPosition property (DLL: getter with fallback, setter is plain assignment)
    // SetZ is applied at call sites via ToVector3().SetZ()
    Vector3 GetCastPosition() const {
        if (_castPosition.IsValid() && _castPosition.To2D().IsValid()) {
            return _castPosition;
        }
        if (Input.Unit.IsValid()) {
            const auto serverPosition = Input.Unit.ServerPosition();
            return serverPosition.IsValid() && !serverPosition.IsZero()
                ? serverPosition
                : Input.Unit.Position();
        }
        return Vector3();
    }

    void SetCastPosition(const Vector3& v) {
        _castPosition = v;
    }

    // UnitPosition property (DLL: getter with fallback, setter is plain assignment)
    Vector3 GetUnitPosition() const {
        if (_unitPosition.IsValid() && _unitPosition.To2D().IsValid()) {
            return _unitPosition;
        }
        if (Input.Unit.IsValid()) {
            const auto serverPosition = Input.Unit.ServerPosition();
            return serverPosition.IsValid() && !serverPosition.IsZero()
                ? serverPosition
                : Input.Unit.Position();
        }
        return Vector3();
    }

    void SetUnitPosition(const Vector3& v) {
        _unitPosition = v;
    }

    // OriginHitchance property (DLL: returns _originHitchance if Hitchance==Collision, else Hitchance)
    HitChance GetOriginHitchance() const {
        return (Hitchance == HitChance::Collision) ? _originHitchance : Hitchance;
    }

    void SetOriginHitchance(HitChance h) {
        _originHitchance = h;
    }

    // AoeTargetsHitCount computed property (DLL: Max(_aoeTargetsHitCount, AoeTargetsHit.Count))
    int GetAoeTargetsHitCount() const {
        return std::max(AoeTargetsHitCount, static_cast<int>(AoeTargetsHit.size()));
    }
};
} // namespace SDK

namespace SDK::Prediction {

// ============================================================================
// IPrediction — C++ contract equivalent to EnsoulSharp.SDK.IPrediction.
// ============================================================================
class IPrediction {
public:
    virtual ~IPrediction() = default;

    virtual PredictionOutput GetPrediction(PredictionInput input) = 0;
    virtual PredictionOutput GetPrediction(PredictionInput input,
                                           bool ft,
                                           bool checkCollision) = 0;
};

// ============================================================================
// Movement class (ported from Movement.cs lines 32-499)
// ============================================================================
namespace Movement {

// ----------------------------------------------------------------------------
// PositionAfter (Movement.cs lines 457-479)
// Calculates the unit position after "t" seconds.
// ----------------------------------------------------------------------------
inline Vec2 PositionAfter(const AIBaseClient& unit, float t, float speed = FLT_MAX) {
    float distance = t * speed;
    auto waypoints3D = unit.GetWaypoints();
    std::vector<Vec2> path;
    path.reserve(waypoints3D.size());
    for (const auto& wp : waypoints3D) {
        path.push_back(wp.To2D());
    }

    if (path.empty()) {
        return {};
    }

    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        const Vec2& a = path[i];
        const Vec2& b = path[i + 1];
        float d = a.Distance(b);

        if (d < distance) {
            distance -= d;
        } else {
            return a + ((b - a).Normalized() * distance);
        }
    }

    return path.back();
}

// ----------------------------------------------------------------------------
// GetHitchance (DLL: PredictionSDK.GetHitchance)
// Returns VeryHigh if hero's current path time < 0.1, else High.
// Non-heroes always return VeryHigh.
// ----------------------------------------------------------------------------
inline HitChance GetHitchance(const AIBaseClient& unit) {
    if (unit.IsHero()) {
        if (SDK::GamePath::PathTracker::GetCurrentPath(unit).Time() < 0.1) {
            return HitChance::VeryHigh;
        }
        return HitChance::High;
    }
    return HitChance::VeryHigh;
}

inline float DistancePointSegmentSqr(const Vec2& point,
                                     const Vec2& segmentStart,
                                     const Vec2& segmentEnd,
                                     bool onlyIfOnSegment = true) {
    const auto projection = Vec2Ext::ProjectOn(point, segmentStart, segmentEnd);
    if (onlyIfOnSegment && !projection.IsOnSegment) {
        return FLT_MAX;
    }
    return point.DistanceSqr(onlyIfOnSegment
        ? projection.SegmentPoint
        : projection.LinePoint);
}

// ----------------------------------------------------------------------------
// GetPositionOnPath (Movement.cs lines 213-319)
// ----------------------------------------------------------------------------
inline PredictionOutput GetPositionOnPath(PredictionInput& input,
                                          std::vector<Vec2> path,
                                          float speed = -1.0f,
                                          bool needToFixSpeed = false) {
    Vector3 serverPos = input.Unit.ServerPosition();
    if (!serverPos.IsValid() || serverPos.IsZero()) {
        serverPos = input.Unit.Position();
    }

    if (needToFixSpeed &&
        serverPos.To2D().DistanceSquared(input.ResolveFrom().To2D()) < 250.0f * 250.0f) {
        speed *= 1.5f;
    }

    speed = (std::abs(speed - (-1.0f)) < 0.0001f) ? input.Unit.MoveSpeed() : speed;

    if (path.size() <= 1 || (input.Unit.Spellbook().IsWindingUp() && !SDK::Extensions::IsDashing(input.Unit))) {
        PredictionOutput output;
        output.Input = input;
        output.SetUnitPosition(serverPos);
        output.SetCastPosition(serverPos);
        output.Hitchance = HitChance::VeryHigh;
        return output;
    }

    float pLength = SDK::Utils::MathUtils::PathLength(path);

    const auto stationaryOutput = [&]() {
        PredictionOutput output;
        output.Input = input;
        output.SetUnitPosition(serverPos);
        output.SetCastPosition(serverPos);
        output.Hitchance = HitChance::VeryHigh;
        return output;
    };

    // Short path / stationary check. EnsoulSharp special-cases practice
    // dummies/minions/jungle for path.Count == 2 && pathLength < 5. NightSharp
    // can miss the dummy CharacterName when the object existed before injection,
    // so also trust the movement/path state here.
    bool isJungle = false;
    if (input.Unit.IsMinion()) {
        AIMinionClient minion(input.Unit.Address());
        isJungle = minion.IsJungle();
    }
    std::string characterName = input.Unit.CharacterName();
    if ((path.size() == 2 && pLength < 5.0f) &&
        (characterName == "PracticeTool_TargetDummy" ||
         input.Unit.IsMinion() ||
         isJungle ||
         !input.Unit.IsMoving())) {
        return stationaryOutput();
    }
    if (pLength < 1.0f || !input.Unit.IsMoving()) {
        return stationaryOutput();
    }

    // Skillshots with only a delay (speed == MaxValue)
    if (pLength >= (input.Delay * speed) - input.RealRadius()
        && std::abs(input.Speed - FLT_MAX) < 0.0001f) {
        float tDistance = (input.Delay * speed) - input.RealRadius();

        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            Vec2 a = path[i];
            Vec2 b = path[i + 1];
            float d = a.Distance(b);

            if (d >= tDistance) {
                Vec2 direction = (b - a).Normalized();

                Vec2 cp = a + (direction * tDistance);
                float pDist = (i == path.size() - 2)
                    ? std::min(tDistance + input.RealRadius(), d)
                    : (tDistance + input.RealRadius());
                Vec2 p = a + (direction * pDist);

                PredictionOutput output;
                output.Input = input;
                output.SetCastPosition(Vec3Ext::SetZ(Vec3::From2D(cp)));
                output.SetUnitPosition(Vec3Ext::SetZ(Vec3::From2D(p)));
                output.Hitchance = GetHitchance(input.Unit);
                return output;
            }

            tDistance -= d;
        }
    }

    // Skillshot with a delay and speed (speed != MaxValue)
    if (pLength >= (input.Delay * speed) - input.RealRadius()
        && std::abs(input.Speed - FLT_MAX) > 0.0001f) {
        float distance = (input.Delay * speed) - input.RealRadius();
        // DLL: for Line/Cone, if unit is close to From (<200), don't subtract RealRadius
        if ((IsLineSpellType(input.Type) || IsConeSpellType(input.Type))
            && input.ResolveFrom().To2D().DistanceSquared(serverPos.To2D()) < 200.0f * 200.0f) {
            distance = input.Delay * speed;
        }
        path = SDK::Utils::MathUtils::CutPath(path, std::max(0.0f, distance));
        float tT = 0.0f;

        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            Vec2 a = path[i];
            Vec2 b = path[i + 1];
            float tB = a.Distance(b) / speed;
            Vec2 direction = (b - a).Normalized();
            a = a - (direction * (speed * tT));

            auto sol = Vec2Ext::VectorMovementCollision(
                a, b, speed, input.ResolveFrom().To2D(), input.Speed, tT);
            float t = sol.CollisionTime;
            Vec2 pos = sol.CollisionPosition;

            if (pos.IsValid() && t >= tT && t <= tT + tB) {
                if (pos.DistanceSquared(b) < 20.0f) {
                    break;
                }
                Vec2 p = pos - (direction * input.RealRadius());

                PredictionOutput output;
                output.Input = input;
                output.SetCastPosition(Vec3Ext::SetZ(Vec3::From2D(pos)));
                output.SetUnitPosition(Vec3Ext::SetZ(Vec3::From2D(p)));
                output.Hitchance = GetHitchance(input.Unit);
                return output;
            }

            tT += tB;
        }
    }

    Vec2 position = path.back();
    PredictionOutput output;
    output.Input = input;
    output.SetCastPosition(Vec3Ext::SetZ(Vec3::From2D(position)));
    output.SetUnitPosition(Vec3Ext::SetZ(Vec3::From2D(position)));
    output.Hitchance = HitChance::Medium;
    return output;
}

// ----------------------------------------------------------------------------
// GetAdvancedPrediction (Movement.cs lines 102-121)
// ----------------------------------------------------------------------------
inline PredictionOutput GetAdvancedPrediction(PredictionInput& input, float additionalSpeed = 0.0f) {
    float speed = std::abs(additionalSpeed) < 0.0001f ? input.Speed : input.Speed * additionalSpeed;

    if (std::abs(speed - static_cast<float>(INT_MAX)) < 0.0001f) {
        speed = 90000.0f;
    }

    AIBaseClient unit = input.Unit;
    Vec2 position = PositionAfter(unit, 1.0f, unit.MoveSpeed() - 100.0f);

    // C# source: prediction = position + (speed * (input.Delay / 1000))
    // The scalar is added in the direction of the unit's movement.
    // Derive direction from the unit's current waypoints.
    Vec2 moveDir(0, 0);
    auto waypoints3D = unit.GetWaypoints();
    if (waypoints3D.size() >= 2) {
        Vec2 a = waypoints3D[0].To2D();
        Vec2 b = waypoints3D[1].To2D();
        Vec2 diff = b - a;
        if (diff.LengthSqr() > 0.001f) {
            moveDir = diff.Normalized();
        }
    }

    Vec2 prediction = position + moveDir * (speed * (input.Delay / 1000.0f));

    PredictionOutput output;
    output.Input = input;
    output.SetUnitPosition(Vec3Ext::SetZ(Vec3::From2D(position)));
    output.SetCastPosition(Vec3Ext::SetZ(Vec3::From2D(prediction)));
    output.Hitchance = HitChance::High;
    return output;
}

// ----------------------------------------------------------------------------
// GetDashingPrediction (Movement.cs lines 130-174)
// ----------------------------------------------------------------------------
inline PredictionOutput GetDashingPrediction(PredictionInput& input) {
    PredictionOutput result;
    result.Input = input;

    auto dashData = SDK::Extensions::GetDashInfo(input.Unit);
    input.Delay += 0.1f;

    Vec3 serverPosition = input.Unit.ServerPosition();
    if (!serverPosition.IsValid() || serverPosition.IsZero()) {
        serverPosition = input.Unit.Position();
    }

    Vec2 startPos = dashData.StartPos.IsValid() && !dashData.StartPos.IsZero()
        ? dashData.StartPos.To2D()
        : serverPosition.To2D();
    Vec2 endPos = dashData.PathCount > 0
        ? dashData.Path[dashData.PathCount - 1].To2D()
        : (dashData.EndPos.IsValid() ? dashData.EndPos.To2D() : startPos);

    std::vector<Vec2> dashPath = { startPos, endPos };
    auto dashPred = GetPositionOnPath(input, dashPath, dashData.Speed);
    if (dashPred.Hitchance >= HitChance::High &&
        DistancePointSegmentSqr(
            dashPred.GetUnitPosition().To2D(),
            serverPosition.To2D(),
            endPos) < 200.0f * 200.0f) {
        dashPred.SetCastPosition(dashPred.GetUnitPosition());
        dashPred.Hitchance = HitChance::Dash;
        return dashPred;
    }

    if (dashData.StartPos.Distance2D(dashData.EndPos) > 200.0f &&
        dashData.Speed > 1.0f) {
        const float timeToPoint =
            (input.Delay * 0.5f) +
            (input.ResolveFrom().To2D().Distance(endPos) / input.Speed) -
            0.25f;
        const float timeForUnit =
            (input.Unit.Distance(Vec3::From2D(endPos)) / dashData.Speed) +
            (input.RealRadius() / input.Unit.MoveSpeed());

        if (timeToPoint <= timeForUnit) {
            PredictionOutput output;
            output.Input = input;
            output.SetCastPosition(Vec3::From2D(endPos));
            output.SetUnitPosition(Vec3::From2D(endPos));
            output.Hitchance = HitChance::Dash;
            return output;
        }
    }

    result.SetCastPosition(Vec3::From2D(endPos));
    result.SetUnitPosition(Vec3::From2D(endPos));
    return result;
}

// ----------------------------------------------------------------------------
// GetImmobilePrediction (Movement.cs lines 184-202)
// ----------------------------------------------------------------------------
inline PredictionOutput GetImmobilePrediction(PredictionInput& input, double remainingImmobileT) {
    Vector3 serverPos = input.Unit.ServerPosition();
    if (!serverPos.IsValid() || serverPos.IsZero()) {
        serverPos = input.Unit.Position();
    }
    float timeToReach = input.Delay + (input.Unit.Distance(input.ResolveFrom()) / input.Speed);

    if ((double)timeToReach <= remainingImmobileT + (double)(input.RealRadius() / input.Unit.MoveSpeed())) {
        PredictionOutput output;
        output.Input = input;
        output.SetCastPosition(serverPos);
        output.SetUnitPosition(serverPos);
        output.Hitchance = HitChance::Immobile;
        return output;
    }

    if ((double)(timeToReach - 0.2f) <=
        remainingImmobileT + (double)(input.RealRadius() / input.Unit.MoveSpeed())) {
        PredictionOutput output;
        output.Input = input;
        output.SetCastPosition(serverPos);
        output.SetUnitPosition(serverPos);
        output.Hitchance = HitChance::VeryHigh;
        return output;
    }

    PredictionOutput output;
    output.Input = input;
    output.SetCastPosition(serverPos);
    output.SetUnitPosition(serverPos);
    output.Hitchance = HitChance::Medium;
    return output;
}

// ----------------------------------------------------------------------------
// UnitIsImmobileUntil (Movement.cs lines 486-496)
// ----------------------------------------------------------------------------
inline double UnitIsImmobileUntil(const AIBaseClient& unit) {
    // DLL checks 9 buff types: Charm, Knockup, Stun, Suppression, Snare,
    // Fear, Taunt, Knockback, Asleep
    float gameTime = SDK::Game::Time();
    double maxEndTime = 0.0;

    uintptr_t buffs[256] = {};
    const int count = CoreBuffs::Enumerate(unit.Address(), buffs, 256);
    for (int i = 0; i < count; ++i) {
        CoreBuffs::BuffRef buff{ buffs[i] };
        if (!buff.IsActive(gameTime)) continue;

        int type = buff.GetType();
        if (type == BuffType::Charm || type == BuffType::Knockup
            || type == BuffType::Stun || type == BuffType::Suppression
            || type == BuffType::Snare || type == BuffType::Fear
            || type == BuffType::Taunt || type == BuffType::Knockback
            || type == BuffType::Asleep) {
            float endTime = buff.GetEndTime();
            if (gameTime <= endTime && endTime > maxEndTime) {
                maxEndTime = endTime;
            }
        }
    }

    return maxEndTime - gameTime;
}

// ----------------------------------------------------------------------------
// GetStandardPrediction (Movement.cs lines 431-448)
// ----------------------------------------------------------------------------
inline PredictionOutput GetStandardPrediction(PredictionInput& input) {
    auto waypoints3D = input.Unit.GetWaypoints();
    std::vector<Vec2> path;
    path.reserve(waypoints3D.size());
    for (const auto& wp : waypoints3D) {
        path.push_back(wp.To2D());
    }

    auto result = GetPositionOnPath(input, path, input.Unit.MoveSpeed(), true);

    // C# has an empty if block: if (result.Hitchance >= HitChance.High && input.Unit is AIHeroClient) {}
    // This is a no-op, so we skip it.

    return result;
}

// ----------------------------------------------------------------------------
// GetPrediction(PredictionInput, bool ft, bool checkCollision)
// (DLL: PredictionSDK.GetPrediction)
// ----------------------------------------------------------------------------
inline bool IsPredictionTargetUsable(const AIBaseClient& unit) {
    if (SDK::Extensions::IsValidTarget(unit, FLT_MAX, false)) {
        return true;
    }

    // NightSharp object flags can currently report practice/custom targets as
    // !IsVisible and IsInvulnerable even though they are targetable, rendered,
    // have valid HP/position/path, and native CastSpell can hit them. Do not
    // let those two unstable flags make Math prediction return an empty
    // output before it reaches the actual movement/path calculation.
    if (!unit.IsValid() || (unit.IsDead() && !unit.IsZombie()) ||
        !unit.IsTargetable()) {
        return false;
    }

    if (unit.IsHero() && !unit.IsVisible()) {
        return false;
    }

    const Vector3 position = unit.Position();
    if (!position.IsValid() || position.IsZero()) {
        return false;
    }

    return true;
}

inline PredictionOutput GetPrediction(PredictionInput input, bool ft, bool checkCollision) {
    PredictionOutput result;

    if (!IsPredictionTargetUsable(input.Unit)) {
        PredictionOutput empty;
        empty.Input = input;
        return empty;
    }

    // DLL: Yuumi attached to ally check
    if (input.Unit.IsHero() && input.Unit.CharacterName() == "Yuumi") {
        for (const auto& hero : SDK::GameObjects::Heroes()) {
            if (SDK::Extensions::IsValidTarget(hero, FLT_MAX, false)
                && hero.Team() == input.Unit.Team()
                && hero.HasBuff("YuumiWAlly")
                && hero.Distance(input.Unit) <= 50.0f) {
                PredictionOutput output;
                output.Input = input;
                return output;
            }
        }
    }

    if (ft) {
        input.Delay += (static_cast<float>(SDK::Game::Ping()) / 2000.0f) + 0.06f;

        if (input.AoE) {
            // Route to Cluster::GetAoEPrediction for AoE skillshots
            // (Circle, Cone, Line). This matches C# pattern:
            //   if (input.AoE) return Cluster.GetAoEPrediction(input);
            return Prediction::Cluster::GetAoEPrediction(input);
        }
    }

    Vector3 unitServerPosition = input.Unit.ServerPosition();
    if (!unitServerPosition.IsValid() || unitServerPosition.IsZero()) {
        unitServerPosition = input.Unit.Position();
    }

    // Target too far away (DLL: RangeCheckFrom + unit server position)
    if (std::abs(input.Range - FLT_MAX) > 0.0001f
        && unitServerPosition.DistanceSqr2D(input.ResolveRangeCheckFrom())
        > std::pow(input.Range * 1.5f, 2)) {
        PredictionOutput output;
        output.Input = input;
        return output;
    }

    bool hasResult = false;
    if (SDK::Extensions::IsDashing(input.Unit)) {
        result = GetDashingPrediction(input);
        hasResult = true;
    } else {
        double remainingImmobileT = UnitIsImmobileUntil(input.Unit);
        if (remainingImmobileT >= 0.0) {
            result = GetImmobilePrediction(input, remainingImmobileT);
            hasResult = true;
        }
    }

    if (!hasResult) {
        result = GetStandardPrediction(input);
    }

    // Range checks + cast position clamping (C# lines 381-408)
    if (std::abs(input.Range - FLT_MAX) > 0.0001f) {
        if (result.Hitchance >= HitChance::High
            && input.ResolveRangeCheckFrom().DistanceSqr2D(unitServerPosition)
            > std::pow(input.Range + (input.RealRadius() * 3.0f / 4.0f), 2)) {
            result.Hitchance = HitChance::Medium;
        }

        if (input.ResolveRangeCheckFrom().DistanceSqr2D(result.GetUnitPosition())
            > std::pow(input.Range + (IsCircleSpellType(input.Type)
                ? input.RealRadius() : 0.0f), 2)) {
            result.Hitchance = HitChance::OutOfRange;
        }

    }

    // Collision check: EnsoulSharp checks only CastPosition here and respects
    // MaxCollisionCount. Broader collision probes belong in Collisions callers.
    //
    // input.Collision stays the single gate. Forcing the projectile-wall group to
    // run whenever it is false was tried and reverted: in this codebase champions
    // pass collision=false to mean "ground-targeted / cannot be intercepted", not
    // "projectile that minions ignore", so it wall-gated Tristana W's self-dash,
    // Aatrox Q's ground sweep, Xerath W/R artillery, Cassiopeia Q/W/R, Viktor W/R,
    // Thresh E, Caitlyn W and Kog'Maw R — none of which a Wind Wall can stop.
    //
    // A spell that a wall does block but minions do not is expressed with the
    // existing API instead: collision=true plus
    // SetCollisionObjects({YasuoWall, SamiraWall, MelWall}), which reaches
    // ProcessProjectileWalls while skipping the unit passes.
    if (checkCollision && input.Collision && result.Hitchance > HitChance::None) {
        std::vector<Vector3> positions = { result.GetCastPosition() };
        auto collision = SDK::Collision::GetCollision(positions, input);
        collision.erase(
            std::remove_if(collision.begin(), collision.end(), [&](const AIBaseClient& object) {
                return !object.IsValid() || object.NetworkId() == input.Unit.NetworkId();
            }),
            collision.end());
        if (static_cast<float>(collision.size()) > input.MaxCollisionCount) {
            result.CollisionObjects = collision;
            result.SetOriginHitchance(result.Hitchance);
            result.Hitchance = HitChance::Collision;
            return result;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
// GetPrediction(PredictionInput) (Movement.cs lines 87-90)
// ----------------------------------------------------------------------------
inline PredictionOutput GetPrediction(PredictionInput input) {
    return GetPrediction(input, true, true);
}

// ----------------------------------------------------------------------------
// GetPrediction overloads (Movement.cs lines 44-76)
// ----------------------------------------------------------------------------
inline PredictionOutput GetPrediction(const AIBaseClient& unit, float delay) {
    PredictionInput input;
    input.Unit = unit;
    input.Delay = delay;
    return GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit, float delay, float radius) {
    PredictionInput input;
    input.Unit = unit;
    input.Delay = delay;
    input.Radius = radius;
    return GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit, float delay, float radius, float speed) {
    PredictionInput input;
    input.Unit = unit;
    input.Delay = delay;
    input.Radius = radius;
    input.Speed = speed;
    return GetPrediction(input);
}

// ----------------------------------------------------------------------------
// ResolveFrom - helper used by Collision.h
// ----------------------------------------------------------------------------
inline Vector3 ResolveFrom(const PredictionInput& input) {
    return input.ResolveFrom();
}

// ----------------------------------------------------------------------------
// CollectLineCollisions - helper used by Collision.h
// Returns list of game objects that collide on the line from->to.
// ----------------------------------------------------------------------------
inline std::vector<AIBaseClient> CollectLineCollisions(
    const Vector3& from, const Vector3& to, float radius,
    const AIBaseClient& ignored,
    CollisionableObjects flags) {

    std::vector<AIBaseClient> result;
    Vec2 from2D = from.To2D();
    Vec2 to2D = to.To2D();
    const auto player = SDK::ObjectManager::Player();
    const auto playerTeam = player.IsValid()
        ? player.Team()
        : GameObjectTeam::Unknown;

    auto isEnemy = [&](const AIBaseClient& unit) {
        return playerTeam == GameObjectTeam::Unknown ||
               unit.Team() != playerTeam;
    };

    auto checkUnit = [&](const AIBaseClient& unit) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsVisible()) return;
        if (ignored.IsValid() && unit.NetworkId() == ignored.NetworkId()) return;

        Vec2 unitPos = unit.Position().To2D();
        auto proj = Vec2Ext::ProjectOn(unitPos, from2D, to2D);

        float distToLine = 0.0f;
        if (proj.IsOnSegment) {
            distToLine = unitPos.Distance(proj.SegmentPoint);
        } else {
            distToLine = std::min(unitPos.Distance(from2D), unitPos.Distance(to2D));
        }

        if (distToLine <= radius + unit.BoundingRadius()) {
            result.push_back(unit);
        }
    };

    if (SDK::HasFlag(flags, CollisionableObjects::Minions)) {
        for (const auto& minion : SDK::ObjectManager::Get<AIMinionClient>()) {
            checkUnit(minion);
        }
    }

    if (SDK::HasFlag(flags, CollisionableObjects::Heroes)) {
        for (const auto& hero : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!isEnemy(hero)) {
                continue;
            }
            checkUnit(hero);
        }
    }

    // ── Samira Blade Whirl (W) ───────────────────────────────────────
    // Samira's W blocks projectiles in a radius around her for ~1s
    // Detect via buff "SamiraW" or "SamiraWBuff"
    if (SDK::HasFlag(flags, CollisionableObjects::SamiraWall)) {
        for (const auto& hero : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!hero.IsValid() || hero.IsDead()) continue;
            if (!isEnemy(hero)) continue;
            if (hero.CharacterName() != "Samira") continue;
            if (!hero.HasBuff("SamiraW") && !hero.HasBuff("SamiraWBuff"))
                continue;

            // Check if skillshot path passes through Samira's W radius
            Vec2 samiraPos = hero.Position().To2D();
            auto proj = Vec2Ext::ProjectOn(samiraPos, from2D, to2D);
            float distToLine = proj.IsOnSegment
                ? samiraPos.Distance(proj.SegmentPoint)
                : std::min(samiraPos.Distance(from2D), samiraPos.Distance(to2D));
            // Samira W radius ~ 260 (her W AoE)
            if (distToLine <= 260.0f + radius) {
                result.push_back(hero);
            }
        }
    }

    // ── Mel Rebuttal (W) ─────────────────────────────────────────────
    // Mel's W creates a 175-radius barrier that destroys/reflects projectiles
    // Duration ~0.75s. Detect via buff "MelW" or "MelWBuff" or "MelRebuttal"
    if (SDK::HasFlag(flags, CollisionableObjects::MelWall)) {
        for (const auto& hero : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!hero.IsValid() || hero.IsDead()) continue;
            if (!isEnemy(hero)) continue;
            if (hero.CharacterName() != "Mel") continue;
            if (!hero.HasBuff("MelW") && !hero.HasBuff("MelWBuff")
                && !hero.HasBuff("MelRebuttal"))
                continue;

            // Check if skillshot path passes through Mel's W barrier radius
            Vec2 melPos = hero.Position().To2D();
            auto proj = Vec2Ext::ProjectOn(melPos, from2D, to2D);
            float distToLine = proj.IsOnSegment
                ? melPos.Distance(proj.SegmentPoint)
                : std::min(melPos.Distance(from2D), melPos.Distance(to2D));
            // Mel W effect radius = 175
            if (distToLine <= 175.0f + radius) {
                result.push_back(hero);
            }
        }
    }

    return result;
}

} // namespace Movement

// ============================================================================
// Prediction facade — registry equivalent to EnsoulSharp.SDK.Prediction.
// ============================================================================
namespace detail {

class SDKPrediction final : public IPrediction {
public:
    PredictionOutput GetPrediction(PredictionInput input) override {
        return Movement::GetPrediction(input);
    }

    PredictionOutput GetPrediction(PredictionInput input,
                                   bool ft,
                                   bool checkCollision) override {
        return Movement::GetPrediction(input, ft, checkCollision);
    }
};

inline constexpr const char* SDKPredictionName = "SDK Prediction";
inline bool FacadeInitialized = false;
inline SDKPrediction DefaultPrediction;
inline std::unordered_map<std::string, IPrediction*> Implementations;
inline IPrediction* Implementation = nullptr;
inline std::string SelectedPredictionName;

} // namespace detail

inline void Initialize() {
    if (detail::FacadeInitialized) {
        return;
    }

    detail::FacadeInitialized = true;
    detail::Implementations.emplace(detail::SDKPredictionName, &detail::DefaultPrediction);
    detail::Implementation = &detail::DefaultPrediction;
    detail::SelectedPredictionName = detail::SDKPredictionName;
}

inline bool AddPrediction(const std::string& name, IPrediction* prediction) {
    Initialize();
    if (name.empty() || prediction == nullptr ||
        detail::Implementations.find(name) != detail::Implementations.end()) {
        return false;
    }

    detail::Implementations.emplace(name, prediction);
    return true;
}

inline bool AddPrediction(const std::string& name, IPrediction& prediction) {
    return AddPrediction(name, &prediction);
}

inline bool SetPrediction(const std::string& name) {
    Initialize();
    const auto it = detail::Implementations.find(name);
    if (it == detail::Implementations.end() || it->second == nullptr) {
        return false;
    }

    detail::Implementation = it->second;
    detail::SelectedPredictionName = name;
    return true;
}

inline bool RemovePrediction(const std::string& name) {
    Initialize();
    if (name == detail::SDKPredictionName) return false;
    const auto it = detail::Implementations.find(name);
    if (it == detail::Implementations.end()) return false;
    IPrediction* removed = it->second;
    detail::Implementations.erase(it);
    if (detail::Implementation == removed) {
        detail::Implementation = &detail::DefaultPrediction;
        detail::SelectedPredictionName = detail::SDKPredictionName;
    }
    return true;
}

inline IPrediction* GetPrediction(const std::string& name) {
    Initialize();
    const auto it = detail::Implementations.find(name);
    return it != detail::Implementations.end() ? it->second : nullptr;
}

inline IPrediction* GetPrediction(const char* name) {
    return name ? GetPrediction(std::string(name)) : nullptr;
}

inline IPrediction* GetSDKPrediction() {
    Initialize();
    return &detail::DefaultPrediction;
}

inline IPrediction* CurrentPrediction() {
    Initialize();
    return detail::Implementation;
}

inline const std::string& CurrentPredictionName() {
    Initialize();
    return detail::SelectedPredictionName;
}

inline bool SuspendSdkPredictionRuntime(void*) {
    Initialize();
    if (detail::Implementation == &detail::DefaultPrediction) {
        detail::Implementation = nullptr;
        detail::SelectedPredictionName.clear();
    }
    return true;
}

inline bool ResumeSdkPredictionRuntime(void*) {
    return SetPrediction(detail::SDKPredictionName);
}

inline PredictionOutput GetPrediction(PredictionInput input,
                                      bool ft,
                                      bool checkCollision) {
    Initialize();
    return detail::Implementation
        ? detail::Implementation->GetPrediction(input, ft, checkCollision)
        : Movement::GetPrediction(input, ft, checkCollision);
}

inline PredictionOutput GetPrediction(PredictionInput input) {
    Initialize();
    return detail::Implementation
        ? detail::Implementation->GetPrediction(input)
        : Movement::GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit, float delay) {
    PredictionInput input;
    input.Unit = unit;
    input.Delay = delay;
    return GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit,
                                      float delay,
                                      float radius) {
    PredictionInput input;
    input.Unit = unit;
    input.Delay = delay;
    input.Radius = radius;
    return GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit,
                                      float delay,
                                      float radius,
                                      float speed) {
    PredictionInput input;
    input.Unit = unit;
    input.Delay = delay;
    input.Radius = radius;
    input.Speed = speed;
    return GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit,
                                      float delay,
                                      float radius,
                                      float speed,
                                      bool addHitBox) {
    PredictionInput input;
    input.Unit = unit;
    input.Delay = delay;
    input.Radius = radius;
    input.Speed = speed;
    input.AddHitBox = addHitBox;
    return GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit,
                                      float delay,
                                      float radius,
                                      float speed,
                                      CollisionObjectsBridge collisionable) {
    PredictionInput input;
    input.Unit = unit;
    input.Delay = delay;
    input.Radius = radius;
    input.Speed = speed;
    input.CollisionObjects = collisionable.ToArray();
    return GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& unit,
                                      float delay,
                                      float radius,
                                      float speed,
                                      bool addHitBox,
                                      CollisionObjectsBridge collisionable) {
    PredictionInput input;
    input.Unit = unit;
    input.Delay = delay;
    input.Radius = radius;
    input.Speed = speed;
    input.AddHitBox = addHitBox;
    input.CollisionObjects = collisionable.ToArray();
    return GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& target, PredictionInput input) {
    input.Unit = target;
    return GetPrediction(input);
}

} // namespace SDK::Prediction
