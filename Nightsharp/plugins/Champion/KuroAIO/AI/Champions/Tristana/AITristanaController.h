#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AITristanaGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>

namespace Plugins::KuroAIO::AI::Controllers::Tristana {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* ChargeMenu = nullptr;
inline Menu* JumpMenu = nullptr;
inline Menu* BusterMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline constexpr int kCombatWAfterRLockMs = 900;
inline constexpr int kCombatWRetryThrottleMs = 400;
inline constexpr float kMinimumRocketJumpDisplacement = 200.0f;

// Buster Shot displaces the target. A jump request immediately after it is
// based on stale target state and usually becomes an accidental second commit.
inline bool RecentBusterShot(int lockMs = kCombatWAfterRLockMs) {
    const int now = Now();
    return LastRCastTick > 0 && now >= LastRCastTick &&
        now - LastRCastTick < std::max(0, lockMs);
}

inline bool MeaningfulRocketJumpLanding(const Vector3& landing) {
    const auto player = GameObjects::Player();
    return player.IsValid() && landing.IsValid() && !landing.IsZero() &&
        player.Position().Distance2D(landing) >=
            kMinimumRocketJumpDisplacement;
}
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int PendingRapidFireTargetId = 0;
inline int PendingRapidFireUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline constexpr std::uint32_t kFocusLeaseOwnerId = 0x54524953u; // "TRIS"
inline SDK::KuroTargetSelector::ProviderToken TristanaProviderToken = 0;
inline SDK::KuroTargetSelector::IKuroTargetSelector* TristanaProviderService = nullptr;
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

inline int ExplosiveChargeStacks(const AIBaseClient& target) {
    return target.IsValid() && target.HasBuff("TristanaECharge")
        ? ChargeStacks(target.GetBuffCount("TristanaECharge"))
        : 0;
}

inline bool HasExplosiveCharge(const AIBaseClient& target) {
    return target.IsValid() && target.HasBuff("TristanaECharge");
}

inline float ExplosiveChargeRemainingMs(const AIBaseClient& target) {
    return BuffRemainingMs(target, "TristanaECharge");
}

inline float ExplosiveChargeDamage(const AIBaseClient& target,
                                   int bonusStacks = 0) {
    const auto player = GameObjects::Player();
    const int rank = ControllerHelpers::SpellRank(2);
    if (!player.IsValid() || !target.IsValid() || rank <= 0 ||
        !HasExplosiveCharge(target)) {
        return 0.0f;
    }
    const int stacks = ChargeStacks(
        ExplosiveChargeStacks(target) + std::max(0, bonusStacks));
    const float critMultiplier = std::max(
        1.0f, ::CoreAIHeroClient::CritDamageMultiplier(player.Address()));
    const float raw = ExplosiveChargeRaw(
        rank, stacks, player.BonusAttackDamage(), player.AP(),
        player.Crit(), critMultiplier);
    return player.CalculatePhysicalDamage(target, raw);
}

inline bool BuildTristanaTargetFacts(
    const SDK::KuroTargetSelector::TargetRequest& request,
    const AIHeroClient& target,
    SDK::KuroTargetSelector::TargetFacts& facts) {
    (void)request;
    const int stacks = ExplosiveChargeStacks(target);
    facts.AddProviderFact("tristana.e_stacks", static_cast<float>(stacks));
    facts.AddProviderFact(
        "tristana.e_remaining_ms", ExplosiveChargeRemainingMs(target));
    return true;
}

inline SDK::KuroTargetSelector::RejectReason ValidateTristanaTarget(
    const SDK::KuroTargetSelector::TargetProviderContext& context) {
    (void)context;
    return SDK::KuroTargetSelector::RejectReason::None;
}

inline SDK::KuroTargetSelector::ScoreContribution ScoreTristanaTarget(
    const SDK::KuroTargetSelector::TargetProviderContext& context) {
    if (!context.Facts) return {};
    const float stacks = context.Facts->ProviderFactValue(
        "tristana.e_stacks");
    const float remaining = context.Facts->ProviderFactValue(
        "tristana.e_remaining_ms");
    const float urgency = remaining > 0.0f && remaining <= 900.0f
        ? (900.0f - remaining) * 0.08f : 0.0f;
    return {
        "tristana-charge", "Explosive Charge stacks/urgency",
        stacks * 18.0f + urgency, 0.0f, 120.0f
    };
}

inline void EnsureTristanaTargetProvider() {
    auto* service = SDK::KuroTargetSelector::ActiveService();
    if (!service) {
        // Kuro may be inactive while the object is still alive. Unregister in
        // that case, but never dereference a service after its plugin unload.
        if (TristanaProviderService && TristanaProviderToken &&
            SDK::KuroTargetSelector::LiveService() ==
                TristanaProviderService) {
            (void)TristanaProviderService->UnregisterProvider(
                TristanaProviderToken);
        }
        TristanaProviderToken = 0;
        TristanaProviderService = nullptr;
        return;
    }
    if (TristanaProviderService != service) {
        if (TristanaProviderService && TristanaProviderToken &&
            SDK::KuroTargetSelector::LiveService() ==
                TristanaProviderService) {
            (void)TristanaProviderService->UnregisterProvider(
                TristanaProviderToken);
        }
        TristanaProviderToken = 0;
        TristanaProviderService = service;
    }
    if (TristanaProviderToken) return;

    const SDK::KuroTargetSelector::TargetRuleProvider provider{
        kFocusLeaseOwnerId,
        "tristana.explosive_charge",
        SDK::KuroTargetSelector::ProviderPriorityBand::ChampionMechanic,
        &BuildTristanaTargetFacts,
        &ValidateTristanaTarget,
        &ScoreTristanaTarget,
    };
    TristanaProviderToken = service->RegisterProvider(provider);
}

inline float TargetedRange() {
    const auto player = GameObjects::Player();
    return player.IsValid()
        ? DynamicTargetedRange(player.AttackRange())
        : 550.0f;
}

inline void RefreshDynamicRanges() {
    // Spell::CastOnUnit performs a center-distance check, while the live game
    // data marks E/R as using bounding boxes. Keep a neutral display/default
    // allowance here; each actual cast below installs the exact target radius.
    const float range = TargetedRange() + 65.0f;
    if (Engine::RuntimeSpells[2]) Engine::RuntimeSpells[2]->Range = range;
    if (Engine::RuntimeSpells[3]) Engine::RuntimeSpells[3]->Range = range;
}

inline void PrepareTargetedCastRange(int index,
                                     const AIBaseClient& target) {
    if (index < 0 || index >= 4 || !Engine::RuntimeSpells[index] ||
        !target.IsValid()) return;
    Engine::RuntimeSpells[index]->Range =
        TargetedRange() + std::max(0.0f, target.BoundingRadius()) + 2.0f;
}

inline bool TargetedSpellReach(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid() &&
        player.Position().Distance2D(target.Position()) <=
            TargetedRange() + target.BoundingRadius();
}

inline bool RocketJumpTargetIsWorthCommitting(
    const AIHeroClient& target,
    const Vector3& landing,
    Mode mode) {
    return ShouldCommitTargetedRocketJump(
        mode == Mode::Combo || mode == Mode::Harass,
        RecentBusterShot(),
        target.IsValid() && !InAutoAttackRange(target),
        MeaningfulRocketJumpLanding(landing));
}

inline void ReleaseTristanaFocus() {
    (void)AICombatTargetCoordinator::FocusLease::Release(
        kFocusLeaseOwnerId);
    OwnedFocusTargetId = 0;
    OwnedFocusUntil = 0;
}

inline bool AcquireTristanaFocus(const AIHeroClient& target,
                                 int lifetimeMs) {
    if (!Engine::ValidEnemy(target, 850.0f) ||
        ControllerHelpers::IsCommonUntargetableOrImmune(target) ||
        OrbwalkerAttackProjectileBlocked(target)) {
        return false;
    }
    const int now = Now();
    if (!AICombatTargetCoordinator::FocusLease::Acquire(
            kFocusLeaseOwnerId,
            static_cast<int>(target.NetworkId()),
            AICombatTargetCoordinator::LeaseStrength::Hard,
            now,
            std::max(1, lifetimeMs),
            100)) {
        return false;
    }
    OwnedFocusTargetId = static_cast<int>(target.NetworkId());
    OwnedFocusUntil = now + std::max(1, lifetimeMs);
    return true;
}

inline AIHeroClient CurrentTristanaFocus(float range) {
    const int now = Now();
    const auto lease = AICombatTargetCoordinator::FocusLease::Snapshot(now);
    if (lease.OwnerId != kFocusLeaseOwnerId ||
        lease.TargetNetworkId <= 0 ||
        lease.Status == AICombatTargetCoordinator::LeaseStatus::Inactive ||
        lease.Status == AICombatTargetCoordinator::LeaseStatus::Terminal ||
        lease.ManualOverride) {
        return {};
    }

    const auto target = ControllerHelpers::HeroByNetworkId(
        lease.TargetNetworkId);
    if (lease.Status == AICombatTargetCoordinator::LeaseStatus::Suspended &&
        target.IsValid() &&
        (!HasExplosiveCharge(target) || ExplosiveChargeStacks(target) >= 4)) {
        ReleaseTristanaFocus();
        return {};
    }
    const bool legal = Engine::ValidEnemy(target, range) &&
        HasExplosiveCharge(target) &&
        ExplosiveChargeStacks(target) < 4 &&
        InAutoAttackRange(target) &&
        !ControllerHelpers::IsCommonUntargetableOrImmune(target) &&
        !OrbwalkerAttackProjectileBlocked(target);
    if (!legal) {
        if (lease.Status == AICombatTargetCoordinator::LeaseStatus::Active) {
            (void)AICombatTargetCoordinator::FocusLease::BlockedTarget(
                lease.TargetNetworkId, now);
        }
        return {};
    }

    if (lease.Status == AICombatTargetCoordinator::LeaseStatus::Suspended) {
        const int ttl = std::max(
            1, OwnedFocusUntil > now ? OwnedFocusUntil - now : 850);
        if (!AICombatTargetCoordinator::FocusLease::Restore(
            kFocusLeaseOwnerId,
            lease.TargetNetworkId,
            now,
            ttl)) {
            return {};
        }
        OwnedFocusUntil = now + ttl;
    }
    OwnedFocusTargetId = lease.TargetNetworkId;
    if (OwnedFocusUntil <= now) OwnedFocusUntil = now + 1;
    return target;
}

inline bool CastQ(const AIHeroClient& target, Mode mode) {
    if (!CanUse(0, mode, true) || !Engine::ValidEnemy(target) ||
        !InAutoAttackRange(target) ||
        !CastThrottlePassed(LastQCastTick, 75)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    LastQCastTick = Now();
    PendingRapidFireTargetId = PendingRapidFireUntil = 0;
    return true;
}

inline bool CastE(const AIHeroClient& target,
                  Mode mode,
                  bool attackIntent) {
    if (!CanUse(2, mode, true) || !TargetedSpellReach(target) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastECastTick, 80)) return false;
    const bool projectileWall = TargetProjectileWall(2, target, 45.0f);
    if (!ShouldCastExplosiveCharge(
            true, true, attackIntent, HasExplosiveCharge(target),
            projectileWall)) {
        return false;
    }
    PrepareTargetedCastRange(2, target);
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastECastTick = Now();
    PendingRapidFireTargetId = static_cast<int>(target.NetworkId());
    PendingRapidFireUntil = Now() + 420;
    (void)AcquireTristanaFocus(target, 1000);
    return true;
}

inline bool SafeJumpLanding(const Vector3& position,
                            const AIHeroClient& target,
                            int maximumEnemies) {
    if (!position.IsValid() || position.IsZero() ||
        SDK::NavMesh::IsWall(position) ||
        Engine::CountEnemiesAt(position, 625.0f) > maximumEnemies ||
        HasReadyPointClickThreatAt(position) ||
        HasReadyDashHazardAt(position)) return false;
    return Engine::PositionDangerScore(
         position, target, Engine::ResolvedSpecs[1]) > -10000.0f;
}

inline JumpContext BuildJumpContext(const AIHeroClient& target,
                                    const SDK::PredictionOutput& prediction,
                                    int maximumEnemies,
                                    bool flee = false) {
    JumpContext context{};
    const Vector3 landing = prediction.GetCastPosition();
    context.PredictionHits = ControllerHelpers::PredictionAtLeast(
        prediction, SDK::HitChance::High);
    context.LandingSafe = SafeJumpLanding(
        landing, target, maximumEnemies);
    const bool detonatesCharge = HasExplosiveCharge(target) &&
        WillDetonateCharge(ExplosiveChargeStacks(target), 1);
    context.Lethal = SpellDamage(1, target) +
        (detonatesCharge
             ? ExplosiveChargeDamage(target, 1) : 0.0f) >=
        target.Health() + target.AllShield();
    context.Flee = flee;
    context.BetterAttack = LocalAttackReadySoon(target, 260);
    context.EnemiesAtLanding = Engine::CountEnemiesAt(landing, 625.0f);
    context.MaximumEnemies = maximumEnemies;
    return context;
}

inline bool CastWOnTarget(const AIHeroClient& target,
                          Mode mode) {
    // W is a high-commit execute/engage tool, not generic automatic KS.
    if (mode != Mode::Combo && mode != Mode::Harass) return false;
    if (!CanUse(1, mode) ||
        !Engine::ValidEnemy(target, 940.0f) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastWCastTick, kCombatWRetryThrottleMs)) {
        return false;
    }
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(
        1, target, SDK::HitChance::High, false, &prediction);
    const Vector3 landing = prediction.GetCastPosition();
    if (!hit ||
        !RocketJumpTargetIsWorthCommitting(target, landing, mode)) {
        return false;
    }
    const int maximumEnemies = Slider(JumpMenu, "MaximumEnemies", 1);
    auto context = BuildJumpContext(target, prediction, maximumEnemies);
    context.PredictionHits = context.PredictionHits && hit;
    if (!ShouldRocketJump(context) ||
        !Engine::ControllerCastPosition(1, landing)) return false;
    LastWCastTick = Now();
    return true;
}

