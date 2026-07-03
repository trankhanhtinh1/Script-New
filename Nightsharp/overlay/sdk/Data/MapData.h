#pragma once
#include <cstdint>

namespace SDK::Generated::MapData {

struct MapRow {
    int32_t MapId;
    const char* Name;
    const char* ShortName;
    float GridX;
    float GridY;
    int32_t StartingLevel;
};

inline constexpr int kMapCount = 3;
inline const MapRow kMaps[] = {
    MapRow{ 10, "The Twisted Treeline", "twistedTreeline", 7700.0f, 7237.0f, 1 },
    MapRow{ 11, "Summoner's Rift", "summonerRift", 7410.0f, 7318.0f, 1 },
    MapRow{ 12, "Howling Abyss", "howlingAbyss", 6560.0f, 6309.0f, 3 }
};

} // namespace SDK::Generated::MapData
