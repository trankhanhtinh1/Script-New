#pragma once

#include "../../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::Yasuo {

inline bool IsYasuoQ3Name(const std::string& name) {
    return !name.empty() && _stricmp(name.c_str(), "YasuoQ3Wrapper") == 0;
}

inline bool IsYasuoQ2Name(const std::string& name) {
    return !name.empty() && _stricmp(name.c_str(), "YasuoQ2Wrapper") == 0;
}

inline bool IsYasuoQ1Name(const std::string& name) {
    return !name.empty() && _stricmp(name.c_str(), "YasuoQ1Wrapper") == 0;
}

inline std::string QSpellName() {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    const auto q = player.Spellbook().GetSpell(SpellSlot::Q);
    return q.IsValid() ? q.Name() : std::string();
}

inline bool HaveQ2() {
    return IsYasuoQ2Name(QSpellName());
}

inline bool HaveQ1() {
    const std::string name = QSpellName();
    return IsYasuoQ1Name(name) || (!IsYasuoQ2Name(name) && !IsYasuoQ3Name(name));
}

inline bool HaveQ3() {
    const auto player = Player();
    return player.IsValid() &&
           (IsYasuoQ3Name(QSpellName()) || player.HasBuff("YasuoQ3W"));
}

inline Vector3 Grounded(Vector3 position) {
    if (!position.IsZero()) {
        position.y = NavMesh::GetHeightForPosition(position);
    }
    return position;
}

inline Vector3 PosAfterE(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return {};
    }

    if (target.DistanceToPlayer() > 410.0f) {
        return Grounded(target.Position().Extend(player.Position(), -50.0f));
    }

    return Grounded(player.Position().Extend(target.Position(), 475.0f));
}

inline bool UnderTower(const Vector3& position) {
    const auto player = Player();
    const float extraRadius = player.IsValid() ? player.BoundingRadius() : 65.0f;
    const float range = 850.0f + extraRadius;
    const float rangeSqr = range * range;

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*for (const auto& turret : GameObjects::EnemyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) {
            continue;
        }
        if (turret.Position().DistanceSqr2D(position) <= rangeSqr) {
            return true;
        }
    }*/

    for (const auto& spawn : GameObjects::EnemySpawnPoints()) {
        if (spawn.IsValid() && spawn.Position().DistanceSqr2D(position) <= rangeSqr) {
            return true;
        }
    }
    return false;
}

inline bool CanE(const AIBaseClient& target) {
    return ValidTarget(target) && !target.HasBuff("YasuoE");
}

inline bool AllowDashTo(const Vector3& position, bool allowTower) {
    if (position.IsZero() || NavMesh::IsWall(position)) {
        return false;
    }
    return allowTower || !UnderTower(position);
}

inline bool AllowDashTo(const AIBaseClient& target, bool allowTower) {
    return target.IsValid() && AllowDashTo(PosAfterE(target), allowTower);
}

inline bool IsAirborne(const AIHeroClient& target) {
    if (!ValidHeroTarget(target)) {
        return false;
    }

    const uintptr_t address = target.Address();
    return CoreBuffs::HasBuffType(address, 29) ||
           CoreBuffs::HasBuffType(address, SDK::Prediction::BuffType::Knockup) ||
           CoreBuffs::HasBuffType(address, SDK::Prediction::BuffType::Knockback);
}

inline float AirborneRemaining(const AIHeroClient& target) {
    if (!ValidHeroTarget(target)) {
        return 0.0f;
    }

    const float gameTime = CoreBuffs::ResolveGameTime();
    uintptr_t buffs[256] = {};
    const int count = CoreBuffs::Enumerate(target.Address(), buffs, 256);
    float remaining = 0.0f;

    for (int i = 0; i < count; ++i) {
        CoreBuffs::BuffRef buff{ buffs[i] };
        if (!buff.IsActive(gameTime)) {
            continue;
        }

        const int type = buff.GetType();
        if (type == 29 ||
            type == SDK::Prediction::BuffType::Knockup ||
            type == SDK::Prediction::BuffType::Knockback) {
            remaining = std::max(remaining, buff.GetRemainingTime(gameTime));
        }
    }

    return remaining;
}

inline Vector3 PredictedPosition(const Spell& spell, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return {};
    }

    if (player.IsDashing()) {
        return target.Position();
    }

    const auto prediction = spell.GetPrediction(target);
    const Vector3 castPosition = prediction.GetCastPosition();
    return castPosition.IsZero() ? target.Position() : castPosition;
}

inline int CountEnemyHeroesNear(const Vector3& position, float range) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) &&
            enemy.Position().DistanceSqr2D(position) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

inline AIBaseClient BestDashObjectNear(const Vector3& desired,
                                       float eRange,
                                       float desiredRange,
                                       bool preferFarthest,
                                       bool allowTower,
                                       bool includeHeroes = true) {
    const auto player = Player();
    if (!player.IsValid() || desired.IsZero()) {
        return {};
    }

    AIBaseClient best;

    auto consider = [&](const AIBaseClient& unit) {
        if (!ValidTarget(unit, eRange) || !CanE(unit)) {
            return;
        }

        if (!includeHeroes && unit.IsHero()) {
            return;
        }

        const Vector3 after = PosAfterE(unit);
        if (!AllowDashTo(after, allowTower)) {
            return;
        }

        const float afterDistance = after.Distance2D(desired);
        if (afterDistance > desiredRange && afterDistance >= player.Position().Distance2D(desired)) {
            return;
        }

        if (!best.IsValid()) {
            best = unit;
            return;
        }

        const bool unitIsHero = unit.IsHero();
        const bool bestIsHero = best.IsHero();
        if (unitIsHero != bestIsHero) {
            if (!unitIsHero) {
                best = unit;
            }
            return;
        }

        const float unitDist = std::max(50.0f, afterDistance);
        const float bestDist = std::max(50.0f, PosAfterE(best).Distance2D(desired));
        if (std::abs(unitDist - bestDist) > 0.001f) {
            if (unitDist < bestDist) {
                best = unit;
            }
            return;
        }

        const float unitPlayerDist = unit.DistanceToPlayer();
        const float bestPlayerDist = best.DistanceToPlayer();

        if (preferFarthest) {
            if (unitPlayerDist > bestPlayerDist) {
                best = unit;
            }
        }
        else {
            if (unitPlayerDist < bestPlayerDist) {
                best = unit;
            }
        }
    };

    if (includeHeroes) {
        for (const auto& hero : GameObjects::EnemyHeroes()) {
            consider(hero);
        }
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        consider(minion);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        consider(monster);
    }

    return best;
}

inline std::vector<AIBaseClient> ClearUnits(float range) {
    std::vector<AIBaseClient> units;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, range)) {
            units.push_back(AIBaseClient(minion.Handle()));
        }
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (ValidTarget(monster, range) && monster.IsJungle() && !monster.IsPlant()) {
            units.push_back(AIBaseClient(monster.Handle()));
        }
    }
    return units;
}

inline AIBaseClient BestLowHealthClearUnit(float range, float damage) {
    AIBaseClient best;
    float bestHealth = FLT_MAX;

    for (const auto& unit : ClearUnits(range)) {
        if (unit.Health() <= damage && unit.Health() < bestHealth) {
            best = unit;
            bestHealth = unit.Health();
        }
    }

    return best;
}

} // namespace Plugins::KuroAIO::Yasuo
