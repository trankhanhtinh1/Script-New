#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "../Profiles/AISejuani.h"
#include "AISejuaniGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Sejuani {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::Now;
using ControllerHelpers::OrbwalkerHeroTarget;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Protected;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;
inline std::array<int, 32> EIds{};
inline std::array<int, 32> EStacks{};
inline std::array<int, 32> EExpiry{};
inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastDamageTick = 0;
inline int FrostBrokenTick = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int GapcloserTargetId = 0;
inline Vector3 GapcloserEndpoint{};
inline int GapcloserUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptUntil = 0;
inline int RMissileId = 0;
inline int RCastTick = 0;
inline bool RInFlight = false;
inline bool RControllerOwned = false;

inline int EIndex(int id, bool create = true) {
    if (id == 0) return -1;
    for (std::size_t i = 0; i < EIds.size(); ++i)
        if (EIds[i] == id) return static_cast<int>(i);
    if (!create) return -1;
    for (std::size_t i = 0; i < EIds.size(); ++i) {
        if (EIds[i] == 0) { EIds[i] = id; return static_cast<int>(i); }
    }
    return -1;
}
inline int EStackCount(const AIHeroClient& target) {
    if (!target.IsValid()) return 0;
    const int id = static_cast<int>(target.NetworkId());
    const int index = EIndex(id);
    const int observed = ControllerHelpers::MaximumBuffCount(target,
        {"SejuaniEPassive", "SejuaniEMarker", "SejuaniEMarkerMax", "sejuanifrost"});
    if (observed > 0) {
        if (index >= 0) { EStacks[static_cast<std::size_t>(index)] = std::clamp(observed, 0, kEMaxStacks); EExpiry[static_cast<std::size_t>(index)] = Now() + kEStackDurationMs; }
        return std::clamp(observed, 0, kEMaxStacks);
    }
    if (index >= 0 && EExpiry[static_cast<std::size_t>(index)] >= Now())
        return EStacks[static_cast<std::size_t>(index)];
    if (index >= 0) { EStacks[static_cast<std::size_t>(index)] = 0; EExpiry[static_cast<std::size_t>(index)] = 0; }
    return 0;
}
inline void ObserveEStack(const AIHeroClient& target) {
    const int index = EIndex(static_cast<int>(target.NetworkId()));
    if (index < 0) return;
    EStacks[static_cast<std::size_t>(index)] = AdvanceEStacks(EStacks[static_cast<std::size_t>(index)]);
    EExpiry[static_cast<std::size_t>(index)] = Now() + kEStackDurationMs;
}
inline void ClearEStack(int id) {
    const int index = EIndex(id, false);
    if (index >= 0) { EStacks[static_cast<std::size_t>(index)] = 0; EExpiry[static_cast<std::size_t>(index)] = 0; }
}
inline bool ReadyFor(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && (reactive || SpellEnabled(slot, mode)) &&
        ControllerHelpers::HasCurrentResource(ControllerHelpers::SpellCost(slot)) &&
        (reactive || Now() - LastCastTick[static_cast<std::size_t>(slot)] >= 45);
}
inline bool CanAct(bool reactive) {
    const auto player = GameObjects::Player();
    return player.IsValid() && !Engine::IsPlayerCrowdControlled(player) &&
        (reactive || !ControllerHelpers::PreserveAttack(false));
}
inline bool PassiveActive() {
    const auto player = GameObjects::Player();
    return player.IsValid() && FrostArmorActive(Now(), LastDamageTick,
        player.Level(), FrostBrokenTick);
}
inline float QDamage(const AIHeroClient& target) {
    const auto p = GameObjects::Player();
    return p.IsValid() && Engine::ValidEnemy(target) ? p.CalculateMagicDamage(target,
        QRawDamage(SpellRank(0), p.AP())) : 0.0f;
}
inline float WDamage(const AIHeroClient& target, bool second = true) {
    const auto p = GameObjects::Player();
    return p.IsValid() && Engine::ValidEnemy(target) ? p.CalculatePhysicalDamage(target,
        second ? WSecondRawDamage(SpellRank(1), p.AP(), p.MaxHealth()) :
            WFirstRawDamage(SpellRank(1), p.AP(), p.MaxHealth())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto p = GameObjects::Player();
    return p.IsValid() && Engine::ValidEnemy(target) ? p.CalculateMagicDamage(target,
        ERawDamage(SpellRank(2), p.AP())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target, bool empowered = false) {
    const auto p = GameObjects::Player();
    return p.IsValid() && Engine::ValidEnemy(target) ? p.CalculateMagicDamage(target,
        RRawDamage(SpellRank(3), p.AP(), empowered)) : 0.0f;
}
inline bool SafeEndpoint(const Vector3& endpoint, bool reactive, bool lethal) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || endpoint.IsZero() || !endpoint.IsValid() || SDK::NavMesh::IsWall(endpoint)) return false;
    const bool startedUnderTurret = Engine::UnderEnemyTurret(p.Position());
    const bool endpointUnderTurret = Engine::UnderEnemyTurret(endpoint);
    return SafeDashEndpoint(p.Position(), endpoint, !SDK::NavMesh::IsWallBetween(p.Position(), endpoint),
        true, startedUnderTurret, endpointUnderTurret,
        Engine::CountEnemiesAt(endpoint, 250.0f), Slider(QMenu, "MaxEndpointEnemies", 2),
        lethal, reactive || p.HealthPercent() <= Slider(TacticsMenu, "DefensiveHealth", 35));
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || !Engine::ValidEnemy(target, kQRange + target.BoundingRadius()) ||
        !ReadyFor(0, mode, reactive) || !CanAct(reactive) || Protected(target)) return false;
    const Vector3 aim = PredictPosition(target, kQDelay);
    const Vector3 endpoint = DashEndpoint(p.Position(), aim);
    const bool lethal = Lethal(target, QDamage(target));
    if (!SafeEndpoint(endpoint, reactive, lethal) ||
        (!reactive && Orbwalker::IsWindingUp() && Bool(TacticsMenu, "PreserveAttacks", true))) return false;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    LastCastTick[0] = Now();
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || !Engine::ValidEnemy(target, kWRange + 50.0f) ||
        !ReadyFor(1, mode, reactive) || !CanAct(reactive) || Protected(target)) return false;
    const Vector3 aim = PredictPosition(target, kWSecondDelay);
    if (!WFirstHit(p.Position(), aim, target.Position(), target.BoundingRadius()) ||
        ProjectileWallBlocksFromPlayer(aim, kWHalfWidth) ||
        (!reactive && Orbwalker::IsWindingUp() && Bool(TacticsMenu, "PreserveAttacks", true))) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    ObserveEStack(target);
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || !Engine::ValidEnemy(target, kERange) ||
        !ReadyFor(2, mode, reactive) || !CanAct(reactive) || Protected(target) ||
        EStackCount(target) < kEMaxStacks) return false;
    if (Engine::CountEnemiesAt(target.Position(), 250.0f) > Slider(EMenu, "MaxStunEnemies", 3) && !reactive) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastCastTick[2] = Now();
    ClearEStack(static_cast<int>(target.NetworkId()));
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false, bool manual = false) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || !Engine::ValidEnemy(target, kRRange) || !ReadyFor(3, mode, reactive) ||
        !CanAct(reactive) || Protected(target)) return false;
    const Vector3 aim = PredictPosition(target, 0.45f);
    if (!aim.IsValid() || aim.IsZero() || p.Position().Distance2D(aim) > kRRange + target.BoundingRadius() ||
        ProjectileWallBlocksFromPlayer(aim, kRWidth * 0.5f)) return false;
    const auto prediction = Engine::RuntimeSpells[3]->GetPrediction(target);
    if (!prediction.GetCastPosition().IsValid() || !prediction.CollisionObjects.empty()) return false;
    int hits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes())
        if (Engine::ValidEnemy(enemy) && SegmentHits(p.Position(), aim,
            PredictPosition(enemy, 0.45f), kRWidth * 0.5f, enemy.BoundingRadius())) ++hits;
    const bool lethal = Lethal(target, RDamage(target, false));
    const bool defensive = reactive || IncomingThreatUntil >= Now() ||
        p.HealthPercent() <= Slider(TacticsMenu, "DefensiveHealth", 35);
    if (!ShouldCastR(true, true, true, false, lethal, defensive, manual,
        hits, Slider(RMenu, "MinimumTargets", 2))) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastCastTick[3] = RCastTick = Now();
    RInFlight = true; RControllerOwned = true;
    return true;
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || p.ManaPercent() < Slider(WMenu, "HarassMana", 50)) return;
    if (CastW(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}
inline void Farm(Mode mode) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || p.ManaPercent() < Slider(FarmMenu, "Mana", 35)) return;
    if (mode == Mode::Jungle) {
        const auto monster = SelectJungleTarget(kQRange, 0.20f, 100000.0f);
        if (monster.IsValid() && !monster.IsDead() && monster.IsTargetable() &&
            SafeObjectiveCommit(IsEpicMonster(monster), Engine::CountEnemiesAt(monster.Position(), 900.0f) > 0,
                monster.Health() <= QRawDamage(SpellRank(0), p.AP()), p.HealthPercent() > 40.0f, p.HealthPercent() < 25.0f) &&
            ReadyFor(0, mode) && Engine::ControllerCastUnit(0, AIBaseClient(monster.Handle()))) {
            LastCastTick[0] = Now(); return;
        }
    }
    (void)Engine::TryFarm(mode);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, kERange) && CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target, kQRange) && CastQ(target, Mode::Flee, true)) return;
    (void)CastR(target, Mode::Flee, true, true);
}
inline void Automatic(const AIHeroClient& target) {
    if (GapcloserTargetId && GapcloserUntil >= Now()) {
        const auto threat = Engine::EnemyByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(threat, kERange) && CastE(threat, Mode::Automatic, true)) return;
    }
    if (InterruptTargetId && InterruptUntil >= Now()) {
        const auto threat = Engine::EnemyByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(threat, kRRange) && CastR(threat, Mode::Automatic, true)) return;
    }
    if (Engine::ValidEnemy(target, kRRange)) (void)CastR(target, Mode::Automatic, true);
}
inline void ReconcileState() {
    const auto p = GameObjects::Player();
    const int now = Now();
    for (const auto& enemy : GameObjects::EnemyHeroes()) (void)EStackCount(enemy);
    if (p.IsValid() && (p.HasBuff("SejuaniPassive") || p.HasBuff("SejuaniPassiveDefense")))
        FrostBrokenTick = std::max(FrostBrokenTick, now);
    if (RInFlight && now - RCastTick > 2500) { RInFlight = false; RMissileId = 0; }
    if (GapcloserUntil < now) GapcloserTargetId = 0;
    if (InterruptUntil < now) InterruptTargetId = 0;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    AIHeroClient target = PreferredEnemyTarget(selected, mode == Mode::Flee ? 900.0f : kRRange);
    if (!target.IsValid()) target = OrbwalkerHeroTarget(kRRange);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (!Engine::WasControllerCast(slot)) ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatUntil = std::max(IncomingThreatUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
        if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(IncomingHardCCUntil, IncomingThreatUntil);
    }
    if (analysis.Valid && analysis.TargetsPlayer) LastDamageTick = now;
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender) && (Engine::TextContains(args.BuffName, "SejuaniPassive") || Engine::TextContains(args.BuffName, "FrostArmor"))) FrostBrokenTick = Now();
    if (Engine::TextContains(args.BuffName, "SejuaniEPassive") || Engine::TextContains(args.BuffName, "SejuaniEMarker")) {
        const auto target = Engine::EnemyByNetworkId(id);
        if (Engine::ValidEnemy(target)) ObserveEStack(target);
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (Engine::TextContains(args.BuffName, "SejuaniEPassive") || Engine::TextContains(args.BuffName, "SejuaniEMarker")) ClearEStack(id);
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "SejuaniPassive")) LastDamageTick = Now();
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (Engine::TextContains(args.Sender.Name, "SejuaniRMissile") ||
        Engine::TextContains(args.Sender.CharacterName, "SejuaniRMissile")) {
        RMissileId = static_cast<int>(args.Sender.NetworkId);
        RInFlight = true;
    }
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (id == RMissileId) {
        RMissileId = 0;
        RInFlight = false;
    }
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto p = GameObjects::Player();
    if (!p.IsValid()) return;
    Drawing::DrawCircle(p.Position(), kQRange, 0xFF66CCFFu, 1.5f, 40);
    Drawing::DrawCircle(p.Position(), kRRange, 0xFFFFAA44u, 1.5f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("SejuaniOneTrick", "Sejuani frost tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("DefensiveHealth", "Defensive health percent", 35, 10, 80));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve AA windup", true));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Arctic Assault"));
    QMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 1, 5));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Winter's Wrath"));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 50, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Permafrost"));
    EMenu->Add(new MenuSlider("MaxStunEnemies", "Maximum enemies near stun", 3, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Glacial Prison"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum predicted targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("SejuaniFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("SejuaniCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}
inline void OnLoad() {
    EIds.fill(0); EStacks.fill(0); EExpiry.fill(0); LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = LastDamageTick = FrostBrokenTick = ManualOwnershipUntil = 0;
    IncomingThreatUntil = IncomingHardCCUntil = GapcloserTargetId = GapcloserUntil = 0;
    InterruptTargetId = InterruptUntil = RMissileId = RCastTick = 0; RInFlight = RControllerOwned = false;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    EIds.fill(0); EStacks.fill(0); EExpiry.fill(0); RInFlight = false;
}
inline constexpr const char* Scenarios[] = {
    "Pin passive and spell values to Riot live 26.15 and CommunityDragon 16.15",
    "Reconcile Frost Armor readiness and its three-second post-damage persistence from events and polling",
    "Use passive bonus armor and magic resistance as a defensive resource, never as permission to turret dive",
    "Use Q Arctic Assault as a 625-unit, 1000-speed dash with sampled terrain and endpoint safety gates",
    "Reject Q endpoints through walls, unsafe turrets, excessive enemies or uncertain walkability",
    "Preserve selected target before orbwalker target and selector fallback",
    "Predict W first and second flail phases and keep both hit windows distinct",
    "Track Permafrost marks per target with four-stack and five-second expiry reconciliation",
    "Cast E stun only on observed four-stack targets and clear the consumed mark state",
    "Respect AA windup and yield after manual Q W E or R ownership",
    "Predict R projectile collision, 1300 reach, 120 width and 1600 speed before casting",
    "Model R frost explosion, two-second zone and empowered 1.5-second stun outcome",
    "Reserve nonlethal R for configured multi-target value while allowing lethal or defensive casts",
    "Reject R projectile wall blocks and protected, invulnerable or spell-shielded targets",
    "Keep objective Q secure behind epic-monster threat, player-health and ally-presence safety checks",
    "Use Q and E peel only when an ally threat is reachable without unsafe displacement",
    "Reconcile enemy gapcloser and interrupt threats from process-spell events",
    "Require observed runtime readiness, mana reserve, cooldown throttle and player action ownership",
    "Combo sequences Q, two-phase W, four-stack E and then Glacial Prison",
    "Harass spends W/E only above the configured mana reserve and never initiates R",
    "LaneClear Jungle and LastHit delegate to shared farm policy after objective safety checks",
    "Flee prioritizes Permafrost peel, safe Q exit and manual-assist R",
    "Automatic mode only answers defense, interrupt or kill-secure opportunities, never fresh engage",
    "Never automate items, summoner spells or unrelated movement ownership",
    "Draw ranges and passive state without changing gameplay decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Sejuani";
    controller.ControllerId = "champion.kuroaio.ai.sejuani.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISejuani.md";
    controller.ImplementationSummary = "Frost Armor timing, terrain-safe Arctic Assault, two-phase flail marks, four-stack Permafrost and collision-safe Glacial Prison.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser =
        &ControllerHelpers::CaptureGapcloserEvent<
            &GapcloserTargetId, &GapcloserEndpoint, &GapcloserUntil, 600, 900>;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptUntil, 1300, 250, 4000>;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Sejuani
