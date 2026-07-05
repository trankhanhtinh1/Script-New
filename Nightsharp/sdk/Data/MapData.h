#pragma once
#include <cstdint>

namespace SDK::Generated::MapData {

struct MapRow {
    int32_t MapId;
    const char* Name;
    const char* ShortName;
    const char* MapStringId;
    float GridX;
    float GridY;
    int32_t StartingLevel;
    bool InCDragonLatest;
};

inline constexpr int kMapCount = 19;
inline const MapRow kMaps[] = {
    MapRow{ 0,  "Common",                  "common",                    "",           0.0f,    0.0f,    1, true  },
    MapRow{ 1,  "Summoner's Rift",         "summonerRiftOriginalSummer", "",           0.0f,    0.0f,    1, false },
    MapRow{ 2,  "Summoner's Rift",         "summonerRiftOriginalAutumn", "",           0.0f,    0.0f,    1, false },
    MapRow{ 3,  "The Proving Grounds",     "provingGrounds",            "",           0.0f,    0.0f,    1, false },
    MapRow{ 4,  "Twisted Treeline",        "twistedTreelineOriginal",   "",           0.0f,    0.0f,    1, false },
    MapRow{ 8,  "The Crystal Scar",        "crystalScar",               "",           0.0f,    0.0f,    1, false },
    MapRow{ 10, "The Twisted Treeline",    "twistedTreeline",           "",           7700.0f, 7237.0f, 1, false },
    MapRow{ 11, "Summoner's Rift",         "summonerRift",              "SR",         7410.0f, 7318.0f, 1, true  },
    MapRow{ 12, "Howling Abyss",           "howlingAbyss",              "HA",         6560.0f, 6309.0f, 3, true  },
    MapRow{ 14, "Butcher's Bridge",        "butchersBridge",            "",           0.0f,    0.0f,    3, false },
    MapRow{ 16, "Cosmic Ruins",            "cosmicRuins",               "",           0.0f,    0.0f,    1, false },
    MapRow{ 18, "Valoran City Park",       "valoranCityPark",           "",           0.0f,    0.0f,    1, false },
    MapRow{ 19, "Substructure 43",         "substructure43",            "",           0.0f,    0.0f,    1, false },
    MapRow{ 20, "Crash Site",              "crashSite",                 "",           0.0f,    0.0f,    1, false },
    MapRow{ 21, "Nexus Blitz",             "nexusBlitz",                "NB",         0.0f,    0.0f,    1, true  },
    MapRow{ 22, "Teamfight Tactics",       "teamfightTactics",          "TFT",        0.0f,    0.0f,    1, true  },
    MapRow{ 30, "Arena",                   "arena",                     "TGR",        0.0f,    0.0f,    1, true  },
    MapRow{ 33, "Swarm",                   "swarm",                     "Strawberry", 0.0f,    0.0f,    1, true  },
    MapRow{ 35, "The Bandlewood",          "bandlewood",                "BDW",        0.0f,    0.0f,    1, true  }
};

inline const MapRow* FindByMapId(int32_t mapId) {
    for (const auto& map : kMaps) {
        if (map.MapId == mapId) {
            return &map;
        }
    }
    return nullptr;
}

inline bool IsSummonersRift(int32_t mapId) {
    return mapId == 11;
}

} // namespace SDK::Generated::MapData
