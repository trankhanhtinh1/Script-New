#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIMissFortuneGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace Plugins::KuroAIO::AI::Controllers::MissFortune {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* LoveTapMenu = nullptr;
inline Menu* BounceMenu = nullptr;
inline Menu* UltimateMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int IncomingThreatUntil = 0;
inline int PendingUltimateTargetId = 0;
inline int PendingUltimateUntil = 0;
inline bool PendingUltimateManual = false;
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline Mode LastMode = Mode::None;
inline bool BulletTimeOwnsOrbwalkerPause = false;
inline bool PreviousAttackEnabled = true;
inline bool PreviousMoveEnabled = true;
inline AIHeroClient LastSmartTarget = {};

inline Vec2 ToVec2(const Vector3& value) {
    return {value.x, value.z};
}

inline bool LoveTapMarked(const AIBaseClient& target) {
    return target.IsValid() && target.HasBuff("MissFortunePassive");
}

inline bool RecentlyAttacked(const AIBaseClient& target,
                             int windowMs = 430) {
    return RecentlyAttackedTarget(
        target, LastAfterAttackTargetId, LastAfterAttackTick, windowMs);
}

inline bool BulletTimeActive() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (player.HasBuff("MissFortuneBulletSound")) return true;
    return LastRCastTick > 0 && Now() - LastRCastTick <= 700 &&
           player.Spellbook().IsChanneling();
}

inline bool ChannelSafe() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
           IncomingThreatUntil < Now() &&
           Engine::CountEnemiesAt(player.Position(), 650.0f) == 0 &&
           !HasReadyPointClickThreatAt(player.Position()) &&
           !HasReadyDashHazardAt(player.Position());
}

inline void AcquireBulletTimePause() {
    if (BulletTimeOwnsOrbwalkerPause) return;
    PreviousAttackEnabled = Orbwalker::AttackEnabled();
    PreviousMoveEnabled = Orbwalker::MoveEnabled();
    Orbwalker::AttackEnabled(false);
    Orbwalker::MoveEnabled(false);
    BulletTimeOwnsOrbwalkerPause = true;
}

inline bool BounceBlocked(const AIBaseClient& primary,
                          const AIHeroClient& desired,
                          const Vector3& desiredPosition) {
    if (!primary.IsValid() || !desired.IsValid()) return true;
    const auto player = GameObjects::Player();
    const Vec2 origin = ToVec2(player.Position());
    const Vec2 pivot = ToVec2(primary.Position());
    const float desiredDistance = primary.Position().Distance2D(desiredPosition);
    if (ControllerHelpers::ProjectileWallBlocks(
            primary.Position(), desiredPosition, 70.0f)) {
        return true;
    }
    const auto blocks = [&](const AIBaseClient& unit) {
        if (!unit.IsValid() || unit.IsDead() ||
            unit.NetworkId() == primary.NetworkId() ||
            unit.NetworkId() == desired.NetworkId()) return false;
        if (SDK::HealthPrediction::GetPrediction(unit, 350, 0) <= 0.0f) {
            return false;
        }
        const float distance = primary.Position().Distance2D(unit.Position());
        return distance + unit.BoundingRadius() < desiredDistance &&
            BounceConeContains(
                origin, pivot, ToVec2(unit.Position()), 500.0f, 30.0f);
    };
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (blocks(AIBaseClient(minion.Handle()))) return true;
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (blocks(AIBaseClient(monster.Handle()))) return true;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (blocks(AIBaseClient(enemy.Handle()))) return true;
    }
    return false;
}

inline float DoubleUpTargetRange(const AIBaseClient& target = {}) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 550.0f;
    return target.IsValid()
        ? ControllerHelpers::AutoAttackRange(target)
        : (player.AttackRange() + 65.0f);
}

