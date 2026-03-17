#include "EvadeHelper.h"
#include "../EvadeSpells/EvadeSpellData.h"
#include "../../GameObjects/ObjectManager.h"
#include "../../GameObjects/NavGrid.h"

namespace EzEvade {

    // =========================================================================
    // PlayerInSkillShot (C# line 21-24)
    // =========================================================================
    bool EvadeHelper::PlayerInSkillShot(Spell& spell)
    {
        return Position::InSkillShot(ObjectCache::myHeroCache.serverPos2D, spell,
            ObjectCache::myHeroCache.boundingRadius);                              // C# line 23
    }

    // =========================================================================
    // InitPositionInfo (C# line 26-50)
    // =========================================================================
    PositionInfo EvadeHelper::InitPositionInfo(const Vec2& pos, float extraDelayBuffer,
        float extraEvadeDistance, const Vec2& lastMovePos, Spell* lowestEvadeTimeSpell)
    {
        Vec2 adjustedPos = pos;
        if (!ObjectCache::myHeroCache.isMoving &&
            ObjectCache::myHeroCache.serverPos2D.Distance(pos) <= 75)              // C# line 28
            adjustedPos = ObjectCache::myHeroCache.serverPos2D;                    // C# line 29

        int extraDist = ObjectCache::GetSlider("ExtraCPADistance");                // C# line 31

        PositionInfo posInfo = CanHeroWalkToPos(adjustedPos,
            ObjectCache::myHeroCache.moveSpeed,
            extraDelayBuffer + ObjectCache::gamePing, (float)extraDist);           // C# line 34
        posInfo.isDangerousPos = Position::CheckDangerousPos(adjustedPos, 6);      // C# line 35
        posInfo.hasExtraDistance = extraEvadeDistance > 0 &&
            Position::CheckDangerousPos(adjustedPos, extraEvadeDistance);           // C# line 36
        posInfo.closestDistance = posInfo.distanceToMouse;                          // C# line 37
        posInfo.distanceToMouse = Position::GetPositionValue(adjustedPos);          // C# line 38
        posInfo.posDistToChamps = Position::GetDistanceToChampions(adjustedPos);    // C# line 39
        posInfo.speed = ObjectCache::myHeroCache.moveSpeed;                        // C# line 40

        int rejectMinDist = ObjectCache::GetSlider("RejectMinDistance");            // C# line 42
        if (rejectMinDist > 0 && (float)rejectMinDist > posInfo.closestDistance)    // C# line 43
            posInfo.rejectPosition = true;                                          // C# line 44

        int minComfort = ObjectCache::GetSlider("MinComfortZone");                  // C# line 46
        if ((float)minComfort > posInfo.posDistToChamps)                            // C# line 46
            posInfo.hasComfortZone = false;                                         // C# line 47

        return posInfo;                                                             // C# line 49
    }

    // =========================================================================
    // GetBestPositionTest (C# line 52-127) — simplified, returns sorted vector
    // =========================================================================
    std::vector<PositionInfo> EvadeHelper::GetBestPositionTest()
    {
        int posChecked = 0;                                                         // C# line 54
        int maxPosToCheck = 50;                                                     // C# line 55
        int posRadius = 50;                                                         // C# line 56
        int radiusIndex = 0;                                                        // C# line 57

        Vec2 heroPoint = ObjectCache::myHeroCache.serverPos2D;                      // C# line 59
        Vec2 lastMovePos = ObjectCache::myHeroCache.serverPos2D;                    // C# line 60 (no GetCursorWorldPosition in SDK)

        int extraDelayBuffer = ObjectCache::GetSlider("ExtraPingBuffer");           // C# line 62
        int extraEvadeDistance = ObjectCache::GetSlider("ExtraEvadeDistance");       // C# line 63

        if (ObjectCache::GetBool("HigherPrecision")) {                              // C# line 65
            maxPosToCheck = 150; posRadius = 25;                                    // C# line 67-68
        }

        std::vector<PositionInfo> posTable;                                         // C# line 71
        auto fastestPositions = GetFastestPositions();                              // C# line 72

        Spell* lowestEvadeTimeSpell = nullptr;                                      // C# line 74
        float lowestEvadeTime = SpellDetector::GetLowestEvadeTime(lowestEvadeTimeSpell); // C# line 75

        for (auto& pos : fastestPositions)                                          // C# line 77
            posTable.push_back(InitPositionInfo(pos, (float)extraDelayBuffer,
                (float)extraEvadeDistance, lastMovePos, lowestEvadeTimeSpell));      // C# line 79

        while (posChecked < maxPosToCheck)                                          // C# line 82
        {
            radiusIndex++;                                                          // C# line 84
            int curRadius = radiusIndex * (2 * posRadius);                          // C# line 86
            int curCircleChecks = (int)std::ceil(
                (2.0 * M_PI * (double)curRadius) / (2.0 * (double)posRadius));      // C# line 87

            for (int i = 1; i < curCircleChecks; i++)                               // C# line 89
            {
                posChecked++;                                                       // C# line 91
                double cRadians = (2.0 * M_PI / (curCircleChecks - 1)) * i;         // C# line 92
                Vec2 pos = {
                    (float)std::floor(heroPoint.x + curRadius * std::cos(cRadians)),
                    (float)std::floor(heroPoint.y + curRadius * std::sin(cRadians))
                };                                                                  // C# line 93
                posTable.push_back(InitPositionInfo(pos, (float)extraDelayBuffer,
                    (float)extraEvadeDistance, lastMovePos, lowestEvadeTimeSpell));  // C# line 96
            }
        }

        // Sort: C# line 120-124
        std::sort(posTable.begin(), posTable.end(), [](const PositionInfo& a, const PositionInfo& b) {
            if (a.isDangerousPos != b.isDangerousPos) return (int)a.isDangerousPos < (int)b.isDangerousPos;
            if (a.posDangerLevel != b.posDangerLevel) return a.posDangerLevel < b.posDangerLevel;
            if (a.posDangerCount != b.posDangerCount) return a.posDangerCount < b.posDangerCount;
            return a.distanceToMouse < b.distanceToMouse;
        });

        return posTable;                                                            // C# line 126
    }

