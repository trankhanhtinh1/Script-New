#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AISionGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Sion {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CaptureInterruptableEvent;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline Mode LastMode = Mode::None;

inline bool QCharging = false;
inline bool QOwned = false;
inline int QChargeStartTick = 0;
inline int QTargetId = 0;
inline Vector3 QLastAim{};
inline bool WActive = false;
inline bool WOwned = false;
inline int WStartTick = 0;
inline int ETargetId = 0;
inline int ECastTick = 0;
inline bool EFirstBodyObserved = false;
inline bool RActive = false;
inline bool ROwned = false;
inline int RStartTick = 0;
inline int RTargetId = 0;
inline Vector3 RLastSteer{};
inline bool PassiveZombie = false;
inline int PassiveExpireTick = 0;

using ControllerHelpers::Now;

inline bool Ready(int slot, Mode mode) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() &&
        (mode == Mode::None || SpellEnabled(slot, mode));
}
inline bool Throttle(int slot, int delay = 100) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Protected;
inline bool Lethal(const AIHeroClient& target, int slot) {
    return Engine::ValidEnemy(target) && slot >= 0 && slot < 4 &&
        Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->GetDamage(target) >=
            target.Health() + target.AllShield();
}
inline bool HasPassiveBuff() {
    const auto player = GameObjects::Player();
    return player.IsValid() && (player.HasBuff("SionPassive") ||
        player.HasBuff("SionPassiveBuff") || player.HasBuff("SionDeath"));
}
inline bool RuntimeQCharging() {
    return Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsCharging();
}
inline bool RuntimeWActive() {
    const auto player = GameObjects::Player();
    return player.IsValid() && (player.HasBuff("SionW") ||
        player.HasBuff("SionSoulFurnace") || player.HasBuff("SionWShield"));
}
inline bool RuntimeRActive() {
    const auto player = GameObjects::Player();
    return player.IsValid() && (player.HasBuff("SionR") ||
        player.HasBuff("SionRIndicator") || player.HasBuff("SionRMovement"));
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;

    const bool passive = HasPassiveBuff();
    if (passive && !PassiveZombie) PassiveExpireTick = now +
        static_cast<int>(kPassiveLifetimeSeconds * 1000.0f);
    PassiveZombie = PassiveZombieActive(passive, player.IsDead(),
                                         PassiveExpireTick, now);
    if (!PassiveZombie && PassiveExpireTick > 0 && now > PassiveExpireTick)
        PassiveExpireTick = 0;

    const bool charging = RuntimeQCharging();
    if (charging && !QCharging) {
        QCharging = true;
        QChargeStartTick = now;
    } else if (!charging && QCharging && now - QChargeStartTick > 180) {
        QCharging = false;
        QOwned = false;
        QChargeStartTick = 0;
        QTargetId = 0;
    }
    if (WActive && !RuntimeWActive() && now - WStartTick > 260)
        WActive = false;
    if (RuntimeWActive()) WActive = true;
    if (!RuntimeRActive() && RActive && now - RStartTick > 900)
        RActive = false;
    if (RuntimeRActive()) RActive = true;
    if (IncomingThreatUntil <= now) IncomingThreatUntil = 0;
    if (IncomingHardCcUntil <= now) IncomingHardCcUntil = 0;
    if (InterruptExpireTick <= now) InterruptTargetId = 0;
}

inline float ChargeSeconds() {
    return QChargeStartTick > 0
        ? std::max(0.0f, static_cast<float>(Now() - QChargeStartTick) / 1000.0f)
        : 0.0f;
}

inline bool StartQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PassiveZombie || QCharging || !Ready(0, mode) ||
        !Throttle(0) || Protected(target) || PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition().IsValid()
        ? prediction.GetCastPosition() : PredictPosition(target, kQDelay);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) >
        kQRange + target.BoundingRadius() || SDK::NavMesh::IsWallBetween(
            player.Position(), aim, kQHalfWidthMin)) return false;
    if (Engine::UnderEnemyTurret(aim) &&
        !Engine::UnderEnemyTurret(player.Position())) return false;
    if (!Engine::ControllerCastPosition(0, ClampQEndpoint(player.Position(), aim)))
        return false;
    QCharging = true;
    QOwned = true;
    QChargeStartTick = Now();
    QTargetId = static_cast<int>(target.NetworkId());
    QLastAim = aim;
    LastCastTick[0] = Now();
    return true;
}

