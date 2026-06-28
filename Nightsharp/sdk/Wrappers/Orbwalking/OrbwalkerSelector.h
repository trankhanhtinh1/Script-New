#pragma once

#include "OrbwalkerBase.h"
#include "../TargetSelector/TargetSelector.h"
#include "../../Enumerations/HealthPredictionType.h"
#include "../../Enumerations/MinionTypes.h"
#include "../../Math/HealthPrediction.h"
#include "../../UI/UI.h"
#include "../../Wrappers/Damages/Damage.h"
#include "../../Utils/AutoAttack.h"
#include "../../../FpsDropDebug.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace SDK {

class OrbwalkerSelector {
private:
    static constexpr float kLaneClearWaitTime = 2.0f;

    static constexpr const char* kClones[] = {
        "leblanc", "monkeyking", "neeko", "shaco"
    };

    static constexpr const char* kIgnoreMinions[] = {
        "jarvanivstandard"
    };

    static constexpr const char* kSpecialMinions[] = {
        "annietibbers", "elisespiderling", "heimertyellow",
        "heimertblue", "ivernminion", "malzaharvoidling",
        "shacobox", "teemomushroom", "yorickghoulmelee",
        "yorickbigghoul", "zyrathornplant", "zyragraspingplant"
    };

    static bool IsIgnoredMinion(const char* name) {
        if (!name || !name[0]) return false;
        for (const auto* ign : kIgnoreMinions) {
            if (_stricmp(name, ign) == 0) return true;
        }
        return false;
    }

    static bool IsSpecialMinion(const char* name) {
        if (!name || !name[0]) return false;
        for (const auto* spec : kSpecialMinions) {
            if (_stricmp(name, spec) == 0) return true;
        }
        return false;
    }

    static bool IsClone(const char* name) {
        if (!name || !name[0]) return false;
        for (const auto* clone : kClones) {
            if (_stricmp(name, clone) == 0) return true;
        }
        return false;
    }

    OrbwalkerBase* orbwalker_;
    Menu* menu_;
    AIMinionClient laneClearMinion_;

    int FarmDelay() const {
        auto* advanced = menu_->GetSubMenu("advanced");
        return advanced ? advanced->Get<MenuSlider>("delayFarm")->Value : 30;
    }

    bool MenuBool(const char* name) const {
        auto* advanced = menu_->GetSubMenu("advanced");
        return advanced && advanced->Get<UI::MenuBool>(name)->Value;
    }

    static std::vector<AIMinionClient> OrderEnemyMinions(
        const std::vector<AIMinionClient>& minions) {
        auto sorted = minions;
        std::sort(sorted.begin(), sorted.end(),
            [](const AIMinionClient& a, const AIMinionClient& b) {
                const bool aSiege = HasFlag(a.GetMinionType(), MinionTypes::Siege);
                const bool bSiege = HasFlag(b.GetMinionType(), MinionTypes::Siege);
                if (aSiege != bSiege) return aSiege > bSiege;

                const bool aSuper = HasFlag(a.GetMinionType(), MinionTypes::Super);
                const bool bSuper = HasFlag(b.GetMinionType(), MinionTypes::Super);
                if (aSuper != bSuper) return aSuper > bSuper;

                if (a.Health() != b.Health()) return a.Health() < b.Health();
                return a.MaxHealth() > b.MaxHealth();
            });
        return sorted;
    }

    std::vector<AIMinionClient> OrderJungleMinions(
        std::vector<AIMinionClient> minions) const {
        const bool prioritizeSmall = MenuBool("prioritizeSmallJungle");
        if (prioritizeSmall) {
            std::sort(minions.begin(), minions.end(),
                [](const AIMinionClient& a, const AIMinionClient& b) {
                    return a.MaxHealth() < b.MaxHealth();
                });
        } else {
            std::sort(minions.begin(), minions.end(),
                [](const AIMinionClient& a, const AIMinionClient& b) {
                    return a.MaxHealth() > b.MaxHealth();
                });
        }
        return minions;
    }