inline AIBaseClient FindBouncePrimary(const AIHeroClient& desired,
                                      bool* routeBlocked = nullptr,
                                      bool* guaranteedCritical = nullptr) {
    if (routeBlocked) *routeBlocked = false;
    if (guaranteedCritical) *guaranteedCritical = false;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(desired, 1200.0f)) return {};
    const Vector3 desiredPosition = ControllerHelpers::PredictPosition(
        desired, 0.35f);
    AIBaseClient best{};
    float bestScore = -FLT_MAX;
    bool sawBlockedRoute = false;
    bool bestCritical = false;
    const auto consider = [&](const AIBaseClient& primary) {
        const float predictedHealth = SDK::HealthPrediction::GetPrediction(
            primary, 350, 0);
        if (!ControllerHelpers::ValidHostileUnitInGameplayRange(
                primary, DoubleUpTargetRange(primary)) || predictedHealth <= 0.0f ||
            ControllerHelpers::ProjectileWallBlocksFromPlayer(
                primary.Position(), 70.0f) ||
            primary.Position().Distance2D(desiredPosition) >
                500.0f + desired.BoundingRadius() ||
            !BounceConeContains(
                ToVec2(player.Position()), ToVec2(primary.Position()),
                ToVec2(desiredPosition), 500.0f + desired.BoundingRadius(),
                30.0f)) {
            return;
        }
        if (BounceBlocked(primary, desired, desiredPosition)) {
            sawBlockedRoute = true;
            return;
        }
        float score = 800.0f -
            primary.Position().Distance2D(desiredPosition);
        const bool critical = Engine::RuntimeSpells[0] &&
            Engine::RuntimeSpells[0]->GetDamage(primary) >= predictedHealth;
        if (critical) {
            score += 250.0f;
        }
        if (!primary.IsMoving()) score += 35.0f;
        if (score > bestScore) {
            best = primary;
            bestScore = score;
            bestCritical = critical;
        }
    };
    for (const auto& minion : GameObjects::EnemyMinions()) {
        consider(AIBaseClient(minion.Handle()));
    }
    for (const auto& monster : GameObjects::Jungle()) {
        consider(AIBaseClient(monster.Handle()));
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (enemy.NetworkId() != desired.NetworkId()) {
            consider(AIBaseClient(enemy.Handle()));
        }
    }
    if (routeBlocked) *routeBlocked = sawBlockedRoute && !best.IsValid();
    if (guaranteedCritical) *guaranteedCritical = best.IsValid() && bestCritical;
    return best;
}

inline AIBaseClient ResolveDoubleUpRoute(const AIHeroClient& target,
                                         bool& direct,
                                         bool& bounceBlocked,
                                         bool& guaranteedCritical) {
    const bool directAvailable = Engine::ValidEnemy(target, DoubleUpTargetRange(target));
    auto primary = FindBouncePrimary(
        target, &bounceBlocked, &guaranteedCritical);
    const bool useBounce = ShouldUseBounceRoute(
        directAvailable, primary.IsValid(), guaranteedCritical);
    direct = directAvailable && !useBounce;
    if (!useBounce) {
        primary = {};
        bounceBlocked = false;
        guaranteedCritical = false;
    }
    return primary;
}

inline float DoubleUpDamage(const AIBaseClient& target,
                            bool guaranteedCritical) {
    const float normal = SpellDamage(0, target);
    if (!guaranteedCritical) return normal;
    const auto player = GameObjects::Player();
    const float critMultiplier = player.IsValid()
        ? std::max(1.0f,
              ::CoreAIHeroClient::CritDamageMultiplier(player.Address()))
        : 1.0f;
    return normal * BounceCriticalMultiplier(critMultiplier);
}

inline float BulletTimeDamage(const AIBaseClient& target,
                              bool targetControlled) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const int rank = ControllerHelpers::SpellRank(3);
    const float raw = BulletTimeRawPerWave(
        rank, player.TotalAttackDamage(), player.AP()) *
        static_cast<float>(ConservativeBulletTimeHits(
            rank, targetControlled));
    return player.CalculatePhysicalDamage(target, raw);
}

inline float MakeItRainDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculateMagicDamage(target, MakeItRainRaw(
        ControllerHelpers::SpellRank(2), player.AP()));
}

inline float MakeItRainSecureDamage(const AIHeroClient& target) {
    return MakeItRainDamage(target) *
        MakeItRainSecureFraction(IsImmobile(target));
}

inline DoubleUpContext BuildDoubleUpContext(
    const AIHeroClient& target,
    bool direct,
    const AIBaseClient& primary,
    bool bounceBlocked,
    bool guaranteedCritical) {
    DoubleUpContext context{};
    context.Direct = direct;
    context.BounceRoute = primary.IsValid();
    context.AttackAvailable = LocalAttackReadySoon(target, 210);
    context.RecentlyAttacked = RecentlyAttacked(target);
    context.Lethal = DoubleUpDamage(target, guaranteedCritical) >=
        target.Health() + target.AllShield();
    context.BounceBlocker = bounceBlocked;
    const Vector3 destination = direct ? target.Position() : primary.Position();
    context.ProjectileWall = destination.IsValid() &&
        ControllerHelpers::ProjectileWallBlocksFromPlayer(
            destination, 70.0f);
    return context;
}

