#pragma once

#include "EvadeSettings.h"
#include "PositionInfo.h"

#include "../../../Core/CoreNavGrid.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <climits>
#include <cmath>
#include <memory>
#include <vector>

namespace Plugins::KuroEvade {

class EvadeHelper {
public:
    using SkillshotList = std::vector<std::shared_ptr<SDK::Skillshot>>;

    explicit EvadeHelper(const EvadeSettings& settings)
        : m_settings(settings) {
        if (m_settings.PreventEnemy) {
            for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
                if (enemy.IsValid() && !enemy.IsDead() && enemy.IsVisible()) {
                    m_enemyPositions.push_back(enemy.ServerPosition().To2D());
                }
            }
        }
        if (m_settings.PreventTower) {
            for (const auto& turret : SDK::GameObjects::EnemyTurrets()) {
                if (turret.IsValid() && !turret.IsDead()) {
                    m_turretPositions.push_back(turret.Position().To2D());
                }
            }
        }
    }

    bool IsEndangered(const SDK::AIHeroClient& /*player*/, const Vec2& heroPos,
                      float boundingRadius, const SkillshotList& skillshots) const {
        const int now = SDK::Variables::TickCount();
        const int reaction = std::max(
            m_settings.ReactionTime, m_settings.SpellDetectionTime);
        const float minHit = static_cast<float>(m_settings.MinHitTime);

        for (const auto& s : skillshots) {
            if (!s || !ShouldConsiderSpell(*s) ||
                !InSkillShot(*s, heroPos, boundingRadius + m_settings.ExtraSpellRadius)) {
                continue;
            }
            if (reaction > 0 && now - s->StartTime < reaction) {
                continue;
            }
            if (SpellHitTime(*s, heroPos) > minHit) {
                continue;
            }
            return true;
        }
        return false;
    }

    bool FindBestPosition(const SDK::AIHeroClient& player, const Vec2& heroPos,
                          float boundingRadius, const SkillshotList& skillshots,
                          Vec2& out, bool allowDangerousFallback = false) const {
        const float speed = std::max(50.0f, player.MoveSpeed());
        const float delayMs = m_settings.ExtraDelay +
            static_cast<float>(SDK::Game::Ping()) +
            std::max(0.0f, m_settings.CurrentWindupDelay);
        const float extraDist = m_settings.ExtraDist;
        const Vec2 mousePos = SDK::Game::CursorPos().To2D();
        const float planeY = player.ServerPosition().y;
        const bool fastSort = UseFastSort(LowestHitTime(heroPos, boundingRadius, skillshots));
        const int maxPosToCheck = m_settings.CandidateBudget > 0
            ? std::clamp(m_settings.CandidateBudget, 16, 200)
            : (m_settings.HigherPrecision ? 150 : 50);
        const int posRadius = maxPosToCheck <= 32
            ? 60
            : (m_settings.HigherPrecision ? 25 : 50);

        std::vector<Vec2> positions;
        positions.reserve(static_cast<std::size_t>(maxPosToCheck) + 40);
        AddGradientPositions(heroPos, boundingRadius, skillshots, positions);
        if (m_settings.KurokamiPosition) {
            AddKurokamiPositions(heroPos, boundingRadius, skillshots, positions);
        }
        AddFastestPositions(heroPos, boundingRadius, skillshots, positions);
        AddWallDetourPositions(heroPos, mousePos, planeY, boundingRadius, positions);
        AddRadialPositions(heroPos, maxPosToCheck, posRadius, positions);

        std::vector<PositionInfo> scored;
        scored.reserve(positions.size());
        for (const Vec2& position : positions) {
            scored.push_back(ScorePosition(position, speed, delayMs, extraDist,
                                           heroPos, boundingRadius, planeY, mousePos, skillshots));
        }

        std::sort(scored.begin(), scored.end(), [&](const PositionInfo& a, const PositionInfo& b) {
            return Better(a, b, fastSort);
        });

        for (const PositionInfo& info : scored) {
            if (info.dangerous) {
                continue;
            }

            Vec2 target = info.position;
            if ((fastSort || info.hasExtraDistance) && m_settings.ExtraEvadeDistance > 0.0f) {
                target = GetExtendedSafePosition(heroPos, info.position, m_settings.ExtraEvadeDistance,
                                                 speed, delayMs, extraDist, boundingRadius,
                                                 planeY, mousePos, skillshots);
            }

            const PositionInfo finalInfo = ScorePosition(target, speed, delayMs, extraDist,
                                                         heroPos, boundingRadius, planeY, mousePos, skillshots);
            if (!finalInfo.dangerous) {
                out = target;
                return true;
            }
        }

        if (allowDangerousFallback) {
            for (const PositionInfo& info : scored) {
                if (!info.wall) {
                    out = info.position;
                    return true;
                }
            }
        }
        return false;
    }

    static int DangerValue(const SDK::Skillshot& spell) {
        return std::max(1, spell.SData.DangerValue);
    }

    bool ShouldConsiderSpell(const SDK::Skillshot& spell) const {
        // SpellDetector owns lifetime.  The only expired objects it deliberately
        // keeps are persistent trap zones, which must remain dodgeable.
        if (m_settings.DodgeDangerousOnly && DangerValue(spell) < 3) {
            return false;
        }
        if (!m_settings.DodgeCircular && SDK::IsCircleSpellType(spell.SData.SpellType)) {
            return false;
        }
        return true;
    }

