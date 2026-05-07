#pragma once

// ============================================================================
// AttackableUnit — legacy alias for `AIBaseClient`
// ============================================================================
// In the EnsoulSharp SDK taxonomy, "AttackableUnit" is the parent of
// AIBaseClient. NightSharp collapsed the two because the game exposes stat
// accessors (HP / armor / MR) through the same layout whether the caller
// talks to a hero or a minion. This alias is kept to ease porting any
// consumer that still references the old EnsoulSharp name.
// ============================================================================

#include "AIBaseClient.h"

namespace SDK {
using AttackableUnit = AIBaseClient;
}