inline bool CastQ(const AIHeroClient& target, Mode mode) {
    if (!CanUse(0, mode) || !Engine::ValidEnemy(target, 1200.0f) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastQCastTick, 35)) return false;
    bool direct = false;
    bool blocked = false;
    bool guaranteedCritical = false;
    const auto primary = ResolveDoubleUpRoute(
        target, direct, blocked, guaranteedCritical);
    const auto context = BuildDoubleUpContext(
        target, direct, primary, blocked, guaranteedCritical);
    if (!ShouldCastDoubleUp(context)) return false;
    const bool casted = direct
        ? Engine::ControllerCastUnit(0, target)
        : Engine::ControllerCastUnit(0, primary);
    if (!casted) return false;
    LastQCastTick = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode) {
    if (!CanUse(1, mode, true) || !Engine::ValidEnemy(target) ||
        !InAutoAttackRange(target) ||
        !CastThrottlePassed(LastWCastTick, 90)) return false;
    if (LocalAttackReadySoon(target, 180) &&
        AutoDamage(target) >= target.Health() + target.AllShield()) {
        return false;
    }
    if (!Engine::ControllerCastSelf(1)) return false;
    LastWCastTick = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target,
                  Mode mode,
                  bool gapcloser,
                  bool ultimateSetup) {
    if (!CanUse(2, mode, gapcloser || ultimateSetup) ||
        !Engine::ValidEnemy(target, 1040.0f) ||
        !CastThrottlePassed(LastECastTick, 45)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(
        2, target, SDK::HitChance::High, false, &prediction);
    RainContext context{};
    context.PredictionHits = hit;
    context.AttackAvailable = LocalAttackReadySoon(target, 260);
    context.Lethal = MakeItRainSecureDamage(target) >=
        target.Health() + target.AllShield();
    context.Escaping = IsEscaping(target, 0.50f);
    context.Immobilized = IsImmobile(target);
    context.Gapcloser = gapcloser;
    context.UltimateSetup = ultimateSetup;
    if (!ShouldMakeItRain(context) ||
        !Engine::ControllerCastPosition(2, prediction.GetCastPosition())) {
        return false;
    }
    LastECastTick = Now();
    return true;
}

inline int CountConeTargets(const Vector3& aim,
                            float range = 1400.0f,
                            float halfAngle = 17.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range + 100.0f)) continue;
        const Vector3 predicted = ControllerHelpers::PredictPosition(
            enemy, 0.35f);
        if (PositionProjectileWall(3, predicted, 45.0f)) continue;
        if (DirectionConeContains(
                ToVec2(player.Position()), ToVec2(aim), ToVec2(predicted),
                range + enemy.BoundingRadius(), halfAngle)) {
            ++count;
        }
    }
    return count;
}

inline BulletTimeContext BuildBulletTimeContext(
    const AIHeroClient& target,
    const Vector3& aim,
    bool manual) {
    const auto player = GameObjects::Player();
    const int count = CountConeTargets(aim);
    BulletTimeContext context{};
    context.Manual = manual;
    context.SafeChannel = ChannelSafe();
    context.TargetInCone = player.IsValid() && DirectionConeContains(
        ToVec2(player.Position()), ToVec2(aim), ToVec2(aim),
        1400.0f + target.BoundingRadius(), 17.0f);
    context.ProjectileWall = PositionProjectileWall(3, aim, 45.0f);
    context.BetterAttack = LocalAttackReadySoon(target, 320);
    const bool controlled = IsImmobile(target) ||
        SDK::HasBuffOfType(target, SDK::BuffType::Slow);
    context.LethalChannel = BulletTimeDamage(target, controlled) >=
        target.Health() + target.AllShield();
    context.ValuableCone = controlled && count >= 1;
    context.TargetsInCone = count;
    context.MinimumTargets = Slider(UltimateMenu, "MinimumTargets", 2);
    return context;
}

