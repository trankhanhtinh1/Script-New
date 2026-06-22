# EnsoulSharp Core Parity Audit

Date: 2026-06-22

Scope:
- ILSpy MCP: `EnsoulSharp/Reference/EnsoulSharp.dll`
- SDK source parity: `EnsoulSharp.SDK/`
- IDA MCP: current League IDB
- Offsets source: `NightSharp/Core/offset.h`
- NightSharp source: `NightSharp/Core`, `NightSharp/SDK`, `NightSharp/Plugins`

Goal:
- Biet ro EnsoulSharp co surface nao.
- Biet ro `EnsoulSharp.SDK/` co SDK surface nao.
- Biet NightSharp dang co gi, thieu gi, thua gi.
- Biet file nao can viet/sua.
- Biet offset nao trong `offset.h` duoc dung cho tung phan.
- Xay dung NightSharp SDK matching 1-1 voi EnsoulSharp SDK ve public API, file grouping, wrapper naming, va event shape.

Layering rule:
- `NightSharp/Core/*` la native/core layer: doc memory, goi native, bridge hook, validate offset.
- `NightSharp/SDK/*` follow `EnsoulSharp.SDK/Core/*` 1-1 ve public API va namespace.
- `CoreCastSpell` chi la native cast executor. It already handles position/skillshot, self, target, charge begin, and charge release.
- Nhung gi khong phai hanh dong cast truc tiep ma thuoc spellbook state/data/event/facade thi dua vao `CoreSpellBook` / `CoreSpellDataInst`, khong dua vao `CoreCastSpell`.

EnsoulSharp.SDK source map:

| EnsoulSharp.SDK path | NightSharp target | Notes |
|---|---|---|
| `EnsoulSharp.SDK/Core/Wrappers/Spells/*` | `NightSharp/SDK/Wrappers/Spells/*` | Spell facade, SpellTypes, LastCast, Tracker, Database. |
| `EnsoulSharp.SDK/Core/Events/*` | `NightSharp/SDK/Events/*` | Event public shape should match SDK naming where native hooks are stable. |
| `EnsoulSharp.SDK/Core/Wrappers/Orbwalking/*` | `NightSharp/SDK/Wrappers/Orbwalking/*` | Orbwalker API parity. |
| `EnsoulSharp.SDK/Core/Wrappers/TargetSelector/*` | `NightSharp/SDK/Wrappers/TargetSelector/*` | Target selector modes/weights parity. |
| `EnsoulSharp.SDK/Core/Wrappers/Damages/*` | `NightSharp/SDK/Wrappers/Damages/*` | Damage API/data parity. |
| `EnsoulSharp.SDK/Core/Math/*` | `NightSharp/SDK/Math/*` | Prediction, Geometry, Collision, Polygons. |
| `EnsoulSharp.SDK/Core/Utils/*` | `NightSharp/SDK/Utils/*` | Utility classes, DelayAction, Cursor, Cache, Render. |
| `EnsoulSharp.SDK/Core/UI/*` | `NightSharp/SDK/UI/*` | Menu, PermaShow, Notifications. |
| `EnsoulSharp.SDK/Core/Extensions/*` | `NightSharp/SDK/Extensions/*` | Unit/vector/enumerable extension parity. |
| `EnsoulSharp.SDK/Resources/Data/*` | `NightSharp/SDK/Data/*` or generated data | Copy/static data can be done without native reverse. |

## OnCreate / OnDelete Current State

Offsets in `offset.h` are kept unchanged:

```cpp
constexpr uintptr_t OnCreate = 0x560700; // OnCreateObject
constexpr uintptr_t OnDelete = 0x54CC30; // OnDeleteObject
```

Current implementation:

| Layer | File | Status |
|---|---|---|
| Hook install | `NightSharp/Core/Corehook.h` | Removed. `OnCreate` and `OnDelete` are not in `HookId`/`kHookSpecs`, so the inline hook installer no longer patches `0x560700` or `0x54CC30`. |
| Core event bridge | `NightSharp/Core/CoreEvents.h` | Removed. No lifecycle snapshot/queue and no Core `AddOnCreate`/`AddOnDelete` forwards. |
| SDK event API | `NightSharp/SDK/Events/Events.h` | Removed. No `AddOnCreate`, `OnCreate`, `RemoveOnCreate`, `AddOnDelete`, `OnDelete`, `RemoveOnDelete`, or `hook.OnCreate/OnDelete`. |
| SDK object cache | `NightSharp/SDK/GameObjects/GameObjects.h` | Removed lifecycle add/remove subscription. Cache still rebuilds on load. |
| Test plugin | `NightSharp/Plugins/Core/ObjectLifecycleTestPlugin.h` | Deleted. |
| Plugin registration | `NightSharp/Plugins/PluginBootstrap.h` | Removed `ObjectLifecycleTestPlugin` include and registration. |
| Project files | `NightSharp/NightSharp.vcxproj`, `NightSharp/NightSharp.vcxproj.filters` | No lifecycle test plugin entry is required. |

IDA details:

