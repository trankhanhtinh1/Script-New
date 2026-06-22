#pragma once

#include "../../Core/CoreBuffs.h"
#include "../Core/Objects.h"
#include "../Enumerations/DamageType.h"
#include "../GameObjects/ObjectManager.h"
#include "Logging.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace SDK::Core::Utils {

struct InvulnerableEntry {
    using CheckFunctionType = std::function<bool(const AIBaseClient&, DamageType)>;

    std::string BuffName;
    std::string ChampionName;
    CheckFunctionType CheckFunction;
    std::optional<DamageType> DamageTypeValue;
    bool IsShield = false;
    int MinHealthPercent = 0;

    InvulnerableEntry() = default;
    explicit InvulnerableEntry(std::string buffName)
        : BuffName(std::move(buffName)) {}
};

class Invulnerable {
public:
    static std::vector<InvulnerableEntry>& Entries() {
        EnsureInitialized();
        return EntriesStorage();
    }

    static bool Check(const AIHeroClient& hero,
                      DamageType damageType = DamageType::True,
                      bool ignoreShields = true,
                      float damage = -1.0f) {
        EnsureInitialized();
        if (!hero.IsValid()) {
            return false;
        }

        constexpr int kBuffTypeInvulnerability = 18; // EnsoulSharp.BuffType.Invulnerability.
        if (CoreBuffs::HasBuffType(hero.Address(), kBuffTypeInvulnerability) || hero.IsInvulnerable()) {
            return true;
        }

        const std::string heroName = ToLower(hero.CharacterName());
        for (const auto& entry : EntriesStorage()) {
            if (!entry.ChampionName.empty() && ToLower(entry.ChampionName) != heroName) {
                continue;
            }
            if (entry.DamageTypeValue && *entry.DamageTypeValue != damageType) {
                continue;
            }
            if (!hero.HasBuff(entry.BuffName.c_str())) {
                continue;
            }
            if (ignoreShields && entry.IsShield) {
                continue;
            }
            if (entry.CheckFunction && !ExecuteCheckFunction(entry, hero, damageType)) {
                continue;
            }
            if (damage > 0.0f && entry.MinHealthPercent > 0 && hero.MaxHealth() > 0.0f) {
                const float healthAfter = ((hero.Health() - damage) / hero.MaxHealth()) * 100.0f;
                if (healthAfter >= static_cast<float>(entry.MinHealthPercent)) {
                    continue;
                }
            }
            return true;
        }

        return false;
    }

    static void Deregister(const InvulnerableEntry& entry) {
        auto& entries = Entries();
        entries.erase(
            std::remove_if(entries.begin(), entries.end(), [&](const InvulnerableEntry& current) {
                return EqualsInsensitive(current.BuffName, entry.BuffName);
            }),
            entries.end());
    }

    static InvulnerableEntry* GetItem(const std::string& buffName) {
        auto& entries = Entries();
        const auto it = std::find_if(entries.begin(), entries.end(), [&](const InvulnerableEntry& entry) {
            return EqualsInsensitive(entry.BuffName, buffName);
        });
        return it != entries.end() ? &(*it) : nullptr;
    }

    static void Register(const InvulnerableEntry& entry) {
        if (entry.BuffName.empty() || GetItem(entry.BuffName)) {
            return;
        }
        EntriesStorage().push_back(entry);
    }

private:
    static std::vector<InvulnerableEntry>& EntriesStorage() {
        static std::vector<InvulnerableEntry> entries;
        return entries;
    }

    static bool& Initialized() {
        static bool initialized = false;
        return initialized;
    }

    static void EnsureInitialized() {
        if (Initialized()) {
            return;
        }
        Initialized() = true;

        auto& entries = EntriesStorage();
        entries.push_back(Make("FerociousHowl", "Alistar", std::nullopt, false, 0, [](const AIBaseClient&, DamageType) {
            const auto player = SDK::ObjectManager::Player();
            return player.IsValid() && player.CountEnemyHeroesInRange(player.AttackRange() + player.BoundingRadius()) > 1;
        }));
        entries.push_back(Make("FioraW", "Fiora"));
        entries.push_back(Make("JaxCounterStrike", "Jax", DamageType::Physical));
        entries.push_back(Make("KayleR", "", std::nullopt, true));
        entries.push_back(Make("KindredRNoDeathBuff", "", std::nullopt, false, 10, [](const AIBaseClient& target, DamageType) {
            return target.HealthPercent() <= 10.0f;
        }));
        entries.push_back(Make("malzaharpassiveshield", "Malzahar", DamageType::Magical, true));
        entries.push_back(Make("Meditate", "MasterYi", std::nullopt, false, 0, [](const AIBaseClient&, DamageType) {
            const auto player = SDK::ObjectManager::Player();
            return player.IsValid() && player.CountEnemyHeroesInRange(player.AttackRange() + player.BoundingRadius()) > 1;
        }));
        entries.push_back(Make("MorganaE", "", DamageType::Magical, true));
        entries.push_back(Make("NocturneShroudofDarkness", "Nocturne"));
        entries.push_back(Make("ShenWBuff", "", DamageType::Physical));
        entries.push_back(Make("SivirE", "Sivir", std::nullopt, true));
        entries.push_back(Make("TaricR"));
        entries.push_back(Make("UndyingRage", "Tryndamere", std::nullopt, false, 0, [](const AIBaseClient& target, DamageType) {
            // TODO(Spellbook parity): EnsoulSharp scales this threshold by Tryndamere R level.
            // NightSharp currently exposes no spell level reader, so use the highest threshold.
            return target.Health() <= 70.0f;
        }));
        entries.push_back(Make("XinZhaoRRangedImmunity", "XinZhao", DamageType::Physical));
        entries.push_back(Make("bansheesveil", "", DamageType::Magical, true));
        entries.push_back(Make("itemmagekillerveil", "", DamageType::Magical, true));
    }

    static InvulnerableEntry Make(const char* buffName,
                                  const char* championName = "",
                                  std::optional<DamageType> damageType = std::nullopt,
                                  bool isShield = false,
                                  int minHealthPercent = 0,
                                  InvulnerableEntry::CheckFunctionType checkFunction = {}) {
        InvulnerableEntry entry(buffName ? buffName : "");
        entry.ChampionName = championName ? championName : "";
        entry.DamageTypeValue = damageType;
        entry.IsShield = isShield;
        entry.MinHealthPercent = minHealthPercent;
        entry.CheckFunction = std::move(checkFunction);
        return entry;
    }

    static bool ExecuteCheckFunction(const InvulnerableEntry& entry,
                                     const AIHeroClient& hero,
                                     DamageType damageType) {
        try {
            return entry.CheckFunction ? entry.CheckFunction(hero, damageType) : true;
        } catch (...) {
            Logging::Write()(LogLevel::Error, "Invulnerable check function crashed.");
            return false;
        }
    }

    static std::string ToLower(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    }

    static bool EqualsInsensitive(const std::string& lhs, const std::string& rhs) {
        return ToLower(lhs) == ToLower(rhs);
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using Invulnerable = ::SDK::Core::Utils::Invulnerable;
    using InvulnerableEntry = ::SDK::Core::Utils::InvulnerableEntry;
} // namespace SDK::Utils
