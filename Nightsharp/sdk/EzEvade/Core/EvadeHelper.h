#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/EzEvade/Helpers/Position.h"
#include "sdk/EzEvade/Helpers/PositionInfo.h"
#include "sdk/EzEvade/Spells/Spell.h"
#include "sdk/EzEvade/Spells/SpellRuntime.h"
#include "sdk/EzEvade/Spells/SpellDetector.h"
#include "sdk/EzEvade/Spells/SpellData.h"
#include <algorithm>
#include <cfloat>
#include <vector>

namespace EzEvade {

class EvadeHelper {
public:
    static inline bool FastEvadeMode = false;

    static bool SegmentHitsWall(const Vec2& from, const Vec2& to) {
        Vec2 dir = (to - from).Normalized();
        float dist = from.Distance(to);
        if (dist <= 1.0f) {
            return false;
        }

        const int steps = std::max(2, (int)(dist / 50.0f));
        for (int i = 1; i <= steps; ++i) {
            Vec2 p = from + dir * (dist * ((float)i / (float)steps));
            if (SDK::GameObject::IsWallAt(Vec3::From2D(p, SDK::GameObjects::Player.GetPosition().y))) {
                return true;
            }
        }
        return false;
    }

    static Vec3 GetNearWallPoint(const Vec3& start, const Vec3& end, float sampleStep = 35.0f) {
        const float totalDist = start.Distance2D(end);
        if (totalDist <= 1.0f) {
            return Vec3();
        }

        const float step = std::max(sampleStep, 5.0f);
        const int samples = std::max(2, (int)(totalDist / step));
        for (int i = 1; i <= samples; ++i) {
            const float t = (float)i / (float)samples;
            const Vec3 probe = Vec3::Lerp(start, end, t);
            if (SDK::GameObject::IsWallAt(probe)) {
                return probe;
            }
        }
        return Vec3();
    }

    static bool PlayerInSkillShot(const Spell& spell) {
        return Position::InSkillShot(ObjectCache::MyHeroCache.ServerPos2D, spell, ObjectCache::MyHeroCache.BoundingRadius);
    }

    static PositionInfo CanHeroWalkToPos(const Vec2& pos, float speed, float delay, float extraDist,
                                         bool useServerPosition = true) {
        (void)useServerPosition;

        int posDangerLevel = 0;
        int posDangerCount = 0;
        float closestDistance = FLT_MAX;
        std::vector<int> dodgeableSpells = {};
        std::vector<int> undodgeableSpells = {};

        const Vec2 heroPos = ObjectCache::MyHeroCache.ServerPos2D;
        const Vec2 walkDir = (pos - heroPos).Normalized();
        const Vec2 projectedHeroPos = heroPos + walkDir * speed * (delay / 1000.0f);

        for (const auto* spell : SpellRuntime::ActiveSpells()) {
            if (!spell || !spell->Info) {
                continue;
            }

            float closest = FLT_MAX;
            if (spell->Type == SpellType::Line || spell->Type == SpellType::Arc) {
                auto proj = SDK::GeometryAdv::ProjectOn(projectedHeroPos, spell->CurrentSpellPosition, SpellExtensions::GetSpellEndPosition(*spell));
                closest = projectedHeroPos.Distance(proj.SegmentPoint) - (spell->Radius + ObjectCache::MyHeroCache.BoundingRadius + extraDist);
            } else if (spell->Type == SpellType::Circular) {
                closest = projectedHeroPos.Distance(spell->EndPos) - (spell->Radius + extraDist);
            } else if (spell->Type == SpellType::Cone) {
                closest = projectedHeroPos.Distance(spell->EndPos) - (spell->Radius + extraDist);
            }

            if (closest < closestDistance) {
                closestDistance = std::max(0.0f, closest);
            }

            const bool hitNow = Position::InSkillShot(projectedHeroPos, *spell,
                ObjectCache::MyHeroCache.BoundingRadius + extraDist);

            if (hitNow) {
                posDangerLevel = std::max(posDangerLevel, spell->Dangerlevel);
                posDangerCount += spell->Dangerlevel;
                undodgeableSpells.push_back(spell->SpellID);
            } else {
                dodgeableSpells.push_back(spell->SpellID);
            }
        }

        PositionInfo out(
            pos,
            posDangerLevel,
            posDangerCount,
            posDangerCount > 0,
            closestDistance,
            dodgeableSpells,
            undodgeableSpells
        );
        out.ClosestDistance = closestDistance;
        out.Speed = speed;
        return out;
    }

