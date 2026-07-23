#pragma once

#include "../../../sdk/GameObjects/ObjectManager.h"

using namespace ::SDK;

namespace OrbwalkerKuro::OrbwalkingDetail {

inline constexpr float kLaneClearWaitTime = 1.7f;

inline bool IsValidAttackTarget(const AIHeroClient& player,
                                const AttackableUnit& target,
                                float range = FLT_MAX) {
    if (!player.IsValid() || !target.IsValid()) {
        return false;
    }
    if ((!target.IsEnemy() && target.Team() != GameObjectTeam::Neutral) || (!target.IsZombie() && target.IsDead())) {
        return false;
    }
    if (!target.IsVisible() || !target.IsTargetable() || target.IsInvulnerable()) {
        return false;
    }
    const Vector3 origin = player.Position();
    return range >= FLT_MAX * 0.5f ||
           origin.DistanceSqr2D(target.Position()) <= range * range;
}

inline bool IsValidAttackTarget(const AttackableUnit& target, float range = FLT_MAX) {
    return IsValidAttackTarget(GameObjects::Player(), target, range);
}

inline bool IsValidCurrentAttackTarget(const AIHeroClient& player,
                                       const AttackableUnit& target) {
    return IsValidAttackTarget(player, target) &&
           IsTargetWithinCurrentAttackRange(player, target);
}

inline bool IsValidCurrentAttackTarget(const AttackableUnit& target) {
    return IsValidCurrentAttackTarget(GameObjects::Player(), target);
}

inline bool IsGangplankBarrel(const AIMinionClient& minion) {
    return _stricmp(minion.CharacterName().c_str(), "gangplankbarrel") == 0;
}

inline bool IsIgnoredMinion(const AIMinionClient& minion) {
    return _stricmp(minion.CharacterName().c_str(), "jarvanivstandard") == 0;
}

inline bool IsValidMinionTarget(const AIMinionClient& minion, float range = FLT_MAX) {
    return !minion.IsPlant() &&
           !IsIgnoredMinion(minion) &&
           IsValidAttackTarget(minion, range);
}

inline bool IsValidMinionTarget(const AIHeroClient& player,
                                const AIMinionClient& minion,
                                float range = FLT_MAX) {
    return !minion.IsPlant() &&
           !IsIgnoredMinion(minion) &&
           IsValidAttackTarget(player, minion, range);
}

inline bool IsValidCurrentMinionTarget(const AIHeroClient& player,
                                       const AIMinionClient& minion) {
    return !minion.IsPlant() &&
           !IsIgnoredMinion(minion) &&
           IsValidCurrentAttackTarget(
               player, AttackableUnit(minion.Handle()));
}

inline bool IsSiegeMinion(const AIMinionClient& minion) {
    return HasFlag(minion.GetMinionType(), MinionTypes::Siege);
}

inline bool IsSuperMinion(const AIMinionClient& minion) {
    return HasFlag(minion.GetMinionType(), MinionTypes::Super);
}

inline bool HasMinion(const std::vector<AIMinionClient>& minions, const AIMinionClient& minion) {
    return std::any_of(minions.begin(), minions.end(), [&](const AIMinionClient& existing) {
        return existing.Compare(minion) ||
               (existing.NetworkId() != 0 && existing.NetworkId() == minion.NetworkId());
    });
}

inline void AddUniqueMinion(std::vector<AIMinionClient>& minions, const AIMinionClient& minion) {
    if (!HasMinion(minions, minion)) {
        minions.push_back(minion);
    }
}

inline void OrderLaneMinions(std::vector<AIMinionClient>& minions) {
    std::stable_sort(
        minions.begin(),
        minions.end(),
        [](const AIMinionClient& left, const AIMinionClient& right) {
            if (IsSiegeMinion(left) != IsSiegeMinion(right)) {
                return IsSiegeMinion(left);
            }
            if (IsSuperMinion(left) != IsSuperMinion(right)) {
                return !IsSuperMinion(left);
            }
            return left.Health() < right.Health();
        });
}

inline void OrderJungleMinions(std::vector<AIMinionClient>& minions, bool prioritizeSmallJungle) {
    std::stable_sort(
        minions.begin(),
        minions.end(),
        [prioritizeSmallJungle](const AIMinionClient& left, const AIMinionClient& right) {
            return prioritizeSmallJungle
                ? left.MaxHealth() < right.MaxHealth()
                : left.MaxHealth() > right.MaxHealth();
        });
}

struct MinionTargetLists {
    std::vector<AIMinionClient> targets;
    std::vector<AIMinionClient> laneMinions;
};

inline MinionTargetLists GetMinionsForMode(OrbwalkingMode mode,
                                           const OrbwalkerMenu& menu,
                                           const AIHeroClient& player) {
    MinionTargetLists lists;
    auto& result = lists.targets;
    auto& laneMinions = lists.laneMinions;
    if (mode == OrbwalkingMode::None || !player.IsValid()) {
        return lists;
    }

    const bool includeLaneAndJungleAndWard = mode != OrbwalkingMode::Combo;
    std::vector<AIMinionClient> jungleMinions;
    std::vector<AIMinionClient> wardMinions;
    std::vector<AIMinionClient> specialMinions;
    std::vector<AIMinionClient> cloneMinions;

    if (includeLaneAndJungleAndWard) {
        const auto& enemyMinions = GameObjects::EnemyMinions();
        laneMinions.reserve(enemyMinions.size());
        for (const auto& minion : enemyMinions) {
            if (IsValidCurrentMinionTarget(player, minion) &&
                !IsGangplankBarrel(minion)) {
                AddUniqueMinion(laneMinions, minion);
            }
        }
        OrderLaneMinions(laneMinions);

        const auto& jungle = GameObjects::Jungle();
        jungleMinions.reserve(jungle.size());
        for (const auto& minion : jungle) {
            if (IsValidCurrentMinionTarget(player, minion) &&
                !IsGangplankBarrel(minion)) {
                AddUniqueMinion(jungleMinions, minion);
            }
        }
        OrderJungleMinions(jungleMinions, menu.PrioritizeSmallJungle());

        if (menu.AttackWards()) {
            const auto& wards = GameObjects::EnemyWards();
            wardMinions.reserve(wards.size());
            for (const auto& ward : wards) {
                if (IsValidCurrentMinionTarget(player, ward)) {
                    AddUniqueMinion(wardMinions, ward);
                }
            }
        }
    }

    if (menu.AttackSpecialMinions()) {
        const auto& specials = GameObjects::EnemySpecialMinions();
        specialMinions.reserve(specials.size());
        for (const auto& minion : specials) {
            if (IsValidCurrentMinionTarget(player, minion)) {
                AddUniqueMinion(specialMinions, minion);
            }
        }
    }

    if (menu.AttackClones()) {
        const auto& clones = GameObjects::EnemyClones();
        cloneMinions.reserve(clones.size());
        for (const auto& clone : clones) {
            if (IsValidCurrentMinionTarget(player, clone)) {
                AddUniqueMinion(cloneMinions, clone);
            }
        }
    }

    result.reserve(
        laneMinions.size() +
        jungleMinions.size() +
        wardMinions.size() +
        specialMinions.size() +
        cloneMinions.size());

    auto append = [&result](const std::vector<AIMinionClient>& values) {
        for (const auto& minion : values) {
            AddUniqueMinion(result, minion);
        }
    };

    auto appendOrdinary = [&]() {
        append(laneMinions);
        append(jungleMinions);
    };

    if (menu.AttackWards() && menu.PrioritizeWards() &&
        menu.AttackSpecialMinions() && menu.PrioritizeSpecialMinions()) {
        append(wardMinions);
        append(specialMinions);
        appendOrdinary();
    } else if (menu.AttackSpecialMinions() && menu.PrioritizeSpecialMinions()) {
        append(specialMinions);
        appendOrdinary();
        append(wardMinions);
    } else if (menu.AttackWards() && menu.PrioritizeWards()) {
        append(wardMinions);
        appendOrdinary();
        append(specialMinions);
    } else {
        appendOrdinary();
        append(specialMinions);
        append(wardMinions);
    }

    if (menu.AttackBarrels()) {
        for (const auto& minion : GameObjects::Get<AIMinionClient>()) {
            if (IsGangplankBarrel(minion) &&
                minion.Health() <= 1.0f &&
                IsValidCurrentAttackTarget(
                    player, AttackableUnit(minion.Handle()))) {
                AddUniqueMinion(result, minion);
            }
        }
    }

    append(cloneMinions);
    return lists;
}

inline bool CanLastHitMinion(const AIHeroClient& player,
                             const AIMinionClient& minion,
                             int farmDelay) {
    const float damage = GetCurrentAutoAttackDamage(player, minion);
    if (damage <= 0.0f) {
        return false;
    }
    if (minion.MaxHealth() <= 10.0f) {
        return minion.Health() <= 1.0f || minion.Health() <= damage;
    }
    if (minion.Health() <= damage) {
        return true;
    }

    const int timeToHit = static_cast<int>(
        std::max(0.0f, Utils::AutoAttack::GetTimeToHit(minion)));
    const float predictedHealth = HealthPrediction::GetPrediction(
        minion,
        timeToHit,
        farmDelay);

    return predictedHealth <= damage;
}

inline AttackableUnit GetKillableMinion(const AIHeroClient& player,
                                        const std::vector<AIMinionClient>& minions,
                                        int farmDelay) {
    for (const auto& minion : minions) {
        if (CanLastHitMinion(player, minion, farmDelay)) {
            return minion;
        }
    }
    return {};
}

inline AttackableUnit GetHeroTarget(const AIHeroClient& player) {
    if (auto* selector = TargetSelector::Instance()) {
        const auto targets = selector->GetTargets(FLT_MAX, DamageType::True);
        for (const auto& hero : targets) {
            const AttackableUnit target(hero.Handle());
            if (IsValidCurrentAttackTarget(player, target)) {
                return target;
            }
        }
    }

    for (const auto& hero : GameObjects::EnemyHeroes()) {
        const AttackableUnit target(hero.Handle());
        if (IsValidCurrentAttackTarget(player, target)) {
            return target;
        }
    }
    return {};
}

inline bool HasEnemyHeroNearAutoAttackRange(const AIHeroClient& player) {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.IsDead()) {
            continue;
        }

