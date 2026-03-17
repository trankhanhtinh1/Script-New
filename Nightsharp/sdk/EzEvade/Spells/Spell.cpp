#include "Spell.h"
#include "../../Math/Polygon.h"
#include "../../Math/MathUtils.h"
#include "../Helpers/ObjectCache.h"
#include "../Helpers/Position.h"
#include "../Utils/EvadeUtils.h"
#include <cfloat>

namespace EzEvade {

    float Spell::GetSpellRadius() const
    {
        float radiusVal = (float)ObjectCache::GetSlider(info.spellName + "SpellRadius");
        float extraRadius = (float)ObjectCache::GetSlider("ExtraSpellRadius");

        if (info.hasEndExplosion && spellType == SpellType::Circular)
        {
            return info.secondaryRadius + extraRadius;
        }

        if (spellType == SpellType::Arc)
        {
            float spellRange = startPos.Distance(endPos);
            float arcRadius = info.radius * (1.0f + spellRange / 100.0f) + extraRadius;
            return arcRadius;
        }

        return radiusVal + extraRadius;
    }

    int Spell::GetSpellDangerLevel() const
    {
        return ObjectCache::GetDangerLevel(info.spellName + "DangerLevel");
    }

    std::string Spell::GetSpellDangerString() const
    {
        switch (GetSpellDangerLevel())
        {
        case 1:  return "Low";
        case 3:  return "High";
        case 4:  return "Extreme";
        default: return "Normal";
        }
    }

    bool Spell::hasProjectile() const
    {
        return info.projectileSpeed > 0 && info.projectileSpeed != FLT_MAX;
    }

    Vec2 Spell::GetSpellProjection(const Vec2& pos, bool predictPos) const
    {
        if (spellType == SpellType::Line || spellType == SpellType::Arc)
        {
            if (predictPos)
            {
                Vec2 spellPos = currentSpellPosition;
                Vec2 spellEnd = GetSpellEndPosition();
                return SDK::Geometry::ProjectOn(pos, spellPos, spellEnd).segmentPoint;
            }

            return SDK::Geometry::ProjectOn(pos, startPos, endPos).segmentPoint;
        }

        if (spellType == SpellType::Circular)
        {
            return endPos;
        }

        if (spellType == SpellType::Cone)
        {
            // Empty in C# code as well
        }

        return Vec2(0, 0);
    }

    SDK::GameObject* Spell::CheckSpellCollision(bool ignoreSelf) const
    {
        if (info.collisionObjects.empty())
        {
            return nullptr;
        }

        // Note: SDK::GameObjects stores values, not pointers.
        // For collision we check positions but can't return a persistent pointer easily.
        // We do a simple proximity check and return nullptr (collision = no block found).
        // Full implementation would need object caching by address.

        Vec2 spellPos = currentSpellPosition;

        if (info.CollidesWithChampions())
        {
            for (auto& hero : SDK::GameObjects::AllyHeroes)
            {
                if (!hero.IsAlive()) continue;
                if (ignoreSelf && hero.GetNetId() == SDK::GameObjects::Player.GetNetId())
                    continue;
                Vec2 heroPos = hero.GetPosition().To2D();
                if (heroPos.Distance(spellPos) < radius + 65.0f)
                {
                    if (Position::InSkillShot(heroPos, *this, 65.0f, false))
                    {
                        // Collision detected — return non-null sentinel
                        // We can't return a stable pointer to a value-type object easily,
                        // so we store the player address temporarily. 
                        // For now, signal collision with a reinterpret hack:
                        static SDK::GameObject s_collisionDummy;
                        s_collisionDummy = hero;
                        return &s_collisionDummy;
                    }
                }
            }
        }

        if (info.CollidesWithMinions())
        {
            for (auto& minion : SDK::GameObjects::AllyMinions)
            {
                if (!minion.IsAlive()) continue;
                std::string skinName = minion.GetName();
                std::transform(skinName.begin(), skinName.end(), skinName.begin(), ::tolower);
                if (skinName == "teemomushroom" || skinName == "shacobox")
                    continue;
                Vec2 minionPos = minion.GetPosition().To2D();
                if (minionPos.Distance(spellPos) < radius + 65.0f)
                {
                    if (Position::InSkillShot(minionPos, *this, 65.0f, false))
                    {
                        static SDK::GameObject s_collisionDummy;
                        s_collisionDummy = minion;
                        return &s_collisionDummy;
                    }
                }
            }
        }

        return nullptr;
    }

