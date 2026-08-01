#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIMelGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Mel {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

struct MarkState {
    int NetworkId = 0;
    int Count = 0;
    int ExpireTick = 0;
};
inline std::array<MarkState, 32> Marks = {};
inline std::array<int, 4> LastCastTick = {};
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int ReflectionCastTick = 0;
inline int ManualOverrideUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastTargetId = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 55) {
    return slot >= 0 && slot < static_cast<int>(LastCastTick.size()) &&
        ControllerHelpers::CastThrottleReady(LastCastTick[static_cast<std::size_t>(slot)], delay);
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
inline bool ManaOk(int slot, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float floor = mode == Mode::Harass ?
        static_cast<float>(Slider(QMenu, "HarassMana", 42)) :
        static_cast<float>(Slider(FarmMenu, "Mana", 28));
    return slot == 1 || player.ManaPercent() >= floor;
}
inline MarkState* FindMark(int networkId, bool create = false) {
    if (networkId == 0) return nullptr;
    for (auto& mark : Marks) if (mark.NetworkId == networkId) return &mark;
    if (!create) return nullptr;
    MarkState* free = nullptr;
    for (auto& mark : Marks) {
        if (mark.NetworkId == 0 || mark.ExpireTick < Now()) { free = &mark; break; }
    }
    if (!free) free = &Marks.front();
    *free = {networkId, 0, Now() + kPassiveMarkDurationMs};
    return free;
}
inline int MarkCount(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0;
    const auto* mark = FindMark(static_cast<int>(target.NetworkId()));
    if (mark && mark->ExpireTick >= Now()) return ClampMarks(mark->Count);
    return target.HasBuff("MelPassiveOverwhelm") ? 1 : 0;
}
inline int MarkedTargets() {
    int count = 0;
    for (const auto& mark : Marks) if (mark.NetworkId != 0 &&
        mark.ExpireTick >= Now() && mark.Count > 0) ++count;
    return count;
}
inline bool ExecuteReady(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && CanExecute(target.Health(), target.AllShield(),
        SpellRank(3), player.AP(), MarkCount(target));
}
inline Vector3 Aim(const AIHeroClient& target, float delay) {
    return Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->GetPrediction(target).GetCastPosition().IsValid()
        ? Engine::RuntimeSpells[0]->GetPrediction(target).GetCastPosition()
        : PredictPosition(target, delay);
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(0, mode) ||
        !Throttle(0) || !ManaOk(0, mode) || PreserveAttack(reactive)) return false;
    const Vector3 aim = Aim(target, kQDelay);
    if (!QVolleyHits(player.Position(), aim, target.Position(), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQHalfWidth)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    LastTargetId = static_cast<int>(target.NetworkId());
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(2, mode) ||
        !Throttle(2) || !ManaOk(2, mode) || PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, kEDelay);
    if (!ERootHits(player.Position(), aim, target.Position(), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kEHalfWidth)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    LastTargetId = static_cast<int>(target.NetworkId());
    return true;
}
inline bool CastW(Mode mode, bool reactive = true) {
    const auto player = GameObjects::Player();
    const int now = Now();
    const bool incomingThreat = IncomingThreatUntil > now ||
        IncomingHardCCUntil > now;
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 120) ||
        !incomingThreat) return false;
    if (!reactive && PreserveAttack(false)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    ReflectionCastTick = Now();
    LastCastTick[1] = ReflectionCastTick;
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 120) ||
        Protected(target) || PreserveAttack(false)) return false;
    const int marks = MarkCount(target);
    const bool execute = ExecuteReady(target);
    const UltimateContext context{true, Engine::ValidEnemy(target), marks > 0,
        execute, player.HealthPercent() <= Slider(RMenu, "DefensiveHP", 25),
        manual, MarkedTargets(), Slider(RMenu, "MinimumTargets", 2)};
    if (!ShouldCastUltimate(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    LastTargetId = static_cast<int>(target.NetworkId());
    for (auto& mark : Marks) if (mark.ExpireTick >= Now()) mark = {};
    return true;
}
inline void ReconcileState() {
    const int now = Now();
    for (auto& mark : Marks) if (mark.ExpireTick < now) mark = {};
    if (ReflectionCastTick > 0 && now > ReflectionCastTick + 1100) ReflectionCastTick = 0;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("MelW") || player.HasBuff("MelWReflect"))
        ReflectionCastTick = std::max(ReflectionCastTick, now);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (ExecuteReady(target) && CastR(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 42)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (IncomingThreatUntil > Now() && CastW(Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastE(target, Mode::Flee, true);
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 1100.0f : kQRange);
    if (ManualOverrideUntil > Now()) return true;
    if (mode != Mode::Flee && IncomingHardCCUntil > Now() && CastW(mode, true)) return true;
    if (Engine::ValidEnemy(target) && ExecuteReady(target) && CastR(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "Mana", 28))
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && AutomaticAllowed({
            ManualOverrideUntil > Now(), IncomingThreatUntil > Now(),
            IncomingHardCCUntil > Now(), ExecuteReady(target)})) {
            if (IncomingThreatUntil > Now()) (void)CastW(Mode::Automatic, true);
            else (void)CastR(target, Mode::Automatic);
        }
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
            if (!Engine::WasControllerCast(slot)) ManualOverrideUntil =
                now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
        IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick,
                                      analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.BuffName, "Overwhelm") ||
        Engine::TextContains(args.BuffName, "MelPassive")) {
        auto* mark = FindMark(static_cast<int>(args.Sender.NetworkId), true);
        if (mark) {
            mark->Count = ClampMarks(std::max(mark->Count, args.Count));
            if (mark->Count <= 0) mark->Count = 1;
            mark->ExpireTick = std::max(Now() + kPassiveMarkDurationMs,
                static_cast<int>(args.EndTime * 1000.0f));
        }
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "Overwhelm")) {
        if (auto* mark = FindMark(static_cast<int>(args.Sender.NetworkId))) *mark = {};
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFE8B84Au, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFFB15CFFu, 1.0f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("MelTactics", "Mel reflection tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Radiant Volley"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 42, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Reflection"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Solar Snare"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Golden Eclipse"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum marked targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("DefensiveHP", "Defensive ultimate HP", 25, 5, 60));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("MelFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 28, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("MelCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and E ranges", false));
}
inline void OnLoad() {
    Marks.fill({}); LastCastTick.fill(0); IncomingThreatUntil = IncomingHardCCUntil = 0;
    ReflectionCastTick = ManualOverrideUntil = LastAutoTargetId = LastAutoTick = LastTargetId = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Marks.fill({});
}
inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Track Overwhelm marks from buff events and polling expiry",
    "Only execute when observed stored Overwhelm damage covers health and shields",
    "Use predicted Q volley with range, width and projectile-wall safety",
    "Use predicted Solar Snare with collision and root-line safety",
    "Reserve Reflection for a verified incoming projectile, hard crowd control or lethal threat",
    "Reconcile Reflection's live buff and 0.75-second denial window",
    "Preserve selected target before orbwalker and selector fallback",
    "Preserve AA windup unless reactive safety or confirmed execute requires a cast",
    "Track cooldowns, mana floors and manual spell ownership per slot",
    "Use Golden Eclipse only with live marks, execute value or configured multi-target value",
    "Never spend global ultimate on unmarked targets or speculative telemetry",
    "Combo builds Q/E marks before R and does not consume W offensively",
    "Harass uses mana-safe Q/E poke without opening an unsolicited global engage",
    "LaneClear Jungle and LastHit delegate to shared farm policy",
    "Flee reflects committed threats and roots a verified pursuer",
    "Automatic mode permits only defense, projectile denial or confirmed execute",
    "Reject invulnerable, untargetable and spell-shielded targets",
    "Yield after observed manual Q W E or R ownership",
    "Never automate items, summoner spells or movement ownership",
    "Draw ranges without changing gameplay decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Mel;
    controller.ControllerId = "champion.kuroaio.ai.mel.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIMel.md";
    controller.ImplementationSummary =
        "Observed Overwhelm mark/execute state, prediction-safe radiant lines, projectile reflection and conservative global finisher.";
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
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Mel
