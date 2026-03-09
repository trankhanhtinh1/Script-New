#pragma once
#include "GameObjects.h"
#include "sdk/Events/InterruptableSpell.h"
#include "sdk/Wrappers/Spells/LastCast.h"
#include <cfloat>

namespace SDK {
namespace GameObjectExtensions {

    // EnsoulSharp-like PreviousPosition helper.
    inline Vec3 PreviousPosition(const GameObject& obj) {
        if (!obj.IsValid()) {
            return Vec3();
        }

        AiManager ai = obj.GetAiManager();
        if (ai.IsValid()) {
            Vec3 pathStart = ai.GetPathStart();
            if (!pathStart.IsZero()) {
                return pathStart;
            }
        }

        Vec3 serverPos = obj.GetServerPosition();
        if (!serverPos.IsZero()) {
            return serverPos;
        }

        return obj.GetPosition();
    }

    // EnsoulSharp-like DistanceToPlayer helper.
    inline float DistanceToPlayer(const GameObject& obj) {
        const auto& player = GameObjects::Player;
        if (!obj.IsValid() || !player.IsValid()) {
            return FLT_MAX;
        }
        return obj.DistanceTo(player);
    }

    // Keeps EnsoulSharp typo naming for compatibility with older scripts.
    inline bool IsCastingImporantSpell(const GameObject& obj) {
        if (!obj.IsValid()) {
            return false;
        }

        SpellBook sb(obj.address);
        if (!sb.IsValid() || !sb.IsCasting()) {
            return false;
        }

        auto last = LastCast::GetLastCastedSpell(obj);
        if (!last.IsValid) {
            return false;
        }

        if (last.TimeSinceCast(Game::GetTime()) > 0.5f) {
            return false;
        }

        return InterruptableSpell::IsInterruptable(
            obj.GetChampionName(),
            static_cast<SpellSlotId>(last.Slot));
    }

    // Correct spelling alias.
    inline bool IsCastingImportantSpell(const GameObject& obj) {
        return IsCastingImporantSpell(obj);
    }

} // namespace GameObjectExtensions
} // namespace SDK
