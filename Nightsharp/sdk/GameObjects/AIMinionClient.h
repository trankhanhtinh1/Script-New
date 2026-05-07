#pragma once

// ============================================================================
// AIMinionClient — any non-hero, non-turret actor on the map
// ============================================================================
// Covers lane minions (`SRU_ChaosMinion*` / `SRU_OrderMinion*`), jungle camps
// (small / large / epic — see `JungleTier` below), plants (`SRU_Plant_*`),
// wards, pets, clones, and special summons (Tibbers, Shaco box, Yorick
// ghouls, Zyra plants, …). Classification into those sub-kinds lives in
// `ObjectManager::detail::*` helpers; the wrapper itself stays slim and
// exposes:
//
//     * `IsJungle()`       — alias of the inherited `IsJungleMonster()`.
//     * `JungleTier`       — enum { Unknown, Small, Large, Epic }.
//     * `GetJungleTier()`  — name-based tier resolution (small raptor /
//       krug / wolf mini → Small; gromp / blue / red / scuttle / big camp
//       leader → Large; dragon / baron / herald / voidgrub / atakhan →
//       Epic). Returns `Unknown` when the object is not a jungle monster.
//
// Tier strings are compared case-insensitively and use `find()` so any
// trailing suffix Riot adds (`_elder`, `_air`, `_mini3`, …) still resolves.
// ============================================================================

#include "AIBaseClient.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace SDK {

enum class JungleTier : int {
    Unknown = 0,  // not a jungle monster
    Small,        // krug/raptor/wolf mini — no buff, part of a camp
    Large,        // gromp, blue, red, scuttle, camp leaders (big krug/raptor/wolf)
    Epic          // dragon, baron, rift herald, voidgrub, atakhan
};

class AIMinionClient : public AIBaseClient {
public:
    using AIBaseClient::AIBaseClient;

    bool IsJungle() const { return IsJungleMonster(); }

    // ── Jungle tier resolver ──────────────────────────────────────────────
    // Uses the best available name (CharacterName() fallback to Name()) and
    // does a case-insensitive substring match. Returns `JungleTier::Unknown`
    // for non-jungle objects. Called once per query — do NOT cache in the
    // wrapper because the name buffer is re-read from game memory each time.
    JungleTier GetJungleTier() const {
        if (!IsValid() || !IsAlive()) {
            return JungleTier::Unknown;
        }

        std::string name = CharacterName();
        if (name.empty()) {
            name = Name();
        }
        if (name.empty()) {
            return JungleTier::Unknown;
        }
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        auto contains = [&name](const char* token) {
            return name.find(token) != std::string::npos;
        };

        // Epic first — these are objectives, never treat as large/small.
        if (contains("sru_baron") || contains("sru_atakhan") ||
            contains("sru_riftherald") || contains("voidgrub") ||
            contains("sru_dragon") || contains("sru_hextech") ||
            contains("sru_chemtech")) {
            return JungleTier::Epic;
        }

        // Small — camp filler with `_mini` suffix (raptors, krugs, wolves).
        // Exception: `sru_krug_medium` is the medium krug spawned after
        // killing the big krug; treat as Small because it has low HP and
        // no buff reward.
        if (contains("_mini") || contains("sru_krug_medium")) {
            return JungleTier::Small;
        }

        // Large — buff holders + scuttle + camp leaders.
        if (contains("sru_blue") || contains("sru_red") ||
            contains("sru_gromp") || contains("sru_krug") ||
            contains("sru_murkwolf") || contains("sru_razorbeak") ||
            contains("sru_crab") || contains("sru_riftscuttler")) {
            return JungleTier::Large;
        }

        // Unknown jungle-flagged object — fall through as Unknown so the
        // caller can decide how aggressive it wants to be.
        return JungleTier::Unknown;
    }

    bool IsSmallJungle() const { return GetJungleTier() == JungleTier::Small; }
    bool IsLargeJungle() const { return GetJungleTier() == JungleTier::Large; }
    bool IsEpicJungle() const { return GetJungleTier() == JungleTier::Epic; }
};

} // namespace SDK
