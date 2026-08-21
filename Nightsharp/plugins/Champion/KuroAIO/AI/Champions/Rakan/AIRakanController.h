#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIRakanGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Rakan {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastEnemyThreatUntil = 0;
inline int LastHardCcThreatUntil = 0;
inline int RActiveUntil = 0;
inline int ELastCastTick = 0;
inline bool EReturnPending = false;
inline Vector3 LastRDirection{};
inline Vector3 LastWLanding{};

using ControllerHelpers::Now;

using ControllerHelpers::Ready;

inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

using ControllerHelpers::PreserveAttack;

inline bool ProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline AIHeroClient SelectEnemy(float range = kQRange) {
    return Engine::SelectTarget(range);
}

inline AIHeroClient SelectAlly(bool defensive = false) {
    const auto ally = SelectProtectionAlly(700.0f);
    if (Engine::ValidAlly(ally, 700.0f)) return ally;
    if (defensive) {
        const auto player = GameObjects::Player();
        if (player.IsValid()) return player;
    }
    return {};
}

inline bool SafeLanding(const Vector3& endpoint, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !LandingSafe(
            endpoint, Engine::CountEnemiesAt(endpoint, 260.0f),
            Engine::UnderEnemyTurret(endpoint), SDK::NavMesh::IsWall(endpoint),
            defensive ? Slider(EMenu, "DefensiveMaxEnemies", 3)
                      : Slider(EMenu, "MaxLandingEnemies", 2))) return false;
    return defensive || Engine::UnderEnemyTurret(player.Position()) ||
        !Engine::UnderEnemyTurret(endpoint);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange) ||
        ProtectedTarget(target) || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, 0.25f);
    if (!QHits(player.Position(), aim, target.Position(),
              target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f))
        return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange + 100.0f) ||
        ProtectedTarget(target) || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive)) return false;
    const Vector3 landing = WLandingPoint(
        player.Position(), PredictPosition(target, 0.25f));
    if (!SafeLanding(landing, defensive) ||
        !WKnockupHits(player.Position(), landing, target.Position(),
                      target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(1, landing)) return false;
    LastCastTick[1] = Now();
    LastWLanding = landing;
    return true;
}

inline bool CastE(const AIHeroClient& ally, Mode mode, bool reactive = false,
                  bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidAlly(ally, kEDashRange) ||
        ally.NetworkId() == player.NetworkId() || !Ready(2, mode) ||
        !Throttle(2, 90) || PreserveAttack(reactive)) return false;
    const Vector3 endpoint = AllyDashEndpoint(player.Position(), ally.Position());
    if (!SafeLanding(endpoint, defensive) ||
        !ReturnSafe(player.Position(), ally.Position(),
                    Engine::CountEnemiesAt(endpoint, 260.0f),
                    Engine::UnderEnemyTurret(endpoint),
                    SDK::NavMesh::IsWall(endpoint),
                    defensive ? 3 : Slider(EMenu, "MaxLandingEnemies", 2)))
        return false;
    if (!Engine::ControllerCastUnit(2, ally)) return false;
    LastCastTick[2] = ELastCastTick = Now();
    EReturnPending = !defensive;
    return true;
}
inline bool CastEReturn(const AIHeroClient& ally, Mode mode, bool reactive = true) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidAlly(ally, kEDashRange) ||
        ally.NetworkId() == player.NetworkId() || !Ready(2, mode) ||
        !Throttle(2, 90) || PreserveAttack(reactive)) return false;
    const Vector3 endpoint = AllyDashEndpoint(player.Position(), ally.Position());
    if (!SafeLanding(endpoint, true) ||
        !ReturnSafe(player.Position(), ally.Position(),
                    Engine::CountEnemiesAt(endpoint, 260.0f),
                    Engine::UnderEnemyTurret(endpoint),
                    SDK::NavMesh::IsWall(endpoint), 3)) return false;
    if (!Engine::ControllerCastUnit(2, ally)) return false;
    LastCastTick[2] = ELastCastTick = Now();
    EReturnPending = false;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 120) ||
        PreserveAttack(reactive)) return false;
    const int nearby = Engine::CountEnemiesAt(player.Position(), kRRadius);
    if (!defensive && !Engine::ValidEnemy(target, 900.0f)) return false;
    if (!defensive && nearby < Slider(RMenu, "MinimumTargets", 2) &&
        (!Engine::ValidEnemy(target) ||
         player.Position().Distance2D(target.Position()) > 700.0f)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    RActiveUntil = Now() + 4000;
    LastRDirection = Engine::ValidEnemy(target)
        ? Direction2D(player.Position(), target.Position())
        : mode == Mode::Flee
            ? Direction2D(player.Position(), Game::CursorPos()) : Vec3{};
    return true;
}