inline bool CastWFlee(const AIHeroClient& threat) {
    if (!CanUse(1, Mode::Flee, true) ||
        !CastThrottlePassed(LastWCastTick, 80)) return false;
    const Vector3 landing = Engine::BestSafePosition(
        Engine::ResolvedSpecs[1], threat, AimPolicy::SafeCursor);
    if (!MeaningfulRocketJumpLanding(landing)) return false;
    const int maximumEnemies = Slider(JumpMenu, "MaximumEnemies", 1);
    JumpContext context{};
    context.PredictionHits = landing.IsValid() && !landing.IsZero();
    context.LandingSafe = SafeJumpLanding(
        landing, threat, maximumEnemies);
    context.Flee = true;
    context.EnemiesAtLanding = Engine::CountEnemiesAt(landing, 625.0f);
    context.MaximumEnemies = maximumEnemies;
    if (!ShouldRocketJump(context) ||
        !Engine::ControllerCastPosition(1, landing)) return false;
    LastWCastTick = Now();
    return true;
}

inline BusterContext BuildBusterContext(const AIHeroClient& target,
                                        Mode mode,
                                        bool gapcloser) {
    BusterContext context{};
    if (!TargetedSpellReach(target)) return context;
    const float health = target.Health() + target.AllShield();
    const float rDamage = SpellDamage(3, target);
    const bool detonatesCharge = HasExplosiveCharge(target) &&
        WillDetonateCharge(ExplosiveChargeStacks(target), 1);
    const float chargeDamage = detonatesCharge
        ? ExplosiveChargeDamage(target, 1) : 0.0f;
    context.InRange = true;
    context.Lethal = rDamage >= health;
    context.DetonationLethal = detonatesCharge &&
        rDamage + chargeDamage >= health;
    context.Gapcloser = gapcloser;
    context.SelfPeel = gapcloser ||
        (mode == Mode::Flee &&
         GameObjects::Player().Position().Distance2D(target.Position()) <
             475.0f);
    context.AttackAvailable = LocalAttackReadySoon(target, 220);
    context.ProjectileWall = TargetProjectileWall(3, target, 55.0f);
    return context;
}