    // =========================================================================
    // GetBestPosition (C# line 129-266)
    // =========================================================================
    PositionInfo* EvadeHelper::GetBestPosition()
    {
        static PositionInfo resultInfo; // static to return pointer safely

        int posChecked = 0;                                                         // C# line 131
        int maxPosToCheck = 50;                                                     // C# line 132
        int posRadius = 50;                                                         // C# line 133
        int radiusIndex = 0;                                                        // C# line 134

        int extraDelayBuffer = ObjectCache::GetSlider("ExtraPingBuffer");           // C# line 136
        int extraEvadeDistance = ObjectCache::GetSlider("ExtraEvadeDistance");       // C# line 137

        SpellDetector::UpdateSpells();                                              // C# line 139
        CalculateEvadeTime();                                                       // C# line 140

        if (ObjectCache::GetBool("CalculateWindupDelay")) {                         // C# line 142
            float extraWindupDelay = 0; // Evade::lastWindupTime - EvadeUtils::TickCount() // C# line 144
            if (extraWindupDelay > 0) extraDelayBuffer += (int)extraWindupDelay;    // C# line 147
        }

        // extraDelayBuffer += (int)(Evade::avgCalculationTime);                    // C# line 151

        if (ObjectCache::GetBool("HigherPrecision")) {                              // C# line 153
            maxPosToCheck = 150; posRadius = 25;                                    // C# line 155-156
        }

        Vec2 heroPoint = ObjectCache::myHeroCache.serverPos2D;                      // C# line 159
        Vec2 lastMovePos = ObjectCache::myHeroCache.serverPos2D;                    // C# line 160 (no GetCursorWorldPosition in SDK)

        std::vector<PositionInfo> posTable;                                         // C# line 162

        Spell* lowestEvadeTimeSpell = nullptr;                                      // C# line 164
        float lowestEvadeTime = SpellDetector::GetLowestEvadeTime(lowestEvadeTimeSpell); // C# line 165

        auto fastestPositions = GetFastestPositions();                              // C# line 167
        for (auto& pos : fastestPositions)                                          // C# line 169
            posTable.push_back(InitPositionInfo(pos, (float)extraDelayBuffer,
                (float)extraEvadeDistance, lastMovePos, lowestEvadeTimeSpell));

        while (posChecked < maxPosToCheck)                                          // C# line 174
        {
            radiusIndex++;
            int curRadius = radiusIndex * (2 * posRadius);                          // C# line 178
            int curCircleChecks = (int)std::ceil(
                (2.0 * M_PI * (double)curRadius) / (2.0 * (double)posRadius));      // C# line 179
            for (int i = 1; i < curCircleChecks; i++) {                             // C# line 181
                posChecked++;
                double cRadians = (2.0 * M_PI / (curCircleChecks - 1)) * i;
                Vec2 pos = {
                    (float)std::floor(heroPoint.x + curRadius * std::cos(cRadians)),
                    (float)std::floor(heroPoint.y + curRadius * std::sin(cRadians))
                };
                posTable.push_back(InitPositionInfo(pos, (float)extraDelayBuffer,
                    (float)extraEvadeDistance, lastMovePos, lowestEvadeTimeSpell));  // C# line 186
            }
        }

        // Sorting logic (C# line 190-243)
        // Simplified: use smooth mode by default
        int fastEvadeActivTime = ObjectCache::GetSlider("FastEvadeActivationTime");

        bool useFastSort = false;
        if (fastEvadeActivTime > 0 &&
            (float)fastEvadeActivTime + ObjectCache::gamePing + (float)extraDelayBuffer > lowestEvadeTime)
        {
            useFastSort = true;
            fastEvadeMode = true;                                                   // C# line 219
        }

        if (useFastSort || fastEvadeMode)
        {
            // Sort by intersectionTime descending (fastest escape)
            std::sort(posTable.begin(), posTable.end(), [](const PositionInfo& a, const PositionInfo& b) {
                if (a.isDangerousPos != b.isDangerousPos) return (int)a.isDangerousPos < (int)b.isDangerousPos;
                if (a.intersectionTime != b.intersectionTime) return a.intersectionTime > b.intersectionTime;
                if (a.posDangerLevel != b.posDangerLevel) return a.posDangerLevel < b.posDangerLevel;
                return a.posDangerCount < b.posDangerCount;
            });
        }
        else
        {
            // Smooth mode sort
            std::sort(posTable.begin(), posTable.end(), [](const PositionInfo& a, const PositionInfo& b) {
                if (a.rejectPosition != b.rejectPosition) return (int)a.rejectPosition < (int)b.rejectPosition;
                if (a.posDangerLevel != b.posDangerLevel) return a.posDangerLevel < b.posDangerLevel;
                if (a.posDangerCount != b.posDangerCount) return a.posDangerCount < b.posDangerCount;
                return a.distanceToMouse < b.distanceToMouse;
            });

            // If can't dodge smoothly, try fastest (C# line 229-242)
            if (!posTable.empty() && posTable.front().posDangerCount != 0) {
                auto fastTable = posTable;
                std::sort(fastTable.begin(), fastTable.end(), [](const PositionInfo& a, const PositionInfo& b) {
                    if (a.isDangerousPos != b.isDangerousPos) return (int)a.isDangerousPos < (int)b.isDangerousPos;
                    if (a.intersectionTime != b.intersectionTime) return a.intersectionTime > b.intersectionTime;
                    if (a.posDangerLevel != b.posDangerLevel) return a.posDangerLevel < b.posDangerLevel;
                    return a.posDangerCount < b.posDangerCount;
                });
                if (!fastTable.empty() && fastTable.front().posDangerCount == 0) {
                    posTable = fastTable;
                    fastEvadeMode = true;
                }
            }
        }

        // Find first valid position (C# line 245-263)
        for (auto& posInfo : posTable)
        {
            if (!CheckPathCollision(posInfo.position))        // C# line 247
            {
                if (fastEvadeMode) {                                                // C# line 249
                    posInfo.position = GetExtendedSafePosition(
                        ObjectCache::myHeroCache.serverPos2D, posInfo.position,
                        (float)extraEvadeDistance);                                  // C# line 251
                    resultInfo = CanHeroWalkToPos(posInfo.position,
                        ObjectCache::myHeroCache.moveSpeed, ObjectCache::gamePing, 0); // C# line 252
                    return &resultInfo;
                }

                if (PositionInfoStillValid(posInfo)) {                              // C# line 255
                    if (Position::CheckDangerousPos(posInfo.position,
                        (float)extraEvadeDistance))                                  // C# line 257
                        posInfo.position = GetExtendedSafePosition(
                            ObjectCache::myHeroCache.serverPos2D,
                            posInfo.position, (float)extraEvadeDistance);            // C# line 258

                    resultInfo = posInfo;
                    return &resultInfo;                                              // C# line 260
                }
            }
        }

        resultInfo = PositionInfo::SetAllUndodgeable();                             // C# line 265
        return &resultInfo;
    }

