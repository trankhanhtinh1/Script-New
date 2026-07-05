#pragma once

#include "../../GameObjects/ObjectManager.h"

namespace SDK::OrbwalkingDetail {

inline constexpr float kLaneClearWaitTime = 1.4f;

inline bool IsValidAttackTarget(const AIHeroClient& player,
                                const AttackableUnit& target,
                                float range = FLT_MAX) {
    if (!player.IsValid() || !target.IsValid()) {
        return false;
    }
    if ((!target.IsEnemy() && target.Team() != GameObjectTeam::Neutral) ||
        (!target.IsZombie() && target.IsDead())) {
        return false;
    }
    if (!target.IsVisible() || !target.IsTargetable() || target.IsInvulnerable()) {
        return false;
    }
    const Vector3 origin = player.ServerPosition().IsZero()
        ? player.Position()
        : player.ServerPosition();
    return range >= FLT_MAX * 0.5f ||
           origin.DistanceSqr2D(target.Position()) <= range * range;
}

inline bool IsValidAttackTarget(const AttackableUnit& target, float range = FLT_MAX) {
    return IsValidAttackTarget(GameObjects::Player(), target, range);
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

inline std::vector<AIMinionClient> GetMinionsForMode(OrbwalkingMode mode,
                                                     const OrbwalkerMenu& menu,
                                                     const AIHeroClient& player) {
    std::vector<AIMinionClient> result;
    if (mode == OrbwalkingMode::None || !player.IsValid()) {
        return result;
    }

    const bool includeLaneAndJungleAndWard = mode != OrbwalkingMode::Combo;
    std::vector<AIMinionClient> laneMinions;
    std::vector<AIMinionClient> jungleMinions;
    std::vector<AIMinionClient> wardMinions;
    std::vector<AIMinionClient> specialMinions;
    std::vector<AIMinionClient> cloneMinions;

    auto isInAttackRange = [&player](const AIMinionClient& minion) {
        return Utils::AutoAttack::GetRealAutoAttackRange(player, minion);
    };

    if (includeLaneAndJungleAndWard) {
        const auto& enemyMinions = GameObjects::EnemyMinions();
        laneMinions.reserve(enemyMinions.size());
        for (const auto& minion : enemyMinions) {
            if (IsValidMinionTarget(player, minion, isInAttackRange(minion)) &&
                !IsGangplankBarrel(minion)) {
                AddUniqueMinion(laneMinions, minion);
            }
        }
        OrderLaneMinions(laneMinions);

        const auto& jungle = GameObjects::Jungle();
        jungleMinions.reserve(jungle.size());
        for (const auto& minion : jungle) {
            if (IsValidMinionTarget(player, minion, isInAttackRange(minion)) &&
                !IsGangplankBarrel(minion)) {
                AddUniqueMinion(jungleMinions, minion);
            }
        }
        OrderJungleMinions(jungleMinions, menu.PrioritizeSmallJungle());

        if (menu.AttackWards()) {
            const auto& wards = GameObjects::EnemyWards();
            wardMinions.reserve(wards.size());
            for (const auto& ward : wards) {
                if (IsValidMinionTarget(player, ward, isInAttackRange(ward))) {
                    AddUniqueMinion(wardMinions, ward);
                }
            }
        }
    }

    if (menu.AttackSpecialMinions()) {
        const auto& specials = GameObjects::EnemySpecialMinions();
        specialMinions.reserve(specials.size());
        for (const auto& minion : specials) {
            if (IsValidMinionTarget(player, minion, isInAttackRange(minion))) {
                AddUniqueMinion(specialMinions, minion);
            }
        }
    }

    if (menu.AttackClones()) {
        const auto& clones = GameObjects::EnemyClones();
        cloneMinions.reserve(clones.size());
        for (const auto& clone : clones) {
            if (IsValidMinionTarget(player, clone, isInAttackRange(clone))) {
                AddUniqueMinion(cloneMinions, clone);
            }
        }
    }

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
                IsValidAttackTarget(player, minion, isInAttackRange(minion))) {
                AddUniqueMinion(result, minion);
            }
        }
    }

    append(cloneMinions);
    return result;
}