inline bool CastR(const AIHeroClient& target,
                  Mode mode,
                  bool manual) {
    if (BulletTimeActive() || !Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !Engine::ValidEnemy(target, 1400.0f) ||
        !CastThrottlePassed(LastRCastTick, 160)) return false;
    if (!manual && !CanUse(3, mode)) return false;
    const Vector3 aim = ControllerHelpers::PredictPosition(target, 0.30f);
    const auto context = BuildBulletTimeContext(target, aim, manual);
    if (!ShouldStartBulletTime(context) ||
        !Engine::ControllerCastPosition(3, aim)) return false;
    LastRCastTick = Now();
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    AcquireBulletTimePause();
    return true;
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target,
    Mode mode) {
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool attack = OrbwalkerAttackRoute(target);
    bool bounceBlocked = false;
    bool guaranteedCritical = false;
    bool direct = false;
    const auto primary = ResolveDoubleUpRoute(
        target, direct, bounceBlocked, guaranteedCritical);
    const auto qContext = BuildDoubleUpContext(
        target, direct, primary, bounceBlocked, guaranteedCritical);
    const bool q = CanUse(0, mode) &&
        !ControllerHelpers::HasSpellShieldOrImmunity(target) &&
        ShouldCastDoubleUp(qContext);
    SDK::PredictionOutput ePrediction{};
    const bool eHit = CanUse(2, mode) && distance <= 1040.0f &&
        PredictionHits(2, target, SDK::HitChance::High, false, &ePrediction);
    RainContext rain{};
    rain.PredictionHits = eHit;
    rain.AttackAvailable = LocalAttackReadySoon(target, 260);
    rain.Lethal = MakeItRainSecureDamage(target) >=
        target.Health() + target.AllShield();
    rain.Escaping = IsEscaping(target, 0.50f);
    rain.Immobilized = IsImmobile(target);
    const bool e = eHit && ShouldMakeItRain(rain);
    const Vector3 rAim = ControllerHelpers::PredictPosition(target, 0.30f);
    const auto rContext = BuildBulletTimeContext(target, rAim, false);
    const bool r = CanUse(3, mode) && distance <= 1400.0f &&
        ShouldStartBulletTime(rContext);
    const std::array<bool, 4> reachable = {q, false, e, r};
    float estimated = EstimatedDamage(target, reachable, attack ? 2 : 0);
    if (q && guaranteedCritical) {
        estimated += std::max(
            0.0f, DoubleUpDamage(target, true) - SpellDamage(0, target));
    }
    if (e) {
        estimated += std::max(
            0.0f, MakeItRainDamage(target) - SpellDamage(2, target));
    }
    if (r) {
        const bool controlled = IsImmobile(target) ||
            SDK::HasBuffOfType(target, SDK::BuffType::Slow);
        estimated += std::max(
            0.0f, BulletTimeDamage(target, controlled) -
                      SpellDamage(3, target));
    }
    auto context = BaseTargetContext(
        target, estimated);
    context.AutoReachable = attack;
    context.DirectSpellReachable = q || e;
    context.ExecuteReachable = r;
    context.ProjectileBlocked = !attack && !q && !e && !r &&
        (bounceBlocked || qContext.ProjectileWall ||
         rContext.ProjectileWall ||
         OrbwalkerAttackProjectileBlocked(target));
    return context;
}

inline MarksmanTargeting::TargetContext KillSecureFacts(
    const AIHeroClient& target) {
    const float health = target.Health() + target.AllShield();
    const float distance = GameObjects::Player().Position().Distance2D(
        target.Position());
    bool bounceBlocked = false;
    bool guaranteedCritical = false;
    bool direct = false;
    const auto primary = ResolveDoubleUpRoute(
        target, direct, bounceBlocked, guaranteedCritical);
    const auto qContext = BuildDoubleUpContext(
        target, direct, primary, bounceBlocked, guaranteedCritical);
    const bool q = CanUse(0, Mode::Automatic) &&
        DoubleUpDamage(target, guaranteedCritical) >= health &&
        !ControllerHelpers::HasSpellShieldOrImmunity(target) &&
        ShouldCastDoubleUp(qContext);

    SDK::PredictionOutput ePrediction{};
    const bool eHit = CanUse(2, Mode::Automatic) &&
        MakeItRainSecureDamage(target) >= health && distance <= 1040.0f &&
        PredictionHits(2, target, SDK::HitChance::High, false, &ePrediction);
    RainContext rain{};
    rain.PredictionHits = eHit;
    rain.Lethal = true;
    const bool e = eHit && ShouldMakeItRain(rain);

    auto context = BaseTargetContext(
        target, std::max(q ? DoubleUpDamage(target, guaranteedCritical) : 0.0f,
                         e ? MakeItRainSecureDamage(target) : 0.0f));
    context.DirectSpellReachable = q || e;
    context.ProjectileBlocked = !q && !e &&
        (bounceBlocked || qContext.ProjectileWall);
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred,
                                      Mode mode) {
    LastSmartTarget = ControllerHelpers::SelectReachableEnemy(
        preferred, 1450.0f,
        [mode](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode);
        });
    return LastSmartTarget;
}

