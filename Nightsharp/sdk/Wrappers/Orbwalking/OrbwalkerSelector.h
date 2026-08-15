#pragma once

#include "OrbwalkerBase.h"
#include "../TargetSelector/TargetSelector.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace SDK {

class OrbwalkerSelector : public OrbwalkerBase {
public:
    explicit OrbwalkerSelector(Menu* parentMenu)
        : OrbwalkerBase(parentMenu) {}

    bool ShouldWait() override {
        if (!initialized_) {
            return false;
        }
        return ShouldWait(GetMinions(200.0f));
    }

    AttackableUnit GetTarget() override {
        if (!initialized_) {
            FarmDebugLogTargetDecision("blocked:not-initialized", {}, 0, false, true);
            return {};
        }

        const auto player = GameObjects::Player();
        const OrbwalkingMode mode = ActiveMode();
        if (!player.IsValid() || player.IsDead() ||
            mode == OrbwalkingMode::None ||
            mode == OrbwalkingMode::Flee) {
            FarmDebugLogTargetDecision("blocked:player-invalid-dead-mode-none-flee", {}, 0, false, true);
            return {};
        }

        std::vector<AIMinionClient> minions = GetMinions(200.0f);
        FarmDebugLogTargetDecision("scan-complete", {}, minions.size(), false, false);
        const bool fastLaneClear = mode == OrbwalkingMode::LaneClear && IsFastLaneClear();

        FarmDebugBreadcrumb("before-barrel", minions.size());
        AttackableUnit barrel = GetBarrelTarget();
        FarmDebugBreadcrumb("after-barrel", minions.size(), barrel);
        if (barrel.IsValid()) {
            FarmDebugLogTargetDecision("return:barrel", barrel, minions.size(), false, true);
            return barrel;
        }

        if (!fastLaneClear &&
            (mode == OrbwalkingMode::Harass || mode == OrbwalkingMode::LaneClear) &&
            !player.IsUnderEnemyTurret() &&
            !Bool(prioritizeMenu_, "FarmOverHarass", true)) {
            FarmDebugBreadcrumb("before-pre-farm-hero", minions.size());
            AttackableUnit heroTarget = GetChampionTarget();
            FarmDebugBreadcrumb("after-pre-farm-hero", minions.size(), heroTarget);
            if (heroTarget.IsValid()) {
                FarmDebugLogTargetDecision("return:pre-farm-hero", heroTarget, minions.size(), false, true);
                return heroTarget;
            }
        }

        FarmDebugBreadcrumb("before-support-check", minions.size());
        const bool skipFarmForSupport = ShouldSkipFarmForSupportMode();
        FarmDebugBreadcrumb(skipFarmForSupport ? "after-support-check-skip" : "after-support-check-allow", minions.size());
        if (fastLaneClear && !skipFarmForSupport) {
            FarmDebugBreadcrumb("before-fast-lane-clear", minions.size());
            AttackableUnit fastLaneClearTarget = GetFastLaneClearTarget(minions);
            FarmDebugBreadcrumb("after-fast-lane-clear", minions.size(), fastLaneClearTarget);
            if (fastLaneClearTarget.IsValid()) {
                FarmDebugLogTargetDecision("return:fast-lane-clear", fastLaneClearTarget, minions.size(), false, true);
                return fastLaneClearTarget;
            }
        } else if (fastLaneClear) {
            FarmDebugLogTargetDecision("skip:fast-lane-clear-support-mode", {}, minions.size(), false, false);
        }

        if (mode != OrbwalkingMode::Combo && !skipFarmForSupport && !fastLaneClear) {
            FarmDebugBreadcrumb("before-last-hit", minions.size());
            AttackableUnit lastHit = GetLastHitTarget(minions);
            FarmDebugBreadcrumb("after-last-hit", minions.size(), lastHit);
            if (lastHit.IsValid()) {
                FarmDebugLogTargetDecision("return:last-hit", lastHit, minions.size(), false, true);
                return lastHit;
            }
        } else if (mode != OrbwalkingMode::Combo) {
            FarmDebugLogTargetDecision(
                fastLaneClear ? "skip:last-hit-fast-lane-clear" : "skip:last-hit-support-mode",
                {},
                minions.size(),
                false,
                false);
        }

        FarmDebugBreadcrumb("before-force-target", minions.size(), forceTarget_);
        if (forceTarget_.IsValid() &&
            OrbwalkingDetail::IsValidAttackTarget(forceTarget_, GetAutoAttackRange(forceTarget_))) {
            FarmDebugLogTargetDecision("return:force-target", forceTarget_, minions.size(), false, true);
            return forceTarget_;
        }

        if (mode != OrbwalkingMode::Combo &&
            (minions.empty() || Bool(prioritizeMenu_, "Turret", true))) {
            FarmDebugBreadcrumb("before-structure", minions.size());
            AttackableUnit structure = GetStructureTarget();
            FarmDebugBreadcrumb("after-structure", minions.size(), structure);
            if (structure.IsValid()) {
                FarmDebugLogTargetDecision("return:structure", structure, minions.size(), false, true);
                return structure;
            }
        }

        FarmDebugBreadcrumb("before-should-wait", minions.size());
        const bool waitForFarm = mode != OrbwalkingMode::Combo && ShouldWait(minions);
        FarmDebugBreadcrumb(waitForFarm ? "after-should-wait-true" : "after-should-wait-false", minions.size());
        if (waitForFarm) {
            FarmDebugLogTargetDecision("state:should-wait-farm", {}, minions.size(), waitForFarm, true);
        }
        if (mode != OrbwalkingMode::LastHit &&
            (mode != OrbwalkingMode::LaneClear || !waitForFarm)) {
            FarmDebugBreadcrumb("before-hero", minions.size());
            AttackableUnit heroTarget = GetChampionTarget();
            FarmDebugBreadcrumb("after-hero", minions.size(), heroTarget);
            if (heroTarget.IsValid()) {
                FarmDebugLogTargetDecision("return:hero", heroTarget, minions.size(), waitForFarm, true);
                return heroTarget;
            }
        }

        if (mode != OrbwalkingMode::Combo &&
            Bool(prioritizeMenu_, "SpecialMinion", false) &&
            !waitForFarm) {
            FarmDebugBreadcrumb("before-priority-special-minion", minions.size());
            AttackableUnit specialMinion = GetSpecialMinionTarget(mode);
            FarmDebugBreadcrumb("after-priority-special-minion", minions.size(), specialMinion);
            if (specialMinion.IsValid()) {
                FarmDebugLogTargetDecision(
                    "return:priority-special-minion",
                    specialMinion,
                    minions.size(),
                    waitForFarm,
                    true);
                return specialMinion;
            }
        }

        if (mode == OrbwalkingMode::Combo) {
            FarmDebugBreadcrumb("before-combo-jungle-plant", minions.size());
            AttackableUnit plant = GetJunglePlantTarget();
            FarmDebugBreadcrumb("after-combo-jungle-plant", minions.size(), plant);
            if (plant.IsValid()) {
                FarmDebugLogTargetDecision("return:combo-jungle-plant", plant, minions.size(), waitForFarm, true);
                return plant;
            }
        }

        if (mode == OrbwalkingMode::Harass ||
            mode == OrbwalkingMode::LaneClear ||
            mode == OrbwalkingMode::LastHit) {
            FarmDebugBreadcrumb("before-jungle", minions.size());
            AttackableUnit jungle = GetJungleTarget();
            FarmDebugBreadcrumb("after-jungle", minions.size(), jungle);
            if (jungle.IsValid()) {
                FarmDebugLogTargetDecision("return:jungle", jungle, minions.size(), waitForFarm, true);
                return jungle;
            }
        }

        if (mode != OrbwalkingMode::Combo &&
            Bool(farmMenu_, "TurretFarm", true)) {
            FarmDebugBreadcrumb("before-turret-farm", minions.size());
            AttackableUnit turretFarm = GetTurretFarmTarget(minions);
            FarmDebugBreadcrumb("after-turret-farm", minions.size(), turretFarm);
            if (turretFarm.IsValid()) {
                FarmDebugLogTargetDecision("return:turret-farm", turretFarm, minions.size(), waitForFarm, true);
                return turretFarm;
            }
        }

        if (mode == OrbwalkingMode::LaneClear && !waitForFarm) {
            FarmDebugBreadcrumb("before-lane-clear", minions.size());
            AttackableUnit laneClear = GetLaneClearTarget(minions);
            FarmDebugBreadcrumb("after-lane-clear", minions.size(), laneClear);
            if (laneClear.IsValid()) {
                FarmDebugLogTargetDecision("return:lane-clear", laneClear, minions.size(), waitForFarm, true);
                return laneClear;
            }
        }

        // Structures (turret/inhibitor/nexus) must NEVER be auto-attacked in
        // Combo — holding the combo key under an enemy turret should target
        // champions only, not the turret. EnsoulSharp NewOrbwalker only ever
        // targets structures inside its `activeMode != Combo` block; this
        // fallback mirrors that gate so structures are attackable with the farm
        // keys (LaneClear / LastHit / Harass) only.
        if (mode != OrbwalkingMode::Combo && !waitForFarm) {
            FarmDebugBreadcrumb("before-special-minion", minions.size());
            AttackableUnit specialMinion = GetSpecialMinionTarget(mode);
            FarmDebugBreadcrumb("after-special-minion", minions.size(), specialMinion);
            if (specialMinion.IsValid()) {
                FarmDebugLogTargetDecision(
                    "return:special-minion",
                    specialMinion,
                    minions.size(),
                    waitForFarm,
                    true);
                return specialMinion;
            }
        }

        if (mode != OrbwalkingMode::Combo) {
            FarmDebugBreadcrumb("before-structure-fallback", minions.size());
            AttackableUnit structure = GetStructureTarget();
            FarmDebugBreadcrumb("after-structure-fallback", minions.size(), structure);
            if (structure.IsValid()) {
                FarmDebugLogTargetDecision("return:structure-fallback", structure, minions.size(), waitForFarm, true);
                return structure;
            }
        }

        FarmDebugLogTargetDecision("return:none", {}, minions.size(), waitForFarm, true);
        return {};
    }

protected:
    std::vector<AIMinionClient> GetMinions(float extraRange = 0.0f) const {
        std::vector<AIMinionClient> result;
        const float baseRange = GetAutoAttackRange(AttackableUnit());
        const float range = baseRange + extraRange + 120.0f;
        int total = 0;
        int invalid = 0;
        int ignored = 0;
        int rejectedTarget = 0;

        auto appendIfValid = [&](const AIMinionClient& minion) {
            ++total;
            if (!minion.IsValid()) {
                ++invalid;
                return;
            }
            if (IsIgnoredMinion(minion)) {
                ++ignored;
                return;
            }
            if (!OrbwalkingDetail::IsValidAttackTarget(minion, range)) {
                ++rejectedTarget;
                return;
            }
            result.push_back(minion);
        };

        for (const auto& minion : GameObjects::EnemyMinions()) {
            appendIfValid(minion);
        }
        if (Bool(attackableMenu_, "Wards", true)) {
            for (const auto& ward : GameObjects::EnemyWards()) {
                appendIfValid(ward);
            }
        }
        if (Bool(attackableMenu_, "JunglePlant", false)) {
            for (const auto& plant : GameObjects::JunglePlants()) {
                appendIfValid(plant);
            }
        }
        FarmDebugLogMinionScan(
            total,
            static_cast<int>(result.size()),
            invalid,
            ignored,
            rejectedTarget,
            extraRange,
            range);
        return result;
    }

