#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIOlaf.h"
#include "AIOlafGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Olaf {
using ControllerHelpers::SpellEnabled;

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Slider;

inline Menu* TacticsMenu = nullptr;
inline bool AxeTracked = false;
inline Vector3 AxePosition{};
inline int AxeExpireTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline int LastAxeCastTick = 0;
inline int LastAttackTargetId = 0;
inline bool RagnarokActive = false;
inline std::array<int, 4> LastCastTick{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        (reactive || LastCastTick[static_cast<std::size_t>(slot)] + 45 <= Now());
}

inline bool CanUseAxe(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kUndertowRange + 80.0f)) {
        return false;
    }
    const Vector3 aim = PredictPosition(target, 0.25f);
    return AxeLandingValid(player.Position(), aim, target.BoundingRadius()) &&
        !ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kUndertowWidth);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode, reactive) || !CanUseAxe(target) ||
        (!reactive && Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true))) return false;
    auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    if (!prediction.GetCastPosition().IsValid() ||
        prediction.GetCastPosition().IsZero() ||
        !prediction.CollisionObjects.empty()) return false;
    const Vector3 aim = prediction.GetCastPosition();
    if (!UndertowLineHits(player.Position(), aim, aim, kUndertowWidth) ||
        SDK::NavMesh::IsWall(aim)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    LastAxeCastTick = Now();
    AxeTracked = true;
    AxePosition = aim;
    AxeExpireTick = Now() + kAxeLifetimeMs;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRecklessSwingRange) ||
        !Ready(2, mode, reactive) ||
        (!reactive && Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true))) return false;
    const float rawDamage = 70.0f + 0.40f * player.TotalAttackDamage();
    const bool lethal = rawDamage >= target.Health() + target.AllShield();
    if (!lethal && target.HealthPercent() > 55.0f && mode == Mode::Harass) return false;
    if (!Engine::TryCast(Profiles::Olaf.Spells[2], target, mode,
                         lethal ? StepRule::RequireTargetLow : StepRule::None,
                         reactive)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) ||
        player.HealthPercent() > Slider(TacticsMenu, "WHealth", 72) && !reactive) {
        return false;
    }
    if (!Engine::TryCast(Profiles::Olaf.Spells[1], target, mode,
                         StepRule::AllowDuringWindup, reactive)) return false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode, reactive) || RagnarokActive) return false;
    const bool hardThreat = IncomingThreatUntil >= Now();
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRagnarokThreatRadius);
    if (!RagnarokCommitAllowed(hardThreat,
                               Orbwalker::CanAttack() || Orbwalker::IsWindingUp(),
                               player.HealthPercent(), enemies)) return false;
    if (!reactive && player.HealthPercent() > Slider(TacticsMenu, "RHealth", 60)) {
        return false;
    }
    if (!Engine::TryCast(Profiles::Olaf.Spells[3], target, mode,
                         StepRule::AllowDuringWindup, true)) return false;
    LastCastTick[3] = Now();
    RagnarokActive = true;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kUndertowRange + 80.0f)) return;
    if (CastE(target, Mode::Combo, false)) return;
    if (CastQ(target, Mode::Combo, false)) return;
    if (CastR(target, Mode::Combo, false)) return;
    (void)CastW(target, Mode::Combo, false);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "HarassMana", 38)) return;
    if (AxeTracked && AxeCanReset(AxePosition.Distance2D(player.Position()),
                                  player.Position().Distance2D(target.Position()),
                                  AxeTracked, target.IsValid())) {
        (void)CastQ(target, Mode::Harass, false);
        return;
    }
    if (CastE(target, Mode::Harass, false)) return;
    (void)CastQ(target, Mode::Harass, false);
}

