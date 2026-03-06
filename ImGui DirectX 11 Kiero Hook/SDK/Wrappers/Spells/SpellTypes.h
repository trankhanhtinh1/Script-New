#pragma once
// ============================================================================
// SpellTypes.h — Skillshot Type Subclasses (EnsoulSharp SDK Port)
// ============================================================================
// Full port of EnsoulSharp.SDK/Core/Wrappers/Spells/SpellTypes/*
//
// Classes:
//   - Skillshot (base)        — Common skillshot tracking & polygon
//   - SkillshotLine           — Line skillshot (Ezreal Q, Nidalee Q)
//   - SkillshotCircle         — Circle skillshot (Ziggs Q, Brand W)
//   - SkillshotCone           — Cone skillshot (Annie W, Cho'Gath W)
//   - SkillshotMissileLine    — Line with missile (Morgana Q, Jinx W)
//   - SkillshotMissileCircle  — Circle with missile (Lulu E)
//   - SkillshotRing           — Ring skillshot (Veigar E)
//   - SkillshotArc            — Arc skillshot (Diana Q)
//
// Used by: Evade system, spell tracking, danger zone visualization
// ============================================================================

#include "Enums.h"
#include "SpellDatabaseEntry.h"
#include "Polygon.h"
#include "Game.h"
#include "GameObject.h"
#include "GameObjects.h"
#include "EventSystem.h"
#include "SpellDatabase.h"
#include "core/Vector.h"
#include <cmath>
#include <algorithm>
#include <memory>
#include <unordered_set>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SDK {

// ============================================================================
// DetectionType — How a skillshot was detected
// Reference: EnsoulSharp.SDK SpellTypes/DetectionType
// ============================================================================
enum class DetectionType {
    ProcessSpell,     // Detected via OnProcessSpellCast
    MissileCreate,    // Detected via missile creation
    RecvPacket,       // Detected via network packet (not used in internal)
    Unknown
};

// ============================================================================
// Skillshot — Base class for all skillshot types
// ============================================================================
// Represents an active skillshot in the game world.
// Tracks position, generates danger polygon, and checks if positions are safe.
//
// Source: EnsoulSharp.SDK/Core/Wrappers/Spells/SpellTypes/ (base functionality)
// ============================================================================
class Skillshot {
public:
    // ---- Identity ----
    SpellType Type;                 // The specific skillshot type
    SpellDatabaseEntry SpellData;   // Full spell database entry
    std::string SpellName;          // Spell name for quick reference
    std::string CasterName;         // Champion who cast this

    // ---- Geometry ----
    Vec2 StartPosition;     // Where the spell was cast from
    Vec2 EndPosition;       // Target/end position
    Vec2 Direction;         // Normalized direction
    Vec2 MissilePosition;   // Current missile position (for missile types)

    // ---- Timing ----
    float StartTime = 0.0f;        // Game time when spell was cast
    float EndTime = 0.0f;          // Game time when spell expires
    float CastDelay = 0.0f;        // Cast delay in seconds

    // ---- Properties ----
    float Speed = 0.0f;            // Missile speed (units/sec), 0 = instant
    float Range = 0.0f;            // Spell range
    float Width = 0.0f;            // Spell width/radius
    float Radius = 0.0f;           // Circle radius (alias)
    float Angle = 0.0f;            // Cone angle (radians)
    float RingWidth = 0.0f;        // Ring inner->outer width
    int DangerLevel = 1;           // 1-5 danger rating
    bool IsDangerous = false;

    // ---- Missile Physics (EnsoulSharp: MissileAccel, MissileMinSpeed, MissileMaxSpeed) ----
    float MissileAccel = 0.0f;     // Missile acceleration (units/sec²), 0 = constant speed
    float MissileMinSpeed = 0.0f;  // Minimum missile speed (if accelerating)
    float MissileMaxSpeed = 0.0f;  // Maximum missile speed
    bool  MissileFollowsCaster = false; // Missile stays relative to caster (Viktor R, etc.)
    float CurrentSpeed = 0.0f;     // Computed current missile speed (with accel)

    // ---- Detection ----
    DetectionType Detection = DetectionType::Unknown;  // How this was detected
    unsigned int CasterNetId = 0;   // Network ID of caster (for dedup & tracking)
    unsigned int MissileNetId = 0;  // Network ID of missile object (for matching)

    // ---- State ----
    bool IsActive = true;          // Is the skillshot still active?
    bool IsMissile = false;        // Does this type have a visible missile?
    int CollisionFlags = 0;        // CollisionableObjects flags

    // ---- Danger Zone ----
    Polygon DangerPolygon;          // Current danger polygon
    Polygon EvadePolygon;           // Slightly expanded polygon for safe evade

    // ====================================================================
    // Constructors
    // ====================================================================
    Skillshot() = default;
    virtual ~Skillshot() = default;