    std::vector<AIMinionClient> GetLaneMinions(const std::vector<AIMinionClient>& minions) const {
        // GetTarget() calls into GetLastHitTarget/ShouldWait/GetLaneClearTarget in the same
        // frame while Harass/LaneClear/LastHit is active, and each of those independently
        // calls GetLaneMinions() on the same `minions` vector. Cache the filtered result per
        // tick so the filter only runs once per frame instead of up to 3x.
        const int now = Tick();
        if (now == laneMinionsCacheTick_ && minions.size() == laneMinionsCacheSourceSize_) {
            return laneMinionsCache_;
        }

        std::vector<AIMinionClient> result;
        for (const auto& minion : minions) {
            if (IsEnemyLaneMinion(minion)) {
                result.push_back(minion);
            }
        }

        laneMinionsCacheTick_ = now;
        laneMinionsCacheSourceSize_ = minions.size();
        laneMinionsCache_ = result;
        return result;
    }

    bool IsEnemyLaneMinion(const AIMinionClient& minion) const {
        if (!minion.IsValid() || minion.IsJungle() || minion.IsPlant() ||
            OrbwalkingDetail::IsWard(minion) || minion.IsPet() || minion.IsClone()) {
            return false;
        }

        // NOTE: previously this scanned GameObjects::EnemyMinions() linearly for every
        // candidate minion (called from GetLaneMinions(), which itself is called up to
        // 3x per GetTarget() call while Harass/LaneClear/LastHit is held). That made the
        // check O(n^2) per frame and was the main cause of the FPS drop on V/C/X.
        // OrbwalkingDetail::IsLaneMinion() already classifies lane minions via minion
        // type flags in O(1), so the manual list scan was redundant.
        return OrbwalkingDetail::IsLaneMinion(minion);
    }

