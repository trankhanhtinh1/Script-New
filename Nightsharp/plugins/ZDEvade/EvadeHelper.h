#pragma once

#include "PositionInfo.h"
#include "SpellDetector.h"
#include "../../SDK/SDK.h"
#include "../../Core/CoreNavGrid.h"

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
    static constexpr float kEdgeSafetyBuffer = 100.0f;
    static constexpr float kDodgeTargetExtraBuffer = 60.0f;
    static constexpr float kHeadOnBackstepBuffer = 180.0f;
    static inline bool fastEvadeMode = false;
    static constexpr float kMinComfortZone = 550.0f;
    static constexpr float kTurretRange = 875.0f;

    static Vec2 GetCurrentSpellPosition(const TrackedSpell& spell, int afterTime = 0, bool allowNegative = false) {
        if (spell.isMissile && spell.missile.IsValid()) return spell.GetMissilePosition(afterTime);
        if (!IsLineType(spell.Type()) && spell.Type() != ZDSpellType::Arc) {
            return IsCircleType(spell.Type()) ? spell.endPos : spell.startPos;
        }
        const float speed = spell.info.projectileSpeed;
        if (speed <= 0.0f || speed >= FLT_MAX * 0.5f) return spell.startPos;
        const int elapsed = SDK::Variables::TickCount() + afterTime - spell.startTime - spell.info.spellDelay;
        if (elapsed < 0 && !allowNegative) return spell.startPos;
        return spell.startPos + spell.direction * speed * (static_cast<float>(elapsed) / 1000.0f);
    }

    // GetPositionValue: cursor distance plus turret/enemy penalty.
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

    // GetDistanceToChampions.
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

    // IsNearEnemy.
    static bool IsNearEnemy(const Vec2& pos, const Vec2& heroPos, float distance) {
        const float curDist = GetDistanceToChampions(heroPos);
        const float posDist = GetDistanceToChampions(pos);
        if (curDist < distance) return curDist > posDist;
        return posDist < distance;
    }

    // CheckMoveToDirection: path crossing check.
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

    static bool InSkillShot(const TrackedSpell& spell, const Vec2& pos, float radius) {
        const ZDSpellType type = spell.Type();
        const float spellRadius = spell.Radius();
        if (IsLineType(type)) {
            // Use current missile position as start — not the original cast position.
            // The line only covers from where the missile is now
            // to the end position. Positions behind the missile head are safe.
            Vec2 start = GetCurrentSpellPosition(spell);
            bool onSeg = false;
            const Vec2 proj = EvadeMath::ProjectOn(pos, start, spell.endPos, onSeg);
            return onSeg && EvadeMath::Dist(proj, pos) <= spellRadius + radius;
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

    static float SpellHitTime(const TrackedSpell& spell, const Vec2& pos) {
        const int now = SDK::Variables::TickCount();
        if (IsLineType(spell.Type())) {
            const float speed = spell.MissileSpeed();
            if (speed >= FLT_MAX * 0.5f) {
                return std::max(0.0f, static_cast<float>(spell.endTime - now - SDK::Game::Ping()));
            }
            const Vec2 head = GetCurrentSpellPosition(spell, SDK::Game::Ping(), true);
            return (EvadeMath::Dist(head, pos) / std::max(1.0f, speed)) * 1000.0f;
        }
        return std::max(0.0f, static_cast<float>(spell.endTime - now - SDK::Game::Ping()));
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
            const float hitTime = std::max(0.0f, static_cast<float>(spell.startTime + spell.Delay() - now) - delayMs);
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

    // ScorePosition: walkability and position-info scoring.
    static PositionInfo ScorePosition(const Vec2& pos, float speed, float delayMs, float extraDist,
                                      const Vec2& heroPos, float boundingRadius, float planeY,
                                      const std::vector<TrackedSpell>& spells) {
        PositionInfo info;
        info.position = pos;
        bool wall = WallBlocks(heroPos, pos, planeY);

        for (const auto& s : spells) {
            int danger = std::max(1, s.DangerValue());
            const float cpaDist = GetClosestDistanceApproach(s, pos, speed, delayMs,
                                                             heroPos, boundingRadius, extraDist);
            info.closestDistance = std::min(info.closestDistance, cpaDist);
            if (InSkillShot(s, pos, std::max(0.0f, boundingRadius - 8.0f)) ||
                PredictSpellCollision(s, pos, speed, delayMs, heroPos, boundingRadius, extraDist) ||
                (!IsLineType(s.Type()) && IsNearEnemy(pos, heroPos, kMinComfortZone))) {
                info.posDangerLevel = std::max(info.posDangerLevel, danger);
                info.posDangerCount += danger;
            }
        }

        info.isDangerousPos = CheckDangerousPos(pos, 6.0f, boundingRadius) > 0 || wall;
        // When close to enemy, prefer positions that are further from spell source
        // (backward escape > sideways when hero is near source)
        float distToMouse = GetPositionValue(pos);
        // Add travel distance penalty: positions closer to hero are faster to reach
        const float travelDist = EvadeMath::Dist(heroPos, pos);
        // Blend: 70% cursor direction + 30% travel distance (shorter = better)
        info.distanceToMouse = distToMouse + travelDist * 0.3f;
        if (wall) info.rejectPosition = true;
        return info;
    }

    // GetFastestPosition: perpendicular escape point for a spell.
    static Vec2 GetFastestPosition(const TrackedSpell& spell, const Vec2& heroPos,
                                   float boundingRadius) {
        if (IsLineType(spell.Type())) {
            // Project hero onto the spell line, extend from projection through hero
            // by radius + boundingRadius + 10 — shortest path out of the line
            const Vec2 start = spell.startPos;
            bool onSeg = false;
            const Vec2 proj = EvadeMath::ProjectOn(heroPos, start, spell.endPos, onSeg);
            Vec2 dir = heroPos - proj;
            const float len = EvadeMath::Norm(dir);
            if (len < 1.0f) {
                // Hero exactly on line center — pick perpendicular of spell direction
                dir = Vec2(-spell.direction.y, spell.direction.x);
            } else {
                dir = dir * (1.0f / len);
            }
            return proj + dir * (spell.Radius() + boundingRadius + 10.0f);
        }
        if (IsCircleType(spell.Type())) {
            // Extend from circle center through hero by radius + 10
            Vec2 dir = heroPos - spell.endPos;
            const float len = EvadeMath::Norm(dir);
            if (len < 1.0f) dir = Vec2(1.0f, 0.0f);
            else dir = dir * (1.0f / len);
            return spell.endPos + dir * (spell.Radius() + 10.0f);
        }
        return Vec2(0.0f, 0.0f);
    }

    static void AddSafestPathCandidates(const TrackedSpell& spell, const Vec2& heroPos,
                                        float boundingRadius, float extraDist,
                                        float evadeDistance, std::vector<Vec2>& positions) {
        if (IsLineType(spell.Type())) {
            const Vec2 start = spell.startPos;
            bool onSeg = false;
            const Vec2 proj = EvadeMath::ProjectOn(heroPos, start, spell.endPos, onSeg);
            const Vec2 perp(-spell.direction.y, spell.direction.x);
            const float heroDistToLine = EvadeMath::Dist(heroPos, proj);
            // Adaptive: actual escape distance from hero to spell boundary + buffer
            // When hero is near the edge, this is much shorter than fixed distance
            const float spellEdge = spell.Radius() + boundingRadius;
            const float escapeDist = std::max(spellEdge - heroDistToLine, 0.0f) + extraDist + kEdgeSafetyBuffer;
            const float minDist = spellEdge + extraDist + kEdgeSafetyBuffer;
            const float dist = std::max(escapeDist, minDist);
            const float farDist = dist + std::clamp(evadeDistance, 50.0f, 180.0f);
            // Perpendicular candidates (both sides)
            positions.push_back(proj + perp * dist);
            positions.push_back(proj - perp * dist);
            positions.push_back(proj + perp * farDist);
            positions.push_back(proj - perp * farDist);
            // Backward escape: move away from spell source when hero is close
            const float distToSource = EvadeMath::Dist(heroPos, start);
            if (distToSource < 600.0f) {
                const float backstep = std::max(kHeadOnBackstepBuffer, spellEdge * 0.5f);
                positions.push_back(heroPos - spell.direction * backstep);
                positions.push_back(heroPos - spell.direction * (backstep + std::clamp(evadeDistance, 50.0f, 160.0f)));
            }
            // Closest boundary point: project hero onto spell boundary (shortest escape)
            if (heroDistToLine > 1.0f) {
                Vec2 toHero = heroPos - proj;
                const float len = EvadeMath::Norm(toHero);
                if (len > 0.1f) {
                    toHero = toHero * (1.0f / len);
                    positions.push_back(proj + toHero * (spellEdge + extraDist + 10.0f));
                }
            }
            return;
        }
        // Circle/Cone: fastest perpendicular + backward from spell center
        const Vec2 fastest = GetFastestPosition(spell, heroPos, boundingRadius);
        if (fastest.x != 0.0f || fastest.y != 0.0f) {
            positions.push_back(fastest);
            Vec2 dir = fastest - heroPos;
            const float len = EvadeMath::Norm(dir);
            if (len > 1.0f) positions.push_back(fastest + dir * (std::clamp(evadeDistance, 50.0f, 160.0f) / len));
        }
        // Backward from spell center when hero is close
        if (IsCircleType(spell.Type())) {
            const float distToCenter = EvadeMath::Dist(heroPos, spell.endPos);
            if (distToCenter < spell.Radius() + 300.0f) {
                Vec2 awayDir = heroPos - spell.endPos;
                const float awayLen = EvadeMath::Norm(awayDir);
                if (awayLen > 1.0f) {
                    awayDir = awayDir * (1.0f / awayLen);
                    const float escapeDist = spell.Radius() + boundingRadius + extraDist + kEdgeSafetyBuffer;
                    positions.push_back(spell.endPos + awayDir * escapeDist);
                }
            }
        }
    }

    static void AddHeadOnPathCandidates(const TrackedSpell& spell, const Vec2& heroPos,
                                         const Vec2& movePos, float boundingRadius,
                                         float extraDist, std::vector<Vec2>& positions) {
        if (!IsLineType(spell.Type())) return;
        Vec2 moveDir = movePos - heroPos;
        const float moveLen = EvadeMath::Norm(moveDir);
        if (moveLen < 1.0f) return;
        moveDir = moveDir * (1.0f / moveLen);
        if (EvadeMath::Dot(moveDir, spell.direction) > -0.25f) return;
        const Vec2 start = spell.startPos;
        bool onSeg = false;
        const Vec2 proj = EvadeMath::ProjectOn(heroPos, start, spell.endPos, onSeg);
        const Vec2 perp(-spell.direction.y, spell.direction.x);
        const float sideDist = spell.Radius() + boundingRadius + extraDist + kEdgeSafetyBuffer + kDodgeTargetExtraBuffer;
        const float backDist = std::max(kHeadOnBackstepBuffer, sideDist * 0.45f);
        const Vec2 back = spell.direction * -backDist;
        positions.push_back(proj + perp * sideDist + back);
        positions.push_back(proj - perp * sideDist + back);
    }

    // ── GetExtendedSafePosition: push dodge position further from skillshot edge ──
    // Tries forward extension first, then perpendicular to nearest spell if blocked
    static Vec2 GetExtendedSafePosition(const Vec2& from, const Vec2& to,
                                        float extendDistance, float boundingRadius,
                                        float planeY,
                                        const std::vector<TrackedSpell>& spells) {
        if (extendDistance <= 0.0f) return to;
        Vec2 dir = to - from;
        const float len = EvadeMath::Norm(dir);
        if (len < 1.0f) return to;
        dir = dir * (1.0f / len);

        // Forward extension
        Vec2 lastSafe = to;
        const float step = 50.0f;
        for (float d = step; d <= extendDistance; d += step) {
            const Vec2 candidate = to + dir * d;
            bool dangerous = false;
            for (const auto& s : spells) {
                if (InSkillShot(s, candidate, boundingRadius + kEdgeSafetyBuffer)) { dangerous = true; break; }
            }
            if (dangerous || WallBlocks(from, candidate, planeY)) break;
            lastSafe = candidate;
        }
        // If forward extension made progress, use it
        if (EvadeMath::Dist(lastSafe, to) > 1.0f) return lastSafe;

        // Forward blocked: try perpendicular to nearest spell direction
        const TrackedSpell* nearest = nullptr;
        float nearestDist = FLT_MAX;
        for (const auto& s : spells) {
            const float d = EvadeMath::Dist(from, s.startPos);
            if (d < nearestDist) { nearestDist = d; nearest = &s; }
        }
        if (nearest && IsLineType(nearest->Type())) {
            const Vec2 perp(-nearest->direction.y, nearest->direction.x);
            for (int side = 0; side < 2; ++side) {
                const Vec2 pdir = side == 0 ? perp : perp * -1.0f;
                Vec2 perpSafe = to;
                for (float d = step; d <= extendDistance; d += step) {
                    const Vec2 candidate = to + pdir * d;
                    bool dangerous = false;
                    for (const auto& s : spells) {
                        if (InSkillShot(s, candidate, boundingRadius + kEdgeSafetyBuffer)) { dangerous = true; break; }
                    }
                    if (dangerous || WallBlocks(from, candidate, planeY)) break;
                    perpSafe = candidate;
                }
                if (EvadeMath::Dist(perpSafe, to) > EvadeMath::Dist(lastSafe, to))
                    lastSafe = perpSafe;
            }
        }
        return lastSafe;
    }

    // GetBestPosition: ring search for the safest position.
    static bool GetBestPosition(const SDK::AIHeroClient& player, const Vec2& heroPos,
                                float boundingRadius,
                                const std::vector<TrackedSpell>& spells,
                                Vec2& out, float extraDelay, float extraDist,
                                float evadeDistance = 0.0f,
                                int maxPosToCheck = 150, int posRadius = 50) {
        const float speed = std::max(50.0f, player.MoveSpeed());
        const float delayMs = extraDelay + static_cast<float>(SDK::Game::Ping());
        const float planeY = player.ServerPosition().y;
        const float lowestEvadeTime = GetLowestEvadeTime(spells, heroPos, boundingRadius, speed);
        fastEvadeMode = lowestEvadeTime < FLT_MAX &&
            65.0f + static_cast<float>(SDK::Game::Ping()) + extraDelay > lowestEvadeTime;

        std::vector<PositionInfo> posTable;
        std::vector<PositionInfo> directTable;

        for (const auto& s : spells) {
            std::vector<Vec2> candidates;
            AddSafestPathCandidates(s, heroPos, boundingRadius, extraDist, evadeDistance, candidates);
            for (const auto& candidate : candidates) {
                PositionInfo info = ScorePosition(candidate, speed, delayMs, extraDist,
                                                  heroPos, boundingRadius, planeY, spells);
                directTable.push_back(info);
                posTable.push_back(info);
            }
        }
        // Sort direct candidates and pick the first safe one.
        std::sort(directTable.begin(), directTable.end(), Better);
        for (const auto& info : directTable) {
            if (!info.rejectPosition && !info.isDangerousPos) {
                out = info.position;
                return true;
            }
        }

        int posChecked = 0;
        int radiusIndex = 0;

        while (posChecked < maxPosToCheck) {
            radiusIndex++;
            const int curRadius = radiusIndex * (2 * posRadius);
            const int circleChecks = std::max(1,
                static_cast<int>(std::ceil((2.0 * 3.14159265358979323846 * curRadius) / (2.0 * posRadius))));

            for (int i = 1; i < circleChecks && posChecked < maxPosToCheck; ++i) {
                posChecked++;
                const double rad = (2.0 * 3.14159265358979323846 / (circleChecks - 1)) * i;
                const Vec2 candidate(
                    std::floor(heroPos.x + curRadius * static_cast<float>(std::cos(rad))),
                    std::floor(heroPos.y + curRadius * static_cast<float>(std::sin(rad))));

                posTable.push_back(ScorePosition(candidate, speed, delayMs, extraDist,
                                                  heroPos, boundingRadius, planeY, spells));
            }
        }

        // Sort by fastEvadeMode, then fall back from normal to fast.
        if (fastEvadeMode) {
            std::sort(posTable.begin(), posTable.end(), FastBetter);
        } else {
            std::sort(posTable.begin(), posTable.end(), Better);
            auto firstNonReject = std::find_if(posTable.begin(), posTable.end(),
                [](const PositionInfo& info) { return !info.rejectPosition; });
            if (firstNonReject != posTable.end() && firstNonReject->isDangerousPos) {
                std::vector<PositionInfo> fastTable = posTable;
                std::sort(fastTable.begin(), fastTable.end(), FastBetter);
                auto fastFirst = std::find_if(fastTable.begin(), fastTable.end(),
                    [](const PositionInfo& info) { return !info.rejectPosition; });
                if (fastFirst != fastTable.end() && !fastFirst->isDangerousPos) {
                    posTable = std::move(fastTable);
                    fastEvadeMode = true;
                }
            }
        }

        // Return first non-reject position
        for (const auto& info : posTable) {
            if (!info.rejectPosition) {
                out = info.position;
                if (evadeDistance > 0.0f) {
                    out = GetExtendedSafePosition(heroPos, out, std::min(evadeDistance, kDodgeTargetExtraBuffer),
                                                  boundingRadius, planeY, spells);
                }
                return !info.isDangerousPos;
            }
        }
        out = heroPos;
        return false;
    }

    static bool GetBestPositionMovementBlock(const Vec2& movePos,
                                             const SDK::AIHeroClient& player,
                                             const Vec2& heroPos, float boundingRadius,
                                             const std::vector<TrackedSpell>& spells,
                                             Vec2& out, float extraDelay, float extraDist,
                                             float evadeDistance = 0.0f) {
        const float speed = std::max(50.0f, player.MoveSpeed());
        const float delayMs = extraDelay + static_cast<float>(SDK::Game::Ping());
        const float planeY = player.ServerPosition().y;

        std::vector<PositionInfo> posTable;
        std::vector<PositionInfo> directTable;

        for (const auto& s : spells) {
            std::vector<Vec2> candidates;
            AddHeadOnPathCandidates(s, heroPos, movePos, boundingRadius, extraDist, candidates);
            for (const auto& candidate : candidates) {
                PositionInfo info = ScorePosition(candidate, speed, delayMs, extraDist,
                                                  heroPos, boundingRadius, planeY, spells);
                directTable.push_back(info);
                posTable.push_back(info);
            }
        }
        // GetBestPositionMovementBlock: sort direct candidates and pick the first safe one.
        std::sort(directTable.begin(), directTable.end(), Better);
        for (const auto& info : directTable) {
            if (!info.rejectPosition && !info.isDangerousPos) {
                out = info.position;
                return true;
            }
        }

        directTable.clear();
        for (const auto& s : spells) {
            std::vector<Vec2> candidates;
            AddSafestPathCandidates(s, heroPos, boundingRadius, extraDist, evadeDistance, candidates);
            for (const auto& candidate : candidates) {
                PositionInfo info = ScorePosition(candidate, speed, delayMs, extraDist,
                                                  heroPos, boundingRadius, planeY, spells);
                directTable.push_back(info);
                posTable.push_back(info);
            }
        }
        std::sort(directTable.begin(), directTable.end(), Better);
        for (const auto& info : directTable) {
            if (!info.rejectPosition && !info.isDangerousPos) {
                out = info.position;
                return true;
            }
        }

        // Movement block: ring search.
        constexpr int kMaxPos = 50;
        constexpr int kPosRadius = 50;
        int posChecked = 0;
        int radiusIndex = 0;

        while (posChecked < kMaxPos) {
            radiusIndex++;
            const int curRadius = radiusIndex * (2 * kPosRadius);
            const int circleChecks = std::max(1,
                static_cast<int>(std::ceil((2.0 * 3.14159265358979323846 * curRadius) / (2.0 * kPosRadius))));

            for (int i = 1; i < circleChecks && posChecked < kMaxPos; ++i) {
                posChecked++;
                const double rad = (2.0 * 3.14159265358979323846 / (circleChecks - 1)) * i;
                const Vec2 candidate(
                    std::floor(heroPos.x + curRadius * static_cast<float>(std::cos(rad))),
                    std::floor(heroPos.y + curRadius * static_cast<float>(std::sin(rad))));

                posTable.push_back(ScorePosition(candidate, speed, delayMs, extraDist,
                                                  heroPos, boundingRadius, planeY, spells));
            }
        }

        std::sort(posTable.begin(), posTable.end(), Better);

        for (const auto& info : posTable) {
            if (!info.rejectPosition) {
                out = info.position;
                if (evadeDistance > 0.0f) {
                    out = GetExtendedSafePosition(heroPos, out, std::min(evadeDistance, kDodgeTargetExtraBuffer),
                                                  boundingRadius, planeY, spells);
                }
                return !info.isDangerousPos;
            }
        }
        out = heroPos;
        return false;
    }

    static bool GetBestEvadeSpellPosition(const SDK::AIHeroClient& player, const Vec2& heroPos,
                                          float boundingRadius,
                                          const std::vector<TrackedSpell>& spells,
                                          float range, float spellSpeed,
                                          float spellDelay, bool fixedRange,
                                          Vec2& out) {
        if (!player.IsValid() || range <= 0.0f) return false;
        const float speed = spellSpeed > 0.0f ? spellSpeed : 5000.0f;
        const float delayMs = spellDelay + static_cast<float>(SDK::Game::Ping());
        const float planeY = player.ServerPosition().y;
        std::vector<Vec2> candidates;
        const Vec2 mouseDir = (SDK::Game::CursorPos().To2D() - heroPos).Normalized();
        if (!mouseDir.IsZero()) candidates.push_back(heroPos + mouseDir * range);
        for (const auto& s : spells) {
            AddSafestPathCandidates(s, heroPos, boundingRadius, 0.0f, range, candidates);
            if (IsLineType(s.Type())) {
                const Vec2 back = s.direction * -range;
                candidates.push_back(heroPos + back);
            }
        }
        constexpr int kRingChecks = 16;
        for (int i = 0; i < kRingChecks; ++i) {
            const float rad = (2.0f * 3.14159265358979323846f * static_cast<float>(i)) / static_cast<float>(kRingChecks);
            const Vec2 dir(std::cos(rad), std::sin(rad));
            candidates.push_back(heroPos + dir * range);
            if (!fixedRange) candidates.push_back(heroPos + dir * (range * 0.55f));
        }
        std::vector<PositionInfo> posTable;
        posTable.reserve(candidates.size());
        for (Vec2 candidate : candidates) {
            Vec2 dir = candidate - heroPos;
            const float dist = EvadeMath::Norm(dir);
            if (dist < 1.0f) continue;
            if (dist > range) {
                dir = dir * (1.0f / dist);
                candidate = heroPos + dir * range;
            }
            if (!fixedRange && dist < 75.0f) continue;
            posTable.push_back(ScorePosition(candidate, speed, delayMs, 0.0f,
                                             heroPos, boundingRadius, planeY, spells));
        }
        std::sort(posTable.begin(), posTable.end(), Better);
        for (const auto& info : posTable) {
            if (!info.rejectPosition && !info.isDangerousPos) {
                out = info.position;
                return true;
            }
        }
        return false;
    }

    // CheckMovePath checks path crossing plus endpoint safety.
    static bool CheckMovePath(const Vec2& movePos, float delayMs,
                              const Vec2& heroPos, float boundingRadius,
                              const std::vector<TrackedSpell>& spells, float moveSpeed) {
        (void)delayMs;
        (void)moveSpeed;
        // Check if walking from heroPos to movePos crosses any skillshot
        if (CheckMoveToDirection(heroPos, movePos, boundingRadius)) return true;
        // Also check endpoint with PredictSpellCollision
        for (const auto& s : spells) {
            if (InSkillShot(s, movePos, boundingRadius) ||
                PredictSpellCollision(s, movePos, moveSpeed, delayMs, heroPos, boundingRadius, 0.0f))
                return true;
        }
        return false;
    }

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

private:
    // Normal sort: rejectPosition, posDangerLevel, posDangerCount, distanceToMouse.
    static bool Better(const PositionInfo& a, const PositionInfo& b) {
        if (a.rejectPosition != b.rejectPosition) return !a.rejectPosition;
        if (a.posDangerLevel != b.posDangerLevel) return a.posDangerLevel < b.posDangerLevel;
        if (a.posDangerCount != b.posDangerCount) return a.posDangerCount < b.posDangerCount;
        return a.distanceToMouse < b.distanceToMouse;
    }

    // Fast sort: isDangerousPos, intersectionTime descending, posDangerLevel, posDangerCount.
    static bool FastBetter(const PositionInfo& a, const PositionInfo& b) {
        if (a.isDangerousPos != b.isDangerousPos) return !a.isDangerousPos;
        if (a.closestDistance != b.closestDistance) return a.closestDistance > b.closestDistance;
        if (a.posDangerLevel != b.posDangerLevel) return a.posDangerLevel < b.posDangerLevel;
        if (a.posDangerCount != b.posDangerCount) return a.posDangerCount < b.posDangerCount;
        return a.distanceToMouse < b.distanceToMouse;
    }
};

} // namespace ZDEvade
