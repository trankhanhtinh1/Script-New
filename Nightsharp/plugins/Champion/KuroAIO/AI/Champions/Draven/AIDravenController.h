#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIDravenGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstring>
#include <initializer_list>

namespace Plugins::KuroAIO::AI::Controllers::Draven {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::Now;
using ControllerHelpers::SpellEventNameContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* AxeMenu = nullptr;
inline Menu* ReturnMenu = nullptr;
inline Menu* SafetyMenu = nullptr;

inline std::array<AxeState, 8> Axes = {};
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline int ReturnMissileNetworkId = 0;
inline Vector3 ReturnStart = {};
inline Vector3 ReturnEnd = {};
inline bool ReturnInFlight = false;
inline bool SpinningAxes = false;
inline bool CatchIntent = false;
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

inline bool NameContainsAny(const char* value,
                            std::initializer_list<const char*> tokens) {
    if (!value) return false;
    for (const auto* token : tokens) {
        if (token && token[0] && Engine::TextContains(value, token)) return true;
    }
    return false;
}

inline bool IsAxeObject(const SDK::Events::ObjectEventArgs& args) {
    return NameContainsAny(args.Sender.Name,
        {"draven", "spinningaxe", "spinningattack", "dravenq"}) &&
        NameContainsAny(args.Sender.Name,
            {"axe", "spinning", "q_mark", "catch"});
}

inline bool IsAxeMissile(const SDK::Events::ObjectEventArgs& args) {
    return NameContainsAny(args.MissileName,
               {"draven", "spinningaxe", "dravenq"}) ||
           NameContainsAny(args.SpellName, {"dravenq", "spinningattack"});
}

inline bool IsReturnMissile(const SDK::Events::ObjectEventArgs& args) {
    return NameContainsAny(args.MissileName,
               {"dravenr", "whirlingdeath", "dravenrmis"}) ||
           NameContainsAny(args.SpellName, {"dravenr", "whirlingdeath"});
}

inline int ObjectId(const SDK::Events::ObjectEventArgs& args) {
    return static_cast<int>(args.Sender.NetworkId != 0
        ? args.Sender.NetworkId : args.Sender.Index);
}

inline AxeState* FindAxe(int id, bool create = false) {
    AxeState* empty = nullptr;
    AxeState* oldest = nullptr;
    for (auto& axe : Axes) {
        if (id != 0 && axe.NetworkId == id) return &axe;
        if (axe.NetworkId == 0 && !empty) empty = &axe;
        if (!oldest || axe.ExpireTick < oldest->ExpireTick) oldest = &axe;
    }
    if (!create) return nullptr;
    AxeState* result = empty ? empty : oldest;
    if (result) *result = {};
    return result;
}

inline void PruneAxes() {
    const int now = Now();
    for (auto& axe : Axes) {
        if (axe.NetworkId != 0 && !AxeActive(axe, now)) ClearAxe(axe);
    }
}

inline AxeState* BestCatchAxe() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return nullptr;
    AxeState* best = nullptr;
    float bestDistance = FLT_MAX;
    for (auto& axe : Axes) {
        if (!AxeActive(axe, Now())) continue;
        const float distance = player.Position().Distance2D(axe.Position);
        if (distance < bestDistance) {
            best = &axe;
            bestDistance = distance;
        }
    }
    return best;
}

inline int SpinningAxeCount() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return SpinningAxes ? 1 : 0;
    const int count = player.GetBuffCount("DravenSpinningAttack");
    return std::clamp(count > 0 ? count : (SpinningAxes ? 1 : 0), 0, 2);
}

inline bool HasSpinningAxe() {
    return SpinningAxeCount() > 0 || SpinningAxes;
}

inline int PassiveStackCount() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    const int named = player.GetBuffCount("DravenPassiveStacks");
    return named > 0 ? named : player.GetBuffCount("DravenPassive");
}

inline bool TargetReach(const AIHeroClient& target, float range) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid() &&
           player.Position().Distance2D(target.Position()) <=
               range + target.BoundingRadius();
}

