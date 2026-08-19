#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIVladimirGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Vladimir {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttackEvent;
using ControllerHelpers::CaptureLocalAutoAttackEvent;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellRank;
using ControllerHelpers::PlayerMobilityLocked;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

struct HemoplagueMark {
    int NetworkId = 0;
    int ExpireTick = 0;
    int ObservedCount = 1;
};

inline std::array<HemoplagueMark, 24> Marks = {};
inline std::array<int, 4> LastCastTick = {};
inline int IncomingThreatUntil = 0;
inline int IncomingHardCrowdControlUntil = 0;
inline int ManualOverrideUntil = 0;
inline int LastTargetId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int EChargeStartTick = 0;
inline int EExpectedReleaseTick = 0;
inline int WInvulnerableUntil = 0;
inline bool QEmpowered = false;
inline bool ECharging = false;
inline bool WControllerOwned = false;
inline Mode LastMode = Mode::None;

inline bool Throttle(int slot, int delay = 45) {
    return slot >= 0 && slot < static_cast<int>(LastCastTick.size()) &&
        ControllerHelpers::CastThrottleReady(LastCastTick[static_cast<std::size_t>(slot)], delay);
}

inline bool PreserveAttack(bool reactive = false) {
    return !reactive && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 25 &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline HemoplagueMark* FindMark(int id, bool create = false) {
    if (id == 0) return nullptr;
    for (auto& mark : Marks) if (mark.NetworkId == id) return &mark;
    if (!create) return nullptr;
    HemoplagueMark* free = nullptr;
    for (auto& mark : Marks) {
        if (mark.NetworkId == 0 || mark.ExpireTick < Now()) {
            free = &mark;
            break;
        }
    }
    if (!free) free = &Marks.front();
    *free = {id, Now() + static_cast<int>(kRMarkSeconds * 1000.0f), 1};
    return free;
}

inline int MarkCount(const AIHeroClient& target) {
    if (!target.IsValid()) return 0;
    const int id = static_cast<int>(target.NetworkId());
    const auto* mark = FindMark(id);
    if (mark && mark->ExpireTick >= Now()) return mark->ObservedCount;
    return target.HasBuff("VladimirHemoplagueDebuff") ||
        target.HasBuff("vladimirhemoplague") ? 1 : 0;
}

inline int ActiveMarkedTargets() {
    int count = 0;
    for (const auto& mark : Marks) {
        if (mark.NetworkId != 0 && mark.ExpireTick >= Now()) ++count;
    }
    return count;
}

inline bool LowHealthSafety(bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (player.HealthPercent() <= Slider(WMenu, "EmergencyHP", 23)) return reactive;
    return player.HealthPercent() >= Slider(TacticsMenu, "MinimumCommitHP", 30) || reactive;
}

inline bool HealthCostSafe(float costPercent, bool reactive = false) {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        HealthTradeSafe(player.Health(), player.MaxHealth(), costPercent,
                        reactive ? 8.0f : Slider(TacticsMenu, "MinimumPostCastHP", 12));
}

inline AIHeroClient SelectTarget(const AIHeroClient& selected, float range) {
    return PreferredEnemyTarget(selected, range);
}

inline bool TargetLegal(const AIHeroClient& target, float range) {
    return Engine::ValidEnemy(target, range) && !HasSpellShieldOrImmunity(target) &&
        !target.IsInvulnerable() && target.IsTargetable();
}

inline Vector3 PredictedTarget(const AIHeroClient& target, float delay) {
    if (!target.IsValid()) return {};
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        const Vector3 cast = prediction.GetCastPosition();
        if (cast.IsValid() && !cast.IsZero()) return cast;
    }
    return PredictPosition(target, delay);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!TargetLegal(target, kQRange + 35.0f) || !player.IsValid() ||
        !Ready(0, mode) || !Throttle(0) || PreserveAttack(reactive)) return false;
    const float qRaw = QDamage(SpellRank(0), player.AP(), QEmpowered);
    const float qHeal = QHeal(SpellRank(0), player.AP(), QEmpowered,
        100.0f - player.HealthPercent());
    const bool qKillable = qRaw >= target.Health() + target.AllShield();
    const bool qSustain = qHeal >= player.MaxHealth() - player.Health();
    const Vector3 predicted = PredictedTarget(target, kQDelay);
    if (!LowHealthSafety(reactive) && !qKillable && !qSustain) return false;
    if (player.HealthPercent() < 20.0f && !qKillable && !qSustain && !reactive) return false;
    if (!QReachable(player.Position(), predicted, target.BoundingRadius())) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    LastCastTick[0] = Now();
    LastTargetId = static_cast<int>(target.NetworkId());
    QEmpowered = false;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = true) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 160) ||
        PreserveAttack(reactive) || !LowHealthSafety(reactive)) return false;
    const bool hardCc = IncomingHardCrowdControlUntil >= Now();
    const bool lethal = IncomingThreatUntil >= Now() && player.HealthPercent() <= 34.0f;
    const bool flee = mode == Mode::Flee;
    if (!CanUsePool(player.HealthPercent(), hardCc, lethal, flee) ||
        !HealthCostSafe(15.0f, reactive)) return false;
    if (TargetLegal(target, kWRadius + 100.0f) &&
        !CircleHits(player.Position(), PredictPosition(target, 0.15f), kWRadius,
                    target.BoundingRadius()) && !flee) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    WInvulnerableUntil = Now() + static_cast<int>(kWDurationSeconds * 1000.0f);
    WControllerOwned = true;
    return true;
}