    Skillshot(const SpellDatabaseEntry& data, const Vec2& start, const Vec2& end,
              const std::string& caster = "")
    {
        SpellData = data;
        SpellName = data.SpellName;
        CasterName = caster;
        Type = data.Type;
        StartPosition = start;
        EndPosition = end;
        StartTime = Game::GetTime();
        Speed = static_cast<float>(data.MissileSpeed);
        CurrentSpeed = Speed;
        Range = static_cast<float>(data.Range);
        Width = static_cast<float>(data.GetRealWidth());
        Radius = static_cast<float>(data.Radius);
        Angle = static_cast<float>(data.Angle) * static_cast<float>(M_PI) / 180.0f;
        RingWidth = static_cast<float>(data.RingRadius);
        CastDelay = data.GetDelayInSeconds();
        DangerLevel = data.DangerValue;
        IsDangerous = data.IsDangerous;
        CollisionFlags = data.CollisionObjects;

        // Missile physics from SpellDatabaseEntry
        MissileAccel = static_cast<float>(data.MissileAccel);
        MissileMinSpeed = static_cast<float>(data.MissileMinSpeed);
        MissileMaxSpeed = static_cast<float>(data.MissileMaxSpeed);
        MissileFollowsCaster = data.MissileFollowsCaster;

        // Calculate direction
        Vec2 diff = EndPosition - StartPosition;
        float len = diff.Length();
        Direction = (len > 0) ? Vec2(diff.x / len, diff.y / len) : Vec2(0, 1);

        // Calculate end time
        if (Speed > 0) {
            float travelTime = Range / Speed;
            EndTime = StartTime + CastDelay + travelTime;
        } else {
            EndTime = StartTime + CastDelay + 0.5f; // Instant spells last ~0.5s
        }

        // Initial missile position = start
        MissilePosition = StartPosition;

        // Build initial polygon
        UpdatePolygon();
    }

    // ====================================================================
    // Factory — Create appropriate subclass from SpellDatabaseEntry
    // ====================================================================
    static std::shared_ptr<Skillshot> Create(const SpellDatabaseEntry& data,
                                              const Vec2& start, const Vec2& end,
                                              const std::string& caster = "");

    // ====================================================================
    // Update — Called each frame to update missile position & polygon
    // ====================================================================
    virtual void Update() {
        if (!IsActive) return;

        float now = Game::GetTime();

        // Check expired
        if (now > EndTime) {
            IsActive = false;
            return;
        }

        // Update missile position for missile-type skillshots
        if (IsMissile && Speed > 0) {
            float elapsed = now - StartTime - CastDelay;
            if (elapsed > 0) {
                float dist = 0.0f;

                // MissileAccel support (EnsoulSharp: accelerating/decelerating missiles)
                if (MissileAccel != 0.0f) {
                    // v(t) = v0 + a*t, clamped to [min, max]
                    CurrentSpeed = Speed + MissileAccel * elapsed;
                    if (MissileMaxSpeed > 0) CurrentSpeed = std::min(CurrentSpeed, MissileMaxSpeed);
                    if (MissileMinSpeed > 0) CurrentSpeed = std::max(CurrentSpeed, MissileMinSpeed);

                    // s = v0*t + 0.5*a*t^2 (with clamping approximation)
                    dist = Speed * elapsed + 0.5f * MissileAccel * elapsed * elapsed;
                    if (dist < 0) dist = 0;
                } else {
                    dist = elapsed * Speed;
                }

                // MissileFollowsCaster: update start position to caster's current position
                if (MissileFollowsCaster && CasterNetId != 0) {
                    for (auto& hero : GameObjects::EnemyHeroes) {
                        if (hero.IsValid() && hero.GetNetId() == CasterNetId) {
                            Vec2 newStart = hero.GetPosition().To2D();
                            // Shift end position relative to new start
                            Vec2 offset = newStart - StartPosition;
                            StartPosition = newStart;
                            EndPosition = EndPosition + offset;
                            break;
                        }
                    }
                }

                MissilePosition = StartPosition + Direction * dist;

                // Cap at max range
                float maxDist = Range;
                if ((MissilePosition - StartPosition).Length() > maxDist) {
                    MissilePosition = StartPosition + Direction * maxDist;
                    IsActive = false;
                }
            }
        }

        // Rebuild polygon
        UpdatePolygon();
    }

    // ====================================================================
    // UpdatePolygon — Virtual, overridden by subclasses
    // ====================================================================
    virtual void UpdatePolygon() {
        // Default: rectangle from start to end
        RectanglePoly rect(StartPosition, EndPosition, Width);
        DangerPolygon.Points = rect.Points;
        BuildEvadePolygon();
    }