    std::vector<AIMinionClient> GetMinions(OrbwalkingMode mode) const {
        NS_PROFILE("orb.GetMinions.total");
        const bool includeMinions = mode != OrbwalkingMode::Combo;
        const bool attackWards = MenuBool("attackWards");
        const bool attackClones = MenuBool("attackClones");
        const bool attackSpecialMinions = MenuBool("attackSpecialMinions");
        const bool prioritizeWards = MenuBool("prioritizeWards");
        const bool prioritizeSpecialMinions = MenuBool("prioritizeSpecialMinions");

        std::vector<AIMinionClient> minionList;
        std::vector<AIMinionClient> specialList;
        std::vector<AIMinionClient> cloneList;
        std::vector<AIMinionClient> wardList;

        {
            NS_PROFILE("orb.GameObjects.EnemyMinions");
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (!IsValidUnit(minion)) continue;

                const std::string baseName = minion.CharacterName();

                if (includeMinions && minion.IsMinion()) {
                    minionList.push_back(minion);
                } else if (attackSpecialMinions && IsSpecialMinion(baseName.c_str())) {
                    specialList.push_back(minion);
                } else if (attackClones && IsClone(baseName.c_str())) {
                    cloneList.push_back(minion);
                }
            }
        }

        if (includeMinions) {
            NS_PROFILE("orb.GameObjects.Jungle");
            minionList = OrderEnemyMinions(minionList);
            std::vector<AIMinionClient> jungleList;
            for (const auto& j : GameObjects::Jungle()) {
                if (IsValidUnit(j)) {
                    const std::string jName = j.CharacterName();
                    if (_stricmp(jName.c_str(), "gangplankbarrel") != 0) {
                        jungleList.push_back(j);
                    }
                }
            }
            auto orderedJungle = OrderJungleMinions(jungleList);
            minionList.insert(minionList.end(), orderedJungle.begin(), orderedJungle.end());
        }

        if (attackWards) {
            NS_PROFILE("orb.GameObjects.EnemyWards");
            for (const auto& w : GameObjects::EnemyWards()) {
                if (IsValidUnit(w)) {
                    wardList.push_back(w);
                }
            }
        }

        std::vector<AIMinionClient> finalList;
        if (attackWards && prioritizeWards && attackSpecialMinions && prioritizeSpecialMinions) {
            finalList.insert(finalList.end(), wardList.begin(), wardList.end());
            finalList.insert(finalList.end(), specialList.begin(), specialList.end());
            finalList.insert(finalList.end(), minionList.begin(), minionList.end());
        } else if (attackSpecialMinions && prioritizeSpecialMinions) {
            finalList.insert(finalList.end(), specialList.begin(), specialList.end());
            finalList.insert(finalList.end(), minionList.begin(), minionList.end());
            finalList.insert(finalList.end(), wardList.begin(), wardList.end());
        } else if (attackWards && prioritizeWards) {
            finalList.insert(finalList.end(), wardList.begin(), wardList.end());
            finalList.insert(finalList.end(), minionList.begin(), minionList.end());
            finalList.insert(finalList.end(), specialList.begin(), specialList.end());
        } else {
            finalList.insert(finalList.end(), minionList.begin(), minionList.end());
            finalList.insert(finalList.end(), specialList.begin(), specialList.end());
            finalList.insert(finalList.end(), wardList.begin(), wardList.end());
        }

        if (MenuBool("attackBarrels")) {
            for (const auto& j : GameObjects::Jungle()) {
                if (IsValidUnit(j)) {
                    const std::string jName = j.CharacterName();
                    if (_stricmp(jName.c_str(), "gangplankbarrel") == 0 && j.Health() <= 1.0f) {
                        finalList.push_back(j);
                    }
                }
            }
        }

        if (attackClones) {
            finalList.insert(finalList.end(), cloneList.begin(), cloneList.end());
        }

        finalList.erase(
            std::remove_if(finalList.begin(), finalList.end(),
                [](const AIMinionClient& m) {
                    return IsIgnoredMinion(m.CharacterName().c_str());
                }),
            finalList.end());

