#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIKledGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Kled {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline MountState Mount = MountState::Mounted;
inline float Courage = 100.0f;
inline bool PassiveCooldown = false;
inline bool QChainActive = false;
inline int QTargetId = 0;
inline int QCastTick = 0;
inline int QTetherExpireTick = 0;
inline bool WActive = false;
inline int WStage = 0;
inline int WStartTick = 0;
inline int EActiveTick = 0;
inline int ETargetId = 0;
inline bool ERecastReady = false;
inline bool RCharging = false;
inline int RCastTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline std::array<int, 4> LastCastTick{};

inline bool Mounted() { return Mount == MountState::Mounted; }
inline bool Dismounted() { return Mount == MountState::Dismounted; }
inline bool Remounting() { return Mount == MountState::Remounting; }

inline bool Throttle(int slot, int delay = 55) {
    return slot >= 0 && slot < 4 &&
        LastCastTick[static_cast<std::size_t>(slot)] + delay <= Now();
}

inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        ControllerHelpers::HasSpellShieldOrImmunity(target);
}

inline float QDamage(const AIHeroClient& target, bool tetherPop = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = QDamageRaw(SpellRank(0), player.TotalAttackDamage(),
                                 Mounted(), tetherPop);
    return player.CalculatePhysicalDamage(target, raw);
}

inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = WFourthHitRaw(SpellRank(1), target.MaxHealth(),
                                   player.TotalAttackDamage());
    return player.CalculatePhysicalDamage(target, raw);
}

inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(
        target, EDamageRaw(SpellRank(2), player.TotalAttackDamage()));
}

inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(
        target, RDamageRaw(SpellRank(3), player.BonusAttackDamage()));
}

inline void ClearQChain() {
    QChainActive = false;
    QTargetId = 0;
    QCastTick = 0;
    QTetherExpireTick = 0;
}

inline void ClearE() {
    EActiveTick = 0;
    ETargetId = 0;
    ERecastReady = false;
}

inline void ReconcileMount() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Courage = ClampCourage(player.Mana());
    PassiveCooldown = player.HasBuff("KledPassiveCooldown");
    MountObservation observation{};
    observation.MountedBuff = player.HasBuff("KledRider") ||
        player.HasBuff("KledMounted") || player.HasBuff("KledMount");
    observation.DismountedBuff = player.HasBuff("KledDismounted") ||
        player.HasBuff("KledDismount");
    observation.RemountBuff = player.HasBuff("KledRemount") ||
        player.HasBuff("KledMounting");
    observation.PassiveCooldown = PassiveCooldown;
    observation.Courage = Courage;
    observation.Previous = Mount;
    if (observation.MountedBuff || observation.DismountedBuff ||
        observation.RemountBuff || Dismounted() || Remounting()) {
        Mount = ResolveMountState(observation);
    }
    if (Dismounted() && CourageCanRemount(Courage, false, PassiveCooldown)) {
        Mount = MountState::Remounting;
    }
    if (Remounting() && observation.MountedBuff) Mount = MountState::Mounted;
}

inline void ReconcileQ() {
    if (!QChainActive) return;
    const auto player = GameObjects::Player();
    const auto target = HeroByNetworkId(QTargetId);
    const int now = Now();
    if (!player.IsValid() || !Engine::ValidEnemy(target) ||
        now > QTetherExpireTick + 150 ||
        !QChainCanHold(player.Position(), PredictPosition(target, 0.05f),
                       static_cast<float>(now - QCastTick) / 1000.0f)) {
        ClearQChain();
    }
}

inline void ReconcileE() {
    if (EActiveTick == 0) return;
    const auto player = GameObjects::Player();
    const auto target = HeroByNetworkId(ETargetId);
    if (!player.IsValid() || Now() > EActiveTick +
            static_cast<int>(kERecastSeconds * 1000.0f) + 150 ||
        (!Engine::ValidEnemy(target) && !ERecastReady)) {
        ClearE();
    }
}

