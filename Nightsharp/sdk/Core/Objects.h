#pragma once

// ============================================================================
// sdk/Core/Objects.h — BACKWARDS-COMPAT SHIM
// ============================================================================
// The real hierarchy lives under `sdk/GameObjects/` (Phase 2.1 refactor):
//
//     GameObject.h             — base wrapper
//     AttackableUnit.h         — alias for AIBaseClient
//     AIBaseClient.h           — combat / orders / damage / spellbook
//     AIHeroClient.h           — champions
//     AIMinionClient.h         — minions / jungle / plants / wards / pets
//     AITurretClient.h         — structures
//     MissileClient.h          — in-flight projectiles
//     SpellbookClient.h        — keyed slot accessor
//     SpellDataInstClient.h    — per-slot snapshot
//     ObjectManager.h          — roster enumerators (+ legacy GameObjects:: alias)
//
// Pre-2026-04 consumers still include `sdk/Core/Objects.h` expecting the
// whole monolithic API in a single header. Rather than rewrite 50+ includes
// at once, this shim simply pulls in `ObjectManager.h` which transitively
// exposes every class and namespace. New code should include the specific
// `sdk/GameObjects/*.h` header(s) it needs instead of this shim.
// ============================================================================

#include "../GameObjects/ObjectManager.h"
