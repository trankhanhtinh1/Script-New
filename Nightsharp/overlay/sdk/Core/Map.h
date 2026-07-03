#pragma once

#include "../../Core/CoreMap.h"
#include "Objects.h"

namespace SDK::Map {

using MapId = ::CoreMap::MapId;
using MapBounds = ::CoreMap::MapBounds;
using TacticalMapState = ::CoreMap::TacticalMapState;

inline MapId Id() {
    return ::CoreMap::GetMapId();
}

inline int IdRaw() {
    return static_cast<int>(Id());
}

inline bool GetMapBounds(MapBounds& out) {
    return ::CoreMap::GetMapBounds(out);
}

} // namespace SDK::Map

namespace SDK::TacticalMap {

using TacticalMapState = ::CoreMap::TacticalMapState;

inline TacticalMapState GetState() {
    return ::CoreMap::GetTacticalMap();
}

inline Vector2 Size() {
    return ::CoreMap::Size();
}

inline Vector2 Offset() {
    return ::CoreMap::Offset();
}

inline Vector3 CenterWorldPos() {
    return ::CoreMap::CenterWorldPos();
}

inline bool WorldToMinimap(const Vector3& world, Vector2& minimap) {
    return ::CoreMap::WorldToMinimap(world, minimap);
}

inline Vector2 WorldToMinimap(const Vector3& world) {
    return ::CoreMap::WorldToMinimap(world);
}

inline bool MinimapToWorld(const Vector2& minimap, Vector3& world) {
    return ::CoreMap::MinimapToWorld(minimap, world);
}

inline Vector3 MinimapToWorld(const Vector2& minimap) {
    return ::CoreMap::MinimapToWorld(minimap);
}

} // namespace SDK::TacticalMap

namespace SDK::Core {
namespace Map = ::SDK::Map;
namespace TacticalMap = ::SDK::TacticalMap;
}
