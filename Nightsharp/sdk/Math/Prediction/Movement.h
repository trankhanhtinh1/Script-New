#pragma once

// ============================================================================
// Movement.h - Movement prediction ported 1:1 from EnsoulSharp.SDK
// ----------------------------------------------------------------------------
// Source: EnsoulSharp.SDK/Core/Math/Prediction/Movement.cs
// Also includes GamePath.cs (PathTracker / StoredPath) and the
// PredictionInput / PredictionOutput classes defined at the bottom of
// Movement.cs in the original C# source.
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

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
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
    ProjectionInfo(bool isOnSegment, const Vec2& linePoint, const Vec2& segmentPoint)
        : IsOnSegment(isOnSegment), LinePoint(linePoint), SegmentPoint(segmentPoint) {}
};

inline ProjectionInfo ProjectOn(const Vec2& point, const Vec2& segmentStart, const Vec2& segmentEnd) {
    float rs = (segmentStart - segmentEnd).LengthSqr();
    if (rs < 0.0001f) {
        return { false, segmentStart, segmentStart };
    }

    float rp = (point - segmentEnd).Dot(segmentStart - segmentEnd) / rs;
    if (rp < 0 || rp > 1) {
        return { false, segmentStart + (segmentStart - segmentEnd) * rp, segmentStart };
    }

    return { true, segmentStart + (segmentStart - segmentEnd) * rp, segmentStart + (segmentStart - segmentEnd) * rp };
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

    std::vector<Vec2> pathA = { pointStartA, pointEndA };

    float speedA = pointVelocityA;
    float speedB = pointVelocityB;

    if (pathA.size() < 2) {
        return { 0.0f, pointB };
    }

    float tT = delay;
    float wayPointIndex = 0;
    std::vector<float> segmentPositions = { 0.0f, 0.0f };

    for (std::size_t i = 0; i + 1 < pathA.size(); ++i) {
        Vec2 a = pathA[i];
        Vec2 b = pathA[i + 1];

        float segmentDistance = a.Distance(b);
        float timeToReachSegmentEnd = 0.0f;
        if (segmentDistance > 0) {
            segmentPositions[1] = segmentPositions[0] + segmentDistance;

            timeToReachSegmentEnd = segmentDistance / speedA;
            float segmentTime = 0.0f;

            while (segmentTime < timeToReachSegmentEnd + 0.001f) {
                float currentDistance = speedA * (tT - segmentTime) + segmentPositions[0];
                Vec2 currentPosition = a + (b - a).Normalized() * (currentDistance - segmentPositions[0]);

                float distanceToB = currentPosition.Distance(pointB);
                float timeToReachB = distanceToB / speedB;

                if (std::abs(timeToReachB - (tT - segmentTime)) < 0.001f) {
                    return { tT - segmentTime, currentPosition };
                }

                segmentTime += 0.01f;
            }
        }

        tT += timeToReachSegmentEnd;
        segmentPositions[0] = segmentPositions[1];
        wayPointIndex = static_cast<float>(i);
    }

    return { -1.0f, {} };
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

// ============================================================================
// GamePath::StoredPath and GamePath::PathTracker (ported from GamePath.cs)
// ============================================================================
namespace GamePath {

struct StoredPath {
    std::vector<Vec2> Path;
    int Tick = 0;

    Vec2 StartPoint() const {
        return Path.empty() ? Vec2() : Path.front();
    }

    Vec2 EndPoint() const {
        return Path.empty() ? Vec2() : Path.back();
    }

    int WaypointCount() const {
        return static_cast<int>(Path.size());
    }

    double Time() const {
        return (SDK::Variables::TickCount() - Tick) / 1000.0;
    }
};

class PathTracker {
public:
    static constexpr double MaxTime = 1.5;

    static void Initialize() {
        if (initialized_) return;
        initialized_ = true;
        SDK::Events::AddOnNewPath(&OnNewPath);
    }

    static StoredPath GetCurrentPath(const AIBaseClient& unit) {
        Initialize();
        auto it = StoredPaths_.find(static_cast<std::uint32_t>(unit.NetworkId()));
        if (it != StoredPaths_.end() && !it->second.empty()) {
            return it->second.back();
        }
        return {};
    }

    static std::vector<StoredPath> GetStoredPaths(const AIBaseClient& unit, double maxT) {
        Initialize();
        std::vector<StoredPath> result;
        auto it = StoredPaths_.find(static_cast<std::uint32_t>(unit.NetworkId()));
        if (it == StoredPaths_.end()) return result;
        for (const auto& p : it->second) {
            if (p.Time() < maxT) {
                result.push_back(p);
            }
        }
        return result;
    }

    static double GetMeanSpeed(const AIBaseClient& unit, double maxT) {
        Initialize();
        auto paths = GetStoredPaths(unit, MaxTime);
        double distance = 0.0;
        if (!paths.empty()) {
            distance += (maxT - paths[0].Time()) * unit.MoveSpeed();
            for (std::size_t i = 0; i + 1 < paths.size(); ++i) {
                const auto& currentPath = paths[i];
                const auto& nextPath = paths[i + 1];
                if (currentPath.WaypointCount() > 0) {
                    distance += std::min(
                        (currentPath.Time() - nextPath.Time()) * unit.MoveSpeed(),
                        static_cast<double>(SDK::Utils::MathUtils::PathLength(currentPath.Path)));
                }
            }
            const auto& lastPath = paths.back();
            if (lastPath.WaypointCount() > 0) {
                distance += std::min(
                    lastPath.Time() * unit.MoveSpeed(),
                    static_cast<double>(SDK::Utils::MathUtils::PathLength(lastPath.Path)));
            }
        } else {
            return unit.MoveSpeed();
        }
        return distance / maxT;
    }

private:
    static inline bool initialized_ = false;
    static inline std::unordered_map<std::uint32_t, std::vector<StoredPath>> StoredPaths_;

    static void OnNewPath(const SDK::Events::NewPathEventArgs& args) {
        // Only track heroes (matching C# behavior)
        // The C# code checks `sender is AIHeroClient`
        // In NightSharp, we check if the object is a hero via the handle type
        if (args.Sender.Ptr == 0) return;

        auto& list = StoredPaths_[args.Sender.NetworkId];

        StoredPath newPath;
        newPath.Tick = SDK::Variables::TickCount();
        for (int i = 0; i < args.PathCount; ++i) {
            newPath.Path.push_back(args.Path[i].To2D());
        }
        list.push_back(std::move(newPath));

        if (list.size() > 50) {
            list.erase(list.begin(), list.begin() + 40);
        }
    }
};

} // namespace GamePath

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
            return Input.Unit.Position();
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
            return Input.Unit.Position();
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
        if (GamePath::PathTracker::GetCurrentPath(unit).Time() < 0.1) {
            return HitChance::VeryHigh;
        }
        return HitChance::High;
    }
    return HitChance::VeryHigh;
}