| RVA | IDA function | Native shape | Risk | Current decision |
|---|---|---|---|---|
| `0x560700` | `sub_560700` | `manager, object, networkId`; inserts object into ObjectManager network-id tree at `manager + 0x38` | Prologue contains RIP-relative compare at `+0x0A`; raw trampoline does not relocate RIP-relative stolen bytes | Keep offset only. Do not hook until a safer reverse/adapter is implemented. |
| `0x54CC30` | `sub_54CC30` | `manager, object`; removes object from arrays/trees and later calls object virtual cleanup | Object memory is being removed/destroyed | Keep offset only. Do not hook until delete lifetime is fully understood. |

EnsoulSharp equivalent:
- `EnsoulSharp.GameObject.OnCreate`
- `EnsoulSharp.GameObject.OnDelete`
- Native wrapper shape in ILSpy: `EventHandler<void (__cdecl*)(GameObject*)>.Add(EventAdapter.GetGameObjectCreateHandler(...))`

Current NightSharp intentionally does not expose this surface. The offsets remain documented in `offset.h`, but no code path installs or dispatches them.

## File Plan By EnsoulSharp Surface

### 1. Object Lifecycle

EnsoulSharp:
- `GameObject.OnCreate`
- `GameObject.OnDelete`
- `GameObject.OnIntegerPropertyChange`
- `GameObject.OnFloatPropertyChange`

NightSharp files:
- Existing: `NightSharp/Core/Corehook.h`
- Existing: `NightSharp/Core/CoreEvents.h`
- Existing: `NightSharp/Core/CoreObjectManager.h`
- Existing: `NightSharp/Core/CoreObjects.h`
- Existing: `NightSharp/SDK/Events/Events.h`
- Existing: `NightSharp/SDK/GameObjects/GameObjects.h`

Offsets used:
- `Offset::Hooks::OnCreate` exists but is not wired.
- `Offset::Hooks::OnDelete` exists but is not wired.
- `Offset::Hooks::OnIntegerPropertyChange`
- `Offset::ObjectManagerRuntime::NetworkIdTree`
- `Offset::ObjectManagerRuntime::NetworkIdTreeNodeKey`
- `Offset::ObjectManagerRuntime::NetworkIdTreeNodeObject`
- `Offset::All::Index`
- `Offset::All::NetId`
- `Offset::All::Team`
- `Offset::All::Dead`
- `Offset::All::Visible`
- `Offset::All::Position`
- `Offset::All::Name`
- `Offset::All::CharacterName`

Still missing:
- Safe implementation for `GameObject.OnCreate`.
- Safe implementation for `GameObject.OnDelete`.
- `GameObject.OnFloatPropertyChange` equivalent.
- True EventAdapter-backed `GameObject*` callback storage.

Do not add:
- Duplicate offset aliases such as `OnCreateObject` or `OnAssign`.
- Direct `OnCreate` prologue detour at `0x560700`; it can steal RIP-relative bytes.
- Any `OnCreate`/`OnDelete` plugin test until the native hook is stable.

### 2. CoreSpellBook

EnsoulSharp:
- `Spellbook`
- `SpellDataInst`
- `SpellData`
- `Spellbook.OnCastSpell`
- `Spellbook.OnStopCast`
- `Spellbook.OnUpdateChargedSpell`
- `Spellbook.CastSpell(...)`
- `Spellbook.UpdateChargedSpell(...)`
- `Spellbook.CanUseSpell(...)`
- `Spellbook.ActiveSpell`
- `Spellbook.Spells`

NightSharp files/status:
- Implemented: `NightSharp/Core/CoreSpellBook.h`
- Implemented: `NightSharp/Core/CoreSpellDataInst.h`
- Implemented bridge: `NightSharp/SDK/Core/Objects.h` (`SpellBookClient`, `SpellDataInstClient`, `AIBaseClient::Spellbook`, `AIBaseClient::GetSpell`)
- Implemented debug plugin: `NightSharp/Plugins/Core/SpellTrackingDebugPlugin.h`
- Existing dependency only: `NightSharp/Core/CoreCastSpell.h`
- Still missing SDK facade: `NightSharp/SDK/Wrappers/Spells/Spellbook.h`
- Still missing SDK facade: `NightSharp/SDK/Wrappers/Spells/SpellDataInst.h`
- Follow SDK source: `EnsoulSharp.SDK/Core/Wrappers/Spells/Spell.cs`
- Follow SDK source: `EnsoulSharp.SDK/Core/Wrappers/Spells/SpellTypes/*`
- Follow SDK source: `EnsoulSharp.SDK/Core/Wrappers/Spells/LastCast*.cs`
- Follow SDK source: `EnsoulSharp.SDK/Core/Wrappers/Spells/Tracker/*`
- Follow SDK source: `EnsoulSharp.SDK/Core/Wrappers/Spells/Database/*`
- Update: `NightSharp/SDK/SDK.h`
- Update: `NightSharp/SDK/Facade.h`
- Optional follow-up test plugin: `NightSharp/Plugins/Core/SpellBookTestPlugin.h`