    // =========================================================================
    // GetBestPositionMovementBlock (C# line 268-322)
    // =========================================================================
    PositionInfo* EvadeHelper::GetBestPositionMovementBlock(const Vec2& movePos)
    {
        static PositionInfo resultInfo;

        int posChecked = 0, maxPosToCheck = 50, posRadius = 50, radiusIndex = 0;   // C# line 270-273
        int extraEvadeDistance = ObjectCache::GetSlider("ExtraAvoidDistance");       // C# line 275

        Vec2 heroPoint = ObjectCache::myHeroCache.serverPos2D;                      // C# line 277
        std::vector<PositionInfo> posTable;                                         // C# line 280

        int extraDist = ObjectCache::GetSlider("ExtraCPADistance");                 // C# line 282
        int extraDelayBuffer = ObjectCache::GetSlider("ExtraPingBuffer");           // C# line 283

        while (posChecked < maxPosToCheck)                                          // C# line 285
        {
            radiusIndex++;
            int curRadius = radiusIndex * (2 * posRadius);
            int curCircleChecks = (int)std::ceil(
                (2.0 * M_PI * (double)curRadius) / (2.0 * (double)posRadius));
            for (int i = 1; i < curCircleChecks; i++) {
                posChecked++;
                double cRadians = (2.0 * M_PI / (curCircleChecks - 1)) * i;
                Vec2 pos = {
                    (float)std::floor(heroPoint.x + curRadius * std::cos(cRadians)),
                    (float)std::floor(heroPoint.y + curRadius * std::sin(cRadians))
                };
                auto posInfo = CanHeroWalkToPos(pos, ObjectCache::myHeroCache.moveSpeed,
                    (float)extraDelayBuffer + ObjectCache::gamePing, (float)extraDist); // C# line 298
                posInfo.isDangerousPos = Position::CheckDangerousPos(pos, 6)
                    || CheckMovePath(pos);                                          // C# line 299
                posInfo.distanceToMouse = Position::GetPositionValue(pos);          // C# line 300
                posInfo.hasExtraDistance = extraEvadeDistance > 0 &&
                    Position::HasExtraAvoidDistance(pos, (float)extraEvadeDistance); // C# line 301
                posTable.push_back(posInfo);                                        // C# line 303
            }
        }

        // Sort (C# line 307-311)
        std::sort(posTable.begin(), posTable.end(), [](const PositionInfo& a, const PositionInfo& b) {
            if (a.isDangerousPos != b.isDangerousPos) return (int)a.isDangerousPos < (int)b.isDangerousPos;
            if (a.posDangerLevel != b.posDangerLevel) return a.posDangerLevel < b.posDangerLevel;
            if (a.hasExtraDistance != b.hasExtraDistance) return (int)a.hasExtraDistance < (int)b.hasExtraDistance;
            return a.distanceToMouse < b.distanceToMouse;
        });

        for (auto& posInfo : posTable) {                                            // C# line 313
            if (!CheckPathCollision(posInfo.position)) {     // C# line 315
                resultInfo = posInfo;
                return &resultInfo;                                                 // C# line 317
            }
        }

        return nullptr;                                                             // C# line 321
    }

    // =========================================================================
    // GetBestPositionBlink (C# line 324-380)
    // =========================================================================
    PositionInfo* EvadeHelper::GetBestPositionBlink()
    {
        static PositionInfo resultInfo;
        int posChecked = 0, maxPosToCheck = 100, posRadius = 50, radiusIndex = 0;
        int extraEvadeDistance = std::max(100, ObjectCache::GetSlider("ExtraEvadeDistance"));
        Vec2 heroPoint = ObjectCache::myHeroCache.serverPos2DPing;
        int minComfortZone = ObjectCache::GetSlider("MinComfortZone");
        std::vector<PositionInfo> posTable;

        while (posChecked < maxPosToCheck) {
            radiusIndex++;
            int curRadius = radiusIndex * (2 * posRadius);
            int curCircleChecks = (int)std::ceil((2.0 * M_PI * curRadius) / (2.0 * posRadius));
            for (int i = 1; i < curCircleChecks; i++) {
                posChecked++;
                double cRadians = (2.0 * M_PI / (curCircleChecks - 1)) * i;
                Vec2 pos = {
                    (float)std::floor(heroPoint.x + curRadius * std::cos(cRadians)),
                    (float)std::floor(heroPoint.y + curRadius * std::sin(cRadians))
                };
                bool isDangerousPos = Position::CheckDangerousPos(pos, 6);
                float dist = Position::GetPositionValue(pos);
                PositionInfo posInfo(pos, isDangerousPos, dist);
                posInfo.hasExtraDistance = extraEvadeDistance > 0 ?
                    Position::CheckDangerousPos(pos, (float)extraEvadeDistance) : false;
                posInfo.posDistToChamps = Position::GetDistanceToChampions(pos);
                if ((float)minComfortZone < posInfo.posDistToChamps)
                    posTable.push_back(posInfo);
            }
        }

        std::sort(posTable.begin(), posTable.end(), [](const PositionInfo& a, const PositionInfo& b) {
            if (a.isDangerousPos != b.isDangerousPos) return (int)a.isDangerousPos < (int)b.isDangerousPos;
            if (a.hasExtraDistance != b.hasExtraDistance) return (int)a.hasExtraDistance < (int)b.hasExtraDistance;
            return a.distanceToMouse < b.distanceToMouse;
        });

        for (auto& posInfo : posTable) {
            if (!CheckPointCollision(posInfo.position)) {
                resultInfo = posInfo;
                return &resultInfo;
            }
        }
        return nullptr;
    }

