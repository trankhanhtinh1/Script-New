#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIXerath.h"
#include "AIXerathGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Xerath {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline std::array<int, 4> LastCastTick{};
inline int ManualOverrideUntil = 0;
inline int LastAutoTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptUntil = 0;
inline Vector3 GapcloserEndpoint{};

inline bool QCharging = false;
inline bool QOwned = false;
inline bool QInterrupted = false;
inline int QStartTick = 0;
inline int QTargetId = 0;
inline Vector3 QDirection{};

inline int LastWTargetId = 0;
inline Vector3 LastWCenter{};
inline bool LastWCenterHit = false;
inline int EStunnedTargetId = 0;
inline int EStunUntil = 0;

inline bool RChanneling = false;
inline bool ROwned = false;
inline bool RInterrupted = false;
inline int RStartTick = 0;
inline int RLastShotTick = -1;
inline int RTargetId = 0;
inline int RShotsRemaining = 0;
inline int RShotsFired = 0;
inline Vector3 RLastAim{};

inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || IsCommonUntargetableOrImmune(target) ||
           target.IsInvulnerable() || target.HasBuff("BansheesVeil") ||
           target.HasBuff("EdgeOfNight") || target.HasBuff("SivirE") ||
           target.HasBuff("NocturneShroudofDarkness") || target.HasBuff("MorganaE") ||
           target.HasBuff("BlackShield") || target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("KayleR");
}

inline bool Throttle(int slot, int delay = 70) {
    return slot >= 0 && slot < 4 && LastCastTick[static_cast<std::size_t>(slot)] + delay <= Now();
}
inline bool PreserveAttack(int slot, bool reactive) {
    return !reactive && slot != 3 && Orbwalker::IsWindingUp() &&
           ControllerHelpers::Bool(Engine::HumanMenu, "PreserveAttacks", true);
}
inline bool RuntimeQCharging() {
    const auto player = GameObjects::Player();
    return (Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsCharging()) ||
           (player.IsValid() && (player.HasBuff("XerathArcanopulseChargeUp") ||
                                 player.HasBuff("XerathQSoundBuff")));
}
inline bool RuntimeRChanneling() {
    const auto player = GameObjects::Player();
    return (Engine::RuntimeSpells[3] && Engine::RuntimeSpells[3]->IsCharging()) ||
           (player.IsValid() && (player.HasBuff("XerathRRampUp") ||
                                 player.HasBuff("XerathLocusOfPower2")));
}
inline bool IsOwnChannelUnsafe() {
    const auto player = GameObjects::Player();
    return !player.IsValid() || ControllerHelpers::PlayerMobilityLocked() ||
           (Engine::UnderEnemyTurret(player.Position()) &&
            Engine::CountEnemiesAt(player.Position(), 700.0f) >
                ControllerHelpers::Slider(UltimateMenu, "MaxChannelEnemies", 2));
}

inline float QDamage(const AIBaseClient& target) {
    if (!target.IsValid()) return 0.0f;
    const float runtime = Engine::RuntimeSpells[0]
        ? std::max(0.0f, Engine::RuntimeSpells[0]->GetDamage(target)) : 0.0f;
    return runtime > 1.0f ? runtime : QRawDamage(SpellRank(0), ControllerHelpers::AP());
}
inline float WDamage(const AIBaseClient& target, bool center) {
    if (!target.IsValid()) return 0.0f;
    const float runtime = Engine::RuntimeSpells[1]
        ? std::max(0.0f, Engine::RuntimeSpells[1]->GetDamage(target)) : 0.0f;
    if (runtime > 1.0f) return center ? runtime * 1.667f : runtime;
    return center ? WCenterDamage(SpellRank(1), ControllerHelpers::AP())
                  : WRawDamage(SpellRank(1), ControllerHelpers::AP());
}
inline float EDamage(const AIBaseClient& target) {
    if (!target.IsValid()) return 0.0f;
    const float runtime = Engine::RuntimeSpells[2]
        ? std::max(0.0f, Engine::RuntimeSpells[2]->GetDamage(target)) : 0.0f;
    return runtime > 1.0f ? runtime : ERawDamage(SpellRank(2), ControllerHelpers::AP());
}
inline float RDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const float runtime = Engine::RuntimeSpells[3]
        ? std::max(0.0f, Engine::RuntimeSpells[3]->GetDamage(target)) : 0.0f;
    return runtime > 1.0f ? runtime : RShotDamage(SpellRank(3), ControllerHelpers::AP(), RShotsFired);
}

