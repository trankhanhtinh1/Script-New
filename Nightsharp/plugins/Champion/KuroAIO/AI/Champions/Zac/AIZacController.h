#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIZac.h"
#include "AIZacGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Zac {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ::Plugins::KuroAIO::Bool;
using ::Plugins::KuroAIO::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline PassiveState CurrentPassiveState = PassiveState::Ready;
inline QState CurrentQState = QState::Ready;
inline EState CurrentEState = EState::Ready;
inline RState CurrentRState = RState::Ready;
inline int PassiveCooldownEndTick = 0;
inline int BlobCount = 0;
inline std::array<Vector3, 4> BlobPositions{};
inline int QFirstTargetId = 0;
inline int QFirstCastTick = 0;
inline Vector3 QFirstPosition{};
inline int WCastTick = 0;
inline int ECastTick = 0;
inline Vector3 EEndpoint{};
inline int RCarryTargetId = 0;
inline int RMissileId = 0;
inline int RCarryTick = 0;
inline int RBounceCount = 0;
inline int RCastTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
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

inline bool HealthCostAvailable(const AIHeroClient& target, float minimumHealth = 20.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.HealthPercent() <= minimumHealth) return false;
    return !Engine::ValidEnemy(target) || target.HealthPercent() > 0.0f;
}

inline float WDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || !Engine::RuntimeSpells[1]) return 0.0f;
    const float runtime = Engine::RuntimeSpells[1]->GetDamage(target);
    return runtime > 0.0f ? runtime : WRawDamage(SpellRank(1), target.MaxHealth(), ControllerHelpers::AP());
}

inline float EDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    return EImpactDamage(SpellRank(2), ControllerHelpers::AP(), Now() - ECastTick);
}

