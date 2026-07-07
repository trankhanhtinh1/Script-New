# EnsoulSharp API Full Port — Extension Pattern

**Date**: 2026-07-07
**Status**: Approved
**Approach**: B — Extension Pattern (mirror EnsoulSharp structure)

## Overview

Port missing EnsoulSharp SDK APIs to NightSharp so plugins can be ported 1:1.
All core functions already exist in `CoreControl.h`, `CoreBuffs.h`, `CoreItem.h`,
`CoreCastSpell.h` — this spec adds the SDK wrapper layer.

## New Files

### 1. `sdk/Enums/BuffType.h`
```cpp
#pragma once

namespace SDK {
enum class BuffType : int {
    Internal = 0,
    Aura,
    CombatEnchancer,
    CombatDehancer,
    SpellShield,
    Stun,
    Invisibility,
    Silence,
    Taunt,
    Berserk,
    Polymorph,
    Slow,
    Snare,
    Damage,
    Heal,
    Haste,
    SpellImmunity,
    PhysicalImmunity,
    Invulnerability,
    AttackSpeedSlow,
    NearSight,
    Currency,
    Fear,
    Charm,
    Poison,
    Suppression,
    Blind,
    Counter,
    Shred,
    Flee,
    Knockup,
    Knockback,
    Disarm,
    Grounded,
    Drowsy,
    Asleep,
    Obscured,
    ClickproofToEnemies,
    UnKillable
};
} // namespace SDK
```
Mirrors `EnsoulSharp.BuffType` (39 values). Used by `HasBuffOfType()`.

### 2. `sdk/Enums/GameObjectOrder.h`
```cpp
#pragma once

namespace SDK {
enum class GameObjectOrder : int {
    HoldPosition = 1,
    MoveTo = 2,
    AttackUnit = 3,
    PetAttack = 5,
    PetMove = 6,
    AttackMove = 7,
    PetReturn = 9,
    Stop = 10,
    PetStop = 11
};
} // namespace SDK
```
Mirrors `EnsoulSharp.GameObjectOrder`. Maps to core `OrderType` enum.

### 3. `sdk/Extensions/AIBaseClientExtensions.h`
```cpp
#pragma once

#include "../Core/Objects.h"
#include "../Enums/BuffType.h"
#include "../Enums/GameObjectOrder.h"
#include "../../core/CoreControl.h"
#include "../../core/CoreBuffs.h"

namespace SDK {

// ── Attack timing (delegates to CoreControl) ──
float AttackDelay(const AIBaseClient& source);
float AttackWindup(const AIBaseClient& source);
float AttackCastDelay(const AIBaseClient& source);  // alias for AttackWindup
float AttackSpeed(const AIBaseClient& source);       // 1.0f / AttackDelay

// ── State flags (reads ActionState via native or offset) ──
bool CanAttack(const AIBaseClient& source);
bool CanMove(const AIBaseClient& source);
bool CanCast(const AIBaseClient& source);
bool CanWalk(const AIBaseClient& source);

// ── Buff type check ──
bool HasBuffOfType(const AIBaseClient& source, BuffType type);

// ── IssueOrder (delegates to CoreControl::IssueOrder) ──
bool IssueOrder(const AIBaseClient& source, GameObjectOrder order,
                Vector3 position, bool triggerEvent = true);
bool IssueOrder(const AIBaseClient& source, GameObjectOrder order,
                const AIBaseClient& target, bool triggerEvent = true);

} // namespace SDK
```

**Implementation details**:
- `AttackDelay(source)` → `CoreControl::GetAttackDelay(source.Address())`
- `AttackWindup(source)` → `CoreControl::GetAttackWindup(source.Address())`
- `AttackSpeed(source)` → `1.0f / AttackDelay(source)` (matches EnsoulSharp `1f / AttackDelay`)
- `CanAttack(source)` → call native `CanAttack` at `Offset::ControlRuntime::CanAttack` with source.Address(), or read ActionState flags
- `CanMove/CanCast/CanWalk` → read ActionState bitfield at `Offset::AttackableUnit::ActionState1`
- `HasBuffOfType(source, type)` → `CoreBuffs::HasBuffType(source.Address(), static_cast<int>(type))`
- `IssueOrder(source, order, pos)` → map `GameObjectOrder` to `OrderType`, call `CoreControl::IssueOrder()`