inline bool DefensiveAutomatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const auto ally = SelectAlly(true);
    const bool allyThreatened = Engine::ValidAlly(ally) &&
        ally.HealthPercent() <= Slider(TacticsMenu, "AllyHealthThreshold", 48) &&
        Engine::CountEnemiesAt(ally.Position(), 700.0f) > 0;
    const bool playerThreatened = player.HealthPercent() <=
        Slider(TacticsMenu, "PlayerHealthThreshold", 35) &&
        Engine::CountEnemiesAt(player.Position(), 700.0f) > 0;
    if (allyThreatened && CastE(ally, Mode::Automatic, true, true)) return true;
    if (Engine::ValidEnemy(target) &&
        (allyThreatened || playerThreatened) && CastQ(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && (LastHardCcThreatUntil > Now() ||
        playerThreatened) && CastW(target, Mode::Automatic, true, true)) return true;
    if ((LastEnemyThreatUntil > Now() || playerThreatened || allyThreatened) &&
        CastR(target, Mode::Automatic, true, true)) return true;
    return false;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    const auto ally = SelectAlly(false);
    if (CastR(target, Mode::Combo, false, false)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (Engine::ValidAlly(ally) && CastE(ally, Mode::Combo)) return;
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() <
        Slider(QMenu, "HarassMana", 52)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (CastW(target, Mode::Harass)) return;
}

inline void Flee(const AIHeroClient& pursuer) {
    const auto ally = SelectAlly(true);
    if (Engine::ValidAlly(ally) && CastE(ally, Mode::Flee, true, true)) return;
    if (Engine::ValidEnemy(pursuer) && CastW(pursuer, Mode::Flee, true, true)) return;
    if (CastR(pursuer, Mode::Flee, true, true)) return;
}

inline void ReconcileState() {
    const int now = Now();
    if (RActiveUntil <= now) RActiveUntil = 0;
    if (ELastCastTick > 0 && now - ELastCastTick > 2200) EReturnPending = false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("RakanR") || player.HasBuff("RakanRState"))
        RActiveUntil = std::max(RActiveUntil, now + 250);
    if (!player.HasBuff("RakanE") && now - ELastCastTick > 900)
        EReturnPending = false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    (void)selected;
    ReconcileState();
    const auto player = GameObjects::Player();
    const AIHeroClient target = SelectEnemy(mode == Mode::Flee ? 900.0f : 1000.0f);
    if (EReturnPending && mode != Mode::LaneClear &&
        mode != Mode::Jungle && mode != Mode::LastHit) {
        const auto returnAlly = SelectAlly(mode == Mode::Automatic ||
                                           mode == Mode::Flee);
        if (Engine::ValidAlly(returnAlly) &&
            CastEReturn(returnAlly, mode, mode == Mode::Automatic ||
                       mode == Mode::Flee)) return true;
    }
    if (mode == Mode::Automatic && DefensiveAutomatic(target)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 900.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (player.IsValid() && player.ManaPercent() >= Slider(FarmMenu, "Mana", 35))
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
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
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = now;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    LastEnemyThreatUntil = std::max(LastEnemyThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        LastHardCcThreatUntil = std::max(LastHardCcThreatUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "RakanR"))
        RActiveUntil = std::max(RActiveUntil, Now() + 250);
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "RakanR")) RActiveUntil = 0;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kWRange, 0xFFB968E0u, 1.5f, 36);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFFFC06Au, 1.5f, 36);
    if (LastWLanding.IsValid() && !LastWLanding.IsZero())
        Drawing::DrawCircle(LastWLanding, kWRadius, 0xFFCC66CCu, 1.0f, 30);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("RakanTactics", "Rakan support tactics"));
    TacticsMenu->Add(new MenuSlider("AllyHealthThreshold", "Protect ally below health %", 48, 10, 90));
    TacticsMenu->Add(new MenuSlider("PlayerHealthThreshold", "Defend self below health %", 35, 10, 90));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Gleaming Quill heal"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 52, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Grand Entrance"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Battle Dance safety"));
    EMenu->Add(new MenuSlider("MaxLandingEnemies", "Maximum enemies at landing", 2, 0, 5));
    EMenu->Add(new MenuSlider("DefensiveMaxEnemies", "Defensive landing enemies", 3, 0, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "The Quickness"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum charm targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("RakanFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("RakanCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw W/R ranges", false));
}

inline void OnLoad() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = 0;
    LastEnemyThreatUntil = LastHardCcThreatUntil = 0;
    RActiveUntil = ELastCastTick = 0;
    EReturnPending = false;
    LastRDirection = LastWLanding = {};
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    LastWLanding = LastRDirection = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15 metadata",
    "Select the explicit enemy before orbwalker and global target fallback",
    "Select a vulnerable, high-value ally for Battle Dance protection",
    "Use Q line contact and heal-window timing without inventing SDK state",
    "Reject Q through projectile walls and protected targets",
    "Use predicted W landing points for knockup and interrupt setups",
    "Reject W landings through walls, turrets or excessive enemy count",
    "Dash E to allies only when the endpoint remains a safe return route",
    "Preserve E charge intent and reconcile observed E/R buffs by polling",
    "Charm movement uses R self cast while retaining cursor direction intent",
    "Use R for multi-target engage, incoming hard CC or defensive peel",
    "Automatic mode is defensive and never starts an unsolicited fresh engage",
    "Combo layers R movement, W knockup, Q heal damage and E return",
    "Harass spends Q first and keeps mana floor before W poke",
    "Flee prioritizes ally E, then peel W and defensive R",
    "LaneClear Jungle and LastHit delegate to shared farm policy",
    "Expose spell, buff, attack and polling event callbacks",
    "Draw W/R safety ranges without changing decisions",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Rakan;
    controller.ControllerId = "champion.kuroaio.ai.rakan.support";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIRakan.md";
    controller.ImplementationSummary =
        "Ally-first Battle Dance routing, safe Grand Entrance landing, Q sustain and defensive charm movement.";
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

    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Rakan