inline bool ReleaseQ(const AIHeroClient& fallback, Mode mode, bool reactive = false,
                     bool interrupt = false) {
    if (!QCharging || !QOwned || PassiveZombie || !Engine::RuntimeSpells[0] ||
        !RuntimeQCharging() || !SpellEnabled(0, mode) || Protected(fallback)) return false;
    const auto target = QTargetId != 0 ? HeroByNetworkId(QTargetId) : fallback;
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition().IsValid()
        ? prediction.GetCastPosition() : PredictPosition(target, kQDelay);
    const float charge = ChargeSeconds();
    const bool wall = !aim.IsValid() || aim.IsZero() ||
        SDK::NavMesh::IsWallBetween(player.Position(), aim, QHalfWidth(charge));
    const bool turret = Engine::UnderEnemyTurret(aim) &&
        !Engine::UnderEnemyTurret(player.Position());
    const QReleaseContext context{
        true, true, aim.IsValid() && !aim.IsZero(),
        prediction.Hitchance >= SDK::HitChance::High, wall, turret,
        Engine::ValidEnemy(target), Lethal(target, 0), interrupt ||
            IncomingHardCcUntil > Now(), reactive, charge};
    if (charge < static_cast<float>(Slider(QMenu, "MinimumChargeMs", 260)) / 1000.0f &&
        !context.Lethal && !context.Interrupt) return false;
    if (!ShouldReleaseQ(context)) return false;
    Engine::ArmControllerCast(0);
    if (!Engine::RuntimeSpells[0]->ShootChargedSpell(aim)) {
        Engine::CancelControllerCast(0);
        return false;
    }
    QLastAim = aim;
    LastCastTick[0] = Now();
    QCharging = false;
    QOwned = false;
    QChargeStartTick = 0;
    QTargetId = 0;
    return true;
}

inline bool StartW(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PassiveZombie || WActive || !Ready(1, mode) ||
        !Throttle(1) || PreserveAttack(reactive)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WActive = true;
    WOwned = true;
    WStartTick = Now();
    LastCastTick[1] = Now();
    return true;
}
inline bool DetonateW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PassiveZombie || !WActive || !WOwned ||
        !Engine::ValidEnemy(target) || !WExplosionHits(player.Position(),
            PredictPosition(target, 0.20f), target.BoundingRadius()) ||
        !SpellEnabled(1, mode) || !Throttle(1) || Protected(target)) return false;
    const WDetonationContext context{
        true, true, true, true, Lethal(target, 1),
        reactive || IncomingThreatUntil > Now(), false};
    if (!ShouldDetonateW(context)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WActive = false;
    WOwned = false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PassiveZombie || !Engine::ValidEnemy(target, kERange + 80.0f) ||
        !Ready(2, mode) || !Throttle(2) || Protected(target) ||
        PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition().IsValid()
        ? prediction.GetCastPosition() : PredictPosition(target, 0.25f);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) >
        kERange + target.BoundingRadius() || ControllerHelpers::ProjectileWallBlocksFromPlayer(
            aim, kEHalfWidth) || !EProjectileHits(player.Position(), aim,
            PredictPosition(target, 0.25f), target.BoundingRadius())) return false;
    if (Engine::UnderEnemyTurret(aim) && !Engine::UnderEnemyTurret(player.Position()) &&
        !reactive) return false;
    if (!Engine::ControllerCastPosition(2, ClampEEndpoint(player.Position(), aim)))
        return false;
    ETargetId = static_cast<int>(target.NetworkId());
    ECastTick = Now();
    EFirstBodyObserved = false;
    LastCastTick[2] = Now();
    return true;
}

inline int PredictedRHits(const Vector3& origin, const Vector3& endpoint,
                          const AIHeroClient& primary) {
    int hits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const Vector3 predicted = PredictPosition(enemy, 0.35f);
        if (RPathHits(origin, endpoint, predicted, enemy.BoundingRadius())) ++hits;
    }
    if (Engine::ValidEnemy(primary) && hits == 0 &&
        RPathHits(origin, endpoint, PredictPosition(primary, 0.35f),
                  primary.BoundingRadius())) ++hits;
    return hits;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool interrupt = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PassiveZombie || RActive || !Engine::ValidEnemy(target, kRMaximumRange + 100.0f) ||
        !Ready(3, mode) || !Throttle(3, 160) || Protected(target) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = SteerREndpoint(player.Position(), PredictPosition(target, 0.35f));
    if (!SafeDestination(aim, SDK::NavMesh::IsWall(aim),
        Engine::UnderEnemyTurret(aim), false)) return false;
    const int hits = PredictedRHits(player.Position(), aim, target);
    const RCollisionContext context{
        true, true, hits > 0, SDK::NavMesh::IsWallBetween(player.Position(), aim,
            kRHalfWidth), Engine::UnderEnemyTurret(aim) &&
            !Engine::UnderEnemyTurret(player.Position()), Engine::ValidEnemy(target),
        Lethal(target, 3), interrupt || InterruptTargetId != 0, reactive,
        hits, Slider(RMenu, "MinimumTargets", 1)};
    if (!ShouldCommitR(context)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    RActive = true;
    ROwned = true;
    RStartTick = Now();
    RTargetId = static_cast<int>(target.NetworkId());
    RLastSteer = aim;
    LastCastTick[3] = Now();
    return true;
}

