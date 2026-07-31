#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "../Profiles/AIMasterYi.h"
#include "AIMasterYiGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::MasterYi {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::AutoAttackRange;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Protected;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline std::array<int, 4> LastCastTick{};
inline int LastAttackTargetId = 0;
inline int MarkedTargetId = 0;
inline int MarkExpireTick = 0;
inline int MeditationStartTick = 0;
inline int MeditationUntilTick = 0;
inline int MeditationProtectedUntilTick = 0;
inline bool Meditating = false;
inline int HighlanderExpireTick = 0;
inline bool HighlanderActive = false;
inline int IncomingThreatTargetId = 0;
inline int IncomingThreatUntil = 0;
inline Vector3 IncomingThreatEndpoint{};
inline int PendingTakedownTargetId = 0;
inline int PendingTakedownHealth = 0;

inline bool Ready(int index, Mode mode, bool reactive = false) {
    return index >= 0 && index < 4 && Engine::RuntimeSpells[index] &&
        Engine::RuntimeSpells[index]->IsReady() && SpellEnabled(index, mode) &&
        (reactive || LastCastTick[static_cast<std::size_t>(index)] + 35 <= Now());
}

inline bool IncomingThreat() {
    return IncomingThreatUntil >= Now();
}

inline bool TargetIsMarked(const AIHeroClient& target) {
    return target.IsValid() && static_cast<int>(target.NetworkId()) == MarkedTargetId &&
        MarkExpireTick >= Now();
}

inline bool AlphaStrikeLandingAllowed(const AIHeroClient& target,
                                      const Vector3& landing,
                                      bool lethalDive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kAlphaStrikeRange + 90.0f) ||
        !AlphaStrikeReachable(player.Position(), landing, target.BoundingRadius())) {
        return false;
    }
    if (SDK::NavMesh::IsWall(landing)) return false;
    return AlphaStrikeLandingSafe(
        landing, false, Engine::UnderEnemyTurret(landing),
        Engine::CountEnemiesAt(landing, 475.0f),
        Slider(TacticsMenu, "MaxQEnemies", 2), lethalDive);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Meditating || !Ready(0, mode, reactive) ||
        !Engine::ValidEnemy(target, kAlphaStrikeRange + 90.0f) ||
        ControllerHelpers::PreserveAttack(reactive) && InAutoAttackRange(target, 15.0f)) {
        return false;
    }
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero() || !AlphaStrikeLandingAllowed(target, aim)) return false;
    if (ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 1.0f)) return false;
    if (!prediction.CollisionObjects.empty() && !Engine::IsHardCrowdControlled(target)) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    LastCastTick[0] = Now();
    LastAttackTargetId = static_cast<int>(target.NetworkId());
    MarkedTargetId = LastAttackTargetId;
    MarkExpireTick = Now() + kTargetMarkLifetimeMs;
    return true;
}
inline float WujuDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    static constexpr float baseDamage[] = {
        0.0f, 14.0f, 16.0f, 18.0f, 20.0f, 22.0f};
    const int rank = std::clamp(SpellRank(2), 1, 5);
    return WujuTrueDamage(baseDamage[rank], player.BonusAttackDamage());
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Meditating || !Ready(2, mode, reactive) ||
        !Engine::ValidEnemy(target, AutoAttackRange(target, kWujuAttackRangePadding)) ||
        ControllerHelpers::PreserveAttack(reactive) && !InAutoAttackRange(target, 15.0f)) {
        return false;
    }
    const bool lethal = WujuLethal(WujuDamage(target), target.Health(),
                                   target.AllShield(), Protected(target));
    const bool setup = TargetIsMarked(target) ||
        target.HealthPercent() <= Slider(TacticsMenu, "EHealth", 65);
    if (!lethal && !setup && mode != Mode::Combo) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Meditating || !Ready(1, mode, reactive)) return false;
    const bool low = player.HealthPercent() <= Slider(TacticsMenu, "WHealth", 38);
    const bool safe = MeditateStartAllowed(
        player.HealthPercent(), IncomingThreat(),
        Engine::CountEnemiesAt(player.Position(), 475.0f),
        Engine::UnderEnemyTurret(player.Position()),
        ControllerHelpers::PlayerMobilityLocked(),
        static_cast<float>(Slider(TacticsMenu, "WHealth", 38)));
    if (!safe || (!low && mode != Mode::Automatic && mode != Mode::Flee)) return false;
    if (Orbwalker::IsWindingUp() && !reactive) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    Meditating = true;
    MeditationStartTick = Now();
    MeditationUntilTick = Now() + static_cast<int>(kMeditateChannelSeconds * 1000.0f);
    MeditationProtectedUntilTick = MeditationUntilTick +
        static_cast<int>(kMeditateLingerSeconds * 1000.0f);
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool escape = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Meditating || HighlanderActive ||
        !Ready(3, mode, reactive)) return false;
    const bool validTarget = Engine::ValidEnemy(target, kAlphaStrikeRange + 100.0f);
    const bool killable = validTarget &&
        (WujuLethal(WujuDamage(target), target.Health(), target.AllShield(),
                    Protected(target)) || target.HealthPercent() <=
             Slider(TacticsMenu, "RStartHealth", 72));
    const bool inReach = validTarget &&
        player.Position().Distance2D(target.Position()) <= 850.0f;
    const bool allowed = HighlanderCommitAllowed(
        HighlanderActive, killable, inReach,
        validTarget && Engine::UnderEnemyTurret(target.Position()),
        validTarget ? Engine::CountEnemiesAt(target.Position(), 700.0f) :
                      Engine::CountEnemiesAt(player.Position(), 700.0f),
        Slider(TacticsMenu, "MaxCommitEnemies", 2), escape);
    if (!allowed && !(escape && IncomingThreat())) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    HighlanderActive = true;
    HighlanderExpireTick = Now() +
        static_cast<int>(kHighlanderDurationSeconds * 1000.0f);
    return true;
}