Offsets used:
- `Offset::SpellRuntime::SpellBookOffset`
- `Offset::SpellRuntime::ActiveSpellCast`
- `Offset::SpellBookLayout::Owner`
- `Offset::SpellBookLayout::CasterNetId`
- `Offset::SpellBookLayout::ActiveSlot`
- `Offset::SpellBookLayout::SpellSlotArray`
- `Offset::SpellSlotLayout::*`
- `Offset::SpellCastInfoLayout::*`
- `Offset::SpellCastInfoEventLayout::*`
- `Offset::SpellDataLayout::*`
- `Offset::SpellDataResourceLayout::*`
- `Offset::ControlRuntime::CastSpellSafe`
- `Offset::ControlRuntime::GetSpellState`
- `Offset::ControlRuntime::GetSpellRemainingCooldown`
- `Offset::ControlRuntime::GetSpellCastInfo`
- `Offset::ControlRuntime::UpdateChargeableSpell`
- `Offset::Hooks::ProcessCastSpell`
- `Offset::Hooks::OnProcessSpell`
- `Offset::Hooks::OnStopCast`
- `Offset::Hooks::OnUpdateChargeableSpell`

Current status:
- `CoreCastSpell` already covers direct cast execution: position/skillshot, self, target, charge begin, and charge release.
- Do not expand `CoreCastSpell` for Spellbook read-side state or SDK facade work.
- `CoreSpellBook` now owns Spellbook read-side state: owner, caster network id, active spell pointer, active slot, spell array, cast info, spell state, readiness, cast/channel/charge checks, and local-player cast routing through `CoreCastSpell`.
- `CoreSpellDataInst` now owns per-slot spell data: slot pointer resolution, `SpellInfo`, `SpellData`, resource pointer, name, level, learned, cooldown expires, remaining cooldown, total cooldown, mana cost, ammo, recharge fields, raw native state.
- `SDK/Core/Objects.h` now exposes the first EnsoulSharp-like facade for `SpellDataInstClient` and `SpellBookClient`.
- `Plugins/Core/SpellTrackingDebugPlugin.h` now draws player/enemy Q/W/E/R/D/F level, remaining cooldown, total cooldown, simplified state, and recent `OnProcessSpell`/`OnDoCast` hits. It refreshes spell memory on a timer and uses event-based cooldown fallback for enemy spells.
- `SpellState` values are kept compatible with EnsoulSharp (`Ready=0`, `Cooldown=0x20`, `NoMana=0x40`, etc.). `RawState()` keeps the native bitmask; `State()` returns a simplified state for SDK readiness checks.
- SDK wrappers should still follow `EnsoulSharp.SDK/Core/Wrappers/Spells/*` 1-1, using `CoreCastSpell` only when a public `Spell.Cast(...)`/`Spellbook.CastSpell(...)` call needs to execute a cast.

IDA 13337 verification used for this implementation:
- `Offset::ControlRuntime::GetSpellState = 0x962210`: `sub_962210(spellbook, slot, outByte)` reads `*(spellbook + 0xAE0 + 8 * slot)` and returns an EnsoulSharp-style state bitmask.
- `Offset::ControlRuntime::GetSpellRemainingCooldown = 0x932C70`: `sub_932C70(spellSlot)` reads `slot + 0x30` cooldown expiry, uses game time, checks ammo/recharge fields including `slot + 0x68`, then clamps at zero.
- `SpellDataResourceLayout::ResCooldownTime = 0x6C8`: `sub_96CB90(spellDataResource, level)` returns `*(resource + 0x6C8 + 4 * max(level - 1, 0))`.
- Previous `0x92D820` was inside another function and is not a safe callable entry point; it was corrected in `offset.h`.
- `Offset::ControlRuntime::GetSpellCastInfo = 0x27DD20`: returns per-slot cast info from the owner cast-info array. Use mainly for diagnostics; active cast should prefer `Spellbook + Offset::SpellRuntime::ActiveSpellCast`.

Enemy cooldown tracking note:
- Native `GetSpellState` and `GetSpellRemainingCooldown` only report what exists in that object's replicated `SpellSlot` timer fields. They are enough for the local player, but enemy `slot + 0x30` is not always updated/replicated after an observed cast.
- Enemy Q/W/E/R/D/F display should therefore use a hybrid source: read level/state from `SpellBook` when available, then on `OnProcessSpell`/`OnDoCast` compute fallback cooldown from `SpellDataResource + 0x6C8`, apply ability haste for Q/W/E/R, store `expireTime = GameTime + cooldown`, and display `max(nativeRemaining, eventTrackedRemaining)`.
- Do not move this fallback into `CoreCastSpell`; it belongs in `CoreSpellBook`/`CoreSpellDataInst` or the later EnsoulSharp.SDK-style spell tracker facade.

Build verification:
- 2026-06-22: `MSBuild NightSharp.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /v:minimal` succeeded and produced `NightSharp/bin/Release/NightSharp.dll`.

Still missing:
- Dedicated `NightSharp/SDK/Wrappers/Spells/*` classes matching `EnsoulSharp.SDK/Core/Wrappers/Spells/*`.
- Spellbook event facade parity for `OnCastSpell`, `OnStopCast`, and `OnUpdateChargedSpell` at the SDK wrapper layer.
- Reverse/verification for toggle state, icon used, tooltip vars, numerical display, and full `SpellData` parameter getters.

