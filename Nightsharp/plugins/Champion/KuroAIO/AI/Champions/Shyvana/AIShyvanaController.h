#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIShyvana.h"
#include "AIShyvanaGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Shyvana {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline FormState CurrentForm = FormState::Human;
inline FlightState CurrentFlight = FlightState::Ready;
inline QResetState CurrentQ = QResetState::Ready;
inline float Fury = 0.0f;
inline int LastFuryTick = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int MarkTick = 0;
inline int MarkTargetId = 0;
inline int FlightTargetId = 0;
inline Vector3 FlightEndpoint{};
inline int ManualOwnershipUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline std::array<int, 4> LastCastTick{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        (reactive || Now() - LastCastTick[static_cast<std::size_t>(slot)] >= 45);
}

inline bool PreserveAttack(bool reactive) {
    return !reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool DragonFormActive(const AIHeroClient& player) {
    return player.IsValid() && (player.HasBuff("ShyvanaDragonForm") ||
        player.HasBuff("ShyvanaRDragonForm") || player.HasBuff("ShyvanaTransform"));
}

inline float ObservedFury(const AIHeroClient& player) {
    if (!player.IsValid()) return 0.0f;
    const float resource = CurrentResource(100.0f);
    const float passive = static_cast<float>(std::max(0, player.GetBuffCount("ShyvanaPassive")));
    const float dragon = static_cast<float>(std::max(0, player.GetBuffCount("ShyvanaR")));
    return ClampFury(std::max(resource, std::max(passive, dragon)));
}

inline bool Marked(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) && (target.HasBuff("ShyvanaE" ) ||
        target.HasBuff("ShyvanaEFireball") || target.HasBuff("ShyvanaEInDragonForm"));
}

inline bool SafeEndpoint(const Vector3& endpoint, bool defensive, bool fleeing, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero()) return false;
    const bool wall = SDK::NavMesh::IsWall(endpoint);
    const bool turret = Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position());
    return DragonEndpointSafe(wall, turret, Engine::CountEnemiesAt(endpoint, 350.0f),
        Slider(TacticsMenu, "MaxCommitEnemies", 2), lethal, defensive, fleeing);
}

inline bool ManaGate(Mode mode, bool urgent) {
    const float reserve = static_cast<float>(
        mode == Mode::Harass ? Slider(TacticsMenu, "HarassFuryReserve", 15) :
        (mode == Mode::Jungle ? Slider(FarmMenu, "JungleFuryReserve", 5) :
         (mode == Mode::LaneClear || mode == Mode::LastHit ?
              Slider(FarmMenu, "LaneFuryReserve", 8) : 0)));
    return urgent || Fury >= reserve;
}

inline float DamageFor(const AIHeroClient& target, int slot) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    if (slot == 0) return player.CalculatePhysicalDamage(target,
        QDamage(SpellRank(0), player.TotalAttackDamage(), player.BonusAttackDamage()));
    if (slot == 1) return player.CalculatePhysicalDamage(target,
        WTickDamage(SpellRank(1), player.BonusAttackDamage()) * 3.0f);
    if (slot == 2) return player.CalculateMagicDamage(target,
        EDamage(SpellRank(2), player.AP(), player.BonusAttackDamage()));
    return player.CalculateMagicDamage(target,
        RImpactDamage(SpellRank(3), player.AP(), player.BonusAttackDamage()));
}

inline bool CastTwinBite(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || PreserveAttack(reactive) ||
        !ManaGate(mode, reactive) || !Ready(0, mode, reactive) ||
        player.Position().Distance2D(target.Position()) > kQReach + target.BoundingRadius()) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    CurrentQ = QResetState::Armed;
    QCastTick = LastCastTick[0] = Now();
    Fury = FuryAfterAttack(Fury, CurrentForm);
    return true;
}

inline bool CastBurnout(const AIHeroClient& target, Mode mode, bool reactive = false,
                        bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PreserveAttack(reactive) || !ManaGate(mode, reactive) ||
        !Ready(1, mode, reactive)) return false;
    if (Engine::ValidEnemy(target) && player.Position().Distance2D(target.Position()) >
        kWRadius + target.BoundingRadius() && !fleeing) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WCastTick = LastCastTick[1] = Now();
    return true;
}

inline bool CastFlameBreath(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || PreserveAttack(reactive) ||
        !ManaGate(mode, reactive) || !Ready(2, mode, reactive)) return false;
    const Vector3 predicted = PredictPosition(target, 0.25f);
    if (!ProjectileReachable(player.Position(), predicted, target.BoundingRadius()) ||
        ProjectileWallBlocksFromPlayer(predicted, kEWidth) ||
        !ProjectileCollision(player.Position(), predicted, predicted, kEWidth,
                             target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(2, predicted)) return false;
    ECastTick = LastCastTick[2] = Now();
    MarkTargetId = static_cast<int>(target.NetworkId());
    MarkTick = Now();
    return true;
}

