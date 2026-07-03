#pragma once

#include "../../Core/CoreMissionInfo.h"

#include <cstdint>
#include <string>

namespace SDK::MissionInfo {

using ElementalTerrain = ::CoreMissionInfo::ElementalTerrain;
using Snapshot = ::CoreMissionInfo::Snapshot;

inline uintptr_t Address() {
    return ::CoreMissionInfo::Address();
}

inline bool IsAvailable() {
    return ::CoreMissionInfo::IsAvailable();
}

inline std::uint32_t MapId() {
    return ::CoreMissionInfo::MapId();
}

inline std::uint32_t QueueId() {
    return ::CoreMissionInfo::QueueId();
}

inline std::uint32_t GameType() {
    return ::CoreMissionInfo::GameType();
}

inline std::uint64_t GameId() {
    return ::CoreMissionInfo::GameId();
}

inline bool HasValidGameId() {
    return ::CoreMissionInfo::HasValidGameId();
}

inline bool ReadGameMode(char* out, int outCount) {
    return ::CoreMissionInfo::ReadGameMode(out, outCount);
}

inline std::string GameMode() {
    return ::CoreMissionInfo::GameMode();
}

inline std::string Mission() {
    return ::CoreMissionInfo::Mission();
}

inline std::uint8_t SelectedElementalTerrainMask() {
    return ::CoreMissionInfo::SelectedElementalTerrainMask();
}

inline std::uint8_t SelectedElementalTerrainRaw() {
    return ::CoreMissionInfo::SelectedElementalTerrainRaw();
}

inline ElementalTerrain SelectedElementalTerrain() {
    return ::CoreMissionInfo::SelectedElementalTerrain();
}

inline ElementalTerrain DragonSoulTerrain() {
    return SelectedElementalTerrain();
}

inline std::uint8_t SelectedBaronPitMask() {
    return ::CoreMissionInfo::SelectedBaronPitMask();
}

inline std::uint8_t SelectedBaronPit() {
    return ::CoreMissionInfo::SelectedBaronPit();
}

inline const char* ElementalTerrainName(ElementalTerrain terrain) {
    return ::CoreMissionInfo::ElementalTerrainName(terrain);
}

inline const char* DragonSoulName() {
    return ::SDK::MissionInfo::ElementalTerrainName(DragonSoulTerrain());
}

inline Snapshot GetSnapshot() {
    return ::CoreMissionInfo::GetSnapshot();
}

} // namespace SDK::MissionInfo

namespace SDK::Core {
namespace MissionInfo = ::SDK::MissionInfo;
}