    // =========================================================================
    // CheckWindupTime (C# line 627-641)
    // =========================================================================
    bool EvadeHelper::CheckWindupTime(float windupTime)
    {
        for (auto& entry : SpellDetector::spells) {
            Spell& spell = entry.second;
            float hitTime = spell.GetSpellHitTime(ObjectCache::myHeroCache.serverPos2D);
            if (hitTime < windupTime) return true;
        }
        return false;
    }

    // =========================================================================
    // PositionInfoStillValid (C# line 659-662)
    // =========================================================================
    bool EvadeHelper::PositionInfoStillValid(const PositionInfo& /*posInfo*/, float /*moveSpeed*/)
    {
        return true; // C# line 661: "too buggy"
    }

    // =========================================================================
    // GetExtendedSafePosition (C# line 680-700)
    // =========================================================================
    Vec2 EvadeHelper::GetExtendedSafePosition(const Vec2& from, const Vec2& to, float extendDistance)
    {
        Vec2 diff = to - from;
        float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        Vec2 direction = len > 0 ? Vec2{diff.x / len, diff.y / len} : Vec2{0, 0};
        Vec2 lastPosition = to;
        float sectorDistance = 50.0f;

        for (float i = sectorDistance; i <= extendDistance; i += sectorDistance) {
            Vec2 pos = to + direction * i;
            if (Position::CheckDangerousPos(pos, 6) ||
                CheckPathCollision(pos))
                return lastPosition;
            lastPosition = pos;
        }
        return lastPosition;
    }

    // =========================================================================
    // GetExtendedPositions (C# line 664-678)
    // =========================================================================
    std::vector<Vec2> EvadeHelper::GetExtendedPositions(const Vec2& from, const Vec2& to, float extendDistance)
    {
        Vec2 diff = to - from;
        float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        Vec2 direction = len > 0 ? Vec2{diff.x / len, diff.y / len} : Vec2{0, 0};
        std::vector<Vec2> positions;
        float sectorDistance = 50.0f;
        for (float i = sectorDistance; i < extendDistance; i += sectorDistance)
            positions.push_back(to + direction * i);
        return positions;
    }