inline bool CanLastHitMinion(const AIHeroClient& player,
                             const AIMinionClient& minion,
                             int farmDelay) {
    const float damage = Damage::GetAutoAttackDamage(player, minion);
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

    return predictedHealth > 0.0f && predictedHealth <= damage;
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
        const auto hero = selector->GetTarget(-1.0f, DamageType::True);
        const AttackableUnit target(hero.Handle());
        if (IsValidAttackTarget(player, target, Utils::AutoAttack::GetRealAutoAttackRange(player, target))) {
            return target;
        }
    }

    AttackableUnit best;
    float bestDistance = FLT_MAX;
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        const AttackableUnit target(hero.Handle());
        if (!IsValidAttackTarget(player, target, Utils::AutoAttack::GetRealAutoAttackRange(player, target))) {
            continue;
        }

        const float distance = player.Distance(target);
        if (distance < bestDistance) {
            best = target;
            bestDistance = distance;
        }
    }
    return best;
}

inline bool HasSoonKillableMinion(const AIHeroClient& player,
                                  const AIMinionClient& skip,
                                  int farmDelay,
                                  int predictionTime) {
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if ((skip.IsValid() && skip.Compare(minion)) ||
            !IsValidMinionTarget(player, minion, Utils::AutoAttack::GetRealAutoAttackRange(player, minion))) {
            continue;
        }

        const float damage = Damage::GetAutoAttackDamage(player, minion);
        if (damage <= 0.0f) {
            continue;
        }

        const float predictedHealth = HealthPrediction::GetPrediction(
            minion,
            predictionTime,
            farmDelay,
            HealthPredictionType::Simulated);
        if (predictedHealth > 0.0f && predictedHealth < damage) {
            return true;
        }
    }
    return false;
}

} // namespace SDK::OrbwalkingDetail

