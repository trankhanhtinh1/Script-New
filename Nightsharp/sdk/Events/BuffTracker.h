#pragma once

// ═══════════════════════════════════════════════════════════════════
// BuffTracker — SDK event system for OnBuffGain / OnBuffLose
//
// Usage (in champion script):
//   SDK::Events::BuffTracker::OnBuffGain([](const AIBaseClient& sender, const BuffEventArgs& args) {
//       if (sender.IsMe() && args.Name == "ThreshQ") {
//           // cast E to escape
//       }
//   });
//
// Integration (call once per tick in main loop):
//   SDK::Events::BuffTracker::Update();
// ═══════════════════════════════════════════════════════════════════

#include "../Core/Objects.h"
#include "../../core/CoreBuffs.h"

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

namespace SDK::Events {

// ── BuffEventArgs — matches C# AIBaseClientBuffGainEventArgs ──
struct BuffEventArgs {
    std::string Name;          // args.Buff.Name
    int         Type    = 0;   // args.Buff.Type  (bitmask: 1=Internal, 2=Aura, etc.)
    int         Stacks  = 0;   // args.Buff.Count
    float       StartTime = 0.0f;
    float       EndTime   = 0.0f;
    uintptr_t   Address   = 0; // raw buff ptr for advanced use
};

namespace BuffTracker {

using BuffCallback = std::function<void(const AIBaseClient&, const BuffEventArgs&)>;

// ── Internal state ──────────────────────────────────────────────
namespace detail {

    struct BuffSnapshot {
        char name[96] = {};
        int type   = 0;
        int stacks = 0;
        float startTime = 0.0f;
        float endTime   = 0.0f;
    };

    // Fixed-size storage to avoid C++ objects in SEH-sensitive paths
    static constexpr int kMaxCallbacks = 32;
    static constexpr int kMaxHeroes = 12;
    static constexpr int kMaxBuffsPerHero = 64;

    struct HeroBuffState {
        uintptr_t objAddr = 0;
        BuffSnapshot buffs[kMaxBuffsPerHero] = {};
        int count = 0;
    };

    inline BuffCallback  s_gainCbs[kMaxCallbacks] = {};
    inline int           s_gainCount = 0;
    inline BuffCallback  s_loseCbs[kMaxCallbacks] = {};
    inline int           s_loseCount = 0;
    inline HeroBuffState s_prev[kMaxHeroes] = {};
    inline int           s_prevCount = 0;
    inline bool          s_initialized = false;

    // SEH-safe callback invocation — no C++ objects with destructors
    inline void InvokeGainSafe(BuffCallback* cbs, int count,
                               uintptr_t senderAddr,
                               const char* name, int type, int stacks,
                               float startTime, float endTime) {
        BuffEventArgs args;
        args.Name      = name;
        args.Type      = type;
        args.Stacks    = stacks;
        args.StartTime = startTime;
        args.EndTime   = endTime;
        AIBaseClient sender(senderAddr);
        for (int i = 0; i < count; ++i) {
            if (cbs[i]) {
                cbs[i](sender, args);
            }
        }
    }

    inline void InvokeLoseSafe(BuffCallback* cbs, int count,
                               uintptr_t senderAddr,
                               const char* name, int type, int stacks,
                               float startTime, float endTime) {
        BuffEventArgs args;
        args.Name      = name;
        args.Type      = type;
        args.Stacks    = stacks;
        args.StartTime = startTime;
        args.EndTime   = endTime;
        AIBaseClient sender(senderAddr);
        for (int i = 0; i < count; ++i) {
            if (cbs[i]) {
                cbs[i](sender, args);
            }
        }
    }

