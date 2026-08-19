#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIPantheonGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Pantheon {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NameEquals;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEventNameContainsAny;
using MarksmanControllerHelpers::CanUse;
using MarksmanControllerHelpers::CastThrottlePassed;
using MarksmanControllerHelpers::ClearTemporaryOrbwalkerFocus;
using MarksmanControllerHelpers::ManualUltimatePressed;
using MarksmanControllerHelpers::OwnedOrbwalkerFocus;
using MarksmanControllerHelpers::PredictionProjectileWall;
using MarksmanControllerHelpers::RedirectBeforeAttackToFocus;
using MarksmanControllerHelpers::SetTemporaryOrbwalkerFocus;
using MarksmanControllerHelpers::SpellDamage;

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int PassiveStacks = 0;
inline bool PassiveReadyConfirmed = false;
inline int PassiveObservedTick = 0;
inline int QCastTick = 0;
inline int QChargeStartTick = 0;
inline int QTargetId = 0;
inline bool QControllerOwned = false;
inline bool QPreserveFullDamage = false;
inline int WCastTick = 0;
inline int WTargetId = 0;
inline bool EmpoweredWFollowupPending = false;
inline int EmpoweredWFollowupUntil = 0;
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline int ECastTick = 0;
inline bool EActive = false;
inline bool EWasEmpowered = false;
inline int EStartTick = 0;
inline int EExpireTick = 0;
inline int RCastTick = 0;
inline bool RChannelActive = false;
inline int RChannelUntil = 0;
inline Vector3 LastRLanding = {};
inline int PlayerOverrideUntil = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int IncomingThreatId = 0;
inline int IncomingThreatUntil = 0;
inline Vector3 IncomingThreatSource = {};
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Ready;

inline bool PreservingAttack(bool reactive = false) {
    return !reactive && Orbwalker::IsWindingUp() &&
           Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool PlayerLow() {
    const auto player = GameObjects::Player();
    return player.IsValid() && player.HealthPercent() <=
        static_cast<float>(Slider(EMenu, "EmergencyHp", 30));
}

inline bool PassiveEmpowered() {
    return PassiveReadyConfirmed || PassiveStacks >= 5;
}

inline int CounterBuffStacks(const AIHeroClient& player) {
    if (!player.IsValid()) return 0;
    return std::clamp(std::max(
        player.GetBuffCount("PantheonPassiveCounter"),
        player.GetBuffCount("pantheonpassivecounter")), 0, 5);
}

inline bool QRuntimeCharging() {
    return Engine::RuntimeSpells[0] &&
           Engine::RuntimeSpells[0]->IsCharging();
}

inline bool QCharging() {
    if (QRuntimeCharging()) return true;
    return QChargeStartTick > 0 && Now() - QChargeStartTick <= 1050;
}

inline float QChargeSeconds() {
    return QChargeStartTick > 0
        ? std::max(0, Now() - QChargeStartTick) / 1000.0f
        : 0.0f;
}

inline bool EBuffActive(const AIHeroClient& player) {
    return player.IsValid() &&
        (player.HasBuff("PantheonE") || player.HasBuff("pantheone"));
}

inline bool RBuffActive(const AIHeroClient& player) {
    return player.IsValid() &&
        (player.HasBuff("PantheonRJump") ||
         player.HasBuff("PantheonRChannel") ||
         player.HasBuff("PantheonRFall"));
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    const bool readyBuff = player.HasBuff("PantheonPassiveReady") ||
                           player.HasBuff("pantheonpassiveready");
    const int observedStacks = CounterBuffStacks(player);
    if (readyBuff) {
        PassiveStacks = 5;
        PassiveReadyConfirmed = true;
        PassiveObservedTick = now;
    } else if (observedStacks > 0 || now - PassiveObservedTick > 220) {
        PassiveStacks = observedStacks;
        PassiveReadyConfirmed = false;
        PassiveObservedTick = now;
    }
    if (QRuntimeCharging()) {
        if (QChargeStartTick <= 0) {
            QChargeStartTick = now;
            QControllerOwned = false;
        }
    } else if (QChargeStartTick > 0 && now - QChargeStartTick > 120) {
        QChargeStartTick = 0;
        QTargetId = 0;
        QControllerOwned = false;
        QPreserveFullDamage = false;
    }
    const bool eBuff = EBuffActive(player);
    if (eBuff) {
        EActive = true;
        if (EStartTick <= 0) EStartTick = now;
        if (EExpireTick <= now) EExpireTick = now + 1650;
    } else if (EActive && now - EStartTick > 140) {
        EActive = false;
        EWasEmpowered = false;
        EStartTick = EExpireTick = 0;
    }
    if (RBuffActive(player)) {
        RChannelActive = true;
        if (RChannelUntil <= now) RChannelUntil = now + 3200;
    } else if (RChannelActive && now > RChannelUntil) {
        RChannelActive = false;
        RChannelUntil = 0;
    }
    if (EmpoweredWFollowupPending && now > EmpoweredWFollowupUntil) {
        EmpoweredWFollowupPending = false;
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    }
    if (IncomingThreatUntil < now) {
        IncomingThreatId = 0;
        IncomingThreatSource = {};
    }
    if (GapcloserExpireTick < now) GapcloserTargetId = 0;
    if (InterruptExpireTick < now) InterruptTargetId = 0;
}

inline AIHeroClient ResolveTarget(const AIHeroClient& preferred, float range) {
    if (Engine::ValidEnemy(preferred, range)) return preferred;
    const auto orbwalker = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orbwalker, range)) return orbwalker;
    const auto selected = ControllerHelpers::PlayerSelectedEnemy(range);
    if (Engine::ValidEnemy(selected, range)) return selected;
    return Engine::SelectTarget(range);
}

inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) ||
           IsCommonUntargetableOrImmune(target);
}

inline bool QPrediction(const AIHeroClient& target, float range,
                        SDK::PredictionOutput* output = nullptr) {
    if (!Engine::RuntimeSpells[0] ||
        !Engine::ValidEnemy(target, range + 100.0f)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(
        target, false, range);
    if (output) *output = prediction;
    return ControllerHelpers::PredictionAtLeast(
        prediction, SDK::HitChance::High);
}

inline bool QFirstBodyClear(const SDK::PredictionOutput& prediction) {
    return prediction.CollisionObjects.empty();
}

inline float TargetEffectiveHealth(const AIHeroClient& target) {
    return std::max(1.0f, target.Health() + target.AllShield());
}

inline bool QLethal(const AIHeroClient& target) {
    return SpellDamage(0, target) + 2.0f >= TargetEffectiveHealth(target);
}

inline bool TotalComboLethal(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    const float autoDamage = SDK::Damage::GetAutoAttackDamage(
        player, target, true);
    return SpellDamage(0, target) + SpellDamage(1, target) +
           SpellDamage(2, target) + autoDamage * 2.0f + 4.0f >=
           TargetEffectiveHealth(target);
}

inline bool SafeVaultTarget(const AIHeroClient& target,
                            bool fleeing, bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() ||
        !Engine::ValidEnemy(target, kWRange + 45.0f)) return false;
    const Vector3 endpoint = PredictPosition(target, 0.18f);
    VaultContext context{};
    context.Ready = Ready(1);
    context.TargetValid = !TargetBlocked(target);
    context.InRange = player.Position().Distance2D(endpoint) <=
        kWRange + target.BoundingRadius();
    context.EndpointWalkable = endpoint.IsValid() && !endpoint.IsZero() &&
        !SDK::NavMesh::IsWall(endpoint);
    context.TargetSpellShield = HasSpellShieldOrImmunity(target);
    context.EnemyTurret = Engine::UnderEnemyTurret(endpoint);
    context.Lethal = lethal;
    context.NearbyEnemies = Engine::CountEnemiesAt(endpoint, 650.0f);
    context.MaximumEnemies = Slider(WMenu, "MaximumEnemies", 2);
    context.PlayerLow = PlayerLow() && !fleeing;
    return VaultSafe(context);
}

inline bool CastW(const AIHeroClient& target, Mode mode,
                  bool reactive = false, bool fleeing = false) {
    if (!CanUse(1, mode, reactive) ||
        !CastThrottlePassed(WCastTick, 45) || TargetBlocked(target)) return false;
    const bool lethal = TotalComboLethal(target);
    if (!SafeVaultTarget(target, fleeing, lethal) ||
        (PreservingAttack(reactive) && !lethal)) return false;
    const bool empowered = PassiveEmpowered();
    if (!Engine::ControllerCastUnit(1, target)) return false;
    WCastTick = Now();
    WTargetId = static_cast<int>(target.NetworkId());
    if (empowered) {
        PassiveStacks = 0;
        PassiveReadyConfirmed = false;
        EmpoweredWFollowupPending = true;
        EmpoweredWFollowupUntil = Now() + 1150;
        (void)SetTemporaryOrbwalkerFocus(
            target, 775.0f, 1150, OwnedFocusTargetId, OwnedFocusUntil);
    } else {
        PassiveStacks = std::min(5, PassiveStacks + 1);
    }
    return true;
}