### 3. CoreAiManager / Movement

EnsoulSharp:
- `AIBaseClient.Path`
- `AIBaseClient.ServerPosition`
- `AIBaseClient.Direction`
- `AIBaseClient.IsMoving`
- `AIBaseClient.GetPath(...)`
- `AIBaseClient.OnNewPath`

NightSharp files to write:
- New: `NightSharp/Core/CoreAiManager.h`
- Update: `NightSharp/Core/CoreEvents.h`
- Update: `NightSharp/SDK/GameObjects/GameObjects.h`
- New SDK facade if desired: `NightSharp/SDK/GameObjects/AiManager.h`

Offsets used:
- `Offset::AiManager::AiManager`
- `Offset::AiManager::CurrentSegment`
- `Offset::AiManager::DashSpeed`
- `Offset::AiManager::IsDashing`
- `Offset::AiManager::IsMoving`
- `Offset::AiManager::MoveVec3`
- `Offset::AiManager::NavArray`
- `Offset::AiManager::PathState`
- `Offset::AiManager::SegmentsCount`
- `Offset::AiManager::ServerPos`
- `Offset::AiManager::StartPath`
- `Offset::AiManager::TargetPosition`
- `Offset::AiManager::Velocity`
- `Offset::NavGridRuntime::GetAiManager`
- `Offset::Hooks::OnNewPath`

Current status:
- `OnNewPath` event exists.
- Full AiManager reader is missing.

Note:
- EnsoulSharp has no public type literally named `AiManager`; this is a NightSharp core helper for EnsoulSharp `AIBaseClient` movement parity.

### 4. CoreNavGrid / NavMesh

EnsoulSharp:
- `NavMesh.GetCollisionFlags`
- `NavMesh.SetCollisionFlags`
- `NavMesh.IsWallOfType`
- `NavMesh.IsWater`
- `NavMesh.WorldToGrid`
- `NavMesh.GridToWorld`
- `NavMesh.GetHeightForPosition`
- `NavMeshCell.CollFlags`
- `CollisionFlags.None/Grass/Wall/Building/Prop/GlobalVision`

NightSharp files to write:
- New: `NightSharp/Core/CoreNavGrid.h`
- New SDK facade: `NightSharp/SDK/Core/NavMesh.h`
- Optional enum: `NightSharp/SDK/Enumerations/CollisionFlags.h`
- Update: `NightSharp/SDK/SDK.h`
- Update: `NightSharp/SDK/Facade.h`

Offsets used:
- `Offset::NavGridRuntime::NavGrid`
- `Offset::NavGridRuntime::GetCollisionFlags`
- `Offset::NavGridLayout::*`
- `Offset::NavGridFlags::*`
- `Offset::NavGridCellLayout::*`

Current status:
- Implemented: `NightSharp/Core/CoreNavGrid.h`.
- Implemented SDK enum: `NightSharp/SDK/Enumerations/CollisionFlags.h`.
- Implemented SDK facade: `NightSharp/SDK/Core/NavMesh.h`.
- Implemented visual test plugin: `NightSharp/Plugins/Utility/NavGridDrawPlugin.h`.
- Updated: `NightSharp/SDK/SDK.h`, `NightSharp/SDK/Facade.h`, `NightSharp/Plugins/PluginBootstrap.h`.
- Existing `Offset::NavGridRuntime::GetCollisionFlags = 0x1243760` is documented but not called by `CoreNavGrid`; IDA shows it is not a simple public `GetCollisionFlags(float,float)` getter.

Mapping:
- EnsoulSharp `CollisionFlags.Grass` is the public brush/bush meaning.
- NightSharp can expose helper `IsBrush` as `Grass`/brush flag check.

IDA / ILSpy verification used:
- IDA 13337 `sub_1243DB0`: clamps grid x/y, uses `manager + 0x708` width, `manager + 0x70C` height, `manager + 0x110` cell data, and `16 * (x + y * width)` cell stride.
- IDA 13337 `sub_1243DB0`: collision flags are read from `*(cell + 0x00) + 0x06` when a cell overlay exists, otherwise from `cell + 0x08`.
- IDA 13337 `sub_124B2E0`: world-to-cell uses `(world.x - minX) * inverseScale` and `(world.z - minZ) * inverseScale`, with `minX = manager + 0xEC`, `minZ = manager + 0xF4`, `inverseScale = manager + 0x714`.
- IDA 13337 `sub_124B2E0`: native brush/passability bits are `0xC00`; non-brush passability rejects raw flag bit `0x0002`.
- IDA 13337 `sub_1243760`: this RVA is a path-neighbor expansion helper with a many-argument ABI; it calls `sub_124AEA0` and should not be used as the public collision flag getter.
- ILSpy `EnsoulSharp.CollisionFlags`: `None=0`, `Grass=1`, `Wall=2`, `Building=0x40`, `Prop=0x80`, `GlobalVision=0x100`.
- ILSpy `EnsoulSharp.NavMesh`: `GetCollisionFlags`, `SetCollisionFlags`, `IsWallOfType`, `IsWater`, `WorldToGrid`, `GridToWorld`, `GetHeightForPosition`, and `GetCell` are static facade methods over native `NavGrid`.

