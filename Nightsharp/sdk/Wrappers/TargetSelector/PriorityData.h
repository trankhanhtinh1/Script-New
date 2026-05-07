#pragma once

#include <algorithm>
#include <cstring>
#include <string>

namespace SDK::TargetSelectorData {

struct PriorityEntry {
    const char* Champion;
    int Priority;
};

inline int ClampPriority(int value) {
    return std::max(1, std::min(5, value));
}

inline int GetDefaultPriority(const std::string& championName) {
    static constexpr PriorityEntry entries[] = {
        {"Ahri", 4}, {"Anivia", 4}, {"Annie", 4}, {"Ashe", 4}, {"AurelionSol", 4},
        {"Azir", 4}, {"Brand", 4}, {"Caitlyn", 4}, {"Cassiopeia", 4}, {"Corki", 4},
        {"Draven", 4}, {"Ezreal", 4}, {"Graves", 4}, {"Jhin", 4}, {"Jinx", 4},
        {"Kaisa", 4}, {"Kalista", 4}, {"Karma", 4}, {"Karthus", 4}, {"Katarina", 4},
        {"Kennen", 4}, {"KogMaw", 4}, {"Leblanc", 4}, {"Lucian", 4}, {"Lux", 4},
        {"Malzahar", 4}, {"MasterYi", 4}, {"MissFortune", 4}, {"Neeko", 4}, {"Orianna", 4},
        {"Quinn", 4}, {"Sivir", 4}, {"Soraka", 4}, {"Syndra", 4}, {"Taliyah", 4},
        {"Talon", 4}, {"Teemo", 4}, {"Tristana", 4}, {"TwistedFate", 4}, {"Twitch", 4},
        {"Varus", 4}, {"Vayne", 4}, {"Veigar", 4}, {"Velkoz", 4}, {"Viktor", 4},
        {"Xayah", 4}, {"Xerath", 4}, {"Zed", 4}, {"Ziggs", 4}, {"Zoe", 4},
        {"Akali", 3}, {"Diana", 3}, {"Ekko", 3}, {"FiddleSticks", 3}, {"Fiora", 3},
        {"Fizz", 3}, {"Heimerdinger", 3}, {"Illaoi", 3}, {"Jayce", 3}, {"Kassadin", 3},
        {"Kayle", 3}, {"Khazix", 3}, {"Kindred", 3}, {"Lissandra", 3}, {"Mordekaiser", 3},
        {"Nidalee", 3}, {"Riven", 3}, {"Ryze", 3}, {"Shaco", 3}, {"Sylas", 3},
        {"Vladimir", 3}, {"Yasuo", 3}, {"Zilean", 3},
        {"Aatrox", 2}, {"Camille", 2}, {"Darius", 2}, {"Elise", 2}, {"Evelynn", 2},
        {"Galio", 2}, {"Gangplank", 2}, {"Gragas", 2}, {"Irelia", 2}, {"Jax", 2},
        {"Kayn", 2}, {"Kled", 2}, {"LeeSin", 2}, {"Morgana", 2}, {"Nocturne", 2},
        {"Pantheon", 2}, {"Poppy", 2}, {"RekSai", 2}, {"Rengar", 2}, {"Rumble", 2},
        {"Swain", 2}, {"Trundle", 2}, {"Tryndamere", 2}, {"Udyr", 2}, {"Urgot", 2},
        {"Vi", 2}, {"XinZhao", 2},
        {"Alistar", 1}, {"Amumu", 1}, {"Bard", 1}, {"Blitzcrank", 1}, {"Braum", 1},
        {"Chogath", 1}, {"DrMundo", 1}, {"Garen", 1}, {"Gnar", 1}, {"Hecarim", 1},
        {"Ivern", 1}, {"Janna", 1}, {"JarvanIV", 1}, {"Leona", 1}, {"Lulu", 1},
        {"Malphite", 1}, {"Maokai", 1}, {"MonkeyKing", 1}, {"Nami", 1}, {"Nasus", 1},
        {"Nautilus", 1}, {"Nunu", 1}, {"Olaf", 1}, {"Ornn", 1}, {"Pyke", 1},
        {"Rakan", 1}, {"Rammus", 1}, {"Renekton", 1}, {"Sejuani", 1}, {"Shen", 1},
        {"Shyvana", 1}, {"Singed", 1}, {"Sion", 1}, {"Skarner", 1}, {"Sona", 1},
        {"TahmKench", 1}, {"Taric", 1}, {"Thresh", 1}, {"Volibear", 1}, {"Warwick", 1},
        {"Yorick", 1}, {"Zac", 1}, {"Zyra", 1}
    };

    for (const auto& entry : entries) {
        if (_stricmp(entry.Champion, championName.c_str()) == 0) {
            return entry.Priority;
        }
    }
    return 1;
}

} // namespace SDK::TargetSelectorData
