#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AILeonaGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Leona {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellSlotOrEventNameContainsAny;

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
inline int ProtectedAllyId = 0;
inline int ManualOwnershipUntil = 0;
inline int WActiveUntil = 0;
inline int EActiveUntil = 0;
inline int RActiveUntil = 0;
inline int QArmedUntil = 0;
inline int LastThreatUntil = 0;
inline int LastHardCcUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd{};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline Vector3 LastECast{};
inline Vector3 LastRCast{};

inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool ProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline AIHeroClient SelectEnemy(const AIHeroClient& selected, float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orbwalker = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orbwalker, range)) return orbwalker;
    return Engine::SelectTarget(range);
}

inline AIHeroClient SelectAlly(bool defensive) {
    const auto ally = SelectProtectionAlly(
        1200.0f, ProtectedAllyId, ProtectedAllyId == 0 ? 0 : Now() + 450);
    if (Engine::ValidAlly(ally, 1200.0f)) {
        ProtectedAllyId = static_cast<int>(ally.NetworkId());
        return ally;
    }
    if (defensive) return GameObjects::Player();
    return {};
}

inline bool SafePoint(const Vector3& point, bool defensive = false,
                      bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !point.IsValid() || SDK::NavMesh::IsWall(point)) {
        return false;
    }
    const int enemies = Engine::CountEnemiesAt(point, 260.0f);
    const int maximum = Slider(EMenu, defensive ? "DefensiveMaxEnemies" :
                               "MaxLandingEnemies", defensive ? 4 : 3);
    if (enemies > maximum) return false;
    if (Engine::UnderEnemyTurret(point) &&
        !Engine::UnderEnemyTurret(player.Position()) && !defensive && !lethal) {
        return false;
    }
    return true;
}

inline bool AllyCanFollow(const AIHeroClient& ally, const Vector3& endpoint) {
    const auto player = GameObjects::Player();
    return Engine::ValidAlly(ally, 1200.0f) && player.IsValid() &&
        ally.NetworkId() != player.NetworkId() &&
        ally.Position().Distance2D(endpoint) <=
            Slider(TacticsMenu, "AllyFollowRange", 900);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 160.0f) ||
        ProtectedTarget(target) || !Ready(0, mode) || !Throttle(0) ||
        target.Position().Distance2D(player.Position()) > 150.0f) return false;
    const bool urgent = reactive || LastHardCcUntil > Now() ||
        static_cast<int>(target.NetworkId()) == GapcloserTargetId;
    if (!QResetAllowed(true, LastAutoTick >= Now() - 220, urgent,
                       PreserveAttack(reactive))) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    LastCastTick[0] = Now();
    QArmedUntil = Now() + 520;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive)) return false;
    const bool contact = Engine::ValidEnemy(target, kWRadius + 80.0f) ||
        Engine::CountEnemiesAt(player.Position(), kWRadius) > 0;
    if (!contact || (!reactive && Engine::UnderEnemyTurret(player.Position()) &&
                     Engine::CountEnemiesAt(player.Position(), kWRadius) < 2)) {
        return false;
    }
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    WActiveUntil = Now() + 5000;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange) ||
        ProtectedTarget(target) || !Ready(2, mode) || !Throttle(2, 85) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, 0.25f);
    std::vector<ZenithBody> bodies;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kERange)) continue;
        bodies.push_back({enemy.Position(), enemy.BoundingRadius(),
                          static_cast<int>(enemy.NetworkId()), true, true, true});
    }
    const ZenithContact first = FirstZenithChampion(
        player.Position(), aim, bodies, kERange, kEWidth);
    if (!first.Hit || first.BodyId != static_cast<int>(target.NetworkId()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kEWidth * 0.5f) ||
        !SafePoint(first.Position, reactive)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    EActiveUntil = Now() + 900;
    LastECast = first.Position;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool peel = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 120) ||
        PreserveAttack(reactive)) return false;
    if (!Engine::ValidEnemy(target, kRRange)) return false;
    const auto ally = SelectAlly(peel || mode == Mode::Automatic);
    const Vector3 predicted = PredictPosition(target, kRDelaySeconds);
    if (!predicted.IsValid() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(predicted, kRRadius) ||
        !SolarFlareHits(predicted, predicted, target.BoundingRadius())) return false;
    const int enemies = Engine::CountEnemiesAt(predicted, kRRadius);
    const bool lethal = target.HealthPercent() <=
        Slider(RMenu, "LethalHealthPercent", 26);
    const bool follow = Engine::ValidAlly(ally) &&
        AllyCanFollow(ally, predicted);
    const bool playerUnder = Engine::UnderEnemyTurret(player.Position());
    const bool safe = SolarFlareSafe(
        follow, peel, enemies, Slider(RMenu, "MaximumEnemies", 4),
        Engine::UnderEnemyTurret(predicted), lethal, playerUnder);
    if (!safe || (!peel && enemies < Slider(RMenu, "MinimumTargets", 2) && !lethal)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(3, predicted)) return false;
    LastCastTick[3] = Now();
    RActiveUntil = Now() + 900;
    LastRCast = predicted;
    return true;
}

