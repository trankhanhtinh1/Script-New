#pragma once
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include <cfloat>
#include "../../GameObjects/GameObjects.h"
#include "../../Math/MathUtils.h"
#include "ObjectCache.h"
#include "../Utils/EvadeUtils.h"

// ============================================================================
// PositionInfo + PositionInfoExtensions
//   C# original: ezEvade.PositionInfo (PositionInfo.cs, 201 lines)
//   Line-by-line port preserving original logic
// ============================================================================

namespace EzEvade {

    // Forward declarations — these classes will be implemented in their own files
    class Spell;
    class EvadeHelper;

    // ========================================================================
    // PositionInfo
    //   C# lines 15-123
    // ========================================================================
    class PositionInfo {
    public:
        // C# line 17: private static AIHeroClient myHero
        // → use SDK::GameObjects::Player

        int   posDangerLevel  = 0;                          // C# line 19
        int   posDangerCount  = 0;                          // C# line 20
        bool  isDangerousPos  = false;                      // C# line 21
        float distanceToMouse = 0;                          // C# line 22
        std::vector<int> dodgeableSpells;                   // C# line 23
        std::vector<int> undodgeableSpells;                 // C# line 24
        std::vector<int> spellList;                         // C# line 25
        Vec2  position = { 0, 0 };                          // C# line 26
        float timestamp = 0;                                // C# line 27
        float endTime = 0;                                  // C# line 28
        bool  hasExtraDistance = false;                      // C# line 29
        float closestDistance = FLT_MAX;                     // C# line 30: float.MaxValue
        float intersectionTime = FLT_MAX;                   // C# line 31: float.MaxValue
        bool  rejectPosition = false;                       // C# line 32
        float posDistToChamps = FLT_MAX;                    // C# line 33
        bool  hasComfortZone = true;                        // C# line 34
        SDK::GameObject* target = nullptr;                  // C# line 35
        bool  recalculatedPath = false;                     // C# line 36
        float speed = 0;                                    // C# line 37

        // Default constructor
        PositionInfo() = default;

        // ====================================================================
        // Constructor 1 (full)
        //   C# lines 39-56
        // ====================================================================
        PositionInfo(
            Vec2 position_,
            int posDangerLevel_,
            int posDangerCount_,
            bool isDangerousPos_,
            float distanceToMouse_,
            std::vector<int> dodgeableSpells_,
            std::vector<int> undodgeableSpells_)
        {
            this->position         = position_;                     // C# line 48
            this->posDangerLevel   = posDangerLevel_;               // C# line 49
            this->posDangerCount   = posDangerCount_;               // C# line 50
            this->isDangerousPos   = isDangerousPos_;               // C# line 51
            this->distanceToMouse  = distanceToMouse_;              // C# line 52
            this->dodgeableSpells  = std::move(dodgeableSpells_);   // C# line 53
            this->undodgeableSpells = std::move(undodgeableSpells_); // C# line 54
            this->timestamp        = EvadeUtils::TickCount();       // C# line 55
        }

        // ====================================================================
        // Constructor 2 (simple)
        //   C# lines 58-67
        // ====================================================================
        PositionInfo(
            Vec2 position_,
            bool isDangerousPos_,
            float distanceToMouse_)
        {
            this->position        = position_;                      // C# line 63
            this->isDangerousPos  = isDangerousPos_;                // C# line 64
            this->distanceToMouse = distanceToMouse_;               // C# line 65
            this->timestamp       = EvadeUtils::TickCount();        // C# line 66
        }

        // ====================================================================
        // SetAllDodgeable() — no args
        //   C# lines 69-72
        // ====================================================================
        static PositionInfo SetAllDodgeable();

        // ====================================================================
        // SetAllDodgeable(position)
        //   C# lines 74-93
        // ====================================================================
        static PositionInfo SetAllDodgeable(Vec2 position);

        // ====================================================================
        // SetAllUndodgeable()
        //   C# lines 95-122
        // ====================================================================
        static PositionInfo SetAllUndodgeable();

        // ====================================================================
        // Extension methods (PositionInfoExtensions)
        //   C# lines 125-197
        // In C++ these become member functions
        // ====================================================================

        // C# lines 129-147: GetHighestSpellID
        int GetHighestSpellID() const {
            int highest = 0;                                        // C# line 134

            for (auto spellID : undodgeableSpells)                  // C# line 136
            {
                highest = std::max(highest, spellID);               // C# line 138
            }

            for (auto spellID : dodgeableSpells)                    // C# line 141
            {
                highest = std::max(highest, spellID);               // C# line 143
            }

            return highest;                                         // C# line 146
        }

        // C# lines 149-152: isSamePosInfo
        bool IsSamePosInfo(const PositionInfo& other) const {
            // C# line 151: new HashSet<int>(spellList).SetEquals(other.spellList)
            std::unordered_set<int> set1(spellList.begin(), spellList.end());
            std::unordered_set<int> set2(other.spellList.begin(), other.spellList.end());
            return set1 == set2;
        }

        // C# lines 154-174: isBetterMovePos
        // Requires EvadeHelper::CanHeroWalkToPos — forward declared, implemented externally
        bool IsBetterMovePos() const;

        // C# lines 176-196: CompareLastMovePos
        // Requires EvadeHelper::CanHeroWalkToPos — forward declared, implemented externally
        PositionInfo CompareLastMovePos() const;
    };

} // namespace EzEvade