        const AttackableUnit target(enemy.Handle());
        if (IsValidAttackTarget(player, target) &&
            IsTargetWithinCurrentAttackRange(player, target, 2.0f)) {
            return true;
        }
    }
    return false;
}

inline AttackableUnit FirstValidMinionTarget(const AIHeroClient& player,
                                             const std::vector<AIMinionClient>& minions) {
    for (const auto& minion : minions) {
        if (IsValidCurrentMinionTarget(player, minion)) {
            return AttackableUnit(minion.Handle());
        }
    }
    return {};
}

inline AttackableUnit GetComboFallbackCandidate(const OrbwalkerMenu& menu,
                                                const AIHeroClient& player) {
    if (menu.AttackSpecialMinions()) {
        const AttackableUnit special =
            FirstValidMinionTarget(player, GameObjects::EnemySpecialMinions());
        if (special.IsValid()) {
            return special;
        }
    }

    if (menu.AttackBarrels()) {
        for (const auto& minion : GameObjects::Get<AIMinionClient>()) {
            if (IsGangplankBarrel(minion) &&
                minion.Health() <= 1.0f &&
                IsValidCurrentAttackTarget(
                    player, AttackableUnit(minion.Handle()))) {
                return AttackableUnit(minion.Handle());
            }
        }
    }

    if (menu.AttackClones()) {
        return FirstValidMinionTarget(player, GameObjects::EnemyClones());
    }

    return {};
}