        return finalList;
    }

    bool IsValidUnit(const AttackableUnit& unit, float range = 0.0f) const {
        const float effectiveRange = range > 0.0f
            ? range
            : Utils::AutoAttack::GetRealAutoAttackRange(GameObjects::Player(), unit);
        return Extensions::IsValidTarget(unit, effectiveRange);
    }

public:
    OrbwalkerSelector(OrbwalkerBase* orbwalker, Menu* menu)
        : orbwalker_(orbwalker), menu_(menu) {
    }

    AttackableUnit ForceTarget = {};

    std::vector<AIMinionClient> GetEnemyMinions(float range = 0.0f) const {
        std::vector<AIMinionClient> result;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (IsValidUnit(m, range) && !IsIgnoredMinion(m.CharacterName().c_str())) {
                result.push_back(m);
            }
        }
        return result;
    }

    AttackableUnit GetTarget(OrbwalkingMode mode) {
        NS_PROFILE("orb.Selector.GetTarget.total");
        const auto player = GameObjects::Player();
        if (!player.IsValid()) return {};

        if ((mode == OrbwalkingMode::Hybrid || mode == OrbwalkingMode::LaneClear)
            && !MenuBool("prioritizeFarm")) {
            AIHeroClient target;
            {
                NS_PROFILE("orb.TargetSelector.GetTarget");
                target = TargetSelector::Instance()->GetTarget(-1.0f, DamageType::Physical);
            }
            if (target.IsValid() && Utils::AutoAttack::InAutoAttackRange(target)) {
                return target;
            }
        }

        std::vector<AIMinionClient> minions;
        {
            NS_PROFILE("orb.GetMinions");
            minions = (mode != OrbwalkingMode::None)
                ? GetMinions(mode)
                : std::vector<AIMinionClient>();
        }

        if (mode == OrbwalkingMode::LaneClear || mode == OrbwalkingMode::Hybrid
            || mode == OrbwalkingMode::LastHit) {
            {
                NS_PROFILE("orb.GetTarget.sortMinions");
                std::sort(minions.begin(), minions.end(),
                    [](const AIMinionClient& a, const AIMinionClient& b) {
                        return a.Health() < b.Health();
                    });
            }

            for (const auto& minion : minions) {
                if (minion.MaxHealth() <= 10.0f) {
                    if (minion.Health() <= 1.0f) {
                        return minion;
                    }
                } else {
                    float predHealth;
                    {
                        NS_PROFILE("orb.HealthPrediction.GetPrediction");
                        int timeToHit = static_cast<int>(Utils::AutoAttack::GetTimeToHit(minion));
                        predHealth = HealthPrediction::GetPrediction(
                            minion, timeToHit, FarmDelay(), HealthPredictionType::Simulated);
                    }

                    if (predHealth <= 0.0f) {
                        OrbwalkingActionArgs args = {};
                        args.Position = minion.Position();
                        args.Target = minion;
                        args.Process = true;
                        args.Type = OrbwalkingType::NonKillableMinion;
                        orbwalker_->InvokeAction(args);
                    }

                    float aaDamage;
                    {
                        NS_PROFILE("orb.Damage.GetAutoAttackDamage");
                        aaDamage = Damage::GetAutoAttackDamage(
                            AIHeroClient(player.Address()), minion, true);
                    }
                    if (predHealth > 0.0f && predHealth < aaDamage) {
                        return minion;
                    }
                }
            }
        }

        if (ForceTarget.IsValid()
            && Extensions::IsValidTarget(ForceTarget)
            && Utils::AutoAttack::InAutoAttackRange(ForceTarget)) {
            return ForceTarget;
        }

        if (mode == OrbwalkingMode::LaneClear
            && (!MenuBool("prioritizeMinions") || minions.empty())) {
            for (const auto& turret : GameObjects::EnemyTurrets()) {
                if (turret.IsValid() && Extensions::IsValidTarget(turret)
                    && Utils::AutoAttack::InAutoAttackRange(turret)) {
                    return turret;
                }
            }
            for (const auto& inhib : GameObjects::EnemyInhibitors()) {
                if (inhib.IsValid() && Extensions::IsValidTarget(inhib)
                    && Utils::AutoAttack::InAutoAttackRange(inhib)) {
                    return inhib;
                }
            }
            auto nexus = GameObjects::EnemyNexus();
            if (nexus.IsValid() && Extensions::IsValidTarget(nexus)
                && Utils::AutoAttack::InAutoAttackRange(nexus)) {
                return nexus;
            }
        }

        if (mode != OrbwalkingMode::LastHit) {
            auto target = TargetSelector::Instance()->GetTarget(-1.0f, DamageType::Physical);
            if (target.IsValid() && Utils::AutoAttack::InAutoAttackRange(target)) {
                return target;
            }
        }

        if (mode == OrbwalkingMode::LaneClear || mode == OrbwalkingMode::Hybrid) {
            for (const auto& m : minions) {
                if (m.Team() == GameObjectTeam::Neutral) {
                    return m;
                }
            }
        }

        if (mode == OrbwalkingMode::LaneClear || mode == OrbwalkingMode::Hybrid
            || mode == OrbwalkingMode::LastHit) {
            std::vector<AIMinionClient> turretMinions;
            for (const auto& m : minions) {
                if (m.IsMinion() && m.IsUnderAllyTurret()) {
                    turretMinions.push_back(m);
                }
            }

            if (!turretMinions.empty()) {
                auto turretMinion = std::find_if(turretMinions.begin(), turretMinions.end(),
                    [](const AIMinionClient& m) {
                        return HealthPrediction::HasTurretAggro(m);
                    });

                if (turretMinion != turretMinions.end()) {
                    int hpLeftBeforeDie = 0;
                    int hpLeft = 0;
                    int turretAttackCount = 0;

                    auto aggroTurret = HealthPrediction::GetAggroTurret(*turretMinion);
                    if (aggroTurret.IsValid()) {
                        int turretStartTick = HealthPrediction::TurretAggroStartTick(*turretMinion);
                        float turretCastDelay = CoreControl::GetAttackWindup(aggroTurret.Address());
                        float turretMissileSpeed = 1200.0f;
                        float distance = turretMinion->Distance(aggroTurret.Position());
                        float boundingRadius = aggroTurret.BoundingRadius();
                        float travelTime = (distance - boundingRadius) / (turretMissileSpeed + 70.0f);
                        if (travelTime < 0.0f) travelTime = 0.0f;

                        int turretLandTick = turretStartTick
                            + static_cast<int>(turretCastDelay * 1000.0f)
                            + static_cast<int>(1000.0f * travelTime);

                        float turretAttackDelay = CoreControl::GetAttackDelay(aggroTurret.Address());
                        if (turretAttackDelay < 0.1f) turretAttackDelay = 0.833f;

                        for (float i = static_cast<float>(turretLandTick) + 50.0f;
                             i < static_cast<float>(turretLandTick) + (3.0f * turretAttackDelay * 1000.0f) + 50.0f;
                             i += turretAttackDelay * 1000.0f) {
                            int time = static_cast<int>(i) - Variables::TickCount() + (Game::Ping() / 2);
                            if (time < 0) time = 0;
                            float predHp = HealthPrediction::GetPrediction(
                                *turretMinion, time, 70, HealthPredictionType::Simulated);
                            if (predHp > 0.0f) {
                                hpLeft = static_cast<int>(predHp);
                                ++turretAttackCount;
                            } else {
                                hpLeftBeforeDie = hpLeft;
                                hpLeft = 0;
                                break;
                            }
                        }

                        bool hasFarmMinion = false;
                        bool hasNonKillable = false;
                        AIMinionClient farmMinion;
                        AIMinionClient nonKillableMinion;

                        if (hpLeft == 0 && turretAttackCount != 0 && hpLeftBeforeDie != 0) {
                            float aaDamage = Damage::GetAutoAttackDamage(
                                AIHeroClient(player.Address()), *turretMinion, true);
                            int hits = hpLeftBeforeDie / static_cast<int>(aaDamage > 0.0f ? aaDamage : 1.0f);
                            int timeBeforeDie = turretLandTick
                                + static_cast<int>((turretAttackCount + 1) * turretAttackDelay * 1000.0f)
                                - Variables::TickCount();
                            int timeUntilAttackReady = (orbwalker_->LastAutoAttackTick
                                + static_cast<int>(CoreControl::GetAttackDelay(player.Address()) * 1000.0f)
                                > (Variables::TickCount() + (Game::Ping() / 2) + 25))
                                ? orbwalker_->LastAutoAttackTick
                                    + static_cast<int>(CoreControl::GetAttackDelay(player.Address()) * 1000.0f)
                                    - (Variables::TickCount() + (Game::Ping() / 2) + 25)
                                : 0;
                            float timeToLandAttack = Utils::AutoAttack::GetTimeToHit(*turretMinion);

                            if (hits >= 1
                                && (static_cast<float>(hits) * CoreControl::GetAttackDelay(player.Address()) * 1000.0f)
                                    + static_cast<float>(timeUntilAttackReady) + timeToLandAttack
                                    < static_cast<float>(timeBeforeDie)) {
                                farmMinion = *turretMinion;
                                hasFarmMinion = true;
                            } else if (hits >= 1
                                && (static_cast<float>(hits) * CoreControl::GetAttackDelay(player.Address()) * 1000.0f)
                                    + static_cast<float>(timeUntilAttackReady) + timeToLandAttack
                                    > static_cast<float>(timeBeforeDie)) {
                                nonKillableMinion = *turretMinion;
                                hasNonKillable = true;
                            }
                        } else if (hpLeft == 0 && turretAttackCount == 0 && hpLeftBeforeDie == 0) {
                            nonKillableMinion = *turretMinion;
                            hasNonKillable = true;
                        }

                        if (ShouldWaitUnderTurret(hasNonKillable ? &nonKillableMinion : nullptr)) {
                            return {};
                        }

                        if (hasFarmMinion) {
                            return farmMinion;
                        }

                        for (const auto& m : turretMinions) {
                            if (m.NetworkId() != turretMinion->NetworkId()
                                && !HealthPrediction::HasMinionAggro(m)) {
                                int minionHp = static_cast<int>(m.Health());
                                int turretDamage = static_cast<int>(
                                    aggroTurret.GetAutoAttackDamage(m, false));
                                int playerDamage = static_cast<int>(
                                    Damage::GetAutoAttackDamage(AIHeroClient(player.Address()), m, true));
                                if (turretDamage > 0 && (minionHp % turretDamage) > playerDamage) {
                                    return m;
                                }
                            }
                        }
                    }
                } else {
                    if (ShouldWaitUnderTurret()) {
                        return {};
                    }

                    for (const auto& m : turretMinions) {
                        if (!HealthPrediction::HasMinionAggro(m)) {
                            for (const auto& turret : GameObjects::AllyTurrets()) {
                                if (Extensions::IsValidTarget(turret, 950.0f, false, m.Position())) {
                                    int minionHp = static_cast<int>(m.Health());
                                    int turretDamage = static_cast<int>(
                                        turret.GetAutoAttackDamage(m, false));
                                    int playerDamage = static_cast<int>(
                                        Damage::GetAutoAttackDamage(AIHeroClient(player.Address()), m, true));
                                    if (turretDamage > 0 && (minionHp % turretDamage) > playerDamage) {
                                        return m;
                                    }
                                }
                            }
                        }
                    }
                }
                return {};
            }
        }

        if (mode == OrbwalkingMode::LaneClear) {
            if (!ShouldWait()) {
                if (laneClearMinion_.IsValid()
                    && Utils::AutoAttack::InAutoAttackRange(laneClearMinion_)) {
                    if (laneClearMinion_.MaxHealth() <= 10.0f) {
                        return laneClearMinion_;
                    }
                    float predHealth = HealthPrediction::GetPrediction(
                        laneClearMinion_,
                        static_cast<int>(CoreControl::GetAttackDelay(player.Address()) * 1000.0f * kLaneClearWaitTime),
                        FarmDelay(),
                        HealthPredictionType::Simulated);
                    float aaDamage = Damage::GetAutoAttackDamage(
                        AIHeroClient(player.Address()), laneClearMinion_, true);
                    if (predHealth >= 2.0f * aaDamage
                        || std::abs(predHealth - laneClearMinion_.Health()) < std::numeric_limits<float>::epsilon()) {
                        return laneClearMinion_;
                    }
                }

                for (const auto& m : minions) {
                    if (m.Team() == GameObjectTeam::Neutral) continue;
                    if (m.MaxHealth() <= 10.0f) {
                        laneClearMinion_ = m;
                        return m;
                    }
                    float predHealth = HealthPrediction::GetPrediction(
                        m,
                        static_cast<int>(CoreControl::GetAttackDelay(player.Address()) * 1000.0f * kLaneClearWaitTime),
                        FarmDelay(),
                        HealthPredictionType::Simulated);
                    float aaDamage = Damage::GetAutoAttackDamage(
                        AIHeroClient(player.Address()), m, true);
                    if (predHealth >= 2.0f * aaDamage
                        || std::abs(predHealth - m.Health()) < std::numeric_limits<float>::epsilon()) {
                        laneClearMinion_ = m;
                        return m;
                    }
                }
            }
        }

        if (mode == OrbwalkingMode::Combo) {
            if (!minions.empty()) {
                bool enemyHeroesNearby = false;
                float range = Utils::AutoAttack::GetRealAutoAttackRange(player);
                for (const auto& hero : GameObjects::EnemyHeroes()) {
                    if (hero.IsValid() && Extensions::IsValidTarget(hero, range * 2.0f)) {
                        enemyHeroesNearby = true;
                        break;
                    }
                }
                if (!enemyHeroesNearby) {
                    return minions[0];
                }
            }
        }

        return {};
    }

    bool ShouldWait() const {
        NS_PROFILE("orb.ShouldWait.total");
        const auto player = GameObjects::Player();
        if (!player.IsValid()) return false;

        std::vector<AIMinionClient> minions;
        {
            NS_PROFILE("orb.GetEnemyMinions");
            minions = GetEnemyMinions();
        }
        for (const auto& m : minions) {
            float predHealth;
            {
                NS_PROFILE("orb.HealthPrediction.GetPrediction");
                predHealth = HealthPrediction::GetPrediction(
                    m,
                    static_cast<int>(CoreControl::GetAttackDelay(player.Address()) * 1000.0f * kLaneClearWaitTime),
                    FarmDelay(),
                    HealthPredictionType::Simulated);
            }
            float aaDamage;
            {
                NS_PROFILE("orb.Damage.GetAutoAttackDamage");
                aaDamage = Damage::GetAutoAttackDamage(
                    AIHeroClient(player.Address()), m, true);
            }
            if (predHealth < aaDamage) {
                return true;
            }
        }
        return false;
    }

    bool ShouldWaitUnderTurret(const AIMinionClient* noneKillableMinion = nullptr) const {
        const auto player = GameObjects::Player();
        if (!player.IsValid()) return false;

        for (const auto& m : GetEnemyMinions()) {
            if (noneKillableMinion
                && noneKillableMinion->NetworkId() == m.NetworkId()) {
                continue;
            }
            if (!Extensions::IsValidTarget(m)) continue;
            if (!Utils::AutoAttack::InAutoAttackRange(m)) continue;

            float predHealth = HealthPrediction::GetPrediction(
                m,
                static_cast<int>((CoreControl::GetAttackDelay(player.Address()) * 1000.0f)
                    + Utils::AutoAttack::GetTimeToHit(m)),
                FarmDelay(),
                HealthPredictionType::Simulated);
            float aaDamage = Damage::GetAutoAttackDamage(
                AIHeroClient(player.Address()), m, true);
            if (predHealth < aaDamage) {
                return true;
            }
        }
        return false;
    }
};

} // namespace SDK