    // ====================================================================
    // Build evade polygon (slightly expanded danger polygon)
    // ====================================================================
    void BuildEvadePolygon(float extraWidth = 15.0f) {
        EvadePolygon.Points.clear();
        // Simple expansion: offset each point outward from center
        if (DangerPolygon.Points.empty()) return;

        Vec2 center = DangerPolygon.Center();
        for (auto& pt : DangerPolygon.Points) {
            Vec2 dir = pt - center;
            float len = dir.Length();
            if (len > 0) {
                Vec2 expanded = pt + Vec2(dir.x / len, dir.y / len) * extraWidth;
                EvadePolygon.Points.push_back(expanded);
            } else {
                EvadePolygon.Points.push_back(pt);
            }
        }
    }

    // ====================================================================
    // Hit Detection
    // ====================================================================

    /// Is a position inside the danger zone? (with optional unit radius)
    virtual bool IsDangerousTo(const Vec2& position, float unitRadius = 65.0f) const {
        if (!IsActive) return false;

        // Check if point is inside danger polygon (with unit radius buffer)
        // Quick check: distance to polygon edge vs unit radius
        if (DangerPolygon.Points.empty()) return false;

        // For circles/cones, use polygon check
        if (DangerPolygon.IsInside(position)) return true;

        // Also check if within unitRadius of any polygon edge
        if (unitRadius > 0) {
            float distToPoly = DangerPolygon.DistanceToEdge(position);
            return distToPoly <= unitRadius;
        }

        return false;
    }

    /// Is a GameObject in danger?
    bool IsDangerousTo(const GameObject& unit, float extraRadius = 0.0f) const {
        if (!unit.IsValid()) return false;
        Vec2 pos = unit.GetPosition().To2D();
        float radius = unit.GetBoundingRadius() + extraRadius;
        return IsDangerousTo(pos, radius);
    }

    /// Is the local player in danger?
    bool IsDangerousToMe(float extraRadius = 0.0f) const {
        return IsDangerousTo(GameObjects::Player, extraRadius);
    }

    // ====================================================================
    // Safe Position — Find nearest safe position outside polygon
    // ====================================================================
    Vec2 GetSafePosition(const Vec2& currentPos, float unitRadius = 65.0f) const {
        if (!IsDangerousTo(currentPos, unitRadius))
            return currentPos; // Already safe

        // Find closest point on polygon perimeter and move outward
        Vec2 closest = DangerPolygon.ClosestPointOnEdge(currentPos);
        Vec2 dir = closest - DangerPolygon.Center();
        float len = dir.Length();
        if (len > 0) {
            dir = Vec2(dir.x / len, dir.y / len);
            return closest + dir * (unitRadius + 20.0f);
        }

        // Fallback: move away from start
        Vec2 awayDir = currentPos - StartPosition;
        len = awayDir.Length();
        if (len > 0) {
            awayDir = Vec2(awayDir.x / len, awayDir.y / len);
            return currentPos + awayDir * 200.0f;
        }

        return currentPos;
    }

    // ====================================================================
    // Missile Position — Get current missile position at a given time
    // ====================================================================
    Vec2 GetMissilePositionAt(float time) const {
        if (Speed <= 0) return EndPosition;

        float elapsed = time - StartTime - CastDelay;
        if (elapsed <= 0) return StartPosition;

        float dist = 0.0f;
        if (MissileAccel != 0.0f) {
            // s = v0*t + 0.5*a*t^2
            dist = Speed * elapsed + 0.5f * MissileAccel * elapsed * elapsed;
            if (dist < 0) dist = 0;
        } else {
            dist = elapsed * Speed;
        }

        if (dist >= Range) return StartPosition + Direction * Range;
        return StartPosition + Direction * dist;
    }

    // ====================================================================
    // Time to hit — Estimate time until skillshot reaches a position
    // ====================================================================
    float GetTimeToHit(const Vec2& position) const {
        if (Speed <= 0) {
            // Instant: hits at StartTime + CastDelay
            float hitTime = StartTime + CastDelay;
            float now = Game::GetTime();
            return (hitTime > now) ? (hitTime - now) : 0.0f;
        }

        // Missile: time = distance / speed
        float dist = (position - StartPosition).Length();
        return CastDelay + dist / Speed;
    }

    // ====================================================================
    // Is Expired?
    // ====================================================================
    bool IsExpired() const {
        return !IsActive || Game::GetTime() > EndTime;
    }

    // ====================================================================
    // Remaining time before expiry
    // ====================================================================
    float GetRemainingTime() const {
        float remaining = EndTime - Game::GetTime();
        return (remaining > 0) ? remaining : 0.0f;
    }

    // ====================================================================
    // Get the effective width for collision checks
    // ====================================================================
    virtual float GetEffectiveWidth() const { return Width; }

};


