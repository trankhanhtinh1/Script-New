#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIWarwick.h"
#include "AIWarwickGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Warwick {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Slider;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline QState CurrentQState = QState::Ready;
inline EState CurrentEState = EState::Ready;
inline RState CurrentRState = RState::Ready;
inline int QTargetId = 0;
inline int QCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int RTargetId = 0;
inline int RMissileId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline std::array<int, 4> LastCastTick{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
           Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
           (reactive || LastCastTick[static_cast<std::size_t>(slot)] + 45 <= Now());
}

inline bool PreserveAttack(bool reactive) {
    return !reactive && Orbwalker::IsWindingUp() &&
           Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target);
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = QRawDamage(SpellRank(0), player.TotalAttackDamage(), player.AP(),
                                 target.MaxHealth());
    return player.CalculateMagicDamage(target, raw);
}

inline float RDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || !Engine::RuntimeSpells[3]) return 0.0f;
    const float runtime = Engine::RuntimeSpells[3]->GetDamage(target);
    return runtime > 0.0f ? runtime : 525.0f + 1.67f * ControllerHelpers::BonusAttackDamage();
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    const bool holding = CurrentQState == QState::Holding;
    if (!player.IsValid() || Protected(target) || PreserveAttack(reactive) ||
        (!holding && !QReachable(player.Position(), target.Position(),
                                 target.BoundingRadius(), false))) {
        return false;
    }
    if (CurrentQState == QState::Holding) {
        const int elapsed = Now() - QCastTick;
        const bool outOfRange = !QReachable(player.Position(), target.Position(),
                                            target.BoundingRadius(), true);
        const bool lethal = ControllerHelpers::Lethal(target, QDamage(target));
        if (!QReleaseAllowed(CurrentQState, elapsed, target.HealthPercent(), outOfRange,
                             lethal, player.HealthPercent() <= 38.0f)) return false;
        if (!Engine::ControllerCastUnit(0, target)) return false;
        CurrentQState = QState::ReleasePending;
        LastCastTick[0] = Now();
        return true;
    }
    if (CurrentQState != QState::Ready || !Ready(0, mode, reactive)) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    CurrentQState = QState::Holding;
    QTargetId = static_cast<int>(target.NetworkId());
    QCastTick = Now();
    LastCastTick[0] = QCastTick;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || !Ready(1, mode, reactive) ||
        PreserveAttack(reactive)) return false;
    const bool combatMode = mode == Mode::Combo || mode == Mode::Harass ||
                            mode == Mode::Automatic || mode == Mode::Jungle;
    if (!BloodScentAllows(target.HealthPercent(), player.HealthPercent(), true, combatMode)) {
        return false;
    }
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PreserveAttack(reactive)) return false;
    if (CurrentEState == EState::Reduction) {
        const int nearby = Engine::CountEnemiesAt(player.Position(), kERange + 45.0f);
        const bool threatened = IncomingThreatUntil >= Now();
        if (!ERecastAllowed(CurrentEState, Now() - ECastTick, nearby, threatened, fleeing)) return false;
        if (!Engine::ControllerCastSelf(2)) return false;
        CurrentEState = EState::RecastPending;
        LastCastTick[2] = Now();
        return true;
    }
    if (CurrentEState != EState::Ready || !Ready(2, mode, reactive)) return false;
    const int nearby = Engine::CountEnemiesAt(player.Position(), kERange + 45.0f);
    const bool threatened = IncomingThreatUntil >= Now();
    if (!reactive && !fleeing && nearby <= 0 && !Engine::ValidEnemy(target, kERange)) return false;
    if (!reactive && player.HealthPercent() > 72.0f && !threatened && nearby <= 0) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    CurrentEState = EState::Reduction;
    ECastTick = Now();
    LastCastTick[2] = ECastTick;
    return true;
}