Implementation notes:
- `CoreNavGrid` returns EnsoulSharp-compatible public flags: raw native brush `0xC00` is normalized to public `CollisionFlags::Grass = 1`.
- Raw wall detection uses `CELL_WALL = 0x0002`, matching EnsoulSharp public `CollisionFlags::Wall = 2`.
- `SetCollisionFlags` converts public `Grass` back to native raw brush `0xC00` before writing the target cell flag storage.
- `GetHeightForPosition` currently returns `0.0f`; height callable RVA is still not re-derived. The draw plugin projects wall/brush rings on the local player's current Y plane, so the visual validation does not depend on height.
- `NavGridDrawPlugin` draws world-projected rings around nearby wall/building cells and brush cells, matching the requested wall/bush debug overlay shape.

Build verification:
- 2026-06-22: `E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe NightSharp.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /v:minimal` succeeded and produced `NightSharp/bin/Release/NightSharp.dll`.

### 5. CoreView / Drawing Projection

EnsoulSharp:
- `Drawing.Width`
- `Drawing.Height`
- `Drawing.View`
- `Drawing.Projection`
- `Drawing.WorldToScreen`
- `Drawing.ScreenToWorld`
- `Drawing.WorldToMinimap`
- `Drawing.MinimapToWorld`

NightSharp files to write:
- New: `NightSharp/Core/CoreView.h`
- Update: `NightSharp/SDK/UI/Drawing.h`
- Optional SDK facade: `NightSharp/SDK/Core/View.h`

Offsets used:
- `Offset::DrawingRuntime::WorldToScreen`
- `Offset::DrawingRuntime::ViewProjectionRoot`
- `Offset::DrawingRuntime::WorldToScreenContextOffset`
- `Offset::DrawingRuntime::HudInstance`
- `Offset::DrawingRuntime::ViewPort`
- `Offset::DrawingRuntime::ViewPort2`
- `Offset::DrawingRuntime::Renderer`
- `Offset::DrawingRuntime::ViewProjOffset`
- `Offset::DrawingMatrixRuntime::ProjMatrixRelative`
- `Offset::HudRuntime::ViewportW2S`
- `Offset::TacticalMapLayout::*`

Current status:
- Implemented: `NightSharp/Core/CoreView.h`.
- Implemented SDK facade: `NightSharp/SDK/Core/View.h`.
- Updated: `NightSharp/SDK/UI/Drawing.h`.
- `Drawing.Width` / `Drawing.Height` now use CoreView renderer size fallback.
- `Drawing.View` / `Drawing.Projection` expose the native view/projection matrix pair through `CoreView::Matrix4x4`.
- `Drawing.WorldToScreen` uses the native `sub_12F6A90` path first and falls back to local view-projection matrix projection.
- `Drawing.ScreenToWorld` inverts the view-projection matrix and intersects with the local player's Y plane.
- `Drawing.WorldToMinimap` / `Drawing.MinimapToWorld` forward to `CoreMap`.
- Added global Drawing toggle: `F7` flips `SDK::Drawing::IsEnabled()`. When disabled, `OnDraw`/`OnEndScene`, `WorldToScreen`/`OnScreen`, and draw primitives no-op.

IDA notes:
- `sub_12F6A90` subtracts the camera origin from the world position and calls `sub_12F69B0` with the view/projection/viewport context at `qword_1E79D20 + 0x2F8`.
- `sub_12F69B0` writes projected x/y/z and returns true only when z is in `[0,1)` and x/y are inside the viewport bounds.
- `sub_5999D0` confirms mouse screen coordinates at `MouseInput + 0x0C` and `MouseInput + 0x10`; this supports the existing cursor/screen-to-world fallback.

### 6. CoreMap / TacticalMap

EnsoulSharp:
- `TacticalMap.Size`
- `TacticalMap.Offset`
- `TacticalMap.CenterWorldPos`
- `Game.MapId`

NightSharp files to write:
- New: `NightSharp/Core/CoreMap.h`
- New SDK facade: `NightSharp/SDK/Core/Map.h`
- Update or integrate: `NightSharp/SDK/Data/MapData.h`

Offsets used:
- `Offset::GameRuntime::GetMapID`
- `Offset::TacticalMapLayout::NegMinimapX`
- `Offset::TacticalMapLayout::NegMinimapY`
- `Offset::TacticalMapLayout::MinimapX`
- `Offset::TacticalMapLayout::MinimapY`
- `Offset::TacticalMapLayout::MinimapWidth`
- `Offset::TacticalMapLayout::MinimapHeight`
- `Offset::TacticalMapLayout::CachedWidth`
- `Offset::TacticalMapLayout::CachedHeight`
- `Offset::TacticalMapLayout::ControllerMap`
- `Offset::TacticalMapLayout::ScaleX`
- `Offset::TacticalMapLayout::ScaleY`

