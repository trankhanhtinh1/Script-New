#pragma once

#include "SpecialSpellCommon.h"

#include <unordered_map>
#include <unordered_set>

namespace Plugins::KuroEvade::SpecialSpells {

struct Yuumi {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        (void)context;
        (void)result;
        return false;
    }

    static void BeginUpdate() {
        ActiveMissiles().clear();
    }

    static void Update(SDK::Skillshot& skillshot) {
        if (_stricmp(skillshot.SData.SpellName.c_str(), "YuumiQCast") != 0) {
            return;
        }

        auto* missileSkillshot = dynamic_cast<SDK::SkillshotMissile*>(&skillshot);
        if (!missileSkillshot || !missileSkillshot->Missile.IsValid()) {
            return;
        }

        const int missileId = missileSkillshot->Missile.NetworkId();
        if (missileId == 0) {
            return;
        }
        ActiveMissiles().insert(missileId);

        const int now = SDK::Variables::TickCount();
        State& state = States()[missileId];
        const Vec2 current = missileSkillshot->Missile.Position().To2D();
        if (!state.LastPosition.IsValid() || state.LastPosition.IsZero()) {
            state.LastPosition = skillshot.StartPosition;
            state.LastUpdateTick = now;
        }
        if (now - state.LastUpdateTick <= 100) {
            return;
        }

        Vec2 direction = (current - state.LastPosition).Normalized();
        if (direction.IsZero()) {
            direction = skillshot.Direction;
        }
        state.LastPosition = current;
        state.LastUpdateTick = now;
        if (direction.IsZero()) {
            return;
        }

        skillshot.StartPosition = current;
        skillshot.EndPosition = current + direction * 500.0f;
        RefreshLineGeometry(skillshot);
    }

    static void EndUpdate() {
        auto& states = States();
        const auto& active = ActiveMissiles();
        for (auto it = states.begin(); it != states.end();) {
            if (active.find(it->first) == active.end()) {
                it = states.erase(it);
            } else {
                ++it;
            }
        }
    }

    static void Clear() {
        States().clear();
        ActiveMissiles().clear();
    }

private:
    struct State {
        Vec2 LastPosition;
        int LastUpdateTick = 0;
    };

    static std::unordered_map<int, State>& States() {
        static std::unordered_map<int, State> states;
        return states;
    }

    static std::unordered_set<int>& ActiveMissiles() {
        static std::unordered_set<int> active;
        return active;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