namespace SDK {

inline AttackableUnit OrbwalkerBase::GetTarget() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead() || !menu_.Enabled()) {
        return {};
    }

    const OrbwalkingMode mode = context_.activeMode != OrbwalkingMode::None
        ? context_.activeMode
        : ActiveMode();
    if (mode == OrbwalkingMode::None) {
        return {};
    }

    SnapshotAttackTimings(player);

    if ((mode == OrbwalkingMode::Hybrid || mode == OrbwalkingMode::LaneClear) &&
        !menu_.PrioritizeFarm()) {
        const AttackableUnit target = OrbwalkingDetail::GetHeroTarget(player);
        if (target.IsValid()) {
            return target;
        }
    }

    const bool farmMode =
        mode == OrbwalkingMode::LaneClear ||
        mode == OrbwalkingMode::Hybrid ||
        mode == OrbwalkingMode::Harass ||
        mode == OrbwalkingMode::LastHit;

    if (mode == OrbwalkingMode::Combo) {
        if (context_.forceTarget.IsValid() &&
            OrbwalkingDetail::IsValidAttackTarget(
                player,
                context_.forceTarget,
                Utils::AutoAttack::GetRealAutoAttackRange(player, context_.forceTarget))) {
            return context_.forceTarget;
        }

        const AttackableUnit target = OrbwalkingDetail::GetHeroTarget(player);
        if (target.IsValid()) {
            return target;
        }

        const auto comboMinions = OrbwalkingDetail::GetMinionsForMode(mode, menu_, player);
        if (!comboMinions.empty()) {
            const auto& enemies = GameObjects::EnemyHeroes();
            const bool enemyNear = std::any_of(
                enemies.begin(),
                enemies.end(),
                [&](const AIHeroClient& enemy) {
                    if (!enemy.IsValid() || enemy.IsDead()) {
                        return false;
                    }
                    const AttackableUnit enemyTarget(enemy.Handle());
                    return OrbwalkingDetail::IsValidAttackTarget(
                        player,
                        enemyTarget,
                        Utils::AutoAttack::GetRealAutoAttackRange(player, enemyTarget) * 2.0f);
                });
            if (!enemyNear) {
                return comboMinions.front();
            }
        }
        return {};
    }

    const auto minions = OrbwalkingDetail::GetMinionsForMode(mode, menu_, player);

    if (farmMode) {
        const AttackableUnit killableMinion =
            OrbwalkingDetail::GetKillableMinion(player, minions, menu_.DelayFarm());

        if (killableMinion.IsValid()) {
            return killableMinion;
        }
    }

    if (context_.forceTarget.IsValid() &&
        OrbwalkingDetail::IsValidAttackTarget(
            player,
            context_.forceTarget,
            Utils::AutoAttack::GetRealAutoAttackRange(player, context_.forceTarget))) {
        return context_.forceTarget;
    }

    if (mode == OrbwalkingMode::LaneClear &&
        (!menu_.PrioritizeMinions() || minions.empty())) {
        for (const auto& turret : GameObjects::EnemyTurrets()) {
            const AttackableUnit target(turret.Handle());
            if (OrbwalkingDetail::IsValidAttackTarget(
                    player,
                    target,
                    Utils::AutoAttack::GetRealAutoAttackRange(player, target))) {
                return target;
            }
        }
        for (const auto& inhibitor : GameObjects::EnemyInhibitors()) {
            const AttackableUnit target(inhibitor.Handle());
            if (OrbwalkingDetail::IsValidAttackTarget(
                    player,
                    target,
                    Utils::AutoAttack::GetRealAutoAttackRange(player, target))) {
                return target;
            }
        }
        const auto nexus = GameObjects::EnemyNexus();
        const AttackableUnit nexusTarget(nexus.Handle());
        if (OrbwalkingDetail::IsValidAttackTarget(
                player,
                nexusTarget,
                Utils::AutoAttack::GetRealAutoAttackRange(player, nexusTarget))) {
            return nexusTarget;
        }
    }

    if (mode != OrbwalkingMode::LastHit) {
        const AttackableUnit target = OrbwalkingDetail::GetHeroTarget(player);
        if (target.IsValid()) {
            return target;
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
            const int waitTime = static_cast<int>(
                context_.attackDelayMs + std::max(0.0f, Utils::AutoAttack::GetTimeToHit(turretMinions.front())));
            AIMinionClient turretTarget;
            for (const auto& minion : turretMinions) {
                if (HealthPrediction::HasTurretAggro(minion)) {
                    turretTarget = minion;
                    break;
                }
            }

            const auto& allyTurrets = GameObjects::AllyTurrets();
            for (const auto& minion : turretMinions) {
                if (HealthPrediction::HasMinionAggro(minion)) {
                    continue;
                }

                const float playerDamage = Damage::GetAutoAttackDamage(player, minion);
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
                        return minion;
                    }
                }
            }
        }
    }

    if (mode == OrbwalkingMode::LaneClear && !ShouldWait()) {
        auto canLaneClear = [&](const AIMinionClient& minion) {
            if (!OrbwalkingDetail::IsValidMinionTarget(
                    player,
                    minion,
                    Utils::AutoAttack::GetRealAutoAttackRange(player, minion)) ||
                minion.Team() == GameObjectTeam::Neutral) {
                return false;
            }
            if (minion.MaxHealth() <= 10.0f) {
                return true;
            }

            const float damage = Damage::GetAutoAttackDamage(player, minion);
            if (damage <= 0.0f) {
                return false;
            }
            const int predictionTime = static_cast<int>(
                context_.attackDelayMs * OrbwalkingDetail::kLaneClearWaitTime);
            const float predictedHealth = HealthPrediction::GetPrediction(
                minion,
                predictionTime,
                menu_.DelayFarm(),
                HealthPredictionType::Simulated);

            return predictedHealth >= 0 || std::fabs(predictedHealth - minion.Health()) < FLT_EPSILON;
        };

        const AIMinionClient currentLaneClearMinion(context_.laneClearMinion.Handle());
        if (currentLaneClearMinion.IsValid() && canLaneClear(currentLaneClearMinion)) {
            return currentLaneClearMinion;
        }

        for (const auto& minion : minions) {
            if (canLaneClear(minion)) {
                context_.laneClearMinion = minion;
                return minion;
            }
        }
    }

    return {};
}

inline bool OrbwalkerBase::ShouldWait() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return false;
    }

    SnapshotAttackTimings(player);
    const int predictionTime = static_cast<int>(
        context_.attackDelayMs * OrbwalkingDetail::kLaneClearWaitTime);
    return OrbwalkingDetail::HasSoonKillableMinion(
        player,
        {},
        menu_.DelayFarm(),
        predictionTime);
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

} // namespace SDK