inline void Farm(Mode mode) {
    if (mode == Mode::Jungle && Ready(1, mode) &&
        GameObjects::Player().HealthPercent() < 65.0f) {
        (void)CastW({}, mode, false);
    }
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, kRagnarokThreatRadius) &&
        target.Position().Distance2D(GameObjects::Player().Position()) < 450.0f) {
        if (CastR(target, Mode::Flee, true)) return;
    }
    const auto player = GameObjects::Player();
    const Vector3 cursor = Game::CursorPos();
    if (!AxeLandingValid(player.Position(), cursor) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(cursor, kUndertowWidth)) return;
    if (Engine::ControllerCastPosition(0, cursor)) {
        LastCastTick[0] = Now();
        AxeTracked = true;
        AxePosition = cursor;
        AxeExpireTick = Now() + kAxeLifetimeMs;
    }
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (IncomingThreatUntil >= Now()) {
        const auto threat = Engine::EnemyByNetworkId(IncomingThreatTargetId);
        if (Engine::ValidEnemy(threat, kRagnarokThreatRadius) &&
            CastR(threat, Mode::Automatic, true)) return;
        if (Engine::ValidEnemy(threat, kRecklessSwingRange) &&
            CastE(threat, Mode::Automatic, true)) return;
    }
    if (player.HealthPercent() < 38.0f && Engine::ValidEnemy(target, kRecklessSwingRange)) {
        (void)CastW(target, Mode::Automatic, true);
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    if (AxeTracked && AxeExpireTick < Now()) AxeTracked = false;
    if (RagnarokActive && (!Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady())) RagnarokActive = false;
    if (IncomingThreatUntil < Now()) {
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(
        selected, kUndertowRange + 80.0f);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Olaf tactics"));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 38, 0, 100));
    TacticsMenu->Add(new MenuSlider("WHealth", "Use W below health percent", 72, 1, 100));
    TacticsMenu->Add(new MenuSlider("RHealth", "Allow R below health percent", 60, 1, 100));
}

inline void OnLoad() {
    AxeTracked = false;
    AxePosition = {};
    AxeExpireTick = 0;
    IncomingThreatUntil = 0;
    IncomingThreatTargetId = 0;
    IncomingThreatEndpoint = {};
    LastCastTick.fill(0);
    RagnarokActive = false;
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    OnLoad();
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (!Engine::WasControllerCast(static_cast<int>(args.Slot))) return;
        LastAttackTargetId = static_cast<int>(args.TargetNetworkId);
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick,
                                       analysis.LineThreatUntilTick);
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAttackTargetId = static_cast<int>(args.TargetNetworkId);
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) &&
        Engine::TextContains(args.BuffName, "olafragnarok")) RagnarokActive = true;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) &&
        Engine::TextContains(args.BuffName, "olafragnarok")) RagnarokActive = false;
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.EndTime <= Game::Time()) RagnarokActive = false;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
                           IncomingThreatUntil, kRagnarokThreatRadius, 1200);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && ControllerHelpers::ObjectEventIsAllied(args)) {
        AxePosition = args.Sender.Position;
        AxeTracked = AxePosition.IsValid() && !AxePosition.IsZero();
        AxeExpireTick = Now() + kAxeLifetimeMs;
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && AxeTracked &&
        args.Sender.Position.Distance2D(AxePosition) < kUndertowSlowRadius) {
        AxeTracked = false;
    }
}

inline constexpr const char* Scenarios[] = {
    "Undertow prediction, collision and axe return reset",
    "Reckless Swing true-damage lethal boundary and health cost",
    "Tough It Out attack-windup shield timing",
    "Ragnarok crowd-control immunity commit gate",
    "jungle low-health W and E last hit",
    "turret chase and enemy-count safety",
    "manual cast and buff expiry reconciliation",
    "gapcloser and interrupt threat tracking",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Olaf;
    controller.ControllerId = "champion.kuroaio.ai.olaf.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIOlaf.md";
    controller.ImplementationSummary =
        "Axe lifecycle, true-damage health gate, Ragnarok threat reaction and mode-specific farm logic.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &OnBuffUpdate;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Olaf
