#pragma once

#include "PositionInfo.h"

#include <algorithm>
#include <unordered_set>

namespace Plugins::KuroEvade {

struct PositionInfoExtensions final {
    static int GetHighestSpellID(const PositionInfo& info) {
        int highest = 0;
        for (int id : info.undodgeableSpells) {
            highest = std::max(highest, id);
        }
        for (int id : info.dodgeableSpells) {
            highest = std::max(highest, id);
        }
        return highest;
    }

    static bool IsSamePosInfo(const PositionInfo& lhs, const PositionInfo& rhs) {
        return std::unordered_set<int>(lhs.spellList.begin(), lhs.spellList.end()) ==
               std::unordered_set<int>(rhs.spellList.begin(), rhs.spellList.end());
    }

    static bool IsBetterMovePos(const PositionInfo& current, const PositionInfo& candidate) {
        if (candidate.dangerCount != current.dangerCount) {
            return candidate.dangerCount <= current.dangerCount;
        }
        if (candidate.dangerLevel != current.dangerLevel) {
            return candidate.dangerLevel <= current.dangerLevel;
        }
        return !candidate.dangerous || current.dangerous;
    }

    static PositionInfo CompareLastMovePos(const PositionInfo& current, const PositionInfo& candidate) {
        return IsBetterMovePos(current, candidate) ? candidate : current;
    }
};

} // namespace Plugins::KuroEvade
