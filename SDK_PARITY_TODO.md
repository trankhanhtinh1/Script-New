# NightSharp SDK Parity TODO

Date: 2026-06-24

Goal:
- Build `NightSharp/SDK/` to match `EnsoulSharp.SDK/Core/` 1-1 in public API shape, file grouping, event naming, wrapper naming, and data behavior where possible.
- Use `EnsoulSharp.SDK/` as the SDK surface source of truth.
- Use `EnsoulSharp/Reference/EnsoulSharp.dll` only as Core/API reference.
- Use IDA/ReClass/current offsets for native behavior; do not trust old NightSharp source for offsets or native logic.
- Build with `E:\Visual Studio` after SDK/API changes.

Related notes:
- Core/native audit: `EnsoulSharp_CoreParity_Audit.md`
- `GameObject.OnCreate` / `GameObject.OnDelete`: current source has `OnCreateObject` / `OnDeleteObject` SDK events. Any lifecycle work must be runtime verified because this hook has crashed before.
- `MissileClient.OnCreate`: use `OnMissileCreate`. This matches the intended SDK meaning of `MissileClient.OnCreate`.
- `GameObject.OnCreate`: use generic `OnCreateObject` for `SourceObjectName` spells.
- Cheat Engine rule: no breakpoint usage unless `NightSharp/Core/PackmanHook.h` is confirmed stable.

Status legend:
- `[x]` File/surface exists and is exposed.
- `[~]` Partial: file exists, but TODOs or backend dependencies remain.
- `[ ]` Missing or not wired.
- `[CORE]` Needs native reverse/core support before SDK can be complete.
- `[DATA]` Can be copied/generated from `EnsoulSharp.SDK/Resources/Data/*`.

## Current High-Level Status

- [x] Enumerations mostly exist under `NightSharp/SDK/Enumerations/*`.
- [x] Spell database files exist under `NightSharp/SDK/Wrappers/Spells/Database/*`.
- [~] Spell wrappers exist under `NightSharp/SDK/Wrappers/Spells/*`, but several TODOs wait on Prediction, Collision, TargetSelector, Damage, HealthPrediction, and Core cast packet decoding.
- [~] SpellTypes exist under `NightSharp/SDK/Wrappers/Spells/SpellTypes/*`; hit prediction, collision clipping, source-object rewrites, and draw debug are still partial.
- [~] Tracker exists under `NightSharp/SDK/Wrappers/Spells/Tracker/*`; champion-specific `_ZiggsR` is still missing.
- [~] Events exist, but several event filters/semantics need parity validation.
- [~] Damage wrappers exist, but need API parity review against `EnsoulSharp.SDK/Core/Wrappers/Damages/*`.
- [ ] TargetSelector wrapper tree is missing.
- [ ] Orbwalking wrapper tree is missing.
- [ ] Prediction tree is missing.
- [ ] Items and Map wrappers are missing at the EnsoulSharp.SDK path.
- [x] No SharpDX port is needed on C++; use NightSharp `Vec2` / `Vec3` helpers instead.
- [~] UI/menu base exists. `UI/IMenu/Customizer` and `UI/IMenu/Skins` are intentionally excluded.
- [~] Utils exist, but need member-by-member parity review.

## Priority Order

### P0 - Keep The Existing Spell/Evade Path Correct

- [ ] Port `Wrappers/Spells/Tracker/Skillshots/_ZiggsR.h`.
- [ ] Verify `Detector.h` uses both event paths exactly like EnsoulSharp.SDK:
  - `GameObject.OnCreate` equivalent: `Events::AddOnCreateObject` for `SourceObjectName`.
  - `MissileClient.OnCreate` equivalent: `Events::AddOnMissileCreate` for missiles.
- [ ] Add a lightweight runtime test plugin for Detector/Tracker skillshots:
  - draw active skillshot polygon/count,
  - show spell name/missile name/source object name,
  - show caster net id/champion,
  - show detection type: `ProcessSpell`, `Missile`, `CreateObject`.