inline bool CastR(const AIHeroClient& target,
                  Mode mode,
                  bool gapcloser,
                  bool manual) {
    if (!Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !TargetedSpellReach(target) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastRCastTick, 90)) return false;
    if (!manual && !CanUse(3, mode, gapcloser)) return false;
    const auto context = BuildBusterContext(target, mode, gapcloser);
    if (context.ProjectileWall) return false;
    if (!manual && !ShouldCastBusterShot(context)) return false;
    if (manual && !context.InRange) return false;
    PrepareTargetedCastRange(3, target);
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastRCastTick = Now();
    ReleaseTristanaFocus();
    return true;
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target,
    Mode mode) {
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool attack = OrbwalkerAttackRoute(target);
    const bool spellShield =
        ControllerHelpers::HasSpellShieldOrImmunity(target);
    const bool eWall = TargetProjectileWall(2, target, 45.0f);
    const bool e = CanUse(2, mode, true) && TargetedSpellReach(target) &&
        attack && !spellShield && !eWall && !HasExplosiveCharge(target);
    const bool wCandidate =
        mode == Mode::Combo || mode == Mode::Harass;
    SDK::PredictionOutput wPrediction{};
    const bool wReady = wCandidate && !RecentBusterShot() &&
        CanUse(1, mode) &&
        distance <= 940.0f &&
        PredictionHits(1, target, SDK::HitChance::High, false, &wPrediction);
    const Vector3 wLanding = wPrediction.GetCastPosition();
    const bool w = wReady && !spellShield &&
        RocketJumpTargetIsWorthCommitting(target, wLanding, mode) &&
        ShouldRocketJump(BuildJumpContext(
            target, wPrediction, Slider(JumpMenu, "MaximumEnemies", 1)));
    const bool rReady = CanUse(3, mode) && TargetedSpellReach(target) &&
        !spellShield;
    const auto rContext = BuildBusterContext(target, mode, false);
    const bool r = rReady && ShouldCastBusterShot(rContext);
    const std::array<bool, 4> reachable = {false, w, e, r};
    int expectedAutos = 0;
    if (attack) {
        expectedAutos = HasExplosiveCharge(target)
            ? std::max(1, 4 - ExplosiveChargeStacks(target))
            : (e ? 4 : 2);
    }
    float estimated = AutoDamage(target) *
        static_cast<float>(expectedAutos);
    if (w) estimated += SpellDamage(1, target);
    if (e) estimated += SpellDamage(2, target);
    if (r) {
        const bool detonatesCharge = HasExplosiveCharge(target) &&
            WillDetonateCharge(ExplosiveChargeStacks(target), 1);
        estimated += SpellDamage(3, target) +
            (detonatesCharge
                 ? ExplosiveChargeDamage(target, 1) : 0.0f);
    }
    auto context = BaseTargetContext(target, estimated);
    context.AutoReachable = attack;
    context.SetupReachable = e;
    context.DirectSpellReachable = w;
    context.ExecuteReachable = r;
    context.ProjectileBlocked = !attack && !w && !e && !r &&
        (OrbwalkerAttackProjectileBlocked(target) || eWall ||
         rContext.ProjectileWall);
    if (HasExplosiveCharge(target)) {
        context.Priority += 165.0f +
            static_cast<float>(ExplosiveChargeStacks(target)) * 65.0f;
    }
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred,
                                      Mode mode) {
    LastSmartTarget = ControllerHelpers::SelectReachableEnemy(
        preferred, 950.0f,
        [mode](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode);
        });
    return LastSmartTarget;
}

