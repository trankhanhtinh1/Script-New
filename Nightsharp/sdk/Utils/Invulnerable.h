#pragma once

#include "Logging.h"
#include "../Core/Objects.h"
#include "../Enumerations/DamageType.h"

#include <algorithm>
#include <functional>
#include <new>
#include <string>
#include <vector>

namespace SDK::Utils {

struct InvulnerableEntry {
    std::string BuffName = {};
    std::string ChampionName = {};
    std::function<bool(const AIBaseClient&, DamageType)> CheckFunction = {};
    bool HasDamageType = false;
    DamageType EntryDamageType = DamageType::True;
    bool IsShield = false;
    int MinHealthPercent = 0;
};

namespace Invulnerable {

namespace detail {
    inline std::vector<InvulnerableEntry>*& Entries() {
        static auto* storage = new(std::nothrow) std::vector<InvulnerableEntry>();
        return storage;
    }

    inline bool g_initialized = false;
}

inline void Initialize() {
    if (detail::g_initialized) {
        return;
    }
    detail::g_initialized = true;

    auto* entries = detail::Entries();
    if (!entries || !entries->empty()) {
        return;
    }

    entries->push_back({ "FerociousHowl", "Alistar", [](const AIBaseClient&, DamageType) {
        return ObjectManager::Player().CountEnemyHeroesInRange(ObjectManager::Player().GetRealAutoAttackRange()) > 1;
    }, false, DamageType::True, false, 0 });
    entries->push_back({ "FioraW", "Fiora", {}, false, DamageType::True, false, 0 });
    entries->push_back({ "JaxCounterStrike", "Jax", {}, true, DamageType::Physical, false, 0 });
    entries->push_back({ "KayleR", "Kayle", {}, false, DamageType::True, true, 0 });
    entries->push_back({ "KindredRNoDeathBuff", "Kindred", [](const AIBaseClient& target, DamageType) {
        return target.HealthPercent() <= 10.0f;
    }, false, DamageType::True, false, 10 });
    entries->push_back({ "malzaharpassiveshield", "Malzahar", {}, true, DamageType::Magical, true, 0 });
    entries->push_back({ "MorganaE", "Morgana", {}, true, DamageType::Magical, true, 0 });
    entries->push_back({ "SivirE", "Sivir", {}, false, DamageType::True, true, 0 });
    entries->push_back({ "TaricR", "Taric", {}, false, DamageType::True, false, 0 });
    entries->push_back({ "XinZhaoRRangedImmunity", "XinZhao", {}, true, DamageType::Physical, false, 0 });
    entries->push_back({ "bansheesveil", "", {}, true, DamageType::Magical, true, 0 });
    entries->push_back({ "itemmagekillerveil", "", {}, true, DamageType::Magical, true, 0 });
}

inline void Register(const InvulnerableEntry& entry) {
    Initialize();
    auto* entries = detail::Entries();
    if (!entries || entry.BuffName.empty()) {
        return;
    }

    const auto it = std::find_if(entries->begin(), entries->end(), [&entry](const InvulnerableEntry& current) {
        return _stricmp(current.BuffName.c_str(), entry.BuffName.c_str()) == 0;
    });

    if (it == entries->end()) {
        entries->push_back(entry);
    }
}

inline void Deregister(const InvulnerableEntry& entry) {
    auto* entries = detail::Entries();
    if (!entries) {
        return;
    }

    entries->erase(std::remove_if(entries->begin(), entries->end(), [&entry](const InvulnerableEntry& current) {
        return _stricmp(current.BuffName.c_str(), entry.BuffName.c_str()) == 0;
    }), entries->end());
}

inline const InvulnerableEntry* GetItem(const std::string& buffName) {
    Initialize();
    auto* entries = detail::Entries();
    if (!entries) {
        return nullptr;
    }

    const auto it = std::find_if(entries->begin(), entries->end(), [&buffName](const InvulnerableEntry& current) {
        return _stricmp(current.BuffName.c_str(), buffName.c_str()) == 0;
    });

    return it == entries->end() ? nullptr : &*it;
}

inline std::vector<InvulnerableEntry> Entries() {
    Initialize();
    return detail::Entries() ? *detail::Entries() : std::vector<InvulnerableEntry>{};
}

inline bool Check(const AIHeroClient& hero,
                  DamageType damageType = DamageType::True,
                  bool ignoreShields = true,
                  float damage = -1.0f) {
    Initialize();
    if (!hero.IsValid()) {
        return false;
    }

    if (hero.IsInvulnerable()) {
        return true;
    }

    auto* entries = detail::Entries();
    if (!entries) {
        return false;
    }

    for (const auto& entry : *entries) {
        if (!entry.ChampionName.empty() && _stricmp(entry.ChampionName.c_str(), hero.CharacterName().c_str()) != 0) {
            continue;
        }
        if (entry.HasDamageType && entry.EntryDamageType != damageType) {
            continue;
        }
        if (!hero.HasBuff(entry.BuffName.c_str())) {
            continue;
        }
        if (ignoreShields && entry.IsShield) {
            continue;
        }

        bool passed = true;
        if (entry.CheckFunction) {
            try {
                passed = entry.CheckFunction(hero, damageType);
            } catch (...) {
                Logging::Write(true, true, "Invulnerable::Check")(LogLevel::Error, "Entry check threw.");
                passed = false;
            }
        }

        if (!passed) {
            continue;
        }

        if (damage > 0.0f && entry.MinHealthPercent > 0) {
            const float hpAfter = hero.Health() - damage;
            const float hpPercent = hero.MaxHealth() > 0.0f ? (hpAfter / hero.MaxHealth()) * 100.0f : 0.0f;
            if (hpPercent >= entry.MinHealthPercent) {
                continue;
            }
        }

        return true;
    }

    return false;
}

} // namespace Invulnerable
} // namespace SDK::Utils

