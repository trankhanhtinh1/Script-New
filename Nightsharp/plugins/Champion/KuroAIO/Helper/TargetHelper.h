#pragma once

#include "../../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <vector>

namespace Plugins::KuroAIO {

inline AIHeroClient Player() {
    return ObjectManager::Player();
}

inline std::string GetObjectName(const GameObject& object) {
    if (!object.IsValid()) return {};
    char nameBuf[96] = {};
    if (::Core::Objects::ReadName(object.Address(), nameBuf, sizeof(nameBuf)) && nameBuf[0]) {
        return nameBuf;
    }
    return {};
}

inline std::string GetObjectCharacterName(const GameObject& object) {
    if (!object.IsValid()) return {};
    std::string name = object.CharacterName();
    if (!name.empty()) return name;
    char nameBuf[96] = {};
    if (::Core::Objects::ReadCharacterName(object.Address(), nameBuf, sizeof(nameBuf)) && nameBuf[0]) {
        return nameBuf;
    }
    return {};
}

inline bool Recent(int tick, int windowMs) {
    const int now = SDK::Variables::TickCount();
    return tick > 0 && now >= tick && now - tick <= windowMs;
}

inline bool EqualsIgnoreCase(const char* left, const char* right) {
    return left && right && left[0] && right[0] && _stricmp(left, right) == 0;
}

inline bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f && unit.IsTargetable();
}

inline bool ValidTarget(const AIBaseClient& unit, float range = FLT_MAX) {
    return ValidUnit(unit) && Extensions::IsValidTarget(unit, range, true);
}

inline bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

inline std::vector<AIHeroClient> EnemyHeroes(float range) {
    std::vector<AIHeroClient> result;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy, range)) {
            result.push_back(enemy);
        }
    }
    return result;
}

inline std::vector<AIHeroClient> EnemyHeroesByHealth(float range) {
    auto result = EnemyHeroes(range);
    std::sort(result.begin(), result.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
        return a.Health() < b.Health();
    });
    return result;
}

inline AIHeroClient GetTarget(float range, DamageType damageType) {
    if (auto* selector = SDK::TargetSelector::Instance()) {
        const auto selected = selector->GetTarget(range, damageType);
        if (ValidHeroTarget(selected, range)) {
            return selected;
        }
    }

    const auto enemies = EnemyHeroesByHealth(range);
    return enemies.empty() ? AIHeroClient() : enemies.front();
}

inline AIHeroClient GetPhysicalTarget(float range) {
    return GetTarget(range, DamageType::Physical);
}

inline AIHeroClient GetMagicalTarget(float range) {
    return GetTarget(range, DamageType::Magical);
}

} // namespace Plugins::KuroAIO