inline bool ImmediateAttackKill(const AIHeroClient& target) {
    return ImmediateAttackKillRoute(target);
}

inline AIHeroClient ProtectedImmediateAttackKill() {
    const auto selected = ControllerHelpers::PlayerSelectedEnemy(850.0f);
    if (ImmediateAttackKill(selected)) return selected;
    const auto orbTarget = ControllerHelpers::OrbwalkerHeroTarget(850.0f);
    return ImmediateAttackKill(orbTarget) ? orbTarget : AIHeroClient{};
}

inline void RefreshChargeFocus(Mode mode,
                               const AIHeroClient& preferred) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    auto protectedKill = ProtectedImmediateAttackKill();
    if (!protectedKill.IsValid() && ImmediateAttackKill(preferred)) {
        protectedKill = preferred;
    }
    auto owned = CurrentTristanaFocus(850.0f);
    const auto leaseBeforeSelection =
        AICombatTargetCoordinator::FocusLease::Snapshot(Now());
    if (!combat || !owned.IsValid() || !HasExplosiveCharge(owned) ||
        ExplosiveChargeStacks(owned) >= 4 || !InAutoAttackRange(owned) ||
        (protectedKill.IsValid() &&
         protectedKill.NetworkId() != owned.NetworkId())) {
        if (!combat || (owned.IsValid() &&
            (ExplosiveChargeStacks(owned) >= 4 ||
             !InAutoAttackRange(owned) ||
             (protectedKill.IsValid() &&
              protectedKill.NetworkId() != owned.NetworkId())))) {
            ReleaseTristanaFocus();
        }
        owned = {};
    }
    if (owned.IsValid() || !combat ||
        (leaseBeforeSelection.OwnerId == kFocusLeaseOwnerId &&
         leaseBeforeSelection.Status ==
             AICombatTargetCoordinator::LeaseStatus::Suspended)) {
        return;
    }
    if (protectedKill.IsValid()) {
        if (ImmediateAttackKill(protectedKill)) {
            (void)AcquireTristanaFocus(protectedKill, 450);
        }
        return;
    }

    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 850.0f) ||
            !InAutoAttackRange(enemy)) continue;
        const int stacks = ExplosiveChargeStacks(enemy);
        const bool protectsDifferentKill = protectedKill.IsValid() &&
            protectedKill.NetworkId() != enemy.NetworkId();
        if (!ShouldFocusCharge(
                HasExplosiveCharge(enemy), stacks, true,
                protectsDifferentKill)) continue;
        const bool oneAutoKills = AutoDamage(enemy) >=
            enemy.Health() + enemy.AllShield();
        float score = static_cast<float>(stacks) * 160.0f -
            enemy.HealthPercent();
        if (oneAutoKills) score += 520.0f;
        const float remaining = ExplosiveChargeRemainingMs(enemy);
        if (remaining > 0.0f && remaining <= 900.0f) {
            score += 360.0f + (900.0f - remaining) * 0.25f;
        }
        if (preferred.IsValid() &&
            preferred.NetworkId() == enemy.NetworkId()) score += 200.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    if (best.IsValid()) {
        (void)AcquireTristanaFocus(best, 850);
    }
}