- [ ] Runtime verify missile fields used by Tracker:
  - `MissileClient::CastInfoBase = 0x2C0`,
  - `SpellDataPtr = CastInfoBase + 0x00`,
  - `SpellName = CastInfoBase + 0x20`,
  - `MissileName = CastInfoBase + 0x48`,
  - `TargetIndex = CastInfoBase + 0x9C`,
  - `CasterIndex = CastInfoBase + 0xA0`,
  - `MissileNetId = CastInfoBase + 0xAC`,
  - `StartPos = CastInfoBase + 0xD0`,
  - `EndPos = CastInfoBase + 0xDC`,
  - `CastEndPos = CastInfoBase + 0xE8`.
- [ ] Keep coordinate convention native `x,y,z`; do not reintroduce EnsoulSharp C# `x,z,y` swap in NightSharp.

### P1 - Finish Existing TODOs In Spells

- [x] Decode spell slot in `CoreEvents::DecodeProcessCastSpell` / `CastSpellEventArgs` so `LastCast::OnCastSpell` can fill `LastCastPacketSentEntry.Slot`. Implemented from IDA 13339; needs runtime verification.
- [CORE] Extend `ProcessSpellEventArgs` with cast info target/source/slot if needed by LastCast and Detector.
- [x] Add `SpellDataResource` parameter accessors required by `Spell.h`: `CastRange`, `LineWidth`, `MissileSpeed`, `CastType`, script name, and icon name are exposed through `CoreSpellDataInst` / `SpellDataInstClient`. `Spell(slot, true)` now fills `Range`, `Width`, and `Speed` from native data with `SpellDatabase` fallback. `CastRadius` direct field is not pinned in IDA 13339 yet, so the accessor returns 0 and `Spell.h` uses database radius when line width is unavailable.
- [ ] Restore `Spell::GetPrediction`, `Spell::GetCollision`, and position overloads after Prediction/Collision are ported.
- [ ] Restore `Spell::GetTarget` after TargetSelector is ported.
- [ ] Restore `Spell::GetDamage` / `CanKill` after Damage wrapper and `AIBaseClient`/`AIHeroClient` accessors are parity-checked.
- [ ] Restore HealthPrediction use after `Math/Prediction/Health.h` is ported.
- [ ] Implement per-spell charged spell event registry if a C++ callback/event model is needed to match EnsoulSharp SDK.
- [ ] Set cast position height through `NavMesh.GetHeightForPosition` where EnsoulSharp does it.

Existing code TODOs to close:
- `NightSharp/SDK/Wrappers/Spells/LastCast.h`
- `NightSharp/SDK/Wrappers/Spells/LastCastedSpellEntry.h`
- `NightSharp/SDK/Wrappers/Spells/Spell.h`
- `NightSharp/SDK/Wrappers/Spells/SpellTypes/BaseSpell.h`
- `NightSharp/SDK/Wrappers/Spells/SpellTypes/Skillshot.h`
- `NightSharp/SDK/Wrappers/Spells/SpellTypes/SkillshotMissile.h`
- `NightSharp/SDK/Wrappers/Spells/Tracker/Detector.h`

### P2 - Math And Prediction

- [ ] Port `Math/Geometry.h`.
- [ ] Port `Math/ConvexHull.h`.
- [ ] Port `Math/Prediction/Cluster.h`.
- [ ] Port `Math/Prediction/GamePath.h`.
- [ ] Port `Math/Prediction/Health.h`.
- [ ] Port `Math/Prediction/Movement.h`.
- [~] Review current `Math/Collision.h` against `EnsoulSharp.SDK/Core/Math/Collision.cs`.
- [~] Review current `Math/Polygons/*` against EnsoulSharp polygon files and fill API gaps.
- [ ] Add tests/plugins for:
  - movement prediction point,
  - collision object detection,
  - polygon containment/intersection,
  - health prediction timing.

### P3 - TargetSelector

Port the whole tree under `NightSharp/SDK/Wrappers/TargetSelector/`:

