#pragma once
// ============================================================================
// EvadeGeometry.h — Complete Evade Geometry Engine (Phase 1)
// Reference: EvadeSharp Evader.cs, EzEvade EvadeHelper.cs,
//            vEvade Core/Evader.cs, Chimera Evade geometry
// ============================================================================

#include "sdk/Math/Polygon.h"
#include "sdk/SDK.h"
#include "core/Vector.h"
#include <vector>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>

namespace Evade {

// ============================================================================
// Enums
// ============================================================================
enum class SpellType : int {
    Line     = 0,
    Circle   = 1,
    Cone     = 2,
    Ring     = 3,
    Arc      = 4
};

// ============================================================================
// SafePathResult — Result of path safety check
// ============================================================================
struct SafePathResult {
    bool IsSafe = true;
    Vec2 IntersectionPoint;
    float TimeToIntersect = FLT_MAX;
    int SkillshotIndex = -1;
};

// ============================================================================
// PositionInfo — Result of position evaluation with multi-factor scoring
// Reference: EzEvade PositionInfo.cs
// ============================================================================
struct PositionInfo {
    Vec2 Position;
    int PosDangerLevel = 0;        // Highest danger level at this pos
    int PosDangerCount = 0;        // Sum of danger levels at this pos
    bool IsDangerousPos = false;   // Is inside any skillshot
    bool HasExtraDistance = false;  // Still inside with extra buffer
    bool RejectPosition = false;   // Rejected (wall, etc.)
    float DistanceToMouse = 0.0f;
    float DistanceToPlayer = 0.0f;
    float ClosestDistance = FLT_MAX; // Closest distance to any skillshot edge
    float PosDistToChamps = FLT_MAX; // Distance to nearest enemy champion
    float PosDistToTurrets = FLT_MAX;// Distance to nearest enemy turret
    float PosDistToAllies = FLT_MAX; // Distance to nearest ally
    float Score = 0.0f;            // Final composite score (lower = better)
    std::vector<int> DodgeableSpells;
    std::vector<int> UndodgeableSpells;

    PositionInfo() = default;

    PositionInfo(const Vec2& pos, int dangerLevel, int dangerCount, bool isDangerous)
        : Position(pos), PosDangerLevel(dangerLevel), PosDangerCount(dangerCount),
          IsDangerousPos(isDangerous) {}

    // Compare: lower is better
    bool IsBetterThan(const PositionInfo& other) const {
        if (RejectPosition != other.RejectPosition) return !RejectPosition;
        if (PosDangerLevel != other.PosDangerLevel) return PosDangerLevel < other.PosDangerLevel;
        if (PosDangerCount != other.PosDangerCount) return PosDangerCount < other.PosDangerCount;
        if (HasExtraDistance != other.HasExtraDistance) return !HasExtraDistance;
        return Score < other.Score;
    }
};

// ============================================================================
// SkillshotPolygon — Active skillshot with geometry data
// ============================================================================
struct SkillshotPolygon {
    SDK::Polygon DangerZone;
    SDK::Polygon EvadePolygon;         // Expanded polygon (with extra buffer)
    Vec2 Start;
    Vec2 End;
    Vec2 Direction;                    // Normalized direction vector
    Vec2 MissilePosition;              // Current missile position (moving spells)
    Vec2 ConeLeft, ConeRight;          // Cone triangle vertices
    float Speed = 0.0f;
    float Width = 0.0f;               // Radius for circle, half-width for line
    float Range = 0.0f;
    float CastTime = 0.0f;            // Game time when spell was cast
    float EndTime = 0.0f;             // Game time when spell expires
    float ExtraDelay = 0.0f;
    float ConeAngle = 0.0f;           // Cone angle in radians
    float RingInnerRadius = 0.0f;     // For ring type
    float ArcAngle = 0.0f;            // For arc type
    int DangerLevel = 1;               // 1-5 danger scale
    SpellType Type = SpellType::Line;
    bool IsActive = true;
    bool IsMissileSpell = false;       // Has a moving missile object
    bool HasCollision = false;         // Blocked by minions/heroes
    int SpellId = -1;
    std::string SpellName;
    std::string CasterName;

    // ---- Create the danger polygon from spell parameters ----
    void UpdatePolygon(int extraWidth = 0) {
        DangerZone = SDK::Polygon();
        EvadePolygon = SDK::Polygon();
        Direction = (End - Start).Normalized();

        const float evadeBuffer = 20.0f;

        switch (Type) {
        case SpellType::Line: {
            SDK::RectanglePoly rect(Start, End, Width + extraWidth);
            rect.UpdatePolygon();
            DangerZone = rect;
            SDK::RectanglePoly rectEvade(Start, End, Width + extraWidth + evadeBuffer);
            rectEvade.UpdatePolygon();
            EvadePolygon = rectEvade;
            break;
        }
        case SpellType::Circle: {
            SDK::CirclePoly circle(End, Width + extraWidth);
            circle.UpdatePolygon();
            DangerZone = circle;
            SDK::CirclePoly circleEvade(End, Width + extraWidth + evadeBuffer);
            circleEvade.UpdatePolygon();
            EvadePolygon = circleEvade;
            break;
        }
        case SpellType::Cone: {
            SDK::SectorPoly sector(Start, End, ConeAngle, Range + extraWidth);
            sector.UpdatePolygon();
            DangerZone = sector;
            // Calculate cone triangle for InSkillShot check
            ConeLeft = Start + Direction.Rotated(ConeAngle / 2.0f) * Range;
            ConeRight = Start + Direction.Rotated(-ConeAngle / 2.0f) * Range;
            SDK::SectorPoly sectorEvade(Start, End, ConeAngle + 0.1f, Range + extraWidth + evadeBuffer);
            sectorEvade.UpdatePolygon();
            EvadePolygon = sectorEvade;
            break;
        }
        case SpellType::Ring: {
            float inner = RingInnerRadius > 0 ? RingInnerRadius : Width * 0.6f;
            SDK::RingPoly ring(End, Width - inner, Width + extraWidth);
            ring.UpdatePolygon();
            DangerZone = ring;
            SDK::RingPoly ringEvade(End, std::max(0.0f, Width - inner - 10), Width + extraWidth + evadeBuffer);
            ringEvade.UpdatePolygon();
            EvadePolygon = ringEvade;
            break;
        }
        case SpellType::Arc: {
            SDK::ArcPoly arc(Start, End, ArcAngle > 0 ? ArcAngle : 0.5f, Width + extraWidth);
            arc.UpdatePolygon();
            DangerZone = arc;
            SDK::ArcPoly arcEvade(Start, End, ArcAngle > 0 ? ArcAngle : 0.5f, Width + extraWidth + evadeBuffer);
            arcEvade.UpdatePolygon();
            EvadePolygon = arcEvade;
            break;
        }
        }
    }

