#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIBraum.h"
#include "AIBraumGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Braum {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatOrigin{};
inline Vector3 IncomingThreatEndpoint{};
inline int ShieldActiveUntil = 0;
inline int LastQTargetId = 0;
inline int LastRTargetId = 0;
inline int FissureActiveUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;

struct PassiveTarget {
    int NetworkId = 0;
    int Stacks = 0;
    int ExpireTick = 0;
};
inline std::array<PassiveTarget, 16> PassiveTargets{};

inline PassiveTarget* FindPassive(int id, bool create = false) {
    if (id == 0) return nullptr;
    for (auto& state : PassiveTargets)
        if (state.NetworkId == id) return &state;
    if (!create) return nullptr;
    for (auto& state : PassiveTargets) {
        if (state.NetworkId == 0 || state.ExpireTick <= Now()) {
            state = PassiveTarget{id, 0, Now() + 4000};
            return &state;
        }
    }
    return nullptr;
}

inline int PassiveStacks(int id) {
    auto* state = FindPassive(id);
    if (!state || state->ExpireTick <= Now()) return 0;
    return ClampStacks(state->Stacks);
}

inline void SetPassiveStacks(int id, int stacks, int durationMs = 4000) {
    auto* state = FindPassive(id, true);
    if (!state) return;
    state->Stacks = ClampStacks(stacks);
    state->ExpireTick = Now() + durationMs;
}

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    if (slot < 0 || slot >= 4 || !Engine::RuntimeSpells[slot] ||
        !Engine::RuntimeSpells[slot]->IsReady() || !SpellEnabled(slot, mode)) return false;
    return reactive || Now() - LastCastTick[static_cast<std::size_t>(slot)] >= 45;
}

inline bool ManaGate(int slot, Mode mode, bool reactive = false) {
    if (reactive) return true;
    const float reserve = static_cast<float>(
        mode == Mode::Harass ? Slider(TacticsMenu, "HarassMana", 56) :
        (mode == Mode::LaneClear || mode == Mode::LastHit ? Slider(FarmMenu, "LaneMana", 30) :
         (mode == Mode::Jungle ? Slider(FarmMenu, "JungleMana", 24) : 0)));
    return ControllerHelpers::PlayerManaPercent() >= reserve &&
           CurrentResource() >= SpellCost(slot);
}

inline bool ProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target);
}

inline AIHeroClient RescueAlly() {
    const auto ally = SelectProtectionAlly(kWRange);
    return Engine::ValidAlly(ally, kWRange) ? ally : AIHeroClient{};
}