- [ ] `HeroVisibleEntry.h`
- [ ] `ITargetSelectorMode.h`
- [ ] `TargetSelector.h`
- [ ] `TargetSelectorDrawing.h`
- [ ] `TargetSelectorHumanizer.h`
- [ ] `TargetSelectorMode.h`
- [ ] `TargetSelectorSelected.h`
- [ ] `Modes/Closest.h`
- [ ] `Modes/IWeightItem.h`
- [ ] `Modes/LeastHealth.h`
- [ ] `Modes/LessAttacksToKill.h`
- [ ] `Modes/LessCastsToKill.h`
- [ ] `Modes/MostAbilityPower.h`
- [ ] `Modes/MostAttackDamage.h`
- [ ] `Modes/NearMouse.h`
- [ ] `Modes/Priority.h`
- [ ] `Modes/PriorityCategory.h`
- [ ] `Modes/Weight.h`
- [ ] `Modes/WeightItemWrapper.h`
- [ ] `Modes/Weights/AbilityPower.h`
- [ ] `Modes/Weights/Aggro.h`
- [ ] `Modes/Weights/AttackDamage.h`
- [ ] `Modes/Weights/CrowdControl.h`
- [ ] `Modes/Weights/FocusMe.h`
- [ ] `Modes/Weights/Gold.h`
- [ ] `Modes/Weights/Killable.h`
- [ ] `Modes/Weights/LowHealth.h`
- [ ] `Modes/Weights/LowResists.h`
- [ ] `Modes/Weights/ShortDistanceCursor.h`
- [ ] `Modes/Weights/ShortDistancePlayer.h`
- [ ] `Modes/Weights/TeamFocus.h`

Dependencies:
- `GameObjects::EnemyHeroes` / hero cache correctness.
- Damage wrapper parity for killable/casts-to-kill modes.
- Cursor/world position parity for `NearMouse`.
- Visibility/fog/object validity accessors.

### P4 - Orbwalking

Port the whole tree under `NightSharp/SDK/Wrappers/Orbwalking/`:

- [ ] `Orbwalker.h`
- [ ] `OrbwalkerBase.h`
- [ ] `OrbwalkerSelector.h`

Dependencies:
- `Utils/AutoAttack.h` parity review.
- `Utils/Cursor.h` parity review.
- `Events/Turret.h`, `Events/Dash.h`, `Events/Gapcloser.h` semantics.
- Spell/basic attack cast timing.
- Input/click control safety.

Recommended implementation order:
- [ ] Read-only attack timer and target selection.
- [ ] Movement/click issue API behind a safety toggle.
- [ ] Full combo/harass/laneclear modes.

### P5 - Items And Map Wrappers

- [ ] Port `Wrappers/Items.h`.
- [ ] Port `Wrappers/Map.h`.
- [~] Use existing `NightSharp/SDK/Core/Map.h` as backend for `Wrappers/Map.h`.
- [CORE] Verify item inventory/accessor offsets if `Items.h` needs active item state, charges, cooldowns, or cast.
- [DATA] Map metadata should come from `EnsoulSharp.SDK/Resources/Data/Map.json` if compatible.

### P6 - Core-Like Top-Level SDK Files

These are path/name parity tasks. NightSharp already has equivalent concepts in different files, so most work is facade/alias/API shape:

- [ ] `Bootstrap.h`
- [ ] `Constants.h`
- [ ] `GameObjects.h`
- [ ] `Variables.h`

Existing NightSharp equivalents:
- `NightSharp/SDK/Core/Game.h`
- `NightSharp/SDK/Core/Variables.h`
- `NightSharp/SDK/GameObjects/GameObjects.h`
- `NightSharp/SDK/Lifecycle.h`

### P7 - Extensions

- [~] Review `Extensions/Enumerable.h`.
- [~] Review `Extensions/Extensions.h`.
- [~] Review `Extensions/Unit.h`.
- [ ] Add any missing `Vec2` / `Vec3` helpers directly to NightSharp extension files if later SDK code needs them.

NightSharp C++ does not use SharpDX. Do not create `Extensions/SharpDX/*` unless a very small compatibility alias is absolutely required by a future port; prefer native `Vec2` / `Vec3` helpers.

### P8 - Events Parity Review

Existing files:
- [~] `Events/Dash.h`
- [~] `Events/Events.h`
- [~] `Events/Gapcloser.h`
- [~] `Events/InterruptableSpell.h`
- [~] `Events/Load.h`
- [~] `Events/Stealth.h`
- [~] `Events/Teleport.h`
- [~] `Events/Turret.h`