    // =========================================================================
    // CalculateEvadeTime (C# line 702-713)
    // =========================================================================
    void EvadeHelper::CalculateEvadeTime()
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        for (auto& entry : SpellDetector::spells) {
            Spell& spell = entry.second;
            float evadeTime, spellHitTime;
            spell.CanHeroEvade(myHero, evadeTime, spellHitTime);
            spell.spellHitTime = spellHitTime;
            spell.evadeTime = evadeTime;
        }
    }

    // =========================================================================
    // GetFastestPosition (C# line 715-730)
    // =========================================================================
    Vec2 EvadeHelper::GetFastestPosition(Spell& spell)
    {
        Vec2 heroPos = ObjectCache::myHeroCache.serverPos2D;
        if (spell.spellType == SpellType::Line) {
            Vec2 projection = SDK::Geometry::ProjectOn(heroPos, spell.startPos, spell.endPos).segmentPoint;
            Vec2 diff = heroPos - projection;
            float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            Vec2 dir = dist > 0 ? Vec2{diff.x / dist, diff.y / dist} : Vec2{0, 0};
            return projection + dir * (spell.radius + ObjectCache::myHeroCache.boundingRadius + 10);
        }
        else if (spell.spellType == SpellType::Circular) {
            Vec2 diff = heroPos - spell.endPos;
            float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            Vec2 dir = dist > 0 ? Vec2{diff.x / dist, diff.y / dist} : Vec2{0, 0};
            return spell.endPos + dir * (spell.radius + 10);
        }
        return {0, 0};
    }

    // =========================================================================
    // GetFastestPositions (C# line 732-750)
    // =========================================================================
    std::vector<Vec2> EvadeHelper::GetFastestPositions()
    {
        std::vector<Vec2> positions;
        for (auto& entry : SpellDetector::spells) {
            auto pos = GetFastestPosition(entry.second);
            if (pos.x != 0 || pos.y != 0) positions.push_back(pos);
        }
        return positions;
    }

    // =========================================================================
    // GetMinCPADistance (C# line 761-772)
    // =========================================================================
    float EvadeHelper::GetMinCPADistance(const Vec2& movePos)
    {
        float minDist = FLT_MAX;
        for (auto& entry : SpellDetector::spells) {
            minDist = std::min(minDist, GetClosestDistanceApproach(entry.second, movePos,
                ObjectCache::myHeroCache.moveSpeed, ObjectCache::gamePing,
                ObjectCache::myHeroCache.serverPos2DPing, 0));
        }
        return minDist;
    }

    // =========================================================================
    // GetClosestDistanceApproach (C# line 908-1071) — simplified
    // =========================================================================
    float EvadeHelper::GetClosestDistanceApproach(Spell& spell, const Vec2& pos, float speed,
        float delay, const Vec2& heroPos, float extraDist)
    {
        Vec2 diff = pos - heroPos;
        float dLen = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        Vec2 walkDir = dLen > 0 ? Vec2{diff.x / dLen, diff.y / dLen} : Vec2{0, 0};

        if (spell.spellType == SpellType::Line && spell.info.projectileSpeed != FLT_MAX)
        {
            // Moving projectile line spell (C# line 912-951)
            Vec2 spellPos = spell.GetCurrentSpellPosition(true, delay);
            Vec2 spellEndPos = spell.GetSpellEndPosition();

            Vec2 cHeroPos, cSpellPos;
            float cpa = MathUtilsCPA::CPAPointsEx(
                heroPos, walkDir * speed, spellPos, spell.direction * spell.info.projectileSpeed,
                pos, spellEndPos, cHeroPos, cSpellPos);

            float checkDist = ObjectCache::myHeroCache.boundingRadius + spell.radius + extraDist;
            // Simplified projection check
            return std::max(0.0f, cpa - checkDist);
        }
        else if (spell.spellType == SpellType::Line && spell.info.projectileSpeed >= FLT_MAX)
        {
            // Instant line spell (C# line 952-963)
            float spellHitTime = std::max(0.0f, spell.endTime - EvadeUtils::TickCount() - delay);
            float walkRange = heroPos.Distance(pos);
            float predictedRange = speed * (spellHitTime / 1000.0f);
            Vec2 tHeroPos = heroPos + walkDir * std::min(predictedRange, walkRange);

            Vec2 projection = SDK::Geometry::ProjectOn(tHeroPos, spell.startPos, spell.endPos).segmentPoint;
            return std::max(0.0f, tHeroPos.Distance(projection) -
                (spell.radius + ObjectCache::myHeroCache.boundingRadius + extraDist));
        }
        else if (spell.spellType == SpellType::Circular)
        {
            // Circular spell (C# line 964-1017)
            float spellHitTime = std::max(0.0f, spell.endTime - EvadeUtils::TickCount() - delay);
            float walkRange = heroPos.Distance(pos);
            float predictedRange = speed * (spellHitTime / 1000.0f);
            Vec2 tHeroPos = heroPos + walkDir * std::min(predictedRange, walkRange);

            // VeigarEventHorizon special case (C# line 971-984)
            if (spell.info.spellName == "VeigarEventHorizon") {
                float wallRadius = 65.0f;
                float midRadius = spell.radius - wallRadius;
                if (spellHitTime == 0) return 0;
                return tHeroPos.Distance(spell.endPos) >= spell.radius
                    ? std::max(0.0f, tHeroPos.Distance(spell.endPos) - midRadius - wallRadius)
                    : std::max(0.0f, midRadius - tHeroPos.Distance(spell.endPos) - wallRadius);
            }

            // DariusCleave special case (C# line 986-999)
            if (spell.info.spellName == "DariusCleave") {
                float wallRadius = 115.0f;
                float midRadius = spell.radius - wallRadius;
                if (spellHitTime == 0) return 0;
                return tHeroPos.Distance(spell.endPos) >= spell.radius
                    ? std::max(0.0f, tHeroPos.Distance(spell.endPos) - midRadius - wallRadius)
                    : std::max(0.0f, midRadius - tHeroPos.Distance(spell.endPos) - wallRadius);
            }

            return std::max(0.0f, tHeroPos.Distance(spell.endPos) - (spell.radius + extraDist));
        }
        return 1.0f;                                                                // C# line 1070
    }

    // =========================================================================
    // PredictSpellCollision (C# line 1073-1088)
    // =========================================================================
    bool EvadeHelper::PredictSpellCollision(Spell& spell, const Vec2& pos, float speed,
        float delay, const Vec2& heroPos, float extraDist, bool useServerPosition)
    {
        float ed = extraDist + 10;
        if (!useServerPosition) {
            return GetClosestDistanceApproach(spell, pos, speed, 0,
                ObjectCache::myHeroCache.serverPos2D, 0) == 0;
        }
        return GetClosestDistanceApproach(spell, pos, speed, delay,
                   ObjectCache::myHeroCache.serverPos2DPing, ed) == 0
            || GetClosestDistanceApproach(spell, pos, speed, ObjectCache::gamePing,
                   ObjectCache::myHeroCache.serverPos2DPing, ed) == 0;
    }

    // =========================================================================
    // CanHeroWalkToPos (C# line 861-906)
    // =========================================================================
    PositionInfo EvadeHelper::CanHeroWalkToPos(const Vec2& pos, float speed, float delay,
        float extraDist, bool useServerPosition)
    {
        int posDangerLevel = 0, posDangerCount = 0;
        float closestDistance = FLT_MAX;
        std::vector<int> dodgeableSpells, undodgeableSpells;

        Vec2 heroPos = ObjectCache::myHeroCache.serverPos2D;
        int minComfortDistance = ObjectCache::GetSlider("MinComfortZone");

        if (!useServerPosition) {
            auto hero = SDK::ObjectManager::GetLocalPlayer();
            if (hero.IsValid()) heroPos = hero.GetPosition().To2D();
        }

        for (auto& entry : SpellDetector::spells) {
            Spell& spell = entry.second;
            closestDistance = std::min(closestDistance,
                GetClosestDistanceApproach(spell, pos, speed, delay, heroPos, extraDist));

            if (Position::InSkillShot(pos, spell, ObjectCache::myHeroCache.boundingRadius - 8)
                || PredictSpellCollision(spell, pos, speed, delay, heroPos, extraDist, useServerPosition)
                || (spell.info.spellType != SpellType::Line &&
                    Position::IsNearEnemy(pos, (float)minComfortDistance)))
            {
                posDangerLevel = std::max(posDangerLevel, spell.dangerlevel);
                posDangerCount += spell.dangerlevel;
                undodgeableSpells.push_back(spell.spellID);
            } else {
                dodgeableSpells.push_back(spell.spellID);
            }
        }

        return PositionInfo(pos, posDangerLevel, posDangerCount,
            posDangerCount > 0, closestDistance, dodgeableSpells, undodgeableSpells);
    }

    // =========================================================================
    // CheckPathCollision (C# line 1107-1120)
    // Uses NavGrid.IsWallBetween to check if path crosses terrain
    // C# original: unit.GetPath(serverPos, movePos) → checks path.Length > 2
    // =========================================================================
    bool EvadeHelper::CheckPathCollision(const Vec2& movePos)
    {
        // unit param removed — use ObjectCache data

        Vec2 serverPos = ObjectCache::myHeroCache.serverPos2D;                      // C# line 1109
        Vec3 from3D = {serverPos.x, 0, serverPos.y};                               // convert to Vec3
        Vec3 to3D = {movePos.x, 0, movePos.y};                                     // convert to Vec3

        // Use NavGrid to check wall between start and end
        SDK::NavGrid navGrid = SDK::NavGrid::Get();                                 // C# equivalent: unit.GetPath()
        if (!navGrid.IsValid()) return false;

        // C# line 1113: path.Length > 2 || movePos.Distance(path[last]) > 5
        // Equivalent: if wall between start and end, path would be rerouted
        return navGrid.IsWallBetween(from3D, to3D, 25.0f);                         // step=25 for precision
    }

    // =========================================================================
    // CheckPointCollision (C# line 1122-1135)
    // Checks if a point is inside terrain (used for blink destinations)
    // C# original: unit.GetPath(movePos) → if endpoint deviates > 5 → wall
    // =========================================================================
    bool EvadeHelper::CheckPointCollision(const Vec2& movePos)
    {
        // unit param removed — use ObjectCache data

        Vec3 pos3D = {movePos.x, 0, movePos.y};                                    // convert to Vec3

        SDK::NavGrid navGrid = SDK::NavGrid::Get();
        if (!navGrid.IsValid()) return false;

        // C# line 1128: if movePos.Distance(path[last]) > 5 → point is in wall
        return navGrid.IsWall(pos3D);                                               // direct wall check
    }

    // =========================================================================
    // CheckMovePath (C# line 1137-1187)
    // =========================================================================
    bool EvadeHelper::CheckMovePath(const Vec2& movePos, float extraDelay)
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return false;

        // Simplified: check if move direction intersects any spell
        Vec2 heroPos = myHero.GetPosition().To2D();
        return CheckMoveToDirection(heroPos, movePos, extraDelay);
    }

    // =========================================================================
    // CheckMoveToDirection (C# line 1217-1318)
    // =========================================================================
    bool EvadeHelper::CheckMoveToDirection(const Vec2& from, const Vec2& movePos, float /*extraDelay*/)
    {
        Vec2 diff = movePos - from;
        float dLen = std::sqrt(diff.x * diff.x + diff.y * diff.y);

        for (auto& entry : SpellDetector::spells)
        {
            Spell& spell = entry.second;
            if (!Position::InSkillShot(from, spell, ObjectCache::myHeroCache.boundingRadius))
            {
                if (spell.spellType == SpellType::Line) {
                    if (spell.LineIntersectLinearSpell(from, movePos))
                        return true;
                }
                else if (spell.spellType == SpellType::Circular) {
                    Vec2 dir = dLen > 0 ? Vec2{diff.x / dLen, diff.y / dLen} : Vec2{0, 0};
                    Vec2 cHeroPos, cSpellPos;
                    float cpa2 = EzMathUtils::GetCollisionDistanceEx(
                        from, dir * ObjectCache::myHeroCache.moveSpeed, 1,
                        spell.endPos, {0, 0}, spell.radius,
                        cHeroPos, cSpellPos);

                    if (spell.info.spellName.find("_trap") != std::string::npos &&
                        !(cpa2 < spell.radius + 10))
                        continue;

                    auto cHeroPosProjection = SDK::Geometry::ProjectOn(cHeroPos, from, movePos);
                    // Simplified check
                    if (cpa2 < spell.radius + ObjectCache::myHeroCache.boundingRadius && cpa2 != FLT_MAX)
                        return true;
                }
            }
        }
        return false;
    }

    // =========================================================================
    // Stubs for remaining methods
    // =========================================================================
    float EvadeHelper::CompareFastestPosition(Spell& spell, const Vec2& start, const Vec2& movePos) { return 0; }
    float EvadeHelper::GetMovementBlockPositionValue(const Vec2& /*pos*/, const Vec2& /*movePos*/) { return 0; }
    float EvadeHelper::GetCombinedIntersectionDistance(const Vec2& /*movePos*/) { return 0; }
    float EvadeHelper::GetIntersectDistance(Spell& /*spell*/, const Vec2& /*start*/, const Vec2& /*end*/) { return FLT_MAX; }
    Vec2 EvadeHelper::GetRealHeroPos(float /*delay*/) { return ObjectCache::myHeroCache.serverPos2D; }
    bool EvadeHelper::LineIntersectLinearSegment(const Vec2& /*a1*/, const Vec2& /*b1*/, const Vec2& /*a2*/, const Vec2& /*b2*/) { return false; }

    // =========================================================================
    // GetBestPositionDash (C# line 382-454)
    // Finds best position for non-targeted dash (e.g. Lucian E, Vayne Q)
    // =========================================================================
    PositionInfo* EvadeHelper::GetBestPositionDash(const EvadeSpellData& spell)
    {
        static PositionInfo resultInfo;

        int posChecked = 0;                                                         // C# line 384
        int maxPosToCheck = 100;                                                    // C# line 385
        int posRadius = 50;                                                         // C# line 386
        int radiusIndex = 0;                                                        // C# line 387

        int extraDelayBuffer = ObjectCache::GetSlider("ExtraPingBuffer");           // C# line 389
        int extraEvadeDistance = std::max(100, ObjectCache::GetSlider("ExtraEvadeDistance")); // C# line 390
        int extraDist = ObjectCache::GetSlider("ExtraCPADistance");                 // C# line 391

        Vec2 heroPoint = ObjectCache::myHeroCache.serverPos2DPing;                  // C# line 393
        std::vector<PositionInfo> posTable;                                         // C# line 396
        auto spellList = SpellDetector::GetSpellList();                             // C# line 397

        int minDistance = 50;                                                       // C# line 399
        int maxDistance = INT_MAX;                                                  // C# line 400

        if (spell.fixedRange) {                                                     // C# line 402
            minDistance = maxDistance = (int)spell.range;                            // C# line 404
        }

        while (posChecked < maxPosToCheck)                                          // C# line 407
        {
            radiusIndex++;                                                          // C# line 409
            int curRadius = radiusIndex * (2 * posRadius) + (minDistance - 2 * posRadius); // C# line 411
            int curCircleChecks = (int)std::ceil(
                (2.0 * M_PI * (double)curRadius) / (2.0 * (double)posRadius));      // C# line 412

            for (int i = 1; i < curCircleChecks; i++)                               // C# line 414
            {
                posChecked++;                                                       // C# line 416
                double cRadians = (2.0 * M_PI / (curCircleChecks - 1)) * i;         // C# line 417
                Vec2 pos = {
                    (float)std::floor(heroPoint.x + curRadius * std::cos(cRadians)),
                    (float)std::floor(heroPoint.y + curRadius * std::sin(cRadians))
                };                                                                  // C# line 418

                auto posInfo = CanHeroWalkToPos(pos, spell.speed,
                    (float)extraDelayBuffer + ObjectCache::gamePing, (float)extraDist); // C# line 420
                posInfo.isDangerousPos = Position::CheckDangerousPos(pos, 6);       // C# line 421
                posInfo.hasExtraDistance = extraEvadeDistance > 0
                    ? Position::CheckDangerousPos(pos, (float)extraEvadeDistance) : false; // C# line 422
                posInfo.distanceToMouse = Position::GetPositionValue(pos);          // C# line 423
                posInfo.posDistToChamps = Position::GetDistanceToChampions(pos);    // C# line 426

                posTable.push_back(posInfo);                                        // C# line 428
            }

            if (curRadius >= maxDistance)                                            // C# line 431
                break;
        }

        // Sort (C# line 435-440)
        std::sort(posTable.begin(), posTable.end(), [](const PositionInfo& a, const PositionInfo& b) {
            if (a.isDangerousPos != b.isDangerousPos) return (int)a.isDangerousPos < (int)b.isDangerousPos;
            if (a.posDangerLevel != b.posDangerLevel) return a.posDangerLevel < b.posDangerLevel;
            if (a.posDangerCount != b.posDangerCount) return a.posDangerCount < b.posDangerCount;
            if (a.hasExtraDistance != b.hasExtraDistance) return (int)a.hasExtraDistance < (int)b.hasExtraDistance;
            return a.distanceToMouse < b.distanceToMouse;
        });

        for (auto& posInfo : posTable) {                                            // C# line 442
            if (!CheckPathCollision(posInfo.position)) {     // C# line 444
                if (PositionInfoStillValid(posInfo, spell.speed)) {                 // C# line 446
                    resultInfo = posInfo;
                    return &resultInfo;                                              // C# line 448
                }
            }
        }

        return nullptr;                                                             // C# line 453
    }

    // =========================================================================
    // GetBestPositionTargetedDash (C# line 456-625)
    // Finds best target + position for targeted dash (e.g. Yasuo E, Katarina E)
    // =========================================================================
    PositionInfo* EvadeHelper::GetBestPositionTargetedDash(const EvadeSpellData& spell)
    {
        static PositionInfo resultInfo;

        int extraDelayBuffer = ObjectCache::GetSlider("ExtraPingBuffer");           // C# line 458
        int extraEvadeDistance = std::max(100, ObjectCache::GetSlider("ExtraEvadeDistance")); // C# line 459
        int extraDist = ObjectCache::GetSlider("ExtraCPADistance");                 // C# line 460

        Vec2 heroPoint = ObjectCache::myHeroCache.serverPos2DPing;                  // C# line 462
        std::vector<PositionInfo> posTable;                                         // C# line 465
        auto spellList = SpellDetector::GetSpellList();                             // C# line 466

        // Helper: check if spell targets contain a specific type
        auto hasTarget = [&](SpellTargets t) {
            for (auto& st : spell.spellTargets)
                if (st == t) return true;
            return false;
        };

        // ---- Collect collision candidates (C# line 476-533) ----
        struct Candidate {
            SDK::GameObject obj;
            Vec2 pos;
        };
        std::vector<Candidate> collisionCandidates;

        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return nullptr;
        int myTeam = (int)myHero.GetTeam();

        if (hasTarget(SpellTargets::Targetables))                                   // C# line 478
        {
            // All valid targets except turrets (C# line 480-487)
            SDK::ObjectManager::ForEach([&](SDK::GameObject& obj) {
                if (!obj.IsValid() || obj.IsMe()) return;
                if (!obj.IsAlive() || !obj.IsTargetable()) return;
                if (obj.IsTurret()) return;
                float dist = heroPoint.Distance(obj.GetPosition().To2D());
                if (dist <= spell.range)
                    collisionCandidates.push_back({obj, obj.GetServerPosition().To2D()});
            });
        }
        else
        {
            // Heroes (C# line 491-511)
            auto heroes = SDK::ObjectManager::GetHeroes();
            for (auto& hero : heroes) {
                if (!hero.IsValid() || hero.IsMe() || !hero.IsAlive()) continue;
                float dist = heroPoint.Distance(hero.GetPosition().To2D());
                if (dist > spell.range) continue;

                bool isAlly = (int)hero.GetTeam() == myTeam;
                bool isEnemy = !isAlly;

                bool shouldAdd = false;
                if (hasTarget(SpellTargets::EnemyChampions) && hasTarget(SpellTargets::AllyChampions))
                    shouldAdd = true;                                               // C# line 496
                else if (hasTarget(SpellTargets::EnemyChampions) && isEnemy)
                    shouldAdd = true;                                               // C# line 500
                else if (hasTarget(SpellTargets::AllyChampions) && isAlly)
                    shouldAdd = true;                                               // C# line 504

                if (shouldAdd && hero.IsVisible())
                    collisionCandidates.push_back({hero, hero.GetServerPosition().To2D()});
            }

            // Minions (C# line 513-532)
            auto minions = SDK::ObjectManager::GetMinions();
            for (auto& minion : minions) {
                if (!minion.IsValid() || !minion.IsAlive()) continue;
                float dist = heroPoint.Distance(minion.GetPosition().To2D());
                if (dist > spell.range) continue;

                bool isAlly = (int)minion.GetTeam() == myTeam;
                bool isEnemy = !isAlly;

                bool shouldAdd = false;
                if (hasTarget(SpellTargets::EnemyMinions) && hasTarget(SpellTargets::AllyMinions))
                    shouldAdd = true;                                               // C# line 518
                else if (hasTarget(SpellTargets::EnemyMinions) && isEnemy)
                    shouldAdd = true;                                               // C# line 522
                else if (hasTarget(SpellTargets::AllyMinions) && isAlly)
                    shouldAdd = true;                                               // C# line 526

                if (shouldAdd)
                    collisionCandidates.push_back({minion, minion.GetServerPosition().To2D()});
            }
        }

        // ---- Process each candidate (C# line 535-593) ----
        for (auto& candidate : collisionCandidates)
        {
            Vec2 pos = candidate.pos;                                               // C# line 537

            // YasuoDashWrapper special case (C# line 541-556)
            if (spell.spellName == "YasuoDashWrapper") {
                if (candidate.obj.HasBuff("YasuoDashWrapper"))                      // C# line 547-549
                    continue;                                                       // C# line 554
            }

            // behindTarget (C# line 558-562)
            if (spell.behindTarget) {
                Vec2 diff = pos - heroPoint;
                float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
                Vec2 dir = len > 0 ? Vec2{diff.x / len, diff.y / len} : Vec2{0, 0};
                pos = pos + dir * (candidate.obj.GetBoundingRadius() + ObjectCache::myHeroCache.boundingRadius);
            }

            // infrontTarget (C# line 564-568)
            if (spell.infrontTarget) {
                Vec2 diff = pos - heroPoint;
                float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
                Vec2 dir = len > 0 ? Vec2{diff.x / len, diff.y / len} : Vec2{0, 0};
                pos = pos - dir * (candidate.obj.GetBoundingRadius() + ObjectCache::myHeroCache.boundingRadius);
            }

            // fixedRange (C# line 570-574)
            if (spell.fixedRange) {
                Vec2 diff = pos - heroPoint;
                float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
                Vec2 dir = len > 0 ? Vec2{diff.x / len, diff.y / len} : Vec2{0, 0};
                pos = heroPoint + dir * spell.range;
            }

            PositionInfo posInfo;

            if (spell.evadeType == EvadeType::Dash) {                               // C# line 576
                posInfo = CanHeroWalkToPos(pos, spell.speed,
                    (float)extraDelayBuffer + ObjectCache::gamePing, (float)extraDist); // C# line 578
                posInfo.isDangerousPos = Position::CheckDangerousPos(pos, 6);       // C# line 579
                posInfo.distanceToMouse = Position::GetPositionValue(pos);          // C# line 580
            }
            else {
                bool isDangerousPos = Position::CheckDangerousPos(pos, 6);          // C# line 585
                float dist = Position::GetPositionValue(pos);                       // C# line 586
                posInfo = PositionInfo(pos, isDangerousPos, dist);                  // C# line 588
            }

            // posInfo.target = candidate.obj; // C# line 591 — target stored as SDK object
            posTable.push_back(posInfo);                                            // C# line 592
        }

        // ---- Sort and return (C# line 595-624) ----
        if (spell.evadeType == EvadeType::Dash)                                     // C# line 595
        {
            std::sort(posTable.begin(), posTable.end(), [](const PositionInfo& a, const PositionInfo& b) {
                if (a.isDangerousPos != b.isDangerousPos) return (int)a.isDangerousPos < (int)b.isDangerousPos;
                if (a.posDangerLevel != b.posDangerLevel) return a.posDangerLevel < b.posDangerLevel;
                if (a.posDangerCount != b.posDangerCount) return a.posDangerCount < b.posDangerCount;
                return a.distanceToMouse < b.distanceToMouse;
            });                                                                     // C# line 597-601

            if (!posTable.empty() && !posTable.front().isDangerousPos) {             // C# line 604
                resultInfo = posTable.front();
                return &resultInfo;                                                 // C# line 607
            }
        }
        else                                                                        // C# line 610
        {
            std::sort(posTable.begin(), posTable.end(), [](const PositionInfo& a, const PositionInfo& b) {
                if (a.isDangerousPos != b.isDangerousPos) return (int)a.isDangerousPos < (int)b.isDangerousPos;
                return a.distanceToMouse < b.distanceToMouse;
            });                                                                     // C# line 612-616

            if (!posTable.empty()) {                                                // C# line 618
                resultInfo = posTable.front();
                return &resultInfo;                                                 // C# line 620
            }
        }

        return nullptr;                                                             // C# line 623
    }

} // namespace EzEvade