    static bool PositionInfoStillValid(const PositionInfo& posInfo, float speedOverride = 0.0f) {
        const float speed = speedOverride > 0.0f ? speedOverride : ObjectCache::MyHeroCache.MoveSpeed;
        const PositionInfo now = CanHeroWalkToPos(
            posInfo.Position,
            speed,
            ObjectCache::GamePing,
            (float)ObjectCache::Menu.GetSlider("ExtraCPADistance", 0)
        );
        return now.PosDangerCount <= posInfo.PosDangerCount;
    }

    static std::vector<Vec2> GetFastestPositions() {
        std::vector<Vec2> positions;
        const Vec2 hero = ObjectCache::MyHeroCache.ServerPos2D;
        const Vec3 mouse3 = SDK::Game::GetMouseWorldPos();
        const Vec2 mouse = mouse3.To2D();

        const Vec2 dir = (mouse - hero).Normalized();
        const Vec2 pDir = dir.Perpendicular();
        positions.push_back(hero + dir * 50.0f);
        positions.push_back(hero + dir * 120.0f);
        positions.push_back(hero + dir * 220.0f);
        positions.push_back(hero + dir * 320.0f);
        positions.push_back(hero + pDir * 150.0f);
        positions.push_back(hero - pDir * 150.0f);
        return positions;
    }

    static Vec2 GetExtendedSafePosition(const Vec2& from, const Vec2& toward, float extraDistance) {
        if (extraDistance <= 0.0f) {
            return toward;
        }

        Vec2 cur = toward;
        Vec2 dir = (toward - from).Normalized();
        for (int i = 0; i < 6; ++i) {
            Vec2 next = cur + dir * (extraDistance / 6.0f);
            if (Position::CheckDangerousPos(next, 0.0f)) {
                break;
            }
            cur = next;
        }
        return cur;
    }