    float Spell::GetSpellHitTime(const Vec2& pos) const
    {
        switch (spellType)
        {
        case SpellType::Line:
            if (info.projectileSpeed >= FLT_MAX)
                return std::max(0.0f, endTime - EvadeUtils::TickCount() - ObjectCache::gamePing);

            {
                Vec2 spellPos = GetCurrentSpellPosition(true, ObjectCache::gamePing, 0);
                return 1000.0f * spellPos.Distance(pos) / info.projectileSpeed;
            }
        case SpellType::Cone:
        case SpellType::Circular:
            return std::max(0.0f, endTime - EvadeUtils::TickCount() - ObjectCache::gamePing);
        }

        return FLT_MAX;
    }

    bool Spell::CanHeroEvade(const SDK::GameObject& hero, float& rEvadeTime, float& rSpellHitTime) const
    {
        Vec2 heroPos = hero.GetPosition().To2D();
        float evadeTimeCalc = 0;
        float spellHitTimeCalc = 0;
        float speed = hero.GetMoveSpeed(); // Mock
        float delay = 0;
        float boundingRadius = 65.0f; // Mock bounding radius

        // EvadeSpell movement buff logic omitted for brevity in port, 
        // as EvadeSpells list hasn't been implemented yet.
        // speed += ...
        // delay += ...

        if (spellType == SpellType::Line)
        {
            Vec2 projection = SDK::Geometry::ProjectOn(heroPos, startPos, endPos).segmentPoint;
            evadeTimeCalc = 1000.0f * (radius - heroPos.Distance(projection) + boundingRadius) / speed;
            spellHitTimeCalc = GetSpellHitTime(projection);
        }
        else if (spellType == SpellType::Circular)
        {
            evadeTimeCalc = 1000.0f * (radius - heroPos.Distance(endPos)) / speed;
            spellHitTimeCalc = GetSpellHitTime(heroPos);
        }
        else if (spellType == SpellType::Cone)
        {
            Vec2 sides[3] = {
                SDK::Geometry::ProjectOn(heroPos, cnStart, cnLeft).segmentPoint,
                SDK::Geometry::ProjectOn(heroPos, cnLeft, cnRight).segmentPoint,
                SDK::Geometry::ProjectOn(heroPos, cnRight, cnStart).segmentPoint
            };

            Vec2 p = sides[0];
            float minDist = heroPos.DistanceSquared(sides[0]);
            for (int i = 1; i < 3; i++) {
                float dist = heroPos.DistanceSquared(sides[i]);
                if (dist < minDist) {
                    minDist = dist;
                    p = sides[i];
                }
            }

            evadeTimeCalc = 1000.0f * (info.range / 2.0f - heroPos.Distance(p) + boundingRadius) / speed;
            spellHitTimeCalc = GetSpellHitTime(heroPos);
        }

        rEvadeTime = evadeTimeCalc;
        rSpellHitTime = spellHitTimeCalc;

        return spellHitTimeCalc - delay > evadeTimeCalc;
    }

    Vec2 Spell::GetSpellEndPosition() const
    {
        return (predictedEndPos.x == 0 && predictedEndPos.y == 0) ? endPos : predictedEndPos;
    }

    void Spell::UpdateSpellInfo()
    {
        currentSpellPosition = GetCurrentSpellPosition();
        currentNegativePosition = GetCurrentSpellPosition(true, 0, 0);
        dangerlevel = GetSpellDangerLevel();
    }