// ============================================================================
// SkillshotLine — Line skillshot (Ezreal Q, Nidalee Q)
// ============================================================================
// Creates a rectangle danger zone from start to end.
// Width = spell width.
//
// Source: EnsoulSharp.SDK SpellTypes/SkillshotLine.cs
// ============================================================================
class SkillshotLine : public Skillshot {
public:
    SkillshotLine() { IsMissile = false; }

    SkillshotLine(const SpellDatabaseEntry& data, const Vec2& start, const Vec2& end,
                  const std::string& caster = "")
        : Skillshot(data, start, end, caster)
    {
        IsMissile = false;
        UpdatePolygon();
    }

    void UpdatePolygon() override {
        // Line skillshot = Rectangle from start to end (or start + direction * range)
        Vec2 actualEnd = EndPosition;
        float dist = (EndPosition - StartPosition).Length();
        if (dist > Range) {
            actualEnd = StartPosition + Direction * Range;
        } else if (dist < Range && SpellData.FixedRange) {
            actualEnd = StartPosition + Direction * Range;
        }

        RectanglePoly rect(StartPosition, actualEnd, Width);
        DangerPolygon.Points = rect.Points;
        BuildEvadePolygon();
    }

    float GetEffectiveWidth() const override { return Width; }
};


// ============================================================================
// SkillshotCircle — Circle skillshot (Ziggs Q, Brand W, Lux E)
// ============================================================================
// Creates a circle danger zone at the end position.
//
// Source: EnsoulSharp.SDK SpellTypes/SkillshotCircle.cs
// ============================================================================
class SkillshotCircle : public Skillshot {
public:
    SkillshotCircle() { IsMissile = false; }

    SkillshotCircle(const SpellDatabaseEntry& data, const Vec2& start, const Vec2& end,
                    const std::string& caster = "")
        : Skillshot(data, start, end, caster)
    {
        IsMissile = false;
        // For circle, Radius is the key metric
        if (Radius <= 0) Radius = Width;
        UpdatePolygon();
    }

    void UpdatePolygon() override {
        // Circle at end position with given radius
        CirclePoly circle(EndPosition, Radius, 22);
        DangerPolygon.Points = circle.Points;
        BuildEvadePolygon();
    }

    bool IsDangerousTo(const Vec2& position, float unitRadius = 65.0f) const override {
        if (!IsActive) return false;
        // Simple distance check for circles (more efficient)
        float dist = (position - EndPosition).Length();
        return dist <= (Radius + unitRadius);
    }

    float GetEffectiveWidth() const override { return Radius; }
};


// ============================================================================
// SkillshotCone — Cone skillshot (Annie W, Cho'Gath W, Miss Fortune R)
// ============================================================================
// Creates a sector/cone danger zone from start position.
//
// Source: EnsoulSharp.SDK SpellTypes/SkillshotCone.cs
// ============================================================================
class SkillshotCone : public Skillshot {
public:
    SkillshotCone() { IsMissile = false; }

    SkillshotCone(const SpellDatabaseEntry& data, const Vec2& start, const Vec2& end,
                  const std::string& caster = "")
        : Skillshot(data, start, end, caster)
    {
        IsMissile = false;
        if (Angle <= 0) Angle = static_cast<float>(M_PI) / 4.0f; // 45 degrees default
        UpdatePolygon();
    }

    void UpdatePolygon() override {
        // Sector/Cone from start in direction of end
        SectorPoly sector(StartPosition, EndPosition, Angle, Range, 22);
        DangerPolygon.Points = sector.Points;
        BuildEvadePolygon();
    }

    bool IsDangerousTo(const Vec2& position, float unitRadius = 65.0f) const override {
        if (!IsActive) return false;

        Vec2 toPos = position - StartPosition;
        float dist = toPos.Length();

        // Check range
        if (dist > Range + unitRadius) return false;

        // Check angle
        if (dist > 0) {
            Vec2 normalizedToPos(toPos.x / dist, toPos.y / dist);
            // Dot product for angle check
            float dot = Direction.x * normalizedToPos.x + Direction.y * normalizedToPos.y;
            float angleBetween = std::acos(std::clamp(dot, -1.0f, 1.0f));
            return angleBetween <= (Angle / 2.0f) + std::atan2(unitRadius, dist);
        }

        return true; // At start position = always hit
    }

    float GetEffectiveWidth() const override { return Range; }
};


// ============================================================================
// SkillshotMissileLine — Line skillshot WITH visible missile (Morgana Q, Jinx W)
// ============================================================================
// Like SkillshotLine but the danger zone follows the missile position.
// The danger zone is a rectangle from missile current position to missile end.
//
// Source: EnsoulSharp.SDK SpellTypes/SkillshotMissileLine.cs
// ============================================================================
class SkillshotMissileLine : public Skillshot {
public:
    SkillshotMissileLine() { IsMissile = true; }