Current status:
- Implemented: `NightSharp/Core/CoreMap.h`.
- Implemented SDK facade: `NightSharp/SDK/Core/Map.h`.
- Updated: `NightSharp/SDK/Core/Game.h` so `Game.MapId()` returns `CoreMap::GetMapId()`.
- `TacticalMap.Size`, `TacticalMap.Offset`, `TacticalMap.CenterWorldPos`, `WorldToMinimap`, and `MinimapToWorld` are exposed.
- `CoreMap` validates a runtime TacticalMap candidate from HUD pointer fields, then falls back to a NavGrid-derived minimap transform if the direct pointer cannot be resolved.
- `Offset::GameRuntime::GetMapID = 0x2AAEC0` is not called; IDA decompiled it into a large unrelated/render-layout routine on this IDB, so the current map id path is derived from NavGrid bounds instead.

IDA notes:
- `sub_134B7D0` uses the `0xA8..0xB4` float region as rectangle/bounds-style fields in the native UI/map layout path.
- `sub_12BC2B0` multiplies UI/map components by scale fields at `0x188` and `0x18C`, matching the existing `TacticalMapLayout::ScaleX/ScaleY` offsets.

### 7. CoreGame / Game Events

EnsoulSharp:
- `Game.Version`
- `Game.CursorPos`
- `Game.State`
- `Game.MapId`
- `Game.Window`
- `Game.Time`
- `Game.Ping`
- `Game.FPS`
- `Game.ZoomMax/ZoomMin/ZoomDesired`
- `Game.OnUpdate`
- `Game.OnWndProc`
- `Game.OnSendChat`
- `Game.OnDisplayChat`
- `Game.OnProcessPacket`
- `Game.OnNotify`
- `Game.Print`
- `Game.Say`
- `Game.ShowPing`
- `Game.SendPing`
- `Game.IsInFogOfWar`

NightSharp files to write/update:
- New or expand: `NightSharp/Core/CoreGame.h`
- Update: `NightSharp/Core/CoreEvents.h`
- Update: `NightSharp/SDK/Core/Game.h`
- New SDK event args if needed: `NightSharp/SDK/Events/GameNotify.h`
- New SDK event args if needed: `NightSharp/SDK/Events/Packet.h`

Offsets used:
- `Offset::GameRuntime::GameTime`
- `Offset::GameRuntime::NetInstance`
- `Offset::GameRuntime::ChatClient`
- `Offset::GameRuntime::ChatInstance`
- `Offset::GameRuntime::OpenWindowsArray`
- `Offset::GameRuntime::OpenWindowsCount`
- `Offset::GameRuntime::CursorPosRaw`
- `Offset::GameRuntime::MouseScreenVec2`
- `Offset::GameRuntime::GetPing`
- `Offset::GameRuntime::GetMapID`
- `Offset::GameRuntime::PrintChat`
- `Offset::Hooks::OnGameUpdate`
- `Offset::Hooks::ProcessWorldEvent`

ProcessWorldEvent note:
- ILSpy search found no public `EnsoulSharp.ProcessWorldEvent`.
- EnsoulSharp equivalent is closest to `Game.OnNotify(GameEventId, otherNetworkId, byte[256])`.
- NightSharp should treat `Offset::Hooks::ProcessWorldEvent` as native event source for a `GameNotify` style wrapper, not as public API name parity.

### 8. CoreHud

EnsoulSharp:
- `Hud.TargetChampionsOnly`
- `Hud.SelectedSpell`
- `Hud.DragonSRX`
- `Hud.PingBar`
- `Hud.PingSpell`
- `Hud.PingStat`
- `Hud.SetCameraLockState`
- `Hud.ShowClick`

NightSharp files to write:
- New: `NightSharp/Core/CoreHud.h`
- New SDK facade: `NightSharp/SDK/Core/Hud.h`

Offsets used:
- `Offset::DrawingRuntime::HudInstance`
- `Offset::HudRuntime::*`
- `Offset::HudSpellTargetingLayout::*`
- `Offset::HudInputLayout::*`
- `Offset::ZoomRuntime::*`
- `Offset::GameRuntime::CursorPosRaw`

Current status:
- Missing dedicated core.

### 9. Inventory / ItemData / Shop

EnsoulSharp:
- `AIBaseClient.InventoryItems`
- `InventorySlot`
- `ItemData`
- `ItemId`
- `BuyItemResult`
- `ShopClient`

NightSharp files to write:
- New: `NightSharp/Core/CoreInventory.h`
- New: `NightSharp/Core/CoreItemData.h`
- New SDK facade: `NightSharp/SDK/GameObjects/InventorySlot.h`
- Expand: `NightSharp/SDK/Data/ItemInfo.h`
- Optional: `NightSharp/Core/CoreShop.h`

Offsets used:
- `Offset::ItemRuntime::InventoryComponent`
- `Offset::ItemRuntime::SlotArray`
- `Offset::ItemRuntime::SlotCount`
- `Offset::ItemRuntime::ItemNode`
- `Offset::ItemRuntime::ItemInfo`
- `Offset::ItemRuntime::DataItemId`
- `Offset::ItemRuntime::DataAbilityHaste`
- `Offset::ItemRuntime::DataHealth`
- `Offset::ItemRuntime::DataArmor`
- `Offset::ItemRuntime::DataMR`
- `Offset::ItemRuntime::DataAD`
- `Offset::ItemRuntime::DataAP`
- `Offset::ItemRuntime::DataAtkSpeedMult`
- `Offset::All::ItemList`
- `Offset::GameRuntime::ShopInstance`