Known TODOs:
- [ ] `Turret.h`: fire only for real turret basic attacks; initialize turret cache like EnsoulSharp.
- [ ] `Stealth.h`: fire only for `AIHeroClient` where EnsoulSharp does.
- [ ] `Teleport.h`: verify recall type/name mapping.
- [ ] `Events.h`: verify event names and argument fields against EnsoulSharp.SDK usage.

Core dependencies:
- stable `OnProcessSpell`,
- stable `OnDoCast`,
- stable `OnStopCast`,
- stable `OnNewPath`,
- stable object lifecycle for generic `GameObject.OnCreate` / `OnDelete`,
- missile create/delete for `MissileClient.OnCreate` / `OnDelete`.

### P9 - Damage Wrappers

Existing files:
- [~] `Wrappers/Damages/Damage.h`
- [~] `Wrappers/Damages/DamageData.h` (NightSharp-only helper)
- [~] `Wrappers/Damages/DamageJson.h`
- [~] `Wrappers/Damages/DamageLibrary.h`
- [~] `Wrappers/Damages/DamageMastery.h`
- [~] `Wrappers/Damages/DamagePassives.h`

TODO:
- [ ] Compare public methods and overloads against EnsoulSharp.SDK.
- [ ] Verify `DamageStage`, `DamageType`, raw stats, armor/mr, crit/on-hit fields.
- [ ] Restore calls from `Spell::GetDamage` once wrapper accessors are complete.
- [DATA] Load/generate data from `EnsoulSharp.SDK/Resources/Data/Database.json` or current NightSharp data source.

### P10 - Utils

Existing files need member-level parity review:

- [~] `Utils/ActionQueue.h`
- [~] `Utils/AutoAttack.h`
- [~] `Utils/BinarySerializer.h`
- [~] `Utils/Cache.h`
- [~] `Utils/CallbackPerformance.h`
- [~] `Utils/Cursor.h`
- [~] `Utils/DelayAction.h`
- [~] `Utils/DynamicInitializer.h`
- [~] `Utils/IFilter.h`
- [~] `Utils/Invulnerable.h`
- [~] `Utils/JsonFactory.h`
- [~] `Utils/Jungle.h`
- [~] `Utils/KeyConvert.h`
- [~] `Utils/Logging.h`
- [~] `Utils/MathUtils.h`
- [~] `Utils/Minion.h`
- [~] `Utils/Performance.h`
- [~] `Utils/Render.h`
- [~] `Utils/ResourceFactory.h`
- [~] `Utils/ResourceImportAttribute.h`
- [~] `Utils/ResourceLoader.h`
- [~] `Utils/Storage.h`
- [~] `Utils/TickOperation.h`
- [~] `Utils/WeightedRandom.h`
- [~] `Utils/WindowsKeys.h`

Likely dependencies:
- JSON/resource loader stability.
- Minion/Jungle object filters.
- Cursor/world-to-screen/world-to-minimap correctness.
- Render/Drawing backend parity.

### P11 - UI/Menu/Notifications

Existing base:
- [~] `UI/PermaShow.h`
- [~] `UI/Notifications/*`
- [~] `UI/IMenu/Menu*.h`
- [~] `UI/IMenu/Values/*`
- [~] `UI/Drawing.h`
- [~] `UI/UI.h`

TODO:
- [ ] Review `UI/Utils.h` from EnsoulSharp.SDK and port only useful helper behavior.

Intentionally excluded:
- `UI/IMenu/Customizer/*`
- `UI/IMenu/Skins/*`

Reason:
- NightSharp uses its own ImGui/menu style. Customizer and skin/theme classes are not needed for current SDK parity target.

### P12 - Data And Resources

Copy/generate equivalent data:

- [DATA] `Resources/Data/Database.json`
- [DATA] `Resources/Data/Gapclosers.json`
- [DATA] `Resources/Data/GlobalInterruptableSpellsList.json`
- [DATA] `Resources/Data/InterruptableSpells.json`
- [DATA] `Resources/Data/Map.json`
- [DATA] `Resources/Data/Priority.json`
- [DATA] Patch/versioned data JSON if still useful.