inline bool TryPendingRapidFire() {
    if (PendingRapidFireUntil < Now()) {
        PendingRapidFireTargetId = PendingRapidFireUntil = 0;
        return false;
    }
    const auto target = ControllerHelpers::HeroByNetworkId(
        PendingRapidFireTargetId);
    return Engine::ValidEnemy(target) && CastQ(target, LastMode);
}

inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    return Engine::ValidEnemy(target) &&
           CastR(target, Mode::Automatic, true, false);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = ControllerHelpers::SelectReachableEnemy(
        preferred, 950.0f,
        [](const AIHeroClient& enemy) {
            auto context = TargetFacts(enemy, Mode::Automatic);
            context.AutoReachable = false;
            context.SetupReachable = false;
            return context;
        });
    if (!Engine::ValidEnemy(target)) return false;
    // Never spend Rocket Jump from the generic automatic kill-secure loop.
    // R is targeted and does not sacrifice Tristana's position.
    return CastR(target, Mode::Automatic, false, false);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (TryPendingRapidFire()) return true;
    if (CastR(target, mode, false, false)) return true;
    return CastWOnTarget(target, mode);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    EnsureTristanaTargetProvider();
    LastMode = mode;
    RefreshDynamicRanges();
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    if (!combat) {
        ReleaseTristanaFocus();
        PendingRapidFireTargetId = PendingRapidFireUntil = 0;
    }
    if (ManualUltimatePressed()) {
        const auto target = ControllerHelpers::NearestEnemyToPlayer(
            preferred, TargetedRange() + 125.0f);
        if (Engine::ValidEnemy(target) &&
            CastR(target, Mode::Automatic, false, true)) return true;
    }
    if (TryAntiGapcloser()) return true;
    if (TryKillSecure(preferred)) return true;
    if (combat) {
        const auto target = SelectSmartTarget(preferred, mode);
        RefreshChargeFocus(mode, target);
        return TryCombat(target, mode);
    }
    if (mode == Mode::Flee) {
        const auto threat = ControllerHelpers::NearestEnemyToPlayer(
            preferred, 950.0f);
        if (Engine::ValidEnemy(threat) &&
            CastR(threat, Mode::Flee, true, false)) return true;
        return CastWFlee(threat);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return Engine::TryFarm(mode);
    return false;
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender) || args.IsAutoAttack) {
        return;
    }
    const int now = Now();
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        ControllerHelpers::SpellEventNameContainsAny(args, {"tristanaq"})) {
        LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"tristanaw"})) {
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"tristanae"})) {
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"tristanar"})) {
        LastRCastTick = now;
    }
}