inline bool EPrediction(const AIHeroClient& target,
                        SDK::PredictionOutput* output = nullptr) {
    if (!Engine::RuntimeSpells[2] || !target.IsValid()) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(2, target, SDK::HitChance::High,
                                    false, &prediction) &&
                     !PredictionProjectileWall(2, prediction, kEHalfWidth);
    if (output) *output = prediction;
    return hit;
}

inline bool RPrediction(const AIHeroClient& target,
                        SDK::PredictionOutput* output = nullptr) {
    if (!Engine::RuntimeSpells[3] || !target.IsValid()) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(3, target, SDK::HitChance::High,
                                    false, &prediction) &&
                     !PredictionProjectileWall(3, prediction, kRHalfWidth);
    if (output) *output = prediction;
    return hit;
}

inline bool AxeCatchSafe(const AxeState& axe) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !AxeActive(axe, Now())) return false;
    const int maximumEnemies = Slider(SafetyMenu, "MaxCatchEnemies", 1);
    const int enemies = Engine::CountEnemiesAt(axe.Position, 450.0f);
    return Geometry::AxeCatchSafe(player.Position(), axe.Position,
                                  enemies, maximumEnemies,
                                  SDK::NavMesh::IsWall(axe.Position), false);
}

inline bool CatchRouteAvailable() {
    const auto* axe = BestCatchAxe();
    return axe && AxeCatchSafe(*axe);
}

