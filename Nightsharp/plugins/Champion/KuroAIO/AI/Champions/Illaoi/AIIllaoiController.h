#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIIllaoiGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Illaoi {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureInterruptableEvent;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Now;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Protected;
using ControllerHelpers::Ready;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline bool WActive = false;
inline bool RActive = false;
inline int RStartTick = 0;
inline int WStartTick = 0;
inline int SpiritTargetId = 0;
inline int SpiritExpireTick = 0;
inline bool SpiritVessel = false;
inline std::array<TentacleState, 32> Tentacles{};
inline Mode LastMode = Mode::None;

inline bool Throttle(int slot, int delay = 70) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool ManaAffordable(int slot, float reservePercent = 0.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float cost = ControllerHelpers::SpellCost(slot);
    return player.Mana() >= cost && player.ManaPercent() >= reservePercent;
}
inline bool LethalWithSlot(const AIHeroClient& target, int slot) {
    return Engine::ValidEnemy(target) && slot >= 0 && slot < 4 &&
        Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->GetDamage(target) >=
            target.Health() + target.AllShield();
}
inline bool NameIsTentacle(const SDK::Events::ObjectEventArgs& args) {
    return args.Sender.IsValid() && ControllerHelpers::AnyTextContains(
        {args.Sender.Name, args.Sender.CharacterName},
        {"IllaoiTentacle", "illaoi_tentacle", "Tentacle"});
}
inline bool NameIsSpirit(const char* value) {
    return ControllerHelpers::TextContainsAny(value,
        {"IllaoiEVessel", "IllaoiESpirit", "IllaoiEStatue", "illaoi_evessel"});
}
inline int FindTentacle(int id) {
    for (std::size_t i = 0; i < Tentacles.size(); ++i)
        if (Tentacles[i].Id == id) return static_cast<int>(i);
    return -1;
}
inline void ObserveTentacleObject(int id, const Vec3& position, bool fromR) {
    if (id == 0 || !position.IsValid()) return;
    const int now = Now();
    int index = FindTentacle(id);
    if (index < 0) {
        for (std::size_t i = 0; i < Tentacles.size(); ++i) {
            if (Tentacles[i].Id == 0 || (!Tentacles[i].Alive &&
                now - Tentacles[i].SpawnTick > 32000)) {
                index = static_cast<int>(i);
                break;
            }
        }
    }
    if (index >= 0) Tentacles[static_cast<std::size_t>(index)] =
        ObserveTentacle(Tentacles[static_cast<std::size_t>(index)], id,
                        position, now, fromR);
}
inline void DisableTentacleObject(int id) {
    const int index = FindTentacle(id);
    if (index >= 0) Tentacles[static_cast<std::size_t>(index)] =
        DisableTentacle(Tentacles[static_cast<std::size_t>(index)], Now());
}
inline bool HaveUsableTentacleNear(const Vec3& point, float range) {
    const int now = Now();
    for (const auto& tentacle : Tentacles) {
        if (TentacleUsable(tentacle, now) && tentacle.Position.Distance2D(point) <= range)
            return true;
    }
    return false;
}
inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool runtimeW = player.HasBuff("IllaoiW") ||
        ControllerHelpers::RuntimeNameContains(1, "IllaoiW");
    const bool runtimeR = player.HasBuff("IllaoiR") ||
        ControllerHelpers::RuntimeNameContains(3, "IllaoiR");
    if (runtimeW) WActive = true;
    else if (WActive && now - WStartTick > 900) WActive = false;
    if (runtimeR) RActive = true;
    else if (RActive && now - RStartTick > 1100) RActive = false;
    if (!SpiritActive(now, SpiritTargetId, SpiritExpireTick)) {
        SpiritTargetId = 0;
        SpiritExpireTick = 0;
        SpiritVessel = false;
    }
    for (auto& tentacle : Tentacles) {
        if (tentacle.Id == 0) continue;
        if (tentacle.DisabledUntil != 0 && now >= tentacle.DisabledUntil) tentacle = {};
        if (tentacle.Alive && now - tentacle.SpawnTick > 120000) tentacle = {};
    }
    if (IncomingThreatUntil <= now) IncomingThreatUntil = 0;
    if (IncomingHardCcUntil <= now) IncomingHardCcUntil = 0;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 60.0f) ||
        !Ready(0, mode) || !Throttle(0) || !ManaAffordable(0, 20.0f) ||
        Protected(target) || PreserveAttack(reactive)) return false;
    auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    Vec3 aim = prediction.GetCastPosition().IsValid() ? prediction.GetCastPosition() :
        PredictPosition(target, kQDelay);
    const bool high = prediction.Hitchance >= SDK::HitChance::High;
    if (!aim.IsValid() || !QPathHits(player.Position(), aim, PredictPosition(target, kQDelay),
                                      target.BoundingRadius()) ||
        (!high && !reactive) || SDK::NavMesh::IsWallBetween(player.Position(), aim, kQHalfWidth) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQHalfWidth)) return false;
    const bool turretRisk = Engine::UnderEnemyTurret(aim) &&
        !Engine::UnderEnemyTurret(player.Position());
    const bool lethal = LethalWithSlot(target, 0);
    if (turretRisk && !reactive && !lethal) return false;
    if (!Engine::ControllerCastPosition(0, ClampQEndpoint(player.Position(), aim))) return false;
    LastCastTick[0] = Now();
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange + 50.0f) ||
        !WInRange(player.Position(), PredictPosition(target, 0.10f), target.BoundingRadius()) ||
        !Ready(1, mode) || !Throttle(1) || !ManaAffordable(1, 20.0f) || Protected(target))
        return false;
    const bool lethal = LethalWithSlot(target, 1);
    const int nearby = Engine::CountEnemiesAt(target.Position(), kRRadius);
    if (!SafeCommit(target.Position().IsValid() ? player.HealthPercent() : 0.0f, nearby,
                    Engine::UnderEnemyTurret(target.Position()),
                    Engine::UnderEnemyTurret(player.Position()), lethal, reactive,
                    static_cast<float>(Slider(WMenu, "MinimumHealth", 24)), Slider(WMenu, "MaximumEnemies", 3))) return false;
    if (PreserveAttack(reactive, lethal)) return false;
    const Vec3 aim = PredictPosition(target, 0.10f);
    if (!aim.IsValid() || !Engine::ControllerCastPosition(1, aim)) return false;
    WActive = true;
    WStartTick = Now();
    LastCastTick[1] = Now();
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange + 60.0f) ||
        !Ready(2, mode) || !Throttle(2) || !ManaAffordable(2, 25.0f) ||
        Protected(target) || PreserveAttack(reactive)) return false;
    auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    Vec3 aim = prediction.GetCastPosition().IsValid() ? prediction.GetCastPosition() :
        PredictPosition(target, kEDelay + player.Position().Distance2D(target.Position()) / kESpeed);
    const bool high = prediction.Hitchance >= SDK::HitChance::High;
    if (!aim.IsValid() || !Reachable(player.Position(), aim, kERange, target.BoundingRadius()) ||
        !ELineHits(player.Position(), aim, PredictPosition(target, kEDelay),
                   target.BoundingRadius()) || (!high && !reactive) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kEHalfWidth) ||
        SDK::NavMesh::IsWallBetween(player.Position(), aim, kEHalfWidth)) return false;
    const bool lethal = LethalWithSlot(target, 2);
    if (Engine::UnderEnemyTurret(target.Position()) &&
        !Engine::UnderEnemyTurret(player.Position()) && !reactive && !lethal) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    SpiritTargetId = static_cast<int>(target.NetworkId());
    SpiritExpireTick = Now() + static_cast<int>(kESpiritSeconds * 1000.0f);
    SpiritVessel = true;
    LastCastTick[2] = Now();
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 180) ||
        !ManaAffordable(3, 0.0f) || PreserveAttack(reactive)) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const bool lethal = Engine::ValidEnemy(target) && LethalWithSlot(target, 3);
    const bool defensive = reactive || player.HealthPercent() <=
        Slider(RMenu, "DefensiveHealth", 42);
    if (!SafeUltimate(player.HealthPercent(), enemies,
                      Engine::UnderEnemyTurret(player.Position()), lethal,
                      defensive, static_cast<float>(Slider(RMenu, "DefensiveHealth", 42)))) return false;
    if (!Engine::UnderEnemyTurret(player.Position()) && enemies >
        Slider(RMenu, "MaximumEnemies", 3) && !defensive && !lethal) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RActive = true;
    RStartTick = Now();
    LastCastTick[3] = Now();
    return true;
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (SpiritVessel && SpiritTargetId == static_cast<int>(target.NetworkId()) &&
        CastQ(target, Mode::Combo)) return;
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Combo)) return;
    if (CastR(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    (void)CastW(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (SpiritVessel && CastQ(target, Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}
inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "MinimumMana", 30)) return;
    if (Ready(0, mode) && ManaAffordable(0) && Engine::TryFarm(mode)) LastCastTick[0] = Now();
    if (Ready(1, mode) && ManaAffordable(1) && Engine::TryFarm(mode)) LastCastTick[1] = Now();
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastQ(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastE(target, Mode::Flee, true);
}
inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    LastMode = mode;
    ReconcileState();
    const float range = mode == Mode::Flee ? 1200.0f : kERange;
    const auto target = Engine::SelectTarget(range);
    const auto player = GameObjects::Player();
    if (player.IsValid() && IncomingHardCcUntil > Now() && Engine::ValidEnemy(target) &&
        CastQ(target, mode, true)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, range)); break;
    case Mode::Automatic:
        if (player.IsValid() && CastR(target, mode, player.HealthPercent() <= 30.0f)) return true;
        if (Engine::ValidEnemy(target)) Combo(target);
        break;
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = args.Slot;
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = now;
            if (slot == 1) { WActive = true; WStartTick = now; }
            if (slot == 2) {
                SpiritTargetId = args.TargetNetworkId != 0 ? static_cast<int>(args.TargetNetworkId) :
                    static_cast<int>(args.Target.NetworkId);
                SpiritExpireTick = now + static_cast<int>(kESpiritSeconds * 1000.0f);
                SpiritVessel = SpiritTargetId != 0;
            }
            if (slot == 3) { RActive = true; RStartTick = now; }
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCcUntil = std::max(
        IncomingHardCcUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    (void)CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
    if (IsLocalPlayer(args.Sender) && args.Slot == 0) {
        for (auto& tentacle : Tentacles)
            if (TentacleUsable(tentacle, Now())) tentacle.LastSlamTick = Now();
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (NameIsSpirit(args.BuffName)) {
        SpiritTargetId = static_cast<int>(args.Sender.NetworkId);
        SpiritExpireTick = Now() + static_cast<int>(kESpiritSeconds * 1000.0f);
        SpiritVessel = true;
    }
    if (!IsLocalPlayer(args.Sender)) return;
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"IllaoiW"})) WActive = true;
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"IllaoiR"})) RActive = true;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (NameIsSpirit(args.BuffName) && static_cast<int>(args.Sender.NetworkId) == SpiritTargetId) {
        SpiritTargetId = 0; SpiritExpireTick = 0; SpiritVessel = false;
    }
    if (!IsLocalPlayer(args.Sender)) return;
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"IllaoiW"})) WActive = false;
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"IllaoiR"})) RActive = false;
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!NameIsTentacle(args)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.Position().Distance2D(args.Sender.Position) >
        kPassiveSpawnRadius + 100.0f) return;
    ObserveTentacleObject(static_cast<int>(args.Sender.NetworkId), args.Sender.Position, RActive);
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (NameIsTentacle(args)) DisableTentacleObject(static_cast<int>(args.Sender.NetworkId));
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (NameIsSpirit(args.SpellName) || NameIsSpirit(args.MissileName)) {
        if (args.TargetNetworkId != 0) SpiritTargetId = static_cast<int>(args.TargetNetworkId);
        SpiritExpireTick = Now() + static_cast<int>(kESpiritSeconds * 1000.0f);
        SpiritVessel = SpiritTargetId != 0;
    }
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)args;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF66CC88u, 1.2f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFFCC8844u, 1.0f, 40);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFDD4466u, 1.4f, 40);
    for (const auto& tentacle : Tentacles)
        if (TentacleUsable(tentacle, Now())) Drawing::DrawCircle(
            tentacle.Position, 95.0f, 0xFFAA66DDu, 1.0f, 24);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("IllaoiOneTrick", "Illaoi tentacle tactics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Tentacle Smash"));
    QMenu->Add(new MenuBool("RequireHighHitChance", "Require high prediction", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Harsh Lesson leap/reset"));
    WMenu->Add(new MenuSlider("MinimumHealth", "Minimum commit health percent", 24, 10, 80));
    WMenu->Add(new MenuSlider("MaximumEnemies", "Maximum nearby enemies", 3, 1, 5));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Test of Spirit"));
    EMenu->Add(new MenuBool("RequireHighHitChance", "Require high prediction", true));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Leap of Faith"));
    RMenu->Add(new MenuSlider("DefensiveHealth", "Defensive health percent", 42, 15, 75));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies for ordinary R", 3, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Q/W farm policy"));
    FarmMenu->Add(new MenuSlider("MinimumMana", "Minimum mana percent", 30, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw ranges and tentacles", false));
}
inline void OnLoad() {
    LastCastTick.fill(0); LastAutoTargetId = LastAutoTick = 0;
    IncomingThreatUntil = IncomingHardCcUntil = 0; WActive = false;
    RActive = false; RStartTick = WStartTick = 0;
    SpiritTargetId = SpiritExpireTick = 0; SpiritVessel = false;
    Tentacles.fill({}); LastMode = Mode::None;
}
inline void OnUnload() { OnLoad(); }

inline constexpr const char* Scenarios[] = {
    "Track passive tentacle create/delete lifecycle and thirty-second disabled lifetime",
    "Poll tentacle records and reconcile missed lifecycle events without stale objects",
    "Predict Q Tentacle Smash slam with 825 reach, 105 width and 0.75-second windup",
    "Reject Q through projectile or navmesh walls and turret-only endpoints",
    "Preserve ordinary attack windup while allowing reactive Q peel",
    "Use W Harsh Lesson as a reachable leap and attack reset",
    "Gate W on mana, health, turret exposure and nearby-enemy density",
    "Track E Test of Spirit vessel target and seven-second spirit window from events and polling",
    "Reject E when prediction, reach, collision, projectile wall or spell protection fails",
    "Apply E spirit leash, vessel duration, slow and damage-transfer state",
    "Spawn and reconcile R tentacles from object events and eight-second field state",
    "Use R only for low-health, lethal or multi-target commitments with turret safety",
    "Respect mana costs, cooldown throttles, autonomous target selection and orbwalker fallback",
    "Capture enemy threat and hard crowd-control windows through process-spell callbacks",
    "Support Combo Harass LaneClear Jungle LastHit Flee and Automatic modes distinctly",
    "Keep Q/W/E/R damage, reach, spawn count and lifecycle boundaries in standalone geometry",
    "Complete ChampionController callback ABI without modifying shared registration files",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Illaoi;
    controller.ControllerId = "champion.kuroaio.ai.illaoi.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIIllaoi.md";
    controller.ImplementationSummary =
        "Tentacle lifecycle tracking, predicted Q slam, W leap/reset, E spirit vessel"
        " reconciliation and low-health/turret-safe R tentacle spawning.";
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
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Illaoi