inline void ReconcileState() {
    const int now = Now();
    if (MarkedTargetId != 0 && now > MarkExpireTick) {
        MarkedTargetId = 0;
    }
    if (Meditating && now >= MeditationUntilTick) Meditating = false;
    if (HighlanderActive && now >= HighlanderExpireTick) HighlanderActive = false;
    if (IncomingThreatUntil < now) {
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
    if (PendingTakedownTargetId != 0 && now > MarkExpireTick) {
        PendingTakedownTargetId = 0;
    }
    if (HighlanderActive && PendingTakedownTargetId != 0) {
        const auto defeated = Engine::EnemyByNetworkId(PendingTakedownTargetId);
        if (!defeated.IsValid() || defeated.IsDead()) {
            HighlanderExpireTick = ExtendHighlander(
                HighlanderExpireTick, now, true);
            PendingTakedownTargetId = 0;
        }
    }
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kAlphaStrikeRange + 100.0f)) return;
    if (CastR(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (!InAutoAttackRange(target, 15.0f) && CastQ(target, Mode::Combo)) return;
    (void)CastW(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "HarassMana", 40)) return;
    if (Engine::ValidEnemy(target, 700.0f) && !InAutoAttackRange(target, 20.0f)) {
        (void)CastQ(target, Mode::Harass);
        return;
    }
    if (CastE(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (mode == Mode::Jungle && player.HealthPercent() <=
        Slider(TacticsMenu, "JungleWHealth", 55)) {
        (void)CastW({}, mode, true);
    }
    (void)Engine::TryFarm(mode);
}

inline void LastHit() {
    const auto target = ControllerHelpers::NearestEnemyToPlayer({}, 500.0f);
    if (Engine::ValidEnemy(target) && CastE(target, Mode::LastHit)) return;
    (void)Engine::TryFarm(Mode::LastHit);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, kAlphaStrikeRange + 100.0f) &&
        !InAutoAttackRange(target, 20.0f)) {
        if (CastQ(target, Mode::Flee, true)) return;
    }
    (void)CastR(target, Mode::Flee, true, true);
    (void)CastW(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    if (IncomingThreat()) {
        const auto threat = Engine::EnemyByNetworkId(IncomingThreatTargetId);
        if (Engine::ValidEnemy(threat, 800.0f) && CastR(threat, Mode::Automatic, true)) return;
        if (CastW(threat, Mode::Automatic, true)) return;
    }
    if (Engine::ValidEnemy(target, kAlphaStrikeRange + 100.0f)) {
        if (CastE(target, Mode::Automatic, true)) return;
        (void)CastQ(target, Mode::Automatic, true);
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(
        selected, kAlphaStrikeRange + 100.0f);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle: Farm(mode); break;
    case Mode::LastHit: LastHit(); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Master Yi tactics"));
    TacticsMenu->Add(new MenuSlider("MaxQEnemies", "Maximum enemies at Alpha landing", 2, 1, 5));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum Highlander commit enemies", 2, 1, 5));
    TacticsMenu->Add(new MenuSlider("EHealth", "Prime Wuju Style below target health", 65, 1, 100));
    TacticsMenu->Add(new MenuSlider("RStartHealth", "Allow Highlander below target health", 72, 1, 100));
    TacticsMenu->Add(new MenuSlider("WHealth", "Meditate below player health", 38, 1, 100));
    TacticsMenu->Add(new MenuSlider("JungleWHealth", "Jungle Meditation health", 55, 1, 100));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 40, 0, 100));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    LastAttackTargetId = 0;
    MarkedTargetId = 0;
    MarkExpireTick = 0;
    MeditationStartTick = 0;
    MeditationUntilTick = 0;
    MeditationProtectedUntilTick = 0;
    Meditating = false;
    HighlanderExpireTick = 0;
    HighlanderActive = false;
    IncomingThreatTargetId = 0;
    IncomingThreatUntil = 0;
    IncomingThreatEndpoint = {};
    PendingTakedownTargetId = 0;
    PendingTakedownHealth = 0;
}

