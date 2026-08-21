#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIHecarim.h"
#include "AIHecarimGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Hecarim {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ::Plugins::KuroAIO::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ::Plugins::KuroAIO::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline QState CurrentQState = QState::Ready;
inline WState CurrentWState = WState::Ready;
inline EState CurrentEState = EState::Ready;
inline RState CurrentRState = RState::Ready;
inline int QStacks = 0;
inline int QLastHitTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int ETargetId = 0;
inline int RCastTick = 0;
inline int RFearUntil = 0;
inline int RTargetId = 0;
inline int RMissileId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;

inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline Vector3 WCenter{};
inline Vector3 EEndpoint{};
inline Vector3 REndpoint{};
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
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    const float raw = QRawDamage(SpellRank(0), player.BonusAttackDamage(), QStacks);
    return player.CalculatePhysicalDamage(target, raw);
}

inline float RDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || !Engine::RuntimeSpells[3]) return 0.0f;
    const float runtime = Engine::RuntimeSpells[3]->GetDamage(target);
    return runtime > 0.0f ? runtime : 150.0f + 1.0f * ControllerHelpers::AP();
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || PreserveAttack(reactive) ||
        !QReachable(player.Position(), target.Position(), target.BoundingRadius()) ||
        !Ready(0, mode, reactive)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    CurrentQState = QState::Active;
    QStacks = QStacksAfterHit(QStacks, Now(), QLastHitTick);
    QLastHitTick = Now();
    LastCastTick[0] = QLastHitTick;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) || PreserveAttack(reactive)) return false;
    const int nearby = Engine::CountEnemiesAt(player.Position(), kWRadius);
    if (!WZoneUseful(player.Position(), player.Position(), nearby, player.HealthPercent(),
                     static_cast<float>(Slider(TacticsMenu, "WHealth", 72)))) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    CurrentWState = WState::ZoneActive;
    WCenter = player.Position();
    WCastTick = Now();
    LastCastTick[1] = WCastTick;
    (void)target;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PreserveAttack(reactive)) return false;
    if (CurrentEState == EState::Charging) {
        const int elapsed = Now() - ECastTick;
        const Vector3 aim = target.IsValid() ? PredictPosition(target, 0.12f) :
            (fleeing ? Game::CursorPos() : Vector3{});
        if (!aim.IsValid() || aim.IsZero()) return false;
        const Vector3 endpoint = player.Position().Extend(aim,
            std::min(kERange, player.Position().Distance2D(aim)));
        const int enemies = Engine::CountEnemiesAt(endpoint, 300.0f);
        if (!EChargeAllowed(CurrentEState, elapsed, Engine::ValidEnemy(target),
                            SDK::NavMesh::IsWall(endpoint), Engine::UnderEnemyTurret(endpoint),
                            enemies > Slider(TacticsMenu, "MaxEEnemies", 2), fleeing)) return false;
        EEndpoint = endpoint;
        ETargetId = target.IsValid() ? static_cast<int>(target.NetworkId()) : 0;
        CurrentEState = EState::RamPending;
        return true;
    }
    if (CurrentEState != EState::Ready || !Ready(2, mode, reactive)) return false;
    const bool hasTarget = Engine::ValidEnemy(target, kERange);
    if (!hasTarget && !fleeing && !reactive) return false;
    if (!Engine::ControllerCastPosition(2, hasTarget ? PredictPosition(target, 0.10f) : Game::CursorPos())) return false;
    CurrentEState = EState::Charging;
    ECastTick = Now();
    ETargetId = hasTarget ? static_cast<int>(target.NetworkId()) : 0;
    LastCastTick[2] = ECastTick;
    return true;
}