    Vec2 Spell::GetCurrentSpellPosition(bool allowNegative, float delay, float extraDistance) const
    {
        Vec2 spellPos = startPos;

        if (!info.updatePosition)
        {
            return spellPos;
        }

        if (spellType == SpellType::Line || spellType == SpellType::Arc)
        {
            float spellTime = EvadeUtils::TickCount() - startTime - info.spellDelay - std::max(0.0f, info.extraEndTime);

            if (info.projectileSpeed >= FLT_MAX)
            {
                return startPos;
            }

            if (spellTime >= 0 || allowNegative)
            {
                spellPos = startPos + direction * info.projectileSpeed * (spellTime / 1000.0f);
            }
        }
        else if (spellType == SpellType::Circular || spellType == SpellType::Cone)
        {
            spellPos = endPos;
        }

        if (spellObject != nullptr && spellObject->IsValid() && spellObject->IsVisible())
        {
            if (spellObject->GetPosition().To2D().Distance(SDK::GameObjects::Player.GetPosition().To2D()) < info.range + 1000.0f)
            {
                spellPos = spellObject->GetPosition().To2D();
            }
        }

        if (delay > 0 && info.projectileSpeed < FLT_MAX && spellType == SpellType::Line)
        {
            spellPos = spellPos + direction * info.projectileSpeed * (delay / 1000.0f);
        }

        if (extraDistance > 0 && info.projectileSpeed < FLT_MAX && spellType == SpellType::Line)
        {
            spellPos = spellPos + direction * extraDistance;
        }

        return spellPos;
    }

    bool Spell::LineIntersectLinearSpell(const Vec2& a, const Vec2& b) const
    {
        float myBoundingRadius = 65.0f; // SDK::GameObjects::Player->BoundingRadius
        Vec2 spellDir = direction;
        Vec2 pSpellDir = direction.Perpendicular();
        float spellRadius = radius;
        Vec2 spellPos = currentSpellPosition;
        Vec2 ePos = GetSpellEndPosition();

        Vec2 startRightPos = spellPos + pSpellDir * (spellRadius + myBoundingRadius);
        Vec2 startLeftPos = spellPos - pSpellDir * (spellRadius + myBoundingRadius);
        Vec2 endRightPos = ePos + pSpellDir * (spellRadius + myBoundingRadius);
        Vec2 endLeftPos = ePos - pSpellDir * (spellRadius + myBoundingRadius);

        auto r1 = SDK::Geometry::SegmentIntersection(a, b, startRightPos, startLeftPos);
        auto r2 = SDK::Geometry::SegmentIntersection(a, b, endRightPos, endLeftPos);
        auto r3 = SDK::Geometry::SegmentIntersection(a, b, startRightPos, endRightPos);
        auto r4 = SDK::Geometry::SegmentIntersection(a, b, startLeftPos, endLeftPos);

        return r1.Intersects || r2.Intersects || r3.Intersects || r4.Intersects;
    }

    bool Spell::LineIntersectLinearSpellEx(const Vec2& a, const Vec2& b, Vec2& intersection) const
    {
        float myBoundingRadius = 65.0f;
        Vec2 spellDir = direction;
        Vec2 pSpellDir = direction.Perpendicular();
        float spellRadius = radius;
        Vec2 spellPos = currentSpellPosition - spellDir * myBoundingRadius;
        Vec2 ePos = GetSpellEndPosition() + spellDir * myBoundingRadius;

        Vec2 startRightPos = spellPos + pSpellDir * (spellRadius + myBoundingRadius);
        Vec2 startLeftPos = spellPos - pSpellDir * (spellRadius + myBoundingRadius);
        Vec2 endRightPos = ePos + pSpellDir * (spellRadius + myBoundingRadius);
        Vec2 endLeftPos = ePos - pSpellDir * (spellRadius + myBoundingRadius);

        std::vector<SDK::Geometry::IntersectionResult> intersects;
        intersects.push_back(SDK::Geometry::SegmentIntersection(a, b, startRightPos, startLeftPos));
        intersects.push_back(SDK::Geometry::SegmentIntersection(a, b, endRightPos, endLeftPos));
        intersects.push_back(SDK::Geometry::SegmentIntersection(a, b, startRightPos, endRightPos));
        intersects.push_back(SDK::Geometry::SegmentIntersection(a, b, startLeftPos, endLeftPos));

        Vec2 heroPos = SDK::GameObjects::Player.GetPosition().To2D();
        std::vector<SDK::Geometry::IntersectionResult> validIntersects;
        for (auto& ir : intersects) {
            if (ir.Intersects) {
                validIntersects.push_back(ir);
            }
        }

        if (validIntersects.size() > 0)
        {
            std::sort(validIntersects.begin(), validIntersects.end(), [&heroPos](auto& left, auto& right) {
                return left.Point.DistanceSquared(heroPos) < right.Point.DistanceSquared(heroPos);
            });

            intersection = validIntersects[0].Point;
            return true;
        }

        intersection = Vec2(0, 0);
        return false;
    }

} // namespace EzEvade