inline bool CastDragonDescent(const AIHeroClient& target, Mode mode, bool reactive = false,
                              bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || PreserveAttack(reactive) ||
        !ManaGate(mode, reactive) || CurrentForm == FormState::Dragon ||
        CurrentFlight == FlightState::Flying || !CanCastR(Fury, CurrentForm, true) ||
        !Ready(3, mode, reactive)) return false;
    const Vector3 predicted = fleeing
        ? player.Position() + SharedGeometry::Direction2D(target.Position(), player.Position()) * kRRange
        : PredictPosition(target, 0.70f);
    if (!predicted.IsValid() || predicted.IsZero()) return false;
    const bool lethal = Lethal(target, DamageFor(target, 3));
    const bool commit = Engine::CountEnemiesAt(predicted, kRImpactRadius) >=
        Slider(TacticsMenu, "MinimumRTargets", 2);
    if (!commit && !lethal && !fleeing && player.HealthPercent() > Slider(TacticsMenu, "DefensiveHealth", 35)) return false;
    if (!SafeEndpoint(predicted, reactive || player.HealthPercent() <= 35.0f, fleeing, lethal)) return false;
    if (!Engine::ControllerCastPosition(3, predicted)) return false;
    CurrentFlight = FlightState::Flying;
    FlightEndpoint = predicted;
    FlightTargetId = static_cast<int>(target.NetworkId());
    RCastTick = LastCastTick[3] = Now();
    Fury = ClampFury(Fury - kRMinimumFury);
    return true;
}

inline bool ObjectiveSpell(Mode mode, const AIMinionClient& objective) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !objective.IsValid() || !ManaGate(mode, false) ||
        player.Position().Distance2D(objective.Position()) > kAttackReach + objective.BoundingRadius()) return false;
    if (Ready(0, mode) && !PreserveAttack(false) && Engine::ControllerCastUnit(0, objective)) {
        CurrentQ = QResetState::Armed;
        QCastTick = LastCastTick[0] = Now();
        return true;
    }
    return false;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CurrentForm == FormState::Human &&
        (CastDragonDescent(target, Mode::Combo) || CastFlameBreath(target, Mode::Combo))) return;
    if (CastBurnout(target, Mode::Combo)) return;
    if (CastTwinBite(target, Mode::Combo)) return;
    (void)CastFlameBreath(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || Fury < Slider(TacticsMenu, "HarassFuryReserve", 15)) return;
    if (CastFlameBreath(target, Mode::Harass)) return;
    if (CastBurnout(target, Mode::Harass)) return;
    (void)CastTwinBite(target, Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ManaGate(mode, mode == Mode::LastHit)) return;
    if (mode == Mode::Jungle) {
        const auto objective = SelectJungleTarget(kWRadius, 0.15f, 100000.0f);
        if (objective.IsValid() && ObjectiveSpell(mode, objective)) return;
    }
    if (Engine::ValidEnemy(target) && CurrentForm == FormState::Human &&
        (CastFlameBreath(target, mode) || CastBurnout(target, mode) || CastTwinBite(target, mode))) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CurrentForm == FormState::Human &&
        (CastDragonDescent(target, Mode::Flee, true, true) || CastBurnout(target, Mode::Flee, true, true))) return;
    (void)CastBurnout(target, Mode::Flee, true, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (CurrentForm == FormState::Human && player.HealthPercent() <= 32.0f &&
        Engine::ValidEnemy(target) && CastDragonDescent(target, Mode::Automatic, true, true)) return;
    if (Engine::ValidEnemy(target) && Marked(target) && player.HealthPercent() <= 42.0f)
        (void)CastBurnout(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    const FormState observed = DragonFormActive(player) ? FormState::Dragon : FormState::Human;
    if (observed != CurrentForm) {
        CurrentForm = observed;
        if (CurrentForm == FormState::Dragon) CurrentFlight = FlightState::Landed;
        else CurrentFlight = FlightState::Ready;
    }
    Fury = ObservedFury(player);
    if (LastFuryTick == 0) LastFuryTick = now;
    else {
        const int tickInterval = CurrentForm == FormState::Dragon ? 150 : 1500;
        if (now > LastFuryTick + tickInterval) {
            Fury = FuryAfterTick(Fury, CurrentForm, now - LastFuryTick);
            LastFuryTick = now;
        }
    }
    if (CurrentQ == QResetState::Armed && !InQResetWindow(CurrentQ, now - QCastTick)) CurrentQ = QResetState::Ready;
    if (CurrentFlight == FlightState::Flying && now - RCastTick >= kRFlightMs) CurrentFlight = FlightState::Landed;
    if (MarkTargetId != 0 && now - MarkTick >= kEMarkDurationMs) MarkTargetId = 0;
    if (CurrentForm == FormState::Dragon && now - RCastTick >= kDragonDurationMs && !player.HasBuff("ShyvanaDragonForm"))
        CurrentForm = FormState::Human;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = PreferredEnemyTarget(selected, mode == Mode::Flee ? kRRange : kERange);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode, target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Shyvana dragon tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Shyvana fury farming"));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at dragon endpoint", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("MinimumRTargets", "Minimum dragon impact targets", 2, 1, 5));
    TacticsMenu->Add(new MenuSlider("DefensiveHealth", "Emergency dragon-flight health percent", 35, 0, 100));
    TacticsMenu->Add(new MenuSlider("HarassFuryReserve", "Fury reserve for harass", 15, 0, 100));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve attack windup", true));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast protection (ms)", 650, 0, 2000));
    FarmMenu->Add(new MenuSlider("LaneFuryReserve", "Fury reserve for lane clear", 8, 0, 100));
    FarmMenu->Add(new MenuSlider("JungleFuryReserve", "Fury reserve for jungle", 5, 0, 100));
}