    // ---- Update missile position for moving spells ----
    void UpdateMissilePosition(float gameTime) {
        if (Speed <= 0.0f || !IsMissileSpell) return;
        float elapsed = gameTime - CastTime - ExtraDelay;
        if (elapsed < 0.0f) elapsed = 0.0f;
        float distTraveled = elapsed * Speed;
        if (distTraveled > Range) distTraveled = Range;
        MissilePosition = Start + Direction * distTraveled;
    }

    // ---- Check if spell has expired ----
    bool IsExpired(float gameTime) const {
        if (EndTime > 0.0f) return gameTime > EndTime;
        if (Speed > 0.0f) {
            float totalTime = Range / Speed;
            return (gameTime - CastTime) > totalTime + 0.5f;
        }
        return false;
    }

    // ---- Point-in-skillshot check (precise per-type, ref: vEvade Position.cs) ----
    bool InSkillShot(const Vec2& point, float extraRadius = 0.0f) const {
        switch (Type) {
        case SpellType::Line: {
            Vec2 spellPos = (Speed > 0.0f) ? MissilePosition : Start;
            auto proj = SDK::GeometryAdv::ProjectOn(point, spellPos, End);
            return proj.IsOnSegment &&
                   proj.SegmentPoint.Distance(point) <= Width + extraRadius;
        }
        case SpellType::Circle: {
            return point.Distance(End) <= Width + extraRadius;
        }
        case SpellType::Cone: {
            // Triangle test: point must be on same side of all 3 edges
            auto isLeft = [](const Vec2& p, const Vec2& a, const Vec2& b) {
                return ((b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)) > 0.0f;
            };
            return !isLeft(point, Start, ConeLeft)
                && !isLeft(point, ConeLeft, ConeRight)
                && !isLeft(point, ConeRight, Start);
        }
        case SpellType::Ring: {
            float dist = point.Distance(End);
            float inner = RingInnerRadius > 0 ? RingInnerRadius : Width * 0.6f;
            return dist <= Width + extraRadius && dist >= inner - extraRadius;
        }
        case SpellType::Arc: {
            auto isLeft = [](const Vec2& p, const Vec2& a, const Vec2& b) {
                return ((b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)) > 0.0f;
            };
            if (isLeft(point, Start, End)) return false;
            float spellRange = Start.Distance(End);
            Vec2 midPoint = Start + Direction * (spellRange / 2.0f);
            return point.Distance(midPoint) <= Width + extraRadius;
        }
        }
        return DangerZone.IsInside(point);
    }

    // ---- Is a point inside the danger zone? (polygon-based) ----
    bool IsDanger(const Vec2& point) const {
        return DangerZone.IsInside(point);
    }

    // ---- Distance from point to nearest edge of danger zone ----
    float DistanceToDanger(const Vec2& point) const {
        return DangerZone.DistanceToEdge(point);
    }

    // ---- Closest point on evade polygon edge ----
    Vec2 ClosestEdgePoint(const Vec2& point) const {
        return EvadePolygon.ClosestPointOnEdge(point);
    }

    // ---- Time until missile reaches a given point (ms) ----
    float TimeToReachPoint(const Vec2& point, float gameTime) const {
        if (Speed <= 0.0f) {
            float activateTime = CastTime + ExtraDelay;
            return (activateTime - gameTime) * 1000.0f;
        }
        float dist = MissilePosition.Distance(point);
        return (dist / Speed) * 1000.0f;
    }

    // ---- Time remaining before this skillshot expires (ms) ----
    float TimeRemaining(float gameTime) const {
        if (EndTime > 0.0f) return (EndTime - gameTime) * 1000.0f;
        if (Speed <= 0.0f) return FLT_MAX;
        float totalTravelTime = Range / Speed;
        float elapsed = gameTime - CastTime;
        return (totalTravelTime - elapsed) * 1000.0f;
    }

    // ---- Check if line from A to B intersects this spell (ref: EzEvade) ----
    bool LineIntersects(const Vec2& from, const Vec2& to) const {
        const auto& poly = EvadePolygon;
        int n = (int)poly.Points.size();
        for (int i = 0; i < n; i++) {
            auto result = SDK::GeometryAdv::SegmentIntersection(
                from, to, poly.Points[i], poly.Points[(i + 1) % n]);
            if (result.Intersects) return true;
        }
        return false;
    }
};

// ============================================================================
// EvadeGeometry — Complete geometry function library for evasion
// ============================================================================
namespace EvadeGeometry {

    // ---- Config constants ----
    constexpr int DiagonalEvadePointsCount = 7;
    constexpr float DiagonalEvadePointsStep = 20.0f;
    constexpr float MaxEvadeRange = 600.0f;
    constexpr float EvadePolygonBuffer = 20.0f;
    constexpr float WallCheckStep = 50.0f;
    constexpr float BoundingRadius = 65.0f; // Default champion bounding radius