    SkillshotMissileLine(const SpellDatabaseEntry& data, const Vec2& start, const Vec2& end,
                         const std::string& caster = "")
        : Skillshot(data, start, end, caster)
    {
        IsMissile = true;
        UpdatePolygon();
    }

    void UpdatePolygon() override {
        // Rectangle from current missile position to end of range
        Vec2 missileEnd = StartPosition + Direction * Range;

        // During cast delay, missile hasn't started yet
        float now = Game::GetTime();
        if (now < StartTime + CastDelay) {
            // Full line from start
            RectanglePoly rect(StartPosition, missileEnd, Width);
            DangerPolygon.Points = rect.Points;
        } else {
            // From current missile position forward
            RectanglePoly rect(MissilePosition, missileEnd, Width);
            DangerPolygon.Points = rect.Points;
        }

        BuildEvadePolygon();
    }

    float GetEffectiveWidth() const override { return Width; }
};


// ============================================================================
// SkillshotMissileCircle — Circle skillshot WITH missile travel (Lulu E, Galio E)
// ============================================================================
// Missile travels to a point, then explodes as a circle.
//
// Source: EnsoulSharp.SDK SpellTypes/SkillshotMissileCircle.cs
// ============================================================================
class SkillshotMissileCircle : public Skillshot {
public:
    bool HasLanded = false;    // Has the missile reached the end?

    SkillshotMissileCircle() { IsMissile = true; }

    SkillshotMissileCircle(const SpellDatabaseEntry& data, const Vec2& start, const Vec2& end,
                           const std::string& caster = "")
        : Skillshot(data, start, end, caster)
    {
        IsMissile = true;
        HasLanded = false;
        if (Radius <= 0) Radius = Width;
        UpdatePolygon();
    }

    void Update() override {
        Skillshot::Update();

        if (!IsActive) return;

        // Check if missile has reached the end point
        float now = Game::GetTime();
        float elapsed = now - StartTime - CastDelay;
        if (Speed > 0 && elapsed > 0) {
            float dist = elapsed * Speed;
            float totalDist = (EndPosition - StartPosition).Length();
            if (dist >= totalDist) {
                HasLanded = true;
                MissilePosition = EndPosition;
            }
        } else if (Speed <= 0) {
            HasLanded = true;
        }
    }

    void UpdatePolygon() override {
        // Circle at end position (whether missile has landed or not, the impact zone is the danger)
        CirclePoly circle(EndPosition, Radius, 22);
        DangerPolygon.Points = circle.Points;
        BuildEvadePolygon();
    }

    bool IsDangerousTo(const Vec2& position, float unitRadius = 65.0f) const override {
        if (!IsActive) return false;

        // For missile circle: danger is at the end position circle
        float dist = (position - EndPosition).Length();
        return dist <= (Radius + unitRadius);
    }

    float GetEffectiveWidth() const override { return Radius; }
};


// ============================================================================
// SkillshotRing — Ring skillshot (Veigar E)
// ============================================================================
// Creates a ring (donut) danger zone. The inside is safe, the ring band is dangerous.
//
// Source: EnsoulSharp.SDK SpellTypes/SkillshotRing.cs
// ============================================================================
class SkillshotRing : public Skillshot {
public:
    float InnerRadius = 0.0f;
    float OuterRadius = 0.0f;

    SkillshotRing() { IsMissile = false; }

    SkillshotRing(const SpellDatabaseEntry& data, const Vec2& start, const Vec2& end,
                  const std::string& caster = "")
        : Skillshot(data, start, end, caster)
    {
        IsMissile = false;
        OuterRadius = static_cast<float>(data.Radius);
        InnerRadius = static_cast<float>(data.RingRadius);
        if (InnerRadius <= 0) InnerRadius = OuterRadius * 0.6f; // Approximate
        if (RingWidth <= 0) RingWidth = OuterRadius - InnerRadius;
        UpdatePolygon();
    }

    void UpdatePolygon() override {
        // Ring at end position
        RingPoly ring(EndPosition, RingWidth, OuterRadius, 22);
        DangerPolygon.Points = ring.Points;
        BuildEvadePolygon();
    }

    bool IsDangerousTo(const Vec2& position, float unitRadius = 65.0f) const override {
        if (!IsActive) return false;

        float dist = (position - EndPosition).Length();

        // Dangerous if within ring band (between inner and outer radius)
        bool inOuter = dist <= (OuterRadius + unitRadius);
        bool outsideInner = dist >= (InnerRadius - unitRadius);

        return inOuter && outsideInner;
    }

    float GetEffectiveWidth() const override { return OuterRadius; }
};


// ============================================================================
// SkillshotArc — Arc skillshot (Diana Q, Xerath E)
// ============================================================================
// Creates an arc-shaped danger zone.
//
// Source: EnsoulSharp.SDK SpellTypes/SkillshotArc.cs
// ============================================================================
class SkillshotArc : public Skillshot {
public:
    SkillshotArc() { IsMissile = true; }

