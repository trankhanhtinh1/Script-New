#pragma once

// ============================================================================
// EvadeHelper.h — Helper functions for skillshot evade (core logic stripped)
//
// Keeps: math, InSkillShot, danger checks, wall checks, spell position,
//        collision prediction, move path checks, evade time, movement cmd.
// Removed: position scoring, candidate generation, best position search,
//          sorting/comparison, strict safe checks.
// ============================================================================

#include "PositionInfo.h"
#include "../Debug/CandidateDebug.h"
#include "../Detection/SpellDetector.h"
#include "../../../SDK/SDK.h"
#include "../../../Core/CoreNavGrid.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <vector>

namespace ZDEvade {

namespace EvadeMath {

inline constexpr float kSmallNum = 0.00000001f;

inline float Dot(const Vec2& u, const Vec2& v) { return u.x * v.x + u.y * v.y; }
inline float Norm(const Vec2& v) { return std::sqrt(Dot(v, v)); }
inline float Dist(const Vec2& u, const Vec2& v) { return Norm(u - v); }

inline Vec2 ProjectOn(const Vec2& point, const Vec2& a, const Vec2& b, bool& isOnSegment) {
    const Vec2 ab = b - a;
    const float lenSqr = Dot(ab, ab);
    if (lenSqr < kSmallNum) {
        isOnSegment = false;
        return a;
    }
    const float t = Dot(point - a, ab) / lenSqr;
    isOnSegment = (t >= 0.0f && t <= 1.0f);
    const float ct = std::clamp(t, 0.0f, 1.0f);
    return a + ab * ct;
}

inline float CpaTime(const Vec2& p1, const Vec2& v1, const Vec2& p2, const Vec2& v2) {
    const Vec2 dv = v1 - v2;
    const float dv2 = Dot(dv, dv);
    if (dv2 < kSmallNum) return 0.0f;
    const Vec2 w0 = p1 - p2;
    return -Dot(w0, dv) / dv2;
}

inline float CpaPointsEx(const Vec2& p1, const Vec2& v1,
                         const Vec2& p2, const Vec2& v2,
                         Vec2& out1, Vec2& out2) {
    const float ctime = std::max(0.0f, CpaTime(p1, v1, p2, v2));
    out1 = p1 + v1 * ctime;
    out2 = p2 + v2 * ctime;
    return Dist(out1, out2);
}

} // namespace EvadeMath

inline bool IsLineType(ZDSpellType t) { return t == ZDSpellType::Line; }
inline bool IsCircleType(ZDSpellType t) { return t == ZDSpellType::Circular; }

class EvadeHelper {
public:
    static constexpr float kEdgeSafetyBuffer = 140.0f;
    static constexpr float kDodgeTargetExtraBuffer = 60.0f;
    static constexpr float kHeadOnBackstepBuffer = 180.0f;
    static constexpr float kDiagonalForwardMin = 90.0f;
    static constexpr float kDiagonalForwardMax = 240.0f;
    static constexpr float kFastLinePathClearance = 55.0f;
    static constexpr float kNormalLinePathClearance = 28.0f;
    static constexpr float kFinalEndpointSafetyBuffer = 30.0f;
    static constexpr float kShortestExitPathStep = 22.0f;
    static constexpr float kShortestExitRadiusStep = 30.0f;
    static constexpr float kShortestExitMinSearchRadius = 650.0f;
    static constexpr float kMinComfortZone = 550.0f;
    static constexpr float kTurretRange = 875.0f;

    enum RejectReason {
        RejectNone = 0,
        RejectWall = 1,
        RejectEndpoint = 2,
        RejectPath = 3,
        RejectCollision = 4,
        RejectLate = 5,
        RejectDanger = 6
    };

    struct CandidateOption {
        Vec2 position = {};
        CandidateSource source = CandidateSource::Unknown;
        CandidateSide side = CandidateSide::None;
        int spellId = -1;
        float escapeDistance = FLT_MAX;
    };

    // ── Candidate debug infrastructure ──
    static inline std::vector<CandidateDebugPoint> candidateDebugPoints;
    static inline int candidateDebugTick = 0;

    static const std::vector<CandidateDebugPoint>& CandidateDebugPoints() { return candidateDebugPoints; }
    static int CandidateDebugTick() { return candidateDebugTick; }
    static void ClearCandidateDebug() { candidateDebugPoints.clear(); candidateDebugTick = 0; }
    static void BeginCandidateDebug() { candidateDebugPoints.clear(); candidateDebugTick = SDK::Variables::TickCount(); }