inline bool BuildAim(const AIHeroClient& target, float delay, Vector3& aim) {
    aim = {};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return false;
    const auto prediction = Engine::RuntimeSpells[0]
        ? Engine::RuntimeSpells[0]->GetPrediction(target) : SDK::PredictionOutput{};
    aim = prediction.GetCastPosition();
    if (aim.IsZero() || !aim.IsValid()) aim = PredictPosition(target, delay);
    return aim.IsValid() && !aim.IsZero();
}

inline bool StartQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || QCharging || RChanneling || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kQMaxRange + 50.0f) || !Ready(0, mode) ||
        !Throttle(0) || PreserveAttack(0, reactive) || ManualOverrideUntil > Now()) return false;
    Vector3 aim{};
    if (!BuildAim(target, 0.25f, aim)) return false;
    const Vector3 direction = Direction2D(player.Position(), aim);
    if (direction.IsZero()) return false;
    if (!Engine::ControllerCastPosition(0, player.Position() + direction * kQMinRange)) return false;
    QCharging = QOwned = true;
    QInterrupted = false;
    QStartTick = LastCastTick[0] = Now();
    QTargetId = static_cast<int>(target.NetworkId());
    QDirection = direction;
    return true;
}
inline bool ReleaseQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !QCharging || !QOwned || QInterrupted || RChanneling ||
        TargetBlocked(target) || !Engine::ValidEnemy(target, kQMaxRange + 50.0f) ||
        !ControllerHelpers::SpellEnabled(0, mode) ||
        !QCanRelease(true, (Now() - QStartTick) / 1000.0f)) return false;
    Vector3 aim{};
    if (!BuildAim(target, 0.25f, aim)) return false;
    const Vector3 direction = Direction2D(player.Position(), aim);
    if (direction.IsZero()) return false;
    const float range = QRangeForCharge((Now() - QStartTick) / 1000.0f);
    const Vector3 endpoint = player.Position() + direction * std::min(range, player.Position().Distance2D(aim));
    if (!QLineHits(player.Position(), endpoint, aim, target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kQWidth * 0.5f) ||
        !Engine::ControllerCastPosition(0, endpoint)) return false;
    QCharging = QOwned = false;
    QStartTick = QTargetId = 0;
    QDirection = {};
    LastCastTick[0] = Now();
    (void)reactive;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || QCharging || RChanneling || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kWRange + 40.0f) || !Ready(1, mode) ||
        !Throttle(1) || PreserveAttack(1, reactive)) return false;
    Vector3 aim{};
    if (!BuildAim(target, kWDelay, aim) || player.Position().Distance2D(aim) > kWRange + target.BoundingRadius() ||
        !WOuterHits(aim, PredictPosition(target, kWDelay), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kWOuterRadius * 0.08f)) return false;
    const bool center = WCenterHits(aim, PredictPosition(target, kWDelay), target.BoundingRadius());
    if (!center && mode == Mode::Harass && target.HealthPercent() > 70.0f) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    LastWTargetId = static_cast<int>(target.NetworkId());
    LastWCenter = aim;
    LastWCenterHit = center;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || QCharging || RChanneling || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kERange + target.BoundingRadius()) || !Ready(2, mode) ||
        !Throttle(2) || PreserveAttack(2, reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    Vector3 aim = prediction.GetCastPosition();
    if (aim.IsZero() || !aim.IsValid()) aim = PredictPosition(target, kEDelay);
    const Vector3 endpoint = player.Position() + Direction2D(player.Position(), aim) *
        std::min(kELineRange, player.Position().Distance2D(aim));
    if (endpoint.IsZero() || !ELineHits(player.Position(), endpoint, aim, target.BoundingRadius()) ||
        !prediction.CollisionObjects.empty() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kEWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    EStunnedTargetId = static_cast<int>(target.NetworkId());
    EStunUntil = Now() + static_cast<int>(EStunDuration(player.Position().Distance2D(aim)) * 1000.0f);
    return true;
}

inline bool StartR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || QCharging || RChanneling || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kRRange) || !Ready(3, mode) || !Throttle(3, 170) ||
        PreserveAttack(3, reactive) || ManualOverrideUntil > Now() || IsOwnChannelUnsafe()) return false;
    const bool lethal = Lethal(target, RDamage(target));
    const bool executeWindow = target.HealthPercent() <=
        ControllerHelpers::Slider(UltimateMenu, "RTargetHP", 48);
    if (!reactive && !lethal && !executeWindow && mode != Mode::Flee) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    const RAmmoState state = BeginR(SpellRank(3));
    RChanneling = ROwned = true;
    RInterrupted = false;
    RStartTick = LastCastTick[3] = Now();
    RLastShotTick = -1;
    RTargetId = static_cast<int>(target.NetworkId());
    RShotsRemaining = state.Remaining;
    RShotsFired = 0;
    return true;
}
inline bool FireR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !RChanneling || !ROwned || RInterrupted || IsOwnChannelUnsafe() ||
        !Engine::ValidEnemy(target, kRRange) || RShotsRemaining <= 0 ||
        !RCanFire({RShotsRemaining, RShotsRemaining + RShotsFired, RShotsFired, true}, Now(), RLastShotTick)) return false;
    const Vector3 aim = PredictPosition(target, kRTrajectorySeconds);
    if (aim.IsZero() || player.Position().Distance2D(aim) > kRRange ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kRRadius)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    --RShotsRemaining;
    ++RShotsFired;
    RLastShotTick = LastCastTick[3] = Now();
    RLastAim = aim;
    (void)mode;
    (void)reactive;
    return true;
}

