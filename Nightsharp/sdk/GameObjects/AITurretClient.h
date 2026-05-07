#pragma once

// ============================================================================
// AITurretClient — map structure (outer / inner / inhibitor / nexus turret)
// ============================================================================
// Inherits AIBaseClient verbatim — turret-specific reads (range, cast target,
// aggro state) already surface through the base accessors because the game
// reuses the `AIUnit` stat layout. Kept as a distinct type so code can
// dispatch on turret vs minion through overload resolution instead of name
// parsing.
// ============================================================================

#include "AIBaseClient.h"

namespace SDK {

class AITurretClient : public AIBaseClient {
public:
    using AIBaseClient::AIBaseClient;
};

} // namespace SDK