    // ── Spell position ──
    static Vec2 GetCurrentSpellPosition(const TrackedSpell& spell, int afterTime = 0, bool allowNegative = false) {
        if (spell.Type() == ZDSpellType::Arc) return spell.startPos;
        if (spell.isMissile && spell.missile.IsValid()) return spell.GetMissilePosition(afterTime);
        if (!IsLineType(spell.Type())) {
            return IsCircleType(spell.Type()) || spell.Type() == ZDSpellType::Ring
                ? spell.endPos
                : spell.startPos;
        }
        const float speed = spell.info.projectileSpeed;
        if (speed <= 0.0f || speed >= FLT_MAX * 0.5f) return spell.startPos;
        const int elapsed = SDK::Variables::TickCount() + afterTime - spell.startTime - spell.info.spellDelay;
        if (elapsed < 0 && !allowNegative) return spell.startPos;
        return spell.startPos + spell.direction * speed * (static_cast<float>(elapsed) / 1000.0f);
    }

    // ── Position value helpers ──
    static float GetPositionValue(const Vec2& pos) {
        float value = EvadeMath::Dist(pos, SDK::Game::CursorPos().To2D());
        for (const auto& turret : SDK::ObjectManager::EnemyTurrets()) {
            if (!turret.IsValid() || turret.IsDead()) continue;
            const Vec2 turretPos = turret.Position().To2D();
            if (turretPos.IsZero()) continue;
            const float dist = EvadeMath::Dist(pos, turretPos);
            if (kTurretRange > dist) value += 5.0f * (kTurretRange - dist);
        }
        return value;
    }

    static float GetDistanceToChampions(const Vec2& pos) {
        float minDist = FLT_MAX;
        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (!hero.IsValid() || hero.IsDead() || !hero.IsVisible()) continue;
            const Vec2 heroPos = hero.ServerPosition().To2D();
            if (heroPos.IsZero()) continue;
            minDist = std::min(minDist, EvadeMath::Dist(pos, heroPos));
        }
        return minDist;
    }

    static bool IsNearEnemy(const Vec2& pos, const Vec2& heroPos, float distance) {
        const float curDist = GetDistanceToChampions(heroPos);
        const float posDist = GetDistanceToChampions(pos);
        if (curDist < distance) return curDist > posDist;
        return posDist < distance;
    }

    // ── Skillshot collision checks ──
    static bool InSkillShot(const TrackedSpell& spell, const Vec2& pos, float radius) {
        const ZDSpellType type = spell.Type();
        const float spellRadius = spell.Radius();
        const float hitRange = spellRadius + radius;
        if (IsLineType(type)) {
            Vec2 start = GetCurrentSpellPosition(spell);
            bool onSeg = false;
            const Vec2 proj = EvadeMath::ProjectOn(pos, start, spell.endPos, onSeg);
            if (onSeg && EvadeMath::Dist(proj, pos) <= hitRange) return true;
            if (EvadeMath::Dist(pos, spell.endPos) <= hitRange) return true;
            if (EvadeMath::Dist(pos, start) <= hitRange) return true;
            return false;
        }
        if (type == ZDSpellType::Ring) {
            const float distance = EvadeMath::Dist(pos, spell.endPos);
            return distance <= spellRadius + radius &&
                distance >= std::max(0.0f, spell.InnerRadius() - radius);
        }
        if (IsCircleType(type) || type == ZDSpellType::Cone) {
            return EvadeMath::Dist(pos, spell.endPos) <= spellRadius + radius;
        }
        return false;
    }

    static int CheckDangerousPos(const Vec2& pos, float extraBuffer, float boundingRadius) {
        int dangerLevel = 0;
        for (const auto& s : SpellDetector::ActiveSpells()) {
            if (InSkillShot(s, pos, boundingRadius + extraBuffer))
                dangerLevel += std::max(1, s.DangerValue());
        }
        return dangerLevel;
    }

    static int CountDangerousPosInList(const Vec2& pos, float extraBuffer, float boundingRadius,
                                       const std::vector<TrackedSpell>& spells) {
        int dangerLevel = 0;
        for (const auto& s : spells) {
            if (InSkillShot(s, pos, boundingRadius + extraBuffer)) dangerLevel += std::max(1, s.DangerValue());
        }
        return dangerLevel;
    }

    // ── Spell timing ──
    static float SpellHitTime(const TrackedSpell& spell, const Vec2& pos) {
        const int now = SDK::Variables::TickCount();
        if (IsLineType(spell.Type())) {
            const float speed = spell.MissileSpeed();
            if (speed >= FLT_MAX * 0.5f) {
                return std::max(
                    0.0f,
                    static_cast<float>(
                        TickDifference(spell.endTime, now) -
                        static_cast<std::int64_t>(SDK::Game::Ping())));
            }
            const Vec2 head = GetCurrentSpellPosition(spell, SDK::Game::Ping(), true);
            return (EvadeMath::Dist(head, pos) / std::max(1.0f, speed)) * 1000.0f;
        }
        return std::max(
            0.0f,
            static_cast<float>(
                TickDifference(spell.endTime, now) -
                static_cast<std::int64_t>(SDK::Game::Ping())));
    }

