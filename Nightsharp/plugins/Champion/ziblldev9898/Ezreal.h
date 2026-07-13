#pragma once

#include "../../../SDK/SDK.h"
#include "AioMenu.h"

#include <cfloat>
#include <string>

namespace Plugins::ziblldev9898::Ezreal {

inline Spell Q{SpellSlot::Q, 1200.0f};
inline bool Loaded = false;
inline DWORD LastQTick = 0;

static AIHeroClient Player() {
    return ObjectManager::Player();
}

static bool IsValidTarget(const AIHeroClient& target) {
    return target.IsValid() && !target.IsDead() && target.Health() > 0.0f &&
        Extensions::IsValidTarget(target, Q.Range, true);
}

static AIHeroClient GetTarget() {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(Q.Range, DamageType::Physical) : AIHeroClient();
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    if (!Loaded || !Q.IsReady() ||
        !AioMenu::Bool("ezreal", "useQ")) return;
    if (SDK::Prediction::CurrentPredictionName() != "ZD Prediction") return;
    if (Orbwalker::ActiveMode() != OrbwalkingMode::Combo) return;

    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) return;

    const DWORD now = GetTickCount();
    if (LastQTick != 0 && now - LastQTick < 100) return;

    const auto target = GetTarget();
    if (!IsValidTarget(target)) return;

    const auto prediction = Q.GetPrediction(target, false, Q.Range, Q.CollisionObjects);
    const HitChance minimumHitChance = player.Position().Distance2D(target.Position()) <= 500.0f
        ? HitChance::Medium
        : HitChance::High;
    if (prediction.Hitchance < minimumHitChance || !prediction.CollisionObjects.empty()) return;
    if (Q.Cast(prediction.GetCastPosition())) LastQTick = now;
}

static void OnGameLoad() {
    if (Loaded || !Player().IsValid()) return;

    Q = Spell(SpellSlot::Q, 1200.0f);
    Q.SetSkillshot(0.25f, 53.0f, 2000.0f, true, SpellType::Line);

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Loaded = true;
}

static void OnUnload() {
    if (!Loaded) return;
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    LastQTick = 0;
    Loaded = false;
}

}
