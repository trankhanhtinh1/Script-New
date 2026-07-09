#pragma once

#include "EvadeSettings.h"
#include "PositionInfo.h"

#include "../../../Core/CoreNavGrid.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <vector>

namespace Plugins::KuroEvade {

class EvadeHelper {
public:
    using SkillshotList = std::vector<std::shared_ptr<SDK::Skillshot>>;

    explicit EvadeHelper(const EvadeSettings& settings)
        : m_settings(settings) {
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
                          Vec2& out) const {
        const float speed = std::max(50.0f, player.MoveSpeed());
        const float delayMs = m_settings.ExtraDelay +
            static_cast<float>(SDK::Game::Ping()) +
            std::max(0.0f, m_settings.CurrentWindupDelay);
        const float extraDist = m_settings.ExtraDist;
        const Vec2 mousePos = SDK::Game::CursorPos().To2D();
        const float planeY = player.ServerPosition().y;
        const bool fastSort = UseFastSort(LowestHitTime(heroPos, boundingRadius, skillshots));
        const int maxPosToCheck = m_settings.HigherPrecision ? 150 : 50;
        const int posRadius = m_settings.HigherPrecision ? 25 : 50;

        std::vector<Vec2> positions;
        positions.reserve(static_cast<std::size_t>(maxPosToCheck) + 40);
        if (m_settings.KurokamiPosition) {
            AddKurokamiPositions(heroPos, boundingRadius, skillshots, positions);
        }
        AddFastestPositions(heroPos, boundingRadius, skillshots, positions);

        int posChecked = 0;
        int radiusIndex = 0;

        while (posChecked < maxPosToCheck) {
            radiusIndex++;
            const int curRadius = radiusIndex * (2 * posRadius);
            const int circleChecks = std::max(1,
                static_cast<int>(std::ceil((2.0 * kPi * curRadius) / (2.0 * posRadius))));

            for (int i = 1; i < circleChecks && posChecked < maxPosToCheck; ++i) {
                posChecked++;
                const double rad = (2.0 * kPi / (circleChecks - 1)) * i;
                positions.emplace_back(
                    std::floor(heroPos.x + curRadius * static_cast<float>(std::cos(rad))),
                    std::floor(heroPos.y + curRadius * static_cast<float>(std::sin(rad))));
            }
        }

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
        return false;
    }

    static int DangerValue(const SDK::Skillshot& spell) {
        return std::max(1, spell.SData.DangerValue);
    }

    bool ShouldConsiderSpell(const SDK::Skillshot& spell) const {
        if (spell.HasExpired()) {
            return false;
        }
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

    static float SpellHitTime(const SDK::Skillshot& spell, const Vec2& pos) {
        const int now = SDK::Variables::TickCount();
        if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(&spell)) {
            const float speed = std::max(1.0f, static_cast<float>(spell.SData.MissileSpeed));
            const Vec2 head = missile->GetMissilePosition(0);
            const float dist = head.Distance(pos);
            return (dist / speed) * 1000.0f;
        }

        return static_cast<float>(spell.StartTime + spell.SData.Delay - now);
    }

    static bool InSkillShot(const SDK::Skillshot& spell, const Vec2& pos, float radius) {
        const SDK::SpellType type = spell.SData.SpellType;
        const float spellRadius = static_cast<float>(spell.SData.Radius);

        if (SDK::IsLineSpellType(type)) {
            const auto proj = SDK::Prediction::Vec2Ext::ProjectOn(pos, spell.StartPosition, spell.EndPosition);
            return proj.IsOnSegment && proj.SegmentPoint.Distance(pos) <= spellRadius + radius;
        }
        if (SDK::IsCircleSpellType(type)) {
            return pos.Distance(spell.EndPosition) <= spellRadius + radius;
        }
        return InPolygon(spell, pos, radius);
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    const EvadeSettings& m_settings;

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

    float GetClosestDistanceApproach(const SDK::Skillshot& spell,
                                     const Vec2& pos, float speed, float delayMs,
                                     const Vec2& heroPos, float boundingRadius,
                                     float extraDist) const {
        const Vec2 walkDir = (pos - heroPos).Normalized();
        const SDK::SpellType type = spell.SData.SpellType;
        const float spellRadius = static_cast<float>(spell.SData.Radius);

        if (SDK::IsLineSpellType(type)) {
            const float missileSpeed = std::max(1.0f, static_cast<float>(spell.SData.MissileSpeed));

            Vec2 spellPos = spell.StartPosition;
            if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(&spell)) {
                spellPos = missile->GetMissilePosition(static_cast<int>(delayMs));
            }
            const Vec2 spellEnd = spell.EndPosition;
            const Vec2 spellVel = spell.Direction * missileSpeed;

            Vec2 cHero;
            Vec2 cSpell;
            const float cpa = CpaPointsEx(
                heroPos, walkDir * speed, spellPos, spellVel, cHero, cSpell);

            const Vec2 extendedPos = pos + walkDir * boundingRadius;
            const auto projHero = SDK::Prediction::Vec2Ext::ProjectOn(cHero, heroPos, extendedPos);
            const auto projSpell = SDK::Prediction::Vec2Ext::ProjectOn(cSpell, spellPos, spellEnd);
            const bool heroOn = projHero.IsOnSegment;
            const bool spellOn = projSpell.IsOnSegment;

            const float checkDist = boundingRadius + spellRadius + extraDist;
            if (spellOn && heroOn) {
                return std::max(0.0f, cpa - checkDist);
            }
            return checkDist;
        }

        if (SDK::IsCircleSpellType(type)) {
            const int now = SDK::Variables::TickCount();
            const float hitTime = std::max(0.0f,
                static_cast<float>(spell.StartTime + spell.SData.Delay - now) - delayMs);
            const float walkRange = heroPos.Distance(pos);
            const float predictedRange = speed * (hitTime / 1000.0f);
            const Vec2 tHeroPos = heroPos + walkDir * std::min(predictedRange, walkRange);
            return std::max(0.0f, tHeroPos.Distance(spell.EndPosition) - (spellRadius + extraDist));
        }

        return 1.0f;
    }