    // Find a buff by name in a snapshot array
    inline int FindBuff(const HeroBuffState& state, const char* name) {
        for (int i = 0; i < state.count; ++i) {
            if (lstrcmpiA(state.buffs[i].name, name) == 0) {
                return i;
            }
        }
        return -1;
    }

} // namespace detail

// ── Public API ──────────────────────────────────────────────────

/// Subscribe to buff gain events
inline void OnBuffGain(BuffCallback cb) {
    if (detail::s_gainCount < detail::kMaxCallbacks) {
        detail::s_gainCbs[detail::s_gainCount++] = std::move(cb);
    }
}

/// Subscribe to buff lose events
inline void OnBuffLose(BuffCallback cb) {
    if (detail::s_loseCount < detail::kMaxCallbacks) {
        detail::s_loseCbs[detail::s_loseCount++] = std::move(cb);
    }
}

/// Clear all subscriptions
inline void Clear() {
    for (int i = 0; i < detail::s_gainCount; ++i) detail::s_gainCbs[i] = nullptr;
    for (int i = 0; i < detail::s_loseCount; ++i) detail::s_loseCbs[i] = nullptr;
    detail::s_gainCount = 0;
    detail::s_loseCount = 0;
    detail::s_prevCount = 0;
    detail::s_initialized = false;
}

/// Call every tick from main update loop
inline void Update() {
    // Skip if no subscribers
    if (detail::s_gainCount == 0 && detail::s_loseCount == 0) {
        return;
    }

    // Build current snapshot for all heroes
    detail::HeroBuffState current[detail::kMaxHeroes] = {};
    int heroCount = 0;

    for (const auto& hero : ObjectManager::Heroes()) {
        if (!hero.IsValid() || heroCount >= detail::kMaxHeroes) continue;

        auto& cur = current[heroCount];
        cur.objAddr = hero.Address();
        cur.count = 0;

        uintptr_t buffAddrs[256] = {};
        const int rawCount = CoreBuffs::Enumerate(cur.objAddr, buffAddrs, 256);

        for (int i = 0; i < rawCount && cur.count < detail::kMaxBuffsPerHero; ++i) {
            CoreBuffs::BuffRef ref{ buffAddrs[i] };
            if (!ref.IsValid() || ref.GetStacks() <= 0) continue;

            auto& snap = cur.buffs[cur.count];
            if (!ref.ReadName(snap.name, sizeof(snap.name))) continue;
            if (snap.name[0] == 0) continue;

            snap.type      = ref.GetType();
            snap.stacks    = ref.GetStacks();
            snap.startTime = ref.GetStartTime();
            snap.endTime   = ref.GetEndTime();
            cur.count++;
        }
        heroCount++;
    }

    // Skip diff on first tick (avoid false positives on load)
    if (!detail::s_initialized) {
        for (int h = 0; h < heroCount; ++h) {
            detail::s_prev[h] = current[h];
        }
        detail::s_prevCount = heroCount;
        detail::s_initialized = true;
        return;
    }

    // Diff: compare current vs previous for each hero
    for (int h = 0; h < heroCount; ++h) {
        const auto& cur = current[h];

        // Find matching previous state by objAddr
        int prevIdx = -1;
        for (int p = 0; p < detail::s_prevCount; ++p) {
            if (detail::s_prev[p].objAddr == cur.objAddr) {
                prevIdx = p;
                break;
            }
        }

        if (prevIdx < 0) {
            // New hero, skip diff
            continue;
        }

        const auto& prev = detail::s_prev[prevIdx];

        // OnBuffGain: in current but NOT in prev
        if (detail::s_gainCount > 0) {
            for (int i = 0; i < cur.count; ++i) {
                if (detail::FindBuff(prev, cur.buffs[i].name) < 0) {
                    detail::InvokeGainSafe(
                        detail::s_gainCbs, detail::s_gainCount,
                        cur.objAddr,
                        cur.buffs[i].name, cur.buffs[i].type,
                        cur.buffs[i].stacks, cur.buffs[i].startTime,
                        cur.buffs[i].endTime);
                }
            }
        }

        // OnBuffLose: in prev but NOT in current
        if (detail::s_loseCount > 0) {
            for (int i = 0; i < prev.count; ++i) {
                if (detail::FindBuff(cur, prev.buffs[i].name) < 0) {
                    detail::InvokeLoseSafe(
                        detail::s_loseCbs, detail::s_loseCount,
                        cur.objAddr,
                        prev.buffs[i].name, prev.buffs[i].type,
                        prev.buffs[i].stacks, prev.buffs[i].startTime,
                        prev.buffs[i].endTime);
                }
            }
        }
    }

    // Update snapshot
    for (int h = 0; h < heroCount; ++h) {
        detail::s_prev[h] = current[h];
    }
    detail::s_prevCount = heroCount;
}

} // namespace BuffTracker
} // namespace SDK::Events