inline bool FirstRHitIsTarget(const Vector3& origin, const Vector3& endpoint,
                              const AIHeroClient& target) {
    std::array<RCollisionTarget, 8> candidates{};
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (count >= static_cast<int>(candidates.size()) ||
            !Engine::ValidEnemy(enemy, kRMaxRange + 100.0f)) continue;
        candidates[static_cast<std::size_t>(count++)] = {
            static_cast<int>(enemy.NetworkId()), PredictPosition(enemy, 0.10f),
            enemy.BoundingRadius(), true};
    }
    return FirstRCollision(origin, endpoint, candidates) == static_cast<int>(target.NetworkId());
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(3, mode, reactive) ||
        CurrentRState != RState::Ready || PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, 0.10f);
    const float distance = player.Position().Distance2D(aim);
    if (!aim.IsValid() || aim.IsZero() || distance > kRMaxRange + target.BoundingRadius()) return false;
    const Vector3 endpoint = player.Position().Extend(aim, std::min(kRMaxRange, distance));
    if (!endpoint.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kRWidth)) return false;
    if (!FirstRHitIsTarget(player.Position(), endpoint, target)) return false;
    const bool lethal = ControllerHelpers::Lethal(target, RDamage(target));
    const int endpointEnemies = Engine::CountEnemiesAt(endpoint, 300.0f);
    const bool safe = RCommitAllowed(true, false, false, Engine::UnderEnemyTurret(endpoint),
        endpointEnemies, Slider(TacticsMenu, "MaxREnemies", 2), lethal, fleeing);
    if (!safe) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    CurrentRState = RState::CastPending;
    REndpoint = endpoint;
    RTargetId = static_cast<int>(target.NetworkId());
    RCastTick = Now();
    RFearUntil = RCastTick + static_cast<int>(RFearDuration(distance));
    LastCastTick[3] = RCastTick;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (player.Position().Distance2D(target.Position()) > kQRange && CastR(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < 35.0f || !Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < 25.0f) return;
    if (Engine::ValidEnemy(target) && QReachable(player.Position(), target.Position(), target.BoundingRadius()) &&
        CastQ(target, mode)) return;
    if (mode != Mode::LastHit && CastW(target, mode)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Flee, true, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastR(target, Mode::Flee, true, true);
}

inline void Automatic(const AIHeroClient& target) {
    if (IncomingThreatUntil >= Now() && Engine::ValidEnemy(target)) {
        if (CastE(target, Mode::Automatic, true)) return;
        (void)CastR(target, Mode::Automatic, true, true);
        return;
    }
    if (Engine::ValidEnemy(target)) {
        if (CastW(target, Mode::Automatic, true)) return;
        (void)CastQ(target, Mode::Automatic, true);
    }
}

inline void ReconcileState() {
    const int now = Now();
    if (QStacks > 0 && !QStacksActive(QStacks, now, QLastHitTick)) {
        QStacks = 0;
        CurrentQState = QState::Ready;
    }
    if (CurrentWState == WState::ZoneActive && now > WCastTick + kWDurationMs) {
        CurrentWState = WState::Ready;
        WCenter = {};
    }
    if (CurrentEState != EState::Ready && now > ECastTick + kEChargeDurationMs + 700) {
        CurrentEState = EState::Ready;
        ETargetId = 0;
        EEndpoint = {};
    }
    if (CurrentRState != RState::Ready &&
        now > (RFearUntil > RCastTick ? RFearUntil + 500 : RCastTick + kRFearDurationMs + 900)) {
        CurrentRState = RState::Ready;
        RTargetId = RMissileId = RFearUntil = 0;
        REndpoint = {};
    }
    if (IncomingThreatUntil < now) {
        IncomingThreatUntil = IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const float range = mode == Mode::Flee ? kRMaxRange + 100.0f : kERange + 100.0f;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, range);
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
    TacticsMenu = root->AddSubMenu(new Menu("HecarimTactics", "Hecarim tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("HecarimFarming", "Hecarim farming"));
    TacticsMenu->Add(new MenuSlider("MaxREnemies", "Maximum enemies at R endpoint", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("MaxEEnemies", "Maximum enemies at E ram endpoint", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("WHealth", "Cast W below health percent", 72, 0, 100));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve attack windup", true));
    FarmMenu->Add(new MenuSlider("Mana", "Farm mana percent", 25, 0, 100));
}

inline void OnLoad() {
    CurrentQState = QState::Ready;
    CurrentWState = WState::Ready;
    CurrentEState = EState::Ready;
    CurrentRState = RState::Ready;
    QStacks = QLastHitTick = WCastTick = ECastTick = ETargetId = 0;
    RCastTick = RTargetId = RMissileId = RFearUntil = LastAutoTargetId = LastAutoTick = 0;
    IncomingThreatUntil = IncomingThreatTargetId = 0;
    WCenter = EEndpoint = REndpoint = IncomingThreatEndpoint = {};
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
        if (args.IsAutoAttack) {
            LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
            LastAutoTick = now;
            return;
        }
        if (args.Slot < 0 || args.Slot > 3) return;
        LastCastTick[static_cast<std::size_t>(args.Slot)] = now;
        if (args.Slot == 0) {
            QStacks = QStacksAfterHit(QStacks, now, QLastHitTick);
            QLastHitTick = now;
            CurrentQState = QState::Active;
        } else if (args.Slot == 1) {
            CurrentWState = WState::ZoneActive;
            WCenter = args.StartPosition.IsValid() && !args.StartPosition.IsZero()
                ? args.StartPosition : GameObjects::Player().Position();
            WCastTick = now;
        } else if (args.Slot == 2) {
            if (CurrentEState == EState::Charging) CurrentEState = EState::RamPending;
            else { CurrentEState = EState::Charging; ECastTick = now; }
        } else if (args.Slot == 3) {
            CurrentRState = RState::CastPending;
            RCastTick = now;
            REndpoint = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
                ? args.EndPosition : args.CastPosition;
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
        if (CurrentEState == EState::RamPending) CurrentEState = EState::Ready;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "hecarimramp")) {
        CurrentQState = QState::Active;
        QLastHitTick = QLastHitTick == 0 ? Now() : QLastHitTick;
    } else if (Engine::TextContains(args.BuffName, "hecarimw")) {
        CurrentWState = WState::ZoneActive;
        WCastTick = Now();
    } else if (Engine::TextContains(args.BuffName, "hecarimcharge")) {
        CurrentEState = EState::Charging;
        ECastTick = Now();
    } else if (Engine::TextContains(args.BuffName, "hecarimult")) {
        CurrentRState = RState::FearActive;
        RCastTick = Now();
        RFearUntil = RCastTick + static_cast<int>(RFearDuration(REndpoint.Distance2D(GameObjects::Player().Position())));
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "hecarimramp")) {
        QStacks = 0;
        CurrentQState = QState::Ready;
    } else if (Engine::TextContains(args.BuffName, "hecarimw")) CurrentWState = WState::Ready;
    else if (Engine::TextContains(args.BuffName, "hecarimcharge")) CurrentEState = EState::Ready;
    else if (Engine::TextContains(args.BuffName, "hecarimult")) CurrentRState = RState::Ready;
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
                           IncomingThreatUntil, kQRange + 100.0f, 1100);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"hecarimult", "hecarimcharge"})) {
        RMissileId = args.MissileNetworkId != 0 ? static_cast<int>(args.MissileNetworkId)
                                                : static_cast<int>(args.Sender.NetworkId);
        CurrentRState = RState::CastPending;
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId != 0 ? static_cast<int>(args.MissileNetworkId)
                                              : static_cast<int>(args.Sender.NetworkId);
    if (id == RMissileId) {
        RMissileId = 0;
        CurrentRState = RState::FearActive;
        if (RFearUntil == 0) RFearUntil = Now() + kRFearDurationMs;
    }
}

inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Rampage hit stacks to three, refreshes eight-second expiry and ramps damage",
    "Spirit of Dread 525/575 zone sustains Hecarim from enemy and allied damage",
    "Devastating Charge speed ramp and delayed ram attack ownership",
    "Charge movement rejects walls, turrets and unsafe enemy endpoints",
    "Onslaught of Shadows 1000-range spectral wave fear endpoint",
    "R wave speed, first collision and distance-scaled fear duration",
    "R endpoint terrain, turret and enemy-count safety with lethal/Flee override",
    "attack-windup preservation and cast-state reconciliation/",
    "event plus polling reconciliation for Q/W/E/R buffs and missiles",
    "combo, harass, lane clear, jungle, last hit, flee and automatic behavior",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Hecarim;
    controller.ControllerId = "champion.kuroaio.ai.hecarim.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIHecarim.md";
    controller.ImplementationSummary =
        "Q Rampage stack ramp, W leech zone, E charge/ram safety, and collision-safe R fear endpoint.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Hecarim
