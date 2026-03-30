#pragma once

#include "../../generated/MapData.generated.h"
#include "../../core/CoreAPI.h"

namespace SDK::Map {

    using MapId = CoreAPI::Game::MapId;

    inline const Generated::MapData::MapRow* Current() {
        const auto id = static_cast<int>(CoreAPI::Game::GetMapId());
        for (const auto& row : Generated::MapData::kMaps) {
            if (row.MapId == id) {
                return &row;
            }
        }
        return nullptr;
    }

    inline MapId Id() {
        return CoreAPI::Game::GetMapId();
    }

    inline const char* Name() {
        const auto* row = Current();
        return row ? row->Name : "Unknown";
    }

    inline const char* ShortName() {
        const auto* row = Current();
        return row ? row->ShortName : "unknown";
    }

    inline bool IsSummonersRift() {
        return static_cast<int>(Id()) == 11;
    }

    inline bool IsHowlingAbyss() {
        return static_cast<int>(Id()) == 12;
    }

    inline bool IsTwistedTreeline() {
        return static_cast<int>(Id()) == 10;
    }

} // namespace SDK::Map