inline bool StartE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || ECharging || !Throttle(2) ||
        PreserveAttack(reactive) || !LowHealthSafety(reactive) ||
        !HealthCostSafe(EHealthCostPercent(0.0f), reactive)) return false;
    const Vector3 predicted = PredictedTarget(target, 0.20f);
    if (TargetLegal(target, kERange + 35.0f) &&
        !EReleaseHits(player.Position(), predicted, 0.0f, target.BoundingRadius())) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    ECharging = true;
    EChargeStartTick = Now();
    EExpectedReleaseTick = Now() + static_cast<int>(kEChargeSeconds * 1000.0f);
    LastCastTick[2] = Now();
    return true;
}

inline bool ReleaseE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ECharging || !Ready(2, mode) || PreserveAttack(reactive)) return false;
    const float charge = std::clamp((Now() - EChargeStartTick) * 0.001f,
                                    0.0f, kEChargeSeconds);
    const Vector3 predicted = PredictedTarget(target, 0.10f);
    if (TargetLegal(target, kERange + 35.0f) &&
        !EReleaseHits(player.Position(), predicted, charge, target.BoundingRadius())) {
        if (charge < 0.18f) return false;
    }
    if (!HealthCostSafe(EHealthCostPercent(charge), reactive) && !reactive) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    ECharging = false;
    EChargeStartTick = EExpectedReleaseTick = 0;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool manual = false) {
    const auto player = GameObjects::Player();
    if (!TargetLegal(target, kRRange + 35.0f) || !player.IsValid() ||
        !Ready(3, mode) || !Throttle(3, 130) || PreserveAttack(false)) return false;
    const Vector3 predicted = PredictedTarget(target, 0.39f);
    if (!RPlacementHits(predicted, target.Position(), target.BoundingRadius()) ||
        ProjectileWallBlocksFromPlayer(predicted, 50.0f)) return false;
    const int grouped = Engine::CountEnemiesAt(predicted, kRRadius);
    const bool underTurret = Engine::UnderEnemyTurret(predicted);
    const int nearby = Engine::CountEnemiesAt(predicted, 650.0f);
    const float rHeal = RHeal(SpellRank(3), player.AP(), std::max(1, grouped));
    const bool healingWindow = rHeal >= player.MaxHealth() - player.Health();
    if (!LowHealthSafety(false) && !healingWindow && !manual) return false;
    const bool lethalWindow = target.HealthPercent() <= 25.0f;
    if (underTurret && !lethalWindow && player.HealthPercent() < 65.0f) return false;
    if (nearby > (Engine::ActiveProfile ? Engine::ActiveProfile->MaximumCommitEnemies : 3) &&
        player.HealthPercent() < 55.0f && !lethalWindow) return false;
    const float burstRaw = RInitialDamage(SpellRank(3), player.AP()) +
        RAmplifiedDamage(QDamage(SpellRank(0), player.AP(), true), true) +
        RAmplifiedDamage(EDamage(SpellRank(2), player.AP(), 1.0f), true);
    const bool shieldedKillable = burstRaw >= target.Health() + target.AllShield();
    if (PlayerMobilityLocked() && player.HealthPercent() < 30.0f && !manual &&
        !lethalWindow) return false;
    const bool low = target.HealthPercent() <= Slider(RMenu, "TargetHP", 58) ||
        shieldedKillable;
    const bool multi = grouped >= Slider(RMenu, "MinimumTargets", 2);
    const VladimirUltimateContext context{true, true, true, low, multi,
        LowHealthSafety(false) || healingWindow, manual};
    if (!ShouldCastHemoplague(context)) return false;
    if (!Engine::ControllerCastPosition(3, predicted)) return false;
    LastCastTick[3] = Now();
    LastTargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline void ReconcileState() {
    const int now = Now();
    for (auto& mark : Marks) if (mark.ExpireTick < now) mark = {};
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("VladimirQFrenzy") || player.HasBuff("vladimirqfrenzy")) {
        QEmpowered = true;
    } else if (QEmpowered && now - LastCastTick[0] > 650) {
        QEmpowered = false;
    }
    if (player.HasBuff("VladimirSanguinePool") || player.HasBuff("VladimirW")) {
        WInvulnerableUntil = std::max(WInvulnerableUntil,
            now + static_cast<int>(kWDurationSeconds * 1000.0f));
    } else if (WInvulnerableUntil < now) {
        WControllerOwned = false;
    }
    if (player.HasBuff("VladimirE")) ECharging = true;
    if (ECharging && now > EExpectedReleaseTick + 250) {
        ECharging = false;
        EChargeStartTick = EExpectedReleaseTick = 0;
    }
}