    bool IsIgnoredMinion(const AIMinionClient& minion) const {
        const std::string name = OrbwalkingDetail::ToLower(minion.CharacterName());
        return name == "jarvanivstandard";
    }

    static bool IsGangplankBarrel(const AIMinionClient& minion) {
        if (!minion.IsValid() || minion.IsDead()) {
            return false;
        }

        return OrbwalkingDetail::ToLower(minion.CharacterName()).find("gangplankbarrel") != std::string::npos;
    }

    AttackableUnit GetChampionTarget() const {
        auto* targetSelector = TargetSelector::Instance();
        if (!targetSelector) {
            return {};
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return {};
        }

        const float range = GetAutoAttackRange(AttackableUnit()) + 125.0f;
        AIHeroClient hero = targetSelector->GetTarget(range, DamageType::Physical, true);
        if (!hero.IsValid()) {
            return {};
        }

        const AttackableUnit target(hero.Handle());
        return OrbwalkingDetail::IsValidAttackTarget(target, GetAutoAttackRange(target))
            ? target
            : AttackableUnit();
    }

    AttackableUnit GetBarrelTarget() const {
        if (!gangplankInGame_ || !Bool(attackableMenu_, "Barrels", true)) {
            return {};
        }

        auto evaluate = [&](const AIMinionClient& minion) -> AttackableUnit {
            if (!IsGangplankBarrel(minion) ||
                !OrbwalkingDetail::IsValidAttackTarget(minion, GetAutoAttackRange(minion))) {
                return {};
            }

            if (minion.Health() <= 1.0f) {
                return AttackableUnit(minion.Handle());
            }

            if (minion.Health() <= 2.0f && minion.HasBuff("gangplankebarrelactive")) {
                const float arrival = GetTimeToHit(minion);
                const float predicted = HealthPrediction::GetPrediction(
                    minion,
                    static_cast<int>(arrival),
                    0,
                    HealthPredictionType::Simulated);
                if (predicted <= 1.0f) {
                    return AttackableUnit(minion.Handle());
                }
            }
            return {};
        };

        auto scan = [&](const std::vector<AIMinionClient>& list) -> AttackableUnit {
            for (const auto& minion : list) {
                AttackableUnit result = evaluate(minion);
                if (result.IsValid()) {
                    return result;
                }
            }
            return {};
        };

        AttackableUnit result = scan(GameObjects::EnemyMinions());
        if (result.IsValid()) return result;
        result = scan(GameObjects::EnemyIgnoredMinions());
        if (result.IsValid()) return result;
        result = scan(GameObjects::Jungle());
        if (result.IsValid()) return result;
        result = scan(ObjectManager::Get<AIMinionClient>());
        if (result.IsValid()) return result;
        return {};
    }

    AttackableUnit GetLastHitTarget(std::vector<AIMinionClient> minions) {
        if (FarmDebugKeyMask() != 0) {
            FarmDebugAppend(
                "[FarmDebug] stage=last-hit-eval step=begin raw=%llu",
                static_cast<unsigned long long>(minions.size()));
        }
        minions = GetLaneMinions(minions);
        if (FarmDebugKeyMask() != 0) {
            FarmDebugAppend(
                "[FarmDebug] stage=last-hit-eval step=after-lane-filter lane=%llu",
                static_cast<unsigned long long>(minions.size()));
        }
        if (minions.empty()) {
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend("[FarmDebug] stage=last-hit-eval lane=0 reason=no-lane-minions");
            }
            return {};
        }

        const auto player = GameObjects::Player();
        if (FarmDebugKeyMask() != 0) {
            FarmDebugAppend("[FarmDebug] stage=last-hit-eval step=before-sort");
        }
        std::stable_sort(minions.begin(), minions.end(), [&](const AIMinionClient& a, const AIMinionClient& b) {
            const MinionTypes at = a.GetMinionType();
            const MinionTypes bt = b.GetMinionType();
            const bool aSiege = HasFlag(at, MinionTypes::Siege);
            const bool bSiege = HasFlag(bt, MinionTypes::Siege);
            if (aSiege != bSiege) {
                return aSiege;
            }
            const bool aSuper = HasFlag(at, MinionTypes::Super);
            const bool bSuper = HasFlag(bt, MinionTypes::Super);
            if (aSuper != bSuper) {
                return aSuper;
            }
            const float ad = std::max(1.0f, player.TotalAttackDamage());
            const float aHits = std::ceil(a.Health() / ad);
            const float bHits = std::ceil(b.Health() / ad);
            if (std::fabs(aHits - bHits) > FLT_EPSILON) {
                return aHits < bHits;
            }
            return a.MaxHealth() > b.MaxHealth();
        });
        if (FarmDebugKeyMask() != 0) {
            FarmDebugAppend("[FarmDebug] stage=last-hit-eval step=after-sort");
        }

        int inspected = 0;
        int nonKillable = 0;
        float debugBestHealth = 0.0f;
        float debugBestPrediction = FLT_MAX;
        float debugBestDamage = 0.0f;
        float debugBestDistance = 0.0f;
        std::string debugBestName;

