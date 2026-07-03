#pragma once

#include "../../GameObjects/ObjectManager.h"

namespace SDK::OrbwalkingDetail {

inline constexpr float kLaneClearWaitTime = 2.0f;

inline bool IsValidAttackTarget(const AttackableUnit& target, float range = FLT_MAX) {
    const auto player = GameObjects::Player();
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
    return range >= FLT_MAX * 0.5f || player.Distance(target) <= range;
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
            if (std::fabs(left.Health() - right.Health()) > FLT_EPSILON) {
                return left.Health() < right.Health();
            }
            return left.MaxHealth() > right.MaxHealth();
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

inline std::vector<AIMinionClient> GetMinionsForMode(OrbwalkingMode mode, const OrbwalkerMenu& menu) {
    std::vector<AIMinionClient> result;
    if (mode == OrbwalkingMode::None) {
        return result;
    }

    const bool includeLaneAndJungle = mode != OrbwalkingMode::Combo;
    std::vector<AIMinionClient> laneMinions;
    std::vector<AIMinionClient> jungleMinions;
    std::vector<AIMinionClient> wardMinions;
    std::vector<AIMinionClient> specialMinions;
    std::vector<AIMinionClient> cloneMinions;

    if (includeLaneAndJungle) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (IsValidMinionTarget(minion) && !IsGangplankBarrel(minion)) {
                AddUniqueMinion(laneMinions, minion);
            }
        }
        OrderLaneMinions(laneMinions);

        for (const auto& minion : GameObjects::Jungle()) {
            if (IsValidMinionTarget(minion) && !IsGangplankBarrel(minion)) {
                AddUniqueMinion(jungleMinions, minion);
            }
        }
        OrderJungleMinions(jungleMinions, menu.PrioritizeSmallJungle());
    }

    if (menu.AttackWards()) {
        for (const auto& ward : GameObjects::EnemyWards()) {
            if (IsValidMinionTarget(ward)) {
                AddUniqueMinion(wardMinions, ward);
            }
        }
    }

    if (menu.AttackSpecialMinions()) {
        for (const auto& minion : GameObjects::EnemySpecialMinions()) {
            if (IsValidMinionTarget(minion)) {
                AddUniqueMinion(specialMinions, minion);
            }
        }
    }

    if (menu.AttackClones()) {
        for (const auto& clone : GameObjects::EnemyClones()) {
            if (IsValidMinionTarget(clone)) {
                AddUniqueMinion(cloneMinions, clone);
            }
        }
    }

    auto append = [&result](const std::vector<AIMinionClient>& values) {
        for (const auto& minion : values) {
            AddUniqueMinion(result, minion);
        }
    };

    std::vector<AIMinionClient> ordinaryMinions;
    for (const auto& minion : laneMinions) {
        AddUniqueMinion(ordinaryMinions, minion);
    }
    for (const auto& minion : jungleMinions) {
        AddUniqueMinion(ordinaryMinions, minion);
    }

    if (menu.AttackWards() && menu.PrioritizeWards() &&
        menu.AttackSpecialMinions() && menu.PrioritizeSpecialMinions()) {
        append(wardMinions);
        append(specialMinions);
        append(ordinaryMinions);
    } else if (menu.AttackSpecialMinions() && menu.PrioritizeSpecialMinions()) {
        append(specialMinions);
        append(ordinaryMinions);
        append(wardMinions);
    } else if (menu.AttackWards() && menu.PrioritizeWards()) {
        append(wardMinions);
        append(ordinaryMinions);
        append(specialMinions);
    } else {
        append(ordinaryMinions);
        append(specialMinions);
        append(wardMinions);
    }

