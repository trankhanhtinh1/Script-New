#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIRenektonGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Renekton {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline float Fury = 0.0f;
inline EStage ECastStage = EStage::Slice;
inline CooldownState Cooldowns{};
inline int ArmorShredTargetId = 0;
inline int ArmorShredExpireTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
inline int Rank(int slot) { return ControllerHelpers::SpellRank(slot); }
inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && ControllerHelpers::Ready(slot) &&
           (reactive || SpellEnabled(slot, mode));
}
inline bool Throttle(int slot, int delay = 55) {
    return slot >= 0 && slot < 4 && Now() >= Cooldowns.ReadyAt[static_cast<std::size_t>(slot)] &&
           (Engine::LastActionTick <= 0 || Now() - Engine::LastActionTick >= delay);
}
inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target);
}
using ControllerHelpers::PreserveAttack;
inline bool InRange(const AIHeroClient& target, float range) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target, range + target.BoundingRadius()) &&
           player.Position().Distance2D(target.Position()) <= range + target.BoundingRadius();
}
inline float BonusAD() {
    const auto player = GameObjects::Player();
    return player.IsValid() ? player.BonusAttackDamage() : 0.0f;
}
inline float TotalAD() {
    const auto player = GameObjects::Player();
    return player.IsValid() ? player.TotalAttackDamage() : 0.0f;
}
using ControllerHelpers::AP;
inline float FuryDamageQ(const AIHeroClient& target, bool empowered) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, QRawDamage(Rank(0), BonusAD(), empowered)) : 0.0f;
}
inline float FuryDamageW(const AIHeroClient& target, bool empowered) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, WRawDamage(Rank(1), TotalAD(), empowered)) : 0.0f;
}
inline float FuryDamageE(const AIHeroClient& target, bool empowered) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, ERawDamage(Rank(2), BonusAD(), empowered)) : 0.0f;
}
inline float DominusDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, RRawDamagePerTick(Rank(3), AP())) : 0.0f;
}
using ControllerHelpers::Lethal;
inline AIHeroClient SelectTarget(float range = 1000.0f) {
    return Engine::SelectTarget(range);
}
inline bool IsArmorShredActive(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) && ArmorShredTargetId == static_cast<int>(target.NetworkId()) &&
           ArmorShredExpireTick > Now();
}
inline EmpoweredChoice DesiredEmpowered(const AIHeroClient& target, bool allIn) {
    FuryChoiceContext context{};
    context.Fury = Fury;
    context.TargetLow = Engine::ValidEnemy(target) && target.HealthPercent() <= Slider(WMenu, "TargetHP", 35);
    context.NeedStun = Engine::ValidEnemy(target) &&
                       (IncomingHardCCUntil > Now() || target.IsDashing());
    context.NeedArmorShred = ECastStage == EStage::Dice && !IsArmorShredActive(target);
    context.NeedSustain = GameObjects::Player().IsValid() &&
                          GameObjects::Player().HealthPercent() <= Slider(QMenu, "SustainHP", 65);
    context.AllIn = allIn;
    return ChooseEmpowered(context);
}
inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Fury = ClampFury(CurrentResource(100.0f));
    const int now = Now();
    if (ArmorShredExpireTick <= now) {
        ArmorShredTargetId = 0;
        ArmorShredExpireTick = 0;
    }
    if (player.HasBuff("RenektonR") || player.HasBuff("RenektonPreExecute")) {
        // Dominus state is observed here; cooldown readiness remains runtime-owned.
    }
    if (player.HasBuff("RenektonDice") || player.HasBuff("RenektonE2")) ECastStage = EStage::Dice;
    (void)now;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !InRange(target, kQRange) ||
        !Ready(0, mode, reactive) || !Throttle(0) ||
        !QHits(player.Position(), PredictPosition(target, 0.25f), target.BoundingRadius())) return false;
    const bool empowered = DesiredEmpowered(target, mode == Mode::Combo) == EmpoweredChoice::Q;
    const bool lethal = Lethal(target, FuryDamageQ(target, empowered));
    if (PreserveAttack(reactive, lethal)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    Cooldowns.ReadyAt[0] = Now() + 80;
    Fury = FuryAfterCast(Fury, empowered ? kEmpoweredThreshold : 0.0f);
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (TargetBlocked(target) || !InRange(target, kWRange) || !Ready(1, mode, reactive) ||
        !Throttle(1, 75)) return false;
    const bool empowered = DesiredEmpowered(target, mode == Mode::Combo) == EmpoweredChoice::W;
    const bool lethal = Lethal(target, FuryDamageW(target, empowered));
    if (PreserveAttack(reactive, lethal)) return false;
    if (!Engine::ControllerCastUnit(1, target)) return false;
    Cooldowns.ReadyAt[1] = Now() + 90;
    Fury = FuryAfterCast(Fury, empowered ? kEmpoweredThreshold : 0.0f);
    return true;
}
inline bool SafeDashEndpoint(const Vector3& origin, const Vector3& endpoint,
                             const AIHeroClient& target, bool fleeing, bool lethal) {
    if (!endpoint.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    DashSafetyContext context{};
    context.Ready = true;
    context.EndpointValid = true;
    context.EndpointWalkable = !SDK::NavMesh::IsWall(endpoint);
    context.ThroughTarget = fleeing || (Engine::ValidEnemy(target) &&
        DashThroughTarget(origin, endpoint, PredictPosition(target, 0.18f), target.BoundingRadius()));
    context.EndpointUnderNewTurret = Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(origin);
    context.Defensive = fleeing;
    context.Lethal = lethal;
    context.Fleeing = fleeing;
    context.EnemiesAtEndpoint = Engine::CountEnemiesAt(endpoint, 500.0f);
    context.MaximumEnemies = Slider(EMenu, "MaxEndpointEnemies", 2);
    return DashSafe(context);
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool fleeing = false, bool reactive = false) {
    if (!Ready(2, mode, reactive) || !Throttle(2, 65)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool validTarget = Engine::ValidEnemy(target);
    Vector3 requested = fleeing ? Game::CursorPos() : PredictPosition(target, 0.18f);
    if (!fleeing && validTarget) {
        const Vector3 direction = Direction2D(player.Position(), requested);
        requested = requested + direction * 90.0f;
    }
    if (!requested.IsValid() || requested.IsZero()) return false;
    Vector3 endpoint = ClampDash(player.Position(), requested);
    if (endpoint.IsZero()) return false;
    const bool empowered = ECastStage == EStage::Dice &&
                           DesiredEmpowered(target, mode == Mode::Combo) == EmpoweredChoice::E;
    const bool lethal = validTarget && Lethal(target, FuryDamageE(target, empowered));
    if (!SafeDashEndpoint(player.Position(), endpoint, target, fleeing, lethal)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    Cooldowns.ReadyAt[2] = Now() + 85;
    ECastStage = ECastStage == EStage::Slice ? EStage::Dice : EStage::Slice;
    Fury = FuryAfterCast(Fury, empowered ? kEmpoweredThreshold : 0.0f);
    if (empowered && validTarget) {
        ArmorShredTargetId = static_cast<int>(target.NetworkId());
        ArmorShredExpireTick = Now() + kArmorShredDurationMs;
    }
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !InRange(target, kRRadius) ||
        !Ready(3, mode, defensive) || !Throttle(3, 120)) return false;
    DominusContext context{};
    context.Ready = true;
    context.TargetValid = true;
    context.InRange = true;
    context.IncomingHardCC = IncomingHardCCUntil > Now();
    context.PlayerLow = player.HealthPercent() <= Slider(RMenu, "PlayerHP", 48);
    context.TargetLow = target.HealthPercent() <= Slider(RMenu, "TargetHP", 45);
    context.AllIn = mode == Mode::Combo;
    context.Defensive = defensive;
    context.NearbyEnemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    context.MinimumEnemies = Slider(RMenu, "MinimumTargets", 2);
    if (!ShouldCastDominus(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    Cooldowns.ReadyAt[3] = Now() + 120;
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, FuryDamageW(target, EmpoweredReady(Fury))) && CastW(target, mode)) return true;
    if (Lethal(target, FuryDamageQ(target, EmpoweredReady(Fury))) && CastQ(target, mode)) return true;
    return ECastStage == EStage::Dice && Lethal(target, FuryDamageE(target, EmpoweredReady(Fury))) &&
           CastE(target, mode);
}
inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastE(target, Mode::Combo)) return true;
    if (CastW(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    if (CastE(target, Mode::Combo)) return true;
    return CastR(target, Mode::Combo);
}
inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || Fury < Slider(QMenu, "HarassFury", 30)) return false;
    if (CastQ(target, Mode::Harass)) return true;
    return CastW(target, Mode::Harass);
}
inline bool TryFlee(const AIHeroClient& threat) {
    if (CastE(threat, Mode::Flee, true, true)) return true;
    if (Engine::ValidEnemy(threat)) (void)CastW(threat, Mode::Flee, true);
    return Engine::ValidEnemy(threat) && CastR(threat, Mode::Flee, true);
}
inline bool TryFarm(Mode mode) {
    if (Fury < Slider(FarmMenu, "MinimumFury", 0)) return false;
    return Engine::TryFarm(mode);
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    (void)selected;
    LastMode = mode;
    ReconcileState();
    const AIHeroClient target = SelectTarget(mode == Mode::Flee ? 900.0f : 1000.0f);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 900.0f);
    if (mode == Mode::Flee) { (void)TryFlee(threat); return true; }
    if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(threat) && CastR(threat, mode, true)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) &&
            AutomaticAllowed({IncomingHardCCUntil > Now(), false,
                              Lethal(target, FuryDamageW(target, EmpoweredReady(Fury))), false}))
            (void)TryKillSecure(target, Mode::Automatic);
        break;
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot >= 4) return;
        if (slot == 2)
            ECastStage = ECastStage == EStage::Slice ? EStage::Dice : EStage::Slice;
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args, 220.0f, 115.0f,
        250, 280, 260, 1500, 450);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatUntil = std::max(IncomingThreatUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
        if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
            IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "RenektonDice") || Engine::TextContains(args.BuffName, "RenektonE2"))
            ECastStage = EStage::Dice;
        return;
    }
    if (Engine::TextContains(args.BuffName, "RenektonEArmor") || Engine::TextContains(args.BuffName, "RenektonE")) {
        ArmorShredTargetId = static_cast<int>(args.Sender.NetworkId);
        ArmorShredExpireTick = now + ControllerHelpers::RemainingMilliseconds(args.EndTime, kArmorShredDurationMs, 250, 5000);
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && (Engine::TextContains(args.BuffName, "RenektonDice") ||
        Engine::TextContains(args.BuffName, "RenektonE2"))) ECastStage = EStage::Slice;
    if (Engine::TextContains(args.BuffName, "RenektonEArmor") &&
        ArmorShredTargetId == static_cast<int>(args.Sender.NetworkId)) ArmorShredExpireTick = 0;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)args;
}
inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), kQRadius, 0x33E67E22u, 1.3f, 48);
    Drawing::DrawCircle(player.Position(), kRRadius, 0x33CC3344u, 1.3f, 48);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("RenektonOneTrick", "Renekton fury diver"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("RenektonQ", "Cull the Meek"));
    QMenu->Add(new MenuSlider("SustainHP", "Prefer empowered Q below player HP", 65, 20, 95));
    QMenu->Add(new MenuSlider("HarassFury", "Minimum fury for harass", 30, 0, 100));
    WMenu = TacticsMenu->AddSubMenu(new Menu("RenektonW", "Ruthless Predator"));
    WMenu->Add(new MenuSlider("TargetHP", "Prefer empowered W below target HP", 35, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("RenektonE", "Slice and Dice safety"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at endpoint", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("RenektonR", "Dominus"));
    RMenu->Add(new MenuSlider("PlayerHP", "Defensive Dominus HP", 48, 10, 90));
    RMenu->Add(new MenuSlider("TargetHP", "All-in Dominus target HP", 45, 10, 90));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum enemies for ordinary all-in", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("RenektonFarm", "Fury-aware farming"));
    FarmMenu->Add(new MenuSlider("MinimumFury", "Minimum fury before farm logic", 0, 0, 100));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("RenektonCoach", "Decision visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}
inline void OnLoad() {
    Fury = 0.0f;
    ECastStage = EStage::Slice;
    Cooldowns = {};
    ArmorShredTargetId = ArmorShredExpireTick = 0;
    IncomingThreatUntil = IncomingHardCCUntil = LastAutoTargetId = LastAutoTick = 0;
    LastMode = Mode::None;
    ReconcileState();
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Cooldowns = {};
    ECastStage = EStage::Slice;
}
inline constexpr const char* Scenarios[] = {
    "Use Riot 26.15 and CommunityDragon 16.15 Summoner's Rift values",
    "Read fury as the bounded special resource and reconcile it on every poll",
    "Reserve 50 fury for empowered Q sustain, W stun or Dice armor shred",
    "Prefer empowered W for kill or stun windows before empowered E and Q",
    "Use Q area and missing-health sustain without interrupting an AA windup",
    "Track W targeted range, two-hit ordinary damage and three-hit empowered stun",
    "Track Slice then Dice recast stage from process-spell and buff callbacks",
    "Reject E endpoints that are walls, new enemy turrets, hazards or overcrowded",
    "Apply empowered Dice armor shred to the observed target for four seconds",
    "Use Dominus for low-health defense, hard-CC response or configured all-ins",
    "Reconcile cooldown and resource state through event callbacks and polling",
    "Cover Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes",
    "Automatic mode kill-secures or responds to threat without fresh engagement",
    "Preserve event-reconciled Q, W, E and R state without manual ownership windows",
    "Preserve attack windup unless reactive or lethal cast value is observed",
    "Reject invulnerable, untargetable, spell-shielded and uncertain targets",
    "Never automate movement ownership, item actives or summoner spells",
    "Keep pure damage, fury thresholds and dash safety independently testable",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Renekton;
    controller.ControllerId = "champion.kuroaio.ai.renekton.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIRenekton.md";
    controller.ImplementationSummary =
        "Fury-threshold Q/W/E selection, autonomous dash safety, armor-shred "
        "tracking and defensive Dominus policy.";
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

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Renekton