    SkillshotArc(const SpellDatabaseEntry& data, const Vec2& start, const Vec2& end,
                 const std::string& caster = "")
        : Skillshot(data, start, end, caster)
    {
        IsMissile = true;
        if (Angle <= 0) Angle = static_cast<float>(M_PI) / 3.0f; // 60 degrees default
        UpdatePolygon();
    }

    void UpdatePolygon() override {
        // Arc from start to end
        float arcAngle = Angle;
        if (arcAngle <= 0) arcAngle = static_cast<float>(M_PI) / 3.0f;

        ArcPoly arc(StartPosition, EndPosition, arcAngle, Width, 22);
        DangerPolygon.Points = arc.Points;
        BuildEvadePolygon();
    }

    float GetEffectiveWidth() const override { return Width; }
};


// ============================================================================
// Skillshot::Create — Factory method implementation
// ============================================================================
// Creates the correct Skillshot subclass based on SpellDatabaseEntry.Type
// ============================================================================
inline std::shared_ptr<Skillshot> Skillshot::Create(
    const SpellDatabaseEntry& data, const Vec2& start, const Vec2& end,
    const std::string& caster)
{
    switch (data.Type) {
    case SpellType::SkillshotLine:
        return std::make_shared<SkillshotLine>(data, start, end, caster);

    case SpellType::SkillshotCircle:
        return std::make_shared<SkillshotCircle>(data, start, end, caster);

    case SpellType::SkillshotCone:
    case SpellType::SkillshotMissileCone:
        return std::make_shared<SkillshotCone>(data, start, end, caster);

    case SpellType::SkillshotMissileLine:
        return std::make_shared<SkillshotMissileLine>(data, start, end, caster);

    case SpellType::SkillshotMissileCircle:
        return std::make_shared<SkillshotMissileCircle>(data, start, end, caster);

    case SpellType::SkillshotRing:
        return std::make_shared<SkillshotRing>(data, start, end, caster);

    case SpellType::SkillshotArc:
    case SpellType::SkillshotMissileArc:
        return std::make_shared<SkillshotArc>(data, start, end, caster);

    default:
        // Fallback to line for unknown types
        return std::make_shared<SkillshotLine>(data, start, end, caster);
    }
}


// ============================================================================
// SkillshotTracker — Manages all active skillshots in the game
// ============================================================================
// Detects new skillshots from spell casts & missiles, tracks active ones,
// removes expired ones. Core system for Evade.
//
// Usage:
//   SkillshotTracker::Update();  // each frame
//   bool danger = SkillshotTracker::IsPositionDangerous(myPos);
//   auto safe = SkillshotTracker::GetSafePosition(myPos);
// ============================================================================
class SkillshotTracker {
public:
    // ====================================================================
    // Active skillshots list
    // ====================================================================
    static inline std::vector<std::shared_ptr<Skillshot>> ActiveSkillshots;

    // Deduplication: track (casterNetId, spellName, startTime) to avoid duplicates
    // from both ProcessSpell and MissileCreate detecting the same cast
    static inline std::unordered_set<std::string> s_dedupKeys;

    static inline bool s_initialized = false;

    // ====================================================================
    // Init — Subscribe to EventSystem for auto-detection
    // Reference: EnsoulSharp SkillshotTracker auto-detects via game events
    // ====================================================================
    static void Init() {
        if (s_initialized) return;
        s_initialized = true;

        // Auto-detect skillshots from ProcessSpellCast
        EventSystem::OnProcessSpellCast([](const SpellCastArgs& args) {
            OnProcessSpellCast(args);
        });

        // Auto-detect from missile creation
        EventSystem::OnMissileCreated([](const MissileArgs& args) {
            OnMissileCreate(args);
        });

        // Remove on missile delete
        EventSystem::OnMissileDeleted([](const MissileArgs& args) {
            OnMissileDelete(args);
        });
    }

    // ====================================================================
    // Update — Call each frame
    // ====================================================================
    static void Update() {
        // Update existing skillshots
        for (auto& ss : ActiveSkillshots) {
            if (ss && ss->IsActive) {
                ss->Update();
            }
        }

        // Remove expired/inactive
        ActiveSkillshots.erase(
            std::remove_if(ActiveSkillshots.begin(), ActiveSkillshots.end(),
                [](const std::shared_ptr<Skillshot>& ss) {
                    return !ss || !ss->IsActive || ss->IsExpired();
                }),
            ActiveSkillshots.end()
        );

        // Clean old dedup keys (older than 5s)
        // Simple: clear every 60s to avoid unbounded growth
        static float lastCleanup = 0.0f;
        float now = Game::GetTime();
        if (now - lastCleanup > 60.0f) {
            s_dedupKeys.clear();
            lastCleanup = now;
        }
    }