inline bool CastTapQ(const AIHeroClient& target, Mode mode,
                     bool reactive = false) {
    if (QCharging() || !CanUse(0, mode, reactive) ||
        !CastThrottlePassed(QCastTick, 32) || TargetBlocked(target)) return false;
    const auto player = GameObjects::Player();
    SDK::PredictionOutput prediction{};
    const bool predictionHigh = QPrediction(target, kTapQRange, &prediction);
    const bool lethal = QLethal(target);
    QTapContext context{};
    context.Ready = true;
    context.PredictionHigh = predictionHigh;
    context.InTapRange = player.IsValid() &&
        player.Position().Distance2D(prediction.GetCastPosition()) <=
            kTapQRange + target.BoundingRadius();
    context.AttackWindingUp = PreservingAttack(reactive);
    context.Lethal = lethal;
    context.ManualOwnership = PlayerOverrideUntil > Now();
    if (!ShouldTapQ(context)) return false;
    const bool empowered = PassiveEmpowered();
    if (!Engine::ControllerCastPosition(0, prediction.GetCastPosition())) return false;
    QCastTick = Now();
    PassiveStacks = empowered ? 0 : std::min(5, PassiveStacks + 1);
    PassiveReadyConfirmed = false;
    return true;
}

inline bool StartThrowQ(const AIHeroClient& target, Mode mode,
                        bool reactive = false) {
    if (QCharging() || !CanUse(0, mode, reactive) ||
        !CastThrottlePassed(QCastTick, 32) || TargetBlocked(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    SDK::PredictionOutput prediction{};
    const bool predictionHigh = QPrediction(target, kThrowQRange, &prediction);
    const float distance = player.Position().Distance2D(
        prediction.GetCastPosition());
    const bool execute = target.HealthPercent() <= 20.0f || QLethal(target);
    QChargeStartContext context{};
    context.Ready = true;
    context.PredictionHigh = predictionHigh;
    context.InThrowRange = distance <= kThrowQRange + target.BoundingRadius();
    context.OutsideTapRange = distance > kTapQRange + target.BoundingRadius();
    context.Execute = execute;
    context.FirstBodyClear = QFirstBodyClear(prediction);
    context.AttackWindingUp = PreservingAttack(reactive);
    context.ManualOwnership = PlayerOverrideUntil > Now();
    if (!ShouldStartQCharge(context) ||
        PredictionProjectileWall(0, prediction, kQRadius)) return false;
    Engine::ArmControllerCast(0);
    if (!Engine::RuntimeSpells[0]->StartCharging(prediction.GetCastPosition())) {
        Engine::CancelControllerCast(0);
        return false;
    }
    Engine::MarkSuccessfulCast(0);
    QCastTick = Now();
    QChargeStartTick = Now();
    QTargetId = static_cast<int>(target.NetworkId());
    QControllerOwned = true;
    QPreserveFullDamage = PassiveEmpowered() || execute;
    return true;
}

inline bool ReleaseThrowQ(const AIHeroClient& fallback) {
    if (!QCharging() || !QControllerOwned || !Engine::RuntimeSpells[0]) return false;
    auto target = HeroByNetworkId(QTargetId);
    if (!Engine::ValidEnemy(target, kThrowQRange + 100.0f)) target = fallback;
    if (!Engine::ValidEnemy(target, kThrowQRange + 100.0f)) return false;
    SDK::PredictionOutput prediction{};
    const bool predictionHigh = QPrediction(target, kThrowQRange, &prediction);
    const auto player = GameObjects::Player();
    QChargeReleaseContext context{};
    context.Charging = true;
    context.ElapsedSeconds = QChargeSeconds();
    context.PredictionHigh = predictionHigh;
    context.InThrowRange = player.IsValid() &&
        player.Position().Distance2D(prediction.GetCastPosition()) <=
            kThrowQRange + target.BoundingRadius();
    context.ProjectileWall = PredictionProjectileWall(0, prediction, kQRadius);
    context.FirstBodyClear = QFirstBodyClear(prediction);
    context.PreserveFullDamage = QPreserveFullDamage;
    if (!ShouldReleaseQCharge(context)) return false;
    Engine::ArmControllerCast(0);
    if (!Engine::RuntimeSpells[0]->ShootChargedSpell(
            prediction.GetCastPosition())) {
        Engine::CancelControllerCast(0);
        return false;
    }
    Engine::MarkSuccessfulCast(0);
    QCastTick = Now();
    const bool empowered = PassiveEmpowered();
    PassiveStacks = empowered ? 0 : std::min(5, PassiveStacks + 1);
    PassiveReadyConfirmed = false;
    QChargeStartTick = QTargetId = 0;
    QControllerOwned = QPreserveFullDamage = false;
    return true;
}

inline AIHeroClient CurrentThreat(const AIHeroClient& fallback = {}) {
    if (IncomingThreatUntil >= Now()) {
        const auto threat = HeroByNetworkId(IncomingThreatId);
        if (Engine::ValidEnemy(threat, 1200.0f)) return threat;
    }
    if (GapcloserExpireTick >= Now()) {
        const auto threat = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(threat, 1200.0f)) return threat;
    }
    return NearestEnemyToPlayer(fallback, 1200.0f);
}

inline bool CastE(const AIHeroClient& threat, Mode mode,
                  bool reactive, bool fleeing = false) {
    if (EActive || QCharging() || !CanUse(2, mode, true) ||
        !CastThrottlePassed(ECastTick, 45)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    Vector3 source = IncomingThreatUntil >= Now() &&
            IncomingThreatSource.IsValid() && !IncomingThreatSource.IsZero()
        ? IncomingThreatSource : threat.Position();
    if (!source.IsValid() || source.IsZero()) return false;
    const Vector3 facing = Engine::ValidEnemy(threat)
        ? PredictPosition(threat, 0.12f) : source;
    EContext context{};
    context.Ready = true;
    context.ThreatCommitted = reactive || IncomingThreatUntil >= Now() ||
                              GapcloserExpireTick >= Now();
    context.SourceInFront = DirectionalShieldBlocks(
        player.Position(), facing, source);
    context.PlayerLow = PlayerLow();
    context.Fleeing = fleeing;
    context.AttackWindingUp = PreservingAttack(reactive);
    context.ManualOwnership = PlayerOverrideUntil > Now();
    if (!ShouldCastE(context)) return false;
    const bool empowered = PassiveEmpowered();
    if (!Engine::ControllerCastPosition(2, facing)) return false;
    ECastTick = EStartTick = Now();
    EExpireTick = Now() + 1650;
    EActive = true;
    EWasEmpowered = empowered;
    PassiveStacks = empowered ? 0 : std::min(5, PassiveStacks + 1);
    PassiveReadyConfirmed = false;
    return true;
}

inline bool TryDefensiveE(const AIHeroClient& fallback,
                          Mode mode = Mode::Automatic) {
    const bool committed = IncomingThreatUntil >= Now() ||
                           GapcloserExpireTick >= Now();
    if (!committed && !PlayerLow()) return false;
    const auto threat = CurrentThreat(fallback);
    return Engine::ValidEnemy(threat, 1200.0f) &&
           CastE(threat, mode, committed, mode == Mode::Flee);
}

inline bool TryManualR(const AIHeroClient& preferred) {
    if (!ManualUltimatePressed() || RChannelActive || QCharging() ||
        !Ready(3) || !CastThrottlePassed(RCastTick, 120)) return false;
    const auto player = GameObjects::Player();
    const auto target = ResolveTarget(preferred, kRRange);
    if (!player.IsValid() || TargetBlocked(target)) return false;
    const Vector3 destination = PredictPosition(target, 2.20f);
    if (!destination.IsValid() || destination.IsZero()) return false;
    const float distance = player.Position().Distance2D(destination);
    const int enemies = Engine::CountEnemiesAt(destination, 700.0f);
    const int allies = CountAlliedFollowup(destination, 900.0f, false);
    const bool lethal = SpellDamage(3, target) + 2.0f >=
        TargetEffectiveHealth(target);
    const bool escapeRoute =
        !ControllerHelpers::HasReadyDashHazardAt(destination, 700.0f) &&
        !ControllerHelpers::HasReadyPointClickThreatAt(destination);
    RLandingContext context{};
    context.ManualRequested = true;
    context.DestinationValid = true;
    context.DestinationWalkable = !SDK::NavMesh::IsWall(destination);
    context.InRange = distance <= kRRange;
    context.BeyondLocalCombat = distance > kThrowQRange;
    context.TargetPredictedInside = true;
    context.EnemyTurret = Engine::UnderEnemyTurret(destination);
    context.EscapeRoute = escapeRoute;
    context.Lethal = lethal;
    context.NearbyEnemies = enemies;
    context.MaximumEnemies = Slider(RMenu, "MaximumEnemies", 2);
    context.AlliedFollowup = allies;
    if (Bool(RMenu, "RequireAlly", true) && allies <= 0 && !lethal)
        context.EscapeRoute = false;
    if (!RLandingSafe(context)) return false;
    if (!Engine::ControllerCastPosition(3, destination)) return false;
    RCastTick = Now();
    RChannelActive = true;
    RChannelUntil = Now() + 4200;
    LastRLanding = destination;
    return true;
}

inline bool TryInterrupt() {
    if (!Bool(Engine::AutomaticMenu, "Interrupt", true) ||
        InterruptExpireTick < Now()) return false;
    const auto target = HeroByNetworkId(InterruptTargetId);
    if (!Engine::ValidEnemy(target, kWRange + 45.0f)) return false;
    return AutomaticAllowed({ false, true, false, false }) &&
           CastW(target, Mode::Automatic, true, false);
}

inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(target, 1200.0f) ||
        !AutomaticAllowed({ true, false, false, false })) return false;
    if (CastE(target, Mode::Automatic, true, false)) return true;
    return Engine::ValidEnemy(target, kWRange + 45.0f) &&
           CastW(target, Mode::Automatic, true, false);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = ResolveTarget(preferred, kThrowQRange);
    if (TargetBlocked(target) || !QLethal(target) ||
        !AutomaticAllowed({ false, false, true, false })) return false;
    const auto player = GameObjects::Player();
    const QCastStyle style = QStyleForDistance(
        player.Position().Distance2D(target.Position()), target.BoundingRadius());
    if (style == QCastStyle::Tap)
        return CastTapQ(target, Mode::Automatic, true);
    if (style == QCastStyle::Throw)
        return StartThrowQ(target, Mode::Automatic, true);
    return false;
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (TargetBlocked(target)) return false;
    if (EmpoweredWFollowupPending) return true;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float distance = player.Position().Distance2D(target.Position());
    const bool qReachable = QStyleForDistance(
        distance, target.BoundingRadius()) != QCastStyle::None;
    const bool safeW = mode == Mode::Combo &&
        distance <= kWRange + target.BoundingRadius() &&
        SafeVaultTarget(target, false, TotalComboLethal(target));
    PassiveContext passive{};
    passive.Empowered = PassiveEmpowered();
    passive.DefensiveThreat = IncomingThreatUntil >= Now() || PlayerLow();
    passive.EReady = Ready(2);
    passive.SafeWEngage = safeW;
    passive.WReady = Ready(1);
    passive.QReady = Ready(0);
    passive.QReachable = qReachable;
    passive.QExecute = target.HealthPercent() <= 20.0f || QLethal(target);
    const EmpoweredSpell choice = ChooseEmpoweredSpell(passive);
    if (choice == EmpoweredSpell::E &&
        CastE(CurrentThreat(target), mode, true, false)) return true;
    if (choice == EmpoweredSpell::W && CastW(target, mode)) return true;
    if (mode == Mode::Combo && safeW &&
        Bool(WMenu, "UseWCombo", true) && CastW(target, mode)) return true;
    const QCastStyle style = QStyleForDistance(distance, target.BoundingRadius());
    if (style == QCastStyle::Tap && Bool(QMenu, "UseTap", true) &&
        CastTapQ(target, mode)) return true;
    if (style == QCastStyle::Throw && Bool(QMenu, "UseThrow", true) &&
        StartThrowQ(target, mode)) return true;
    return false;
}

