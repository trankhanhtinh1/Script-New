#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AINasusGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Nasus {

using namespace Geometry;
using ControllerHelpers::HasSpellShieldOrImmunity;
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

inline float SiphonStacks = 0.0f;
inline int WitherTargetId = 0;
inline int WitherExpireTick = 0;
inline int SpiritFireTargetId = 0;
inline int SpiritFireExpireTick = 0;
inline int FuryExpireTick = 0;
inline int ManualOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline bool QEmpowered = false;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::SpellRank;

inline bool Throttle(int slot, int delay = 70) {
    return slot >= 0 && slot < 4 &&
           Now() - LastCastTick[static_cast<std::size_t>(slot)] >= delay;
}

inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target);
}

inline AIHeroClient SelectTarget(const AIHeroClient& selected, float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orb = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range)) return orb;
    return Engine::SelectTarget(range);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !Ready(0, mode) ||
        !Throttle(0) || !WithinQReach(player.Position().Distance2D(target.Position()),
                                       250.0f, target.BoundingRadius())) return false;
    const float raw = QRawDamage(SpellRank(0), player.TotalAttackDamage(), SiphonStacks);
    const bool lethal = ControllerHelpers::Lethal(target, player.CalculatePhysicalDamage(target, raw));
    if (PreserveAttack(reactive, lethal)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    LastCastTick[0] = Now();
    QEmpowered = false;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!Engine::ValidEnemy(target, kWRange) || TargetBlocked(target) ||
        !Ready(1, mode) || !Throttle(1, 90)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || (!reactive && player.ManaPercent() < Slider(WMenu, "Mana", 35))) return false;
    if (!Engine::ControllerCastUnit(1, target)) return false;
    LastCastTick[1] = Now();
    WitherTargetId = static_cast<int>(target.NetworkId());
    WitherExpireTick = Now() + 5000;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool defensive = false,
                  bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2, 100)) return false;
    const bool validTarget = Engine::ValidEnemy(target, kERange);
    const Vector3 predicted = validTarget ? PredictPosition(target, kEDelay) : Game::CursorPos();
    if (!predicted.IsValid() || predicted.IsZero()) return false;
    const bool lethal = validTarget && ControllerHelpers::Lethal(
        target, player.CalculateMagicDamage(target,
            EInitialDamage(SpellRank(2), player.AP())));
    const ZonePlacementContext context{
        true,
        !predicted.IsZero(),
        !SDK::NavMesh::IsWall(predicted),
        ControllerHelpers::ProjectileWallBlocksFromPlayer(predicted, 60.0f),
        Engine::UnderEnemyTurret(predicted) && !Engine::UnderEnemyTurret(player.Position()),
        defensive || reactive,
        lethal,
        Engine::CountEnemiesAt(predicted, kERadius),
        Slider(EMenu, "MaxEnemies", 2)};
    if (!ZonePlacementSafe(context)) return false;
    if (!Engine::ControllerCastPosition(2, predicted)) return false;
    LastCastTick[2] = Now();
    SpiritFireTargetId = validTarget ? static_cast<int>(target.NetworkId()) : 0;
    SpiritFireExpireTick = Now() + static_cast<int>(kEDurationSeconds * 1000.0f);
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 130)) return false;
    const bool validTarget = Engine::ValidEnemy(target);
    const UltimateContext context{
        true,
        defensive || player.HealthPercent() <= Slider(RMenu, "DefensiveHP", 48),
        validTarget && ControllerHelpers::Lethal(target, player.CalculateMagicDamage(
            target, EInitialDamage(SpellRank(2), player.AP()))),
        validTarget && target.HealthPercent() <= Slider(RMenu, "TargetHP", 58),
        IncomingHardCCUntil > Now(),
        Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(player.Position(), kRRadius),
        Slider(RMenu, "MinimumTargets", 2)};
    if (!ShouldCastUltimate(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    FuryExpireTick = Now() + static_cast<int>(kRDurationSeconds * 1000.0f);
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastQ(target, mode)) return true;
    return CastE(target, mode, false, true);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (now >= WitherExpireTick) WitherTargetId = 0;
    if (now >= SpiritFireExpireTick) SpiritFireTargetId = 0;
    if (now >= FuryExpireTick) FuryExpireTick = 0;
    if (now >= ManualOverrideUntil) ManualOverrideUntil = 0;
    QEmpowered = QEmpowered || player.HasBuff("NasusQ") || player.HasBuff("NasusQAttack");
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    if (ManualOverrideUntil > Now()) return true;
    const AIHeroClient target = SelectTarget(selected, mode == Mode::Flee ? 850.0f : kWRange);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 850.0f);
    if (mode == Mode::Flee) {
        if (Engine::ValidEnemy(threat)) (void)CastW(threat, mode, true);
        (void)CastE(threat, mode, true, true);
        if (GameObjects::Player().HealthPercent() <= Slider(RMenu, "DefensiveHP", 48))
            (void)CastR(threat, mode, true);
        return true;
    }
    if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(threat)) {
        if (CastW(threat, mode, true)) return true;
        (void)CastR(threat, mode, true);
    }
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo:
        if (CastW(target, mode)) return true;
        if (CastE(target, mode)) return true;
        (void)CastQ(target, mode);
        break;
    case Mode::Harass:
        if (CastW(target, mode)) return true;
        (void)CastE(target, mode);
        break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        (void)Engine::TryFarm(mode);
        if (Engine::ValidEnemy(target) && mode != Mode::LastHit) (void)CastE(target, mode);
        break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && IncomingThreatUntil > Now()) (void)CastW(target, mode, true);
        break;
    default:
        break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = now;
            if (!Engine::WasControllerCast(slot)) ManualOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
            if (slot == 0) QEmpowered = true;
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
        IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    const int remaining = ControllerHelpers::RemainingMilliseconds(args.EndTime, 5000, 250, 10000);
    if (Engine::TextContains(args.BuffName, "NasusQ")) QEmpowered = true;
    if (Engine::TextContains(args.BuffName, "NasusW")) {
        WitherTargetId = id;
        WitherExpireTick = Now() + remaining;
    }
    if (Engine::TextContains(args.BuffName, "NasusE")) {
        SpiritFireTargetId = id;
        SpiritFireExpireTick = Now() + remaining;
    }
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "NasusR"))
        FuryExpireTick = Now() + remaining;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    const int id = args.Sender.IsValid() ? static_cast<int>(args.Sender.NetworkId) : 0;
    if (Engine::TextContains(args.BuffName, "NasusQ")) QEmpowered = false;
    if (Engine::TextContains(args.BuffName, "NasusW") && id == WitherTargetId) WitherTargetId = 0;
    if (Engine::TextContains(args.BuffName, "NasusE") && id == SpiritFireTargetId) SpiritFireTargetId = 0;
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "NasusR")) FuryExpireTick = 0;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (QEmpowered && args.Target.IsValid() && args.Target.IsHero()) {
        const AIHeroClient target(args.Target.Handle());
        if (Engine::ValidEnemy(target)) args.Process = true;
    }
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kWRange, 0xFFAA8844u, 1.3f, 48);
    Drawing::DrawCircle(player.Position(), kERange, 0xFFCC6633u, 1.3f, 48);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("NasusOneTrick", "Nasus scaling juggernaut"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("NasusQ", "Siphoning Strike"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("NasusW", "Wither"));
    WMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("NasusE", "Spirit Fire"));
    EMenu->Add(new MenuSlider("MaxEnemies", "Maximum enemies at new zone", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("NasusR", "Fury of the Sands"));
    RMenu->Add(new MenuSlider("DefensiveHP", "Defensive ultimate HP", 48, 10, 85));
    RMenu->Add(new MenuSlider("TargetHP", "Ultimate target HP", 58, 10, 95));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum nearby enemies", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("NasusFarm", "Q stack farming"));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("NasusCoach", "Decision visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw W and E ranges", false));
}