**GameObjectOrder → OrderType mapping**:
| GameObjectOrder | OrderType |
|-----------------|-----------|
| HoldPosition (1) | Hold (1) |
| MoveTo (2) | MoveTo (2) |
| AttackUnit (3) | AttackUnit (3) |
| AttackMove (7) | AttackMove (7) |
| Stop (10) | Stop (10) |
| PetAttack (5) | AutoAttackPet (4) — closest match |
| PetMove (6) | MovePet (6) |

### 4. `sdk/Extensions/AIHeroClientExtensions.h`
```cpp
#pragma once

#include "../Core/Objects.h"
#include "AIBaseClientExtensions.h"

namespace SDK {

// ── Item helpers (delegates to CoreItem + Spellbook) ──
bool CanUseItem(const AIHeroClient& source, int itemId);
bool UseItem(const AIHeroClient& source, int itemId);
bool UseItem(const AIHeroClient& source, int itemId, const AIBaseClient& target);
bool UseItem(const AIHeroClient& source, int itemId, Vector3 position);

// ── Inventory helpers ──
int GetItemSlot(const AIHeroClient& source, int itemId);
// Returns SpellSlot for item, or Unknown if not found

} // namespace SDK
```

**Implementation details**:
- `CanUseItem(source, id)` → find inventory slot with `CoreItem::HasItemId` → check `Spellbook().GetSpell(slot).State() == SpellState::Ready`
- `UseItem(source, id)` → find slot → `Spellbook().CastSpell(slot)`
- `UseItem(source, id, target)` → find slot → `Spellbook().CastSpell(slot, target)`
- `UseItem(source, id, pos)` → find slot → `Spellbook().CastSpell(slot, pos)`

Mirrors `EnsoulSharp.SDK.Items.CanUseItem/UseItem` exactly.

### 5. `sdk/Utils/Items.h`
```cpp
#pragma once

#include "../Extensions/AIHeroClientExtensions.h"

namespace SDK::Items {

// Static helpers matching EnsoulSharp.SDK.Items
bool CanUseItem(const AIHeroClient& source, int itemId);
bool CanUseItem(const AIHeroClient& source, const char* itemName);

bool UseItem(const AIHeroClient& source, int itemId);
bool UseItem(const AIHeroClient& source, int itemId, const AIBaseClient& target);
bool UseItem(const AIHeroClient& source, int itemId, Vector3 position);
bool UseItem(const AIHeroClient& source, const char* itemName, const AIHeroClient& target);

} // namespace SDK::Items
```

Thin wrapper over `AIHeroClientExtensions`. The `source` must be the local player
for UseItem (matches EnsoulSharp behavior — `source.Compare(GameObjects.Player)` check).

### 6. `sdk/Events/BuffTracker.h`
```cpp
#pragma once

#include "../Core/Objects.h"
#include "../Enums/BuffType.h"
#include "../../core/CoreBuffs.h"

namespace SDK::Events {

struct BuffEventArgs {
    std::string Name;
    BuffType    Type      = BuffType::Internal;
    int         Stacks    = 0;
    float       StartTime = 0.0f;
    float       EndTime   = 0.0f;
    uintptr_t   Address   = 0;
    bool IsValid() const { return Address != 0; }
};

using BuffCallback = std::function<void(const AIBaseClient&, const BuffEventArgs&)>;

// Push-driven by core hooks. Initialize() registers with HookEvents.
void Initialize();
void OnBuffUpdate(BuffCallback cb);
void OnBuffGain(BuffCallback cb);    // filter: Stacks > 0
void OnBuffLose(BuffCallback cb);    // filter: Stacks == 0
void Reset();

} // namespace SDK::Events
```

**Implementation**: Wraps existing `HookEvents::OnBuffAdd`/`OnBuffRemove`/`OnBuffUpdate`.
`OnBuffGain` = filter `args.Stacks > 0`, `OnBuffLose` = filter `args.Stacks == 0`.
Pattern from Old source `sdk/Events/BuffTracker.h`.