inline bool Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || QCharging || RChanneling || player.ManaPercent() <
        ControllerHelpers::Slider(FarmMenu, mode == Mode::Jungle ? "JungleMana" : "LaneMana", 38)) return false;
    const auto& units = mode == Mode::Jungle ? GameObjects::Jungle() : GameObjects::EnemyMinions();
    for (const auto& unit : units) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) > kWRange) continue;
        if (mode == Mode::LastHit && player.CalculateMagicDamage(unit, WDamage(unit, true)) < unit.Health()) continue;
        Vector3 aim = PredictPosition(unit, kWDelay);
        if (Ready(1, mode) && Throttle(1) && WOuterHits(aim, unit.Position(), unit.BoundingRadius()) &&
            Engine::ControllerCastPosition(1, aim)) {
            LastCastTick[1] = Now();
            return true;
        }
    }
    return false;
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (QCharging) {
        const bool runtime = RuntimeQCharging();
        if ((!runtime && now - QStartTick > 350) || QInterrupted || now - QStartTick > 4500) {
            QCharging = QOwned = false;
            QTargetId = 0;
            QDirection = {};
        }
    }
    if (RChanneling) {
        const bool runtime = RuntimeRChanneling();
        if (!runtime && now - RStartTick > 450) RChanneling = ROwned = false;
        if (IsOwnChannelUnsafe()) RInterrupted = true;
        if (RShotsRemaining <= 0 || now - RStartTick >= static_cast<int>((kRChannelSeconds + 0.25f) * 1000.0f))
            RChanneling = ROwned = false;
    }
    if (EStunUntil <= now) EStunnedTargetId = EStunUntil = 0;
    if (GapcloserUntil <= now) GapcloserTargetId = GapcloserUntil = 0;
    if (InterruptUntil <= now) InterruptTargetId = InterruptUntil = 0;
    if (ManualOverrideUntil <= now) ManualOverrideUntil = 0;
    (void)player;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (RChanneling) { (void)FireR(target, Mode::Combo); return; }
    if (StartR(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (QCharging) (void)ReleaseQ(target, Mode::Combo);
    else (void)StartQ(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || GameObjects::Player().ManaPercent() <
        ControllerHelpers::Slider(TacticsMenu, "HarassMana", 48)) return;
    if (CastE(target, Mode::Harass)) return;
    if (CastW(target, Mode::Harass)) return;
    if (QCharging) (void)ReleaseQ(target, Mode::Harass);
    else (void)StartQ(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastE(target, Mode::Flee, true)) return;
    if (RChanneling) (void)FireR(target, Mode::Flee, true);
    else if (QCharging) (void)ReleaseQ(target, Mode::Flee, true);
}
inline void Automatic(const AIHeroClient& target) {
    if (InterruptUntil > Now() || GapcloserUntil > Now()) {
        const auto threat = HeroByNetworkId(InterruptTargetId != 0 ? InterruptTargetId : GapcloserTargetId);
        if (Engine::ValidEnemy(threat, kERange)) { (void)CastE(threat, Mode::Automatic, true); return; }
    }
    if (RChanneling) { (void)FireR(target, Mode::Automatic, true); return; }
    if (Engine::ValidEnemy(target) && Lethal(target, RDamage(target))) (void)StartR(target, Mode::Automatic, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOverrideUntil > Now()) return true;
    const auto target = PreferredEnemyTarget(selected, kRRange);
    if (mode == Mode::Automatic) Automatic(target);
    else if (mode == Mode::Flee) Flee(target);
    else if (mode == Mode::Combo) Combo(target);
    else if (mode == Mode::Harass) Harass(target);
    else if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) (void)Farm(mode);
    return true;
}

