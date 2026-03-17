#pragma once
// =========================================================================
// EvadeHelper.h — C++ port of EzEvade/Core/EvadeHelper.cs (1322 lines)
// Line-by-line, preserving original logic
// =========================================================================
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <functional>

#include "../Spells/Spell.h"
#include "../Spells/SpellData.h"
#include "../Helpers/ObjectCache.h"
#include "../Helpers/PositionInfo.h"
#include "../Helpers/Position.h"
#include "../Utils/EvadeUtils.h"
#include "../Utils/MathUtilsEz.h"
#include "../Utils/MathUtilsCPA.h"
#include "../../GameObjects/GameObjects.h"
#include "../../Game.h"
#include "../../Math/MathUtils.h"

namespace EzEvade {

    // Forward declarations
    struct EvadeSpellData;  // defined in EvadeSpellData.h
    namespace SpellDetector {
        extern std::map<int, Spell> spells;
        void UpdateSpells();
        float GetLowestEvadeTime(Spell*& outSpell);
        std::vector<int> GetSpellList();
    }

    // =========================================================================
    // EvadeHelper class
    //   C# original: ezEvade.EvadeHelper (EvadeHelper.cs)
    // =========================================================================
    class EvadeHelper {
    public:
        // C# line 19
        static inline bool fastEvadeMode = false;

        // --- PlayerInSkillShot (C# line 21-24) ---
        static bool PlayerInSkillShot(Spell& spell);

        // --- InitPositionInfo (C# line 26-50) ---
        static PositionInfo InitPositionInfo(const Vec2& pos, float extraDelayBuffer,
            float extraEvadeDistance, const Vec2& lastMovePos, Spell* lowestEvadeTimeSpell);

        // --- GetBestPositionTest (C# line 52-127) ---
        // Test version — returns sorted list
        static std::vector<PositionInfo> GetBestPositionTest();

        // --- GetBestPosition (C# line 129-266) ---
        static PositionInfo* GetBestPosition();

        // --- GetBestPositionMovementBlock (C# line 268-322) ---
        static PositionInfo* GetBestPositionMovementBlock(const Vec2& movePos);

        // --- GetBestPositionBlink (C# line 324-380) ---
        static PositionInfo* GetBestPositionBlink();

        // --- GetBestPositionDash (C# line 382-454) ---
        static PositionInfo* GetBestPositionDash(const EvadeSpellData& spell);

        // --- GetBestPositionTargetedDash (C# line 456-625) ---
        static PositionInfo* GetBestPositionTargetedDash(const EvadeSpellData& spell);

        // --- CheckWindupTime (C# line 627-641) ---
        static bool CheckWindupTime(float windupTime);

        // --- GetMovementBlockPositionValue (C# line 643-657) ---
        static float GetMovementBlockPositionValue(const Vec2& pos, const Vec2& movePos);

        // --- PositionInfoStillValid (C# line 659-662) ---
        static bool PositionInfoStillValid(const PositionInfo& posInfo, float moveSpeed = 0);

        // --- GetExtendedPositions (C# line 664-678) ---
        static std::vector<Vec2> GetExtendedPositions(const Vec2& from, const Vec2& to, float extendDistance);

        // --- GetExtendedSafePosition (C# line 680-700) ---
        static Vec2 GetExtendedSafePosition(const Vec2& from, const Vec2& to, float extendDistance);

        // --- CalculateEvadeTime (C# line 702-713) ---
        static void CalculateEvadeTime();

        // --- GetFastestPosition (C# line 715-730) ---
        static Vec2 GetFastestPosition(Spell& spell);

        // --- GetFastestPositions (C# line 732-750) ---
        static std::vector<Vec2> GetFastestPositions();

        // --- CompareFastestPosition (C# line 752-759) ---
        static float CompareFastestPosition(Spell& spell, const Vec2& start, const Vec2& movePos);

        // --- GetMinCPADistance (C# line 761-772) ---
        static float GetMinCPADistance(const Vec2& movePos);

        // --- GetCombinedIntersectionDistance (C# line 774-786) ---
        static float GetCombinedIntersectionDistance(const Vec2& movePos);

        // --- GetIntersectDistance (C# line 820-859) ---
        static float GetIntersectDistance(Spell& spell, const Vec2& start, const Vec2& end);

        // --- CanHeroWalkToPos (C# line 861-906) ---
        static PositionInfo CanHeroWalkToPos(const Vec2& pos, float speed, float delay, float extraDist, bool useServerPosition = true);

        // --- GetClosestDistanceApproach (C# line 908-1071) ---
        static float GetClosestDistanceApproach(Spell& spell, const Vec2& pos, float speed,
            float delay, const Vec2& heroPos, float extraDist);

        // --- PredictSpellCollision (C# line 1073-1088) ---
        static bool PredictSpellCollision(Spell& spell, const Vec2& pos, float speed,
            float delay, const Vec2& heroPos, float extraDist, bool useServerPosition = true);

        // --- GetRealHeroPos (C# line 1090-1105) ---
        static Vec2 GetRealHeroPos(float delay = 0);

        // --- CheckPathCollision (C# line 1107-1120) ---
        static bool CheckPathCollision(const Vec2& movePos);

        // --- CheckPointCollision (C# line 1122-1135) ---
        static bool CheckPointCollision(const Vec2& movePos);

        // --- CheckMovePath (C# line 1137-1187) ---
        static bool CheckMovePath(const Vec2& movePos, float extraDelay = 0);

        // --- LineIntersectLinearSegment (C# line 1190-1215) ---
        static bool LineIntersectLinearSegment(const Vec2& a1, const Vec2& b1, const Vec2& a2, const Vec2& b2);

        // --- CheckMoveToDirection (C# line 1217-1318) ---
        static bool CheckMoveToDirection(const Vec2& from, const Vec2& movePos, float extraDelay = 0);
    };

} // namespace EzEvade
