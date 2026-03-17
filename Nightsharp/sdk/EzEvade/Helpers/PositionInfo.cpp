#include "PositionInfo.h"
#include "../Spells/SpellDetector.h"
#include "../Core/EvadeHelper.h"
#include "ObjectCache.h"

// ============================================================================
// PositionInfo — Implementation of static/member methods that depend on
// SpellDetector, EvadeHelper, ObjectCache (forward-declared in header).
//
// Wired to SpellDetector::spells for spell iteration.
// IsBetterMovePos / CompareLastMovePos now use EvadeHelper::CanHeroWalkToPos.
// ============================================================================

namespace EzEvade {

    // ========================================================================
    // SetAllDodgeable() — no args
    //   C# lines 69-72
    // ========================================================================
    PositionInfo PositionInfo::SetAllDodgeable() {
        auto& player = SDK::GameObjects::Player;
        Vec2 heroPos = player.GetPosition().To2D();
        return SetAllDodgeable(heroPos);
    }

    // ========================================================================
    // SetAllDodgeable(position)
    //   C# lines 74-93
    // ========================================================================
    PositionInfo PositionInfo::SetAllDodgeable(Vec2 position) {
        std::vector<int> dodgeable;
        std::vector<int> undodgeable;

        for (auto& entry : SpellDetector::spells) {
            dodgeable.push_back(entry.first);
        }

        return PositionInfo(
            position,
            0,                                                      // posDangerLevel
            0,                                                      // posDangerCount
            true,                                                   // isDangerousPos
            0,                                                      // distanceToMouse
            std::move(dodgeable),
            std::move(undodgeable));
    }

    // ========================================================================
    // SetAllUndodgeable()
    //   C# lines 95-122
    // ========================================================================
    PositionInfo PositionInfo::SetAllUndodgeable() {
        std::vector<int> dodgeable;
        std::vector<int> undodgeable;

        int posDangerLevel = 0;
        int posDangerCount = 0;

        for (auto& entry : SpellDetector::spells) {
            Spell& spell = entry.second;
            undodgeable.push_back(entry.first);
            int spellDangerLevel = spell.dangerlevel;
            posDangerLevel = std::max(posDangerLevel, spellDangerLevel);
            posDangerCount += spellDangerLevel;
        }

        auto& player = SDK::GameObjects::Player;
        Vec2 heroPos = player.GetPosition().To2D();

        return PositionInfo(
            heroPos,
            posDangerLevel,
            posDangerCount,
            true,                                                   // isDangerousPos
            0,                                                      // distanceToMouse
            std::move(dodgeable),
            std::move(undodgeable));
    }

    // ========================================================================
    // IsBetterMovePos()
    //   C# lines 154-174
    //   Compares this PositionInfo against the hero's current move destination
    // ========================================================================
    bool PositionInfo::IsBetterMovePos() const {
        auto& player = SDK::GameObjects::Player;

        // C# line 157-162: get hero path end
        Vec3 pathEnd = player.GetPathEnd();
        PositionInfo posInfo;

        if (player.IsMoving()) {
            // C# line 160-161: use last waypoint
            Vec2 movePos = pathEnd.To2D();
            posInfo = EvadeHelper::CanHeroWalkToPos(movePos,
                ObjectCache::myHeroCache.moveSpeed, 0, 0, false);
        } else {
            // C# line 165: use server position
            posInfo = EvadeHelper::CanHeroWalkToPos(
                ObjectCache::myHeroCache.serverPos2D,
                ObjectCache::myHeroCache.moveSpeed, 0, 0, false);
        }

        // C# line 168-170: if current path is less dangerous, new pos is not better
        if (posInfo.posDangerCount < this->posDangerCount) {
            return false;
        }

        return true;                                                // C# line 173
    }

    // ========================================================================
    // CompareLastMovePos()
    //   C# lines 176-196
    //   Returns the better of this PositionInfo and the current move destination
    // ========================================================================
    PositionInfo PositionInfo::CompareLastMovePos() const {
        auto& player = SDK::GameObjects::Player;

        // C# line 179-184
        Vec3 pathEnd2 = player.GetPathEnd();
        PositionInfo posInfo;

        if (player.IsMoving()) {
            Vec2 movePos = pathEnd2.To2D();
            posInfo = EvadeHelper::CanHeroWalkToPos(movePos,
                ObjectCache::myHeroCache.moveSpeed, 0, 0, false);
        } else {
            posInfo = EvadeHelper::CanHeroWalkToPos(
                ObjectCache::myHeroCache.serverPos2D,
                ObjectCache::myHeroCache.moveSpeed, 0, 0, false);
        }

        // C# line 190-195: return the less dangerous one
        if (posInfo.posDangerCount < this->posDangerCount) {
            return posInfo;
        }

        return *this;
    }

} // namespace EzEvade

// Definition of Position::detail::GetDetectorSpells (declared in Position.h)
#include "Position.h"
namespace EzEvade { namespace Position { namespace detail {
    std::map<int, Spell>& GetDetectorSpells() { return SpellDetector::spells; }
}}}