    int HighestDangerLevelAt(const Vec2& heroPos, float boundingRadius,
                             const SkillshotList& skillshots) const {
        int highest = 0;
        for (const auto& s : skillshots) {
            if (s && ShouldConsiderSpell(*s) &&
                InSkillShot(*s, heroPos, boundingRadius + m_settings.ExtraSpellRadius)) {
                highest = std::max(highest, DangerValue(*s));
            }
        }
        return highest;
    }

    float LowestHitTimeAt(const Vec2& heroPos, float boundingRadius,
                          const SkillshotList& skillshots) const {
        float lowest = FLT_MAX;
        for (const auto& s : skillshots) {
            if (s && ShouldConsiderSpell(*s) &&
                InSkillShot(*s, heroPos, boundingRadius + m_settings.ExtraSpellRadius)) {
                lowest = std::min(lowest, SpellHitTime(*s, heroPos));
            }
        }
        return lowest;
    }

    bool WillBeHitBefore(const Vec2& heroPos, float boundingRadius,
                         const SkillshotList& skillshots, float waitMs) const {
        if (waitMs <= 0.0f) {
            return false;
        }
        const float checkRadius = boundingRadius + m_settings.ExtraSpellRadius;
        for (const auto& s : skillshots) {
            if (!s || !ShouldConsiderSpell(*s) ||
                !InSkillShot(*s, heroPos, checkRadius)) {
                continue;
            }
            if (SpellHitTime(*s, heroPos) <= waitMs) {
                return true;
            }
        }
        return false;
    }

    static bool IsMovingLineSpell(const SDK::Skillshot& spell) {
        return SDK::IsLineSpellType(spell.SData.SpellType) &&
               spell.SData.MissileSpeed > 0 &&
               spell.SData.MissileSpeed != INT_MAX;
    }

    static bool IsMovingCircleSpell(const SDK::Skillshot& spell) {
        return SDK::IsCircleSpellType(spell.SData.SpellType) &&
               spell.SData.MissileSpeed > 0 &&
               spell.SData.MissileSpeed != INT_MAX;
    }