inline AttackableUnit GetComboFallbackTarget(const OrbwalkerMenu& menu,
                                             const AIHeroClient& player) {
    const AttackableUnit candidate = GetComboFallbackCandidate(menu, player);
    if (!candidate.IsValid()) {
        return {};
    }

    // Keep combo DPS for champions; fallback minions are only for quiet moments.
    return HasEnemyHeroNearAutoAttackRange(player) ? AttackableUnit() : candidate;
}

inline bool IsSoonKillableMinion(const AIHeroClient& player,
                                 const AIMinionClient& minion,
                                 int farmDelay,
                                 int predictionTime) {
    if (!IsValidCurrentMinionTarget(player, minion)) {
        return false;
    }

    const float damage = GetCurrentAutoAttackDamage(player, minion);
    if (damage <= 0.0f) {
        return false;
    }

    const float predictedHealth = HealthPrediction::GetPrediction(
        minion,
        predictionTime,
        farmDelay,
        HealthPredictionType::Simulated);
    return predictedHealth < damage;
}

inline bool HasSoonKillableMinion(const AIHeroClient& player,
                                  const std::vector<AIMinionClient>& minions,
                                  const AIMinionClient& skip,
                                  int farmDelay,
                                  int predictionTime) {
    for (const auto& minion : minions) {
        if ((skip.IsValid() && skip.Compare(minion)) ||
            IsGangplankBarrel(minion) ||
            !IsSoonKillableMinion(player, minion, farmDelay, predictionTime)) {
            continue;
        }
        return true;
    }
    return false;
}