    // ====================================================================
    // SECTION 1: Wall & Terrain Checks
    // ====================================================================

    // Check if a segment (from → to) crosses any wall
    // Ref: EzEvade EvadeHelper.SegmentHitsWall
    inline bool SegmentHitsWall(const Vec2& from, const Vec2& to, float height = 0.0f) {
        float dist = from.Distance(to);
        if (dist <= 1.0f) return false;

        Vec2 dir = (to - from).Normalized();
        int steps = std::max(2, (int)(dist / WallCheckStep));
        for (int i = 1; i <= steps; i++) {
            Vec2 p = from + dir * (dist * ((float)i / (float)steps));
            if (SDK::GameObject::IsWallAt(Vec3::From2D(p, height))) {
                return true;
            }
        }
        return false;
    }

    // Find the first wall point along a path
    // Ref: EzEvade EvadeHelper.GetNearWallPoint
    inline Vec3 GetNearWallPoint(const Vec3& start, const Vec3& end, float sampleStep = 35.0f) {
        float totalDist = start.Distance2D(end);
        if (totalDist <= 1.0f) return Vec3();

        float step = std::max(sampleStep, 5.0f);
        int samples = std::max(2, (int)(totalDist / step));
        for (int i = 1; i <= samples; i++) {
            float t = (float)i / (float)samples;
            Vec3 probe = Vec3::Lerp(start, end, t);
            if (SDK::GameObject::IsWallAt(probe)) {
                return probe;
            }
        }
        return Vec3();
    }

    // Check if a point is walkable (not inside a wall)
    inline bool IsWalkablePoint(const Vec2& point, float height = 0.0f) {
        return !SDK::GameObject::IsWallAt(Vec3::From2D(point, height));
    }

    // ====================================================================
    // SECTION 2: Point-in-Danger Checks
    // ====================================================================

    // Check if a position is safe from ALL active skillshots
    inline bool IsSafePoint(const Vec2& point,
                            const std::vector<SkillshotPolygon>& skillshots,
                            float extraRadius = 0.0f)
    {
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            if (ss.InSkillShot(point, extraRadius)) return false;
        }
        return true;
    }

