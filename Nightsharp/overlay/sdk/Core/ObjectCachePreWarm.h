#pragma once
// ObjectCachePreWarm.h — PreWarm lives here, not in Objects.h, because
// PreWarm calls GameObjects::Player / EnemyMinions / Jungle / EnemyHeroes,
// and GameObjects.h includes Objects.h — a circular include if we put it there.
//
// Include order: Objects.h -> ... -> GameObjects.h -> ObjectCachePreWarm.h
// PluginManager.h is the only caller; it already includes both Objects.h and
// GameObjects.h before this header.

#include "Objects.h"
#include "../GameObjects/GameObjects.h"
#include "../../FpsDropDebug.h"

namespace ObjectCache {

    // Pre-warm the AiSnap + waypoint cache for all objects the orbwalker will
    // touch this frame. Call once at the top of PluginManager::OnUpdate, AFTER
    // CoreRuntime::TickRead() has bumped phaseGeneration.
    //
    // Effect: every GetAiSnap / GetServerPosition / IsMoving / IsDashing call
    // inside the hot orbwalk loop will hit warm cache entries instead of paying
    // a cold ResolveManager + Read round-trip per object per read.
    inline void PreWarm() {
        NS_PROFILE("ObjectCache::PreWarm");

        // Player — most-read object every frame (position, path, windup, etc.)
        const auto player = SDK::GameObjects::Player();
        if (player.IsValid()) {
            const uintptr_t addr = player.Address();
            GetAiSnap(addr);
            GetWaypoints(addr, 32);  // pre-warm path so Move() angle check hits warm cache
        }

        // Enemy minions — traversed by GetMinions() + HealthPrediction every orbwalk tick
        for (const auto& m : SDK::GameObjects::EnemyMinions()) {
            if (m.IsValid()) GetAiSnap(m.Address());
        }

        // Jungle — traversed alongside lane minions in GetMinions()
        for (const auto& j : SDK::GameObjects::Jungle()) {
            if (j.IsValid()) GetAiSnap(j.Address());
        }

        // Enemy heroes — read by TargetSelector + InAutoAttackRange checks
        for (const auto& h : SDK::GameObjects::EnemyHeroes()) {
            if (h.IsValid()) GetAiSnap(h.Address());
        }
    }

} // namespace ObjectCache