inline bool HasSoonKillableMinion(const AIHeroClient& player,
                                  const AIMinionClient& skip,
                                  int farmDelay,
                                  int predictionTime) {
    return HasSoonKillableMinion(
        player,
        GameObjects::EnemyMinions(),
        skip,
        farmDelay,
        predictionTime);
}

} // namespace OrbwalkerKuro::OrbwalkingDetail

namespace OrbwalkerKuro {

inline AttackableUnit OrbwalkerBase::GetTarget() {
    const int now = Tick();
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead() || !menu_.Enabled()) {
        context_.cachedTargetTick = -1;
        context_.cachedShouldWaitTick = -1;
        return {};
    }

    const OrbwalkingMode mode = context_.activeMode != OrbwalkingMode::None
        ? context_.activeMode
        : ActiveMode();
    if (mode == OrbwalkingMode::None) {
        context_.cachedTargetTick = -1;
        context_.cachedShouldWaitTick = -1;
        return {};
    }

    const int forceTargetNetworkId = context_.forceTarget.IsValid()
        ? context_.forceTarget.NetworkId()
        : 0;
    constexpr int kTargetSelectionThrottleMs = 35;
    if (context_.cachedTargetTick > 0 &&
        now - context_.cachedTargetTick >= 0 &&
        now - context_.cachedTargetTick < kTargetSelectionThrottleMs &&
        context_.cachedTargetMode == mode &&
        context_.cachedTargetForceTargetNetworkId == forceTargetNetworkId) {
        if (!context_.cachedTarget.IsValid() ||
            OrbwalkingDetail::IsValidCurrentAttackTarget(player, context_.cachedTarget)) {
            return context_.cachedTarget;
        }
    }

    auto cacheTarget = [&](const AttackableUnit& target) -> AttackableUnit {
        context_.cachedTarget = target;
        context_.cachedTargetTick = now;
        context_.cachedTargetMode = mode;
        context_.cachedTargetForceTargetNetworkId = forceTargetNetworkId;
        return target;
    };

    ReadAttackTimingsFromMemory(player);

    AttackableUnit cachedHeroTarget;
    bool heroTargetResolved = false;
    auto getHeroTarget = [&]() -> AttackableUnit {
        if (!heroTargetResolved) {
            cachedHeroTarget = OrbwalkingDetail::GetHeroTarget(player);
            heroTargetResolved = true;
        }
        return cachedHeroTarget;
    };

    if ((mode == OrbwalkingMode::Hybrid || mode == OrbwalkingMode::LaneClear) &&
        !menu_.PrioritizeFarm()) {
        const AttackableUnit target = getHeroTarget();
        if (target.IsValid()) {
            return cacheTarget(target);
        }
    }

    const bool farmMode =
        mode == OrbwalkingMode::LaneClear ||
        mode == OrbwalkingMode::Hybrid ||
        mode == OrbwalkingMode::Harass ||
        mode == OrbwalkingMode::LastHit;
    const int farmDelay = menu_.DelayFarm();