inline void RefreshAxeFocus(Mode mode, const AIHeroClient& preferred) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    auto owned = OwnedOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil, 900.0f);
    if (!combat || !owned.IsValid() || !InAutoAttackRange(owned)) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        owned = {};
    }
    if (owned.IsValid()) return;

    AIHeroClient best{};
    float scoreBest = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 950.0f) || !InAutoAttackRange(enemy)) continue;
        float score = 100.0f - enemy.HealthPercent();
        if (preferred.IsValid() && preferred.NetworkId() == enemy.NetworkId()) score += 180.0f;
        if (enemy.Health() + enemy.AllShield() <= AutoDamage(enemy)) score += 300.0f;
        if (score > scoreBest) {
            scoreBest = score;
            best = enemy;
        }
    }
    if (best.IsValid()) {
        (void)SetTemporaryOrbwalkerFocus(best, 900.0f, 850,
                                         OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target, Mode mode) {
    const auto player = GameObjects::Player();
    const float distance = player.IsValid()
        ? player.Position().Distance2D(target.Position()) : FLT_MAX;
    const bool attack = OrbwalkerAttackRoute(target);
    const bool e = CanUse(2, mode) && distance <= kERange + target.BoundingRadius() &&
                   EPrediction(target);
    const bool r = CanUse(3, mode) && RPrediction(target);
    const std::array<bool, 4> reachable = {HasSpinningAxe(), false, e, r};
    float estimate = attack ? AutoDamage(target) * (HasSpinningAxe() ? 2.0f : 1.0f) : 0.0f;
    if (e) estimate += SpellDamage(2, target);
    if (r) estimate += SpellDamage(3, target);
    auto context = BaseTargetContext(target, estimate);
    context.AutoReachable = attack;
    context.DirectSpellReachable = e;
    context.SetupReachable = HasSpinningAxe();
    context.ExecuteReachable = r && context.Killable;
    context.ProjectileBlocked = !attack && !e && !r;
    (void)reachable;
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred, Mode mode) {
    LastSmartTarget = ControllerHelpers::SelectReachableEnemy(
        preferred, kERange,
        [mode](const AIHeroClient& enemy) { return TargetFacts(enemy, mode); });
    return LastSmartTarget;
}

inline bool CastQ(Mode mode) {
    if (HasSpinningAxe() || !CanUse(0, mode) ||
        !CastThrottlePassed(LastQCastTick, 70)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    LastQCastTick = Now();
    SpinningAxes = true;
    return true;
}

inline bool CastW(Mode mode, bool catchMovement = false) {
    if (!CanUse(1, mode, true) || !CastThrottlePassed(LastWCastTick, 80)) return false;
    if (!catchMovement && mode == Mode::Harass &&
        GameObjects::Player().HealthPercent() < 30.0f) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastWCastTick = Now();
    CatchIntent = catchMovement;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!Engine::RuntimeSpells[2] || !TargetReach(target, kERange) ||
        !CastThrottlePassed(LastECastTick, 80) ||
        (!reactive && !CanUse(2, mode))) return false;
    SDK::PredictionOutput prediction{};
    if (!EPrediction(target, &prediction)) return false;
    if (Engine::ControllerCastPosition(2, prediction.GetCastPosition())) {
        LastECastTick = Now();
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        return true;
    }
    return false;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool manual = false) {
    if (!Engine::RuntimeSpells[3] || !target.IsValid() ||
        !CastThrottlePassed(LastRCastTick, 120) ||
        (!manual && !CanUse(3, mode))) return false;
    SDK::PredictionOutput prediction{};
    if (!RPrediction(target, &prediction)) return false;
    const auto player = GameObjects::Player();
    const bool lethal = SpellDamage(3, target) >= target.Health() + target.AllShield();
    const bool returnHit = ReturnInFlight && ReturnPathHits(
        ReturnStart, ReturnEnd, target.Position(), target.BoundingRadius());
    ReturnContext context{};
    context.OutgoingHit = true;
    context.ReturnHit = returnHit;
    context.LethalOnEitherPass = lethal;
    context.MultiTarget = Engine::CountEnemiesAt(prediction.GetCastPosition(), 250.0f) >= 2;
    context.Manual = manual;
    context.CashInSafe = player.IsValid() &&
        player.Position().Distance2D(target.Position()) > 500.0f;
    context.TargetEscaping = target.IsMoving();
    if (!ShouldCastReturn(context)) return false;
    if (Engine::ControllerCastPosition(3, prediction.GetCastPosition())) {
        LastRCastTick = Now();
        ReturnInFlight = true;
        ReturnStart = player.Position();
        ReturnEnd = prediction.GetCastPosition();
        return true;
    }
    return false;
}

inline bool TryCashIn(const AIHeroClient& preferred) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PassiveStackCount() <= 0) return false;
    const auto target = ControllerHelpers::SelectReachableEnemy(
        preferred, kRRange,
        [](const AIHeroClient& enemy) {
            auto context = BaseTargetContext(enemy, SpellDamage(3, enemy));
            context.ExecuteReachable = true;
            context.DirectSpellReachable = true;
            return context;
        });
    if (!Engine::ValidEnemy(target, kRRange)) return false;
    CashInContext cash{};
    cash.PassiveStacks = true;
    cash.TargetLethal = SpellDamage(3, target) >= target.Health() + target.AllShield();
    const auto* catchAxe = BestCatchAxe();
    cash.SafeCatch = !catchAxe || AxeCatchSafe(*catchAxe);
    cash.SelectedTarget = preferred.IsValid() &&
        preferred.NetworkId() == target.NetworkId();
    cash.EnemyNearBase = target.Position().Distance2D(player.Position()) < 900.0f;
    return ShouldCashIn(cash) && CastR(target, Mode::Automatic);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    const auto target = ControllerHelpers::SelectReachableEnemy(
        preferred, kRRange,
        [](const AIHeroClient& enemy) {
            auto context = BaseTargetContext(enemy, SpellDamage(3, enemy));
            context.ExecuteReachable = SpellDamage(3, enemy) >=
                enemy.Health() + enemy.AllShield();
            context.DirectSpellReachable = true;
            return context;
        });
    if (!Engine::ValidEnemy(target, kRRange)) return false;
    return CastR(target, Mode::Automatic);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target, kERange)) return false;
    if (!HasSpinningAxe() && CastQ(mode)) return true;
    if (CastE(target, mode)) return true;
    if (CastW(mode, CatchRouteAvailable())) return true;
    return false;
}

inline bool TryFlee(const AIHeroClient& preferred) {
    const auto threat = ControllerHelpers::NearestEnemyToPlayer(preferred, 900.0f);
    if (Engine::ValidEnemy(threat, 900.0f) && CastE(threat, Mode::Flee, true)) return true;
    return CastW(Mode::Flee, true);
}