inline void RefreshLoveTapFocus(Mode mode,
                                const AIHeroClient& smartTarget) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    const bool smartAttackKill = ImmediateAttackKillRoute(smartTarget);
    auto owned = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 800.0f);
    if (!combat || !Bool(LoveTapMenu, "EnableSwitch", true) ||
        !owned.IsValid() || LoveTapMarked(owned) ||
        !InAutoAttackRange(owned) ||
        (smartAttackKill &&
         owned.NetworkId() != smartTarget.NetworkId())) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        owned = {};
    }
    if (smartAttackKill) {
        if (!owned.IsValid()) {
            (void)ForceImmediateAttackKill(
                smartTarget, 450, OwnedFocusTargetId, OwnedFocusUntil);
        }
        return;
    }
    if (owned.IsValid() || !combat) return;

    auto current = ControllerHelpers::OrbwalkerHeroTarget(800.0f);
    if (!current.IsValid()) current = smartTarget;
    if (!Engine::ValidEnemy(current) || !LoveTapMarked(current) ||
        !InAutoAttackRange(current)) return;
    const auto selected = ControllerHelpers::PlayerSelectedEnemy(800.0f);
    if (selected.IsValid() && selected.NetworkId() == current.NetworkId()) {
        return;
    }
    const float currentTwoAutos = AutoDamage(current) * 2.0f;
    const bool currentKillableSoon = currentTwoAutos >=
        current.Health() + current.AllShield();

    AIHeroClient alternate{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 800.0f) ||
            enemy.NetworkId() == current.NetworkId() ||
            LoveTapMarked(enemy) || !InAutoAttackRange(enemy)) continue;
        if (!ShouldSwapLoveTap(
                true, true, currentKillableSoon, true)) continue;
        float score = 100.0f - enemy.HealthPercent();
        if (smartTarget.IsValid() &&
            smartTarget.NetworkId() == enemy.NetworkId()) score += 120.0f;
        if (score > bestScore) {
            alternate = enemy;
            bestScore = score;
        }
    }
    if (alternate.IsValid()) {
        (void)SetTemporaryOrbwalkerFocus(
            alternate, ControllerHelpers::AutoAttackRange(alternate), 700,
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline void RefreshChannelPause() {
    const bool active = BulletTimeActive();
    if (active && !BulletTimeOwnsOrbwalkerPause) {
        AcquireBulletTimePause();
    } else if (!active && BulletTimeOwnsOrbwalkerPause) {
        Orbwalker::AttackEnabled(PreviousAttackEnabled);
        Orbwalker::MoveEnabled(PreviousMoveEnabled);
        BulletTimeOwnsOrbwalkerPause = false;
    }
}

inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    return Engine::ValidEnemy(target, 1040.0f) &&
           CastE(target, Mode::Automatic, true, false);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = ControllerHelpers::SelectReachableEnemy(
        preferred, 1200.0f,
        [](const AIHeroClient& enemy) {
            return KillSecureFacts(enemy);
    });
    if (!Engine::ValidEnemy(target)) return false;
    const float health = target.Health() + target.AllShield();
    bool bounceBlocked = false;
    bool guaranteedCritical = false;
    bool direct = false;
    const auto primary = ResolveDoubleUpRoute(
        target, direct, bounceBlocked, guaranteedCritical);
    const auto qContext = BuildDoubleUpContext(
        target, direct, primary, bounceBlocked, guaranteedCritical);
    if (DoubleUpDamage(target, guaranteedCritical) >= health &&
        ShouldCastDoubleUp(qContext) && CastQ(target, Mode::Automatic)) {
        return true;
    }
    return MakeItRainSecureDamage(target) >= health &&
           CastE(target, Mode::Automatic, false, false);
}

inline bool TryUltimate(const AIHeroClient& target,
                        Mode mode,
                        bool manual) {
    if (!Engine::ValidEnemy(target, 1400.0f)) return false;
    const Vector3 aim = ControllerHelpers::PredictPosition(target, 0.30f);
    const auto ultimate = BuildBulletTimeContext(target, aim, manual);
    if (!ShouldStartBulletTime(ultimate)) return false;
    const bool controlled = IsImmobile(target) ||
        SDK::HasBuffOfType(target, SDK::BuffType::Slow);
    const bool canPrime = Bool(UltimateMenu, "EBeforeR", true) &&
        ShouldPrimeBulletTime(
            controlled, ControllerHelpers::Ready(2),
            ControllerHelpers::HasResourceFor({2, 3}));
    if (canPrime && CastE(target, mode, false, true)) {
        PendingUltimateTargetId = static_cast<int>(target.NetworkId());
        PendingUltimateUntil = Now() + 1100;
        PendingUltimateManual = manual;
        return true;
    }
    return CastR(target, mode, manual);
}

inline bool TryPendingUltimate(Mode mode) {
    if (PendingUltimateTargetId == 0) return false;
    if (Now() > PendingUltimateUntil) {
        PendingUltimateTargetId = PendingUltimateUntil = 0;
        PendingUltimateManual = false;
        return false;
    }
    if (!PendingUltimateManual &&
        mode != Mode::Combo && mode != Mode::Harass) {
        PendingUltimateTargetId = PendingUltimateUntil = 0;
        return false;
    }
    const auto target = ControllerHelpers::HeroByNetworkId(
        PendingUltimateTargetId);
    if (!Engine::ValidEnemy(target, 1400.0f)) {
        PendingUltimateTargetId = PendingUltimateUntil = 0;
        PendingUltimateManual = false;
        return false;
    }
    if (!ChannelSafe()) {
        PendingUltimateTargetId = PendingUltimateUntil = 0;
        PendingUltimateManual = false;
        return false;
    }
    const Vector3 aim = ControllerHelpers::PredictPosition(target, 0.30f);
    if (PositionProjectileWall(3, aim, 45.0f)) {
        PendingUltimateTargetId = PendingUltimateUntil = 0;
        PendingUltimateManual = false;
        return false;
    }
    if (CastR(target, mode, PendingUltimateManual)) {
        PendingUltimateTargetId = PendingUltimateUntil = 0;
        PendingUltimateManual = false;
        return true;
    }
    // Keep the short E -> R plan stable while Make It Rain is landing. The
    // orbwalker still moves/attacks; only unrelated controller casts wait.
    return true;
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (mode == Mode::Combo && TryUltimate(target, mode, false)) return true;
    if (CastQ(target, mode)) return true;
    return CastE(target, mode, false, false);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    RefreshChannelPause();
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    if (!combat) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    }
    if (BulletTimeActive()) {
        ClearTemporaryOrbwalkerFocus(
            OwnedFocusTargetId, OwnedFocusUntil);
        return false;
    }
    if (TryPendingUltimate(
            combat ? mode : Mode::Automatic)) return true;
    if (ManualUltimatePressed()) {
        const auto target = ControllerHelpers::NearestEnemyToPlayer(
            preferred, 1400.0f);
        if (Engine::ValidEnemy(target) &&
            TryUltimate(target, Mode::Automatic, true)) return true;
    }
    if (TryAntiGapcloser()) return true;
    if (TryKillSecure(preferred)) return true;
    if (combat) {
        const auto target = SelectSmartTarget(preferred, mode);
        RefreshLoveTapFocus(mode, target);
        return TryCombat(target, mode);
    }
    if (mode == Mode::Flee) {
        const auto target = ControllerHelpers::NearestEnemyToPlayer(
            preferred, 1040.0f);
        return Engine::ValidEnemy(target) &&
               CastE(target, Mode::Flee, true, false);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return Engine::TryFarm(mode);
    return false;
}

inline void ObserveSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender)) {
        const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
        const auto player = GameObjects::Player();
        if (analysis.Valid && player.IsValid() &&
            (analysis.TargetsPlayer || analysis.CrossesPlayer ||
             (analysis.Committed &&
              analysis.Enemy.Position().Distance2D(player.Position()) <=
                  650.0f))) {
            IncomingThreatUntil = std::max(
                IncomingThreatUntil,
                std::max(analysis.CommitmentUntilTick,
                         analysis.LineThreatUntilTick));
        }
        return;
    }
    if (args.IsAutoAttack) {
        return;
    }
    const int now = Now();
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        ControllerHelpers::SpellEventNameContainsAny(
            args, {"missfortunericochetshot"})) {
        LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"missfortuneviciousstrikes"})) {
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"missfortunescattershot"})) {
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"missfortunebullettime"})) {
        LastRCastTick = now;
        PendingUltimateTargetId = PendingUltimateUntil = 0;
        PendingUltimateManual = false;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (BulletTimeActive()) {
        args.Process = false;
        return;
    }
    auto focus = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 800.0f);
    if (focus.IsValid() &&
        !RedirectBeforeAttackToFocus(args, focus)) {
        ClearTemporaryOrbwalkerFocus(
            OwnedFocusTargetId, OwnedFocusUntil);
        focus = {};
    }
    if (!focus.IsValid() && args.Target.IsValid() && args.Target.IsHero()) {
        focus = AIHeroClient(args.Target.Handle());
    }
    if (focus.IsValid() &&
        (LastMode == Mode::Combo || LastMode == Mode::Harass)) {
        (void)CastW(focus, LastMode);
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "MissFortuneMechanics", "Miss Fortune Mechanics"));
    LoveTapMenu = TacticsMenu->AddSubMenu(new Menu(
        "LoveTapLogic", "Love Tap / Orbwalker"));
    LoveTapMenu->Add(new MenuBool(
        "EnableSwitch", "Switch to an unmarked reachable target", true));
    BounceMenu = TacticsMenu->AddSubMenu(new Menu(
        "DoubleUpLogic", "Double Up Bounce"));
    BounceMenu->Add(new MenuSeparator(
        "ExactBounce", "Q bounce rejects a closer body in the cone"));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu(
        "BulletTimeLogic", "Bullet Time"));
    UltimateMenu->Add(new MenuSlider(
        "MinimumTargets", "Minimum targets in R cone", 2, 1, 5));
    UltimateMenu->Add(new MenuBool(
        "EBeforeR", "Use E to hold the R target first", true));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    IncomingThreatUntil = 0;
    PendingUltimateTargetId = PendingUltimateUntil = 0;
    PendingUltimateManual = false;
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    LastMode = Mode::None;
    BulletTimeOwnsOrbwalkerPause = false;
    PreviousAttackEnabled = PreviousMoveEnabled = true;
    LastSmartTarget = {};
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    PendingUltimateTargetId = PendingUltimateUntil = 0;
    PendingUltimateManual = false;
    if (BulletTimeOwnsOrbwalkerPause) {
        Orbwalker::AttackEnabled(PreviousAttackEnabled);
        Orbwalker::MoveEnabled(PreviousMoveEnabled);
        BulletTimeOwnsOrbwalkerPause = false;
    }
    TacticsMenu = LoveTapMenu = BounceMenu = UltimateMenu = nullptr;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Reject a selected target with no AA/direct-Q/bounce-Q/E/R route",
    "Use a direct Q inside range and solve a minion bounce outside range",
    "Reject a Q bounce when a closer body occupies the same cone",
    "Preserve a ready attack and weave Q immediately after the attack",
    "Switch Love Tap only from a marked non-killable current target",
    "Never override an explicitly selected current target for Love Tap",
    "Redirect BeforeAttack to the owned Love Tap target and cast W",
    "Release Love Tap focus after the attack or when the mode ends",
    "Use E for escape control, anti-gapcloser, lethal or R setup",
    "Require a safe valuable cone before starting Bullet Time",
    "Exclude projectile-wall-blocked targets from Q and Bullet Time routes",
    "Own orbwalker pause only for the live R channel and restore it",
    "Clear focus and restore prior orbwalker states on unload",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::MissFortune;
    controller.ControllerId =
        "champion.kuroaio.ai.missfortune.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIMissFortune.md";
    controller.ImplementationSummary =
        "Direct and collision-ranked bounce Q, Love Tap orbwalker steering, "
        "BeforeAttack W, E setup/peel and owned safe Bullet Time channel.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveSpell;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack =
        &CaptureAfterAttackAndReleaseOwnedFocusEvent<
            &LastAfterAttackTargetId, &LastAfterAttackTick,
            &OwnedFocusTargetId, &OwnedFocusUntil>;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 760, 900>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::MissFortune