    if (mode == OrbwalkingMode::Combo) {
        if (context_.forceTarget.IsValid() &&
            OrbwalkingDetail::IsValidCurrentAttackTarget(
                player, context_.forceTarget)) {
            return cacheTarget(context_.forceTarget);
        }

        const AttackableUnit target = getHeroTarget();
        if (target.IsValid()) {
            return cacheTarget(target);
        }

        const AttackableUnit fallbackTarget =
            OrbwalkingDetail::GetComboFallbackTarget(menu_, player);
        if (fallbackTarget.IsValid()) {
            return cacheTarget(fallbackTarget);
        }
        return cacheTarget(AttackableUnit());
    }

    const auto minionLists = OrbwalkingDetail::GetMinionsForMode(mode, menu_, player);
    const auto& minions = minionLists.targets;

    if (farmMode) {
        const AttackableUnit killableMinion =
            OrbwalkingDetail::GetKillableMinion(player, minions, farmDelay);

        if (killableMinion.IsValid()) {
            return cacheTarget(killableMinion);
        }
    }

    if (context_.forceTarget.IsValid() &&
        OrbwalkingDetail::IsValidCurrentAttackTarget(
            player, context_.forceTarget)) {
        return cacheTarget(context_.forceTarget);
    }

    if (mode == OrbwalkingMode::LaneClear &&
        (!menu_.PrioritizeMinions() || minions.empty())) {
        for (const auto& turret : GameObjects::EnemyTurrets()) {
            const AttackableUnit target(turret.Handle());
            if (OrbwalkingDetail::IsValidCurrentAttackTarget(player, target)) {
                return cacheTarget(target);
            }
        }
        for (const auto& inhibitor : GameObjects::EnemyInhibitors()) {
            const AttackableUnit target(inhibitor.Handle());
            if (OrbwalkingDetail::IsValidCurrentAttackTarget(player, target)) {
                return cacheTarget(target);
            }
        }
        const auto nexus = GameObjects::EnemyNexus();
        const AttackableUnit nexusTarget(nexus.Handle());
        if (OrbwalkingDetail::IsValidCurrentAttackTarget(
                player, nexusTarget)) {
            return cacheTarget(nexusTarget);
        }
    }

    if (mode != OrbwalkingMode::LastHit) {
        const AttackableUnit target = getHeroTarget();
        if (target.IsValid()) {
            return cacheTarget(target);
        }
    }

    // Jungle camps are Neutral team, so the LaneClear last-hit loop below
    // deliberately skips them (canLaneClear rejects Neutral). The backup
    // OrbwalkerSelector selected jungle through a dedicated GetJungleTarget()
    // branch for Harass/LaneClear/LastHit; mirror that here. `minions` already
    // has the jungle camps appended in priority order (OrderJungleMinions honors
    // PrioritizeSmallJungle), so the first valid Neutral entry is the pick.
    if (mode == OrbwalkingMode::Harass ||
        mode == OrbwalkingMode::LaneClear ||
        mode == OrbwalkingMode::LastHit) {
        for (const auto& minion : minions) {
            if (minion.Team() != GameObjectTeam::Neutral) {
                continue;
            }
            if (OrbwalkingDetail::IsValidCurrentMinionTarget(player, minion)) {
                return cacheTarget(AttackableUnit(minion.Handle()));
            }
        }
    }

    if (farmMode) {
        std::vector<AIMinionClient> turretMinions;
        turretMinions.reserve(minions.size());
        for (const auto& minion : minions) {
            if (minion.Team() != GameObjectTeam::Neutral &&
                minion.IsMinion() &&
                minion.IsUnderAllyTurret()) {
                turretMinions.push_back(minion);
            }
        }

        if (!turretMinions.empty()) {
            const auto& allyTurrets = GameObjects::AllyTurrets();
            for (const auto& minion : turretMinions) {
                if (HealthPrediction::HasMinionAggro(minion)) {
                    continue;
                }

                const float playerDamage =
                    OrbwalkingDetail::GetCurrentAutoAttackDamage(player, minion);
                if (playerDamage <= 0.0f) {
                    continue;
                }

                for (const auto& turret : allyTurrets) {
                    if (!turret.IsValid() ||
                        turret.IsDead() ||
                        turret.Position().DistanceSqr2D(minion.Position()) > 950.0f * 950.0f) {
                        continue;
                    }
                    const float turretDamage = std::max(1.0f, turret.GetAutoAttackDamage(minion, false));
                    if (std::fmod(std::max(0.0f, minion.Health()), turretDamage) > playerDamage) {
                        return cacheTarget(AttackableUnit(minion.Handle()));
                    }
                }
            }
        }
    }

