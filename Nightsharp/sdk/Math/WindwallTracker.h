#pragma once

// ============================================================================
// WindwallTracker.h — single source of truth for live Yasuo windwall emitters.
// Event-driven: tracks EffectEmitter objects named Yasuo_Base_W_windwall<1-5>
// via OnCreateObject/OnDeleteObject. Replaces the dead per-call Get<EffectEmitter>
// scan and the synthetic OnDoCast fallback. Geometry lives in WindwallGeometry.h.
// ============================================================================

#include "../Core/Objects.h"
#include "../Core/Variables.h"
#include "../Events/Events.h"
#include "../GameObjects/GameObjects.h"
#include "WindwallGeometry.h"

#include <algorithm>
#include <string>
#include <vector>

namespace SDK::WindwallTracker {

struct Wall {
    EffectEmitter emitter = {};
    int networkId = 0;
    int level = 1;
    int castTick = 0;
};

namespace detail {

inline std::vector<Wall> g_walls;
inline bool g_subscribed = false;

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

// emitter name is often empty via Name(); fall back to raw name reads.
inline std::string ResolveName(uintptr_t address, const char* eventName) {
    if (eventName && eventName[0]) {
        return eventName;
    }
    char buffer[128] = {};
    if (::Core::Objects::ReadCharacterName(address, buffer, static_cast<int>(sizeof(buffer))) ||
        ::Core::Objects::ReadName(address, buffer, static_cast<int>(sizeof(buffer)))) {
        return buffer;
    }
    return std::string();
}

inline void OnCreate(const Events::ObjectEventArgs& args) {
    if (!HasYasuoInGame()) {
        return;
    }
    if (!args.Sender.Ptr) {
        return;
    }
    const std::string name = ResolveName(args.Sender.Ptr, args.Sender.Name);
    if (!SDK::WindwallGeo::IsWindwallName(name)) {
        return;
    }

    const int networkId = static_cast<int>(args.Sender.NetworkId);
    const auto exists = std::find_if(g_walls.begin(), g_walls.end(), [&](const Wall& w) {
        return (networkId != 0 && w.networkId == networkId) ||
               (w.emitter.IsValid() && w.emitter.Address() == args.Sender.Ptr);
    });
    if (exists != g_walls.end()) {
        return;
    }

    Wall wall;
    wall.emitter = EffectEmitter(args.Sender.Ptr);
    wall.networkId = networkId;
    wall.level = SDK::WindwallGeo::ParseLevel(name);
    wall.castTick = Variables::TickCount();
    g_walls.push_back(wall);
}

inline void OnDelete(const Events::ObjectEventArgs& args) {
    if (!HasYasuoInGame()) {
        return;
    }
    const int networkId = static_cast<int>(args.Sender.NetworkId);
    g_walls.erase(
        std::remove_if(g_walls.begin(), g_walls.end(), [&](const Wall& w) {
            return (networkId != 0 && w.networkId == networkId) ||
                   (args.Sender.Ptr && w.emitter.IsValid() &&
                    w.emitter.Address() == args.Sender.Ptr);
        }),
        g_walls.end());
}

inline void Prune() {
    const int now = Variables::TickCount();
    g_walls.erase(
        std::remove_if(g_walls.begin(), g_walls.end(), [now](const Wall& w) {
            return !w.emitter.IsValid() || w.emitter.IsDead() ||
                   now - w.castTick > 5000;
        }),
        g_walls.end());
}

} // namespace detail

inline void EnsureInitialized() {
    if (detail::g_subscribed) {
        return;
    }
    detail::g_subscribed =
        Events::AddOnCreateObject(&detail::OnCreate) &&
        Events::AddOnDeleteObject(&detail::OnDelete);
}

inline const std::vector<Wall>& Active() {
    EnsureInitialized();
    detail::Prune();
    return detail::g_walls;
}

} // namespace SDK::WindwallTracker