inline void OnLoad() {
    TacticsMenu = FarmMenu = UltimateMenu = nullptr;
    LastCastTick = {};
    ManualOverrideUntil = LastAutoTargetId = GapcloserTargetId = GapcloserUntil = 0;
    InterruptTargetId = InterruptUntil = 0; GapcloserEndpoint = {};
    QCharging = QOwned = QInterrupted = false; QStartTick = QTargetId = 0; QDirection = {};
    LastWTargetId = 0; LastWCenter = {}; LastWCenterHit = false;
    EStunnedTargetId = EStunUntil = 0;
    RChanneling = ROwned = RInterrupted = false; RStartTick = 0; RLastShotTick = -1;
    RTargetId = RShotsRemaining = RShotsFired = 0; RLastAim = {};
}
inline void OnUnload() { OnLoad(); }

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        const bool owned = slot >= 0 && slot < 4 && Engine::WasControllerCast(slot);
        if (!owned) ManualOverrideUntil = now + ControllerHelpers::Slider(TacticsMenu, "ManualOwnershipMs", 600);
        if (slot >= 0 && slot < 4) LastCastTick[static_cast<std::size_t>(slot)] = now;
        if (slot == 0) { QCharging = true; QOwned = owned; QStartTick = now; QInterrupted = false; }
        if (slot == 3) { RChanneling = true; ROwned = owned; RStartTick = now; }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args, 240.0f, 95.0f, 250, 250, 250, 1500, 450);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        GapcloserTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        GapcloserUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
    }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack && args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId);
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "ArcanopulseChargeUp")) { QCharging = true; QStartTick = Now(); }
        if (Engine::TextContains(args.BuffName, "RRampUp") || Engine::TextContains(args.BuffName, "LocusOfPower2")) { RChanneling = true; RStartTick = Now(); }
    } else if (Engine::TextContains(args.BuffName, "XerathMageSpear")) {
        EStunnedTargetId = static_cast<int>(args.Sender.NetworkId); EStunUntil = Now() + 2250;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "ArcanopulseChargeUp")) QCharging = QOwned = false;
    if (IsLocalPlayer(args.Sender) && (Engine::TextContains(args.BuffName, "RRampUp") || Engine::TextContains(args.BuffName, "LocusOfPower2"))) RChanneling = ROwned = false;
    if (static_cast<int>(args.Sender.NetworkId) == EStunnedTargetId && Engine::TextContains(args.BuffName, "XerathMageSpear")) EStunnedTargetId = EStunUntil = 0;
}
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "RRampUp")) RChanneling = true;
    if (static_cast<int>(args.Sender.NetworkId) == EStunnedTargetId && args.EndTime <= Game::Time()) EStunnedTargetId = EStunUntil = 0;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
    if (QCharging || RChanneling) args.Process = false;
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint, GapcloserUntil, 1200.0f, 1400);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptUntil, 1050, 250, 7000);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::MissileEventIsLocal(args) && Engine::TextContains(args.MissileName, "Xerath")) RLastShotTick = Now();
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("XerathTactics", "Xerath charge and safety policy"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 600, 150, 1400));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 0, 100));
    FarmMenu = root->AddSubMenu(new Menu("XerathFarm", "Charged beam and Eye farming"));
    FarmMenu->Add(new MenuSlider("LaneMana", "Lane-clear mana percent", 38, 0, 100));
    FarmMenu->Add(new MenuSlider("JungleMana", "Jungle mana percent", 30, 0, 100));
    UltimateMenu = root->AddSubMenu(new Menu("XerathUltimate", "Artillery channel safety"));
    UltimateMenu->Add(new MenuSlider("RTargetHP", "Allow artillery below target health percent", 48, 1, 100));
    UltimateMenu->Add(new MenuSlider("MaxChannelEnemies", "Maximum enemies near Xerath while channeling", 2, 0, 5));
}

inline constexpr const char* Scenarios[] = {
    "Q four-second charge, 750-to-1125 range interpolation, release prediction and beam collision",
    "W outer 250 radius impact with 100 radius center sweet spot and 1.667 damage multiplier",
    "E 70-width first-collision line prediction, 1600 missile safety and distance-scaled stun",
    "R ten-second channel, 4/5/6 artillery ammo, 0.6 second trajectory and prior-hit ramp damage",
    "R manual ownership, anti-interrupt mobility/turret/enemy-count safety and polling reconciliation",
    "Selected target precedence, orbwalker fallback, auto-attack windup ownership and shield-aware lethal checks",
    "Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic interrupt/gapcloser routes",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Xerath;
    controller.ControllerId = "champion.kuroaio.ai.xerath.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIXerath.md";
    controller.ImplementationSummary =
        "Charge-aware Arcanopulse, center-sweet-spot Eye, first-collision Orb and ammo-tracked artillery channel with manual and anti-interrupt safety.";
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
    controller.OnBuffUpdate = &OnBuffUpdate;
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

} // namespace Plugins::KuroAIO::AI::Controllers::Xerath