    bool PredictSpellCollision(const SDK::Skillshot& spell, const Vec2& pos, float speed,
                               float delayMs, const Vec2& heroPos, float boundingRadius,
                               float extraDist) const {
        return GetClosestDistanceApproach(spell, pos, speed, delayMs, heroPos,
                                          boundingRadius, extraDist + 10.0f) == 0.0f;
    }

    float DistanceToEnemyTurret(const Vec2& pos) const {
        float best = FLT_MAX;
        for (const auto& turret : SDK::GameObjects::EnemyTurrets()) {
            if (!turret.IsValid() || turret.IsDead()) {
                continue;
            }
            best = std::min(best, turret.Position().To2D().Distance(pos));
        }
        return best;
    }

    float DistanceToEnemyChampion(const Vec2& pos) const {
        float best = FLT_MAX;
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) {
                continue;
            }
            best = std::min(best, enemy.ServerPosition().To2D().Distance(pos));
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

    bool WallBlocks(const Vec2& heroPos, const Vec2& pos, float planeY) const {
        const Vec3 dest = Vec3::From2D(pos, planeY);
        const Vec3 from = Vec3::From2D(heroPos, planeY);
        if (!CoreNavGrid::IsWalkable(dest)) {
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
        info.distToEnemy = DistanceToEnemyChampion(pos);
        info.wall = WallBlocks(heroPos, pos, planeY);

        for (const auto& s : skillshots) {
            if (!s || !ShouldConsiderSpell(*s)) {
                continue;
            }
            const int danger = DangerValue(*s);
            const float cpa = GetClosestDistanceApproach(
                *s, pos, speed, delayMs, heroPos, boundingRadius,
                extraDist + m_settings.ExtraSpellRadius);
            info.closestDistance = std::min(info.closestDistance, cpa);

            if (InSkillShot(*s, pos, std::max(0.0f, boundingRadius - 8.0f) + m_settings.ExtraSpellRadius) ||
                PredictSpellCollision(*s, pos, speed, delayMs, heroPos, boundingRadius,
                                      extraDist + m_settings.ExtraSpellRadius)) {
                info.dangerLevel = std::max(info.dangerLevel, danger);
                info.dangerCount += danger;
            }
        }

        info.rejectPosition = m_settings.RejectMinDistance > 0.0f &&
            info.closestDistance < m_settings.RejectMinDistance;
        info.hasExtraDistance = m_settings.ExtraAvoidDistance > 0.0f &&
            CheckDangerousPosition(pos, boundingRadius + m_settings.ExtraAvoidDistance, skillshots);
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
                Vec2 current = s->StartPosition;
                if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(s.get())) {
                    current = missile->GetMissilePosition(0);
                }
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
                Vec2 current = s->StartPosition;
                if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(s.get())) {
                    current = missile->GetMissilePosition(0);
                }
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

private:
    static float CpaTime(const Vec2& p1, const Vec2& v1, const Vec2& p2, const Vec2& v2) {
        const Vec2 dv = v1 - v2;
        const float dv2 = dv.Dot(dv);
        if (dv2 < 0.00000001f) {
            return 0.0f;
        }
        const Vec2 w0 = p1 - p2;
        return -w0.Dot(dv) / dv2;
    }

    static float CpaPointsEx(const Vec2& p1, const Vec2& v1,
                             const Vec2& p2, const Vec2& v2,
                             Vec2& out1, Vec2& out2) {
        const float ctime = std::max(0.0f, CpaTime(p1, v1, p2, v2));
        out1 = p1 + v1 * ctime;
        out2 = p2 + v2 * ctime;
        return out1.Distance(out2);
    }
};

} // namespace Plugins::KuroEvade
