#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AINilahGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Nilah {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Ready;
using ControllerHelpers::PreserveAttack;

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
inline int LastAllyId = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline int QEmpoweredUntil = 0;
inline int WActiveUntil = 0;
inline int WAllyUntil = 0;
inline int ETargetId = 0;
inline int ERecastUntil = 0;
inline int RStartTick = 0;
inline bool QEmpowered = false;
inline bool WActive = false;
inline bool ERecastReady = false;
inline bool RChanneling = false;
inline bool PassiveSharingObserved = false;
inline int PassiveExperienceUntil = 0;
inline int SharedHealUntil = 0;
inline int SharedShieldUntil = 0;
inline SharedResourceState SharedState{};

inline bool Throttle(int slot, int delay = 55) {
    return slot >= 0 && slot < 4 && LastCastTick[static_cast<std::size_t>(slot)] + delay <= Now();
}

inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline bool Threatened(const AIHeroClient& unit, float health = 70.0f) {
    return unit.IsValid() && unit.HealthPercent() <= health &&
        Engine::CountEnemiesAt(unit.Position(), 650.0f) > 0;
}

inline AIHeroClient SelectAlly(bool threatenedOnly = false) {
    const auto ally = SelectProtectionAlly(
        kERange, LastAllyId, LastAllyId == 0 ? 0 : Now() + 300,
        threatenedOnly ? 360.0f : 200.0f, 500.0f);
    if (Engine::ValidAlly(ally, kERange) && (!threatenedOnly || Threatened(ally))) {
        LastAllyId = static_cast<int>(ally.NetworkId());
        return ally;
    }
    const auto player = GameObjects::Player();
    if (player.IsValid() && (!threatenedOnly || Threatened(player))) return player;
    return {};
}

inline float QDamage(const AIHeroClient& target, bool empowered = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const int rank = std::clamp(SpellRank(0), 1, 5);
    const float base = 5.0f * static_cast<float>(rank);
    const float raw = empowered ? player.TotalAttackDamage() + base + player.BonusAttackDamage() * 0.15f :
        base + player.TotalAttackDamage();
    return player.CalculatePhysicalDamage(target, raw);
}

inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const int rank = std::clamp(SpellRank(2), 1, 5);
    return player.CalculatePhysicalDamage(target,
        20.0f * static_cast<float>(rank) + player.BonusAttackDamage() * 0.20f);
}

inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const int rank = std::clamp(SpellRank(3), 1, 3);
    return player.CalculatePhysicalDamage(target,
        15.0f * static_cast<float>(rank) + player.BonusAttackDamage() * 0.50f +
        player.AP() * 0.70f);
}

inline bool AimQ(const AIHeroClient& target, Vector3& aim) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange)) return false;
    aim = PredictPosition(target, kQDelaySeconds);
    if (!aim.IsValid() || aim.IsZero() || !WithinReach(player.Position(), aim, kQRange,
        target.BoundingRadius()) || !QHits(player.Position(), aim, target.Position(),
        target.BoundingRadius()) || ControllerHelpers::ProjectileWallBlocksFromPlayer(aim,
            kQHalfWidth * 0.5f)) return false;
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive, reactive)) return false;
    Vector3 aim{};
    if (!AimQ(target, aim)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    QEmpowered = true;
    QEmpoweredUntil = Now() + 4000;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 90) ||
        PreserveAttack(reactive, reactive)) return false;
    const auto ally = SelectAlly(true);
    const bool allyThreatened = ally.IsValid() && Threatened(ally);
    const bool threat = IncomingThreatUntil > Now() || IncomingHardCcUntil > Now();
    if (!WDefensiveValue(player.HealthPercent(), threat, allyThreatened, reactive) &&
        !QEmpowered && !Engine::ValidEnemy(target)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    WActive = true;
    WActiveUntil = Now() + static_cast<int>(kWDurationSeconds * 1000.0f);
    WAllyUntil = allyThreatened ? Now() + static_cast<int>(kWAllyDurationSeconds * 1000.0f) : 0;
    return true;
}