    // ====================================================================
    // Add a skillshot to tracking (with deduplication)
    // ====================================================================
    static void AddSkillshot(const SpellDatabaseEntry& data,
                              const Vec2& start, const Vec2& end,
                              const std::string& caster = "",
                              DetectionType detection = DetectionType::Unknown,
                              unsigned int casterNetId = 0) {
        // Dedup check: same caster + spell within 0.5s
        std::string key = caster + "_" + data.SpellName + "_" +
            std::to_string(static_cast<int>(Game::GetTime() * 2)); // 0.5s granularity
        if (s_dedupKeys.count(key) > 0) return;
        s_dedupKeys.insert(key);

        auto ss = Skillshot::Create(data, start, end, caster);
        if (ss) {
            ss->Detection = detection;
            ss->CasterNetId = casterNetId;
            ActiveSkillshots.push_back(ss);
        }
    }

    // ====================================================================
    // Add a pre-created skillshot (no dedup)
    // ====================================================================
    static void AddSkillshot(std::shared_ptr<Skillshot> ss) {
        if (ss) ActiveSkillshots.push_back(ss);
    }

    // ====================================================================
    // Remove skillshot by spell name
    // ====================================================================
    static void RemoveByName(const std::string& spellName) {
        ActiveSkillshots.erase(
            std::remove_if(ActiveSkillshots.begin(), ActiveSkillshots.end(),
                [&](const std::shared_ptr<Skillshot>& ss) {
                    return ss && ss->SpellName == spellName;
                }),
            ActiveSkillshots.end()
        );
    }

    // ====================================================================
    // Remove by missile network ID
    // ====================================================================
    static void RemoveByMissileId(unsigned int missileNetId) {
        ActiveSkillshots.erase(
            std::remove_if(ActiveSkillshots.begin(), ActiveSkillshots.end(),
                [&](const std::shared_ptr<Skillshot>& ss) {
                    return ss && ss->MissileNetId == missileNetId;
                }),
            ActiveSkillshots.end()
        );
    }

    // ====================================================================
    // Clear all
    // ====================================================================
    static void Clear() {
        ActiveSkillshots.clear();
        s_dedupKeys.clear();
    }

    // ====================================================================
    // Query: Is a position dangerous?
    // ====================================================================
    static bool IsPositionDangerous(const Vec2& position, float unitRadius = 65.0f) {
        for (auto& ss : ActiveSkillshots) {
            if (ss && ss->IsActive && ss->IsDangerousTo(position, unitRadius))
                return true;
        }
        return false;
    }

    // ====================================================================
    // Query: Is a position dangerous for a specific unit?
    // ====================================================================
    static bool IsDangerousTo(const GameObject& unit) {
        for (auto& ss : ActiveSkillshots) {
            if (ss && ss->IsActive && ss->IsDangerousTo(unit))
                return true;
        }
        return false;
    }

    // ====================================================================
    // Query: Is the local player in danger?
    // ====================================================================
    static bool IsPlayerInDanger() {
        return IsDangerousTo(GameObjects::Player);
    }

    // ====================================================================
    // Query: Get all skillshots that threaten a position
    // ====================================================================
    static std::vector<std::shared_ptr<Skillshot>> GetThreats(
        const Vec2& position, float unitRadius = 65.0f)
    {
        std::vector<std::shared_ptr<Skillshot>> threats;
        for (auto& ss : ActiveSkillshots) {
            if (ss && ss->IsActive && ss->IsDangerousTo(position, unitRadius))
                threats.push_back(ss);
        }
        return threats;
    }

    // ====================================================================
    // Query: Get highest danger level affecting a position
    // ====================================================================
    static int GetDangerLevel(const Vec2& position, float unitRadius = 65.0f) {
        int maxDanger = 0;
        for (auto& ss : ActiveSkillshots) {
            if (ss && ss->IsActive && ss->IsDangerousTo(position, unitRadius)) {
                maxDanger = (std::max)(maxDanger, ss->DangerLevel);
            }
        }
        return maxDanger;
    }

    // ====================================================================
    // Query: Find safest nearby position (simple evade)
    // ====================================================================
    static Vec2 GetSafePosition(const Vec2& currentPos, float unitRadius = 65.0f,
                                 float searchRange = 400.0f) {
        if (!IsPositionDangerous(currentPos, unitRadius))
            return currentPos; // Already safe

        // Try positions in a circle around current pos
        Vec2 bestPos = currentPos;
        int bestDanger = 999;
        float bestDist = 99999.0f;

        const int steps = 16;
        for (int i = 0; i < steps; i++) {
            float angle = (2.0f * static_cast<float>(M_PI) * i) / steps;
            for (float r = 100.0f; r <= searchRange; r += 100.0f) {
                Vec2 testPos(
                    currentPos.x + std::cos(angle) * r,
                    currentPos.y + std::sin(angle) * r
                );

                if (!IsPositionDangerous(testPos, unitRadius)) {
                    float dist = (testPos - currentPos).Length();
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestPos = testPos;
                        bestDanger = 0;
                    }
                }
            }
        }