Current status:
- Static item info exists.
- Native inventory/item wrapper missing.

### 10. Perks

EnsoulSharp:
- `AIHeroClient.Perks`
- `Perk`

NightSharp files to write:
- New: `NightSharp/Core/CorePerks.h`
- New SDK facade: `NightSharp/SDK/GameObjects/Perk.h`

Offsets used:
- `offset.h` currently has no verified public `Perk` layout offsets.

Current status:
- Missing.
- Needs IDA reverse before implementation.

### 11. Buffs

EnsoulSharp:
- `BuffInstance`
- `AIBaseClient.GetBuff`
- `AIBaseClient.GetBuffCount`
- `AIBaseClient.HasBuff`
- `AIBaseClient.HasBuffOfType`
- `AIBaseClient.OnBuffAdd`
- `AIBaseClient.OnBuffRemove`

NightSharp files to write/update:
- Existing: `NightSharp/Core/CoreBuffs.h`
- Update: `NightSharp/Core/CoreEvents.h`
- New SDK facade: `NightSharp/SDK/GameObjects/BuffInstance.h`

Offsets used:
- `Offset::BuffManagerRuntime::BuffManagerOffset`
- `Offset::BuffManagerLayout::*`
- `Offset::BuffEntryLayout::*`
- `Offset::BuffDataLayout::*`
- `Offset::BuffEventLayout::*`
- `Offset::Hooks::OnBuffAdd`
- `Offset::Hooks::OnBuffRemove`
- `Offset::Hooks::OnBuffUpdate`
- `Offset::Hooks::OnBuffGain`

Current status:
- Partial core and events exist.
- Missing complete public `BuffInstance` wrapper.

### 12. Missile

EnsoulSharp:
- `MissileClient`
- `MissileClient.OnCommitMovement`

NightSharp files to write/update:
- New or expand: `NightSharp/Core/CoreMissile.h`
- Existing decode: `NightSharp/Core/CoreEvents.h`
- Existing SDK object wrapper: `NightSharp/SDK/GameObjects/GameObjects.h`
- Existing test: `NightSharp/Plugins/Champion/EzrealMissileLifecyclePlugin.h`

Offsets used:
- `Offset::MissileClient::*`
- `Offset::MissileEventLayout::*`
- `Offset::Hooks::OnMissileCreate`
- `Offset::Hooks::OnMissileDelete`

Current status:
- Missile create/delete decode exists.
- IDA 13337 verified `OnMissileCreate = 0x93ADA0` and `OnMissileDelete = 0x9210A0`.
- `sub_93ADA0` calls `sub_923450(missile + 0x2C0, payload)`, so `MissileClient::CastInfoBase = 0x2C0` is an embedded payload struct, not a pointer.
- `payload + 0xA0` is source object index.
- `payload + 0xAC` is missile network id; the outer object network id is also at `MissileClient + Offset::All::NetId` after `sub_562610`.
- `payload + 0xD0`, `+0xDC`, `+0xE8` are native `Vec3` start/end/cast-end positions. NightSharp keeps native `x,y,z`; do not apply EnsoulSharp C# `x,z,y` conversion in this codebase.
- `CoreEvents::ReadVector3` and `EzrealMissileLifecyclePlugin::ReadNativeWorld` now read native coordinates directly.
- `SDK::MissileClient::CasterNetworkId()` / `TargetNetworkId()` resolve the stored source/target index through `ObjectManager` instead of treating the payload index as a network id.
- `EzrealMissileLifecyclePlugin` now draws using the corrected native coordinate convention.
- Debug/test companion: `NightSharp/Plugins/Core/SpellTrackingDebugPlugin.h` can verify spell level/cooldown/cast events while missile draw fixes are tested in-game.
- Missing `OnCommitMovement` parity.

### 13. Object Model / ObjectManager

EnsoulSharp:
- `ObjectManager`
- `GameObject`
- `AttackableUnit`
- `AIBaseClient`
- `AIHeroClient`
- `AIMinionClient`
- `AITurretClient`
- `BarracksDampenerClient`
- `HQClient`
- `ShopClient`
- `Obj_SpawnPoint`
- `EffectEmitter`
- neutral camps / props / grass objects

NightSharp files to write/update:
- Existing: `NightSharp/Core/CoreObjects.h`
- Existing: `NightSharp/Core/CoreObjectManager.h`
- Existing: `NightSharp/SDK/GameObjects/ObjectManager.h`
- Existing: `NightSharp/SDK/GameObjects/GameObjects.h`
- Optional new split files:
  - `NightSharp/Core/CoreAttackableUnit.h`
  - `NightSharp/Core/CoreAIBaseClient.h`
  - `NightSharp/Core/CoreAIHeroClient.h`
  - `NightSharp/Core/CoreAIMinionClient.h`
  - `NightSharp/Core/CoreAITurretClient.h`