inline void RunZombie(const AIHeroClient& target) {
    if (!PassiveZombie || !Engine::ValidEnemy(target)) return;
    if (Bool(PassiveMenu, "PreserveZombieTarget", true) &&
        ControllerHelpers::InAutoAttackRange(target, 30.0f)) {
        // The shared orbwalker retains attack/movement ownership; this controller
        // only prevents normal spell logic from replacing the zombie's attack.
        return;
    }
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (QCharging) {
        if (ReleaseQ(target, Mode::Combo)) return;
        return;
    }
    if (IncomingThreatUntil > Now() && StartW(Mode::Combo, true)) return;
    if (CastE(target, Mode::Combo)) return;
    if (StartQ(target, Mode::Combo)) return;
    if (WActive && DetonateW(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 42)) return;
    if (QCharging) {
        (void)ReleaseQ(target, Mode::Harass);
        return;
    }
    if (CastE(target, Mode::Harass)) return;
    (void)StartQ(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target) && CastR(target, Mode::Flee, true)) return;
    (void)StartW(Mode::Flee, true);
}
inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 35)) return;
    if (QCharging) {
        const auto target = ControllerHelpers::PreferredEnemyTarget({}, kQRange);
        (void)ReleaseQ(target, mode);
        return;
    }
    (void)Engine::TryFarm(mode);
}
inline bool TryInterrupt(const AIHeroClient& fallback, Mode mode) {
    if (InterruptExpireTick <= Now() || InterruptTargetId == 0 ||
        !Bool(RMenu, "Interrupt", true)) return false;
    const auto interruptTarget = HeroByNetworkId(InterruptTargetId);
    const auto target = Engine::ValidEnemy(interruptTarget, kRMaximumRange)
        ? interruptTarget : fallback;
    return Engine::ValidEnemy(target) && CastR(target, mode, true, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const auto target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f :
        std::max(kRMaximumRange, kERange));
    if (PassiveZombie) {
        RunZombie(target);
        return true;
    }
    if (PlayerOverrideUntil > Now()) return true;
    if (QCharging) {
        if (QOwned) (void)ReleaseQ(target, mode, false,
            IncomingHardCcUntil > Now());
        return true;
    }
    if (RActive) {
        if (Engine::ValidEnemy(target)) RLastSteer = SteerREndpoint(
            GameObjects::Player().Position(), PredictPosition(target, 0.15f));
        return true;
    }
    if (TryInterrupt(target, mode)) return true;
    if (IncomingThreatUntil > Now() && StartW(mode, true)) return true;
    if (Engine::ValidEnemy(target) && Lethal(target, 2) &&
        (WActive ? DetonateW(target, mode, true) : StartW(mode, true))) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Automatic:
        if (Bool(RMenu, "Automatic", true) && Engine::ValidEnemy(target) &&
            (Lethal(target, 3) || IncomingHardCcUntil > Now()))
            (void)CastR(target, mode, true, IncomingHardCcUntil > Now());
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
        if (slot < 0 || slot > 3) return;
        const bool ours = Engine::WasControllerCast(slot);
        if (!ours) PlayerOverrideUntil = now +
            Slider(TacticsMenu, "ManualOwnershipMs", 560);
        LastCastTick[slot] = now;
        if (slot == 0) {
            QCharging = true;
            QOwned = ours;
            QChargeStartTick = now;
            QTargetId = 0;
        } else if (slot == 1) {
            if (WActive && WOwned && ours) {
                WActive = false;
                WOwned = false;
            } else {
                WActive = true;
                WOwned = ours;
                WStartTick = now;
            }
        } else if (slot == 2) {
            ECastTick = now;
            EFirstBodyObserved = args.TargetNetworkId != 0 ||
                args.Target.IsValid();
        } else if (slot == 3) {
            RActive = true;
            ROwned = ours;
            RStartTick = now;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCcUntil = std::max(
        IncomingHardCcUntil, std::max(analysis.CommitmentUntilTick,
                                       analysis.LineThreatUntilTick));
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    (void)CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "SionPassive") ||
            Engine::TextContains(args.BuffName, "SionDeath")) {
            PassiveZombie = true;
            PassiveExpireTick = Now() + static_cast<int>(kPassiveLifetimeSeconds * 1000.0f);
        }
        if (Engine::TextContains(args.BuffName, "SionQ")) QCharging = true;
        if (Engine::TextContains(args.BuffName, "SionW")) WActive = true;
        if (Engine::TextContains(args.BuffName, "SionR")) RActive = true;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "SionPassive") ||
        Engine::TextContains(args.BuffName, "SionDeath")) PassiveZombie = false;
    if (Engine::TextContains(args.BuffName, "SionQ")) QCharging = false;
    if (Engine::TextContains(args.BuffName, "SionW")) WActive = false;
    if (Engine::TextContains(args.BuffName, "SionR")) RActive = false;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    if (QCharging && QOwned && Bool(QMenu, "PreserveCharge", true)) args.Process = false;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFDD7733u, 1.4f, 40);
    Drawing::DrawCircle(player.Position(), kWRadius, 0xFFAA5544u, 1.2f, 40);
    if (RActive && !RLastSteer.IsZero()) Drawing::DrawCircle(RLastSteer,
        kRRadius, 0xFFEEAA44u, 1.5f, 36);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("SionOneTrick", "Sion juggernaut tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    PassiveMenu = TacticsMenu->AddSubMenu(new Menu("Passive", "Glory in Death"));
    PassiveMenu->Add(new MenuBool("PreserveZombieTarget", "Preserve zombie attack target", true));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Decimating Smash charge"));
    QMenu->Add(new MenuSlider("MinimumChargeMs", "Minimum owned charge (ms)", 260, 100, 1900));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 42, 10, 90));
    QMenu->Add(new MenuBool("PreserveCharge", "Protect charged Q from attacks", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Soul Furnace"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Roar projectile"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Unstoppable Onslaught"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum predicted R targets", 1, 1, 4));
    RMenu->Add(new MenuBool("Interrupt", "Use R on interruptible channels", true));
    RMenu->Add(new MenuBool("Automatic", "Allow automatic R defense/interrupt", true));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("SionFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum farm mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("SionCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W ranges", false));
}
inline void OnLoad() {
    LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = PlayerOverrideUntil = 0;
    IncomingThreatUntil = IncomingHardCcUntil = InterruptTargetId = InterruptExpireTick = 0;
    LastMode = Mode::None;
    QCharging = QOwned = WActive = WOwned = EFirstBodyObserved = false;
    RActive = ROwned = PassiveZombie = false;
    QChargeStartTick = QTargetId = WStartTick = ETargetId = ECastTick = 0;
    RStartTick = RTargetId = PassiveExpireTick = 0;
    QLastAim = RLastSteer = {};
}
inline void OnUnload() {
    TacticsMenu = PassiveMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Pin Q charge/release timing, range and damage to Riot 26.15 / CommunityDragon 16.15",
    "Start and release only controller-owned Q; never release a manually held channel",
    "Predict Q impact and reject walls, enemy turret-only endpoints and protected targets",
    "Preserve an ordinary attack windup unless Q is reactive, interrupting or lethal",
    "Reconcile Q charging from spell state plus local cast and buff events",
    "Start W as a shield under incoming pressure and detonate only in its live radius",
    "Keep W detonation controller-owned and avoid burning a manual shield",
    "Predict W explosion targets and reject spell-shielded or invulnerable enemies",
    "Use E prediction with first-body collision and minion projectile semantics",
    "Reject E through projectile walls, unsafe turret endpoints and invalid geometry",
    "Allow E for combo, harass, lane clear, jungle, last hit, flee and peel setup",
    "Steer R toward predicted targets while tracking movement and collision state",
    "Reject R destinations in walls, turret-only pockets or outnumbered nonlethal landings",
    "Use R for verified lethal, interruptible channel, engage and defensive peel paths",
    "Reconcile R movement/impact from buffs and process-spell callbacks; stop on expiry",
    "Capture interrupt windows and prefer R collision over blind long-range casting",
    "Enter Glory in Death from passive buff/death polling and suppress normal spell casts",
    "Preserve zombie attack intent without inventing movement or spell APIs",
    "Preserve selected target before orbwalker and selector fallback in every mode",
    "LaneClear Jungle and LastHit delegate to shared farm policy without R automation",
    "Flee prefers reactive E peel then a safe R route and shield fallback",
    "Automatic mode permits only defensive, interrupting or lethal R/W decisions",
    "Yield after observed manual Q W E or R ownership through the shared engine window",
    "Keep profile metadata, pure geometry and runtime decision loop independently auditable",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Sion;
    controller.ControllerId = "champion.kuroaio.ai.sion.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISion.md";
    controller.ImplementationSummary =
        "Owned Q channel, W shield/detonation, collision-aware E projectile,"
        " steering-safe R and passive Glory in Death reconciliation.";
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
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &OnDoCast;
    controller.OnInterruptable = &CaptureInterruptableEvent<
        &InterruptTargetId, &InterruptExpireTick, 1100, 220, 5000>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Sion
