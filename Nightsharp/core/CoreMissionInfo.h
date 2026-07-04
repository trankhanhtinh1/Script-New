#pragma once

#include "CoreRuntime.h"
#include "Globals.h"
#include "offset.h"

#include <cstdint>
#include <string>

namespace CoreMissionInfo {

enum class ElementalTerrain : std::uint8_t {
    Unknown = 0,
    Infernal = 1,
    Mountain = 2,
    Ocean = 3,
    Wind = 4,
    Cloud = Wind,
    Hextech = 5,
    Chemtech = 6,
};

struct Snapshot {
    uintptr_t address = 0;
    std::uint32_t mapId = 0;
    std::uint32_t queueId = 0;
    std::uint64_t gameId = 0;
    ElementalTerrain selectedElementalTerrain = ElementalTerrain::Unknown;
    std::uint8_t selectedElementalTerrainRaw = 0;
    std::uint8_t selectedElementalTerrainMask = 0;
    std::uint8_t selectedBaronPit = 0;
    std::uint8_t selectedBaronPitMask = 0;
    char gameMode[64] = {};

    bool IsValid() const {
        return Globals::IsValidPtr(address);
    }

    bool HasValidGameId() const {
        return gameId != 0 && gameId != ~std::uint64_t{0};
    }
};

namespace detail {
    inline std::uint8_t FirstSetMissionBit(std::uint8_t mask) {
        for (std::uint8_t result = 1; result < 8; ++result) {
            if ((mask & static_cast<std::uint8_t>(1u << result)) != 0) {
                return result;
            }
        }
        return 0;
    }

    inline bool ReadGameMode(uintptr_t missionInfo, char* out, int outCount) {
        if (!out || outCount <= 1) {
            if (out && outCount > 0) {
                out[0] = 0;
            }
            return false;
        }
        out[0] = 0;

        if (!Globals::IsValidPtr(missionInfo)) {
            return false;
        }

        const uintptr_t direct = Globals::Read<uintptr_t>(
            missionInfo + Offset::MissionInfo::GameMode);
        if (Globals::ReadCString(direct, out, outCount)) {
            return true;
        }

        return Globals::ReadGameString(
            missionInfo + Offset::MissionInfo::GameMode, out, outCount);
    }
} // namespace detail

inline uintptr_t Address() {
    auto& ctx = CoreRuntime::g_ctx;
    if (!CoreRuntime::EnsureInitialized()) {
        return 0;
    }

    if (!Globals::IsValidPtr(ctx.missionInfo)) {
        (void)CoreRuntime::RefreshReadState();
    }
    if (Globals::IsValidPtr(ctx.missionInfo)) {
        return ctx.missionInfo;
    }

    const uintptr_t base = ctx.moduleBase ? ctx.moduleBase : Globals::base;
    if (!base) {
        return 0;
    }
    const uintptr_t global = base + Offset::GameRuntime::MissionInfoInstance;
    return Globals::Read<uintptr_t>(global);
}

inline bool IsAvailable() {
    return Globals::IsValidPtr(Address());
}

inline std::uint32_t MapId() {
    const uintptr_t missionInfo = Address();
    return Globals::IsValidPtr(missionInfo)
        ? Globals::Read<std::uint32_t>(missionInfo + Offset::MissionInfo::MapId)
        : 0;
}

inline std::uint32_t QueueId() {
    const uintptr_t missionInfo = Address();
    return Globals::IsValidPtr(missionInfo)
        ? Globals::Read<std::uint32_t>(missionInfo + Offset::MissionInfo::GameType)
        : 0;
}

inline std::uint32_t GameType() {
    return QueueId();
}

inline std::uint64_t GameId() {
    const uintptr_t missionInfo = Address();
    return Globals::IsValidPtr(missionInfo)
        ? Globals::Read<std::uint64_t>(missionInfo + Offset::MissionInfo::GameId)
        : 0;
}

inline bool HasValidGameId() {
    const std::uint64_t gameId = GameId();
    return gameId != 0 && gameId != ~std::uint64_t{0};
}

inline bool ReadGameMode(char* out, int outCount) {
    return detail::ReadGameMode(Address(), out, outCount);
}

inline std::string GameMode() {
    char buffer[64] = {};
    return ReadGameMode(buffer, static_cast<int>(sizeof(buffer)))
        ? std::string(buffer)
        : std::string();
}

inline std::string Mission() {
    return GameMode();
}

inline std::uint8_t SelectedElementalTerrainMask() {
    const uintptr_t missionInfo = Address();
    return Globals::IsValidPtr(missionInfo)
        ? Globals::Read<std::uint8_t>(
            missionInfo + Offset::MissionInfo::SelectedElementalTerrain)
        : 0;
}

inline std::uint8_t SelectedElementalTerrainRaw() {
    return detail::FirstSetMissionBit(SelectedElementalTerrainMask());
}

inline ElementalTerrain SelectedElementalTerrain() {
    return static_cast<ElementalTerrain>(SelectedElementalTerrainRaw());
}

inline std::uint8_t SelectedBaronPitMask() {
    const uintptr_t missionInfo = Address();
    return Globals::IsValidPtr(missionInfo)
        ? Globals::Read<std::uint8_t>(
            missionInfo + Offset::MissionInfo::SelectedBaronPit)
        : 0;
}

inline std::uint8_t SelectedBaronPit() {
    return detail::FirstSetMissionBit(SelectedBaronPitMask());
}

inline const char* ElementalTerrainName(ElementalTerrain terrain) {
    switch (terrain) {
    case ElementalTerrain::Infernal:
        return "Infernal";
    case ElementalTerrain::Mountain:
        return "Mountain";
    case ElementalTerrain::Ocean:
        return "Ocean";
    case ElementalTerrain::Wind:
        return "Wind";
    case ElementalTerrain::Hextech:
        return "Hextech";
    case ElementalTerrain::Chemtech:
        return "Chemtech";
    default:
        return "Unknown";
    }
}

inline Snapshot GetSnapshot() {
    Snapshot snapshot = {};
    snapshot.address = Address();
    if (!Globals::IsValidPtr(snapshot.address)) {
        return snapshot;
    }

    snapshot.mapId = Globals::Read<std::uint32_t>(
        snapshot.address + Offset::MissionInfo::MapId);
    snapshot.queueId = Globals::Read<std::uint32_t>(
        snapshot.address + Offset::MissionInfo::GameType);
    snapshot.gameId = Globals::Read<std::uint64_t>(
        snapshot.address + Offset::MissionInfo::GameId);
    snapshot.selectedElementalTerrainMask = Globals::Read<std::uint8_t>(
        snapshot.address + Offset::MissionInfo::SelectedElementalTerrain);
    snapshot.selectedElementalTerrainRaw =
        detail::FirstSetMissionBit(snapshot.selectedElementalTerrainMask);
    snapshot.selectedElementalTerrain =
        static_cast<ElementalTerrain>(snapshot.selectedElementalTerrainRaw);
    snapshot.selectedBaronPitMask = Globals::Read<std::uint8_t>(
        snapshot.address + Offset::MissionInfo::SelectedBaronPit);
    snapshot.selectedBaronPit =
        detail::FirstSetMissionBit(snapshot.selectedBaronPitMask);
    (void)detail::ReadGameMode(
        snapshot.address, snapshot.gameMode,
        static_cast<int>(sizeof(snapshot.gameMode)));
    return snapshot;
}

} // namespace CoreMissionInfo