    static float GetClosestDistanceApproach(const TrackedSpell& spell, const Vec2& pos,
                                            float speed, float delayMs,
                                            const Vec2& heroPos, float boundingRadius,
                                            float extraDist) {
        const Vec2 walkDelta = pos - heroPos;
        const float walkLen = EvadeMath::Norm(walkDelta);
        const Vec2 walkDir = walkLen >= 1.0f ? walkDelta * (1.0f / walkLen) : Vec2(0.0f, 0.0f);
        const ZDSpellType type = spell.Type();
        const float spellRadius = spell.Radius();
        if (IsLineType(type)) {
            const float missileSpeed = std::max(1.0f, spell.MissileSpeed());
            Vec2 spellPos = GetCurrentSpellPosition(spell, static_cast<int>(delayMs), true);
            const Vec2 spellEnd = spell.endPos;
            const Vec2 spellVel = spell.direction * missileSpeed;
            Vec2 cHero, cSpell;
            const float cpa = EvadeMath::CpaPointsEx(heroPos, walkDir * speed, spellPos, spellVel, cHero, cSpell);
            bool heroOn = true, spellOn = false;
            if (walkLen >= 1.0f) {
                const Vec2 extendedPos = pos + walkDir * (boundingRadius + speed * delayMs / 1000.0f);
                EvadeMath::ProjectOn(cHero, heroPos, extendedPos, heroOn);
            }
            EvadeMath::ProjectOn(cSpell, spellPos, spellEnd, spellOn);
            const float checkDist = boundingRadius + spellRadius + extraDist;
            if (spellOn && heroOn) return std::max(0.0f, cpa - checkDist);
            return checkDist;
        }
        if (IsCircleType(type)) {
            const int now = SDK::Variables::TickCount();
            const float hitTime = std::max(
                0.0f,
                static_cast<float>(TickDifference(
                    SaturatingTickAdd(spell.startTime, spell.Delay()), now)) -
                    delayMs);
            const float walkRange = EvadeMath::Dist(heroPos, pos);
            const float predictedRange = speed * (hitTime / 1000.0f);
            const Vec2 tHeroPos = heroPos + walkDir * std::min(predictedRange, walkRange);
            return std::max(0.0f, EvadeMath::Dist(tHeroPos, spell.endPos) - (spellRadius + extraDist));
        }
        return 1.0f;
    }

    static bool PredictSpellCollision(const TrackedSpell& spell, const Vec2& pos, float speed,
                                      float delayMs, const Vec2& heroPos, float boundingRadius,
                                      float extraDist) {
        return GetClosestDistanceApproach(spell, pos, speed, delayMs, heroPos, boundingRadius, extraDist + 10.0f) == 0.0f;
    }

    // ── Wall check ──
    static bool WallBlocks(const Vec2& heroPos, const Vec2& pos, float planeY) {
        const Vec3 dest = Vec3::From2D(pos, planeY);
        const Vec3 from = Vec3::From2D(heroPos, planeY);
        if (!CoreNavGrid::IsWalkable(dest)) return true;
        return CoreNavGrid::IsWallBetween(from, dest);
    }

    // ── Path checks ──
    static bool CheckMoveToDirection(const Vec2& from, const Vec2& to, float boundingRadius) {
        const float dist = EvadeMath::Dist(from, to);
        if (dist < 1.0f) return false;
        const int steps = std::max(2, static_cast<int>(dist / 50.0f));
        for (int i = 1; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const Vec2 point = from + (to - from) * t;
            for (const auto& spell : SpellDetector::ActiveSpells()) {
                if (InSkillShot(spell, point, boundingRadius)) return true;
            }
        }
        return false;
    }

    static bool PathExitInfo(const Vec2& from, const Vec2& to, float boundingRadius,
                             float extraBuffer, const std::vector<TrackedSpell>& spells,
                             float& exitDistance) {
        const float dist = EvadeMath::Dist(from, to);
        exitDistance = dist;
        if (dist < 1.0f) return CountDangerousPosInList(to, extraBuffer, boundingRadius, spells) == 0;
        const int steps = std::max(2, static_cast<int>(std::ceil(dist / kShortestExitPathStep)));
        bool foundSafe = false;
        for (int i = 1; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const Vec2 point = from + (to - from) * t;
            const bool safe = CountDangerousPosInList(point, extraBuffer, boundingRadius, spells) == 0;
            if (safe) {
                if (!foundSafe) {
                    foundSafe = true;
                    exitDistance = dist * t;
                }
            } else if (foundSafe) {
                return false;
            }
        }
        return foundSafe;
    }