inline bool RedirectTristanaFocus(
    SDK::OrbwalkingActionArgs& args,
    const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) ||
        ControllerHelpers::IsCommonUntargetableOrImmune(target) ||
        OrbwalkerAttackProjectileBlocked(target) ||
        !InAutoAttackRange(target)) {
        return false;
    }
    if (!args.Target.IsValid() ||
        args.Target.NetworkId() != target.NetworkId()) {
        args.Target = AttackableUnit(target.Handle());
        args.Position = target.Position();
    }
    return true;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    auto focus = CurrentTristanaFocus(850.0f);
    if (focus.IsValid() &&
        (!HasExplosiveCharge(focus) ||
         !RedirectTristanaFocus(args, focus))) {
        ReleaseTristanaFocus();
        focus = {};
    }
    if (!focus.IsValid() && args.Target.IsValid() && args.Target.IsHero()) {
        focus = AIHeroClient(args.Target.Handle());
    }
    if (!focus.IsValid() ||
        (LastMode != Mode::Combo && LastMode != Mode::Harass)) return;
    if (CastE(focus, LastMode, true)) return;
    (void)CastQ(focus, LastMode);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)ControllerHelpers::CaptureAfterAttack(
        args, LastAfterAttackTargetId, LastAfterAttackTick);
    if (LastAfterAttackTargetId == OwnedFocusTargetId) {
        ReleaseTristanaFocus();
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "TristanaMechanics", "Tristana Mechanics"));
    ChargeMenu = TacticsMenu->AddSubMenu(new Menu(
        "ExplosiveChargeLogic", "Explosive Charge / Orbwalker"));
    ChargeMenu->Add(new MenuSeparator(
        "Focus", "Focus reachable E targets until detonation"));
    JumpMenu = TacticsMenu->AddSubMenu(new Menu(
        "RocketJumpLogic", "Rocket Jump Safety"));
    JumpMenu->Add(new MenuSlider(
        "MaximumEnemies", "Maximum enemies at W landing", 1, 0, 3));
    BusterMenu = TacticsMenu->AddSubMenu(new Menu(
        "BusterShotLogic", "Buster Shot"));
    BusterMenu->Add(new MenuSeparator(
        "Reserve", "R is reserved for lethal detonation or peel"));
}

