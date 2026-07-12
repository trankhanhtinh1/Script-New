#pragma once

#include <array>
#include <cstring>
#include <string>

namespace Plugins::TargetSelectorImpulseData {

struct PriorityEntry {
    const char* Alias;
    int Value;
};

inline constexpr std::array<PriorityEntry, 173> Entries{{
    {"Ahri", 5}, {"Akshan", 5}, {"Anivia", 5}, {"Annie", 5}, {"Aphelios", 5},
    {"Ashe", 5}, {"AurelionSol", 5}, {"Aurora", 5}, {"Azir", 5}, {"Brand", 5},
    {"Caitlyn", 5}, {"Cassiopeia", 5}, {"Corki", 5}, {"Draven", 5}, {"Ezreal", 5},
    {"Graves", 5}, {"Hwei", 5}, {"Jhin", 5}, {"Jinx", 5}, {"Kaisa", 5},
    {"Kalista", 5}, {"Karma", 5}, {"Karthus", 5}, {"Katarina", 5}, {"Kennen", 5},
    {"Kindred", 5}, {"KogMaw", 5}, {"Leblanc", 5}, {"Lucian", 5}, {"Lux", 5},
    {"Malzahar", 5}, {"MasterYi", 5}, {"Mel", 5}, {"MissFortune", 5}, {"Neeko", 5},
    {"Orianna", 5}, {"Qiyana", 5}, {"Quinn", 5}, {"Samira", 5}, {"Sivir", 5},
    {"Smolder", 5}, {"Soraka", 5}, {"Sylas", 5}, {"Syndra", 5}, {"Taliyah", 5},
    {"Talon", 5}, {"Teemo", 5}, {"Tristana", 5}, {"TwistedFate", 5}, {"Twitch", 5},
    {"Varus", 5}, {"Vayne", 5}, {"Veigar", 5}, {"Velkoz", 5}, {"Vex", 5},
    {"Viktor", 5}, {"Xayah", 5}, {"Xerath", 5}, {"Yunara", 5}, {"Zed", 5},
    {"Zeri", 5}, {"Ziggs", 5}, {"Zoe", 5},

    {"Akali", 4}, {"Belveth", 4}, {"Briar", 4}, {"Camille", 4}, {"Diana", 4},
    {"Ekko", 4}, {"FiddleSticks", 4}, {"Fiora", 4}, {"Fizz", 4}, {"Gwen", 4},
    {"Heimerdinger", 4}, {"Jayce", 4}, {"Kassadin", 4}, {"Kayle", 4}, {"Kayn", 4},
    {"KhaZix", 4}, {"Lissandra", 4}, {"Locke", 4}, {"Mordekaiser", 4}, {"Naafiri", 4},
    {"Nidalee", 4}, {"Nilah", 4}, {"Riven", 4}, {"Senna", 4}, {"Shaco", 4},
    {"Viego", 4}, {"Vladimir", 4}, {"Yasuo", 4}, {"Yone", 4}, {"Zilean", 4},

    {"Aatrox", 3}, {"Ambessa", 3}, {"Darius", 3}, {"Elise", 3}, {"Evelynn", 3},
    {"Galio", 3}, {"Gangplank", 3}, {"Gragas", 3}, {"Illaoi", 3}, {"Irelia", 3},
    {"Jax", 3}, {"Kled", 3}, {"LeeSin", 3}, {"Lillia", 3}, {"Maokai", 3},
    {"Morgana", 3}, {"Nocturne", 3}, {"Pantheon", 3}, {"Poppy", 3}, {"Pyke", 3},
    {"RekSai", 3}, {"Rengar", 3}, {"Rumble", 3}, {"Ryze", 3}, {"Sett", 3},
    {"Swain", 3}, {"Trundle", 3}, {"Tryndamere", 3}, {"Udyr", 3}, {"Urgot", 3},
    {"Vi", 3}, {"XinZhao", 3}, {"Zaahen", 3},

    {"Alistar", 2}, {"Amumu", 2}, {"Bard", 2}, {"Blitzcrank", 2}, {"Braum", 2},
    {"Chogath", 2}, {"DrMundo", 2}, {"Garen", 2}, {"Gnar", 2}, {"Hecarim", 2},
    {"Ivern", 2}, {"Janna", 2}, {"JarvanIV", 2}, {"KSante", 2}, {"Leona", 2},
    {"Lulu", 2}, {"Malphite", 2}, {"Milio", 2}, {"MonkeyKing", 2}, {"Nami", 2},
    {"Nasus", 2}, {"Nautilus", 2}, {"Nunu", 2}, {"Olaf", 2}, {"Ornn", 2},
    {"Rakan", 2}, {"Rammus", 2}, {"Rell", 2}, {"Renata", 2}, {"Renekton", 2},
    {"Sejuani", 2}, {"Seraphine", 2}, {"Shen", 2}, {"Shyvana", 2}, {"Singed", 2},
    {"Sion", 2}, {"Skarner", 2}, {"Sona", 2}, {"TahmKench", 2}, {"Taric", 2},
    {"Thresh", 2}, {"Volibear", 2}, {"Warwick", 2}, {"Yorick", 2}, {"Yuumi", 2},
    {"Zac", 2}, {"Zyra", 2}
}};

inline int GetDefaultPriority(const std::string& alias) {
    for (const auto& entry : Entries) {
        if (_stricmp(entry.Alias, alias.c_str()) == 0) return entry.Value;
    }
    return 1;
}

} // namespace Plugins::TargetSelectorImpulseData
