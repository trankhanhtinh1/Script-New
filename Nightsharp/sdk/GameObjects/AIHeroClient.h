#pragma once

// ============================================================================
// AIHeroClient — player-controlled champion
// ============================================================================
// Strong-typed leaf of `AIBaseClient`. Heroes expose the exact same API
// surface as AIBaseClient at the moment because every hero-specific field
// (MP / MaxMP / Level / Exp / items) is already read through `GameObject`'s
// stat accessors (the game stores them on the AIBaseClient layout). The
// dedicated type exists mainly so downstream wrappers (`TargetSelector`,
// `Orbwalker`, champion plugins) can take a `const AIHeroClient&` rather
// than a generic `AIBaseClient&` and reject non-champion inputs at compile
// time.
// ============================================================================

#include "AIBaseClient.h"

namespace SDK {

class AIHeroClient : public AIBaseClient {
public:
    using AIBaseClient::AIBaseClient;

    // Phase 2 (Apr 26/2026) stub. Real implementation reads the recall/death
    // state and the "Sion zombie" buff. Returning false here is safe — target
    // selector treats zombies as low-priority but valid targets, and the
    // false branch keeps them in the candidate pool.
    bool IsZombie() const {
        return false;
    }
};

} // namespace SDK