## Modified Files

### `sdk/Wrappers/Spells/Spell.h`
- Move `ShootChargedSpell` from `private` to `public`
- Add `bool IsCharging() const` public accessor
- No other changes to Spell class logic

### `sdk/Core/Objects.h`
- Add `#include "../Enums/BuffType.h"` at top
- Add `#include "../Enums/GameObjectOrder.h"` at top
- No changes to existing class definitions — extensions are free functions

### `sdk/GameObjects/GameObjects.h`
- Verify `Player()` returns type that supports extension calls
- If `Player()` returns `AIBaseClient`, add `PlayerHero()` returning `AIHeroClient`
- Or change `Player()` return type to `AIHeroClient` (preferred, matches EnsoulSharp)

## API Mapping Summary

| EnsoulSharp API | NightSharp Implementation | Core Source |
|----------------|--------------------------|-------------|
| `AIBaseClient.AttackDelay` | `SDK::AttackDelay(source)` | `CoreControl::GetAttackDelay()` |
| `AIBaseClient.AttackCastDelay` | `SDK::AttackCastDelay(source)` | `CoreControl::GetAttackWindup()` |
| `AIBaseClientExtensions.AttackSpeed()` | `SDK::AttackSpeed(source)` | `1.0f / AttackDelay()` |
| `AIBaseClient.CanAttack` | `SDK::CanAttack(source)` | Native `Offset::ControlRuntime::CanAttack` |
| `AIBaseClient.CanMove` | `SDK::CanMove(source)` | ActionState flags |
| `AIBaseClient.CanCast` | `SDK::CanCast(source)` | ActionState flags |
| `AIBaseClient.HasBuffOfType(type)` | `SDK::HasBuffOfType(source, type)` | `CoreBuffs::HasBuffType()` |
| `AIBaseClient.IssueOrder(order, pos)` | `SDK::IssueOrder(source, order, pos)` | `CoreControl::IssueOrder()` |
| `AIBaseClient.IssueOrder(order, target)` | `SDK::IssueOrder(source, order, target)` | `CoreControl::IssueOrder()` |
| `Items.CanUseItem(source, id)` | `SDK::Items::CanUseItem(source, id)` | `CoreItem` + `Spellbook` |
| `Items.UseItem(source, id)` | `SDK::Items::UseItem(source, id)` | `CoreItem` + `Spellbook::CastSpell` |
| `Spell.ShootChargedSpell` | `Spell::ShootChargedSpell` (public) | Already exists, just public |
| `Events.OnBuffGain` | `SDK::Events::OnBuffGain(cb)` | `HookEvents::OnBuffUpdate` |
| `Events.OnBuffLose` | `SDK::Events::OnBuffLose(cb)` | `HookEvents::OnBuffUpdate` |

## CanAttack / CanMove / CanCast Implementation

**IDA verified**: `CanAttack` native at `0x2119E0` takes `object + 0x1470` (ActionState1) as parameter.
`IssueOrder` calls it via `lea rcx, [rsi+1470h]` then `call CanAttack`.

**GameObjectCharacterState flags** (from EnsoulSharp `GameObjectCharacterState` enum):
```cpp
enum class CharacterStateFlags : uint32_t {
    CanAttack  = 0x1,       // bit 0
    CanCast    = 0x4,       // bit 2
    CanMove    = 0x8,       // bit 3
    Immovable  = 0x10,      // bit 4 — CanWalk = !(flags & Immovable)
    IsStealthed = 0x20,
    IsTaunted   = 0x80,
    IsFeared    = 0x100,
    IsSuppressed = 0x400,
    IsCharmed   = 0x20000,
    IsSlowed    = 0x1000000,
    IsGrounded  = 0x8000000,
};
```