Existing NightSharp data files:
- `SDK/Data/SpellData.h`
- `SDK/Data/GapcloserData.h`
- `SDK/Data/InterruptableSpellData.h`
- `SDK/Data/MapData.h`
- `SDK/Data/PriorityData.h`
- `SDK/Data/DamageData.h`
- `SDK/Data/ItemInfo.h`
- `SDK/Data/UnitInfo.h`

TODO:
- [ ] Decide source of truth: generated C++ headers vs runtime JSON loader.
- [ ] Keep `SpellDatabase` schema compatible with EnsoulSharp SDK fields.
- [ ] Add generation step or documented manual conversion.

## Complete Missing File Checklist

This list is generated by mapping `EnsoulSharp.SDK/Core/**/*.cs` to `NightSharp/SDK/**/*.h` and checking what does not currently exist, then removing intentionally excluded C#-only or unwanted UI files.

- [ ] `Bootstrap.h`
- [ ] `Constants.h`
- [ ] `GameObjects.h`
- [ ] `Math/ConvexHull.h`
- [ ] `Math/Geometry.h`
- [ ] `Math/Prediction/Cluster.h`
- [ ] `Math/Prediction/GamePath.h`
- [ ] `Math/Prediction/Health.h`
- [ ] `Math/Prediction/Movement.h`
- [ ] `UI/Utils.h`
- [ ] `Variables.h`
- [ ] `Wrappers/Items.h`
- [ ] `Wrappers/Map.h`
- [ ] `Wrappers/Orbwalking/Orbwalker.h`
- [ ] `Wrappers/Orbwalking/OrbwalkerBase.h`
- [ ] `Wrappers/Orbwalking/OrbwalkerSelector.h`
- [ ] `Wrappers/Spells/Tracker/Skillshots/_ZiggsR.h`
- [ ] `Wrappers/TargetSelector/HeroVisibleEntry.h`
- [ ] `Wrappers/TargetSelector/ITargetSelectorMode.h`
- [ ] `Wrappers/TargetSelector/TargetSelector.h`
- [ ] `Wrappers/TargetSelector/TargetSelectorDrawing.h`
- [ ] `Wrappers/TargetSelector/TargetSelectorHumanizer.h`
- [ ] `Wrappers/TargetSelector/TargetSelectorMode.h`
- [ ] `Wrappers/TargetSelector/TargetSelectorSelected.h`
- [ ] `Wrappers/TargetSelector/Modes/Closest.h`
- [ ] `Wrappers/TargetSelector/Modes/IWeightItem.h`
- [ ] `Wrappers/TargetSelector/Modes/LeastHealth.h`
- [ ] `Wrappers/TargetSelector/Modes/LessAttacksToKill.h`
- [ ] `Wrappers/TargetSelector/Modes/LessCastsToKill.h`
- [ ] `Wrappers/TargetSelector/Modes/MostAbilityPower.h`
- [ ] `Wrappers/TargetSelector/Modes/MostAttackDamage.h`
- [ ] `Wrappers/TargetSelector/Modes/NearMouse.h`
- [ ] `Wrappers/TargetSelector/Modes/Priority.h`
- [ ] `Wrappers/TargetSelector/Modes/PriorityCategory.h`
- [ ] `Wrappers/TargetSelector/Modes/Weight.h`
- [ ] `Wrappers/TargetSelector/Modes/WeightItemWrapper.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/AbilityPower.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/Aggro.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/AttackDamage.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/CrowdControl.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/FocusMe.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/Gold.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/Killable.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/LowHealth.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/LowResists.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/ShortDistanceCursor.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/ShortDistancePlayer.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/TeamFocus.h`

## Excluded From Parity Checklist

These EnsoulSharp.SDK paths are intentionally not ported as 1-1 files:

- `Extensions/SharpDX/*`
  - Reason: SharpDX is a C#/.NET dependency. NightSharp C++ uses `Vec2` / `Vec3`; add native vector helpers only where a later SDK port needs them.
- `UI/IMenu/Customizer/*`
  - Reason: removed from NightSharp SDK scope.
- `UI/IMenu/Skins/*`
  - Reason: removed from NightSharp SDK scope. NightSharp keeps its own ImGui/menu rendering path.

## Verification Checklist For Every Port Batch