inline void OnLoad() {
    TristanaProviderToken = 0;
    TristanaProviderService = nullptr;
    EnsureTristanaTargetProvider();
    ReleaseTristanaFocus();
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    PendingRapidFireTargetId = PendingRapidFireUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    LastMode = Mode::None;
    LastSmartTarget = {};
    RefreshDynamicRanges();
}

inline void OnUnload() {
    if (TristanaProviderService && TristanaProviderToken &&
        SDK::KuroTargetSelector::LiveService() == TristanaProviderService) {
        (void)TristanaProviderService->UnregisterProvider(
            TristanaProviderToken);
    }
    TristanaProviderToken = 0;
    TristanaProviderService = nullptr;
    ReleaseTristanaFocus();
    PendingRapidFireTargetId = PendingRapidFireUntil = 0;
    TacticsMenu = ChargeMenu = JumpMenu = BusterMenu = nullptr;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Refresh E/R range from current attack range instead of a fixed level table",
    "Cast E on the real orbwalker target in BeforeAttack",
    "Queue Q immediately after an E BeforeAttack cast",
    "Force a reachable one-to-three-stack E target through the orbwalker",
    "Redirect BeforeAttack to the owned charged target",
    "Release focus after each attack and rebuild only while E remains valuable",
    "Reserve R for lethal damage, E detonation or close self-peel",
    "Use R immediately against a committed gapcloser",
    "Reject E, R and forced attacks across a projectile wall",
    "Reject Rocket Jump when a normal attack is better",
    "Reject Rocket Jump into wall, lockdown or excess enemies",
    "Use a safe cursor-side Rocket Jump while fleeing",
    "Clear all forced target state on mode exit and unload",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Tristana;
    controller.ControllerId = "champion.kuroaio.ai.tristana.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AITristana.md";
    controller.ImplementationSummary =
        "Dynamic attack-linked E/R range, BeforeAttack E-Q sequencing, charged "
        "target focus, safe lethal-only W and detonation/peel R policy.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 700, 900>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Tristana