    static bool CheckMovePath(const Vec2& movePos, float delayMs,
                              const Vec2& heroPos, float boundingRadius,
                              const std::vector<TrackedSpell>& spells, float moveSpeed) {
        (void)delayMs;
        (void)moveSpeed;
        if (CheckMoveToDirection(heroPos, movePos, boundingRadius)) return true;
        for (const auto& s : spells) {
            if (InSkillShot(s, movePos, boundingRadius) ||
                PredictSpellCollision(s, movePos, moveSpeed, delayMs, heroPos, boundingRadius, 0.0f))
                return true;
        }
        return false;
    }

    // ── Fastest escape position for a single spell ──
    static Vec2 GetFastestPosition(const TrackedSpell& spell, const Vec2& heroPos,
                                   float boundingRadius) {
        if (IsLineType(spell.Type())) {
            const Vec2 start = spell.startPos;
            bool onSeg = false;
            const Vec2 proj = EvadeMath::ProjectOn(heroPos, start, spell.endPos, onSeg);
            Vec2 dir = heroPos - proj;
            const float len = EvadeMath::Norm(dir);
            if (len < 1.0f) {
                dir = Vec2(-spell.direction.y, spell.direction.x);
            } else {
                dir = dir * (1.0f / len);
            }
            return proj + dir * (spell.Radius() + boundingRadius + kFinalEndpointSafetyBuffer);
        }
        if (IsCircleType(spell.Type())) {
            Vec2 dir = heroPos - spell.endPos;
            const float len = EvadeMath::Norm(dir);
            if (len < 1.0f) dir = Vec2(1.0f, 0.0f);
            else dir = dir * (1.0f / len);
            return spell.endPos + dir * (spell.Radius() + kFinalEndpointSafetyBuffer);
        }
        if (spell.Type() == ZDSpellType::Ring) {
            Vec2 dir = heroPos - spell.endPos;
            const float distance = EvadeMath::Norm(dir);
            if (distance < 1.0f) dir = Vec2(1.0f, 0.0f);
            else dir = dir * (1.0f / distance);
            const float inner = std::max(
                0.0f,
                spell.InnerRadius() - boundingRadius - kFinalEndpointSafetyBuffer);
            const float outer = spell.Radius() + boundingRadius + kFinalEndpointSafetyBuffer;
            return spell.endPos + dir * (
                inner > 0.0f && distance - inner <= outer - distance ? inner : outer);
        }
        return Vec2(0.0f, 0.0f);
    }

    // ── Evade time calculations ──
    static float GetEvadeTime(const TrackedSpell& spell, const Vec2& heroPos,
                              float boundingRadius, float moveSpeed) {
        const float speed = std::max(50.0f, moveSpeed);
        if (IsLineType(spell.Type())) {
            bool onSeg = false;
            const Vec2 proj = EvadeMath::ProjectOn(heroPos, spell.startPos, spell.endPos, onSeg);
            const float escapeDist = spell.Radius() - EvadeMath::Dist(heroPos, proj) + boundingRadius;
            return 1000.0f * std::max(0.0f, escapeDist) / speed;
        }
        if (IsCircleType(spell.Type())) {
            const float escapeDist = spell.Radius() - EvadeMath::Dist(heroPos, spell.endPos) + boundingRadius;
            return 1000.0f * std::max(0.0f, escapeDist) / speed;
        }
        if (spell.Type() == ZDSpellType::Ring) {
            const float distance = EvadeMath::Dist(heroPos, spell.endPos);
            const float inner = std::max(0.0f, spell.InnerRadius() - boundingRadius);
            const float outer = spell.Radius() + boundingRadius;
            if (distance < inner || distance > outer) return 0.0f;
            return 1000.0f * std::min(distance - inner, outer - distance) / speed;
        }
        return 0.0f;
    }

    static float GetLowestEvadeTime(const std::vector<TrackedSpell>& spells,
                                    const Vec2& heroPos, float boundingRadius,
                                    float moveSpeed) {
        float lowest = FLT_MAX;
        for (const auto& s : spells) {
            const float hitTime = SpellHitTime(s, heroPos);
            const float evadeTime = GetEvadeTime(s, heroPos, boundingRadius, moveSpeed);
            if (hitTime < FLT_MAX && hitTime - evadeTime < lowest) lowest = hitTime - evadeTime;
        }
        return lowest;
    }
};

} // namespace ZDEvade