inline bool SafeDestination(const Vector3& destination, Mode mode, bool reactive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !destination.IsValid()) return false;
    const bool defensive = reactive || mode == Mode::Flee ||
        player.HealthPercent() <= Slider(TacticsMenu, "DefensiveHealth", 48);
    return WResistSafe(destination, Engine::CountEnemiesAt(destination, 450.0f),
                       Engine::CountAlliesAt(destination, 550.0f),
                       Engine::UnderEnemyTurret(destination) &&
                           !Engine::UnderEnemyTurret(player.Position()),
                       defensive, Slider(TacticsMenu, "MaximumEnemies", 3));
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 60.0f) ||
        ProtectedTarget(target) || !Ready(0, mode, reactive) ||
        !ManaGate(0, mode, reactive) ||
        ControllerHelpers::PreserveAttack(reactive)) return false;
    const Vector3 predicted = PredictPosition(target, reactive ? 0.18f : 0.28f);
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    if (!prediction.CollisionObjects.empty()) return false;
    const auto plan = BuildQPlan(player.Position(), predicted, target.BoundingRadius());
    if (!plan.Valid || !QHits(plan.Origin, plan.Aim, predicted, target.BoundingRadius()) ||
        ProjectileWallBlocksFromPlayer(plan.Aim, kQWidth * 0.5f)) return false;
    const int stacks = PassiveStacks(static_cast<int>(target.NetworkId()));
    const bool stunReady = PassiveStunReady(stacks);
    const float damage = player.CalculateMagicDamage(target,
        QDamage(SpellRank(0), player.AP(), target.MaxHealth()));
    if (!reactive && !Lethal(target, damage) && !stunReady && stacks == 0 &&
        target.HealthPercent() > Slider(TacticsMenu, "QHealth", 82) &&
        Engine::CountAlliesAt(target.Position(), 700.0f) <= 0) return false;
    if (!Engine::ControllerCastPosition(0, plan.Aim)) return false;
    LastQTargetId = static_cast<int>(target.NetworkId());
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& selected, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) || !ManaGate(1, mode, reactive) ||
        ControllerHelpers::PreserveAttack(reactive)) return false;
    const auto ally = RescueAlly();
    if (!ally.IsValid() || ally.NetworkId() == player.NetworkId()) return false;
    const bool selectedAlly = selected.IsValid() &&
        selected.NetworkId() == ally.NetworkId();
    const int enemies = Engine::CountEnemiesAt(ally.Position(), 500.0f);
    const bool hardThreat = IncomingThreatUntil > Now() &&
        (IncomingThreatTargetId == static_cast<int>(ally.NetworkId()) ||
         IncomingThreatTargetId == static_cast<int>(player.NetworkId()));
    if (!WAllyWorthwhile(ally.HealthPercent(), enemies, selectedAlly, hardThreat) &&
        mode != Mode::Flee) return false;
    const auto plan = BuildWPlan(player.Position(), ally.Position());
    if (!plan.Valid || !SafeDestination(plan.Destination, mode, reactive)) return false;
    if (!Engine::ControllerCastUnit(1, ally)) return false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode, reactive) || !ManaGate(2, mode, reactive)) return false;
    const bool incoming = IncomingThreatUntil > Now();
    const bool hardThreat = InterruptExpireTick > Now() || GapcloserExpireTick > Now();
    const int enemies = Engine::CountEnemiesAt(player.Position(), 500.0f);
    const bool worthwhile = EInterceptionWorthwhile(incoming, hardThreat,
        player.HealthPercent(), enemies);
    if (!worthwhile && mode != Mode::Flee && !reactive) return false;
    if (!incoming && !reactive && mode != Mode::Combo) return false;
    if (incoming && IncomingThreatOrigin.IsValid() && IncomingThreatEndpoint.IsValid() &&
        !ShieldCoversLine(player.Position(), IncomingThreatOrigin,
                          IncomingThreatEndpoint, player.Position())) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    ShieldActiveUntil = Now() + static_cast<int>(kEShieldSeconds * 1000.0f);
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRange + 60.0f) ||
        ProtectedTarget(target) || !Ready(3, mode, reactive) ||
        !ManaGate(3, mode, reactive) || ControllerHelpers::PreserveAttack(reactive)) return false;
    const Vector3 predicted = PredictPosition(target, reactive ? 0.22f : 0.42f);
    const auto plan = BuildFissurePlan(player.Position(), predicted, target.BoundingRadius());
    if (!plan.Valid || !FissureHits(plan.Origin, plan.Aim, predicted, target.BoundingRadius()) ||
        ProjectileWallBlocksFromPlayer(plan.Aim, kRWidth * 0.5f)) return false;
    const int enemies = Engine::CountEnemiesAt(predicted, kRKnockupRadius);
    const int allies = Engine::CountAlliesAt(predicted, 700.0f) + 1;
    const bool defensive = reactive || mode == Mode::Flee || mode == Mode::Automatic;
    if (!FissureSafe(predicted, enemies, allies,
                     Engine::UnderEnemyTurret(predicted) &&
                         !Engine::UnderEnemyTurret(player.Position()),
                     SDK::NavMesh::IsWall(predicted), defensive,
                     Slider(TacticsMenu, "MaximumEnemies", 3))) return false;
    if (!defensive && enemies < Slider(TacticsMenu, "MinimumRTargets", 2) &&
        !Lethal(target, player.CalculateMagicDamage(target,
            RDamage(SpellRank(3), player.AP())))) return false;
    const auto ally = RescueAlly();
    if (ally.IsValid() && !AllySafety(ally.HealthPercent(),
            Engine::CountEnemiesAt(ally.Position(), 500.0f),
            Engine::CountAlliesAt(ally.Position(), 650.0f),
            defensive || IncomingThreatUntil > Now(),
            Engine::UnderEnemyTurret(ally.Position()))) return false;
    if (!Engine::ControllerCastPosition(3, plan.Aim)) return false;
    LastRTargetId = static_cast<int>(target.NetworkId());
    FissureActiveUntil = Now() + 4200;
    LastCastTick[3] = Now();
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    const auto ally = RescueAlly();
    if (Engine::ValidAlly(ally) && (ally.HealthPercent() < 62.0f ||
        IncomingThreatUntil > Now()) && CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    if (mode == Mode::Jungle && Engine::ValidEnemy(target))
        if (CastQ(target, mode)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (CastW(target, Mode::Flee, true)) return;
    if (CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target) && CastQ(target, Mode::Flee, true)) return;
    (void)CastR(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto ally = RescueAlly();
    if (Engine::ValidAlly(ally) && (ally.HealthPercent() < Slider(TacticsMenu, "AllyHealth", 62) ||
        IncomingThreatUntil > Now()) && CastW(target, Mode::Automatic, true)) return;
    if ((IncomingThreatUntil > Now() || GapcloserExpireTick > Now()) &&
        CastE(target, Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(target) && CastQ(target, Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastR(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const int now = Now();
    if (IncomingThreatUntil <= now) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
        IncomingThreatOrigin = IncomingThreatEndpoint = {};
    }
    if (ShieldActiveUntil <= now) ShieldActiveUntil = 0;
    if (FissureActiveUntil <= now) FissureActiveUntil = 0;
    for (auto& state : PassiveTargets)
        if (state.ExpireTick <= now) state = {};
    const auto player = GameObjects::Player();
    if (player.IsValid()) {
        if (player.HasBuff("BraumE") || player.HasBuff("BraumEShield"))
            ShieldActiveUntil = std::max(ShieldActiveUntil, now + 300);
        if (player.HasBuff("BraumR"))
            FissureActiveUntil = std::max(FissureActiveUntil, now + 400);
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now() || ShieldActiveUntil > Now() || FissureActiveUntil > Now()) return true;
    const AIHeroClient target = PreferredEnemyTarget(selected, kRRange + 100.0f);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode, target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (args.Slot >= 0 && args.Slot < 4) {
            if (!Engine::WasControllerCast(args.Slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 650);
            LastCastTick[static_cast<std::size_t>(args.Slot)] = now;
            if (args.Slot == 2) ShieldActiveUntil = now + 3200;
            if (args.Slot == 3) FissureActiveUntil = now + 4200;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatTargetId = static_cast<int>(args.TargetNetworkId != 0 ?
        args.TargetNetworkId : args.Target.NetworkId);
    IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
    IncomingThreatOrigin = args.StartPosition;
    IncomingThreatEndpoint = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
        ? args.EndPosition : args.CastPosition;
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
        LastAutoTick = Now();
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == 0) return;
    if (Engine::TextContains(args.BuffName, "braummark") ||
        Engine::TextContains(args.BuffName, "braumpassive")) {
        SetPassiveStacks(id, args.Count > 0 ? args.Count : PassiveStacks(id) + 1,
                         static_cast<int>(kPassiveDuration * 1000.0f));
    }
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "braume"))
        ShieldActiveUntil = std::max(ShieldActiveUntil, Now() + 300);
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (Engine::TextContains(args.BuffName, "braummark") ||
        Engine::TextContains(args.BuffName, "braumpassive")) SetPassiveStacks(id, 0, 0);
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "braume"))
        ShieldActiveUntil = 0;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
        LastAutoTick = Now();
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, IncomingThreatEndpoint,
                           GapcloserExpireTick, kERange, 1000);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptExpireTick, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Braum Concussive Blows tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Braum farming posture"));
    TacticsMenu->Add(new MenuSlider("AllyHealth", "Automatic ally rescue health", 62, 10, 95));
    TacticsMenu->Add(new MenuSlider("DefensiveHealth", "Defensive mobility health", 48, 10, 90));
    TacticsMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at W/R destination", 3, 0, 6));
    TacticsMenu->Add(new MenuSlider("MinimumRTargets", "Minimum R knock-up targets", 2, 1, 5));
    TacticsMenu->Add(new MenuSlider("QHealth", "Minimum Q poke health threshold", 82, 10, 100));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana floor", 56, 0, 95));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast protection (ms)", 650, 0, 2000));
    FarmMenu->Add(new MenuSlider("LaneMana", "Lane-clear mana floor", 30, 0, 95));
    FarmMenu->Add(new MenuSlider("JungleMana", "Jungle mana floor", 24, 0, 95));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    PassiveTargets.fill({});
    LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = 0;
    IncomingThreatUntil = IncomingThreatTargetId = ShieldActiveUntil = 0;
    LastQTargetId = LastRTargetId = FissureActiveUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    IncomingThreatOrigin = IncomingThreatEndpoint = {};
}