inline bool TryFlee(const AIHeroClient& preferred) {
    const auto threat = CurrentThreat(preferred);
    if (!Engine::ValidEnemy(threat, 1200.0f)) return false;
    if (CastE(threat, Mode::Flee, true, true)) return true;
    if (Engine::ValidEnemy(threat, kWRange + 35.0f) && PlayerLow())
        return CastW(threat, Mode::Flee, true, true);
    return false;
}

inline bool TryFarm(Mode mode) {
    if (QCharging() || EActive || EmpoweredWFollowupPending ||
        PreservingAttack(false)) return false;
    const bool lastHit = mode == Mode::LastHit;
    const bool jungle = mode == Mode::Jungle ||
        (!lastHit && !Engine::ClearUnits(true).empty() &&
         Engine::ClearUnits(false).empty());
    const bool preserveEmpowered = Bool(
        PassiveMenu, "PreserveEmpoweredFarm", true) && PassiveEmpowered();
    if (jungle && Bool(WMenu, "UseWJungle", true) &&
        (!preserveEmpowered || Bool(PassiveMenu, "SpendOnJungle", true)) &&
        Engine::TryFarmSpell(1, true, false)) {
        WCastTick = Now(); return true;
    }
    if (Bool(QMenu, "UseFarm", true) && (!preserveEmpowered || lastHit) &&
        Engine::TryFarmSpell(0, jungle, lastHit)) {
        QCastTick = Now(); return true;
    }
    if (!lastHit && Bool(EMenu, "UseFarm", false) && !preserveEmpowered &&
        Engine::TryFarmSpell(2, jungle, false)) {
        ECastTick = Now(); return true;
    }
    return false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    ReconcileState();
    if (RChannelActive) return true;
    if (QCharging()) {
        if (!QControllerOwned) return true;
        auto target = HeroByNetworkId(QTargetId);
        if (!Engine::ValidEnemy(target))
            target = ResolveTarget(preferred, kThrowQRange);
        (void)ReleaseThrowQ(target);
        return true;
    }
    if (PlayerOverrideUntil > Now()) return true;
    if (TryManualR(preferred) || TryInterrupt() || TryAntiGapcloser() ||
        TryDefensiveE(preferred) || TryKillSecure(preferred)) return true;
    if (mode == Mode::Flee) return TryFlee(preferred);
    if (mode == Mode::Combo || mode == Mode::Harass)
        return TryCombat(ResolveTarget(preferred, kThrowQRange), mode);
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return TryFarm(mode);
    return false;
}