inline void OnUnload() { TacticsMenu = nullptr; OnLoad(); }
inline void OnDraw() { ReconcileState(); }

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::WasControllerCast(static_cast<int>(args.Slot))) return;
        if (args.IsAutoAttack && args.Target.IsValid()) {
            LastAttackTargetId = static_cast<int>(args.Target.NetworkId);
            MarkedTargetId = LastAttackTargetId;
            MarkExpireTick = Now() + kTargetMarkLifetimeMs;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick,
                                       analysis.LineThreatUntilTick);
        IncomingThreatEndpoint = {};
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack && args.Target.IsValid()) {
        LastAttackTargetId = static_cast<int>(args.Target.NetworkId);
        MarkedTargetId = LastAttackTargetId;
        MarkExpireTick = Now() + kTargetMarkLifetimeMs;
        PendingTakedownTargetId = LastAttackTargetId;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "meditate")) {
        Meditating = true;
        MeditationStartTick = Now();
        MeditationUntilTick = args.EndTime > Game::Time()
            ? Now() + ControllerHelpers::RemainingMilliseconds(args.EndTime, 4000, 250, 6000)
            : Now() + 4000;
        MeditationProtectedUntilTick = MeditationUntilTick + 500;
    } else if (Engine::TextContains(args.BuffName, "highlander")) {
        HighlanderActive = true;
        HighlanderExpireTick = args.EndTime > Game::Time()
            ? Now() + ControllerHelpers::RemainingMilliseconds(args.EndTime, 7000, 500, 14000)
            : Now() + 7000;
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "meditate")) Meditating = false;
    if (Engine::TextContains(args.BuffName, "highlander")) HighlanderActive = false;
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "meditate") && args.EndTime <= Game::Time()) Meditating = false;
    if (Engine::TextContains(args.BuffName, "highlander") && args.EndTime <= Game::Time()) HighlanderActive = false;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
        MarkedTargetId = LastAttackTargetId;
        MarkExpireTick = Now() + kTargetMarkLifetimeMs;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
        PendingTakedownTargetId = LastAttackTargetId;
        MarkedTargetId = LastAttackTargetId;
        MarkExpireTick = Now() + kTargetMarkLifetimeMs;
    }
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
                           IncomingThreatUntil, 800.0f, 1200);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) { ReconcileState(); }
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) { ReconcileState(); }
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) { ReconcileState(); }
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) { ReconcileState(); }

inline constexpr const char* Scenarios[] = {
    "Alpha Strike predicted target reach, collision and safe landing",
    "Meditation damage reduction and protected channel interrupt gate",
    "Wuju Style true-damage on-hit lethal boundary",
    "Highlander commit, takedown extension and reset-aware kill chain",
    "Double Strike auto-weave and manual cast ownership",
    "selected target and orbwalker fallback policy",
    "jungle sustain, lane clear and last-hit Wuju timing",
    "flee Alpha reposition and automatic gapcloser response",
    "turret and enemy-count mobility safety",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "MasterYi";
    controller.ControllerId = "champion.kuroaio.ai.masteryi.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIMasterYi.md";
    controller.ImplementationSummary =
        "Master Yi Alpha Strike selection, protected Meditation, Wuju true-damage timing and Highlander reset chain.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::MasterYi