inline float RDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || !Engine::RuntimeSpells[3]) return 0.0f;
    const float runtime = Engine::RuntimeSpells[3]->GetDamage(target);
    return runtime > 0.0f ? runtime : target.MaxHealth() * kRDamagePercent / 100.0f;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || PreserveAttack(reactive) || !Ready(0, mode, reactive)) return false;
    const int targetId = static_cast<int>(target.NetworkId());
    if (CurrentQState == QState::FirstHit) {
        if (targetId == QFirstTargetId || Now() - QFirstCastTick > kQPairWindowMs ||
            !QPairHits(QFirstPosition, PredictPosition(target, 0.12f))) return false;
        if (!Engine::ControllerCastUnit(0, target)) return false;
        CurrentQState = QState::Ready;
        QFirstTargetId = 0;
        QFirstPosition = {};
        LastCastTick[0] = Now();
        return true;
    }
    if (!QFirstHitAllowed(CurrentQState, true, false, Orbwalker::IsWindingUp()) ||
        player.Position().Distance2D(target.Position()) > kQRange + target.BoundingRadius()) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    CurrentQState = QState::FirstHit;
    QFirstTargetId = targetId;
    QFirstPosition = PredictPosition(target, 0.12f);
    QFirstCastTick = LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) || PreserveAttack(reactive) ||
        player.HealthPercent() <= Slider(TacticsMenu, "WMinimumHealth", 20)) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kWRadius);
    const bool lethal = Engine::ValidEnemy(target) && ControllerHelpers::Lethal(target, WDamage(target));
    if (!WCastAllowed(player.HealthPercent(), enemies, lethal, IncomingThreatUntil >= Now())) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WCastTick = LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false, bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PreserveAttack(reactive)) return false;
    if (CurrentEState == EState::Charging) {
        const int elapsed = Now() - ECastTick;
        const bool valid = Engine::ValidEnemy(target, kERangeMax + 100.0f);
        if (!EChargeAllowed(CurrentEState, elapsed, valid, fleeing)) return false;
        const Vector3 aim = valid ? PredictPosition(target, 0.18f) : Game::CursorPos();
        if (!aim.IsValid() || aim.IsZero()) return false;
        const float range = std::min(kERangeMax, ERangeForCharge(elapsed));
        const Vector3 endpoint = player.Position().Extend(aim, std::min(range, player.Position().Distance2D(aim)));
        const int enemies = Engine::CountEnemiesAt(endpoint, kEKnockupRadius);
        const bool lethal = valid && ControllerHelpers::Lethal(target, EDamage(target));
        if (!EReleaseSafe(CurrentEState, elapsed, SDK::NavMesh::IsWall(endpoint),
                          Engine::UnderEnemyTurret(endpoint), enemies,
                          Slider(TacticsMenu, "MaxEEnemies", 3), lethal, fleeing)) return false;
        EEndpoint = endpoint;
        if (!Engine::ControllerCastPosition(2, endpoint)) return false;
        CurrentEState = EState::LaunchPending;
        LastCastTick[2] = Now();
        return true;
    }
    if (CurrentEState != EState::Ready || !Ready(2, mode, reactive)) return false;
    const bool valid = Engine::ValidEnemy(target, kERangeMax + 100.0f);
    if (!valid && !fleeing) return false;
    const Vector3 aim = valid ? PredictPosition(target, 0.12f) : Game::CursorPos();
    if (!aim.IsValid() || aim.IsZero() || SDK::NavMesh::IsWall(aim)) return false;
    const Vector3 endpoint = player.Position().Extend(aim, std::min(kERangeMax, player.Position().Distance2D(aim)));
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    CurrentEState = EState::Charging;
    ECastTick = LastCastTick[2] = Now();
    EEndpoint = endpoint;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false, bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) ||
        player.Position().Distance2D(target.Position()) > kRRadius + target.BoundingRadius() ||
        !Ready(3, mode, reactive) || CurrentRState != RState::Ready ||
        PreserveAttack(reactive)) return false;
    std::array<RCollisionTarget, 8> candidates{};
    int candidateCount = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (candidateCount >= static_cast<int>(candidates.size()) ||
            !Engine::ValidEnemy(enemy, kRRadius + 100.0f)) continue;
        candidates[static_cast<std::size_t>(candidateCount++)] = {
            static_cast<int>(enemy.NetworkId()), PredictPosition(enemy, 0.10f),
            enemy.BoundingRadius(), true};
    }
    if (FirstRHit(player.Position(), candidates) != static_cast<int>(target.NetworkId())) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const bool lethal = ControllerHelpers::Lethal(target, RDamage(target));
    if (!RCommitAllowed(CurrentRState, true, false, false,
                        Engine::UnderEnemyTurret(player.Position()), enemies,
                        Slider(TacticsMenu, "MaxREnemies", 3), lethal, fleeing)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    CurrentRState = RState::Bouncing;
    RBounceCount = 0;
    RCarryTargetId = static_cast<int>(target.NetworkId());
    RCarryTick = Now();
    RMissileId = 0;
    RCastTick = LastCastTick[3] = Now();
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CurrentEState == EState::Charging && CastE(target, Mode::Combo)) return;
    if (CurrentQState == QState::FirstHit && CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.HealthPercent() < 42.0f || !Engine::ValidEnemy(target)) return;
    if (CurrentQState == QState::FirstHit && CastQ(target, Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.HealthPercent() < Slider(FarmMenu, "MinimumHealth", 35)) return;
    if (Engine::ValidEnemy(target) && CastW(target, mode)) return;
    if (mode != Mode::LastHit && Engine::ValidEnemy(target)) (void)CastQ(target, mode);
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (CastE(target, Mode::Flee, true, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastR(target, Mode::Flee, true, true);
}

inline void Automatic(const AIHeroClient& target) {
    if (IncomingThreatUntil >= Now() && Engine::ValidEnemy(target)) {
        if (CastE(target, Mode::Automatic, true, true)) return;
        (void)CastR(target, Mode::Automatic, true, true);
        return;
    }
    if (Engine::ValidEnemy(target)) {
        if (CurrentQState == QState::FirstHit && CastQ(target, Mode::Automatic, true)) return;
        if (CastW(target, Mode::Automatic, true)) return;
        (void)CastQ(target, Mode::Automatic, true);
    }
}

inline void ReconcileState() {
    const int now = Now();
    if (CurrentPassiveState == PassiveState::Cooldown && now >= PassiveCooldownEndTick) CurrentPassiveState = PassiveState::Ready;
    if (CurrentQState == QState::FirstHit && now > QFirstCastTick + kQPairWindowMs) {
        CurrentQState = QState::Ready; QFirstTargetId = 0; QFirstPosition = {};
    }
    if (CurrentEState != EState::Ready && now > ECastTick + kEChargeMaxMs + 900) {
        CurrentEState = EState::Ready; EEndpoint = {};
    }
    if (RCarryTargetId != 0 && now > RCarryTick + kRCarryDurationMs) RCarryTargetId = 0;
    if (CurrentRState == RState::Bouncing) {
        RBounceCount = std::min(kRBounces, std::max(0, (now - RCastTick) / kRBounceIntervalMs));
        if (RBounceCount >= kRBounces) { CurrentRState = RState::Ready; RCarryTargetId = 0; }
    }
    if (CurrentRState != RState::Ready && now > RCastTick + kRBounces * kRBounceIntervalMs + 1500) {
        CurrentRState = RState::Ready; RBounceCount = 0; RCarryTargetId = 0;
    }
    if (IncomingThreatUntil < now) IncomingThreatUntil = IncomingThreatTargetId = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, kERangeMax + 200.0f);
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
    TacticsMenu = root->AddSubMenu(new Menu("ZacTactics", "Zac tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("ZacFarming", "Zac farming"));
    TacticsMenu->Add(new MenuSlider("MaxEEnemies", "Maximum enemies at E landing", 3, 0, 5));
    TacticsMenu->Add(new MenuSlider("MaxREnemies", "Maximum enemies during R", 3, 0, 5));
    TacticsMenu->Add(new MenuSlider("WMinimumHealth", "Minimum health percent for W", 20, 1, 100));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast protection (ms)", 650, 0, 2000));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve attack windup", true));
    FarmMenu->Add(new MenuSlider("MinimumHealth", "Farm minimum health percent", 35, 1, 100));
}
inline void OnLoad() {
    CurrentPassiveState = PassiveState::Ready; CurrentQState = QState::Ready;
    CurrentEState = EState::Ready; CurrentRState = RState::Ready;
    PassiveCooldownEndTick = BlobCount = QFirstTargetId = QFirstCastTick = WCastTick = 0;
    ECastTick = RCastTick = RBounceCount = RCarryTargetId = RMissileId = RCarryTick = 0;
    LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = IncomingThreatUntil = IncomingThreatTargetId = 0;
    QFirstPosition = EEndpoint = {}; BlobPositions = {}; LastCastTick.fill(0);
}

inline void OnUnload() { TacticsMenu = nullptr; FarmMenu = nullptr; OnLoad(); }

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack) { LastAutoTargetId = static_cast<int>(args.TargetNetworkId); LastAutoTick = now; return; }
        if (args.Slot < 0 || args.Slot > 3) return;
        if (!Engine::WasControllerCast(args.Slot)) ManualOwnershipUntil = now + static_cast<int>(Slider(TacticsMenu, "ManualOwnershipMs", 650));
        LastCastTick[static_cast<std::size_t>(args.Slot)] = now;
        if (args.Slot == 0) {
            if (CurrentQState == QState::Ready) { CurrentQState = QState::FirstHit; QFirstCastTick = now; QFirstTargetId = static_cast<int>(args.TargetNetworkId); QFirstPosition = args.CastPosition; }
            else CurrentQState = QState::Ready;
        } else if (args.Slot == 1) WCastTick = now;
        else if (args.Slot == 2) { if (CurrentEState == EState::Charging) CurrentEState = EState::LaunchPending; else { CurrentEState = EState::Charging; ECastTick = now; } }
        else { CurrentRState = RState::Bouncing; RCastTick = now; }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatTargetId = static_cast<int>(args.Sender.NetworkId);
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId); LastAutoTick = Now();
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "zacpassive")) { CurrentPassiveState = PassiveState::Split; PassiveCooldownEndTick = 0; }
    else if (Engine::TextContains(args.BuffName, "zacq")) CurrentQState = QState::FirstHit;
    else if (Engine::TextContains(args.BuffName, "zaceprepare")) { CurrentEState = EState::Charging; ECastTick = Now(); }
    else if (Engine::TextContains(args.BuffName, "zacr")) CurrentRState = RState::Bouncing;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "zacpassive")) { CurrentPassiveState = PassiveState::Cooldown; PassiveCooldownEndTick = Now() + 30000; }
    else if (Engine::TextContains(args.BuffName, "zacq")) CurrentQState = QState::Ready;
    else if (Engine::TextContains(args.BuffName, "zaceprepare")) CurrentEState = EState::Ready;
    else if (Engine::TextContains(args.BuffName, "zacr")) { CurrentRState = RState::Ready; RBounceCount = 0; }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) { LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); LastAutoTick = Now(); }
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) { (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick); }
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, EEndpoint, IncomingThreatUntil, kEKnockupRadius, 1200);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !ControllerHelpers::ObjectEventIsAllied(args)) return;
    if (ControllerHelpers::AnyTextContains({args.Sender.Name, args.Sender.CharacterName}, {"zacblob", "zaccell", "zacsplat"})) {
        BlobCount = std::min(4, BlobCount + 1);
        for (auto& position : BlobPositions) if (position.IsZero()) { position = args.Sender.Position; break; }
    }
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (ControllerHelpers::AnyTextContains({args.Sender.Name, args.Sender.CharacterName}, {"zacblob", "zaccell", "zacsplat"})) BlobCount = std::max(0, BlobCount - 1);
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName}, {"zacq", "zace", "zacr"})) {
        if (args.MissileNetworkId != 0) RMissileId = static_cast<int>(args.MissileNetworkId);
    }
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (RMissileId != 0 && static_cast<int>(args.MissileNetworkId) == RMissileId) RMissileId = 0;
}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Cell Division passive split creates four tracked blobs, cooldown and pickup healing",
    "Stretching Strikes Q first hit opens a four-second second-target pair slam",
    "Unstable Matter W spends current health and scales burst against target max health",
    "Elastic Slingshot E charge range ramps from 400 to 1800 and lands a knockup",
    "E release rejects walls, turrets and excessive enemies while allowing lethal/Flee escape",
    "Let's Bounce! R performs four 500ms bounces and carries a safe first target",
    "R endpoint and enemy-count safety gates prevent unsafe turret commits",
    "selected target preference with orbwalker fallback, attack windup and manual ownership",
    "event plus polling reconciliation tracks passive, Q, E, R state and blob objects",
    "combo, harass, lane clear, jungle, last hit, flee and automatic behavior",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Zac;
    controller.ControllerId = "champion.kuroaio.ai.zac.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIZac.md";
    controller.ImplementationSummary = "Cell Division blobs, paired Q, health-cost W, charge-safe E and four-bounce R carry controller.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad; controller.OnUnload = &OnUnload; controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate; controller.OnDraw = &OnDraw; controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnDoCast; controller.OnBuffAdd = &OnBuffAdd; controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBeforeAttack = &OnBeforeAttack; controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser; controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate; controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate; controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Zac
