#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AISmolderGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Smolder {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CursorDirectionAgrees;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int ObservedStacks = 0;
inline int PredictedStacks = 0;
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastBeforeAttackTargetId = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int FlightUntil = 0;
inline int FlightStartTick = 0;
inline Vector3 FlightEndpoint{};
inline Vector3 LastQAim{};
inline Vector3 LastWAim{};
inline Vector3 LastRAim{};
inline Mode LastMode = Mode::None;

inline int PassiveStacks() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    const int first = std::max(0, player.GetBuffCount("SmolderDragonPractice"));
    const int second = std::max(0, player.GetBuffCount("SmolderPassive"));
    return std::max(first, second);
}
inline void ReconcileStacks() {
    const int observed = PassiveStacks();
    if (observed > 0 || ObservedStacks == 0) ObservedStacks = observed;
    PredictedStacks = std::max(0, PredictedStacks);
    if (observed > 0) PredictedStacks = observed;
}
inline int Stacks() { return std::max(ObservedStacks, PredictedStacks); }
inline bool Ready(int slot, Mode mode) {
    return slot >= 0 && slot < 4 && SpellEnabled(slot, mode) &&
           CastThrottleReady(slot, Slider(TacticsMenu, "HumanizerMs", 30));
}
inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target);
}
inline bool PreserveAttack(bool reactive, bool lethal = false) {
    return !reactive && !lethal &&
           Bool(Engine::HumanMenu, "PreserveAttacks", true) &&
           Orbwalker::IsWindingUp() && Orbwalker::AttackCastDelayRemaining() > 25;
}
using ControllerHelpers::Lethal;
inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(target, QRawDamage(
        SpellRank(0), player.TotalAttackDamage(), Stacks(), target.MaxHealth()));
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = WRawDamage(SpellRank(1), player.BonusAttackDamage()) +
        WExplosionRawDamage(SpellRank(1), player.BonusAttackDamage());
    return player.CalculatePhysicalDamage(target, raw);
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(target,
        ERawDamage(SpellRank(2), player.BonusAttackDamage()));
}
inline float RDamage(const AIHeroClient& target, bool center = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(target,
        RRawDamage(SpellRank(3), player.BonusAttackDamage(), center));
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(0, mode) ||
        PreserveAttack(reactive, Lethal(target, QDamage(target)))) return false;
    const Vector3 aim = PredictPosition(target, kQDelay);
    if (!aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > kQRange + target.BoundingRadius()) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    LastQAim = aim;
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(1, mode) ||
        PreserveAttack(reactive, Lethal(target, WDamage(target)))) return false;
    const Vector3 aim = PredictPosition(target, kWDelay);
    if (!aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > kWRange + target.BoundingRadius() ||
        !LineHits(player.Position(), aim, target.Position(), kWRange, kWWidth,
                  target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocks(player.Position(), aim, kWWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    LastWAim = aim;
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false, bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || PreserveAttack(reactive, lethal)) return false;
    const Vector3 requested = Game::CursorPos();
    const Vector3 endpoint = ClampFlightEndpoint(player.Position(), requested);
    if (endpoint.IsZero()) return false;
    const bool defensive = reactive || player.HealthPercent() <=
        static_cast<float>(Slider(EMenu, "DefensiveHp", 40));
    const FlightContext context{
        true, endpoint.IsValid() && !endpoint.IsZero(),
        SDK::NavMesh::IsWall(endpoint),
        !defensive && Engine::UnderEnemyTurret(endpoint),
        !defensive && Engine::CountEnemiesAt(endpoint, 250.0f) >
            Slider(EMenu, "MaxEndpointEnemies", 1),
        CursorDirectionAgrees(endpoint, -0.02f), defensive, lethal, manual,
        player.Position().Distance2D(endpoint), kERange};
    if (!ShouldTakeFlight(context)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastCastTick[2] = Now();
    FlightStartTick = Now();
    FlightUntil = FlightStartTick + static_cast<int>(kEFlightSeconds * 1000.0f);
    FlightEndpoint = endpoint;
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(3, mode) ||
        PreserveAttack(reactive, Lethal(target, RDamage(target)))) return false;
    const Vector3 aim = PredictPosition(target, kRDelay);
    if (!aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > kRRange + target.BoundingRadius() ||
        ControllerHelpers::ProjectileWallBlocks(player.Position(), aim, kRWidth * 0.5f)) return false;
    int targets = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && AreaContains(aim, PredictPosition(enemy, kRDelay),
                                                       kRWidth, enemy.BoundingRadius())) ++targets;
    }
    const bool centerLethal = Lethal(target, RDamage(target, true));
    const RContext context{true, true, false, centerLethal || Lethal(target, RDamage(target)),
        reactive || player.HealthPercent() <= Slider(RMenu, "DefensiveHp", 38), manual,
        targets, Slider(RMenu, "MinimumTargets", 2)};
    if (!ShouldCastR(context)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastCastTick[3] = Now();
    LastRAim = aim;
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, RDamage(target, true)) && CastR(target, mode)) return true;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, false, true)) return true;
    if (Lethal(target, WDamage(target)) && CastW(target, mode)) return true;
    return Lethal(target, QDamage(target)) && CastQ(target, mode);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo, false, false)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 48)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& threat) {
    if (Engine::ValidEnemy(threat) && CastR(threat, Mode::Flee, true)) return;
    (void)CastE(threat, Mode::Flee, true, false);
}
inline bool TryFarm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (mode != Mode::LastHit && player.ManaPercent() < Slider(FarmMenu, "Mana", 42)) return false;
    // Engine::TryFarm uses the live minion/jungle snapshots and remains the
    // conservative fallback when no legal champion target exists.
    return Engine::TryFarm(mode);
}
inline bool Automatic(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (IncomingHardCCUntil > Now() && CastR(target, Mode::Automatic, true)) return true;
    if (TryKillSecure(target, Mode::Automatic)) return true;
    return IncomingThreatUntil > Now() && CastE(target, Mode::Automatic, true);
}
inline void ReconcileState() {
    ReconcileStacks();
    if (FlightUntil > 0 && Now() > FlightUntil) {
        FlightUntil = 0;
        FlightEndpoint = {};
    }
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, kRRange);
    if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(target) &&
        CastR(target, mode, true)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, kRRange)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic: (void)Automatic(target); break;
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
            if (!Engine::WasControllerCast(slot)) ManualOwnershipUntil = now +
                Slider(TacticsMenu, "ManualOwnershipMs", 560);
            LastCastTick[slot] = now;
            if (args.IsAutoAttack) {
                LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
                LastAutoTick = now;
            }
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
        IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick,
                                      analysis.LineThreatUntilTick));
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastBeforeAttackTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF73D7FFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFFFA14Bu, 1.0f, 40);
    if (!FlightEndpoint.IsZero() && FlightUntil > Now())
        Drawing::DrawCircle(FlightEndpoint, 90.0f, 0xFFCC77FFu, 1.5f, 24);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("SmolderOneTrick", "Smolder dragon tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("HumanizerMs", "Minimum cast spacing (ms)", 30, 18, 150));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Dragon Practice and Q evolution"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Achoo line"));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Flight safety"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 1, 0, 4));
    EMenu->Add(new MenuSlider("DefensiveHp", "Defensive flight health percent", 40, 10, 80));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "MMOOOMMMM"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum nonlethal targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("DefensiveHp", "Defensive heal health percent", 38, 10, 80));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("SmolderFarm", "Stack and farm policy"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum clear mana percent", 42, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("SmolderCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/R and flight endpoint", false));
}
inline void OnLoad() {
    ObservedStacks = PredictedStacks = 0;
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = LastBeforeAttackTargetId = 0;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    FlightUntil = FlightStartTick = 0;
    FlightEndpoint = LastQAim = LastWAim = LastRAim = {};
    LastMode = Mode::None;
    ReconcileStacks();
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    FlightEndpoint = {};
}
inline constexpr const char* Scenarios[] = {
    "Pin Smolder mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Track Dragon Practice from buff events plus polling reconciliation",
    "Evolve Q exactly at 25, 125 and 225 observed stacks",
    "Keep Q stack-scaled damage and explosion area boundary-safe",
    "Preserve selected target before orbwalker and selector fallback",
    "Use W prediction, width, collision and line reach before casting",
    "Use E only with a valid cursor endpoint and safe flight destination",
    "Reject E endpoints through walls, turrets or excessive enemy threat",
    "Preserve cursor intent and never own player movement after flight",
    "Use R for center execute, area damage, defensive heal or configured team value",
    "Reject R through projectile walls and require observed prediction",
    "Preserve AA windup unless a reactive or lethal cast is justified",
    "Respect manual Q W E R ownership windows from process-spell events",
    "Reconcile flight lifecycle from polling after cast and expiry",
    "Combo uses W/Q pressure, safe E and area/execute R",
    "Harass spends mana only above the configured reserve",
    "LaneClear uses live farm snapshots and Q/W wave pressure",
    "Jungle uses the shared jungle-unit farm policy without invented targets",
    "LastHit delegates to the conservative farm path",
    "Flee uses defensive R and an away-safe flight endpoint",
    "Automatic mode permits only defense, threat response or kill secure",
    "Never automate items, summoners, attack movement or cursor changes",
    "Draw ranges and endpoint state without changing gameplay decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Smolder;
    controller.ControllerId = "champion.kuroaio.ai.smolder.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISmolder.md";
    controller.ImplementationSummary =
        "Dragon Practice stack reconciliation, Q evolution, safe flight endpoint and area/execute R policy.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnBuffAdd = &ControllerHelpers::ForwardNoArgBuffEvent<&ReconcileStacks>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardNoArgBuffEvent<&ReconcileStacks>;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardNoArgBuffEvent<&ReconcileStacks>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Smolder