inline bool DefensiveAutomatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const auto ally = SelectAlly(true);
    const bool threatenedAlly = Engine::ValidAlly(ally, 900.0f) &&
        ally.HealthPercent() <= Slider(TacticsMenu, "AllyHealthThreshold", 48) &&
        Engine::CountEnemiesAt(ally.Position(), 650.0f) > 0;
    const bool threatenedSelf = player.HealthPercent() <=
        Slider(TacticsMenu, "PlayerHealthThreshold", 36) &&
        Engine::CountEnemiesAt(player.Position(), 600.0f) > 0;
    if (threatenedAlly && Engine::ValidEnemy(target, kRRange) &&
        AllyPeelSafe(true, true,
                     ally.Position().Distance2D(target.Position()) < kRRadius,
                     threatenedSelf, Engine::UnderEnemyTurret(ally.Position())) &&
        CastR(target, Mode::Automatic, true, true)) return true;
    if (threatenedSelf && CastW(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target, 160.0f) &&
        (threatenedAlly || threatenedSelf) && CastQ(target, Mode::Automatic, true)) return true;
    return false;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastR(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastQ(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() <
        Slider(TacticsMenu, "HarassMana", 58)) return;
    if (CastE(target, Mode::Harass)) return;
    if (CastW(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& pursuer) {
    if (Engine::ValidEnemy(pursuer, 160.0f) && CastQ(pursuer, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(pursuer, kRRange) && CastR(pursuer, Mode::Flee, true, true)) return;
    (void)CastW(pursuer, Mode::Flee, true);
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("LeonaW") || player.HasBuff("LeonaWActive")) {
        WActiveUntil = std::max(WActiveUntil, now + 250);
    } else if (WActiveUntil < now) {
        WActiveUntil = 0;
    }
    if (player.HasBuff("LeonaSolarFlare")) {
        RActiveUntil = std::max(RActiveUntil, now + 250);
    } else if (RActiveUntil < now) {
        RActiveUntil = 0;
    }
    if (EActiveUntil < now) EActiveUntil = 0;
    if (QArmedUntil < now) QArmedUntil = 0;
    if (ManualOwnershipUntil < now) ManualOwnershipUntil = 0;
    if (GapcloserExpireTick < now) GapcloserTargetId = 0;
    if (InterruptExpireTick < now) InterruptTargetId = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const auto player = GameObjects::Player();
    const AIHeroClient target = SelectEnemy(selected, mode == Mode::Flee ? 1000.0f : kRRange);
    if (ManualOwnershipUntil > Now()) return true;
    if (mode == Mode::Automatic && DefensiveAutomatic(target)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 900.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (player.IsValid() && player.ManaPercent() >=
            Slider(FarmMenu, "Mana", 35)) (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic: break;
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
            if (!Engine::WasControllerCast(slot)) {
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
            }
            LastCastTick[slot] = now;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    LastThreatUntil = std::max(LastThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) {
        LastHardCcUntil = std::max(LastHardCcUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "LeonaW")) WActiveUntil = Now() + 250;
    if (Engine::TextContains(args.BuffName, "SolarFlare")) RActiveUntil = Now() + 250;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "LeonaW")) WActiveUntil = 0;
    if (Engine::TextContains(args.BuffName, "SolarFlare")) RActiveUntil = 0;
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kERange, 0xFFEEA13Bu, 1.3f, 36);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFFFD35Au, 1.3f, 36);
    if (LastECast.IsValid() && !LastECast.IsZero())
        Drawing::DrawCircle(LastECast, 80.0f, 0xFFEEA13Bu, 1.0f, 24);
    if (LastRCast.IsValid() && !LastRCast.IsZero())
        Drawing::DrawCircle(LastRCast, kRRadius, 0xFFFFD35Au, 1.0f, 30);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("LeonaTactics", "Leona engage and peel tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("AllyHealthThreshold", "Protect ally below health %", 48, 10, 90));
    TacticsMenu->Add(new MenuSlider("PlayerHealthThreshold", "Defend self below health %", 36, 10, 90));
    TacticsMenu->Add(new MenuSlider("AllyFollowRange", "Required ally follow range", 900, 450, 1400));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 58, 10, 90));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Shield of Daybreak reset"));
    QMenu->Add(new MenuBool("ResetStun", "Reset attack into stun", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Eclipse resistance stance"));
    WMenu->Add(new MenuBool("UseBeforeEntry", "Arm stance before entry", true));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Zenith Blade first hit"));
    EMenu->Add(new MenuSlider("MaxLandingEnemies", "Maximum enemies at dash", 3, 0, 5));
    EMenu->Add(new MenuSlider("DefensiveMaxEnemies", "Defensive dash enemies", 4, 0, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Solar Flare prediction and AOE"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum engage targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at cast", 4, 1, 5));
    RMenu->Add(new MenuSlider("LethalHealthPercent", "Lethal single-target health %", 26, 5, 60));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("LeonaFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("LeonaCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw E/R ranges", false));
}

inline void OnLoad() {
    std::fill(LastCastTick.begin(), LastCastTick.end(), 0);
    LastAutoTargetId = LastAutoTick = ProtectedAllyId = 0;
    ManualOwnershipUntil = WActiveUntil = EActiveUntil = RActiveUntil = 0;
    QArmedUntil = LastThreatUntil = LastHardCcUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    GapcloserEnd = LastECast = LastRCast = {};
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    LastECast = LastRCast = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin Leona to Riot 26.15 and CommunityDragon 16.15 records",
    "Select explicit enemy, then orbwalker hero target, then engine fallback",
    "Reset Shield of Daybreak into the next attack only in a real windup or urgent stun",
    "Preserve valuable auto-attack windups unless Q stun or peel is urgent",
    "Arm Eclipse before Zenith Blade enters a dangerous enemy cluster",
    "Track Eclipse resistance stance from buff events and polling reconciliation",
    "Predict Zenith Blade and reject projectile-wall blocked lines",
    "Require the selected champion to be the first valid Zenith Blade champion hit",
    "Reject Zenith Blade landing under a turret or above enemy density limits",
    "Predict Solar Flare impact after its delay and include target radius",
    "Require Solar Flare AOE count, lethal payoff or ally-safe peel conversion",
    "Reject Solar Flare when ally follow-up or peel safety is absent",
    "Use Solar Flare to engage for an allied carry only when the carry can follow",
    "Use Solar Flare to peel a threatened ally without abandoning the player",
    "Gate every engage by turret state, enemy count, mobility hazards and resources",
    "Use distinct Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic branches",
    "Yield ownership after observed manual Q W E or R casts",
    "Reconcile hostile casts and hard crowd-control windows for automatic peel",
    "Expose process-spell, buff, attack, gapcloser, interrupt and polling callbacks",
    "Use deterministic Q W E and R damage and boundary geometry in standalone tests",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Leona;
    controller.ControllerId = "champion.kuroaio.ai.leona.support";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AILeona.md";
    controller.ImplementationSummary =
        "Peel-first Leona controller with Q attack-reset stun ownership, W resistance "
        "stance reconciliation, first-hit predicted E dash, and ally-safe Solar Flare "
        "prediction/AOE engage and peel gates.";
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
    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<
        &GapcloserTargetId, &GapcloserEnd, &GapcloserExpireTick, 900, 800>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<
        &InterruptTargetId, &InterruptExpireTick, 900, 120, 2200>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Leona