inline bool FirstRHitIsTarget(const Vector3& origin, const Vector3& endpoint,
                              const AIHeroClient& target) {
    std::array<RCollisionTarget, 8> candidates{};
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (count >= static_cast<int>(candidates.size()) ||
            !Engine::ValidEnemy(enemy, RReach(GameObjects::Player().MoveSpeed()) + 100.0f)) continue;
        const Vector3 predicted = PredictPosition(enemy, 0.10f);
        candidates[static_cast<std::size_t>(count++)] = {
            static_cast<int>(enemy.NetworkId()), predicted, enemy.BoundingRadius(), true};
    }
    return FirstRCollision(origin, endpoint, candidates) == static_cast<int>(target.NetworkId());
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(3, mode, reactive) ||
        CurrentRState != RState::Ready || PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, 0.10f);
    const float reach = RReach(player.MoveSpeed());
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > reach + target.BoundingRadius()) return false;
    const Vector3 endpoint = player.Position().Extend(aim, std::min(reach,
        player.Position().Distance2D(aim)));
    if (!endpoint.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kRWidth)) return false;
    if (!FirstRHitIsTarget(player.Position(), endpoint, target)) return false;
    const bool lethal = ControllerHelpers::Lethal(target, RDamage(target));
    const bool playerLow = player.HealthPercent() <= 45.0f;
    const bool healing = RHealingWorthwhile(player.HealthPercent(), target.HealthPercent(),
                                            RDamage(target), false);
    if (!lethal && !healing && !fleeing && target.HealthPercent() > 45.0f) return false;
    const int endpointEnemies = Engine::CountEnemiesAt(endpoint, 300.0f);
    const int maximumEnemies = Slider(TacticsMenu, "MaxREnemies", 2);
    const bool safe = RCommitAllowed(true, false, false, Engine::UnderEnemyTurret(endpoint),
                                     endpointEnemies, maximumEnemies, lethal, playerLow, fleeing);
    if (!safe) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    CurrentRState = RState::CastPending;
    RTargetId = static_cast<int>(target.NetworkId());
    RCastTick = Now();
    LastCastTick[3] = RCastTick;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (target.HealthPercent() <= 50.0f && CastW(target, Mode::Combo)) return;
    if (player.Position().Distance2D(target.Position()) > kQFollowRange + target.BoundingRadius() &&
        CastR(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (target.HealthPercent() <= 45.0f) (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < 35.0f || !Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (target.HealthPercent() <= 50.0f) (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < 30.0f) return;
    if (Engine::ValidEnemy(target) && QReachable(player.Position(), target.Position(),
                                                 target.BoundingRadius(), false) &&
        (target.HealthPercent() <= 35.0f || player.HealthPercent() <= 45.0f) &&
        CastQ(target, mode)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastE(target, Mode::Flee, true, true)) return;
    const auto player = GameObjects::Player();
    if (player.IsValid() && player.HealthPercent() <= 45.0f && CastQ(target, Mode::Flee, true)) return;
    (void)CastR(target, Mode::Flee, true, true);
}

inline void Automatic(const AIHeroClient& target) {
    if (IncomingThreatUntil >= Now() && Engine::ValidEnemy(target)) {
        if (CastE(target, Mode::Automatic, true)) return;
        if (GameObjects::Player().HealthPercent() <= 45.0f && CastR(target, Mode::Automatic, true)) return;
    }
    if (Engine::ValidEnemy(target)) {
        if (target.HealthPercent() <= 50.0f && CastW(target, Mode::Automatic, true)) return;
        if (GameObjects::Player().HealthPercent() <= 38.0f && CastQ(target, Mode::Automatic, true)) return;
    }
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (CurrentQState == QState::Holding && now > QCastTick + kQReleaseWindowMs) {
        CurrentQState = QState::Ready;
        QTargetId = 0;
    }
    if (CurrentQState == QState::ReleasePending && now > QCastTick + kQReleaseWindowMs + 250) {
        CurrentQState = QState::Ready;
        QTargetId = 0;
    }
    if (CurrentQState == QState::Holding && !player.Spellbook().IsChanneling() &&
        now > QCastTick + kQChannelMs + 150) {
        CurrentQState = QState::Ready;
        QTargetId = 0;
    }
    if (CurrentEState != EState::Ready && now > ECastTick + kEReductionMs + 500) {
        CurrentEState = EState::Ready;
    }
    if (CurrentRState != RState::Ready && now > RCastTick + kRSuppressionMs + 700) {
        CurrentRState = RState::Ready;
        RTargetId = 0;
        RMissileId = 0;
    }
    if (IncomingThreatUntil < now) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const float targetRange = mode == Mode::Flee ? RReach(GameObjects::Player().MoveSpeed()) + 100.0f
                                                 : std::max(kQFollowRange, kERange) + 100.0f;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, targetRange);
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
    TacticsMenu = root->AddSubMenu(new Menu("Warwick tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Warwick farming"));
    TacticsMenu->Add(new MenuSlider("MaxREnemies", "Maximum enemies at R landing", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast protection (ms)", 650, 0, 2000));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve attack windup", true));
    FarmMenu->Add(new MenuSlider("Mana", "Farm mana percent", 30, 0, 100));
}

inline void OnLoad() {
    CurrentQState = QState::Ready;
    CurrentEState = EState::Ready;
    CurrentRState = RState::Ready;
    QTargetId = RTargetId = RMissileId = 0;
    QCastTick = ECastTick = RCastTick = 0;
    LastAutoTargetId = LastAutoTick = 0;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingThreatTargetId = 0;
    IncomingThreatEndpoint = {};
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
        if (args.Slot < 0 || args.Slot > 3) return;
        if (!Engine::WasControllerCast(args.Slot)) {
            ManualOwnershipUntil = now + static_cast<int>(Slider(TacticsMenu, "ManualOwnershipMs", 650));
        }
        LastCastTick[static_cast<std::size_t>(args.Slot)] = now;
        if (args.Slot == 0) {
            if (CurrentQState == QState::Holding) CurrentQState = QState::ReleasePending;
            else { CurrentQState = QState::Holding; QCastTick = now; }
        } else if (args.Slot == 2) {
            if (CurrentEState == EState::Reduction) CurrentEState = EState::RecastPending;
            else { CurrentEState = EState::Reduction; ECastTick = now; }
        } else if (args.Slot == 3) {
            CurrentRState = RState::CastPending;
            RCastTick = now;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatTargetId = static_cast<int>(args.Sender.NetworkId);
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
        IncomingThreatEndpoint = args.EndPosition;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
        LastAutoTick = Now();
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "warwickq")) {
            CurrentQState = QState::Holding;
            if (QCastTick == 0) QCastTick = Now();
        } else if (Engine::TextContains(args.BuffName, "warwicke")) {
            CurrentEState = EState::Reduction;
            ECastTick = Now();
        } else if (Engine::TextContains(args.BuffName, "warwickr")) {
            CurrentRState = RState::Suppressing;
            RCastTick = Now();
        }
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "warwickq")) {
        CurrentQState = QState::Ready;
        QTargetId = 0;
    } else if (Engine::TextContains(args.BuffName, "warwicke")) {
        CurrentEState = EState::Ready;
    } else if (Engine::TextContains(args.BuffName, "warwickr")) {
        CurrentRState = RState::Ready;
        RTargetId = 0;
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
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
                           IncomingThreatUntil, kERange + 80.0f, 1100);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"warwickr", "infiniteduress"})) {
        RMissileId = args.MissileNetworkId != 0 ? static_cast<int>(args.MissileNetworkId)
                                                : static_cast<int>(args.Sender.NetworkId);
        CurrentRState = RState::CastPending;
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId != 0 ? static_cast<int>(args.MissileNetworkId)
                                              : static_cast<int>(args.Sender.NetworkId);
    if (id == RMissileId) RMissileId = 0;
}

inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Jaws of the Beast channel hold, follow range and release healing",
    "Q percent-health damage and low-health lethal gate",
    "Blood Hunt 50/25 percent scent tiers and active cast threshold",
    "Primal Howl reduction window and one-second fear recast",
    "Infinite Duress movement-speed reach and first-champion collision",
    "R suppression, low-health healing and endpoint turret safety",
    "attack-windup preservation and manual cast ownership",
    "event plus polling reconciliation for Q/E/R buffs and missiles",
    "combo, harass, lane clear, jungle, last hit, flee and automatic behavior",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Warwick;
    controller.ControllerId = "champion.kuroaio.ai.warwick.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIWarwick.md";
    controller.ImplementationSummary =
        "Q hold/release and healing, W blood-scent thresholds, E fear reduction, and collision-safe R suppression.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Warwick
