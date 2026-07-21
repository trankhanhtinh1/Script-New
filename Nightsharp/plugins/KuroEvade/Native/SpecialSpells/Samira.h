#pragma once

#include "SpecialSpellCommon.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Plugins::KuroEvade::SpecialSpells::Samira {

struct DashState {
    Vec2 Start;
    Vec2 End;
    float Height = 0.0f;
    int StartTick = 0;
    int EndTick = 0;
};

inline std::unordered_map<int, DashState> ActiveDashes;

inline bool IsFlair(const Database::SpellData& data) {
    return EqualsText(data.DetectionGroup, "SamiraQ") ||
           EqualsSpell(data, "SamiraQ") ||
           EqualsSpell(data, "SamiraQGun") ||
           EqualsSpell(data, "SamiraQSword") ||
           EqualsSpell(data, "SamiraQBufferedSword");
}

inline void RefreshFromLiveDash(const SDK::AIBaseClient& caster,
                                DashState& state,
                                int now) {
    const auto dash = SDK::Events::Dash::GetDashInfo(caster);
    if (!dash.IsDash || dash.EndTick <= now || dash.EndPos.IsZero()) {
        return;
    }
    if (!dash.StartPos.IsZero()) {
        state.Start = dash.StartPos.To2D();
        state.Height = dash.StartPos.y;
    }
    state.End = dash.EndPos.To2D();
    state.StartTick = dash.StartTick > 0 ? dash.StartTick : state.StartTick;
    state.EndTick = dash.EndTick;
}

inline bool ProcessCast(const CastContext& context, ProcessResult& result) {
    const int casterId = context.Caster.NetworkId();
    const int now = SDK::Variables::TickCount();
    if (EqualsSpell(context.Source, "SamiraE")) {
        const Vec2 start = context.Start.IsZero()
            ? context.Caster.Position().To2D()
            : context.Start;
        const Vec2 direction = SafeDirection(start, context.End, context.Caster);
        constexpr float dashRange = 650.0f;
        constexpr float dashSpeed = 1600.0f;
        const int startTick = now - SDK::Game::Ping() / 2;
        DashState state;
        state.Start = start;
        state.End = start + direction * dashRange;
        state.Height = context.Caster.IsValid()
            ? context.Caster.Position().y
            : context.Start3.y;
        state.StartTick = startTick;
        state.EndTick = startTick + std::max(0, context.Source.Delay) +
            static_cast<int>(std::lround(1000.0f * dashRange / dashSpeed));
        ActiveDashes[casterId] = state;

        result.Data.Range = dashRange;
        result.Data.MissileSpeed = dashSpeed;
        result.Data.UseEndPosition = false;
        result.Data.FixedRange = false;
        result.Data.Finalize();
        return true;
    }

    if (!IsFlair(context.Source)) {
        return false;
    }
    const auto found = ActiveDashes.find(casterId);
    if (found == ActiveDashes.end()) {
        return false;
    }

    DashState state = found->second;
    RefreshFromLiveDash(context.Caster, state, now);
    ActiveDashes[casterId] = state;
    if (state.Start.IsZero() || state.End.IsZero() ||
        now < state.StartTick - 75 || now > state.EndTick + 75) {
        return false;
    }

    Database::SpellData explosives = context.Source;
    explosives.DisplayName = "Flair (E-Q Explosives)";
    explosives.Type = Database::SkillShotType::SkillshotLine;
    explosives.Range = std::max(1.0f, state.Start.Distance(state.End));
    explosives.Radius = 65.0f; // 130 total width for the non-projectile Q.
    explosives.MissileSpeed = 0.0f;
    explosives.Delay = std::max(0, state.EndTick - state.StartTick);
    explosives.ExtraEndTime = 125;
    explosives.UseEndPosition = true;
    explosives.FixedRange = false;
    explosives.HasEndExplosion = false;
    explosives.CollisionObjects.clear();
    explosives.Finalize();

    AddExtra(result,
             From2D(state.Start, state.Height),
             From2D(state.End, state.Height),
             explosives, state.StartTick, true);
    result.NoProcess = true;
    return true;
}

inline void BeginUpdate() {
    const int now = SDK::Variables::TickCount();
    for (auto it = ActiveDashes.begin(); it != ActiveDashes.end();) {
        if (now > it->second.EndTick + 300) {
            it = ActiveDashes.erase(it);
        } else {
            ++it;
        }
    }
}

inline void Clear() {
    ActiveDashes.clear();
}

} // namespace Plugins::KuroEvade::SpecialSpells::Samira
