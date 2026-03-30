#pragma once

// ============================================================================
// ObjectTracker — Poll-based OnAssign / OnDelete
//
// EnsoulSharp equivalents:
//   GameObject.OnAssign  → ObjectTracker::AddOnAssign()
//   GameObject.OnDelete  → ObjectTracker::AddOnDelete()
//
// How it works (manual-map safe):
//   Each tick we snapshot NetworkIds of all object lists.
//   Compare against previous tick:
//     - New IDs present     → fire OnAssign
//     - IDs that disappeared → fire OnDelete
//
// DEPENDENCY: SDK ObjectManager (sdk/Core/Objects.h)
//   Provides: Heroes(), AllyMinions(), EnemyMinions(), JungleMinions(),
//             AllyTurrets(), EnemyTurrets(), Missiles()
//
// NOTE: Core không cần bổ sung gì.
// ============================================================================

#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"

#include <algorithm>
#include <new>
#include <unordered_set>

namespace SDK::Events::ObjectTracker {

// ---------------------------------------------------------------------------
// Handler types
// ---------------------------------------------------------------------------

/// Fires when a new object appears in the object list
using AssignHandler = void(*)(const GameObject& sender);

/// Fires when an object disappears from the object list
using DeleteHandler = void(*)(const GameObject& sender);

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------
namespace detail {

    struct Snapshot {
        std::unordered_set<int> Heroes;
        std::unordered_set<int> Minions;
        std::unordered_set<int> Turrets;
        std::unordered_set<int> Missiles;
    };

    inline Snapshot* g_prev = nullptr;
    inline Snapshot* g_curr = nullptr;
    inline MenuUI::FixedList<AssignHandler, 64> g_onAssign = {};
    inline MenuUI::FixedList<DeleteHandler, 64> g_onDelete = {};

    inline bool EnsureStorage() {
        if (!g_prev) g_prev = new(std::nothrow) Snapshot();
        if (!g_curr) g_curr = new(std::nothrow) Snapshot();
        return g_prev && g_curr;
    }

    template<typename T>
    inline void CollectIds(const std::vector<T>& objects, std::unordered_set<int>& ids) {
        for (const auto& obj : objects) {
            const int id = obj.NetworkId();
            if (id != 0) ids.insert(id);
        }
    }

    inline void BuildSnapshot(Snapshot& snap) {
        snap.Heroes.clear();
        snap.Minions.clear();
        snap.Turrets.clear();
        snap.Missiles.clear();

        CollectIds(ObjectManager::Heroes(), snap.Heroes);
        CollectIds(ObjectManager::AllyMinions(), snap.Minions);
        CollectIds(ObjectManager::EnemyMinions(), snap.Minions);
        CollectIds(ObjectManager::JungleMinions(), snap.Minions);
        CollectIds(ObjectManager::AllyTurrets(), snap.Turrets);
        CollectIds(ObjectManager::EnemyTurrets(), snap.Turrets);
        CollectIds(ObjectManager::Missiles(), snap.Missiles);
    }

    inline void DiffAndFire(const std::unordered_set<int>& prev,
                            const std::unordered_set<int>& curr) {
        // New entries → OnAssign
        for (const int id : curr) {
            if (prev.find(id) == prev.end()) {
                GameObject obj = ObjectManager::GetByNetId(id);
                if (obj.IsValid()) {
                    for (const auto& h : g_onAssign) { if (h) h(obj); }
                }
            }
        }
        // Removed entries → OnDelete
        for (const int id : prev) {
            if (curr.find(id) == curr.end()) {
                GameObject obj = ObjectManager::GetByNetId(id);
                // Object may already be invalid when deleted; still fire event
                for (const auto& h : g_onDelete) {
                    if (h) h(obj);
                }
            }
        }
    }

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

inline void Initialize() { detail::EnsureStorage(); }

inline bool AddOnAssign(AssignHandler h) { return h && detail::g_onAssign.push_back(h); }
inline bool OnAssign(AssignHandler h)    { return AddOnAssign(h); }

inline bool AddOnDelete(DeleteHandler h) { return h && detail::g_onDelete.push_back(h); }
inline bool OnDelete(DeleteHandler h)    { return AddOnDelete(h); }

inline void Update() {
    if (!detail::EnsureStorage()) return;

    // Build current snapshot
    detail::BuildSnapshot(*detail::g_curr);

    // Skip diff on first tick (no previous data)
    if (!detail::g_prev->Heroes.empty() || !detail::g_prev->Minions.empty()
        || !detail::g_prev->Turrets.empty() || !detail::g_prev->Missiles.empty()) {

        if (!detail::g_onAssign.empty() || !detail::g_onDelete.empty()) {
            detail::DiffAndFire(detail::g_prev->Heroes,   detail::g_curr->Heroes);
            detail::DiffAndFire(detail::g_prev->Minions,  detail::g_curr->Minions);
            detail::DiffAndFire(detail::g_prev->Turrets,  detail::g_curr->Turrets);
            detail::DiffAndFire(detail::g_prev->Missiles, detail::g_curr->Missiles);
        }
    }

    // Swap: current becomes previous for next tick
    std::swap(detail::g_prev, detail::g_curr);
}

inline void Reset() {
    if (detail::g_prev) { detail::g_prev->Heroes.clear(); detail::g_prev->Minions.clear();
                          detail::g_prev->Turrets.clear(); detail::g_prev->Missiles.clear(); }
    if (detail::g_curr) { detail::g_curr->Heroes.clear(); detail::g_curr->Minions.clear();
                          detail::g_curr->Turrets.clear(); detail::g_curr->Missiles.clear(); }
    detail::g_onAssign.clear();
    detail::g_onDelete.clear();
}

} // namespace SDK::Events::ObjectTracker