- [ ] Header parses standalone if practical.
- [ ] `NightSharp/SDK/SDK.h` exposes the new public surface.
- [ ] Existing code compiles with `Release|x64`.
- [ ] Any native offset used was checked in IDA/current dump and documented in `EnsoulSharp_CoreParity_Audit.md`.
- [ ] Any runtime-risk feature has a read-only test plugin before enabling control/cast/click behavior.
- [ ] No old-source offset is copied without current reverse verification.

## Next Work Order To Unblock Dangling TODOs

Do these in order. Each item unlocks TODOs above it depends on.

1. [x] Finish local cast request event data first.
   - `ProcessSpellEventArgs` already has `Slot`, `SourceIndex`, `TargetIndex`, `TargetNetworkId`, `CasterNetworkId`, `CastInfo`, `SpellData`, names, and positions.
   - Implemented from IDA 13339: `ProcessCastSpell(0x292310)` parses the request with `sub_910A80(parsedCastInfo, request + 0x18)`, and `OnProcessSpell(0x97E4C0)` consumes `parsedCastInfo + 0x154` as the slot.
   - `DecodeProcessCastSpell` now decodes the request byte at `request + 0xC4` into `CastSpellEventArgs::Slot`.
   - `LastCastPacketSentEntry` now stores the decoded slot instead of `SpellSlot::Unknown`.
   - Only extend `ProcessSpellEventArgs` further if IDA confirms missing fields such as total cast time or another reliable target/source id.

2. [x] Add spell parameter accessors for `Spell.h`.
   - `CoreSpellDataInst` / `SpellDataInstClient` now expose `CastRange`, `LineWidth`, `MissileSpeed`, `CastType`, script name, and icon name from `SpellDataResource`.
   - `Spell(SpellSlot::Q, loadFromGame=true)` now fills `Range`, `Width`, and `Speed` from native data and falls back to `SpellDatabase` if native data is missing.
   - IDA 13339 confirms the `SPELLPARAM_CASTRADIUS` enum value, but not a stable direct float field. `CastRadius()` intentionally returns 0 until that getter path is pinned; `Spell.h` uses database radius fallback.

3. [SDK] Port Math foundation before higher wrappers.
   - `Math/Geometry.h`
   - `Math/ConvexHull.h`
   - `Math/Prediction/GamePath.h`
   - `Math/Prediction/Movement.h`
   - `Math/Prediction/Health.h`
   - Review `Math/Collision.h`
   - This unlocks `Spell::GetPrediction`, `Spell::GetCollision`, skillshot hit prediction, and health prediction TODOs.

4. [SDK] Port `TargetSelector`.
   - Start with base selector and simple modes: `Closest`, `LeastHealth`, `NearMouse`.
   - Add weight modes after Damage and stats are ready.
   - This unlocks `Spell::GetTarget` and common champion script usage.

5. [SDK] Finish Damage wrapper parity.
   - Verify `DamageLibrary`, `DamagePassives`, `DamageMastery`, champion data, and required unit stat accessors.
   - This unlocks `Spell::GetDamage`, `Spell::CanKill`, and TargetSelector killable/casts-to-kill modes.

6. [SDK] Finish Spell tracker special cases.
   - Port `Wrappers/Spells/Tracker/Skillshots/_ZiggsR.h`.
   - Add a read-only tracker debug plugin showing active skillshots and detection source.
   - This validates `GameObject.OnCreate` + `MissileClient.OnCreate` matching EnsoulSharp.SDK.

7. [SDK] Port `Orbwalking` only after TargetSelector, AutoAttack, Cursor, and attack timing are stable.
   - Start read-only.
   - Add movement/click issuing behind a safety toggle.

8. [SDK/DATA] Port `Wrappers/Items.h`, `Wrappers/Map.h`, and data generation/copy.
   - Use current `Core/Map.h` backend for map facade.
   - Reverse item cooldown/charges/cast only if needed by public API.

9. [SDK] Review Events and Utils member-by-member.
   - Close `Turret`, `Stealth`, `Teleport`, `DelayAction`, `Minion`, `Jungle`, `Cursor`, and `Render` parity gaps.

After each item:
- update this TODO,
- update `EnsoulSharp_CoreParity_Audit.md` if native offsets or core behavior changed,
- expose new public headers through `NightSharp/SDK/SDK.h`,
- build `Release|x64`.