    static PositionInfo GetBestPosition() {
        FastEvadeMode = false;

        int maxPosToCheck = ObjectCache::Menu.GetBool("HigherPrecision", false) ? 150 : 50;
        int posRadius = ObjectCache::Menu.GetBool("HigherPrecision", false) ? 25 : 50;
        const float extraDelayBuffer = (float)ObjectCache::Menu.GetSlider("ExtraPingBuffer", 65);
        const float extraEvadeDistance = (float)ObjectCache::Menu.GetSlider("ExtraEvadeDistance", 100);
        const float extraDist = (float)ObjectCache::Menu.GetSlider("ExtraCPADistance", 10);
        const Vec2 mouse = SDK::Game::GetMouseWorldPos().To2D();

        std::vector<PositionInfo> candidates;
        candidates.reserve((size_t)maxPosToCheck + 16);

        for (const auto& p : GetFastestPositions()) {
            PositionInfo info = CanHeroWalkToPos(
                p, ObjectCache::MyHeroCache.MoveSpeed, extraDelayBuffer + ObjectCache::GamePing, extraDist);
            info.IsDangerousPos = Position::CheckDangerousPos(p, 6.0f);
            info.HasExtraDistance = extraEvadeDistance > 0.0f && Position::CheckDangerousPos(p, extraEvadeDistance);
            info.DistanceToMouse = p.Distance(mouse);
            info.PosDistToChamps = Position::GetDistanceToChampions(p);
            candidates.push_back(info);
        }

        for (const auto& p : Position::GetSurroundingPositions(maxPosToCheck, posRadius)) {
            PositionInfo info = CanHeroWalkToPos(
                p, ObjectCache::MyHeroCache.MoveSpeed, extraDelayBuffer + ObjectCache::GamePing, extraDist);
            info.IsDangerousPos = Position::CheckDangerousPos(p, 6.0f);
            info.HasExtraDistance = extraEvadeDistance > 0.0f && Position::CheckDangerousPos(p, extraEvadeDistance);
            info.DistanceToMouse = p.Distance(mouse);
            info.PosDistToChamps = Position::GetDistanceToChampions(p);
            candidates.push_back(info);
        }

        std::sort(candidates.begin(), candidates.end(), [](const PositionInfo& a, const PositionInfo& b) {
            if (a.RejectPosition != b.RejectPosition) return a.RejectPosition < b.RejectPosition;
            if (a.PosDangerLevel != b.PosDangerLevel) return a.PosDangerLevel < b.PosDangerLevel;
            if (a.PosDangerCount != b.PosDangerCount) return a.PosDangerCount < b.PosDangerCount;
            return a.DistanceToMouse < b.DistanceToMouse;
        });

        for (auto& posInfo : candidates) {
            if (CheckPathCollision(SDK::GameObjects::Player, posInfo.Position)) {
                continue;
            }
            if (!PositionInfoStillValid(posInfo)) {
                continue;
            }
            if (extraEvadeDistance > 0.0f && Position::CheckDangerousPos(posInfo.Position, extraEvadeDistance)) {
                posInfo.Position = GetExtendedSafePosition(ObjectCache::MyHeroCache.ServerPos2D, posInfo.Position, extraEvadeDistance);
            }
            return posInfo;
        }

        return PositionInfo::SetAllUndodgeable();
    }

    static PositionInfo GetBestPositionMovementBlock(const Vec2& movePos) {
        const float extraEvadeDistance = (float)ObjectCache::Menu.GetSlider("ExtraAvoidDistance", 50);
        const float extraDist = (float)ObjectCache::Menu.GetSlider("ExtraCPADistance", 10);
        const float extraDelayBuffer = (float)ObjectCache::Menu.GetSlider("ExtraPingBuffer", 65);

        std::vector<PositionInfo> candidates;
        candidates.reserve(64);

        for (const auto& p : Position::GetSurroundingPositions(64, 50)) {
            PositionInfo info = CanHeroWalkToPos(
                p, ObjectCache::MyHeroCache.MoveSpeed, extraDelayBuffer + ObjectCache::GamePing, extraDist);
            info.IsDangerousPos = Position::CheckDangerousPos(p, 6.0f) || CheckMovePath(p);
            info.DistanceToMouse = p.Distance(movePos);
            info.HasExtraDistance = extraEvadeDistance > 0.0f && Position::HasExtraAvoidDistance(p, extraEvadeDistance);
            candidates.push_back(info);
        }

        std::sort(candidates.begin(), candidates.end(), [](const PositionInfo& a, const PositionInfo& b) {
            if (a.IsDangerousPos != b.IsDangerousPos) return a.IsDangerousPos < b.IsDangerousPos;
            if (a.PosDangerLevel != b.PosDangerLevel) return a.PosDangerLevel < b.PosDangerLevel;
            if (a.HasExtraDistance != b.HasExtraDistance) return a.HasExtraDistance < b.HasExtraDistance;
            return a.DistanceToMouse < b.DistanceToMouse;
        });

        for (auto& info : candidates) {
            if (!CheckPathCollision(SDK::GameObjects::Player, info.Position)) {
                return info;
            }
        }

        return PositionInfo();
    }