inline bool SafeDashTo(const Vector3& endpoint, bool fleeing, bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || endpoint.IsZero()) return false;
    const int enemies = Engine::CountEnemiesAt(endpoint, 300.0f);
    return DashSafe(endpoint, SDK::NavMesh::IsWall(endpoint),
        Engine::UnderEnemyTurret(endpoint) && !Engine::UnderEnemyTurret(player.Position()),
        enemies, Slider(EMenu, "MaxDashEnemies", 2), fleeing, lethal);
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false, bool allyTarget = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() || !Engine::ValidAlly(target, kERange) &&
        !Engine::ValidEnemy(target, kERange)) return false;
    if (!Ready(2, mode) || !Throttle(2, 70) || PreserveAttack(reactive, reactive)) return false;
    const bool enemy = Engine::ValidEnemy(target, kERange);
    const bool lethal = enemy && Lethal(target, EDamage(target));
    const Vector3 endpoint = DashEndpoint(player.Position(), target.Position());
    if (!SafeDashTo(endpoint, fleeing, lethal)) return false;
    if (!allyTarget && !enemy) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastCastTick[2] = Now();
    ETargetId = static_cast<int>(target.NetworkId());
    ERecastReady = true;
    ERecastUntil = Now() + 700;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 120) ||
        PreserveAttack(reactive, reactive)) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const bool lethal = Engine::ValidEnemy(target, kRRadius) && Lethal(target, RDamage(target));
    const bool healNeeded = player.HealthPercent() <= Slider(RMenu, "HealAt", 48);
    const RDecisionContext context{
        enemies >= Slider(RMenu, "MinimumEnemies", 2), lethal, healNeeded,
        Engine::UnderEnemyTurret(player.Position()),
        enemies <= Slider(TacticsMenu, "MaxCommitEnemies", 2) || lethal, reactive};
    if (!ShouldCastR(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    RStartTick = Now();
    RChanneling = true;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 50)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (CastW(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& pursuer) {
    if (CastW(pursuer, Mode::Flee, true)) return;
    const auto ally = SelectAlly(true);
    if (ally.IsValid() && CastE(ally, Mode::Flee, true, true, true)) return;
    if (Engine::ValidEnemy(pursuer)) (void)CastE(pursuer, Mode::Flee, true, true);
}

inline bool Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const auto ally = SelectAlly(true);
    if (Threatened(player) || (ally.IsValid() && Threatened(ally))) {
        if (CastW(target, Mode::Automatic, true)) return true;
        if (ally.IsValid() && CastE(ally, Mode::Automatic, true, true, true)) return true;
    }
    if (IncomingHardCcUntil > Now() && CastW(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && Lethal(target, QDamage(target, true)) &&
        CastQ(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && CastR(target, Mode::Automatic, true)) return true;
    return false;
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (QEmpowered && now > QEmpoweredUntil + 200) QEmpowered = false;
    if (WActive && now > WActiveUntil + 200) WActive = false;
    if (ERecastReady && now > ERecastUntil + 150) {
        ERecastReady = false;
        ETargetId = 0;
    }
    if (RChanneling && now > RStartTick + static_cast<int>(kRDurationSeconds * 1000.0f) + 250)
        RChanneling = false;
    if (PassiveExperienceUntil > 0 && now > PassiveExperienceUntil) PassiveExperienceUntil = 0;
    if (SharedHealUntil > 0 && now > SharedHealUntil) SharedHealUntil = 0;
    if (SharedShieldUntil > 0 && now > SharedShieldUntil) SharedShieldUntil = 0;
    if (player.HasBuff("NilahP") || player.HasBuff("NilahPassive") ||
        player.HasBuff("NilahJoyUnending")) {
        PassiveExperienceUntil = now + 1800;
        SharedState.PassiveObserved = true;
        SharedState.LastEventTick = now;
    }
    if (player.HasBuff("NilahQ") || player.HasBuff("NilahQEmpowered")) {
        QEmpowered = true;
        QEmpoweredUntil = now + 800;
    }
    if (player.HasBuff("NilahW") || player.HasBuff("NilahWBuff")) {
        WActive = true;
        WActiveUntil = now + static_cast<int>(kWDurationSeconds * 1000.0f);
    }
    if (player.HasBuff("NilahR")) RChanneling = true;
    PassiveSharingObserved = PassiveShareValid(SharedState, now);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const auto target = PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f : kQRange);
    if (ManualOwnershipUntil > Now()) return true;
    if (mode == Mode::Automatic && Automatic(target)) return true;
    if (mode == Mode::Combo) Combo(target);
    else if (mode == Mode::Harass) Harass(target);
    else if (mode == Mode::Flee) Flee(NearestEnemyToPlayer(target, 1000.0f));
    else if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        if (GameObjects::Player().IsValid() &&
            GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "Mana", 38))
            (void)Engine::TryFarm(mode);
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot <= 3 && !Engine::WasControllerCast(slot))
            ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 650);
        if (slot == 0 && !Engine::WasControllerCast(0)) {
            QEmpowered = true;
            QEmpoweredUntil = now + 4000;
        }
        if (slot == 1 && !Engine::WasControllerCast(1)) {
            WActive = true;
            WActiveUntil = now + static_cast<int>(kWDurationSeconds * 1000.0f);
        }
        if (slot == 2 && !Engine::WasControllerCast(2)) {
            ERecastReady = true;
            ERecastUntil = now + 700;
        }
        if (slot == 3 && !Engine::WasControllerCast(3)) {
            RChanneling = true;
            RStartTick = now;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        IncomingHardCcUntil = std::max(IncomingHardCcUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = Now();
    if (Engine::TextContains(args.BuffName, "NilahQ")) {
        QEmpowered = true;
        QEmpoweredUntil = now + 4000;
    }
    if (Engine::TextContains(args.BuffName, "NilahW")) {
        WActive = true;
        WActiveUntil = now + static_cast<int>(kWDurationSeconds * 1000.0f);
    }
    const bool passive = Engine::TextContains(args.BuffName, "JoyUnending") ||
        Engine::TextContains(args.BuffName, "NilahPassive") ||
        Engine::TextContains(args.BuffName, "NilahP");
    if (passive) {
        PassiveSharingObserved = true;
        PassiveExperienceUntil = now + 1800;
        SharedState.PassiveObserved = true;
        SharedState.LastEventTick = now;
    }
    if (Engine::TextContains(args.BuffName, "Heal") ||
        Engine::TextContains(args.BuffName, "NilahRHeal")) {
        SharedHealUntil = now + 1800;
        RecordSharedHeal(SharedState, static_cast<int>(args.Sender.NetworkId), 0.0f, now);
    }
    if (Engine::TextContains(args.BuffName, "Shield") ||
        Engine::TextContains(args.BuffName, "NilahPassiveShield")) {
        SharedShieldUntil = now + 1800;
        RecordSharedShield(SharedState, static_cast<int>(args.Sender.NetworkId), 0.0f, now);
    }
    if (Engine::TextContains(args.BuffName, "NilahR")) {
        RChanneling = true;
        RStartTick = now;
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "NilahQ")) QEmpowered = false;
    if (Engine::TextContains(args.BuffName, "NilahW")) WActive = false;
    if (Engine::TextContains(args.BuffName, "NilahR")) RChanneling = false;
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (args.NetworkId != 0) IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 550);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    if (args.NetworkId != 0) IncomingHardCcUntil = std::max(IncomingHardCcUntil, Now() + 650);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
    if (QEmpowered && LastAutoTick > 0) QEmpowered = false;
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (args.Sender.IsValid() && IsLocalPlayer(args.Sender) && args.IsAutoAttack)
        LastAutoTick = Now();
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFDD66FFu, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF66DDFFu, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFFF8866u, 1.4f, 40);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("NilahOneTrick", "Nilah tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 650, 180, 1200));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at dash", 2, 1, 5));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Formless Blade"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 50, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Jubilant Veil"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Slipstream"));
    EMenu->Add(new MenuSlider("MaxDashEnemies", "Maximum enemies at dash endpoint", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Apotheosis"));
    RMenu->Add(new MenuSlider("MinimumEnemies", "Minimum enemies for R", 2, 1, 5));
    RMenu->Add(new MenuSlider("HealAt", "Use R below health percent", 48, 10, 85));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("NilahFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 38, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("NilahCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E/R ranges", false));
}

inline void ResetState() {
    LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = LastAllyId = ManualOwnershipUntil = 0;
    IncomingThreatUntil = IncomingHardCcUntil = QEmpoweredUntil = WActiveUntil = 0;
    WAllyUntil = ETargetId = ERecastUntil = RStartTick = 0;
    PassiveExperienceUntil = SharedHealUntil = SharedShieldUntil = 0;
    QEmpowered = WActive = ERecastReady = RChanneling = PassiveSharingObserved = false;
    SharedState = {};
}

inline void OnLoad() { ResetState(); }
inline void OnUnload() {
    ResetState();
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Pin Nilah passive, Q, W, E and R values to Riot 26.15 / CommunityDragon 16.15",
    "Reconcile Joy Unending experience, shared healing and shared shielding from buffs and polling",
    "Track passive ally recipient and expire stale shared-resource observations safely",
    "Predict Formless Blade with 600 range, 75 width, 2200 speed and projectile-wall rejection",
    "Record Formless Blade empowered-attack state and consume it after the owned attack",
    "Preserve ordinary AA windup while allowing reactive W, E or lethal Q ownership",
    "Use selected enemy before orbwalker fallback for every combat decision",
    "Cast Jubilant Veil only for incoming threat, threatened ally or an empowered trade",
    "Track Jubilant Veil duration and ally-sharing window from buff, spell and polling state",
    "Allow Slipstream through an enemy or ally only when the clamped endpoint is valid",
    "Recast Slipstream within its observed short window and clear stale recast ownership",
    "Reject E wall, enemy-turret and overcommitted endpoints unless the dash is verified lethal",
    "Use Apotheosis radius, multi-target count, lethal damage and missing-health healing gates",
    "Reject unsafe Apotheosis turret commits while permitting explicit manual defensive healing",
    "Record enemy process-spell, hard-CC and gapcloser threat windows for automatic W peel",
    "Automatic mode permits defensive W/E, lethal Q and safe R only",
    "Combo follows Q empower, E chase, W evasion and safe R multi-target route",
    "Harass reserves mana and limits the route to Q, W and E",
    "Flee uses W first, then ally-safe E and only then a pursuer dash",
    "LaneClear, Jungle and LastHit use shared farming policy above Nilah mana reserve",
    "Draw Q/E/R ranges without changing cast decisions",
    "Never automate items, summoners, flash or movement ownership",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Nilah";
    controller.ControllerId = "champion.kuroaio.ai.nilah.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AINilah.md";
    controller.ImplementationSummary =
        "Nilah passive resource reconciliation with Q empowered attacks, W evasion and ally share, E safe recast dash and R multi-target damage/healing gates.";
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
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Nilah
