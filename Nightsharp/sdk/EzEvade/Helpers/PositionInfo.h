#pragma once
#include "sdk/SDK.h"
#include <algorithm>
#include <cfloat>
#include <functional>
#include <unordered_set>
#include <vector>

namespace EzEvade {

struct ActiveSpellSnapshot {
    int SpellId = 0;
    int DangerLevel = 0;
};

class PositionInfo {
public:
    int PosDangerLevel = 0;
    int PosDangerCount = 0;
    bool IsDangerousPos = false;
    float DistanceToMouse = 0.0f;
    std::vector<int> DodgeableSpells = {};
    std::vector<int> UndodgeableSpells = {};
    std::vector<int> SpellList = {};
    Vec2 Position = Vec2();
    float Timestamp = 0.0f;
    float EndTime = 0.0f;
    bool HasExtraDistance = false;
    float ClosestDistance = FLT_MAX;
    float IntersectionTime = FLT_MAX;
    bool RejectPosition = false;
    float PosDistToChamps = FLT_MAX;
    bool HasComfortZone = true;
    SDK::GameObject Target = SDK::GameObject();
    bool RecalculatedPath = false;
    float Speed = 0.0f;

    PositionInfo() = default;

    PositionInfo(const Vec2& position,
                 int posDangerLevel,
                 int posDangerCount,
                 bool isDangerousPos,
                 float distanceToMouse,
                 const std::vector<int>& dodgeableSpells,
                 const std::vector<int>& undodgeableSpells)
        : PosDangerLevel(posDangerLevel),
          PosDangerCount(posDangerCount),
          IsDangerousPos(isDangerousPos),
          DistanceToMouse(distanceToMouse),
          DodgeableSpells(dodgeableSpells),
          UndodgeableSpells(undodgeableSpells),
          Position(position),
          Timestamp((float)SDK::Game::GetTickCount()) {}

    PositionInfo(const Vec2& position,
                 bool isDangerousPos,
                 float distanceToMouse)
        : IsDangerousPos(isDangerousPos),
          DistanceToMouse(distanceToMouse),
          Position(position),
          Timestamp((float)SDK::Game::GetTickCount()) {}

    static inline std::function<std::vector<ActiveSpellSnapshot>()> ActiveSpellsProvider = {};
    static inline std::function<PositionInfo()> CurrentMovePositionProvider = {};

    static PositionInfo SetAllDodgeable() {
        const auto& me = SDK::GameObjects::Player;
        return SetAllDodgeable(me.IsValid() ? me.GetPosition().To2D() : Vec2());
    }

    static PositionInfo SetAllDodgeable(const Vec2& position) {
        std::vector<int> dodgeableSpells;
        std::vector<int> undodgeableSpells;

        if (ActiveSpellsProvider) {
            for (const auto& snap : ActiveSpellsProvider()) {
                dodgeableSpells.push_back(snap.SpellId);
            }
        }

        return PositionInfo(
            position,
            0,
            0,
            true,
            0.0f,
            dodgeableSpells,
            undodgeableSpells
        );
    }

    static PositionInfo SetAllUndodgeable() {
        std::vector<int> dodgeableSpells;
        std::vector<int> undodgeableSpells;
        int posDangerLevel = 0;
        int posDangerCount = 0;

        if (ActiveSpellsProvider) {
            for (const auto& snap : ActiveSpellsProvider()) {
                undodgeableSpells.push_back(snap.SpellId);
                posDangerLevel = std::max(posDangerLevel, snap.DangerLevel);
                posDangerCount += snap.DangerLevel;
            }
        }

        const auto& me = SDK::GameObjects::Player;
        const Vec2 mePos = me.IsValid() ? me.GetPosition().To2D() : Vec2();

        return PositionInfo(
            mePos,
            posDangerLevel,
            posDangerCount,
            true,
            0.0f,
            dodgeableSpells,
            undodgeableSpells
        );
    }
};

namespace PositionInfoExtensions {

inline int GetHighestSpellID(const PositionInfo* posInfo) {
    if (!posInfo) return 0;

    int highest = 0;
    for (int spellID : posInfo->UndodgeableSpells) {
        highest = std::max(highest, spellID);
    }
    for (int spellID : posInfo->DodgeableSpells) {
        highest = std::max(highest, spellID);
    }
    return highest;
}

inline bool IsSamePosInfo(const PositionInfo& a, const PositionInfo& b) {
    std::unordered_set<int> sa(a.SpellList.begin(), a.SpellList.end());
    std::unordered_set<int> sb(b.SpellList.begin(), b.SpellList.end());
    return sa == sb;
}

inline bool IsBetterMovePos(const PositionInfo& newPosInfo) {
    if (!PositionInfo::CurrentMovePositionProvider) {
        return true;
    }

    PositionInfo currentPosInfo = PositionInfo::CurrentMovePositionProvider();
    if (currentPosInfo.PosDangerCount < newPosInfo.PosDangerCount) {
        return false;
    }

    return true;
}

inline PositionInfo CompareLastMovePos(const PositionInfo& newPosInfo) {
    if (!PositionInfo::CurrentMovePositionProvider) {
        return newPosInfo;
    }

    PositionInfo currentPosInfo = PositionInfo::CurrentMovePositionProvider();
    if (currentPosInfo.PosDangerCount < newPosInfo.PosDangerCount) {
        return currentPosInfo;
    }

    return newPosInfo;
}

} // namespace PositionInfoExtensions

} // namespace EzEvade