inline void OnLoad() {
    CurrentForm = FormState::Human;
    CurrentFlight = FlightState::Ready;
    CurrentQ = QResetState::Ready;
    Fury = 0.0f;
    LastFuryTick = QCastTick = WCastTick = ECastTick = RCastTick = MarkTick = 0;
    MarkTargetId = FlightTargetId = ManualOwnershipUntil = LastAutoTargetId = LastAutoTick = 0;
    FlightEndpoint = {};
    LastCastTick.fill(0);
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    FarmMenu = nullptr;
    OnLoad();
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (args.Slot >= 0 && args.Slot < 4) {
            if (!Engine::WasControllerCast(args.Slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 650);
            LastCastTick[static_cast<std::size_t>(args.Slot)] = now;
            if (args.Slot == 0) { CurrentQ = QResetState::Armed; QCastTick = now; }
            else if (args.Slot == 1) WCastTick = now;
            else if (args.Slot == 2) { ECastTick = now; MarkTick = now; }
            else { RCastTick = now; CurrentFlight = FlightState::Flying; }
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) ManualOwnershipUntil = 0;
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
    LastAutoTick = Now();
    Fury = FuryAfterAttack(Fury, CurrentForm);
    CurrentQ = QResetState::Consumed;
    CurrentQ = QResetState::Ready;
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (ControllerHelpers::AnyTextContains({args.BuffName}, {"shyvanadragon", "shyvanar"})) {
            CurrentForm = FormState::Dragon;
            CurrentFlight = FlightState::Landed;
        }
        if (ControllerHelpers::AnyTextContains({args.BuffName}, {"shyvanaq", "shyvanadoubleattack"})) {
            CurrentQ = QResetState::Armed;
            QCastTick = Now();
        }
    } else if (MarkTargetId != 0 && static_cast<int>(args.Sender.NetworkId) == MarkTargetId &&
               ControllerHelpers::AnyTextContains({args.BuffName}, {"shyvanae", "shyvanaburn"})) {
        MarkTick = Now();
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (ControllerHelpers::AnyTextContains({args.BuffName}, {"shyvanadragon", "shyvanar"})) {
        CurrentForm = FormState::Human;
        CurrentFlight = FlightState::Ready;
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime <= Game::Time()) OnBuffRemove(args);
    else OnBuffAdd(args);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
        LastAutoTick = Now();
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    Vector3 endpoint = args.End;
    (void)CaptureGapcloser(args, MarkTargetId, endpoint, ManualOwnershipUntil, kRRange, 1000);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, MarkTargetId, ManualOwnershipUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"shyvanar", "shyvanaw", "shyvanae"})) {
        if (args.Sender.NetworkId != 0) FlightTargetId = static_cast<int>(args.Sender.NetworkId);
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.NetworkId != 0 && static_cast<int>(args.Sender.NetworkId) == FlightTargetId)
        FlightTargetId = 0;
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"shyvanae", "shyvanafireball"})) {
        ECastTick = Now();
        if (MarkTargetId == 0) MarkTick = ECastTick;
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Fury of the Dragonborn resource reconciliation and human/dragon form state",
    "Twin Bite attack reset after windup with manual ownership and reset-window expiry",
    "Burnout movement speed, persistent burn radius and dragon-form enhancement",
    "Flame Breath prediction, projectile collision, wall rejection and mark lifetime",
    "Dragon's Descent fury threshold, flight endpoint and dragon-form commit",
    "Dragon endpoint wall, turret and enemy-count safety with lethal/defensive exceptions",
    "Fury gain from attacks and passive ticks with dragon-form fury drain",
    "Selected target precedence followed by orbwalker target fallback",
    "Combo, harass, lane clear, jungle objective, last-hit, flee and automatic policies",
    "Polling reconciliation across spell, buff, object and missile callbacks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Shyvana;
    controller.ControllerId = "champion.kuroaio.ai.shyvana.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIShyvana.md";
    controller.ImplementationSummary =
        "Owns Shyvana fury, form, Q reset, W burn, E mark/projectile and R flight state, "
        "with objective-aware farming and safe dragon endpoints.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Shyvana