inline void OnLoad() {
    SiphonStacks = 0.0f;
    WitherTargetId = WitherExpireTick = SpiritFireTargetId = SpiritFireExpireTick = 0;
    FuryExpireTick = ManualOverrideUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = 0;
    QEmpowered = false;
    LastMode = Mode::None;
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    QEmpowered = false;
}

inline constexpr const char* Scenarios[] = {
    "Use Riot 26.15 and CommunityDragon 16.15 Summoner's Rift values",
    "Track Siphoning Strike stacks from observed events and polling reconciliation",
    "Preserve an empowered Q for a reachable attack reset or configured last-hit",
    "Keep selected target precedence before orbwalker and selector fallback",
    "Use Wither only on a valid reachable champion and track its five-second state",
    "Predict Spirit Fire placement and reject walls, enemy turrets and overcrowded endpoints",
    "Track Spirit Fire zone ownership, duration and armor reduction by buff events",
    "Reserve Fury of the Sands for low-health defense, hard crowd control or multi-target commit",
    "Reject invulnerable, untargetable, spell-shielded and uncertain targets",
    "Preserve attack windup unless the Q or reactive cast is explicitly justified",
    "Yield after observed manual Q, W, E or R ownership",
    "Respect mana thresholds and do not spend the ultimate during ordinary farming",
    "Cover Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes",
    "Automatic mode is restricted to threat response and defensive Wither",
    "Keep Q damage, stack rewards, zone safety and ultimate gates independently testable",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Nasus";
    controller.OwnsDecisionLoop = true;
    controller.ControllerId = "champion.kuroaio.ai.nasus.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AINasus.md";
    controller.ImplementationSummary =
        "Stack-aware Q attack ownership, Wither peel, safe Spirit Fire placement and defensive Fury of the Sands reconciliation.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
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
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Nasus