        for (const auto& minion : minions) {
            ++inspected;
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend(
                    "[FarmDebug] stage=last-hit-eval step=minion-begin idx=%d name=%s net=%d hp=%.1f max=%.1f dist=%.1f range=%.1f",
                    inspected,
                    minion.CharacterName().c_str(),
                    minion.NetworkId(),
                    minion.Health(),
                    minion.MaxHealth(),
                    player.Distance(minion),
                    GetAutoAttackRange(minion));
            }
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend(
                    "[FarmDebug] stage=last-hit-eval step=before-time-to-hit idx=%d net=%d",
                    inspected,
                    minion.NetworkId());
            }
            const float arrival = GetTimeToHit(minion);
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend(
                    "[FarmDebug] stage=last-hit-eval step=after-time-to-hit idx=%d net=%d arrival=%.1f",
                    inspected,
                    minion.NetworkId(),
                    arrival);
            }
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend(
                    "[FarmDebug] stage=last-hit-eval step=before-health-prediction idx=%d net=%d",
                    inspected,
                    minion.NetworkId());
            }
            const float predicted = HealthPrediction::GetPrediction(
                minion,
                static_cast<int>(arrival),
                Slider(farmMenu_, "FarmDelay", 30),
                HealthPredictionType::Default);
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend(
                    "[FarmDebug] stage=last-hit-eval step=after-health-prediction idx=%d net=%d pred=%.1f",
                    inspected,
                    minion.NetworkId(),
                    predicted);
            }
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend(
                    "[FarmDebug] stage=last-hit-eval step=before-damage idx=%d net=%d",
                    inspected,
                    minion.NetworkId());
            }
            const float damage = GetAutoAttackDamage(minion);
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend(
                    "[FarmDebug] stage=last-hit-eval step=after-damage idx=%d net=%d dmg=%.1f",
                    inspected,
                    minion.NetworkId(),
                    damage);
            }
            if (predicted < debugBestPrediction) {
                debugBestPrediction = predicted;
                debugBestHealth = minion.Health();
                debugBestDamage = damage;
                debugBestDistance = player.Distance(minion);
                debugBestName = minion.CharacterName();
            }

            // EnsoulSharp NewOrbwalker last-hit rule: decide on the PREDICTED
            // health at the moment our auto attack lands, not the current health.
            //   * MaxHealth <= 10 (plants/traps): only when Health <= 1.
            //   * predicted <= 0: minion dies to other damage first -> notify
            //     consumers (NonKillableMinion), then still last-hit to secure CS.
            //   * predicted <= our AA damage: last hit it.
            if (minion.MaxHealth() <= 10.0f) {
                if (minion.Health() <= 1.0f) {
                    return AttackableUnit(minion.Handle());
                }
                continue;
            }

            if (predicted <= 0.0f) {
                ++nonKillable;
                OrbwalkingActionArgs args(
                    OrbwalkingType::NonKillableMinion,
                    AttackableUnit(minion.Handle()),
                    {},
                    "SDK");
                OrbwalkingDetail::FireNonKillableMinion(args);
            }

            if (predicted <= damage) {
                if (FarmDebugKeyMask() != 0) {
                    FarmDebugAppend(
                        "[FarmDebug] stage=last-hit-eval return=killable name=%s net=%d hp=%.1f pred=%.1f dmg=%.1f arrival=%.1f dist=%.1f",
                        minion.CharacterName().c_str(),
                        minion.NetworkId(),
                        minion.Health(),
                        predicted,
                        damage,
                        arrival,
                        player.Distance(minion));
                }
                return AttackableUnit(minion.Handle());
            }
        }
        if (FarmDebugKeyMask() != 0) {
            FarmDebugAppend(
                "[FarmDebug] stage=last-hit-eval return=none lane=%llu inspected=%d nonKillable=%d bestName=%s bestHp=%.1f bestPred=%.1f bestDmg=%.1f bestDist=%.1f",
                static_cast<unsigned long long>(minions.size()),
                inspected,
                nonKillable,
                debugBestName.empty() ? "?" : debugBestName.c_str(),
                debugBestHealth,
                debugBestPrediction == FLT_MAX ? -1.0f : debugBestPrediction,
                debugBestDamage,
                debugBestDistance);
        }
        return {};
    }

    AttackableUnit GetStructureTarget() const {
        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return {};
        }

        for (const auto& turret : GameObjects::EnemyTurrets()) {
            if (!turret.IsValid() || turret.IsDead() ||
                !turret.IsLaneTurret() ||
                turret.IsFountainTurret() || turret.IsShurimaTurret()) {
                continue;
            }
            AttackableUnit target(turret.Handle());
            if (OrbwalkingDetail::IsValidAttackTarget(target, GetAutoAttackRange(target))) {
                return target;
            }
        }

        for (const auto& inhibitor : GameObjects::EnemyInhibitors()) {
            if (!inhibitor.IsValid() || inhibitor.IsDead()) {
                continue;
            }
            AttackableUnit target(inhibitor.Handle());
            if (OrbwalkingDetail::IsValidAttackTarget(target, GetAutoAttackRange(target))) {
                return target;
            }
        }

        const auto nexus = GameObjects::EnemyNexus();
        if (nexus.IsValid() && !nexus.IsDead() && !nexus.HasShield()) {
            AttackableUnit target(nexus.Handle());
            if (OrbwalkingDetail::IsValidAttackTarget(target, GetAutoAttackRange(target))) {
                return target;
            }
        }
        return {};
    }

    AttackableUnit GetJunglePlantTarget() const {
        if (!Bool(attackableMenu_, "JunglePlant", false)) {
            return {};
        }

        for (const auto& plant : GameObjects::JunglePlants()) {
            if (OrbwalkingDetail::IsValidAttackTarget(plant, GetAutoAttackRange(plant))) {
                return AttackableUnit(plant.Handle());
            }
        }
        return {};
    }

    AttackableUnit GetSpecialMinionTarget(OrbwalkingMode mode) const {
        if (mode == OrbwalkingMode::Combo) {
            return {};
        }

        auto validTarget = [&](const AIMinionClient& minion) {
            return OrbwalkingDetail::IsValidAttackTarget(minion, GetAutoAttackRange(minion));
        };

        auto scanSpecial = [&](const std::vector<AIMinionClient>& minions) -> AttackableUnit {
            for (const auto& minion : minions) {
                if (OrbwalkingDetail::IsEnsoulSpecialMinion(minion) &&
                    validTarget(minion)) {
                    return AttackableUnit(minion.Handle());
                }
            }
            return {};
        };

        if (Bool(attackableMenu_, "SpecialMinions", true)) {
            AttackableUnit result = scanSpecial(GameObjects::EnemyPets());
            if (result.IsValid()) return result;
            result = scanSpecial(GameObjects::EnemySpecialMinions());
            if (result.IsValid()) return result;
            result = scanSpecial(GameObjects::EnemyClones());
            if (result.IsValid()) return result;
        }

        if (Bool(attackableMenu_, "Wards", true)) {
            for (const auto& ward : GameObjects::EnemyWards()) {
                if (validTarget(ward)) {
                    return AttackableUnit(ward.Handle());
                }
            }
        }

        if (Bool(attackableMenu_, "JunglePlant", false)) {
            for (const auto& plant : GameObjects::JunglePlants()) {
                if (validTarget(plant)) {
                    return AttackableUnit(plant.Handle());
                }
            }
        }

        return {};
    }

    AttackableUnit GetJungleTarget() const {
        std::vector<AIMinionClient> jungle;
        for (const auto& minion : GameObjects::Jungle()) {
            if (!minion.IsValid() || minion.IsPlant() ||
                !OrbwalkingDetail::IsValidAttackTarget(minion, GetAutoAttackRange(minion))) {
                continue;
            }
            jungle.push_back(minion);
        }

        if (jungle.empty()) {
            return {};
        }

        const bool smallFirst = Bool(prioritizeMenu_, "SmallJungle", false);
        std::stable_sort(jungle.begin(), jungle.end(), [smallFirst](const AIMinionClient& a, const AIMinionClient& b) {
            if (smallFirst) {
                return a.MaxHealth() < b.MaxHealth();
            }
            return a.MaxHealth() > b.MaxHealth();
        });
        return AttackableUnit(jungle.front().Handle());
    }

    AttackableUnit GetTurretFarmTarget(const std::vector<AIMinionClient>& minions) {
        std::vector<AIMinionClient> lane = GetLaneMinions(minions);
        if (!CanTurretFarm(lane)) {
            return {};
        }

        const auto player = GameObjects::Player();
        // REMOVED: Turret/Inhibitor/Nexus disabled by user request
        // AITurretClient tower;
        // float towerDistance = FLT_MAX;
        // for (const auto& turret : GameObjects::AllyTurrets()) {
        //     if (!turret.IsValid() || turret.IsDead()) {
        //         continue;
        //     }
        //     const float distance = turret.Distance(player);
        //     if (distance < towerDistance && distance <= 1500.0f) {
        //         tower = turret;
        //         towerDistance = distance;
        //     }
        // }
        // if (!tower.IsValid()) {
        //     return {};
        // }
        // REMOVED: Turret/Inhibitor/Nexus disabled
        (void)player;
        return {};

        /* REMOVED: Turret/Inhibitor/Nexus disabled by user request
        std::vector<AIMinionClient> source = lane;
        source.erase(
            std::remove_if(source.begin(), source.end(), [&](const AIMinionClient& minion) {
                return !OrbwalkingDetail::IsValidAttackTarget(minion, GetAutoAttackRange(minion)) ||
                       minion.Distance(tower) > 900.0f;
            }),
            source.end());
        if (source.empty()) {
            return {};
        }

        std::stable_sort(source.begin(), source.end(), [&](const AIMinionClient& a, const AIMinionClient& b) {
            return a.DistanceSquared(tower) < b.DistanceSquared(tower);
        });

        auto canLastHitNow = [&](const AIMinionClient& minion) {
            return CanLastHitTurretMinionNow(tower, minion);
        };

        AIMinionClient aggroMinion = TrackedTurretTarget(tower, source);
        if (!aggroMinion.IsValid()) {
            for (const auto& minion : source) {
                if (HealthPrediction::HasTurretAggro(minion)) {
                    aggroMinion = minion;
                    break;
                }
            }
        }

        if (aggroMinion.IsValid()) {
            if (canLastHitNow(aggroMinion)) {
                return AttackableUnit(aggroMinion.Handle());
            }

            const float turretDamage = GetTurretAutoAttackDamage(tower, aggroMinion);
            const float turretImpact = GetTurretAttackImpact(tower, aggroMinion);
            const float aggroPrediction = HealthPrediction::GetPrediction(
                aggroMinion,
                static_cast<int>(turretImpact + GetTimeToHit(aggroMinion)),
                70,
                HealthPredictionType::Simulated);
            if (turretDamage > 0.0f && aggroPrediction > turretDamage) {
                AttackableUnit setup = GetTurretSetupTarget(tower, source, aggroMinion, turretImpact);
                if (setup.IsValid()) {
                    return setup;
                }
            }
            return {};
        }

        for (const auto& minion : source) {
            if (canLastHitNow(minion)) {
                return AttackableUnit(minion.Handle());
            }
        }
        return GetTurretNoAggroSetupTarget(tower, source);
    */
    }

    AttackableUnit GetLaneClearTarget(const std::vector<AIMinionClient>& minions) {
        const int farmDelay = Slider(farmMenu_, "FarmDelay", 30);
        if (laneClearMinion_.IsValid() &&
            OrbwalkingDetail::IsValidAttackTarget(laneClearMinion_, GetAutoAttackRange(laneClearMinion_))) {
            const float prediction = HealthPrediction::GetPrediction(
                laneClearMinion_,
                static_cast<int>(GetAttackDelay() * 2000.0f),
                farmDelay,
                HealthPredictionType::Simulated);
            const float damage = GetAutoAttackDamage(laneClearMinion_);
            if (laneClearMinion_.MaxHealth() <= 10.0f ||
                prediction >= damage * 2.0f ||
                std::fabs(prediction - laneClearMinion_.Health()) < FLT_EPSILON) {
                if (FarmDebugKeyMask() != 0) {
                    FarmDebugAppend(
                        "[FarmDebug] stage=lane-clear-eval return=cached name=%s net=%d hp=%.1f pred=%.1f dmg=%.1f",
                        laneClearMinion_.CharacterName().c_str(),
                        laneClearMinion_.NetworkId(),
                        laneClearMinion_.Health(),
                        prediction,
                        damage);
                }
                return AttackableUnit(laneClearMinion_.Handle());
            }
        }

        std::vector<AIMinionClient> lane = GetLaneMinions(minions);
        std::vector<AIMinionClient> candidates;
        int rejectedRange = 0;
        for (const auto& minion : lane) {
            if (!OrbwalkingDetail::IsValidAttackTarget(minion, GetAutoAttackRange(minion))) {
                ++rejectedRange;
                continue;
            }

            const float prediction = HealthPrediction::GetPrediction(
                minion,
                static_cast<int>(GetAttackDelay() * 2000.0f),
                farmDelay,
                HealthPredictionType::Simulated);
            const float damage = GetAutoAttackDamage(minion);
            if (minion.MaxHealth() <= 10.0f ||
                prediction >= damage * 2.0f ||
                std::fabs(prediction - minion.Health()) < FLT_EPSILON) {
                candidates.push_back(minion);
            }
        }

        if (candidates.empty()) {
            laneClearMinion_ = {};
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend(
                    "[FarmDebug] stage=lane-clear-eval return=none lane=%llu rejectedRange=%d candidates=0",
                    static_cast<unsigned long long>(lane.size()),
                    rejectedRange);
            }
            return {};
        }

        std::stable_sort(candidates.begin(), candidates.end(), [](const AIMinionClient& a, const AIMinionClient& b) {
            return a.Health() > b.Health();
        });
        laneClearMinion_ = candidates.front();
        if (FarmDebugKeyMask() != 0) {
            FarmDebugAppend(
                "[FarmDebug] stage=lane-clear-eval return=best name=%s net=%d hp=%.1f candidates=%llu",
                laneClearMinion_.CharacterName().c_str(),
                laneClearMinion_.NetworkId(),
                laneClearMinion_.Health(),
                static_cast<unsigned long long>(candidates.size()));
        }
        return AttackableUnit(laneClearMinion_.Handle());
    }

    static bool IsFastLaneClearCannonLike(const AIMinionClient& minion) {
        const MinionTypes type = minion.GetMinionType();
        return HasFlag(type, MinionTypes::Siege) || HasFlag(type, MinionTypes::Super);
    }

    static int FastLaneClearFarmPriority(const AIMinionClient& minion) {
        const MinionTypes type = minion.GetMinionType();
        if (IsFastLaneClearCannonLike(minion)) {
            return 2;
        }
        if (HasFlag(type, MinionTypes::Ranged)) {
            return 0;
        }
        if (HasFlag(type, MinionTypes::Melee)) {
            return 1;
        }
        return 3;
    }

    static int FastLaneClearLastHitPriority(const AIMinionClient& minion) {
        const MinionTypes type = minion.GetMinionType();
        if (IsFastLaneClearCannonLike(minion)) {
            return 0;
        }
        if (HasFlag(type, MinionTypes::Melee)) {
            return 1;
        }
        if (HasFlag(type, MinionTypes::Ranged)) {
            return 2;
        }
        return 3;
    }

    bool IsFastLaneClearTargetable(const AIMinionClient& minion) const {
        return minion.IsValid() &&
               OrbwalkingDetail::IsValidAttackTarget(minion, GetAutoAttackRange(minion));
    }

    bool IsFastLaneClearLastHitCandidate(const AIMinionClient& minion) const {
        if (!IsFastLaneClearTargetable(minion)) {
            return false;
        }
        return IsDrawKillableMinionThreshold(minion);
    }

    AttackableUnit GetFastLaneClearTarget(const std::vector<AIMinionClient>& minions) {
        std::vector<AIMinionClient> candidates;
        for (const auto& minion : GetLaneMinions(minions)) {
            if (IsFastLaneClearTargetable(minion)) {
                candidates.push_back(minion);
            }
        }

        if (candidates.empty()) {
            laneClearMinion_ = {};
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend("[FarmDebug] stage=fast-lane-clear-eval return=none candidates=0");
            }
            return {};
        }

        const auto player = GameObjects::Player();
        std::vector<AIMinionClient> lastHitCandidates;
        for (const auto& minion : candidates) {
            if (IsFastLaneClearLastHitCandidate(minion)) {
                lastHitCandidates.push_back(minion);
            }
        }

        if (!lastHitCandidates.empty()) {
            std::stable_sort(lastHitCandidates.begin(), lastHitCandidates.end(), [&](const AIMinionClient& a, const AIMinionClient& b) {
                const int ap = FastLaneClearLastHitPriority(a);
                const int bp = FastLaneClearLastHitPriority(b);
                if (ap != bp) {
                    return ap < bp;
                }
                if (std::fabs(a.Health() - b.Health()) > FLT_EPSILON) {
                    return a.Health() < b.Health();
                }
                if (player.IsValid()) {
                    return a.DistanceSquared(player) < b.DistanceSquared(player);
                }
                return a.NetworkId() < b.NetworkId();
            });

            laneClearMinion_ = lastHitCandidates.front();
            if (FarmDebugKeyMask() != 0) {
                FarmDebugAppend(
                    "[FarmDebug] stage=fast-lane-clear-eval return=last-hit name=%s net=%d hp=%.1f dmg=%.1f priority=%d candidates=%llu",
                    laneClearMinion_.CharacterName().c_str(),
                    laneClearMinion_.NetworkId(),
                    laneClearMinion_.Health(),
                    GetAutoAttackDamage(laneClearMinion_),
                    FastLaneClearLastHitPriority(laneClearMinion_),
                    static_cast<unsigned long long>(lastHitCandidates.size()));
            }
            return AttackableUnit(laneClearMinion_.Handle());
        }

        std::stable_sort(candidates.begin(), candidates.end(), [&](const AIMinionClient& a, const AIMinionClient& b) {
            const int ap = FastLaneClearFarmPriority(a);
            const int bp = FastLaneClearFarmPriority(b);
            if (ap != bp) {
                return ap < bp;
            }
            if (std::fabs(a.Health() - b.Health()) > FLT_EPSILON) {
                return a.Health() > b.Health();
            }
            if (player.IsValid()) {
                return a.DistanceSquared(player) < b.DistanceSquared(player);
            }
            return a.NetworkId() < b.NetworkId();
        });

        laneClearMinion_ = candidates.front();
        if (FarmDebugKeyMask() != 0) {
            FarmDebugAppend(
                "[FarmDebug] stage=fast-lane-clear-eval return=farm name=%s net=%d hp=%.1f priority=%d candidates=%llu",
                laneClearMinion_.CharacterName().c_str(),
                laneClearMinion_.NetworkId(),
                laneClearMinion_.Health(),
                FastLaneClearFarmPriority(laneClearMinion_),
                static_cast<unsigned long long>(candidates.size()));
        }
        return AttackableUnit(laneClearMinion_.Handle());
    }

    bool ShouldWait(const std::vector<AIMinionClient>& minions) {
        const int now = Tick();
        const int missileVersion = FarmMissileVersion();
        const bool fastLaneClear = IsFastLaneClear();
        if (now - cachedShouldWaitTick_ < 80 &&
            cachedShouldWaitMissileVersion_ == missileVersion &&
            cachedShouldWaitFastLaneClear_ == fastLaneClear) {
            return cachedShouldWait_;
        }

        cachedShouldWait_ = ComputeShouldWait(minions);
        cachedShouldWaitMissileVersion_ = missileVersion;
        cachedShouldWaitFastLaneClear_ = fastLaneClear;
        cachedShouldWaitTick_ = now;
        return cachedShouldWait_;
    }

    bool ComputeShouldWait(const std::vector<AIMinionClient>& minions) const {
        // Always-on [SW] trace (throttled): logs every state transition
        // immediately and a summary line at most once per second, so the farm
        // pause behaviour is visible in the debug log without any debug key.
        static long s_calls = 0;
        static int s_lastLogTick = 0;
        static int s_lastResult = -1;
        ++s_calls;

        int inspected = 0;
        std::string bestName;
        float bestPrediction = FLT_MAX;
        float bestDamage = 0.0f;
        float bestHealth = 0.0f;
        std::size_t laneCount = 0;
        float time = 0.0f;

        const auto logResult = [&](bool result, const char* reason) -> bool {
            const int now = Tick();
            const bool transition = (s_lastResult != (result ? 1 : 0));
            if (transition || now - s_lastLogTick >= 1000) {
                s_lastLogTick = now;
                s_lastResult = result ? 1 : 0;
                NightSharpDebug::Logf(
                    "[SW] %s reason=%s mode=%d calls=%ld lane=%zu inspected=%d best=%s hp=%.0f pred=%.0f dmg=%.0f time=%.0f active=%zu",
                    result ? "WAIT" : "go",
                    reason,
                    static_cast<int>(ActiveMode()),
                    s_calls,
                    laneCount,
                    inspected,
                    bestName.empty() ? "-" : bestName.c_str(),
                    bestHealth,
                    bestPrediction == FLT_MAX ? -1.0f : bestPrediction,
                    bestDamage,
                    time,
                    ::SDK::Prediction::Health::detail::ActiveAttacks.size());
            }
            return result;
        };

        const OrbwalkingMode mode = ActiveMode();
        if (mode == OrbwalkingMode::Combo || mode == OrbwalkingMode::Flee ||
            mode == OrbwalkingMode::None || ShouldSkipFarmForSupportMode()) {
            return logResult(false, "mode-or-support");
        }
        if (mode == OrbwalkingMode::LaneClear && IsFastLaneClear()) {
            return logResult(false, "fast-lane-clear");
        }
        if (!Bool(farmMenu_, "ShouldWait", true)) {
            return logResult(false, "menu-off");
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return logResult(false, "no-player");
        }

        const int farmDelay = Slider(farmMenu_, "FarmDelay", 30);
        // EnsoulSharp OrbwalkerSDK.ShouldWait: time = min(50, ping) + (AttackDelay + AttackCastDelay) * 1000
        time = static_cast<float>(std::min(50, Game::Ping())) +
               (GetAttackDelay() + GetAttackCastDelay()) * 1000.0f;

        const auto laneMinions = GetLaneMinions(minions);
        laneCount = laneMinions.size();
        for (const auto& minion : laneMinions) {
            if (!minion.IsValid() || minion.IsDead() || minion.IsJungle() ||
                !OrbwalkingDetail::IsValidAttackTarget(minion, GetAutoAttackRange(minion) + 200.0f)) {
                continue;
            }

            ++inspected;
            const float damage = GetAutoAttackDamage(minion);
            if (damage <= 0.0f) {
                continue;
            }

            // REMOVED: Turret/Inhibitor/Nexus disabled by user request
            /*
            if (ShouldWaitForTurretFarm(minion)) {
                bestName = minion.CharacterName();
                bestHealth = minion.Health();
                bestDamage = damage;
                return logResult(true, "turret-farm");
            }
            */

            // EnsoulSharp NewOrbwalker ShouldWait: the Simulated prediction below
            // already folds in incoming ally/turret damage, so we wait based on it
            // alone. Matches NewOrbwalker.cs ShouldWait exactly.
            const float prediction = HealthPrediction::GetPrediction(
                minion, static_cast<int>(time), farmDelay, HealthPredictionType::Simulated);

            if (prediction < bestPrediction) {
                bestPrediction = prediction;
                bestDamage = damage;
                bestHealth = minion.Health();
                bestName = minion.CharacterName();
            }

            // EnsoulSharp OrbwalkerSDK.ShouldWait: wait when prediction < damage.
            // The Simulated prediction already folds in incoming ally/turret
            // damage, so we wait based on it alone. Matches EnsoulSharp exactly.
            if (prediction < damage) {
                bestPrediction = prediction;
                bestDamage = damage;
                bestHealth = minion.Health();
                bestName = minion.CharacterName();
                return logResult(true, "last-hit-window");
            }
        }
        return logResult(false, "no-imminent-lasthit");
    }

    std::vector<AIMinionClient> GetNearbyAllyFarmMinions(
        const std::vector<AIMinionClient>& laneMinions) const {
        std::vector<AIMinionClient> result;
        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return result;
        }

        for (const auto& ally : GameObjects::AllyMinions()) {
            if (!ally.IsValid() || ally.IsDead() || !ally.IsVisible() ||
                ally.IsJungle() || ally.IsPlant() || ally.IsPet() || ally.IsClone() ||
                !OrbwalkingDetail::IsLaneMinion(ally)) {
                continue;
            }

            if (ally.Distance(player) <= 1800.0f) {
                result.push_back(ally);
                continue;
            }

            for (const auto& minion : laneMinions) {
                if (minion.IsValid() && ally.Distance(minion) <= 1500.0f) {
                    result.push_back(ally);
                    break;
                }
            }
        }
        return result;
    }

    int CountNearbyAllyFarmMinionsTo(
        const AIMinionClient& minion,
        const std::vector<AIMinionClient>& nearbyAllies) const {
        if (!minion.IsValid()) {
            return 0;
        }

        int count = 0;
        for (const auto& ally : nearbyAllies) {
            if (ally.IsValid() && ally.Distance(minion) <= 1500.0f) {
                ++count;
            }
        }
        return count;
    }

    bool HasAllyFarmPressureOnMinion(
        const AIMinionClient& minion,
        int nearbyAllyCount,
        int missileCount) const {
        if (!minion.IsValid()) {
            return false;
        }
        if (std::max(nearbyAllyCount, missileCount) <= 2) {
            return false;
        }
        return missileCount > 0 ||
               nearbyAllyCount > 0 ||
               HealthPrediction::HasMinionAggro(minion);
    }

    float GetLaneWaitPrediction(const AIMinionClient& minion,
                                float time,
                                int farmDelay,
                                bool hasAllyPressure) const {
        float prediction = HealthPrediction::GetPrediction(
            minion,
            static_cast<int>(time),
            farmDelay,
            HealthPredictionType::Default);

        if (std::fabs(prediction - minion.Health()) >= 1.0f) {
            return prediction;
        }

        if (!hasAllyPressure) {
            return prediction;
        }

        const float gameMinute = Game::Time() / 60.0f;
        const float waveDps = std::min(150.0f, 50.0f + gameMinute * 2.5f);
        return std::max(0.0f, minion.Health() - time / 1000.0f * waveDps);
    }

    bool IsTurretFarmBlockedMinion(const AIMinionClient& minion) const {
        if (!minion.IsValid()) {
            return false;
        }
        if (minion.HasBuff("exaltedwithbaronnashorminion")) {
            return true;
        }
        const MinionTypes type = minion.GetMinionType();
        if (HasFlag(type, MinionTypes::Super)) {
            return true;
        }
        const std::string name = OrbwalkingDetail::ToLower(minion.CharacterName());
        return name.find("minionsuper") != std::string::npos;
    }

    bool CanTurretFarm(const std::vector<AIMinionClient>& laneMinions) const {
        const auto player = GameObjects::Player();
        if (!player.IsValid() ||
            !Bool(farmMenu_, "TurretFarm", true) ||
            ShouldSkipFarmForSupportMode() ||
            player.Level() >= Slider(farmMenu_, "TurretFramMaxLevel", 13) ||
            laneMinions.empty()) {
            return false;
        }

        for (const auto& minion : laneMinions) {
            if (IsTurretFarmBlockedMinion(minion)) {
                return false;
            }
        }
        return true;
    }

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*float GetTurretAutoAttackDamage(const AITurretClient& tower,
                                    const AIMinionClient& minion) const {
        if (!tower.IsValid() || !minion.IsValid()) {
            return 0.0f;
        }
        return Prediction::Health::GetAutoAttackDamage(tower, minion);
    }*/

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*float GetTurretAttackImpact(const AITurretClient& tower,
                                const AIMinionClient& minion) const {
        if (!tower.IsValid() || !minion.IsValid()) {
            return 1000.0f;
        }

        float windup = CoreControl::GetAttackWindup(tower.Address()) * 1000.0f;
        if (!std::isfinite(windup) || windup <= 0.0f || windup > 2500.0f) {
            windup = 300.0f;
        }

        constexpr float turretMissileSpeed = 1270.0f;
        const float travel = 1000.0f *
            std::max(0.0f, minion.Distance(tower) - tower.BoundingRadius()) /
            turretMissileSpeed;
        return std::max(0.0f, windup + travel);
    }*/

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*AttackableUnit GetTurretSetupTarget(const AITurretClient& tower,
                                        const std::vector<AIMinionClient>& source,
                                        const AIMinionClient& aggroMinion,
                                        float turretImpact) const {
        for (const auto& minion : source) {
            if (!minion.IsValid() ||
                minion.Compare(aggroMinion) ||
                HealthPrediction::HasTurretAggro(minion) ||
                HealthPrediction::HasMinionAggro(minion)) {
                continue;
            }

            const float playerDamage = GetAutoAttackDamage(minion);
            const float turretDamage = GetTurretAutoAttackDamage(tower, minion);
            if (playerDamage <= 0.0f || turretDamage <= 0.0f) {
                continue;
            }

            const float prediction = HealthPrediction::GetPrediction(
                minion,
                static_cast<int>(GetTimeToHit(minion) + turretImpact),
                70,
                HealthPredictionType::Simulated);
            if (prediction <= 0.0f) {
                continue;
            }

            if (prediction < turretDamage * 2.0f ||
                prediction > turretDamage * 2.0f + playerDamage) {
                if (prediction > turretDamage + playerDamage &&
                    prediction <= turretDamage + playerDamage * 2.0f) {
                    return AttackableUnit(minion.Handle());
                }
                if (prediction > turretDamage * 2.0f + playerDamage * 2.0f) {
                    return AttackableUnit(minion.Handle());
                }
            }
        }
        return {};
    }*/

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*AttackableUnit GetTurretNoAggroSetupTarget(const AITurretClient& tower,
                                               const std::vector<AIMinionClient>& source) const {
        if (!tower.IsValid() || source.empty()) {
            return {};
        }

        const AIMinionClient minion = source.front();
        if (!minion.IsValid()) {
            return {};
        }

        const float playerDamage = GetAutoAttackDamage(minion);
        const float turretDamage = GetTurretAutoAttackDamage(tower, minion);
        if (playerDamage <= 0.0f || turretDamage <= 0.0f) {
            return {};
        }

        const float healthAfterTurret = HealthPrediction::GetPrediction(
            minion,
            1500,
            70,
            HealthPredictionType::Simulated) - turretDamage * 1.1f;
        if (healthAfterTurret > playerDamage &&
            healthAfterTurret < turretDamage * 1.1f) {
            return AttackableUnit(minion.Handle());
        }
        if (healthAfterTurret > turretDamage * 2.0f + playerDamage * 2.0f) {
            return AttackableUnit(minion.Handle());
        }
        return {};
    }*/

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*bool IsTurretFocusedMinion(const AITurretClient& tower,
                               const AIMinionClient& minion) const {
        if (!tower.IsValid() || !minion.IsValid()) {
            return false;
        }
        if (AllyTurretMissileDamageTo(minion, tower.NetworkId()) > 0.0f) {
            return true;
        }
        if (!HealthPrediction::HasTurretAggro(minion)) {
            return false;
        }

        const AIBaseClient aggroTurret = HealthPrediction::GetAggroTurret(minion);
        return !aggroTurret.IsValid() || aggroTurret.NetworkId() == tower.NetworkId();
    }*/

    int TurretFarmSetupShots(const AIMinionClient& minion) const {
        const MinionTypes type = minion.GetMinionType();
        if (HasFlag(type, MinionTypes::Melee)) {
            return 2;
        }
        if (HasFlag(type, MinionTypes::Ranged)) {
            return 1;
        }
        return 0;
    }

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*bool CanLastHitTurretMinionNow(const AITurretClient& tower,
                                   const AIMinionClient& minion) const {
        if (!IsTurretFocusedMinion(tower, minion)) {
            return false;
        }

        const float damage = GetAutoAttackDamage(minion);
        if (damage <= 0.0f) {
            return false;
        }

        const int farmDelay = Slider(farmMenu_, "FarmDelay", 30);
        const int impact = static_cast<int>(GetTimeToHit(minion) + static_cast<float>(farmDelay));
        float prediction = HealthPrediction::GetPrediction(
            minion,
            impact,
            farmDelay,
            HealthPredictionType::Simulated);

        if (!HealthPrediction::HasTurretAggro(minion)) {
            const float missileDamage = AllyTurretMissileDamageTo(minion, tower.NetworkId());
            if (missileDamage > 0.0f) {
                prediction = std::min(prediction, minion.Health() - missileDamage);
            }
        }

        return prediction > 0.0f && prediction <= damage;
    }*/

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*bool ShouldWaitForTurretFarm(const AIMinionClient& minion) const {
        const auto player = GameObjects::Player();
        if (!player.IsValid() ||
            player.Level() >= Slider(farmMenu_, "TurretFramMaxLevel", 13) ||
            !Bool(farmMenu_, "TurretFarm", true) ||
            ShouldSkipFarmForSupportMode() ||
            IsTurretFarmBlockedMinion(minion)) {
            return false;
        }

        AITurretClient tower;
        float towerDistance = FLT_MAX;
        for (const auto& turret : GameObjects::AllyTurrets()) {
            if (!turret.IsValid() || turret.IsDead()) {
                continue;
            }
            const float distance = turret.Distance(player);
            if (distance < towerDistance && distance <= 1500.0f &&
                minion.Distance(turret) <= 900.0f) {
                tower = turret;
                towerDistance = distance;
            }
        }
        if (!tower.IsValid() || !IsTurretFocusedMinion(tower, minion)) {
            return false;
        }

        if (CanLastHitTurretMinionNow(tower, minion)) {
            return true;
        }

        const float playerDamage = GetAutoAttackDamage(minion);
        float turretDamage = AllyTurretMissileDamageTo(minion, tower.NetworkId());
        if (turretDamage <= 0.0f) {
            turretDamage = GetTurretAutoAttackDamage(tower, minion);
        }
        if (playerDamage <= 0.0f || turretDamage <= 0.0f) {
            return false;
        }

        const int setupShots = TurretFarmSetupShots(minion);
        for (int shots = 1; shots <= setupShots; ++shots) {
            const float healthAfterShots = minion.Health() - turretDamage * static_cast<float>(shots);
            if (healthAfterShots > 0.0f && healthAfterShots <= playerDamage) {
                return true;
            }
            if (healthAfterShots <= 0.0f) {
                break;
            }
        }

        return false;
    }*/

    bool cachedShouldWait_ = false;
    bool cachedShouldWaitFastLaneClear_ = false;
    int cachedShouldWaitTick_ = 0;
    int cachedShouldWaitMissileVersion_ = -1;
};

} // namespace SDK