inline bool TryReactive() {
    if (GapcloserExpireTick >= Now()) {
        const auto target = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(target, kERange) && CastE(target, Mode::Automatic, true)) return true;
    }
    if (InterruptExpireTick >= Now()) {
        const auto target = ControllerHelpers::HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(target, kERange) && CastE(target, Mode::Automatic, true)) return true;
    }
    return false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    PruneAxes();
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    if (!combat) ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    RefreshAxeFocus(mode, preferred);

    if (TryReactive()) return true;
    if (ManualUltimatePressed()) {
        const auto manualTarget = ControllerHelpers::NearestEnemyToPlayer(
            preferred, kRRange);
        if (Engine::ValidEnemy(manualTarget, kRRange) &&
            CastR(manualTarget, Mode::Automatic, true)) return true;
    }
    if (TryCashIn(preferred)) return true;
    if (TryKillSecure(preferred)) return true;
    if (mode == Mode::Flee) return TryFlee(preferred);
    if (combat) return TryCombat(SelectSmartTarget(preferred, mode), mode);
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        if (!HasSpinningAxe() && CastQ(mode)) return true;
        return Engine::TryFarm(mode);
    }
    if (mode == Mode::Automatic) return TryCombat(SelectSmartTarget(preferred, mode), mode);
    return false;
}

inline void ObserveLocalSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender) || args.IsAutoAttack) return;
    const int now = Now();
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        SpellEventNameContainsAny(args, {"dravenq", "spinningattack"})) {
        LastQCastTick = now;
        SpinningAxes = true;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               SpellEventNameContainsAny(args, {"dravenw", "bloodrush"})) {
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               SpellEventNameContainsAny(args, {"dravene", "standaside"})) {
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               SpellEventNameContainsAny(args, {"dravenr", "whirlingdeath"})) {
        LastRCastTick = now;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender)) return;
    if (NameContainsAny(args.BuffName,
                        {"dravenSpinningAttack", "dravenq"})) {
        SpinningAxes = true;
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender)) return;
    if (NameContainsAny(args.BuffName,
                        {"dravenSpinningAttack", "dravenq"})) {
        SpinningAxes = false;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    if (!HasSpinningAxe()) return;
    auto* axe = BestCatchAxe();
    if (axe && axe->NetworkId != 0) {
        ClearAxe(*axe);
        SpinningAxes = SpinningAxeCount() > 0;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    const auto* axe = BestCatchAxe();
    const bool catchReachable = axe && AxeCatchReachable(
        GameObjects::Player().Position(), axe->Position, 350.0f);
    const bool catchSafe = axe && AxeCatchSafe(*axe);
    AIHeroClient target{};
    if (args.Target.IsValid() && args.Target.IsHero()) target = AIHeroClient(args.Target.Handle());
    AttackPolicyContext context{};
    context.Windup = true;
    context.CatchReachable = catchReachable;
    context.CatchSafe = catchSafe;
    context.TargetKillable = target.IsValid() &&
        AutoDamage(target) >= target.Health() + target.AllShield();
    context.SelectedTarget = target.IsValid() && LastSmartTarget.IsValid() &&
        target.NetworkId() == LastSmartTarget.NetworkId();
    context.AxeWouldExpire = axe && Now() + 300 >= axe->ExpireTick;
    context.ManualCastPending = Engine::WasControllerCast(0) ||
        Engine::WasControllerCast(1) || Engine::WasControllerCast(2) ||
        Engine::WasControllerCast(3);
    if (!AllowAttackDuringWindup(context)) {
        args.Process = false;
        CatchIntent = true;
        return;
    }
    auto focus = OwnedOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil, 900.0f);
    if (focus.IsValid() && target.IsValid() &&
        focus.NetworkId() != target.NetworkId()) {
        (void)RedirectBeforeAttackToFocus(args, focus);
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAfterAttackTargetId, LastAfterAttackTick);
    CatchIntent = false;
}



inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!ControllerHelpers::ObjectEventIsAllied(args) || !IsAxeObject(args)) return;
    const int id = ObjectId(args);
    auto* axe = FindAxe(id, true);
    if (!axe) return;
    const Vector3 position = args.Sender.Position.IsValid()
        ? args.Sender.Position : args.EndPosition;
    RecordAxe(*axe, id, position, Now());
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = ObjectId(args);
    if (auto* axe = FindAxe(id)) ClearAxe(*axe);
    if (id == ReturnMissileNetworkId) {
        ReturnMissileNetworkId = 0;
        ReturnInFlight = false;
        ReturnStart = ReturnEnd = {};
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args)) return;
    const int id = static_cast<int>(args.MissileNetworkId != 0
        ? args.MissileNetworkId : args.Sender.NetworkId);
    if (IsReturnMissile(args)) {
        ReturnMissileNetworkId = id;
        ReturnInFlight = true;
        ReturnStart = args.StartPosition.IsValid()
            ? args.StartPosition : GameObjects::Player().Position();
        ReturnEnd = args.EndPosition.IsValid() ? args.EndPosition : args.CastEndPosition;
        return;
    }
    if (IsAxeMissile(args) && args.EndPosition.IsValid()) {
        auto* axe = FindAxe(id, true);
        if (axe) RecordAxe(*axe, id, args.EndPosition, Now());
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = static_cast<int>(args.MissileNetworkId != 0
        ? args.MissileNetworkId : args.Sender.NetworkId);
    if (id == ReturnMissileNetworkId) {
        ReturnMissileNetworkId = 0;
        ReturnInFlight = false;
        ReturnStart = ReturnEnd = {};
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("DravenMechanics", "Draven Mechanics"));
    AxeMenu = TacticsMenu->AddSubMenu(new Menu("AxeLogic", "Spinning Axe / Catch"));
    AxeMenu->Add(new MenuSeparator("CatchMovement", "Preserve safe catch movement before AA windup"));
    ReturnMenu = TacticsMenu->AddSubMenu(new Menu("ReturnLogic", "Whirling Death"));
    ReturnMenu->Add(new MenuSeparator("CashIn", "Cash in passive only on a safe lethal route"));
    SafetyMenu = TacticsMenu->AddSubMenu(new Menu("Safety", "Catch Safety"));
    SafetyMenu->Add(new MenuSlider("MaxCatchEnemies", "Maximum enemies around axe", 1, 0, 3));
}

inline void OnLoad() {
    Axes = {};
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = 0;
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    ReturnMissileNetworkId = 0;
    ReturnStart = ReturnEnd = {};
    ReturnInFlight = false;
    SpinningAxes = false;
    CatchIntent = false;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    Axes = {};
    ReturnStart = ReturnEnd = {};
    ReturnMissileNetworkId = 0;
    ReturnInFlight = false;
    TacticsMenu = AxeMenu = ReturnMenu = SafetyMenu = nullptr;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Record allied Draven axe landing objects from create and missile lifecycle events",
    "Reconcile axe expiry and deletion every update rather than trusting one hook",
    "Keep spinning-axe state synchronized with Q cast events and passive buff telemetry",
    "Preserve a selected/orbwalker target while an axe catch route is still safe",
    "Suppress a nonlethal AA during windup when it would abandon a safe active axe",
    "Allow an AA windup when the attack kills, is selected-target intent, or the axe expires",
    "Use Blood Rush for catch movement, spacing or escape instead of unconditional buffs",
    "Reject catch routes through walls, turrets or excess enemies",
    "Throw Stand Aside on predicted reachable targets for peel and interrupt",
    "Use Stand Aside immediately against captured gapclosers and interruptible spells",
    "Track outgoing Whirling Death missile start and end positions",
    "Evaluate return-line intersections before spending a global cast",
    "Cash in Draven passive only on a selected safe lethal route",
    "Reserve automatic Whirling Death for lethal, multi-target or escaping targets",
    "Preserve manual R intent while still requiring a real prediction path",
    "Keep Flee, LaneClear, Jungle and LastHit loops explicit and conservative",
    "Clear focus, axe objects and return state on mode exit and unload",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Draven;
    controller.ControllerId = "champion.kuroaio.ai.draven.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIDraven.md";
    controller.ImplementationSummary =
        "Object-backed spinning-axe and Whirling Death return tracking with safe catch movement, AA-windup preservation, passive cash-in safety, E peel/interrupt and explicit combat/farm/flee loops.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnDoCast = &OnDoCast;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, kERange, 900>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, kERange, 200, 5000>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Draven