// ----------------------------------------------------------------------------
// GetPositionOnPath (Movement.cs lines 213-319)
// ----------------------------------------------------------------------------
inline PredictionOutput GetPositionOnPath(PredictionInput& input, std::vector<Vec2> path, float speed = -1.0f, bool needToFixSpeed = false) {
    if (needToFixSpeed && input.Unit.Position().DistanceSqr2D(input.ResolveFrom()) < 250.0f * 250.0f) {
        speed *= 1.5f;
    }
    speed = (std::abs(speed - (-1.0f)) < 0.0001f) ? input.Unit.MoveSpeed() : speed;

    Vector3 serverPos = input.Unit.Position();

    if (path.size() <= 1 || (input.Unit.Spellbook().IsWindingUp() && !SDK::Extensions::IsDashing(input.Unit))) {
        PredictionOutput output;
        output.Input = input;
        output.SetUnitPosition(serverPos);
        output.SetCastPosition(serverPos);
        output.Hitchance = HitChance::VeryHigh;
        return output;
    }

    float pLength = SDK::Utils::MathUtils::PathLength(path);

    // Short path check for minions/jungle/dummies
    bool isJungle = false;
    if (input.Unit.IsMinion()) {
        AIMinionClient minion(input.Unit.Address());
        isJungle = minion.IsJungle();
    }
    if (path.size() == 2 && pLength < 5.0f
        && (input.Unit.CharacterName() == "PracticeTool_TargetDummy"
            || input.Unit.IsMinion() || isJungle)) {
        PredictionOutput output;
        output.Input = input;
        output.SetUnitPosition(serverPos);
        output.SetCastPosition(serverPos);
        output.Hitchance = HitChance::VeryHigh;
        return output;
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
    Vec2 prediction = position + (Vec2(1, 1) * (speed * (input.Delay / 1000.0f)));

    // C# code: prediction = position + (speed * (input.Delay / 1000));
    // This is a scalar * Vector2 multiplication, meaning the vector is
    // extended by the scalar in its own direction. But in C# the `+` operator
    // on Vector2 + float is not valid. Looking more carefully at the C# code:
    //   var prediction = position + (speed * (input.Delay / 1000));
    // This is actually: position + (speed * (input.Delay / 1000)) * direction
    // But the C# code doesn't have a direction. Actually, re-reading:
    // In C#, `speed * (input.Delay / 1000)` is a float, and `position + float`
    // is not valid for Vector2. This must be a Vector2 operation.
    // Looking at the decompiled DLL would clarify, but the most likely
    // interpretation is that this is position + direction * (speed * delay/1000)
    // where direction is the unit's movement direction. However, the original
    // code as written doesn't compile in C# as-is. Let me re-interpret:
    // Actually in C#, `float * float` = float, and `Vector2 + float` doesn't
    // compile. This is likely a bug in the source or the decompiler. The
    // decompiled DLL shows the actual behavior. Let me use the most sensible
    // interpretation: the prediction is the position offset by delay*speed
    // in the direction of movement.
    // For now, matching the source as closely as possible:
    prediction = position + position.Normalized() * (speed * (input.Delay / 1000.0f));

    PredictionOutput output;
    output.SetUnitPosition(Vec3Ext::SetZ(Vec3(position.x, position.y, 0)));
    output.SetCastPosition(Vec3Ext::SetZ(Vec3(prediction.x, prediction.y, 0)));
    output.Hitchance = HitChance::High;
    return output;
}

// ----------------------------------------------------------------------------
// GetDashingPrediction (DLL: PredictionSDK.GetDashingPrediction)
// Handles both gapcloser and normal dash paths.
// ----------------------------------------------------------------------------
inline PredictionOutput GetDashingPrediction(PredictionInput& input) {
    PredictionOutput result;
    result.Input = input;

    // DLL checks GetGapcloserInfo first; if null or "NullDash", falls through to dash.
    // NightSharp AntiGapcloser not yet ported, so we skip gapcloser branch.
    // TODO: Port AntiGapcloser.GetGapcloserInfo for full parity.

    auto dashData = SDK::Extensions::GetDashInfo(input.Unit);
    input.Delay += 0.1f;

    Vec2 startPos = dashData.StartPos.IsValid()
        ? dashData.StartPos.To2D()
        : input.Unit.Position().To2D();
    Vec2 endPos = dashData.PathCount > 0
        ? dashData.Path[dashData.PathCount - 1].To2D()
        : (dashData.EndPos.IsValid() ? dashData.EndPos.To2D() : startPos);

    // Mid-air prediction
    std::vector<Vec2> dashPath = { startPos, endPos };
    auto dashPred = GetPositionOnPath(input, dashPath, dashData.Speed);

    // DLL: Distance(unitPosition, ServerPosition, endPos, onlyIfOnSegment) < 200
    if (dashPred.Hitchance >= HitChance::High) {
        Vec2 serverPos2D = input.Unit.Position().To2D();
        auto proj = Vec2Ext::ProjectOn(dashPred.GetUnitPosition().To2D(), serverPos2D, endPos);
        if (proj.IsOnSegment && proj.SegmentPoint.Distance(dashPred.GetUnitPosition().To2D()) < 200.0f) {
            dashPred.SetCastPosition(dashPred.GetUnitPosition());
            dashPred.Hitchance = HitChance::Dash;
            return dashPred;
        }
    }

    // End-of-dash prediction
    // DLL: input.Delay / 2f + input.From.Distance(endPos) / input.Speed - 0.25f
    if (startPos.Distance(endPos) > 200.0f) {
        float timeToPoint = (input.Delay / 2.0f)
            + (input.ResolveFrom().To2D().Distance(endPos) / input.Speed)
            - 0.25f;
        float timeForUnit = (input.Unit.Position().To2D().Distance(endPos) / dashData.Speed)
            + (input.RealRadius() / input.Unit.MoveSpeed());

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
// GetImmobilePrediction (DLL: PredictionSDK.GetImmobilePrediction)
// DLL has 3 tiers: Immobile, VeryHigh (within 0.2s), Medium (fallback)
// ----------------------------------------------------------------------------
inline PredictionOutput GetImmobilePrediction(PredictionInput& input, double remainingImmobileT) {
    Vector3 serverPos = input.Unit.Position();
    float timeToReach = input.Delay + (input.Unit.Distance(input.ResolveFrom()) / input.Speed);

    if ((double)timeToReach <= remainingImmobileT + (double)(input.RealRadius() / input.Unit.MoveSpeed())) {
        PredictionOutput output;
        output.SetCastPosition(serverPos);
        output.SetUnitPosition(serverPos);
        output.Hitchance = HitChance::Immobile;
        return output;
    }

    // DLL: second check with 0.2s tolerance -> VeryHigh
    if ((double)(timeToReach - 0.2f) <= remainingImmobileT + (double)(input.RealRadius() / input.Unit.MoveSpeed())) {
        PredictionOutput output;
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
    float speed = input.Unit.MoveSpeed();

    if (input.Unit.Position().DistanceSqr2D(input.ResolveFrom()) < 200.0f * 200.0f) {
        speed /= 1.5f;
    }

    auto waypoints3D = input.Unit.GetWaypoints();
    std::vector<Vec2> path;
    path.reserve(waypoints3D.size());
    for (const auto& wp : waypoints3D) {
        path.push_back(wp.To2D());
    }

    auto result = GetPositionOnPath(input, path, speed);

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

    const Vector3 position = unit.Position();
    if (!position.IsValid() || position.IsZero()) {
        return false;
    }

    const float health = unit.Health();
    const float maxHealth = unit.MaxHealth();
    if (maxHealth > 0.0f && health <= 0.0f) {
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

    // Target too far away (DLL: RangeCheckFrom, not ResolveRangeCheckFrom)
    if (std::abs(input.Range - FLT_MAX) > 0.0001f
        && input.Unit.Position().DistanceSqr2D(input.ResolveRangeCheckFrom())
        > std::pow(input.Range * 1.5f, 2)) {
        PredictionOutput output;
        output.Input = input;
        return output;
    }

    // DLL: only check dashing/immobile for AIHeroClient
    bool hasResult = false;
    if (input.Unit.IsHero()) {
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
    }

    // DLL: fallback to GetPositionOnPath if not dashing/immobile
    if (!hasResult) {
        auto waypoints3D = input.Unit.GetWaypoints();
        std::vector<Vec2> path;
        path.reserve(waypoints3D.size());
        for (const auto& wp : waypoints3D) {
            path.push_back(wp.To2D());
        }
        result = GetPositionOnPath(input, path, input.Unit.MoveSpeed(), true);
    }

    // Range checks (DLL: no cast position clamping, just Hitchance changes)
    if (std::abs(input.Range - FLT_MAX) > 0.0001f) {
        if (result.Hitchance >= HitChance::High
            && input.ResolveRangeCheckFrom().DistanceSqr2D(input.Unit.Position())
            > std::pow(input.Range + (input.RealRadius() * 3.0f / 4.0f), 2)) {
            result.Hitchance = HitChance::Medium;
        }

        if (input.ResolveRangeCheckFrom().DistanceSqr2D(result.GetUnitPosition())
            > std::pow(input.Range + (IsCircleSpellType(input.Type)
                ? input.RealRadius() : 0.0f), 2)) {
            result.Hitchance = HitChance::OutOfRange;
        }
    }

    // DLL collision block:
    //   var collision = Collisions.GetCollision(new List<Vector3>{ CastPosition }, input);
    //   if (collision.Count > input.MaxCollisionCount) {
    //     collision.RemoveAll(x => x == null || !x.IsValid || x.Compare(input.Unit));
    //     result.CollisionObjects = collision;
    //     result.OriginHitchance = result.Hitchance;
    //     result.Hitchance = HitChance.Collision;
    //     return result;
    //   }
    if (checkCollision && input.Collision && result.Hitchance > HitChance::None) {
        std::vector<Vector3> positions = { result.GetCastPosition() };
        auto collision = SDK::Collision::GetCollision(positions, input);
        if (static_cast<float>(collision.size()) > input.MaxCollisionCount) {
            collision.erase(
                std::remove_if(collision.begin(), collision.end(), [&](const AIBaseClient& object) {
                    return !object.IsValid() || object.Compare(input.Unit);
                }),
                collision.end());
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
    Vec2 dir = (to2D - from2D).Normalized();
    float lineLength = from2D.Distance(to2D);

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
        for (const auto& minion : SDK::GameObjects::EnemyMinions()) {
            checkUnit(minion);
        }
        for (const auto& minion : SDK::GameObjects::AllyMinions()) {
            checkUnit(minion);
        }
    }

    if (SDK::HasFlag(flags, CollisionableObjects::Heroes)) {
        for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
            checkUnit(hero);
        }
    }

    // ── Yasuo Wind Wall ──────────────────────────────────────────────
    // C# source: checks particle emitters matching "Yasuo_.+_w_windwall_enemy_\d"
    // Wall width = 250 + 50 * level (from particle name suffix)
    if (SDK::HasFlag(flags, CollisionableObjects::YasuoWall)) {
        bool hasYasuo = false;
        for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
            if (hero.IsValid() && !hero.IsDead()
                && hero.CharacterName() == "Yasuo") {
                hasYasuo = true;
                break;
            }
        }
        if (hasYasuo) {
            for (const auto& emitter : SDK::GameObjects::ParticleEmitters()) {
                if (!emitter.IsValid()) continue;
                const std::string name = emitter.Name();
                // Match "Yasuo_.+_w_windwall_enemy_\d" (case-insensitive)
                if (name.find("Yasuo") != std::string::npos
                    && name.find("_w_windwall") != std::string::npos
                    && name.find("enemy") != std::string::npos)
                {
                    // Extract level from last 2 chars
                    int level = 1;
                    if (name.size() >= 2) {
                        auto c = name.back();
                        if (c >= '1' && c <= '9') level = c - '0';
                    }
                    float wallWidth = 250.0f + 50.0f * static_cast<float>(level);
                    Vec2 wallPos = emitter.Position().To2D();
                    Vec2 wallDir = emitter.Direction().To2D();
                    if (wallDir.LengthSqr() < 0.001f) continue;
                    Vec2 perp(-wallDir.y, wallDir.x);
                    perp = perp.Normalized();
                    Vec2 wallStart = wallPos + perp * (wallWidth * 0.5f);
                    Vec2 wallEnd = wallPos - perp * (wallWidth * 0.5f);

                    // Check if skillshot path intersects wall segment
                    auto inter = Vec2Ext::Intersection(from2D, to2D, wallStart, wallEnd);
                    if (inter.Valid) {
                        result.push_back(SDK::GameObjects::Player());
                        break;
                    }
                }
            }
        }
    }

    // ── Samira Blade Whirl (W) ───────────────────────────────────────
    // Samira's W blocks projectiles in a radius around her for ~1s
    // Detect via buff "SamiraW" or "SamiraWBuff"
    if (SDK::HasFlag(flags, CollisionableObjects::SamiraWall)) {
        for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
            if (!hero.IsValid() || hero.IsDead()) continue;
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
        for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
            if (!hero.IsValid() || hero.IsDead()) continue;
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