inline void OnUnload() {
    TacticsMenu = FarmMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Riot 26.15 and CommunityDragon PC 16.15 Braum metadata and four-hit passive cap",
    "Track Concussive Blows stacks independently per target with event and polling expiry",
    "Prefer selected enemy, then orbwalker target, then engine target for every offensive route",
    "Predict Winter's Bite, include target radius, projectile collision and projectile-wall rejection",
    "Use Q marks and attack windup ownership without spending mana on low-value poke",
    "Stand Behind Me dashes only to a worthwhile ally with resist and turret/enemy-count safety",
    "Unbreakable intercepts analyzed incoming lines and hard threats while preserving mobility safety",
    "Glacial Fissure predicts the selected target and counts enemies inside the knock-up radius",
    "Reject fissure mobility and ally rescue when walls, turrets or enemy density make commitment unsafe",
    "Apply mana, cooldown, damage and shield-worthwhile gates before every controller cast",
    "Combo stacks passive and protects the carry before Q, E and multi-target R commitment",
    "Harass spends Q only above the configured mana floor and avoids empty defensive casts",
    "LaneClear, Jungle and LastHit preserve resources and delegate farming to the shared engine",
    "Flee dashes to a safe ally, raises Unbreakable against threats, peels with Q and fissures pursuers",
    "Automatic mode reconciles ally health, incoming projectiles, gapclosers and interruptible threats",
    "Manual spell events yield ownership briefly while polling reconciles shield, fissure and passive state",
    "Expose complete load, menu, update, draw, spell, buff, attack, gapcloser and interrupt callbacks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Braum;
    controller.ControllerId = "champion.kuroaio.ai.braum.vanguard";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIBraum.md";
    controller.ImplementationSummary =
        "Owns per-target Concussive Blows stacks, predictive Winter's Bite, safe Stand Behind Me "
        "dash, projectile-intercepting Unbreakable and ally-safe Glacial Fissure decisions.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Braum
