#pragma once
#include "sdk/EzEvade/EvadeSpells/EvadeSpellDatabase.h"
#include "sdk/EzEvade/Spells/SpellDatabase.h"
#include "sdk/GameObjects/GameObjects.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Plugins::EvadeMenu {

    struct ChampionEvadeSpellEntry {
        std::string InternalName;
        std::string DisplayName;
        int DefaultDangerLevel = 3;
    };

    struct SkillshotMenuEntry {
        std::string InternalName;
        std::string DisplayName;
        int DefaultDangerLevel = 1;
        bool DefaultDraw = false;
    };

    struct TrapTemplate {
        const char* Champion;
        const char* Slot;
        int DefaultDangerLevel;
        int DefaultExtraDistance;
    };

    inline std::string NormalizeId(const std::string& value) {
        std::string result;
        result.reserve(value.size());

        for (unsigned char ch : value) {
            if (std::isalnum(ch)) {
                result.push_back(static_cast<char>(std::tolower(ch)));
                continue;
            }

            if (result.empty() || result.back() == '_') {
                continue;
            }
            result.push_back('_');
        }

        while (!result.empty() && result.back() == '_') {
            result.pop_back();
        }

        if (result.empty()) {
            return "entry";
        }

        return result;
    }

    inline int SlotSortKey(EzEvade::SpellSlotId slot) {
        return static_cast<int>(slot);
    }

    inline const char* SlotLabel(EzEvade::SpellSlotId slot) {
        switch (slot) {
        case EzEvade::SpellSlotId::Q: return "Q";
        case EzEvade::SpellSlotId::W: return "W";
        case EzEvade::SpellSlotId::E: return "E";
        case EzEvade::SpellSlotId::R: return "R";
        case EzEvade::SpellSlotId::F: return "D";
        case EzEvade::SpellSlotId::T: return "F";
        default: return "?";
        }
    }

    inline std::vector<TrapTemplate> GetTrapTemplates() {
        return {
            { "Teemo", "R", 3, 20 },
            { "Caitlyn", "W", 4, 30 },
            { "Jhin", "E", 3, 25 },
            { "Shaco", "W", 4, 35 },
        };
    }

    inline std::vector<TrapTemplate> GetEnemyTrapTemplates() {
        std::vector<TrapTemplate> result;
        auto traps = GetTrapTemplates();

        for (const auto& trap : traps) {
            auto it = std::find_if(SDK::GameObjects::EnemyHeroes.begin(), SDK::GameObjects::EnemyHeroes.end(),
                [&](const SDK::GameObject& enemy) {
                    return enemy.IsValid() && _stricmp(enemy.GetChampionName().c_str(), trap.Champion) == 0;
                });

            if (it != SDK::GameObjects::EnemyHeroes.end()) {
                result.push_back(trap);
            }
        }

        return result;
    }

    inline std::vector<ChampionEvadeSpellEntry> GetChampionEvadeSpellEntries() {
        std::vector<ChampionEvadeSpellEntry> entries;

        if (!SDK::GameObjects::Player.IsValid()) {
            return entries;
        }

        const std::string playerChampion = SDK::GameObjects::Player.GetChampionName();
        if (playerChampion.empty()) {
            return entries;
        }

        auto spells = EzEvade::GetEvadeSpellsForChampion(playerChampion);
        std::sort(spells.begin(), spells.end(), [](const EzEvade::EvadeSpellData* left, const EzEvade::EvadeSpellData* right) {
            if (SlotSortKey(left->spellKey) != SlotSortKey(right->spellKey)) {
                return SlotSortKey(left->spellKey) < SlotSortKey(right->spellKey);
            }
            if (left->dangerlevel != right->dangerlevel) {
                return left->dangerlevel > right->dangerlevel;
            }
            return left->spellName < right->spellName;
        });

        std::unordered_set<std::string> seen;
        for (const auto* spell : spells) {
            if (spell == nullptr || spell->spellName.empty()) {
                continue;
            }

            const std::string dedupeKey = NormalizeId(std::string(SlotLabel(spell->spellKey)) + "_" + spell->spellName);
            if (!seen.insert(dedupeKey).second) {
                continue;
            }

            ChampionEvadeSpellEntry entry;
            entry.InternalName = dedupeKey;
            entry.DisplayName = std::string(SlotLabel(spell->spellKey)) + " - " + spell->name;
            entry.DefaultDangerLevel = std::clamp(spell->dangerlevel, 1, 5);
            entries.push_back(std::move(entry));
        }

        return entries;
    }

    inline std::string GetSkillshotMenuId(const EzEvade::SpellData& spell) {
        return NormalizeId(spell.spellName);
    }

    inline std::string GetSkillshotDisplayName(const EzEvade::SpellData& spell) {
        if (!spell.displayName.empty()) {
            return spell.displayName;
        }
        return spell.spellName;
    }

    inline std::vector<std::pair<std::string, std::vector<SkillshotMenuEntry>>> GetEnemySkillshotEntries() {
        std::vector<std::pair<std::string, std::vector<SkillshotMenuEntry>>> result;

        std::vector<std::string> champions;
        champions.reserve(SDK::GameObjects::EnemyHeroes.size());

        for (const auto& enemy : SDK::GameObjects::EnemyHeroes) {
            if (!enemy.IsValid()) {
                continue;
            }

            const std::string championName = enemy.GetChampionName();
            if (championName.empty()) {
                continue;
            }

            if (std::find(champions.begin(), champions.end(), championName) == champions.end()) {
                champions.push_back(championName);
            }
        }

        std::sort(champions.begin(), champions.end());

        for (const auto& champion : champions) {
            auto spells = EzEvade::GetSpellsForChampion(champion);
            std::vector<SkillshotMenuEntry> entries;
            std::unordered_set<std::string> seen;

            std::sort(spells.begin(), spells.end(), [](const EzEvade::SpellData* left, const EzEvade::SpellData* right) {
                if (left->dangerLevel != right->dangerLevel) {
                    return left->dangerLevel > right->dangerLevel;
                }
                return left->spellName < right->spellName;
            });

            for (const auto* spell : spells) {
                if (spell == nullptr || !spell->IsSkillshot() || spell->spellName.empty()) {
                    continue;
                }

                const std::string id = GetSkillshotMenuId(*spell);
                if (!seen.insert(id).second) {
                    continue;
                }

                SkillshotMenuEntry entry;
                entry.InternalName = id;
                entry.DisplayName = GetSkillshotDisplayName(*spell);
                entry.DefaultDangerLevel = std::clamp(spell->dangerLevel, 1, 5);
                entry.DefaultDraw = true;
                entries.push_back(std::move(entry));
            }

            result.push_back({ champion, std::move(entries) });
        }

        return result;
    }

} // namespace Plugins::EvadeMenu