**Implementation**:
```cpp
inline uint32_t ReadCharacterState(const AIBaseClient& source) {
    const uintptr_t a = source.Address();
    if (!a) return 0;
    // ActionState1 at 0x1470, flags at +0x30 inside CharacterState struct
    return Globals::Read<uint32_t>(a + Offset::AttackableUnit::ActionState1 + 0x30);
}

bool CanAttack(const AIBaseClient& source) {
    return (ReadCharacterState(source) & 0x1) != 0;
}
bool CanCast(const AIBaseClient& source) {
    return (ReadCharacterState(source) & 0x4) != 0;
}
bool CanMove(const AIBaseClient& source) {
    return (ReadCharacterState(source) & 0x8) != 0;
}
bool CanWalk(const AIBaseClient& source) {
    return (ReadCharacterState(source) & 0x10) == 0;  // Immovable = bit 4
}
```

**Note**: `CanAttack` can also call native `CanAttack(object + 0x1470)` for exact parity,
but reading the flag directly is simpler and avoids function call overhead.

## Jungle Verification

Check `GameObjects.h` `IsKnownJungleMonsterName` includes:
- `Sru_Voidgrub` / `Sru_Voidgrubs` — already present per initial exploration
- Verify `JungleType::Epic` is assigned to Voidgrubs
- No new files needed if already correct

## Files NOT Changed

- `core/offset.h` — already has all needed offsets
- `core/CoreControl.h` — already has IssueOrder, GetAttackDelay, GetAttackWindup
- `core/CoreBuffs.h` — already has HasBuffType, HasBuff, FindByName
- `core/CoreItem.h` — already has inventory functions
- `core/CoreCastSpell.h` — already has CastSpell infrastructure

## Implementation Order

**Quy tắc**: Sau mỗi bước, phải build check lỗi trước khi sang bước tiếp theo.

1. `sdk/Enums/BuffType.h` — no dependencies → **build check**
2. `sdk/Enums/GameObjectOrder.h` — no dependencies → **build check**
3. `sdk/Extensions/AIBaseClientExtensions.h` — depends on Enums + Objects.h → **build check**
4. `sdk/Extensions/AIHeroClientExtensions.h` — depends on AIBaseClientExtensions → **build check**
5. `sdk/Utils/Items.h` — depends on AIHeroClientExtensions → **build check**
6. `sdk/Events/BuffTracker.h` — depends on Objects.h + CoreBuffs → **build check**
7. `sdk/Wrappers/Spells/Spell.h` — move ShootChargedSpell to public → **build check**
8. `sdk/Core/Objects.h` — add includes → **build check**
9. `sdk/GameObjects/GameObjects.h` — verify/fix Player() return type → **build check**
10. Verify Jungle definitions → **build check**

## Testing

- Compile check: all new headers must compile cleanly with existing codebase
- Plugin check: `Kalista.h` TODO comments about missing IssueOrder should be resolvable
- Runtime check: `AttackSpeed()` should return ~0.625-2.5 for a level 1-18 champion
- Runtime check: `CanUseItem()` should return false for empty item slots
- Runtime check: `IssueOrder(MoveTo, pos)` should move the player

## Risks & Mitigations

| Risk | Status | Mitigation |
|------|--------|-----------|
| CanMove/CanCast bit positions unknown | **Resolved** | IDA + EnsoulSharp `GameObjectCharacterState` enum: CanAttack=0x1, CanCast=0x4, CanMove=0x8, Immovable=0x10 |
| CanAttack native parameter type | **Resolved** | IDA: `CanAttack(obj + 0x1470)` — takes CharacterState ptr, not object ptr |
| GameObjectOrder values differ from OrderType | Low risk | Explicit mapping table in spec, values 1/2/3/7/10 match directly |
| Player() returns AIBaseClient not AIHeroClient | **Resolved** | `GameObjects::Player()` already returns `AIHeroClient` (line 919) |
| Items API needs SpellState::Ready check | **Resolved** | `CoreSpellBook::State_Ready = 0` in `CoreSpellDataInst.h` |
| BuffTracker double-registration | Low risk | Guard with `s_registered` flag like Old source pattern |
| CharacterState flags offset +0x30 | Needs runtime verify | IDA shows `CanAttack` reads `[a1+48]` = `+0x30`; if wrong, adjust offset |