Offsets used:
- `Offset::GameObjectsRuntime::*`
- `Offset::ObjectManagerRuntime::*`
- `Offset::ClassificationRuntime::*`
- `Offset::All::*`
- `Offset::AttackableUnit::*`
- `Offset::AIHeroClient::*`
- `Offset::MinionClassRuntime::*`
- `Offset::JungleTypeRuntime::*`
- `Offset::VTable::GameObjectBoundingRadius`

Current status:
- Core object manager exists.
- Per-class public parity is incomplete.

### 14. Drawing Events / Native Devices

EnsoulSharp:
- `Drawing.OnDraw`
- `Drawing.OnBeginScene`
- `Drawing.OnEndScene`
- `Drawing.OnFlushDraw`
- `Drawing.OnPresent`
- `Drawing.OnD3D11Present`
- D3D9/D3D11 device/swapchain access

NightSharp files to write/update:
- Existing overlay: `NightSharp/overlay/Overlay.cpp`
- Existing SDK drawing: `NightSharp/SDK/UI/Drawing.h`
- Optional new: `NightSharp/Core/CoreDrawingEvents.h`

Offsets used:
- Mostly EventAdapter/native render-layer work.
- `offset.h` has render/view globals but not full Drawing EventAdapter entries.

Current status:
- Overlay path exists.
- EnsoulSharp drawing event parity is optional unless plugins require it.

### 15. Hacks

EnsoulSharp:
- `Hacks.ZoomHack`
- `Hacks.HideDrawingsFromCapture`
- `Hacks.DisablePrints`
- `Hacks.DisableDrawings`
- `Hacks.Console`
- `Hacks.AntiDisconnectFlags`
- `Hacks.AntiAFK`

NightSharp files/status:
- New: `NightSharp/Core/CoreHacks.h`
- New SDK facade: `NightSharp/SDK/Core/Hacks.h`
- Existing: `NightSharp/menu/MenuConfig.h` has `Config::StreamProtection::bypassObs`
- Existing: `NightSharp/menu/NightSharpMenu.h` has the `Bypass OBS` menu toggle/text
- Implemented platform call: `NightSharp/overlay/Overlay.cpp::SetAntiCapture`

Offsets used:
- `Offset::ZoomRuntime::*` for zoom-related behavior.
- Other Hacks offsets are not currently verified in `offset.h`.

Current status:
- Zoom UI/config exists; zoom logic is separate from SDK facade parity.
- `Bypass OBS` is wired in the current overlay: `SetWindowDisplayAffinity(g_hOverlay, 0x11)` when enabled, `0x0` when disabled.
- Real capture hiding should still be exposed as SDK parity for `Hacks.HideDrawingsFromCapture`.
- The platform call belongs in overlay/core hacks, not in spell/cast logic.
- Other non-zoom hacks still need IDA or platform-specific implementation.

## What Is Extra / Should Be Removed Or Avoided

| Item | Reason |
|---|---|
| Duplicate offset names like `OnCreateObject`, `OnAssign` | User explicitly requested not to add aliases; current `offset.h` already has correct `OnCreate`/`OnDelete`. |
| Direct prologue hook for `OnCreate` at `0x560700` | Stolen bytes include RIP-relative compare; raw trampoline does not relocate it. |
| Firing OnDelete SDK handlers inside native delete path | Plugin code can touch object while it is being destroyed. Current implementation removes this hook/event path entirely. |
| Treating `ProcessWorldEvent` as an EnsoulSharp public API name | EnsoulSharp does not expose that name; public equivalent is closest to `Game.OnNotify`. |
| Creating public `AiManager` just because NightSharp core uses that name | EnsoulSharp exposes movement through `AIBaseClient`; `CoreAiManager` should stay an internal helper unless SDK compatibility needs it. |
| Rewriting static SDK data before core natives are stable | `SDK/Data/*` is mostly static metadata and should not block Core parity. |

## Suggested Build Order

1. Implement `NightSharp/SDK/Wrappers/Spells/*` following `EnsoulSharp.SDK/Core/Wrappers/Spells/*` 1-1.
2. Done: implement `CoreNavGrid`/`NavMesh`.
3. Done: implement `CoreView` plus `CoreMap`.
4. Implement `CoreAiManager` movement reader.
5. Fill `CoreGame` notify/packet/chat/ping/window APIs.
6. Fill `CoreHud`, Inventory/ItemData, Perks, and remaining object-model class parity.
7. Revisit `OnCreate`/`OnDelete` only after a safer native adapter is reversed in IDA.

## Build Verification

- 2026-06-22: `E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe NightSharp\NightSharp.vcxproj /m /p:Configuration=Release /p:Platform=x64 /nologo`
- Result: build succeeded, 0 warnings, 0 errors.
- 2026-06-22: `E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe NightSharp\NightSharp.sln /m /p:Configuration=Release /p:Platform=x64`
- Result: build succeeded, 0 warnings, 0 errors.
- 2026-06-22: `E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe .\NightSharp.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /v:minimal`
- Result: build succeeded and produced `NightSharp/bin/Release/NightSharp.dll`.