        // If no completely safe pos found, find least dangerous
        if (bestDanger > 0) {
            for (int i = 0; i < steps; i++) {
                float angle = (2.0f * static_cast<float>(M_PI) * i) / steps;
                Vec2 testPos(
                    currentPos.x + std::cos(angle) * searchRange,
                    currentPos.y + std::sin(angle) * searchRange
                );

                int danger = GetDangerLevel(testPos, unitRadius);
                if (danger < bestDanger) {
                    bestDanger = danger;
                    bestPos = testPos;
                }
            }
        }

        return bestPos;
    }

    // ====================================================================
    // Query: Count active skillshots
    // ====================================================================
    static int Count() {
        return static_cast<int>(ActiveSkillshots.size());
    }

private:
    // ====================================================================
    // Auto-detect: OnProcessSpellCast handler
    // Looks up spell in SpellDatabase and adds if it's a known skillshot
    // ====================================================================
    static void OnProcessSpellCast(const SpellCastArgs& args) {
        // Only track enemy hero casts
        if (!args.Sender.IsValid() || !args.Sender.IsHero()) return;
        if (args.Sender.GetTeam() == GameObjects::Player.GetTeam()) return;
        if (args.IsAutoAttack) return; // Skip auto attacks

        // Look up in SpellDatabase by name first
        const SpellDatabaseEntry* entry = SpellDatabase::GetByName(args.SpellName);

        // If not found by name, try by champion
        if (!entry) {
            auto champEntries = SpellDatabase::GetByChampion(args.Sender.GetChampionName());
            for (auto* e : champEntries) {
                if (e && e->IsSkillshot() &&
                    _stricmp(e->SpellName.c_str(), args.SpellName.c_str()) == 0) {
                    entry = e;
                    break;
                }
            }
        }

        if (entry && entry->IsSkillshot()) {
            Vec2 start = args.StartPos.To2D();
            Vec2 end = args.EndPos.To2D();

            AddSkillshot(*entry, start, end,
                         args.Sender.GetChampionName(),
                         DetectionType::ProcessSpell,
                         args.Sender.GetNetId());
        }
    }

    // ====================================================================
    // Auto-detect: OnMissileCreate handler
    // Matches missile to spell database entries
    // ====================================================================
    static void OnMissileCreate(const MissileArgs& args) {
        if (!args.MissileObj.IsValid()) return;

        Missile missile(args.MissileObj.address);
        if (!missile.IsValid()) return;

        // Only track enemy missiles — check caster is not on our team
        int casterNetId = args.CasterNetId;
        bool isEnemy = true;
        for (auto& hero : GameObjects::AllyHeroes) {
            if (hero.IsValid() && static_cast<int>(hero.GetNetId()) == casterNetId) {
                isEnemy = false;
                break;
            }
        }
        // Also check local player
        if (static_cast<int>(GameObjects::Player.GetNetId()) == casterNetId) isEnemy = false;
        if (!isEnemy) return;

        std::string missileName = missile.GetMissileName();
        if (missileName.empty()) return;

        // Look up in SpellDatabase by missile name
        const SpellDatabaseEntry* entry = SpellDatabase::GetByMissileName(missileName);
        if (!entry || !entry->IsSkillshot()) return;

        {
            Vec2 start = missile.GetStartPos().To2D();
            Vec2 end = missile.GetEndPos().To2D();

            // Check if already tracked by ProcessSpell (dedup handles this)
            AddSkillshot(*entry, start, end,
                         entry->ChampionName,
                         DetectionType::MissileCreate,
                         static_cast<unsigned int>(missile.GetCasterNetId()));

            // Store missile NetId for later removal
            if (!ActiveSkillshots.empty()) {
                auto& last = ActiveSkillshots.back();
                if (last) last->MissileNetId = args.MissileObj.GetNetworkId();
            }
        }
    }

    // ====================================================================
    // Auto-detect: OnMissileDelete handler
    // Deactivate corresponding skillshot
    // ====================================================================
    static void OnMissileDelete(const MissileArgs& args) {
        if (!args.MissileObj.IsValid()) return;
        unsigned int netId = static_cast<unsigned int>(args.MissileObj.GetNetworkId());
        if (netId == 0) return;

        // Find and deactivate skillshot with matching missile ID
        for (auto& ss : ActiveSkillshots) {
            if (ss && ss->MissileNetId == netId) {
                ss->IsActive = false;
                break;
            }
        }
    }
};

} // namespace SDK