inline void ObserveLocalSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(
            args, 220.0f, 115.0f, 260, 260, 220, 1600, 600);
        if (threat.Valid && (threat.TargetsPlayer || threat.CrossesPlayer) &&
            threat.Committed) {
            IncomingThreatId = static_cast<int>(threat.Enemy.NetworkId());
            IncomingThreatUntil = std::max(
                threat.CommitmentUntilTick, threat.LineThreatUntilTick);
            IncomingThreatSource = threat.Enemy.Position();
        }
        return;
    }
    if (args.IsAutoAttack) {
        if (!EmpoweredWFollowupPending)
            PassiveStacks = std::min(5, PassiveStacks + 1);
        return;
    }
    const int slot = args.Slot;
    const bool owned = slot >= 0 && slot < 4 && Engine::WasControllerCast(slot);
    if (!owned) {
        PlayerOverrideUntil = now +
            Slider(TacticsMenu, "ManualOwnershipMs", 520);
        if (slot == 0 && QRuntimeCharging()) {
            QChargeStartTick = now;
            QControllerOwned = false;
        }
    }
    if (slot == 0 || SpellEventNameContainsAny(args, { "pantheonq", "cometspear" })) {
        QCastTick = now;
    } else if (slot == 1 || SpellEventNameContainsAny(args, { "pantheonw", "shieldvault" })) {
        WCastTick = now;
        WTargetId = static_cast<int>(args.TargetNetworkId);
    } else if (slot == 2 || SpellEventNameContainsAny(args, { "pantheone", "aegisassault" })) {
        ECastTick = now;
        if (!SpellEventNameContainsAny(args, { "pantheone2" })) {
            EActive = true; EStartTick = now; EExpireTick = now + 1650;
        }
    } else if (slot == 3 || SpellEventNameContainsAny(args, { "pantheonr", "grandstarfall" })) {
        RCastTick = now; RChannelActive = true; RChannelUntil = now + 4200;
    }
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (NameEquals(args.BuffName, "PantheonPassiveReady")) {
        PassiveReadyConfirmed = added;
        PassiveStacks = added ? 5 : std::min(PassiveStacks, 4);
        PassiveObservedTick = now;
    } else if (NameEquals(args.BuffName, "PantheonPassiveCounter")) {
        PassiveStacks = added ? std::clamp(args.Count, 0, 5) : 0;
        PassiveReadyConfirmed = PassiveStacks >= 5;
        PassiveObservedTick = now;
    } else if (NameEquals(args.BuffName, "PantheonE") ||
               NameEquals(args.BuffName, "PantheonEResists")) {
        EActive = added;
        if (added) {
            EStartTick = now;
            EExpireTick = now + ControllerHelpers::RemainingMilliseconds(
                args.EndTime, 1500, 200, 2800);
        } else {
            EWasEmpowered = false; EStartTick = EExpireTick = 0;
        }
    } else if (NameEquals(args.BuffName, "PantheonRJump") ||
               NameEquals(args.BuffName, "PantheonRChannel") ||
               NameEquals(args.BuffName, "PantheonRFall")) {
        RChannelActive = added;
        RChannelUntil = added
            ? now + ControllerHelpers::RemainingMilliseconds(
                args.EndTime, 3200, 300, 5200) : 0;
        if (!added) {
            PassiveStacks = 5;
            PassiveReadyConfirmed = true;
            PassiveObservedTick = now;
        }
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (QCharging() || EActive || RChannelActive) {
        args.Process = false; return;
    }
    if (!EmpoweredWFollowupPending) return;
    const auto focus = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 775.0f);
    if (!focus.IsValid()) {
        EmpoweredWFollowupPending = false; return;
    }
    (void)RedirectBeforeAttackToFocus(args, focus);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    if (EmpoweredWFollowupPending && LastAfterAttackTargetId == WTargetId) {
        PassiveStacks = std::max(PassiveStacks, 3);
        PassiveReadyConfirmed = false;
        PassiveObservedTick = Now();
        EmpoweredWFollowupPending = false;
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), kTapQRange, 0xFFD6A543u, 1.4f, 40);
    Drawing::DrawCircle(player.Position(), kWRange, 0xFF6E86C7u, 1.2f, 40);
    if (QCharging())
        Drawing::DrawCircle(player.Position(), kThrowQRange, 0xFFF1C75Bu, 2.0f, 48);
    if (LastRLanding.IsValid() && !LastRLanding.IsZero())
        Drawing::DrawCircle(LastRLanding, kRDamageRadius, 0xFFB385E8u, 2.0f, 48);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("PantheonOneTrick", "Pantheon one-trick mechanics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 520, 180, 1100));
    PassiveMenu = TacticsMenu->AddSubMenu(new Menu("MortalWill", "Mortal Will"));
    PassiveMenu->Add(new MenuBool("PreserveEmpoweredFarm", "Preserve empowered passive in lane", true));
    PassiveMenu->Add(new MenuBool("SpendOnJungle", "Allow empowered W on jungle", true));
    PassiveMenu->Add(new MenuSeparator("Priority", "Defensive E > safe W engage > clean Q"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("CometSpear", "Comet Spear"));
    QMenu->Add(new MenuBool("UseTap", "Use tap Q inside 575", true));
    QMenu->Add(new MenuBool("UseThrow", "Use thrown Q outside 575", true));
    QMenu->Add(new MenuBool("UseFarm", "Use tap Q for farm", true));
    QMenu->Add(new MenuSeparator("FirstBody", "Empowered/execute throws preserve first-body damage"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("ShieldVault", "Shield Vault"));
    WMenu->Add(new MenuBool("UseWCombo", "Use W in combo", true));
    WMenu->Add(new MenuBool("UseWJungle", "Use W in jungle", true));
    WMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at vault", 2, 1, 5));
    EMenu = TacticsMenu->AddSubMenu(new Menu("AegisAssault", "Aegis Assault"));
    EMenu->Add(new MenuSlider("EmergencyHp", "Defensive E health (%)", 30, 10, 70));
    EMenu->Add(new MenuBool("UseFarm", "Use E for clear", false));
    EMenu->Add(new MenuSeparator("Direction", "Face the observed damage source; never rotate away"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("GrandStarfall", "Grand Starfall"));
    RMenu->Add(new MenuBool("RequireAlly", "Require ally follow-up unless lethal", true));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at landing", 2, 1, 5));
    RMenu->Add(new MenuSeparator("ManualOnly", "R remains manual-assist with landing validation"));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("PantheonCoach", "Coach overlay"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw tactical ranges", false));
}

inline void OnLoad() {
    PassiveStacks = PassiveObservedTick = 0; PassiveReadyConfirmed = false;
    QCastTick = QChargeStartTick = QTargetId = 0;
    QControllerOwned = QPreserveFullDamage = false;
    WCastTick = WTargetId = EmpoweredWFollowupUntil = 0;
    EmpoweredWFollowupPending = false; OwnedFocusTargetId = OwnedFocusUntil = 0;
    ECastTick = EStartTick = EExpireTick = 0; EActive = EWasEmpowered = false;
    RCastTick = RChannelUntil = 0; RChannelActive = false; LastRLanding = {};
    PlayerOverrideUntil = LastAfterAttackTargetId = LastAfterAttackTick = 0;
    IncomingThreatId = IncomingThreatUntil = 0; IncomingThreatSource = {};
    GapcloserTargetId = GapcloserExpireTick = 0; GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = 0; LastMode = Mode::None;
    ReconcileState();
}
inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = PassiveMenu = QMenu = WMenu = EMenu = RMenu = CoachMenu = nullptr;
    LastRLanding = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin kit behavior to Riot 26.15 and CommunityDragon 16.15",
    "Reconcile Mortal Will counter and ready buffs through events and polling",
    "Treat five attacks or basic spell casts as the empowered threshold",
    "Prioritize empowered E for committed incoming damage",
    "Prefer empowered W only when the vault endpoint is safe",
    "Preserve empowered Q for reachable or execute-value targets",
    "Model empowered W as one attack with three Mortal Will-generating strikes",
    "Force only the owned empowered-W target and release it after the attack",
    "Tap Q inside 575 for the sixty-percent cooldown refund path",
    "Start thrown Q only for a high-confidence target inside 1200",
    "Hold thrown Q through the 0.35-second tap/throw boundary",
    "Reject thrown Q through a live projectile wall",
    "Preserve first-body Q damage for empowered and execute throws",
    "Force release at the 0.80-second charge boundary when the line is valid",
    "Block orbwalker attacks only during owned Q charge, E shield, or R channel",
    "Use W stun for safe combo engage and interrupt reactions",
    "Reject W into walls, spell shields, unsafe crowding, or nonlethal turret entry",
    "Face E toward the observed enemy damage source rather than movement direction",
    "Keep E active for its directional protection instead of early damage recast",
    "Use empowered E as defensive protection and its post-slam speed route",
    "Keep R manual-assist and reject local-range casts",
    "Predict R landing after the channel and require walkable destination geometry",
    "Reject R landing under enemy turret unless lethal",
    "Reject crowded R landing without lethal value",
    "Require allied follow-up, lethal damage, or a verified escape route for R",
    "Preserve selected target, then orbwalker target, before policy fallback",
    "Yield temporarily after every non-controller player spell",
    "Protect manual Q charge, E direction, and R channel ownership",
    "Automatic mode allows only defensive E/W, interrupts, and Q kill secure",
    "Combo uses safe W, protected triple attack, Q, and defensive E exit",
    "Harass uses Q without unsolicited W or R commitment",
    "LaneClear preserves empowered passive and uses conservative tap Q",
    "Jungle supports W, tap Q, and opt-in E without global actions",
    "LastHit uses tap Q only when the empowered-passive policy allows it",
    "Flee faces E into pursuit and uses W only as an emergency stun",
    "Never automate Flash, summoner spells, item actives, or Grand Starfall engage",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Pantheon;
    controller.ControllerId = "champion.kuroaio.ai.pantheon.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIPantheon.md";
    controller.ImplementationSummary =
        "Five-action Mortal Will reconciliation and empowered Q/W/E priority; "
        "tap-versus-held Comet Spear with first-body preservation; safe targeted "
        "W and protected triple attack; source-facing E defense; manual-only "
        "Grand Starfall with predicted landing safety and full mode ownership.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad; controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu; controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw; controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>; controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;

    controller.OnBeforeAttack = &OnBeforeAttack; controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 825, 1100>; controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1400, 250, 5000>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Pantheon