    static PositionInfo GetBestPositionBlink() {
        const float extraEvadeDistance = std::max(100.0f, (float)ObjectCache::Menu.GetSlider("ExtraEvadeDistance", 100));
        const float minComfortZone = (float)ObjectCache::Menu.GetSlider("MinComfortZone", 550);
        const Vec2 mouse = SDK::Game::GetMouseWorldPos().To2D();

        std::vector<PositionInfo> candidates;
        for (const auto& p : Position::GetSurroundingPositions(100, 50)) {
            PositionInfo info(p, Position::CheckDangerousPos(p, 6.0f), p.Distance(mouse));
            info.HasExtraDistance = extraEvadeDistance > 0.0f && Position::CheckDangerousPos(p, extraEvadeDistance);
            info.PosDistToChamps = Position::GetDistanceToChampions(p);
            if (info.PosDistToChamps >= minComfortZone) {
                candidates.push_back(info);
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const PositionInfo& a, const PositionInfo& b) {
            if (a.IsDangerousPos != b.IsDangerousPos) return a.IsDangerousPos < b.IsDangerousPos;
            if (a.HasExtraDistance != b.HasExtraDistance) return a.HasExtraDistance < b.HasExtraDistance;
            return a.DistanceToMouse < b.DistanceToMouse;
        });

        for (auto& info : candidates) {
            if (!CheckPointCollision(SDK::GameObjects::Player, info.Position)) {
                return info;
            }
        }

        return PositionInfo();
    }

    static PositionInfo GetBestPositionDash(const EvadeSpellData& spell) {
        const float extraDelayBuffer = (float)ObjectCache::Menu.GetSlider("ExtraPingBuffer", 65);
        const float extraEvadeDistance = std::max(100.0f, (float)ObjectCache::Menu.GetSlider("ExtraEvadeDistance", 100));
        const float extraDist = (float)ObjectCache::Menu.GetSlider("ExtraCPADistance", 10);
        const Vec2 heroPoint = ObjectCache::MyHeroCache.ServerPos2DPing.IsZero() ? ObjectCache::MyHeroCache.ServerPos2D : ObjectCache::MyHeroCache.ServerPos2DPing;
        const Vec2 mouse = SDK::Game::GetMouseWorldPos().To2D();
        const float minRange = spell.fixedRange ? spell.range : 50.0f;
        const float maxRange = spell.fixedRange ? spell.range : spell.range;

        std::vector<PositionInfo> candidates;
        for (const auto& p : Position::GetSurroundingPositions(120, 50)) {
            const float dist = heroPoint.Distance(p);
            if (dist < minRange || dist > maxRange) {
                continue;
            }

            PositionInfo info = CanHeroWalkToPos(p, spell.speed, extraDelayBuffer + ObjectCache::GamePing, extraDist);
            info.IsDangerousPos = Position::CheckDangerousPos(p, 6.0f);
            info.HasExtraDistance = extraEvadeDistance > 0.0f && Position::CheckDangerousPos(p, extraEvadeDistance);
            info.DistanceToMouse = p.Distance(mouse);
            info.PosDistToChamps = Position::GetDistanceToChampions(p);
            candidates.push_back(info);
        }

        std::sort(candidates.begin(), candidates.end(), [](const PositionInfo& a, const PositionInfo& b) {
            if (a.IsDangerousPos != b.IsDangerousPos) return a.IsDangerousPos < b.IsDangerousPos;
            if (a.PosDangerLevel != b.PosDangerLevel) return a.PosDangerLevel < b.PosDangerLevel;
            if (a.PosDangerCount != b.PosDangerCount) return a.PosDangerCount < b.PosDangerCount;
            if (a.HasExtraDistance != b.HasExtraDistance) return a.HasExtraDistance < b.HasExtraDistance;
            return a.DistanceToMouse < b.DistanceToMouse;
        });

        for (auto& info : candidates) {
            if (CheckPathCollision(SDK::GameObjects::Player, info.Position)) {
                continue;
            }
            if (PositionInfoStillValid(info, spell.speed)) {
                return info;
            }
        }

        return PositionInfo();
    }

    static PositionInfo GetBestPositionTargetedDash(const EvadeSpellData& spell) {
        (void)spell;
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) {
            return PositionInfo();
        }