inline void Combo(const AIHeroClient& target) {
    if (!TargetLegal(target, 900.0f)) return;
    if (CastR(target, Mode::Combo)) return;
    if (QEmpowered && CastQ(target, Mode::Combo)) return;
    if (!ECharging && StartE(target, Mode::Combo)) return;
    if (ECharging && (Now() >= EExpectedReleaseTick || target.HealthPercent() <= 42.0f) &&
        ReleaseE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    (void)CastW(target, Mode::Combo, true);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.HealthPercent() < 42.0f ||
        !TargetLegal(target, 700.0f)) return;
    if (QEmpowered && CastQ(target, Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (!ECharging && player.HealthPercent() > 55.0f) (void)StartE(target, Mode::Harass);
    else if (ECharging) (void)ReleaseE(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& target) {
    if (CastW(target, Mode::Flee, true)) return;
    if (TargetLegal(target, 700.0f) && QEmpowered && CastQ(target, Mode::Flee, true)) return;
    if (ECharging) (void)ReleaseE(target, Mode::Flee, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const AIHeroClient target = SelectTarget(selected, mode == Mode::Flee ? 900.0f : 700.0f);
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (ManualOverrideUntil > Now()) return true;
    if ((IncomingHardCrowdControlUntil >= Now() || IncomingThreatUntil >= Now()) &&
        mode != Mode::LaneClear && mode != Mode::Jungle && mode != Mode::LastHit &&
        CastW(target, mode, true)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (player.HealthPercent() >= Slider(FarmMenu, "MinimumHP", 38))
            return Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (IncomingThreatUntil >= Now()) (void)CastW(target, mode, true);
        else if (TargetLegal(target, 700.0f)) {
            if (!CastR(target, mode) && QEmpowered) (void)CastQ(target, mode, true);
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
        if (args.Slot >= 0 && args.Slot < 4) {
            LastCastTick[args.Slot] = now;
            if (!Engine::WasControllerCast(args.Slot))
                ManualOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        }
        if (args.Slot == static_cast<int>(SDK::SpellSlot::E)) ECharging = true;
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args, 230.0f, 110.0f, 280, 260, 220, 1600, 450);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        IncomingHardCrowdControlUntil = std::max(IncomingHardCrowdControlUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (Engine::TextContains(args.BuffName, "Hemoplague") ||
        Engine::TextContains(args.BuffName, "VladimirHemoplagueDebuff")) {
        auto* mark = FindMark(id, true);
        if (mark) {
            mark->ObservedCount = std::max(1, args.Count);
            mark->ExpireTick = std::max(Now() + static_cast<int>(kRMarkSeconds * 1000.0f),
                static_cast<int>(args.EndTime * 1000.0f));
        }
    }
    const auto player = GameObjects::Player();
    if (player.IsValid() && id == static_cast<int>(player.NetworkId()) &&
        Engine::TextContains(args.BuffName, "QFrenzy")) QEmpowered = true;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "Hemoplague")) {
        if (auto* mark = FindMark(static_cast<int>(args.Sender.NetworkId))) *mark = {};
    }
    if (Engine::TextContains(args.BuffName, "QFrenzy")) QEmpowered = false;
    if (Engine::TextContains(args.BuffName, "SanguinePool")) WInvulnerableUntil = 0;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastTargetId = static_cast<int>(args.Target.NetworkId());
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
    LastAutoTick = Now();
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFAA5577u, 1.4f, 40);
    Drawing::DrawCircle(player.Position(), kERadius, 0xFFCC3344u, 1.1f, 40);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFF7733CCu, 1.0f, 40);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("VladimirTactics", "Vladimir health tradeoffs"));
    TacticsMenu->Add(new MenuSlider("MinimumCommitHP", "Minimum commit HP", 30, 8, 80));
    TacticsMenu->Add(new MenuSlider("MinimumPostCastHP", "Minimum post-cast HP", 12, 5, 40));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual ownership (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Transfusion / Frenzy"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Sanguine Pool"));
    WMenu->Add(new MenuSlider("EmergencyHP", "Pool emergency HP", 23, 5, 60));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Tides of Blood charge"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Hemoplague"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum marked targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("TargetHP", "Target HP threshold", 58, 10, 90));
    RMenu->Add(new MenuSlider("MinimumPlayerHP", "Minimum player HP", 28, 8, 70));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Health-safe farming"));
    FarmMenu->Add(new MenuSlider("MinimumHP", "Minimum HP for farm", 38, 10, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q E R ranges", false));
}

inline void OnLoad() {
    Marks.fill({}); LastCastTick.fill(0);
    IncomingThreatUntil = IncomingHardCrowdControlUntil = ManualOverrideUntil = 0;
    LastTargetId = LastAutoTargetId = LastAutoTick = 0;
    EChargeStartTick = EExpectedReleaseTick = WInvulnerableUntil = 0;
    QEmpowered = ECharging = WControllerOwned = false;
    LastMode = Mode::None;
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Marks.fill({});
    QEmpowered = ECharging = WControllerOwned = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin Vladimir health resource and spell semantics to Riot 26.15 / CommunityDragon 16.15",
    "Reconcile Q Frenzy empowerment from buff events and polling expiry",
    "Use predicted, reachable Q with projectile-wall and target immunity checks",
    "Hold empowered Q for a valid champion or health-sustain opportunity",
    "Track one-second E charge and release it at target-safe radius with health-cost gates",
    "Never spend E health cost below the configured post-cast floor unless reactive",
    "Reserve Sanguine Pool for hard crowd control, lethal pressure, low HP or Flee",
    "Reconcile Pool untargetability from events and live buff polling",
    "Place Hemoplague through predicted circle reach and projectile-wall safety",
    "Track four-second Hemoplague marks and amplification targets from buff events",
    "Use R for low target health, grouped enemies or manual assist with player-health gate",
    "Reject invulnerable, untargetable and spell-shielded targets",
    "Preserve AA windup except verified Pool defense or reactive escape",
    "Yield after observed manual Q W E or R ownership and reconcile on polling",
    "Preserve selected target before orbwalker and selector fallback",
    "Respect turret and enemy-count commit limits before damage windows",
    "Combo marks with R, charges E, spends empowered Q, and retains W for safety",
    "Harass uses Q sustain and short E only above the health floor",
    "LaneClear, Jungle and LastHit use health-safe shared farm policy",
    "Flee prioritizes Pool, then empowered Q sustain and E release on pursuers",
    "Automatic mode only reacts to threats or confirmed Hemoplague damage windows",
    "Draw geometry without changing gameplay decisions",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Vladimir;
    controller.ControllerId = "champion.kuroaio.ai.vladimir.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIVladimir.md";
    controller.ImplementationSummary =
        "Health-gated Transfusion/Frenzy, charged Tides of Blood, threat-timed Pool and marked Hemoplague burst.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Vladimir