    // Check danger level at a position (sum of all overlapping spell danger levels)
    // Ref: EzEvade Position.CheckPosDangerLevel
    inline int CheckPosDangerLevel(const Vec2& pos,
                                   const std::vector<SkillshotPolygon>& skillshots,
                                   float extraBuffer = 0.0f)
    {
        int dangerLevel = 0;
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            if (ss.InSkillShot(pos, BoundingRadius + extraBuffer)) {
                dangerLevel += ss.DangerLevel;
            }
        }
        return dangerLevel;
    }

    // Check if position is dangerous (inside any skillshot)
    // Ref: EzEvade Position.CheckDangerousPos
    inline bool CheckDangerousPos(const Vec2& pos,
                                  const std::vector<SkillshotPolygon>& skillshots,
                                  float extraBuffer = 0.0f,
                                  bool checkOnlyDangerous = false)
    {
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            if (checkOnlyDangerous && ss.DangerLevel < 3) continue;
            if (ss.InSkillShot(pos, BoundingRadius + extraBuffer)) {
                return true;
            }
        }
        return false;
    }

    // Get all skillshots that threaten a point
    inline std::vector<const SkillshotPolygon*> GetDangerousSkillshots(
        const Vec2& playerPos,
        const std::vector<SkillshotPolygon>& skillshots)
    {
        std::vector<const SkillshotPolygon*> dangerous;
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            if (ss.InSkillShot(playerPos, BoundingRadius)) {
                dangerous.push_back(&ss);
            }
        }
        return dangerous;
    }

    // Get the highest danger level among threats at a point
    inline int GetHighestDangerLevel(const Vec2& playerPos,
                                     const std::vector<SkillshotPolygon>& skillshots)
    {
        int maxDanger = 0;
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            if (ss.InSkillShot(playerPos, BoundingRadius) && ss.DangerLevel > maxDanger) {
                maxDanger = ss.DangerLevel;
            }
        }
        return maxDanger;
    }

    // Time until the closest threatening skillshot hits player (ms)
    inline float GetTimeToHit(const Vec2& playerPos,
                              const std::vector<SkillshotPolygon>& skillshots,
                              float gameTime)
    {
        float minTime = FLT_MAX;
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            if (!ss.InSkillShot(playerPos, BoundingRadius)) continue;
            float t = ss.TimeToReachPoint(playerPos, gameTime);
            if (t < minTime) minTime = t;
        }
        return minTime;
    }

    // ====================================================================
    // SECTION 3: Path Safety Checks
    // ====================================================================

    // Check if a movement path (single segment) is safe
    // Ref: EvadeSharp Program.IsSafePath
    inline SafePathResult IsSafePath(const Vec2& from, const Vec2& to,
                                     const std::vector<SkillshotPolygon>& skillshots,
                                     float moveSpeed, float delayMs,
                                     float gameTime, int timeOffset = 0)
    {
        SafePathResult result;
        result.IsSafe = true;

        float pathLength = from.Distance(to);
        if (pathLength < 1.0f) return result;

        for (size_t si = 0; si < skillshots.size(); si++) {
            const auto& ss = skillshots[si];
            if (!ss.IsActive) continue;

            // Check if destination is inside danger
            if (ss.InSkillShot(to, BoundingRadius)) {
                result.IsSafe = false;
                result.IntersectionPoint = to;
                result.TimeToIntersect = (pathLength / moveSpeed) * 1000.0f + delayMs;
                result.SkillshotIndex = (int)si;
                return result;
            }

            // Check if path line intersects the evade polygon
            const auto& poly = ss.EvadePolygon;
            int n = (int)poly.Points.size();
            if (n < 3) continue;

            for (int i = 0; i < n; i++) {
                auto intersection = SDK::GeometryAdv::SegmentIntersection(
                    from, to, poly.Points[i], poly.Points[(i + 1) % n]);

                if (intersection.Intersects) {
                    float distToIntersect = from.Distance(intersection.Point);
                    float timeToReachMs = (distToIntersect / moveSpeed) * 1000.0f + delayMs;
                    float skillshotTimeMs = ss.TimeToReachPoint(intersection.Point, gameTime);

                    if (timeToReachMs + timeOffset > skillshotTimeMs) {
                        float exitTime = (pathLength / moveSpeed) * 1000.0f + delayMs;
                        if (exitTime + timeOffset > skillshotTimeMs || ss.InSkillShot(to, BoundingRadius)) {
                            result.IsSafe = false;
                            result.IntersectionPoint = intersection.Point;
                            result.TimeToIntersect = timeToReachMs;
                            result.SkillshotIndex = (int)si;
                            return result;
                        }
                    }
                }
            }
        }
        return result;
    }

    // Check if a multi-waypoint path is safe
    inline SafePathResult IsSafePathMulti(const std::vector<Vec2>& path,
                                          const std::vector<SkillshotPolygon>& skillshots,
                                          float moveSpeed, float delayMs, float gameTime)
    {
        SafePathResult result;
        result.IsSafe = true;
        float accumulatedDelay = delayMs;

        for (size_t i = 0; i + 1 < path.size(); i++) {
            auto segResult = IsSafePath(path[i], path[i + 1], skillshots,
                                        moveSpeed, accumulatedDelay, gameTime);
            if (!segResult.IsSafe) return segResult;
            accumulatedDelay += (path[i].Distance(path[i + 1]) / moveSpeed) * 1000.0f;
        }
        return result;
    }

    // Check if blinking to a position is safe (instant movement)
    // Ref: EvadeSharp Program.IsSafeToBlink
    inline bool IsSafeToBlink(const Vec2& point,
                              const std::vector<SkillshotPolygon>& skillshots,
                              float delayMs, float gameTime)
    {
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            if (!ss.InSkillShot(point, BoundingRadius)) continue;
            float arrivalTimeMs = delayMs;
            float skillshotRemaining = ss.TimeRemaining(gameTime);
            if (arrivalTimeMs < skillshotRemaining) {
                return false;
            }
        }
        return true;
    }

    // Check if walking to a point will cross any skillshot
    // Ref: EzEvade EvadeHelper.CheckMoveToDirection
    inline bool CheckMoveToDirection(const Vec2& from, const Vec2& moveTo,
                                     const std::vector<SkillshotPolygon>& skillshots,
                                     float moveSpeed)
    {
        // If we're already inside a skillshot, don't block
        if (!IsSafePoint(from, skillshots, BoundingRadius)) return false;

        // Check if the movement path intersects any skillshot
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            if (ss.LineIntersects(from, moveTo)) return true;
        }
        return false;
    }

    // Combined check: wall collision + skillshot crossing
    // Ref: EzEvade EvadeHelper.CheckMovePath
    inline bool CheckMovePath(const Vec2& from, const Vec2& moveTo,
                              const std::vector<SkillshotPolygon>& skillshots,
                              float moveSpeed, float height = 0.0f)
    {
        if (SegmentHitsWall(from, moveTo, height)) return true;
        return CheckMoveToDirection(from, moveTo, skillshots, moveSpeed);
    }

    // Check if path from unit to target has wall collision
    // Ref: EzEvade EvadeHelper.CheckPathCollision
    inline bool CheckPathCollision(const Vec2& unitPos, const Vec2& movePos, float height = 0.0f) {
        return SegmentHitsWall(unitPos, movePos, height);
    }

    // ====================================================================
    // SECTION 4: Distance Helpers
    // ====================================================================

    // Distance to nearest enemy champion from a position
    inline float GetDistanceToChampions(const Vec2& pos) {
        float minDist = FLT_MAX;
        for (const auto& hero : SDK::GameObjects::EnemyHeroes) {
            if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
            float d = hero.GetServerPosition().To2D().Distance(pos);
            if (d < minDist) minDist = d;
        }
        return minDist;
    }

    // Distance to nearest enemy turret from a position
    inline float GetDistanceToTurrets(const Vec2& pos) {
        float minDist = FLT_MAX;
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return minDist;

        for (const auto& turret : SDK::GameObjects::EnemyTurrets) {
            if (!turret.IsValid() || turret.IsDead()) continue;
            float d = turret.GetPosition().To2D().Distance(pos);
            if (d < minDist) minDist = d;
        }
        return minDist;
    }

    // Distance to nearest ally champion from a position
    inline float GetDistanceToAllies(const Vec2& pos) {
        float minDist = FLT_MAX;
        for (const auto& hero : SDK::GameObjects::AllyHeroes) {
            if (!hero.IsValid() || !hero.IsAlive() || hero.IsMe()) continue;
            float d = hero.GetServerPosition().To2D().Distance(pos);
            if (d < minDist) minDist = d;
        }
        return minDist;
    }

    // ====================================================================
    // SECTION 5: Position Generation
    // ====================================================================

    // Generate surrounding positions in expanding rings (spiral sample)
    // Ref: EzEvade Position.GetSurroundingPositions
    inline std::vector<Vec2> GetSurroundingPositions(const Vec2& center,
                                                     int maxPositions = 150,
                                                     int posRadius = 25)
    {
        constexpr double kTwoPi = 6.28318530717958647692;
        std::vector<Vec2> positions;
        positions.reserve(maxPositions);
        int posChecked = 0;
        int radiusIndex = 0;

        while (posChecked < maxPositions) {
            radiusIndex++;
            int curRadius = radiusIndex * (2 * posRadius);
            int curCircleChecks = (int)ceil((kTwoPi * (double)curRadius) / (2.0 * (double)posRadius));

            for (int i = 1; i < curCircleChecks && posChecked < maxPositions; i++) {
                float radians = (float)((kTwoPi / (curCircleChecks - 1)) * i);
                Vec2 p(floorf(center.x + (float)curRadius * cosf(radians)),
                       floorf(center.y + (float)curRadius * sinf(radians)));
                positions.push_back(p);
                posChecked++;
            }
        }
        return positions;
    }

    // Generate "fastest" positions (mouse direction + perpendicular)
    // Ref: EzEvade EvadeHelper.GetFastestPositions
    inline std::vector<Vec2> GetFastestPositions(const Vec2& heroPos, const Vec2& mousePos) {
        std::vector<Vec2> positions;
        Vec2 dir = (mousePos - heroPos).Normalized();
        Vec2 pDir = dir.Perpendicular();

        positions.push_back(heroPos + dir * 50.0f);
        positions.push_back(heroPos + dir * 120.0f);
        positions.push_back(heroPos + dir * 220.0f);
        positions.push_back(heroPos + dir * 320.0f);
        positions.push_back(heroPos + pDir * 150.0f);
        positions.push_back(heroPos - pDir * 150.0f);
        // Backward positions for extra options
        positions.push_back(heroPos - dir * 100.0f);
        positions.push_back(heroPos - dir * 200.0f);
        return positions;
    }

    // ====================================================================
    // SECTION 6: Position Scoring
    // ====================================================================

    // Calculate composite score for a position (lower = better)
    // Ref: EzEvade Position.GetPositionValue + GetEnemyPositionValue
    inline float GetPositionScore(const Vec2& pos, const Vec2& mousePos,
                                  float turretRange = 875.0f,
                                  bool preventNearEnemy = true,
                                  float minComfortZone = 550.0f)
    {
        float score = pos.Distance(mousePos);  // Prefer positions closer to mouse

        // Penalty for being close to enemy turrets
        float distToTurrets = GetDistanceToTurrets(pos);
        float turretDangerRange = turretRange + BoundingRadius;
        if (distToTurrets < turretDangerRange) {
            score += 5.0f * (turretDangerRange - distToTurrets);
        }

        // Penalty for being close to enemies
        if (preventNearEnemy) {
            float distToChamps = GetDistanceToChampions(pos);
            if (distToChamps < minComfortZone) {
                score += 2.0f * (minComfortZone - distToChamps);
            }
        }

        // Small bonus for being near allies
        float distToAllies = GetDistanceToAllies(pos);
        if (distToAllies < 800.0f) {
            score -= 0.5f * (800.0f - distToAllies);
        }

        return score;
    }

    // ====================================================================
    // SECTION 7: Evade Point Finding
    // ====================================================================

    // Evaluate a single candidate position
    // Ref: EzEvade EvadeHelper.CanHeroWalkToPos
    inline PositionInfo EvaluatePosition(const Vec2& pos,
                                         const Vec2& heroPos,
                                         const Vec2& mousePos,
                                         const std::vector<SkillshotPolygon>& skillshots,
                                         float moveSpeed, float delayMs,
                                         float gameTime, float extraDist = 0.0f)
    {
        PositionInfo info;
        info.Position = pos;
        info.DistanceToPlayer = heroPos.Distance(pos);
        info.DistanceToMouse = pos.Distance(mousePos);

        // Calculate projected position (where hero will be after delay)
        Vec2 walkDir = (pos - heroPos).Normalized();
        Vec2 projectedPos = heroPos + walkDir * moveSpeed * (delayMs / 1000.0f);

        // Check each skillshot
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;

            bool hitAtProjected = ss.InSkillShot(projectedPos, BoundingRadius + extraDist);
            bool hitAtDest = ss.InSkillShot(pos, BoundingRadius + extraDist);

            if (hitAtProjected || hitAtDest) {
                info.PosDangerLevel = std::max(info.PosDangerLevel, ss.DangerLevel);
                info.PosDangerCount += ss.DangerLevel;
                info.UndodgeableSpells.push_back(ss.SpellId);
            } else {
                info.DodgeableSpells.push_back(ss.SpellId);
            }

            // Track closest distance to any skillshot edge
            float closest = ss.DistanceToDanger(projectedPos);
            if (closest < info.ClosestDistance) {
                info.ClosestDistance = std::max(0.0f, closest);
            }
        }

        info.IsDangerousPos = info.PosDangerCount > 0;
        info.PosDistToChamps = GetDistanceToChampions(pos);
        info.PosDistToTurrets = GetDistanceToTurrets(pos);
        info.PosDistToAllies = GetDistanceToAllies(pos);
        info.Score = GetPositionScore(pos, mousePos);

        return info;
    }

    // Find the closest point outside all danger polygons
    // Ref: EvadeSharp Evader.GetClosestOutsidePoint
    inline Vec2 GetClosestOutsidePoint(const Vec2& from,
                                       const std::vector<SkillshotPolygon>& skillshots)
    {
        Vec2 best;
        float bestDist = FLT_MAX;

        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            const auto& poly = ss.EvadePolygon;
            int n = (int)poly.Points.size();

            for (int i = 0; i < n; i++) {
                auto proj = SDK::GeometryAdv::ProjectOn(
                    from, poly.Points[i], poly.Points[(i + 1) % n]);
                float dist = from.Distance(proj.SegmentPoint);
                if (dist < bestDist && IsSafePoint(proj.SegmentPoint, skillshots)) {
                    bestDist = dist;
                    best = proj.SegmentPoint;
                }
            }
        }
        return best;
    }

    // Extend a safe position further away from danger
    // Ref: EzEvade EvadeHelper.GetExtendedSafePosition
    inline Vec2 GetExtendedSafePosition(const Vec2& from, const Vec2& toward,
                                        const std::vector<SkillshotPolygon>& skillshots,
                                        float extraDistance)
    {
        if (extraDistance <= 0.0f) return toward;

        Vec2 cur = toward;
        Vec2 dir = (toward - from).Normalized();
        for (int i = 0; i < 6; i++) {
            Vec2 next = cur + dir * (extraDistance / 6.0f);
            if (CheckDangerousPos(next, skillshots, 0.0f)) break;
            cur = next;
        }
        return cur;
    }

    // Get evade points from polygon edge walking
    // Ref: EvadeSharp Evader.GetEvadePoints
    inline std::vector<PositionInfo> GetEvadePointsFromEdges(
        const Vec2& playerPos,
        const Vec2& mousePos,
        const std::vector<SkillshotPolygon>& skillshots,
        float moveSpeed, float delayMs, float gameTime,
        float extraDist = 0.0f)
    {
        std::vector<PositionInfo> candidates;
        if (skillshots.empty()) return candidates;

        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            const auto& poly = ss.EvadePolygon;
            int n = (int)poly.Points.size();
            if (n < 3) continue;

            for (int i = 0; i < n; i++) {
                Vec2 sideStart = poly.Points[i];
                Vec2 sideEnd = poly.Points[(i + 1) % n];

                auto proj = SDK::GeometryAdv::ProjectOn(playerPos, sideStart, sideEnd);
                Vec2 originalCandidate = proj.SegmentPoint;
                float distSqr = playerPos.DistanceSqr(originalCandidate);
                if (distSqr > MaxEvadeRange * MaxEvadeRange) continue;

                Vec2 edgeDir = (sideEnd - sideStart).Normalized();
                float sideDistSqr = sideStart.DistanceSqr(sideEnd);
                int numSamples = (distSqr < 200 * 200 && sideDistSqr > 90 * 90)
                    ? DiagonalEvadePointsCount : 0;

                for (int j = -numSamples; j <= numSamples; j++) {
                    Vec2 candidate = originalCandidate + edgeDir * (j * DiagonalEvadePointsStep);

                    // Must be a safe point and walkable
                    if (!IsSafePoint(candidate, skillshots)) continue;
                    if (!IsWalkablePoint(candidate)) continue;

                    PositionInfo info = EvaluatePosition(
                        candidate, playerPos, mousePos, skillshots,
                        moveSpeed, delayMs, gameTime, extraDist);

                    if (!info.IsDangerousPos) {
                        candidates.push_back(info);
                    }
                }
            }
        }
        return candidates;
    }

    // ====================================================================
    // SECTION 8: Best Position Finding (Complete)
    // ====================================================================

    // GetBestPosition — Find best dodge position (walk)
    // Ref: EzEvade EvadeHelper.GetBestPosition + scoring
    inline PositionInfo GetBestPosition(
        const Vec2& heroPos,
        const Vec2& mousePos,
        const std::vector<SkillshotPolygon>& skillshots,
        float moveSpeed, float gameTime,
        float extraDelayBuffer = 65.0f,
        float extraEvadeDistance = 100.0f,
        float extraDist = 10.0f,
        float gamePing = 30.0f,
        bool higherPrecision = false)
    {
        float totalDelay = extraDelayBuffer + gamePing;
        int maxPositions = higherPrecision ? 150 : 50;
        int posRadius = higherPrecision ? 25 : 50;

        std::vector<PositionInfo> candidates;
        candidates.reserve(maxPositions + 16);

        // Phase A: Fast positions (mouse direction + perpendicular)
        for (const auto& p : GetFastestPositions(heroPos, mousePos)) {
            if (!IsWalkablePoint(p)) continue;
            PositionInfo info = EvaluatePosition(
                p, heroPos, mousePos, skillshots, moveSpeed, totalDelay, gameTime, extraDist);
            info.HasExtraDistance = extraEvadeDistance > 0.0f
                && CheckDangerousPos(p, skillshots, extraEvadeDistance);
            candidates.push_back(info);
        }

        // Phase B: Surrounding positions (radial sweep)
        for (const auto& p : GetSurroundingPositions(heroPos, maxPositions, posRadius)) {
            if (!IsWalkablePoint(p)) continue;
            PositionInfo info = EvaluatePosition(
                p, heroPos, mousePos, skillshots, moveSpeed, totalDelay, gameTime, extraDist);
            info.HasExtraDistance = extraEvadeDistance > 0.0f
                && CheckDangerousPos(p, skillshots, extraEvadeDistance);
            candidates.push_back(info);
        }

        // Phase C: Edge walking positions (from polygon edges)
        auto edgeCandidates = GetEvadePointsFromEdges(
            heroPos, mousePos, skillshots, moveSpeed, totalDelay, gameTime, extraDist);
        candidates.insert(candidates.end(), edgeCandidates.begin(), edgeCandidates.end());

        // Sort: safest first, then closest to mouse
        std::sort(candidates.begin(), candidates.end(),
            [](const PositionInfo& a, const PositionInfo& b) {
                if (a.RejectPosition != b.RejectPosition) return !a.RejectPosition;
                if (a.PosDangerLevel != b.PosDangerLevel) return a.PosDangerLevel < b.PosDangerLevel;
                if (a.PosDangerCount != b.PosDangerCount) return a.PosDangerCount < b.PosDangerCount;
                if (a.HasExtraDistance != b.HasExtraDistance) return !a.HasExtraDistance;
                return a.Score < b.Score;
            });

        // Pick best valid candidate
        for (auto& posInfo : candidates) {
            // Reject if path to position hits wall
            if (CheckPathCollision(heroPos, posInfo.Position)) {
                posInfo.RejectPosition = true;
                continue;
            }

            // Verify path is safe
            auto pathCheck = IsSafePath(heroPos, posInfo.Position, skillshots,
                                        moveSpeed, totalDelay, gameTime);
            if (!pathCheck.IsSafe && posInfo.PosDangerLevel == 0) {
                continue; // Path crosses danger but destination is safe — skip
            }

            // Extend safe position if needed
            if (extraEvadeDistance > 0.0f
                && CheckDangerousPos(posInfo.Position, skillshots, extraEvadeDistance)) {
                posInfo.Position = GetExtendedSafePosition(
                    heroPos, posInfo.Position, skillshots, extraEvadeDistance);
            }

            return posInfo;
        }

        // No safe position found
        PositionInfo undodgeable;
        undodgeable.Position = heroPos;
        undodgeable.IsDangerousPos = true;
        undodgeable.PosDangerLevel = 5;
        return undodgeable;
    }

    // GetBestPositionBlink — Find best blink destination
    // Ref: EzEvade EvadeHelper.GetBestPositionBlink
    inline PositionInfo GetBestPositionBlink(
        const Vec2& heroPos,
        const Vec2& mousePos,
        const std::vector<SkillshotPolygon>& skillshots,
        float blinkRange, float gameTime,
        float minComfortZone = 550.0f)
    {
        std::vector<PositionInfo> candidates;

        for (const auto& p : GetSurroundingPositions(heroPos, 100, 50)) {
            float dist = heroPos.Distance(p);
            if (dist > blinkRange) continue;
            if (!IsWalkablePoint(p)) continue;

            PositionInfo info;
            info.Position = p;
            info.IsDangerousPos = CheckDangerousPos(p, skillshots, 6.0f);
            info.HasExtraDistance = CheckDangerousPos(p, skillshots, 100.0f);
            info.DistanceToMouse = p.Distance(mousePos);
            info.PosDistToChamps = GetDistanceToChampions(p);
            info.Score = GetPositionScore(p, mousePos);

            if (info.PosDistToChamps >= minComfortZone) {
                candidates.push_back(info);
            }
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const PositionInfo& a, const PositionInfo& b) {
                if (a.IsDangerousPos != b.IsDangerousPos) return !a.IsDangerousPos;
                if (a.HasExtraDistance != b.HasExtraDistance) return !a.HasExtraDistance;
                return a.Score < b.Score;
            });

        for (auto& info : candidates) {
            if (!CheckPathCollision(heroPos, info.Position)) {
                return info;
            }
        }

        PositionInfo empty;
        empty.IsDangerousPos = true;
        return empty;
    }

    // GetBestPositionDash — Find best position for dash spells
    // Ref: EzEvade EvadeHelper.GetBestPositionDash
    inline PositionInfo GetBestPositionDash(
        const Vec2& heroPos,
        const Vec2& mousePos,
        const std::vector<SkillshotPolygon>& skillshots,
        float dashSpeed, float dashRange, bool fixedRange,
        float moveSpeed, float gameTime,
        float extraDelayBuffer = 65.0f, float gamePing = 30.0f)
    {
        float totalDelay = extraDelayBuffer + gamePing;
        float minRange = fixedRange ? dashRange : 50.0f;
        float maxRange = dashRange;

        std::vector<PositionInfo> candidates;

        for (const auto& p : GetSurroundingPositions(heroPos, 120, 50)) {
            float dist = heroPos.Distance(p);
            if (dist < minRange || dist > maxRange) continue;
            if (!IsWalkablePoint(p)) continue;

            PositionInfo info = EvaluatePosition(
                p, heroPos, mousePos, skillshots, dashSpeed, totalDelay, gameTime);
            info.HasExtraDistance = CheckDangerousPos(p, skillshots, 100.0f);
            candidates.push_back(info);
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const PositionInfo& a, const PositionInfo& b) {
                if (a.IsDangerousPos != b.IsDangerousPos) return !a.IsDangerousPos;
                if (a.PosDangerLevel != b.PosDangerLevel) return a.PosDangerLevel < b.PosDangerLevel;
                if (a.PosDangerCount != b.PosDangerCount) return a.PosDangerCount < b.PosDangerCount;
                if (a.HasExtraDistance != b.HasExtraDistance) return !a.HasExtraDistance;
                return a.Score < b.Score;
            });

        for (auto& info : candidates) {
            if (!CheckPathCollision(heroPos, info.Position)) {
                return info;
            }
        }

        PositionInfo empty;
        empty.IsDangerousPos = true;
        return empty;
    }

    // GetBestPositionTargetedDash — Find best target for targeted dash
    // Ref: EzEvade EvadeHelper.GetBestPositionTargetedDash
    inline PositionInfo GetBestPositionTargetedDash(
        const Vec2& heroPos,
        const Vec2& mousePos,
        const std::vector<SkillshotPolygon>& skillshots,
        float dashRange, float moveSpeed, float gameTime,
        float gamePing = 30.0f)
    {
        PositionInfo best;
        bool found = false;

        auto tryCandidate = [&](const SDK::GameObject& obj) {
            if (!obj.IsValid() || !obj.IsAlive() || obj.IsDead()) return;
            Vec2 targetPos = obj.GetServerPosition().To2D();
            float dist = heroPos.Distance(targetPos);
            if (dist > dashRange) return;

            PositionInfo info = EvaluatePosition(
                targetPos, heroPos, mousePos, skillshots, moveSpeed, gamePing, gameTime);

            if (!found || info.IsBetterThan(best)) {
                best = info;
                found = true;
            }
        };

        // Try ally champions
        for (const auto& hero : SDK::GameObjects::AllyHeroes) {
            if (!hero.IsMe()) tryCandidate(hero);
        }
        // Try ally minions
        for (const auto& minion : SDK::GameObjects::AllyMinions) {
            tryCandidate(minion);
        }
        // Try enemy minions (for offensive dashes like Lee Sin Q)
        for (const auto& minion : SDK::GameObjects::EnemyMinions) {
            tryCandidate(minion);
        }

        if (found) return best;

        PositionInfo empty;
        empty.IsDangerousPos = true;
        return empty;
    }

    // GetBestPositionMovementBlock — Find best position when blocking movement
    // Used when player tries to walk INTO a skillshot
    // Ref: EzEvade EvadeHelper.GetBestPositionMovementBlock
    inline PositionInfo GetBestPositionMovementBlock(
        const Vec2& heroPos,
        const Vec2& movePos,
        const std::vector<SkillshotPolygon>& skillshots,
        float moveSpeed, float gameTime,
        float extraDelayBuffer = 65.0f, float gamePing = 30.0f)
    {
        float totalDelay = extraDelayBuffer + gamePing;

        std::vector<PositionInfo> candidates;

        for (const auto& p : GetSurroundingPositions(heroPos, 64, 50)) {
            if (!IsWalkablePoint(p)) continue;

            PositionInfo info = EvaluatePosition(
                p, heroPos, movePos, skillshots, moveSpeed, totalDelay, gameTime);
            info.HasExtraDistance = CheckDangerousPos(p, skillshots, 50.0f);
            // Use movePos instead of mousePos for distance scoring
            info.DistanceToMouse = p.Distance(movePos);
            candidates.push_back(info);
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const PositionInfo& a, const PositionInfo& b) {
                if (a.IsDangerousPos != b.IsDangerousPos) return !a.IsDangerousPos;
                if (a.PosDangerLevel != b.PosDangerLevel) return a.PosDangerLevel < b.PosDangerLevel;
                if (a.HasExtraDistance != b.HasExtraDistance) return !a.HasExtraDistance;
                return a.DistanceToMouse < b.DistanceToMouse;
            });

        for (auto& info : candidates) {
            if (!CheckPathCollision(heroPos, info.Position)) {
                return info;
            }
        }

        PositionInfo empty;
        return empty;
    }

    // ====================================================================
    // SECTION 9: Polygon Merge (Simple — no ClipperLib dependency)
    // ====================================================================

    // Merge overlapping polygons using convex hull
    // This is a simplified alternative to ClipperLib polygon union.
    // It works well for evade because skillshot polygons are mostly convex.
    inline SDK::Polygon MergePolygons(const std::vector<SDK::Polygon>& polygons) {
        // Collect all points from all polygons
        std::vector<Vec2> allPoints;
        for (const auto& poly : polygons) {
            allPoints.insert(allPoints.end(), poly.Points.begin(), poly.Points.end());
        }
        if (allPoints.empty()) return SDK::Polygon();

        // Compute convex hull of all points
        auto hullPoints = SDK::GeometryAdv::ConvexHull(allPoints);

        SDK::Polygon merged;
        merged.Points = hullPoints;
        return merged;
    }

    // Group overlapping skillshots and merge their polygons
    inline std::vector<SDK::Polygon> GetMergedDangerPolygons(
        const std::vector<SkillshotPolygon>& skillshots)
    {
        std::vector<SDK::Polygon> result;

        // Simple approach: group skillshots whose polygons overlap,
        // then merge each group. For now, just collect all evade polygons.
        // Full ClipperLib integration would do proper boolean union.
        std::vector<SDK::Polygon> activePolygons;
        for (const auto& ss : skillshots) {
            if (ss.IsActive) activePolygons.push_back(ss.EvadePolygon);
        }

        if (activePolygons.empty()) return result;

        // Check which polygons overlap (share any intersecting edge)
        std::vector<bool> merged(activePolygons.size(), false);
        for (size_t i = 0; i < activePolygons.size(); i++) {
            if (merged[i]) continue;

            std::vector<SDK::Polygon> group;
            group.push_back(activePolygons[i]);
            merged[i] = true;

            // Find all polygons that overlap with this group
            bool foundNew = true;
            while (foundNew) {
                foundNew = false;
                for (size_t j = 0; j < activePolygons.size(); j++) {
                    if (merged[j]) continue;
                    for (const auto& gp : group) {
                        if (gp.Intersects(activePolygons[j])) {
                            group.push_back(activePolygons[j]);
                            merged[j] = true;
                            foundNew = true;
                            break;
                        }
                    }
                }
            }

            if (group.size() == 1) {
                result.push_back(group[0]);
            } else {
                result.push_back(MergePolygons(group));
            }
        }

        return result;
    }

    // ====================================================================
    // SECTION 10: Utility Functions
    // ====================================================================

    // Remove expired skillshots from the active list
    inline void RemoveExpired(std::vector<SkillshotPolygon>& skillshots, float gameTime) {
        skillshots.erase(
            std::remove_if(skillshots.begin(), skillshots.end(),
                [gameTime](const SkillshotPolygon& ss) { return ss.IsExpired(gameTime); }),
            skillshots.end());
    }

    // Update all missile positions for moving spells
    inline void UpdateAllMissilePositions(std::vector<SkillshotPolygon>& skillshots, float gameTime) {
        for (auto& ss : skillshots) {
            ss.UpdateMissilePosition(gameTime);
        }
    }

    // Count the number of active skillshots
    inline int CountActiveSkillshots(const std::vector<SkillshotPolygon>& skillshots) {
        int count = 0;
        for (const auto& ss : skillshots) {
            if (ss.IsActive) count++;
        }
        return count;
    }

    // Get total danger from all active skillshots at player position
    inline int GetTotalDanger(const Vec2& playerPos,
                              const std::vector<SkillshotPolygon>& skillshots)
    {
        int total = 0;
        for (const auto& ss : skillshots) {
            if (!ss.IsActive) continue;
            if (ss.InSkillShot(playerPos, BoundingRadius)) {
                total += ss.DangerLevel;
            }
        }
        return total;
    }

} // namespace EvadeGeometry
} // namespace Evade