    if (mode == OrbwalkingMode::LaneClear) {
        const int predictionTime = static_cast<int>(
            context_.attackDelayMs * OrbwalkingDetail::kLaneClearWaitTime);
        const bool shouldWait = OrbwalkingDetail::HasSoonKillableMinion(
            player,
            minionLists.laneMinions,
            {},
            farmDelay,
            predictionTime);
        context_.cachedShouldWait = shouldWait;
        context_.cachedShouldWaitTick = now;
        if (shouldWait) {
            return cacheTarget(AttackableUnit());
        }

        auto canLaneClear = [&](const AIMinionClient& minion) {
            if (!OrbwalkingDetail::IsValidCurrentMinionTarget(player, minion) ||
                minion.Team() == GameObjectTeam::Neutral) {
                return false;
            }
            if (minion.MaxHealth() <= 10.0f) {
                return true;
            }

            const float damage =
                OrbwalkingDetail::GetCurrentAutoAttackDamage(player, minion);
            if (damage <= 0.0f) {
                return false;
            }

            return true;
        };

        const AIMinionClient currentLaneClearMinion(context_.laneClearMinion.Handle());
        if (currentLaneClearMinion.IsValid() && canLaneClear(currentLaneClearMinion)) {
            return cacheTarget(AttackableUnit(currentLaneClearMinion.Handle()));
        }

        for (const auto& minion : minions) {
            if (canLaneClear(minion)) {
                context_.laneClearMinion = minion;
                return cacheTarget(AttackableUnit(minion.Handle()));
            }
        }
    }

    return cacheTarget(AttackableUnit());
}

inline bool OrbwalkerBase::ShouldWait() {
    const int now = Tick();
    if (context_.cachedShouldWaitTick == now) {
        return context_.cachedShouldWait;
    }

    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        context_.cachedShouldWait = false;
        context_.cachedShouldWaitTick = now;
        return false;
    }

    ReadAttackTimingsFromMemory(player);
    const int predictionTime = static_cast<int>(
        context_.attackDelayMs * OrbwalkingDetail::kLaneClearWaitTime);
    context_.cachedShouldWait = OrbwalkingDetail::HasSoonKillableMinion(
        player,
        {},
        menu_.DelayFarm(),
        predictionTime);
    context_.cachedShouldWaitTick = now;
    return context_.cachedShouldWait;
}

inline AttackableUnit OrbwalkerBase::ResolveAttackTarget(const Events::ProcessSpellEventArgs& args) const {
    if (args.Target.IsValid()) {
        return AttackableUnit(args.Target.Ptr);
    }
    if (args.TargetNetworkId != 0 && args.TargetNetworkId != 0xFFFFFFFFu) {
        return ObjectManager::GetUnitByNetworkId<AttackableUnit>(
            static_cast<int>(args.TargetNetworkId));
    }
    if (context_.pendingAttackTargetNetworkId != 0) {
        return ObjectManager::GetUnitByNetworkId<AttackableUnit>(
            context_.pendingAttackTargetNetworkId);
    }
    return {};
}

inline AttackableUnit OrbwalkerBase::ResolveAttackTarget(const Events::ObjectEventArgs& args) const {
    if (args.Target.IsValid()) {
        return AttackableUnit(args.Target.Ptr);
    }
    if (args.TargetNetworkId != 0 && args.TargetNetworkId != 0xFFFFFFFFu) {
        return ObjectManager::GetUnitByNetworkId<AttackableUnit>(
            static_cast<int>(args.TargetNetworkId));
    }
    if (context_.pendingAttackTargetNetworkId != 0) {
        return ObjectManager::GetUnitByNetworkId<AttackableUnit>(
            context_.pendingAttackTargetNetworkId);
    }
    return {};
}

} // namespace OrbwalkerKuro