    static Vec2 CurrentLineStart(const SDK::Skillshot& spell, int afterTime = 0) {
        if (IsMovingLineSpell(spell)) {
            if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(&spell)) {
                return ClampToTravelSegment(
                    missile->GetMissilePosition(afterTime),
                    spell.StartPosition,
                    spell.EndPosition);
            }
        }
        return spell.StartPosition;
    }

    static float SpellHitTime(const SDK::Skillshot& spell, const Vec2& pos) {
        const int now = SDK::Variables::TickCount();
        if (IsMovingLineSpell(spell)) {
            const float speed = std::max(1.0f, static_cast<float>(spell.SData.MissileSpeed));
            const int latency = SDK::Game::Ping();
            const Vec2 head = CurrentLineStart(spell, latency);
            Vec2 direction = spell.Direction;
            if (direction.IsZero()) {
                direction = (spell.EndPosition - spell.StartPosition).Normalized();
            }
            const float longitudinal = std::max(0.0f, (pos - head).Dot(direction));
            const float launchRemaining = std::max(
                0.0f,
                static_cast<float>(spell.StartTime + spell.SData.Delay - now - latency));
            return launchRemaining + (longitudinal / speed) * 1000.0f;
        }

        if (IsMovingCircleSpell(spell)) {
            const int latency = SDK::Game::Ping();
            const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(&spell);
            const Vec2 head = missile
                ? ClampToTravelSegment(missile->GetMissilePosition(latency),
                                       spell.StartPosition, spell.EndPosition)
                : spell.StartPosition;
            Vec2 direction = spell.Direction;
            if (direction.IsZero()) {
                direction = (spell.EndPosition - spell.StartPosition).Normalized();
            }
            const float remaining = std::max(
                0.0f, (spell.EndPosition - head).Dot(direction));
            const float launchRemaining = std::max(
                0.0f,
                static_cast<float>(spell.StartTime + spell.SData.Delay - now - latency));
            return launchRemaining + 1000.0f * remaining /
                std::max(1.0f, static_cast<float>(spell.SData.MissileSpeed));
        }

        return std::max(0.0f, static_cast<float>(
            spell.StartTime + spell.SData.Delay - now - SDK::Game::Ping()));
    }

    static bool InSkillShot(const SDK::Skillshot& spell, const Vec2& pos, float radius) {
        const SDK::SpellType type = spell.SData.SpellType;
        const float spellRadius = static_cast<float>(spell.SData.Radius);

        if (SDK::IsLineSpellType(type)) {
            const auto proj = SDK::Prediction::Vec2Ext::ProjectOn(pos, CurrentLineStart(spell), spell.EndPosition);
            return proj.IsOnSegment && proj.SegmentPoint.Distance(pos) <= spellRadius + radius;
        }
        if (SDK::IsCircleSpellType(type)) {
            return pos.Distance(spell.EndPosition) <= spellRadius + radius;
        }
        return InPolygon(spell, pos, radius);
    }

    bool CheckMoveToDirection(const Vec2& from, const Vec2& to, float boundingRadius,
                              const SkillshotList& skillshots, float moveSpeed,
                              float delayMs) const {
        const float dist = from.Distance(to);
        if (dist < 1.0f) {
            return false;
        }
        for (const auto& spell : skillshots) {
            if (spell && ShouldConsiderSpell(*spell) &&
                PathCollidesWithSpell(*spell, from, to, boundingRadius,
                                      moveSpeed, delayMs, m_settings.ExtraSpellRadius)) {
                return true;
            }
        }
        return false;
    }

    bool CheckMovePath(const std::vector<Vec3>& path, float delayMs,
                       const Vec2& heroPos, float boundingRadius,
                       const SkillshotList& skillshots, float moveSpeed) const {
        Vec2 segmentStart = heroPos;
        float segmentDelayMs = std::max(0.0f, delayMs);
        const float speed = std::max(50.0f, moveSpeed);

        for (const Vec3& waypoint3 : path) {
            const Vec2 waypoint = waypoint3.To2D();
            const float length = segmentStart.Distance(waypoint);
            if (length < 1.0f) {
                continue;
            }
            if (CheckMoveToDirection(segmentStart, waypoint, boundingRadius,
                                     skillshots, speed, segmentDelayMs)) {
                return true;
            }
            segmentDelayMs += 1000.0f * length / speed;
            segmentStart = waypoint;
        }
        return false;
    }

    bool SkillshotAffectsPath(const SDK::Skillshot& spell,
                              const std::vector<Vec3>& path,
                              float delayMs,
                              const Vec2& heroPos,
                              float boundingRadius,
                              float moveSpeed) const {
        if (!ShouldConsiderSpell(spell)) {
            return false;
        }

        Vec2 segmentStart = heroPos;
        float segmentDelayMs = std::max(0.0f, delayMs);
        const float speed = std::max(50.0f, moveSpeed);
        for (const Vec3& waypoint3 : path) {
            const Vec2 waypoint = waypoint3.To2D();
            const float length = segmentStart.Distance(waypoint);
            if (length < 1.0f) {
                continue;
            }
            if (PathCollidesWithSpell(
                    spell, segmentStart, waypoint, boundingRadius,
                    speed, segmentDelayMs, m_settings.ExtraSpellRadius)) {
                return true;
            }
            segmentDelayMs += 1000.0f * length / speed;
            segmentStart = waypoint;
        }
        return false;
    }

    bool CheckMovePath(const Vec2& movePos, float delayMs,
                       const Vec2& heroPos, float boundingRadius,
                       const SkillshotList& skillshots, float moveSpeed) const {
        if (CheckMoveToDirection(heroPos, movePos, boundingRadius,
                                 skillshots, moveSpeed, delayMs)) {
            return true;
        }
        return false;
    }

    bool FindBestPositionForPath(const SDK::AIHeroClient& player, const Vec2& heroPos,
                                 float boundingRadius, const Vec2& targetPos,
                                 const SkillshotList& skillshots, Vec2& out) const {
        const float speed = std::max(50.0f, player.MoveSpeed());
        const float delayMs = m_settings.ExtraDelay +
            static_cast<float>(SDK::Game::Ping()) +
            std::max(0.0f, m_settings.CurrentWindupDelay);
        const float extraDist = m_settings.ExtraDist;
        const float planeY = player.ServerPosition().y;
        const bool fastSort = UseFastSort(LowestHitTime(heroPos, boundingRadius, skillshots));
        const int maxPosToCheck = m_settings.CandidateBudget > 0
            ? std::clamp(m_settings.CandidateBudget, 16, 200)
            : (m_settings.HigherPrecision ? 150 : 50);
        const int posRadius = maxPosToCheck <= 32
            ? 60
            : (m_settings.HigherPrecision ? 25 : 50);

        std::vector<Vec2> positions;
        positions.reserve(static_cast<std::size_t>(maxPosToCheck) + 40);
        AddGradientPositions(heroPos, boundingRadius, skillshots, positions);
        if (m_settings.KurokamiPosition) {
            AddKurokamiPositions(heroPos, boundingRadius, skillshots, positions);
        }
        AddFastestPositions(heroPos, boundingRadius, skillshots, positions);
        AddWallDetourPositions(heroPos, targetPos, planeY, boundingRadius, positions);
        AddRadialPositions(heroPos, maxPosToCheck, posRadius, positions);

        std::vector<PositionInfo> scored;
        scored.reserve(positions.size());
        for (const Vec2& position : positions) {
            scored.push_back(ScorePosition(position, speed, delayMs, extraDist,
                                           heroPos, boundingRadius, planeY, targetPos, skillshots));
        }

        std::sort(scored.begin(), scored.end(), [&](const PositionInfo& a, const PositionInfo& b) {
            return Better(a, b, fastSort);
        });

        for (const PositionInfo& info : scored) {
            if (info.dangerous) {
                continue;
            }

            Vec2 target = info.position;
            if ((fastSort || info.hasExtraDistance) && m_settings.ExtraEvadeDistance > 0.0f) {
                target = GetExtendedSafePosition(heroPos, info.position, m_settings.ExtraEvadeDistance,
                                                 speed, delayMs, extraDist, boundingRadius,
                                                 planeY, targetPos, skillshots);
            }

            const PositionInfo finalInfo = ScorePosition(target, speed, delayMs, extraDist,
                                                         heroPos, boundingRadius, planeY, targetPos, skillshots);
            if (!finalInfo.dangerous) {
                out = target;
                return true;
            }
        }
        return false;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    const EvadeSettings& m_settings;
    std::vector<Vec2> m_enemyPositions;
    std::vector<Vec2> m_turretPositions;

    static bool InPolygon(const SDK::Skillshot& spell, const Vec2& pos, float radius) {
        if (spell.Path.empty()) {
            return false;
        }
        if (SDK::Clipper::PointInPolygon(
                SDK::Clipper::IntPoint(pos.x, pos.y), spell.Path) == 1) {
            return true;
        }
        if (radius <= 0.0f) {
            return false;
        }

        constexpr float twoPi = 6.28318530717958647692f;
        for (int i = 0; i < 8; ++i) {
            const float a = static_cast<float>(i) * (twoPi / 8.0f);
            const Vec2 p(pos.x + radius * std::cos(a), pos.y + radius * std::sin(a));
            if (SDK::Clipper::PointInPolygon(
                    SDK::Clipper::IntPoint(p.x, p.y), spell.Path) == 1) {
                return true;
            }
        }
        return false;
    }

    static float PointSegmentDistance(const Vec2& point, const Vec2& start, const Vec2& end) {
        const auto proj = SDK::Prediction::Vec2Ext::ProjectOn(point, start, end);
        return point.Distance(proj.SegmentPoint);
    }

    static float Cross(const Vec2& a, const Vec2& b, const Vec2& c) {
        return (b - a).Cross(c - a);
    }

    static bool OnSegment(const Vec2& a, const Vec2& b, const Vec2& p) {
        constexpr float eps = 0.001f;
        return std::min(a.x, b.x) - eps <= p.x && p.x <= std::max(a.x, b.x) + eps &&
               std::min(a.y, b.y) - eps <= p.y && p.y <= std::max(a.y, b.y) + eps &&
               std::fabs(Cross(a, b, p)) <= eps;
    }

    static bool SegmentsIntersect(const Vec2& a, const Vec2& b,
                                  const Vec2& c, const Vec2& d) {
        const float c1 = Cross(a, b, c);
        const float c2 = Cross(a, b, d);
        const float c3 = Cross(c, d, a);
        const float c4 = Cross(c, d, b);
        if (((c1 > 0.0f && c2 < 0.0f) || (c1 < 0.0f && c2 > 0.0f)) &&
            ((c3 > 0.0f && c4 < 0.0f) || (c3 < 0.0f && c4 > 0.0f))) {
            return true;
        }
        return OnSegment(a, b, c) || OnSegment(a, b, d) ||
               OnSegment(c, d, a) || OnSegment(c, d, b);
    }

    static float SegmentDistance(const Vec2& a, const Vec2& b,
                                 const Vec2& c, const Vec2& d) {
        if (SegmentsIntersect(a, b, c, d)) {
            return 0.0f;
        }
        return std::min(
            std::min(PointSegmentDistance(a, c, d), PointSegmentDistance(b, c, d)),
            std::min(PointSegmentDistance(c, a, b), PointSegmentDistance(d, a, b)));
    }

    static Vec2 ClampToTravelSegment(const Vec2& point,
                                     const Vec2& start,
                                     const Vec2& end) {
        const Vec2 delta = end - start;
        const float lengthSqr = delta.LengthSqr();
        if (lengthSqr <= 0.0001f) {
            return start;
        }
        const float t = std::clamp((point - start).Dot(delta) / lengthSqr, 0.0f, 1.0f);
        return start + delta * t;
    }

    static Vec2 HeroPositionAt(const Vec2& from,
                               const Vec2& to,
                               float speed,
                               float delaySeconds,
                               float timeSeconds) {
        if (timeSeconds <= delaySeconds) {
            return from;
        }
        const Vec2 delta = to - from;
        const float distance = delta.Length();
        if (distance <= 0.0001f) {
            return from;
        }
        const float travelled = speed * (timeSeconds - delaySeconds);
        if (travelled >= distance) {
            return to;
        }
        return from + delta * (travelled / distance);
    }

    static float MinRelativeDistanceSqr(const Vec2& heroStart,
                                        const Vec2& heroVelocity,
                                        const Vec2& spellStart,
                                        const Vec2& spellVelocity,
                                        float durationSeconds) {
        const Vec2 relativeStart = heroStart - spellStart;
        const Vec2 relativeVelocity = heroVelocity - spellVelocity;
        const float velocitySqr = relativeVelocity.LengthSqr();
        float time = 0.0f;
        if (velocitySqr > 0.00000001f) {
            // d/dt |r + vt|^2 = 2(r + vt).v = 0.
            time = std::clamp(
                -relativeStart.Dot(relativeVelocity) / velocitySqr,
                0.0f,
                std::max(0.0f, durationSeconds));
        }
        return (relativeStart + relativeVelocity * time).LengthSqr();
    }

    static float PointPolygonDistance(const SDK::Skillshot& spell, const Vec2& point) {
        if (spell.Path.empty()) {
            return FLT_MAX;
        }
        if (SDK::Clipper::PointInPolygon(
                SDK::Clipper::IntPoint(point.x, point.y), spell.Path) == 1) {
            return 0.0f;
        }

        float best = FLT_MAX;
        for (std::size_t i = 0; i < spell.Path.size(); ++i) {
            const auto& a = spell.Path[i];
            const auto& b = spell.Path[(i + 1) % spell.Path.size()];
            best = std::min(best, PointSegmentDistance(
                point,
                Vec2(static_cast<float>(a.X), static_cast<float>(a.Y)),
                Vec2(static_cast<float>(b.X), static_cast<float>(b.Y))));
        }
        return best;
    }

    static float SegmentPolygonDistance(const SDK::Skillshot& spell,
                                        const Vec2& from,
                                        const Vec2& to) {
        if (spell.Path.empty()) {
            return FLT_MAX;
        }
        if (PointPolygonDistance(spell, from) == 0.0f ||
            PointPolygonDistance(spell, to) == 0.0f) {
            return 0.0f;
        }

        float best = FLT_MAX;
        for (std::size_t i = 0; i < spell.Path.size(); ++i) {
            const auto& a = spell.Path[i];
            const auto& b = spell.Path[(i + 1) % spell.Path.size()];
            best = std::min(best, SegmentDistance(
                from, to,
                Vec2(static_cast<float>(a.X), static_cast<float>(a.Y)),
                Vec2(static_cast<float>(b.X), static_cast<float>(b.Y))));
        }
        return best;
    }

    static float MovingLineClearance(const SDK::Skillshot& spell,
                                     const Vec2& from,
                                     const Vec2& to,
                                     float heroSpeed,
                                     float delayMs,
                                     float checkRadius) {
        Vec2 direction = spell.Direction;
        if (direction.IsZero()) {
            direction = (spell.EndPosition - spell.StartPosition).Normalized();
        }
        if (direction.IsZero()) {
            return FLT_MAX;
        }

        const float missileSpeed = std::max(
            1.0f, static_cast<float>(spell.SData.MissileSpeed));
        const int now = SDK::Variables::TickCount();
        const float launchSeconds = std::max(
            0.0f,
            static_cast<float>(spell.StartTime + spell.SData.Delay - now) / 1000.0f);
        const Vec2 missileAtActiveStart = launchSeconds > 0.0f
            ? spell.StartPosition
            : CurrentLineStart(spell);
        const float remainingDistance = std::max(
            0.0f, (spell.EndPosition - missileAtActiveStart).Dot(direction));
        if (remainingDistance <= 0.01f) {
            return FLT_MAX;
        }

        const float activeStart = launchSeconds;
        const float activeEnd = activeStart + remainingDistance / missileSpeed;
        const float heroDelay = std::max(0.0f, delayMs) / 1000.0f;
        const float walkDistance = from.Distance(to);
        const float heroArrival = heroDelay + walkDistance / std::max(50.0f, heroSpeed);

        std::array<float, 4> breakpoints = {
            activeStart,
            activeEnd,
            std::clamp(heroDelay, activeStart, activeEnd),
            std::clamp(heroArrival, activeStart, activeEnd),
        };
        std::sort(breakpoints.begin(), breakpoints.end());

        float minDistanceSqr = FLT_MAX;
        const Vec2 heroDirection = (to - from).Normalized();
        const Vec2 missileVelocity = direction * missileSpeed;
        for (std::size_t i = 0; i + 1 < breakpoints.size(); ++i) {
            const float begin = breakpoints[i];
            const float end = breakpoints[i + 1];
            if (end < begin) {
                continue;
            }

            const float midpoint = (begin + end) * 0.5f;
            const Vec2 heroVelocity =
                midpoint > heroDelay && midpoint < heroArrival
                    ? heroDirection * heroSpeed
                    : Vec2();
            const Vec2 heroStart = HeroPositionAt(
                from, to, heroSpeed, heroDelay, begin);
            const Vec2 missileStart = missileAtActiveStart +
                missileVelocity * (begin - activeStart);
            minDistanceSqr = std::min(
                minDistanceSqr,
                MinRelativeDistanceSqr(heroStart, heroVelocity,
                                       missileStart, missileVelocity,
                                       end - begin));
        }

        return std::sqrt(std::max(0.0f, minDistanceSqr)) - checkRadius;
    }

    static float TimedGeometryClearance(const SDK::Skillshot& spell,
                                        const Vec2& from,
                                        const Vec2& to,
                                        float heroSpeed,
                                        float delayMs,
                                        float boundingRadius,
                                        float extraDist) {
        const float heroDelay = std::max(0.0f, delayMs) / 1000.0f;
        const bool persistent = spell.HasExpired();

        if (persistent) {
            if (SDK::IsLineSpellType(spell.SData.SpellType)) {
                return SegmentDistance(from, to, spell.StartPosition, spell.EndPosition) -
                    (static_cast<float>(spell.SData.Radius) + boundingRadius + extraDist);
            }
            if (SDK::IsCircleSpellType(spell.SData.SpellType)) {
                return PointSegmentDistance(spell.EndPosition, from, to) -
                    (static_cast<float>(spell.SData.Radius) + boundingRadius + extraDist);
            }
            return SegmentPolygonDistance(spell, from, to) - boundingRadius - extraDist;
        }

        float impactSeconds = std::max(
            0.0f,
            static_cast<float>(spell.StartTime + spell.SData.Delay -
                               SDK::Variables::TickCount()) / 1000.0f);
        if (IsMovingCircleSpell(spell)) {
            Vec2 direction = spell.Direction;
            if (direction.IsZero()) {
                direction = (spell.EndPosition - spell.StartPosition).Normalized();
            }
            const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(&spell);
            const Vec2 missilePosition = impactSeconds > 0.0f || !missile
                ? spell.StartPosition
                : ClampToTravelSegment(missile->GetMissilePosition(0),
                                       spell.StartPosition, spell.EndPosition);
            const float remaining = std::max(
                0.0f, (spell.EndPosition - missilePosition).Dot(direction));
            impactSeconds += remaining /
                std::max(1.0f, static_cast<float>(spell.SData.MissileSpeed));
        }

        const Vec2 heroAtImpact = HeroPositionAt(
            from, to, std::max(50.0f, heroSpeed), heroDelay, impactSeconds);
        if (SDK::IsLineSpellType(spell.SData.SpellType)) {
            return PointSegmentDistance(
                heroAtImpact, spell.StartPosition, spell.EndPosition) -
                (static_cast<float>(spell.SData.Radius) + boundingRadius + extraDist);
        }
        if (SDK::IsCircleSpellType(spell.SData.SpellType)) {
            return heroAtImpact.Distance(spell.EndPosition) -
                (static_cast<float>(spell.SData.Radius) + boundingRadius + extraDist);
        }
        return PointPolygonDistance(spell, heroAtImpact) - boundingRadius - extraDist;
    }

    bool PathCollidesWithSpell(const SDK::Skillshot& spell,
                               const Vec2& from,
                               const Vec2& to,
                               float boundingRadius,
                               float moveSpeed,
                               float delayMs,
                               float extraRadius) const {
        return GetClosestDistanceApproach(
            spell, to, std::max(50.0f, moveSpeed), delayMs,
            from, boundingRadius, extraRadius) <= 0.0f;
    }

    float GetClosestDistanceApproach(const SDK::Skillshot& spell,
                                     const Vec2& pos, float speed, float delayMs,
                                     const Vec2& heroPos, float boundingRadius,
                                     float extraDist) const {
        const SDK::SpellType type = spell.SData.SpellType;

        float clearance = FLT_MAX;
        if (SDK::IsLineSpellType(type) && IsMovingLineSpell(spell)) {
            clearance = MovingLineClearance(
                spell, heroPos, pos, std::max(50.0f, speed), delayMs,
                static_cast<float>(spell.SData.Radius) + boundingRadius + extraDist);
        } else {
            clearance = TimedGeometryClearance(
                spell, heroPos, pos, std::max(50.0f, speed), delayMs,
                boundingRadius, extraDist);
        }
        return std::max(0.0f, clearance);
    }

    bool PredictSpellCollision(const SDK::Skillshot& spell, const Vec2& pos, float speed,
                               float delayMs, const Vec2& heroPos, float boundingRadius,
                               float extraDist) const {
        return GetClosestDistanceApproach(spell, pos, speed, delayMs, heroPos,
                                          boundingRadius, extraDist + 10.0f) == 0.0f;
    }

    float DistanceToEnemyTurret(const Vec2& pos) const {
        float best = FLT_MAX;
        for (const Vec2& turretPosition : m_turretPositions) {
            best = std::min(best, turretPosition.Distance(pos));
        }
        return best;
    }

    float DistanceToEnemyChampion(const Vec2& pos) const {
        float best = FLT_MAX;
        for (const Vec2& enemyPosition : m_enemyPositions) {
            best = std::min(best, enemyPosition.Distance(pos));
        }
        return best;
    }

    float PositionValue(const Vec2& pos, const Vec2& mousePos, float boundingRadius) const {
        float value = pos.Distance(mousePos);

        if (m_settings.PreventTower) {
            const float turretRange = 875.0f + boundingRadius;
            const float turretDistance = DistanceToEnemyTurret(pos);
            if (turretDistance < turretRange) {
                value += 5.0f * (turretRange - turretDistance);
            }
        }

        if (m_settings.PreventEnemy) {
            const float comfort = m_settings.MinComfortZone;
            const float enemyDistance = DistanceToEnemyChampion(pos);
            if (enemyDistance < comfort) {
                value += 2.0f * (comfort - enemyDistance);
            }
        }
        return value;
    }

    bool WallBlocks(const Vec2& heroPos, const Vec2& pos,
                    float planeY, float boundingRadius) const {
        const Vec3 dest = Vec3::From2D(pos, planeY);
        const Vec3 from = Vec3::From2D(heroPos, planeY);
        const float radius = std::clamp(boundingRadius, 0.0f, 65.0f);
        if (!CoreNavGrid::IsWalkable(dest) ||
            CoreNavGrid::IsWallOfType(dest, CoreNavGrid::Collision_Wall, radius)) {
            return true;
        }
        return CoreNavGrid::IsWallBetween(from, dest);
    }

    bool CheckDangerousPosition(const Vec2& pos, float radius, const SkillshotList& skillshots) const {
        const float checkRadius = radius + m_settings.ExtraSpellRadius;
        for (const auto& s : skillshots) {
            if (s && ShouldConsiderSpell(*s) && InSkillShot(*s, pos, checkRadius)) {
                return true;
            }
        }
        return false;
    }

    PositionInfo ScorePosition(const Vec2& pos, float speed, float delayMs, float extraDist,
                               const Vec2& heroPos, float boundingRadius, float planeY,
                               const Vec2& mousePos, const SkillshotList& skillshots) const {
        PositionInfo info;
        info.position = pos;
        info.distToMouse = PositionValue(pos, mousePos, boundingRadius);
        info.wall = WallBlocks(heroPos, pos, planeY, boundingRadius);

        for (const auto& s : skillshots) {
            if (!s || !ShouldConsiderSpell(*s)) {
                continue;
            }
            const int danger = DangerValue(*s);
            const float cpa = GetClosestDistanceApproach(
                *s, pos, speed, delayMs, heroPos, boundingRadius,
                extraDist + m_settings.ExtraSpellRadius);
            info.closestDistance = std::min(info.closestDistance, cpa);

            if (cpa <= 0.0f) {
                info.dangerLevel = std::max(info.dangerLevel, danger);
                info.dangerCount += danger;
            }
            if (m_settings.ExtraAvoidDistance > 0.0f &&
                cpa < m_settings.ExtraAvoidDistance) {
                info.hasExtraDistance = true;
            }
        }

        info.rejectPosition = m_settings.RejectMinDistance > 0.0f &&
            info.closestDistance < m_settings.RejectMinDistance;
        info.dangerous = info.dangerCount > 0 || info.wall;
        return info;
    }

    Vec2 GetExtendedSafePosition(const Vec2& from, const Vec2& to, float extendDistance,
                                 float speed, float delayMs, float extraDist,
                                 float boundingRadius, float planeY, const Vec2& mousePos,
                                 const SkillshotList& skillshots) const {
        const Vec2 dir = (to - from).Normalized();
        if (dir.IsZero() || extendDistance <= 0.0f) {
            return to;
        }

        Vec2 result = to;
        for (int step = 50; static_cast<float>(step) <= extendDistance; step += 50) {
            const Vec2 candidate = to + dir * static_cast<float>(step);
            const PositionInfo info = ScorePosition(candidate, speed, delayMs, extraDist,
                                                    from, boundingRadius, planeY, mousePos, skillshots);
            if (info.dangerous) {
                return result;
            }
            result = candidate;
        }
        return result;
    }

    static const std::array<Vec2, 64>& UnitDirections() {
        static const std::array<Vec2, 64> directions = [] {
            std::array<Vec2, 64> result = {};
            for (std::size_t i = 0; i < result.size(); ++i) {
                const float angle = static_cast<float>(
                    2.0 * kPi * static_cast<double>(i) /
                    static_cast<double>(result.size()));
                result[i] = Vec2(std::cos(angle), std::sin(angle));
            }
            return result;
        }();
        return directions;
    }

    static void AddRadialPositions(const Vec2& heroPos,
                                   int maxPositions,
                                   int positionRadius,
                                   std::vector<Vec2>& out) {
        const auto& directions = UnitDirections();
        int added = 0;
        int radiusIndex = 0;
        while (added < maxPositions) {
            ++radiusIndex;
            const int radius = radiusIndex * 2 * positionRadius;
            const int checks = std::clamp(
                static_cast<int>(std::ceil(
                    (2.0 * kPi * static_cast<double>(radius)) /
                    (2.0 * static_cast<double>(positionRadius)))),
                4,
                static_cast<int>(directions.size()));
            for (int i = 0; i < checks && added < maxPositions; ++i, ++added) {
                const std::size_t directionIndex =
                    static_cast<std::size_t>(i) * directions.size() /
                    static_cast<std::size_t>(checks);
                const Vec2 candidate = heroPos +
                    directions[directionIndex] * static_cast<float>(radius);
                out.emplace_back(std::floor(candidate.x), std::floor(candidate.y));
            }
        }
    }

    void AddGradientPositions(const Vec2& heroPos,
                              float boundingRadius,
                              const SkillshotList& skillshots,
                              std::vector<Vec2>& out) const {
        Vec2 gradient;
        const Vec2 preferred = (SDK::Game::CursorPos().To2D() - heroPos).Normalized();

        for (const auto& spell : skillshots) {
            if (!spell || !ShouldConsiderSpell(*spell) ||
                !InSkillShot(*spell, heroPos,
                             boundingRadius + m_settings.ExtraSpellRadius)) {
                continue;
            }

            Vec2 outward;
            if (SDK::IsLineSpellType(spell->SData.SpellType)) {
                const Vec2 start = CurrentLineStart(*spell);
                const auto projection = SDK::Prediction::Vec2Ext::ProjectOn(
                    heroPos, start, spell->EndPosition);
                outward = (heroPos - projection.SegmentPoint).Normalized();
                if (outward.IsZero()) {
                    Vec2 direction = spell->Direction;
                    if (direction.IsZero()) {
                        direction = (spell->EndPosition - start).Normalized();
                    }
                    outward = Vec2(-direction.y, direction.x);
                    if (!preferred.IsZero() && outward.Dot(preferred) < 0.0f) {
                        outward = outward * -1.0f;
                    }
                }
            } else {
                outward = (heroPos - spell->EndPosition).Normalized();
            }
            if (outward.IsZero()) {
                continue;
            }

            const float weight = static_cast<float>(DangerValue(*spell));
            gradient = gradient + outward * weight;
            const float nearDistance =
                static_cast<float>(spell->SData.Radius) + boundingRadius + 35.0f;
            out.push_back(heroPos + outward * nearDistance);
            out.push_back(heroPos + outward * (nearDistance + 100.0f));
        }

        gradient = gradient.Normalized();
        if (gradient.IsZero()) {
            return;
        }

        constexpr float angles[] = { 0.0f, -0.45f, 0.45f, -0.85f, 0.85f };
        constexpr float distances[] = { 140.0f, 240.0f, 360.0f };
        for (float angle : angles) {
            const Vec2 direction = SDK::Prediction::Vec2Ext::Rotated(
                gradient, angle).Normalized();
            for (float distance : distances) {
                out.push_back(heroPos + direction * distance);
            }
        }
    }

    void AddWallDetourPositions(const Vec2& heroPos,
                                const Vec2& desiredPosition,
                                float planeY,
                                float boundingRadius,
                                std::vector<Vec2>& out) const {
        const Vec3 from = Vec3::From2D(heroPos, planeY);
        const Vec3 desired = Vec3::From2D(desiredPosition, planeY);
        if (!CoreNavGrid::IsWallBetween(from, desired)) {
            return;
        }

        Vec3 hitPoint3;
        if (!CoreNavGrid::FindWallCollision(from, desired, hitPoint3, 15.0f)) {
            return;
        }
        const Vec2 direction = (desiredPosition - heroPos).Normalized();
        if (direction.IsZero()) {
            return;
        }
        const Vec2 side(-direction.y, direction.x);
        const Vec2 nearSide = hitPoint3.To2D() -
            direction * (boundingRadius + 55.0f);
        constexpr float offsets[] = { 90.0f, 150.0f, 230.0f, 330.0f, 450.0f };
        for (float offset : offsets) {
            const Vec2 left = nearSide + side * offset;
            const Vec2 right = nearSide - side * offset;
            if (!WallBlocks(heroPos, left, planeY, boundingRadius)) {
                out.push_back(left);
            }
            if (!WallBlocks(heroPos, right, planeY, boundingRadius)) {
                out.push_back(right);
            }
        }
    }

    void AddFastestPositions(const Vec2& heroPos, float boundingRadius,
                             const SkillshotList& skillshots, std::vector<Vec2>& out) const {
        for (const auto& s : skillshots) {
            if (!s || !ShouldConsiderSpell(*s) ||
                !InSkillShot(*s, heroPos, boundingRadius + m_settings.ExtraSpellRadius)) {
                continue;
            }

            const float radius = static_cast<float>(s->SData.Radius) +
                boundingRadius + m_settings.ExtraSpellRadius + 15.0f;
            if (SDK::IsLineSpellType(s->SData.SpellType)) {
                Vec2 current = CurrentLineStart(*s);
                const auto proj = SDK::Prediction::Vec2Ext::ProjectOn(heroPos, current, s->EndPosition);
                if (proj.IsOnSegment) {
                    out.push_back(proj.SegmentPoint.Extend(heroPos, radius));
                }
            } else if (SDK::IsCircleSpellType(s->SData.SpellType)) {
                out.push_back(s->EndPosition.Extend(heroPos, radius));
            }
        }
    }

    void AddKurokamiPositions(const Vec2& heroPos, float boundingRadius,
                              const SkillshotList& skillshots, std::vector<Vec2>& out) const {
        for (const auto& s : skillshots) {
            if (!s || !ShouldConsiderSpell(*s) ||
                !InSkillShot(*s, heroPos, boundingRadius + m_settings.ExtraSpellRadius)) {
                continue;
            }

            if (SDK::IsLineSpellType(s->SData.SpellType)) {
                Vec2 current = CurrentLineStart(*s);
                const auto proj = SDK::Prediction::Vec2Ext::ProjectOn(heroPos, current, s->EndPosition);
                if (!proj.IsOnSegment) {
                    continue;
                }
                const Vec2 projection = proj.SegmentPoint;

                Vec2 direction = s->Direction;
                if (direction.IsZero()) {
                    direction = (s->EndPosition - current).Normalized();
                }
                const Vec2 side = SDK::Prediction::Vec2Ext::Rotated(direction, 0.5f * static_cast<float>(kPi)).Normalized() *
                    (static_cast<float>(s->SData.Radius) + boundingRadius + m_settings.ExtraSpellRadius + 50.0f);
                if (!side.IsZero()) {
                    out.push_back(projection + side);
                    out.push_back(projection - side);
                }
            } else if (SDK::IsCircleSpellType(s->SData.SpellType)) {
                const float radius = static_cast<float>(s->SData.Radius) +
                    boundingRadius + m_settings.ExtraSpellRadius + 26.0f;
                constexpr int kCirclePoints = 24;
                for (int i = 0; i < kCirclePoints; ++i) {
                    const float angle = (2.0f * static_cast<float>(kPi) * static_cast<float>(i)) /
                        static_cast<float>(kCirclePoints);
                    out.emplace_back(
                        s->EndPosition.x + radius * std::cos(angle),
                        s->EndPosition.y + radius * std::sin(angle));
                }
            }
        }
    }

    float LowestHitTime(const Vec2& heroPos, float boundingRadius, const SkillshotList& skillshots) const {
        float lowest = FLT_MAX;
        for (const auto& s : skillshots) {
            if (s && ShouldConsiderSpell(*s) &&
                InSkillShot(*s, heroPos, boundingRadius + m_settings.ExtraSpellRadius)) {
                lowest = std::min(lowest, SpellHitTime(*s, heroPos));
            }
        }
        return lowest;
    }

    bool UseFastSort(float lowestHitTime) const {
        if (m_settings.ExtremeEvade || m_settings.EvadeMode == 1) {
            return true;
        }
        if (m_settings.EvadeMode == 0 || lowestHitTime == FLT_MAX) {
            return false;
        }
        return m_settings.FastActivationTime + static_cast<float>(SDK::Game::Ping()) +
            m_settings.ExtraDelay > lowestHitTime;
    }

    static bool Better(const PositionInfo& a, const PositionInfo& b, bool fastSort) {
        if (a.wall != b.wall) {
            return !a.wall;
        }
        if (fastSort) {
            if (a.dangerous != b.dangerous) {
                return !a.dangerous;
            }
            if (a.closestDistance != b.closestDistance) {
                return a.closestDistance > b.closestDistance;
            }
            if (a.dangerLevel != b.dangerLevel) {
                return a.dangerLevel < b.dangerLevel;
            }
            if (a.dangerCount != b.dangerCount) {
                return a.dangerCount < b.dangerCount;
            }
            return a.distToMouse < b.distToMouse;
        }

        if (a.rejectPosition != b.rejectPosition) {
            return !a.rejectPosition;
        }
        if (a.dangerous != b.dangerous) {
            return !a.dangerous;
        }
        if (a.dangerLevel != b.dangerLevel) {
            return a.dangerLevel < b.dangerLevel;
        }
        if (a.dangerCount != b.dangerCount) {
            return a.dangerCount < b.dangerCount;
        }
        if (a.hasExtraDistance != b.hasExtraDistance) {
            return !a.hasExtraDistance;
        }
        return a.distToMouse < b.distToMouse;
    }
};

} // namespace Plugins::KuroEvade