        PositionInfo best;
        bool found = false;
        const Vec2 mouse = SDK::Game::GetMouseWorldPos().To2D();

        auto tryCandidate = [&](const SDK::GameObject& obj) {
            if (!obj.IsValid() || !obj.IsAlive() || obj.IsDead()) return;
            Vec2 targetPos = obj.GetServerPosition().To2D();
            PositionInfo info = CanHeroWalkToPos(targetPos, ObjectCache::MyHeroCache.MoveSpeed, ObjectCache::GamePing, 0.0f);
            info.DistanceToMouse = targetPos.Distance(mouse);
            info.Target = obj;

            if (!found || info.PosDangerCount < best.PosDangerCount ||
                (info.PosDangerCount == best.PosDangerCount && info.DistanceToMouse < best.DistanceToMouse)) {
                best = info;
                found = true;
            }
        };

        for (const auto& hero : SDK::GameObjects::AllyHeroes) {
            if (!hero.IsMe()) {
                tryCandidate(hero);
            }
        }
        for (const auto& minion : SDK::GameObjects::AllyMinions) {
            tryCandidate(minion);
        }

        return found ? best : PositionInfo();
    }

    static bool CheckPathCollision(const SDK::GameObject& unit, const Vec2& movePos) {
        if (!unit.IsValid()) return true;
        const Vec2 from = unit.GetServerPosition().To2D();
        if (from.IsZero()) return true;
        return SegmentHitsWall(from, movePos);
    }

    static bool CheckPointCollision(const SDK::GameObject& unit, const Vec2& movePos) {
        if (!unit.IsValid()) return true;
        const Vec2 from = unit.GetServerPosition().To2D();
        if (from.IsZero()) return true;
        return SegmentHitsWall(from, movePos);
    }

    static bool CheckMoveToDirection(const Vec2& from, const Vec2& movePos, float extraDelay = 0.0f) {
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        (void)extraDelay;
        Vec2 dir = (movePos - from).Normalized();
        for (const auto* spell : SpellRuntime::ActiveSpells()) {
            if (!spell) continue;
            if (!Position::InSkillShot(from, *spell, ObjectCache::MyHeroCache.BoundingRadius)) {
                if (spell->Type == SpellType::Line) {
                    if (SpellExtensions::LineIntersectLinearSpell(*spell, from, movePos)) {
                        return true;
                    }
                } else if (spell->Type == SpellType::Circular) {
                    Vec2 cHeroPos, cSpellPos;
                    const float cpa = MathUtils::GetCollisionDistanceEx(
                        from, dir * me.GetMoveSpeed(), 1.0f,
                        spell->EndPos, Vec2(0, 0), spell->Radius,
                        cHeroPos, cSpellPos);
                    auto proj = SDK::GeometryAdv::ProjectOn(cHeroPos, from, movePos);
                    if (proj.IsOnSegment && cpa != FLT_MAX) {
                        return true;
                    }
                } else if (spell->Type == SpellType::Cone) {
                    if (MathUtils::CheckLineIntersection(from, movePos, spell->CnStart, spell->CnLeft) ||
                        MathUtils::CheckLineIntersection(from, movePos, spell->CnLeft, spell->CnRight) ||
                        MathUtils::CheckLineIntersection(from, movePos, spell->CnRight, spell->CnStart)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    static bool CheckMovePath(const Vec2& movePos, float extraDelay = 0.0f) {
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        const Vec2 from = me.GetServerPosition().To2D();
        if (from.IsZero()) {
            return false;
        }

        if (SegmentHitsWall(from, movePos)) {
            return true;
        }

        return CheckMoveToDirection(from, movePos, extraDelay);
    }
};

// Free-function alias to avoid namespace/type lookup issues in some include orders.
inline Vec3 GetNearWallPoint(const Vec3& start, const Vec3& end, float sampleStep = 35.0f) {
    return EvadeHelper::GetNearWallPoint(start, end, sampleStep);
}

} // namespace EzEvade
