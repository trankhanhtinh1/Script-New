#pragma once
#include "sdk/EzEvade/SpecialSpells/SpecialSpellRegistry.h"

#include "sdk/EzEvade/SpecialSpells/Ahri.h"
#include "sdk/EzEvade/SpecialSpells/AllChampions.h"
#include "sdk/EzEvade/SpecialSpells/Ashe.h"
#include "sdk/EzEvade/SpecialSpells/Azir.h"
#include "sdk/EzEvade/SpecialSpells/Darius.h"
#include "sdk/EzEvade/SpecialSpells/Ekko.h"
#include "sdk/EzEvade/SpecialSpells/Fizz.h"
#include "sdk/EzEvade/SpecialSpells/Graves.h"
#include "sdk/EzEvade/SpecialSpells/Heimerdinger.h"
#include "sdk/EzEvade/SpecialSpells/JarvanIV.h"
#include "sdk/EzEvade/SpecialSpells/Jayce.h"
#include "sdk/EzEvade/SpecialSpells/Jinx.h"
#include "sdk/EzEvade/SpecialSpells/Lucian.h"
#include "sdk/EzEvade/SpecialSpells/Lulu.h"
#include "sdk/EzEvade/SpecialSpells/Lux.h"
#include "sdk/EzEvade/SpecialSpells/Malzahar.h"
#include "sdk/EzEvade/SpecialSpells/Orianna.h"
#include "sdk/EzEvade/SpecialSpells/Sion.h"
#include "sdk/EzEvade/SpecialSpells/Syndra.h"
#include "sdk/EzEvade/SpecialSpells/Taric.h"
#include "sdk/EzEvade/SpecialSpells/Twitch.h"
#include "sdk/EzEvade/SpecialSpells/Viktor.h"
#include "sdk/EzEvade/SpecialSpells/Xerath.h"
#include "sdk/EzEvade/SpecialSpells/Yasuo.h"
#include "sdk/EzEvade/SpecialSpells/Yorick.h"
#include "sdk/EzEvade/SpecialSpells/Zed.h"
#include "sdk/EzEvade/SpecialSpells/Ziggs.h"
#include "sdk/EzEvade/SpecialSpells/Zilean.h"

#include <memory>
#include <unordered_map>

namespace EzEvade {
namespace SpecialSpells {

inline std::unordered_map<std::string, std::unique_ptr<ChampionPlugin>>& GetSpecialSpellPlugins() {
    static std::unordered_map<std::string, std::unique_ptr<ChampionPlugin>> s_plugins;
    return s_plugins;
}

inline bool& IsSpecialSpellPluginLoaded() {
    static bool s_loaded = false;
    return s_loaded;
}

inline std::unique_ptr<ChampionPlugin> CreateChampionPlugin(const std::string& championName) {
    if (_stricmp(championName.c_str(), "Ahri") == 0) return std::make_unique<Ahri>();
    if (_stricmp(championName.c_str(), "Ashe") == 0) return std::make_unique<Ashe>();
    if (_stricmp(championName.c_str(), "Azir") == 0) return std::make_unique<Azir>();
    if (_stricmp(championName.c_str(), "Darius") == 0) return std::make_unique<Darius>();
    if (_stricmp(championName.c_str(), "Ekko") == 0) return std::make_unique<Ekko>();
    if (_stricmp(championName.c_str(), "Fizz") == 0) return std::make_unique<Fizz>();
    if (_stricmp(championName.c_str(), "Graves") == 0) return std::make_unique<Graves>();
    if (_stricmp(championName.c_str(), "Heimerdinger") == 0) return std::make_unique<Heimerdinger>();
    if (_stricmp(championName.c_str(), "JarvanIV") == 0) return std::make_unique<JarvanIV>();
    if (_stricmp(championName.c_str(), "Jayce") == 0) return std::make_unique<Jayce>();
    if (_stricmp(championName.c_str(), "Jinx") == 0) return std::make_unique<Jinx>();
    if (_stricmp(championName.c_str(), "Lucian") == 0) return std::make_unique<Lucian>();
    if (_stricmp(championName.c_str(), "Lulu") == 0) return std::make_unique<Lulu>();
    if (_stricmp(championName.c_str(), "Lux") == 0) return std::make_unique<Lux>();
    if (_stricmp(championName.c_str(), "Malzahar") == 0) return std::make_unique<Malzahar>();
    if (_stricmp(championName.c_str(), "Orianna") == 0) return std::make_unique<Orianna>();
    if (_stricmp(championName.c_str(), "Sion") == 0) return std::make_unique<Sion>();
    if (_stricmp(championName.c_str(), "Syndra") == 0) return std::make_unique<Syndra>();
    if (_stricmp(championName.c_str(), "Taric") == 0) return std::make_unique<Taric>();
    if (_stricmp(championName.c_str(), "Twitch") == 0) return std::make_unique<Twitch>();
    if (_stricmp(championName.c_str(), "Viktor") == 0) return std::make_unique<Viktor>();
    if (_stricmp(championName.c_str(), "Xerath") == 0) return std::make_unique<Xerath>();
    if (_stricmp(championName.c_str(), "Yasuo") == 0) return std::make_unique<Yasuo>();
    if (_stricmp(championName.c_str(), "Yorick") == 0) return std::make_unique<Yorick>();
    if (_stricmp(championName.c_str(), "Zed") == 0) return std::make_unique<Zed>();
    if (_stricmp(championName.c_str(), "Ziggs") == 0) return std::make_unique<Ziggs>();
    if (_stricmp(championName.c_str(), "Zilean") == 0) return std::make_unique<Zilean>();
    return nullptr;
}

inline void LoadSpecialSpellPlugins(bool devMode) {
    auto& plugins = GetSpecialSpellPlugins();
    if (plugins.find("AllChampions") == plugins.end()) {
        plugins["AllChampions"] = std::make_unique<AllChampions>();
    }

    const auto& heroes = devMode ? SDK::GameObjects::AllHeroes : SDK::GameObjects::EnemyHeroes;
    for (const auto& hero : heroes) {
        if (!hero.IsValid()) {
            continue;
        }

        const std::string championName = hero.GetChampionName();
        if (championName.empty()) {
            continue;
        }

        if (plugins.find(championName) != plugins.end()) {
            continue;
        }

        auto plugin = CreateChampionPlugin(championName);
        if (plugin) {
            plugins[championName] = std::move(plugin);
        }
    }

    IsSpecialSpellPluginLoaded() = true;
}

inline void LoadSpecialSpell(SpellData& spellData) {
    auto& plugins = GetSpecialSpellPlugins();

    auto it = plugins.find(spellData.charName);
    if (it != plugins.end() && it->second) {
        it->second->LoadSpecialSpell(spellData);
    }

    auto allIt = plugins.find("AllChampions");
    if (allIt != plugins.end() && allIt->second) {
        allIt->second->LoadSpecialSpell(spellData);
    }
}

} // namespace SpecialSpells
} // namespace EzEvade