inline bool CanCastQ(const AIHeroClient& target, bool reactive,
                     Vector3& aim) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) ||
        !Engine::ValidEnemy(target, kQRange) || !Mounted() ||
        !Ready(0) || !SpellEnabled(0, Mode::Combo) || !Throttle(0)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    aim = prediction.GetCastPosition();
    if (!FinitePoint(aim) || !QLineHits(player.Position(), aim,
                                        target.Position(), target.BoundingRadius()) ||
        !prediction.CollisionObjects.empty() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) {
        return false;
    }
    const bool lethal = ControllerHelpers::Lethal(target, QDamage(target));
    if (!reactive && PreserveWCadence(WStage, WActive, lethal, false)) return false;
    if (!reactive && ControllerHelpers::PreserveAttack(false, lethal)) return false;
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!Ready(0, mode) || !Throttle(0) || Protected(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Mounted() || !Engine::ValidEnemy(target, kQRange)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition();
    if (!FinitePoint(aim) || !QLineHits(player.Position(), aim,
                                        target.Position(), target.BoundingRadius()) ||
        !prediction.CollisionObjects.empty() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    const bool lethal = ControllerHelpers::Lethal(target, QDamage(target));
    if (!reactive && PreserveWCadence(WStage, WActive, lethal, false)) return false;
    if (!reactive && ControllerHelpers::PreserveAttack(false, lethal)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    QChainActive = true;
    QTargetId = static_cast<int>(target.NetworkId());
    QCastTick = Now();
    QTetherExpireTick = QCastTick + static_cast<int>(kQTetherSeconds * 1000.0f);
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Engine::ValidEnemy(target)) return false;
    const int now = Now();
    if (EActiveTick != 0 && ERecastReady) {
        const float elapsed = static_cast<float>(now - EActiveTick) / 1000.0f;
        const Vector3 aim = PredictPosition(target, 0.15f);
        if (!CanRecastE(player.Position(), aim, true, elapsed) ||
            !DashEndpointValid(player.Position(), aim, SDK::NavMesh::IsWall(aim),
                               Engine::UnderEnemyTurret(aim),
                               Engine::CountEnemiesAt(aim, 550.0f),
                               Slider(EMenu, "MaxDashEnemies", 2),
                               ControllerHelpers::Lethal(target, EDamage(target)),
                               kERecastRange) || !Ready(2, mode) || !Throttle(2)) return false;
        if (!Engine::ControllerCastPosition(2, aim)) return false;
        LastCastTick[2] = now;
        ERecastReady = false;
        return true;
    }
    if (EActiveTick != 0 || !Mounted() || !Ready(2, mode) || !Throttle(2) ||
        !Engine::ValidEnemy(target, kERange)) return false;
    const Vector3 aim = PredictPosition(target, 0.25f);
    if (!FinitePoint(aim)) return false;
    const bool lethal = ControllerHelpers::Lethal(target, EDamage(target));
    if (!DashEndpointValid(player.Position(), aim, SDK::NavMesh::IsWall(aim),
                           Engine::UnderEnemyTurret(aim),
                           Engine::CountEnemiesAt(aim, 550.0f),
                           Slider(EMenu, "MaxDashEnemies", 2), lethal, kERange)) return false;
    if (!reactive && PreserveWCadence(WStage, WActive, lethal, false)) return false;
    if (!reactive && ControllerHelpers::PreserveAttack(false, lethal)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = now;
    EActiveTick = now;
    ETargetId = static_cast<int>(target.NetworkId());
    ERecastReady = false;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3) || RCharging ||
        !Mounted()) return false;
    Vector3 endpoint = target.IsValid() ? PredictPosition(target, 0.1f) : Game::CursorPos();
    if (!ChargeEndpointWithin(player.Position(), endpoint)) return false;
    const bool hasTarget = Engine::ValidEnemy(target);
    const bool lethal = hasTarget && ControllerHelpers::Lethal(target, RDamage(target));
    const int enemies = Engine::CountEnemiesAt(endpoint, kRZoneRadius);
    const int allies = Engine::CountAlliesAt(endpoint, 800.0f);
    if (!SafeChargeEndpoint(endpoint, SDK::NavMesh::IsWall(endpoint),
                            Engine::UnderEnemyTurret(endpoint), enemies, allies,
                            Slider(RMenu, "MaxEndpointEnemies", 2), hasTarget, lethal)) return false;
    if (!reactive && !lethal && Engine::CountEnemiesAt(player.Position(), 700.0f) == 0 &&
        target.IsValid() && player.Position().Distance2D(target.Position()) < 900.0f) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    LastCastTick[3] = Now();
    RCharging = true;
    RCastTick = Now();
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kRMaxRange)) return;
    if (CastR(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (WFourthHitReady(WStage, WActive) && ControllerHelpers::Lethal(target, WDamage(target))) return;
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kQRange)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Engine::CountEnemiesAt(player.Position(), 1000.0f) > 0) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, kERange) && CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target, kRMaxRange)) (void)CastR({}, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& fallback) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    AIHeroClient threat = HeroByNetworkId(IncomingThreatTargetId);
    if (!Engine::ValidEnemy(threat, kQRange)) threat = fallback;
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(threat, kERange) &&
        CastE(threat, Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(threat, kQRange) &&
        ControllerHelpers::Lethal(threat, QDamage(threat)) &&
        CastQ(threat, Mode::Automatic, true)) return;
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    ReconcileMount();
    ReconcileQ();
    ReconcileE();
    if (RCharging && Now() > RCastTick +
            static_cast<int>(kRMaxChargeSeconds * 1000.0f) + kRChargeGraceMs) {
        RCharging = false;
    }
    const AIHeroClient target = Engine::SelectTarget(
        mode == Mode::Flee ? kRMaxRange : kQRange);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, kERange)); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const bool dismountSpell =
            Engine::TextContains(args.SpellName, "KledDisMount") ||
            Engine::TextContains(args.ScriptName, "KledDisMount");
        if (dismountSpell) Mount = MountState::Dismounted;
        const int slot = static_cast<int>(args.Slot);
        if (slot == 0 && Mounted()) {
            QTargetId = static_cast<int>(args.TargetNetworkId);
            QCastTick = now;
            QTetherExpireTick = now + static_cast<int>(kQTetherSeconds * 1000.0f);
            QChainActive = QTargetId != 0;
        } else if (slot == 2) {
            const bool recast = Engine::TextContains(args.SpellName, "KledE2") ||
                Engine::TextContains(args.ScriptName, "KledE2");
            if (recast) {
                ERecastReady = false;
            } else {
                EActiveTick = now;
                ETargetId = static_cast<int>(args.TargetNetworkId);
                ERecastReady = false;
            }
        } else if (slot == 3) {
            RCharging = true;
            RCastTick = now;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick,
                                       analysis.LineThreatUntilTick);
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "KledDismount") ||
        Engine::TextContains(args.BuffName, "KledDismounted")) {
        Mount = MountState::Dismounted;
    }
    if (Engine::TextContains(args.BuffName, "KledRemount") ||
        Engine::TextContains(args.BuffName, "KledMounting")) {
        Mount = MountState::Remounting;
    }
    if (Engine::TextContains(args.BuffName, "KledRider") ||
        Engine::TextContains(args.BuffName, "KledMounted")) {
        Mount = MountState::Mounted;
    }
    if (Engine::TextContains(args.BuffName, "KledPassiveCooldown")) PassiveCooldown = true;
    if (Engine::TextContains(args.BuffName, "KledWActive")) {
        WActive = true;
        WStage = 0;
        WStartTick = Now();
    }
    if (Engine::TextContains(args.BuffName, "KledE2Target")) ERecastReady = true;
    if (Engine::TextContains(args.BuffName, "KledR")) RCharging = true;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "KledPassiveCooldown")) PassiveCooldown = false;
    if (Engine::TextContains(args.BuffName, "KledWActive")) {
        WActive = false;
        WStage = 0;
        WStartTick = 0;
    }
    if (Engine::TextContains(args.BuffName, "KledE2Target")) ERecastReady = false;
    if (Engine::TextContains(args.BuffName, "KledR")) RCharging = false;
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    IncomingThreatTargetId = static_cast<int>(args.NetworkId);
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 650);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    IncomingThreatTargetId = static_cast<int>(args.NetworkId);
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 700);
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

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    if (!WActive && GameObjects::Player().HasBuff("KledWActive")) {
        WActive = true;
        WStage = 0;
        WStartTick = Now();
    }
    if (WActive && Mounted()) {
        WStage = AdvanceWAttack(WStage, true);
        if (WStage == 0) WActive = false;
    }
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQDisplayRange, 0xFFFFAA40u, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF66DDFFu, 1.2f, 32);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("KledTactics", "Kled courage tactics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Bear Trap on a Rope"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Violent Tendencies"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Joust"));
    EMenu->Add(new MenuSlider("MaxDashEnemies", "Maximum enemies at dash endpoint", 2, 0, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Chaaaaaaaarge!!!"));
    RMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at charge endpoint", 2, 0, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("KledFarm", "Farm policy"));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("KledCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E ranges", false));
}

inline void ResetState() {
    Mount = MountState::Mounted;
    Courage = 100.0f;
    PassiveCooldown = false;
    ClearQChain();
    WActive = false;
    WStage = 0;
    WStartTick = 0;
    ClearE();
    RCharging = false;
    RCastTick = 0;
    IncomingThreatUntil = 0;
    IncomingThreatTargetId = 0;
    LastAutoTargetId = 0;
    LastAutoTick = 0;
    LastCastTick.fill(0);
}

inline void OnLoad() { ResetState(); }
inline void OnUnload() {
    ResetState();
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Pin Kled Q/W/E/R values to Riot 26.15 and CommunityDragon 16.15",
    "Poll Courage as the 0-100 special resource and reconcile Skaarl mount state",
    "Observe dismount, remounting and passive cooldown transitions from buffs and polling",
    "Keep a mounted Q hit line collision-safe and track its 1.75-second tether leash",
    "Clear Q chain state when the target leaves the 700-unit tether or the pop window expires",
    "Preserve Violent Tendencies auto cadence and count all four landed attacks",
    "Use fourth-hit percent-health physical damage for lethal checks without casting W",
    "Refuse mounted Joust when no mounted state or when its 550-unit dash is unsafe",
    "Track KledE2Target and recast Joust only inside the marked 700-unit window",
    "Reject wall, turret and over-committed E endpoints unless the cast is lethal",
    "Charge R only to a valid endpoint inside the live rank-based global range",
    "Reject R wall, turret and enemy-count unsafe endpoints before committing movement",
    "Use autonomous target selection before orbwalker and engine fallback",
    "Preserve AA windup except reactive peel or verified lethal responses",
    "Resume after observed Q/W/E/R events while preserving AA windup",
    "Automatic mode only responds to bounded threats or verified Q kill-secure",
    "Combo opens with safe charge, then Q tether, Joust and W cadence follow-through",
    "Harass uses Q and E only when mounted, reachable and safe",
    "LaneClear, Jungle and LastHit use shared farm policy without inventing courage",
    "Reconcile stale Q, E and R state every update instead of trusting local casts forever",
    "Draw Q and E ranges without issuing movement orders",
    "Never automate items, summoners or turret dives",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Kled;
    controller.ControllerId = "champion.kuroaio.ai.kled.courage";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIKled.md";
    controller.ImplementationSummary =
        "Courage and Skaarl state reconciliation with Q tether, W four-hit cadence, "
        "safe E recast and bounded R charge endpoint planning.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Kled