    if (menu.AttackBarrels()) {
        for (const auto& minion : GameObjects::Get<AIMinionClient>()) {
            if (IsGangplankBarrel(minion) &&
                minion.Health() <= 1.0f &&
                IsValidAttackTarget(minion, Utils::AutoAttack::GetRealAutoAttackRange(minion))) {
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
    std::vector<AIMinionClient> candidates = minions;
    std::stable_sort(
        candidates.begin(),
        candidates.end(),
        [](const AIMinionClient& left, const AIMinionClient& right) {
            return left.Health() < right.Health();
        });

    for (const auto& minion : candidates) {
        if (CanLastHitMinion(player, minion, farmDelay)) {
            return minion;
        }
    }
    return {};
}

inline AttackableUnit GetHeroTarget(const AIHeroClient& player) {
    if (auto* selector = TargetSelector::Instance()) {
        const auto hero = selector->GetTarget(-1.0f, DamageType::Physical);
        const AttackableUnit target(hero.Handle());
        if (IsValidAttackTarget(target, Utils::AutoAttack::GetRealAutoAttackRange(target))) {
            return target;
        }
    }

    AttackableUnit best;
    float bestDistance = FLT_MAX;
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        const AttackableUnit target(hero.Handle());
        if (!IsValidAttackTarget(target, Utils::AutoAttack::GetRealAutoAttackRange(target))) {
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
            !IsValidMinionTarget(minion, Utils::AutoAttack::GetRealAutoAttackRange(minion))) {
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

    const auto minions = OrbwalkingDetail::GetMinionsForMode(mode, menu_);
    const bool farmMode =
        mode == OrbwalkingMode::LaneClear ||
        mode == OrbwalkingMode::Hybrid ||
        mode == OrbwalkingMode::Harass ||
        mode == OrbwalkingMode::LastHit;

    if (farmMode) {
        const AttackableUnit killableMinion =
            OrbwalkingDetail::GetKillableMinion(player, minions, menu_.DelayFarm());
        if (killableMinion.IsValid()) {
            return killableMinion;
        }
    }

    if (context_.forceTarget.IsValid() &&
        OrbwalkingDetail::IsValidAttackTarget(context_.forceTarget, GetAutoAttackRange(context_.forceTarget))) {
        return context_.forceTarget;
    }

    if (mode == OrbwalkingMode::LaneClear &&
        (!menu_.PrioritizeMinions() || minions.empty())) {
        for (const auto& turret : GameObjects::EnemyTurrets()) {
            const AttackableUnit target(turret.Handle());
            if (OrbwalkingDetail::IsValidAttackTarget(target, GetAutoAttackRange(target))) {
                return target;
            }
        }
        for (const auto& inhibitor : GameObjects::EnemyInhibitors()) {
            const AttackableUnit target(inhibitor.Handle());
            if (OrbwalkingDetail::IsValidAttackTarget(target, GetAutoAttackRange(target))) {
                return target;
            }
        }
        const auto nexus = GameObjects::EnemyNexus();
        const AttackableUnit nexusTarget(nexus.Handle());
        if (OrbwalkingDetail::IsValidAttackTarget(nexusTarget, GetAutoAttackRange(nexusTarget))) {
            return nexusTarget;
        }
    }

    if (mode != OrbwalkingMode::LastHit) {
        const AttackableUnit target = OrbwalkingDetail::GetHeroTarget(player);
        if (target.IsValid()) {
            return target;
        }
    }

    if (mode == OrbwalkingMode::LaneClear ||
        mode == OrbwalkingMode::Hybrid ||
        mode == OrbwalkingMode::Harass) {
        for (const auto& minion : minions) {
            if (minion.Team() == GameObjectTeam::Neutral) {
                return minion;
            }
        }
    }

    if (farmMode) {
        std::vector<AIMinionClient> turretMinions;
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

            if (turretTarget.IsValid() &&
                OrbwalkingDetail::HasSoonKillableMinion(player, turretTarget, menu_.DelayFarm(), waitTime)) {
                return {};
            }

            for (const auto& minion : turretMinions) {
                if (HealthPrediction::HasMinionAggro(minion)) {
                    continue;
                }

                const float playerDamage = Damage::GetAutoAttackDamage(player, minion);
                if (playerDamage <= 0.0f) {
                    continue;
                }

                for (const auto& turret : GameObjects::AllyTurrets()) {
                    if (!turret.IsValid() || turret.IsDead() || turret.Distance(minion) > 950.0f) {
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
            if (!OrbwalkingDetail::IsValidMinionTarget(minion, GetAutoAttackRange(minion)) ||
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
            return predictedHealth >= 2.0f * damage ||
                   std::fabs(predictedHealth - minion.Health()) < FLT_EPSILON;
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

    if (mode == OrbwalkingMode::Combo && !minions.empty()) {
        const bool enemyNear = std::any_of(
            GameObjects::EnemyHeroes().begin(),
            GameObjects::EnemyHeroes().end(),
            [&](const AIHeroClient& enemy) {
                return enemy.IsValid() &&
                       !enemy.IsDead() &&
                       OrbwalkingDetail::IsValidAttackTarget(
                           enemy,
                           Utils::AutoAttack::GetRealAutoAttackRange(enemy) * 2.0f);
            });
        if (!enemyNear) {
            return minions.front();
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

inline float OrbwalkerBase::GetAutoAttackRange(const AttackableUnit& target) const {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        return 0.0f;
    }

    float range = player.AttackRange() + player.BoundingRadius();
    if (target.IsValid()) {
        range += target.BoundingRadius();
    }
    return std::max(0.0f, range);
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
        return ObjectManager::GetUnitByNetworkId<AttackableUnit>(context_.pendingAttackTargetNetworkId);
    }
    return {};
}

} // namespace SDK
