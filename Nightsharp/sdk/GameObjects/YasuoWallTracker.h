#pragma once

#include "../../core/CoreObjectManager.h"
#include "../../core/CoreObjects.h"
#include "../../core/Globals.h"
#include "../Events/Events.h"
#include "../Math/YasuoWallModel.h"
#include "../Variables.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace SDK::YasuoWallTracker {

struct WallSnapshot {
    ::Core::Objects::ObjectHandle main = {};
    ::Core::Objects::ObjectHandle endpointA = {};
    ::Core::Objects::ObjectHandle endpointB = {};
    int level = 1;
    int spawnTick = 0;
    Vec3 center = {};
    Vec3 start = {};
    Vec3 end = {};

    float Span() const {
        return start.To2D().Distance(end.To2D());
    }
};

namespace detail {

inline std::mutex g_mutex;
inline YasuoWallModel::Registry g_registry;
inline std::vector<WallSnapshot> g_active;
inline bool g_createSubscribed = false;
inline bool g_deleteSubscribed = false;
inline bool g_seeded = false;
inline std::uintptr_t g_runtimeBase = 0;
inline int g_lastRefreshTick = -1;

inline YasuoWallModel::Identity ToIdentity(
    const ::Core::Objects::ObjectHandle& handle) {
    return {handle.address, handle.networkId, handle.index};
}

inline YasuoWallModel::Identity ToIdentity(
    const ::Core::Events::ObjectInfo& info) {
    return {info.Ptr, info.NetworkId, info.Index};
}

inline ::Core::Objects::ObjectHandle ToHandle(
    const YasuoWallModel::Identity& identity) {
    return {
        identity.address,
        identity.networkId,
        identity.index,
        ::Core::Objects::ObjectType::GameObject,
    };
}

inline std::string ResolveName(
    std::uintptr_t address,
    const char* eventName) {
    if (eventName && eventName[0]) {
        return eventName;
    }

    char buffer[128] = {};
    return ::Core::Objects::ReadName(
               address,
               buffer,
               static_cast<int>(sizeof(buffer)))
        ? std::string(buffer)
        : std::string();
}

inline void ObserveLocked(
    const ::Core::Events::ObjectInfo& info,
    int now) {
    if (!info.Ptr) {
        return;
    }

    const std::string name = ResolveName(info.Ptr, info.Name);
    const Vec3 position = info.Position.IsValid()
        ? info.Position
        : ::Core::Objects::ReadPosition(info.Ptr);
    g_registry.OnCreate(ToIdentity(info), now, name, position.To2D());
}

inline bool HasYasuoInGame() {
    static bool checked = false;
    static bool hasYasuo = false;
    if (!checked) {
        for (const auto& hero : GameObjects::Heroes()) {
            if (hero.IsValid() && _stricmp(hero.CharacterName().c_str(), "Yasuo") == 0) {
                hasYasuo = true;
                break;
            }
        }
        if (!GameObjects::Heroes().empty()) checked = true;
    }
    return hasYasuo;
}

inline void OnCreate(const Events::ObjectEventArgs& args) {
    if (!HasYasuoInGame()) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    ObserveLocked(args.Sender, Variables::TickCount());
}

inline void OnDelete(const Events::ObjectEventArgs& args) {
    if (!HasYasuoInGame()) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_registry.OnDelete(ToIdentity(args.Sender));
    g_active.clear();
}

inline void SeedLocked(int now) {
    std::uintptr_t objects[16384] = {};
    const int count = ::Core::ObjectManager::EnumerateAll(objects, 16384);
    if (count <= 0) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        const auto handle = ::Core::ObjectManager::MakeHandle(objects[i]);
        char name[128] = {};
        if (!handle.IsValid() ||
            !::Core::Objects::ReadName(
                objects[i],
                name,
                static_cast<int>(sizeof(name)))) {
            continue;
        }

        g_registry.OnCreate(
            ToIdentity(handle),
            now,
            name,
            ::Core::Objects::ReadPosition(objects[i]).To2D());
    }

    g_seeded = true;
}

inline void ResetLocked(int now) {
    g_registry = {};
    g_active.clear();
    g_lastRefreshTick = -1;
    g_runtimeBase = Globals::base;
    g_seeded = false;
    SeedLocked(now);
}

inline void RefreshLocked(int now) {
    if (g_runtimeBase != Globals::base) {
        ResetLocked(now);
    }
    if (!g_seeded) {
        SeedLocked(now);
    }
    if (g_lastRefreshTick == now) {
        return;
    }
    g_lastRefreshTick = now;

    const auto entries = g_registry.Entries();
    for (const auto& entry : entries) {
        auto handle = ToHandle(entry.identity);
        if (!::Core::ObjectManager::Resolve(handle)) {
            g_registry.OnDelete(entry.identity);
            continue;
        }

        std::string name;
        if (entry.pendingName) {
            name = ResolveName(handle.address, nullptr);
        }
        g_registry.Update(
            ToIdentity(handle),
            now,
            name,
            ::Core::Objects::ReadPosition(handle.address).To2D());
    }
    g_registry.Refresh(now);

    g_active.clear();
    for (const auto& wall : g_registry.ActiveWalls()) {
        auto main = ToHandle(wall.main);
        auto endpointA = ToHandle(wall.endpointA);
        auto endpointB = ToHandle(wall.endpointB);
        if (!::Core::ObjectManager::Resolve(main) ||
            !::Core::ObjectManager::Resolve(endpointA) ||
            !::Core::ObjectManager::Resolve(endpointB)) {
            continue;
        }

        const Vec3 mainPosition =
            ::Core::Objects::ReadPosition(main.address);
        g_active.push_back({
            main,
            endpointA,
            endpointB,
            wall.level,
            wall.spawnTick,
            mainPosition,
            Vec3(wall.start.x, mainPosition.y, wall.start.y),
            Vec3(wall.end.x, mainPosition.y, wall.end.y),
        });
    }
}

} // namespace detail

inline void EnsureInitialized() {
    std::lock_guard<std::mutex> lock(detail::g_mutex);

    if (!detail::g_createSubscribed) {
        detail::g_createSubscribed =
            Events::AddOnCreateObject(&detail::OnCreate);
    }
    if (!detail::g_deleteSubscribed) {
        detail::g_deleteSubscribed =
            Events::AddOnDeleteObject(&detail::OnDelete);
    }

    const int now = Variables::TickCount();
    if (detail::g_runtimeBase != Globals::base) {
        detail::ResetLocked(now);
    }
    if (!detail::g_seeded) {
        detail::SeedLocked(now);
    }
}

inline void Refresh() {
    EnsureInitialized();
    std::lock_guard<std::mutex> lock(detail::g_mutex);
    detail::RefreshLocked(Variables::TickCount());
}

inline std::vector<WallSnapshot> ActiveWalls() {
    Refresh();
    std::lock_guard<std::mutex> lock(detail::g_mutex);
    return detail::g_active;
}

inline bool Intersects(
    const Vec3& pathStart,
    const Vec3& pathEnd,
    float radius) {
    for (const auto& wall : ActiveWalls()) {
        YasuoWallModel::WallSegment segment;
        segment.start = wall.start.To2D();
        segment.end = wall.end.To2D();
        if (YasuoWallModel::IntersectsProjectilePath(
                pathStart.To2D(),
                pathEnd.To2D(),
                segment,
                std::max(radius, 0.0f))) {
            return true;
        }
    }
    return false;
}

} // namespace SDK::YasuoWallTracker
