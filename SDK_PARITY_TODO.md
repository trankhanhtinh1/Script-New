# NightSharp SDK Parity TODO

Date: 2026-06-28

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
- [~] Spell wrappers exist under `NightSharp/SDK/Wrappers/Spells/*`; `Spell::GetPrediction`, `Spell::GetCollision`, `Spell::GetDamage`, and `Spell::GetHealthPrediction` are wired to the newer SDK backends. TargetSelector-backed `Spell::GetTarget` is still pending.
- [~] SpellTypes exist under `NightSharp/SDK/Wrappers/Spells/SpellTypes/*`; hit prediction, collision clipping, source-object rewrites, and draw debug are still partial.
- [~] Tracker exists under `NightSharp/SDK/Wrappers/Spells/Tracker/*`; champion-specific `_ZiggsR` is still missing.
- [~] Events exist, but several event filters/semantics need parity validation.
- [x] Damage wrappers are wired: `SDK::Damage::CalculateDamage`, `CalculateMixedDamage`, `GetSpellDamage`, spell-stage damage type routing, CDragon item/rune metadata, rune modifiers, passive damage, and `BonusHealth` scaling are in place.
- [~] TargetSelector wrapper tree exists under `NightSharp/SDK/Wrappers/TargetSelector/*`, but SDK exposure, missing weight modes, and runtime parity review remain.
- [~] Orbwalking wrapper tree exists under `NightSharp/SDK/Wrappers/Orbwalking/*`, but it is not complete and must stay marked partial.
- [x] Math/Prediction tree exists, is exposed through `SDK.h`, and runtime validation for the current line collision / AoE / HealthPrediction path was reported OK.
- [~] Items data exists from CDragon and Core inventory access exists; `Wrappers/Items.h` is still missing at the EnsoulSharp.SDK path. `Wrappers/Map.h` is missing; `SDK/Core/Map.h` backend exists.
- [x] No SharpDX port is needed on C++; use NightSharp `Vec2` / `Vec3` helpers instead.
- [~] UI/menu base exists. `UI/IMenu/Customizer` and `UI/IMenu/Skins` are intentionally excluded.
- [~] Utils exist, but need member-by-member parity review.

Recheck notes 2026-06-27:
- [x] `NightSharp/SDK/Math/*` file coverage is now mostly present.
- [x] `PredictionInput.Type` canonical type is now `SpellType`, with `SkillshotType` adapter compatibility.
- [x] `PredictionInput` has DLL-shape fields/fallbacks: `Spell`, `Unit = GameObjects::Player()`, `From`, `RangeCheckFrom`, `CollisionObjectsBridge`, `MaxCollisionCount`.
- [x] `Movement::GetPrediction` has DLL-style Yuumi/collision block: calls `Collisions.GetCollision`, compares `collision.Count > MaxCollisionCount`, sets `OriginHitchance`, sets `Hitchance = Collision`, and assigns `CollisionObjects`.
- [x] `Collision.h` was reworked toward DLL `EnsoulSharp.SDK.Collisions`, including Heroes/Minions/Jungle/Building/Walls/YasuoWall/SamiraW and NightSharp extension MelW.
- [x] `SDK::Prediction`, `SDK::HealthPrediction`, and `SDK::AoEPrediction` facades exist.
- [x] `Geometry.h` has DLL polygon ops, `SDK::Geometry::Sector`, DLL-compatible `Sector.RotateLineFromPoint`, and math-only `GetCenter/GetCenteredText` helpers.
- [x] `EzrealSemiPlugin` now compile-tests prediction/collision with Q setup `SpellSlot.Q, 1200`, `SetSkillshot(0.25, 60, 2000, true, SpellType.Line)`, draws prediction cast point, and blocks cast on collision.
- [x] Recheck build pass: `NightSharp.sln Release|x64` with MSBuild from `E:\Visual Studio`.
- [x] `SDK.h` exposes Math explicitly: `Math/Prediction.h`, `Math/HealthPrediction.h`, `Math/Geometry.h`, `Math/ConvexHull.h`, and `Math/Collision.h`.
- [x] `Spell.h` is wired into Math/Damage backends: `Prediction::GetPrediction`, `Collision::GetCollision`, `HealthPrediction::GetPrediction`, and `Damage::GetSpellDamage`.

Source tree recheck 2026-06-28:
- [x] Math runtime test reported OK: line collision minion/hero, Yasuo Wind Wall, Samira W, Mel W, AoE circle/cone/line, and HealthPrediction timing.
- [x] `Math/Prediction/GamePath.h` is a public header and no longer duplicates `Movement.h`.
- [~] `Wrappers/TargetSelector/*` files are mostly present; missing source-level parity files are the weight modes listed below.
- [~] `Wrappers/Orbwalking/*` files are present, but Orbwalking is still not complete.
- [ ] Remaining missing paths after excluding SharpDX, `UI/IMenu/Customizer`, and `UI/IMenu/Skins`: `Bootstrap.h`, `Constants.h`, top-level `GameObjects.h`, `Variables.h`, `UI/Utils.h`, `Wrappers/Items.h`, `Wrappers/Map.h`, `_ZiggsR.h`, and seven TargetSelector weight files.

Damage recheck 2026-06-28:
- [x] CDragon latest champion `Locke` exists as id `805`, alias `Locke`.
- [x] Added Locke spell damage data to both damage databases: Q missile, Q nail consume, E arrival, E dash, and R.
- [x] Added Locke passive `Silver Stake` to `DamagePassives.h` as magic on-hit damage scaling from min to max by target missing health.
- [x] Damage data table count is now 173 entries and remains alphabetically sorted for binary search.
- [x] CDragon item/rune database generation is in place:
  - `tools/generate_cdragon_item_rune_data.py`
  - `NightSharp/SDK/Data/ItemData.h`: 706 items with id/name/raw text/categories/build paths/stats/active flag/cooldown/duration/range metadata.
  - `NightSharp/SDK/Data/RuneData.h`: 103 perks and 5 perk styles with tree/slot metadata/cooldown/duration text-derived fields.
  - `InventorySlot` and `RuneManagerClient` can resolve Core runtime ids into generated metadata.
- [x] Damage wrapper parity is complete for current static data and public wrapper calls:
  - `SDK::Damage` public surface now has `CalculateDamage`, `CalculateMixedDamage`, `GetSpellDamage`, `GetPassiveDamage`, and `GetAutoAttackDamage`.
  - `DamageLibrary` exposes stage damage type so spell damage no longer applies all modifiers as Physical.
  - `DamageMastery` applies current rune modifiers: Coup de Grace, Cut Down, Last Stand, Press the Attack, and First Strike via runtime buff/debuff aliases.
  - `BonusHealth` scaling no longer falls back to full `MaxHealth`; it uses item/rune static data exposed through inventory and rune manager.

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
- [x] Wire `Spell::GetPrediction` to `SDK::Prediction::GetPrediction`.
- [x] Wire `Spell::GetCollision` and position overloads to `SDK::Collision/SDK::Collisions`.
- [ ] Restore `Spell::GetTarget` after TargetSelector is exposed through `SDK.h` and runtime validated.
- [x] Wire `Spell::GetDamage` / `CanKill` to `SDK::Damage::GetSpellDamage`.
- [x] Wire `Spell::GetHealthPrediction` to `SDK::HealthPrediction`.
- [x] Add `Spell::SetSkillshot(..., SpellType)` overloads for DLL-style `SpellType.Line/Circle/Cone/Arc` setup.
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

- [x] Port/update `Math/Geometry.h`.
  - Has DLL polygon ops, `SDK::Geometry::Sector`, DLL-compatible `Sector.RotateLineFromPoint`, and math-only `GetCenter/GetCenteredText`.
  - Does not port SharpDX `Sprite/Font` text-measuring overloads.
- [x] Port/review `Math/ConvexHull.h`.
  - File/API exists and current behavior is accepted; only rework if deterministic source-level parity becomes necessary.
- [x] Port/update `Math/Prediction/Cluster.h`.
  - `SDK::AoEPrediction` facade exists.
- [x] Port/update `Math/Prediction/GamePath.h`.
  - `GamePath::PathTracker` now lives in the separate header and is included by `Math/Prediction.h`.
- [x] Port/update `Math/Prediction/Health.h`.
  - `SDK::HealthPrediction` facade/registry exists.
- [x] Port/update `Math/Prediction/Movement.h`.
  - Main source/DLL API exists, Yuumi and collision block are ported, and current runtime validation was reported OK.
- [x] Review/rework current `Math/Collision.h` against DLL `EnsoulSharp.SDK.Collisions`.
  - Has `SDK::Collisions`, `GetCollision`, `IsCollision`, `WillDead`, Heroes/Minions/Jungle/Building/Walls/Yasuo/Samira plus NightSharp MelW extension.
  - Caveat: Yasuo Wind Wall orientation still uses NightSharp `Direction()` fallback because matrix orientation is not exposed.
- [x] Review current `Math/Polygons/*` and fill known API gaps.
  - `SectorPoly` now has DLL-compatible `RotateLineFromPoint`.
- [x] Add compile-test plugin for prediction/collision.
  - `EzrealSemiPlugin` calls `SDK::Prediction::GetPrediction(input)`, blocks cast on collision, and draws cast/collision points.
- [x] Runtime-test plugins/checklist for:
  - movement prediction point,
  - collision object detection,
  - polygon containment/intersection,
  - health prediction timing.

### P3 - TargetSelector

The main tree exists under `NightSharp/SDK/Wrappers/TargetSelector/`, but parity is still partial:

- [x] `HeroVisibleEntry.h`
- [x] `ITargetSelectorMode.h`
- [x] `TargetSelector.h`
- [x] `TargetSelectorDrawing.h`
- [x] `TargetSelectorHumanizer.h`
- [x] `TargetSelectorMode.h`
- [x] `TargetSelectorSelected.h`
- [x] `Modes/Closest.h`
- [x] `Modes/IWeightItem.h`
- [x] `Modes/LeastHealth.h`
- [x] `Modes/LessAttacksToKill.h`
- [x] `Modes/LessCastsToKill.h`
- [x] `Modes/MostAbilityPower.h`
- [x] `Modes/MostAttackDamage.h`
- [x] `Modes/NearMouse.h`
- [x] `Modes/Priority.h`
- [x] `Modes/PriorityCategory.h`
- [x] `Modes/Weight.h`
- [x] `Modes/WeightItemWrapper.h`
- [x] `Modes/Weights/AbilityPower.h`
- [x] `Modes/Weights/Aggro.h`
- [x] `Modes/Weights/AttackDamage.h`
- [x] `Modes/Weights/CrowdControl.h`
- [ ] `Modes/Weights/FocusMe.h`
- [ ] `Modes/Weights/Gold.h`
- [x] `Modes/Weights/Killable.h`
- [ ] `Modes/Weights/LowHealth.h`
- [ ] `Modes/Weights/LowResists.h`
- [ ] `Modes/Weights/ShortDistanceCursor.h`
- [ ] `Modes/Weights/ShortDistancePlayer.h`
- [ ] `Modes/Weights/TeamFocus.h`
- [ ] Expose TargetSelector headers through `NightSharp/SDK/SDK.h` after include-order is confirmed.
- [ ] Runtime-test selection order, selected target focus, humanizer delay, damage-based modes, and no-collision target selection.

Dependencies:
- `GameObjects::EnemyHeroes` / hero cache correctness.
- Damage wrapper parity for killable/casts-to-kill modes.
- Cursor/world position parity for `NearMouse`.
- Visibility/fog/object validity accessors.

### P4 - Orbwalking

Files exist under `NightSharp/SDK/Wrappers/Orbwalking/`, but Orbwalking is not complete:

- [~] `Orbwalker.h`
- [~] `OrbwalkerBase.h`
- [~] `OrbwalkerSelector.h`
- [x] `OrbwalkingActionArgs.h` exists as a NightSharp helper.
- [ ] Expose Orbwalking headers through `NightSharp/SDK/SDK.h` only after runtime safety review.

Dependencies:
- `Utils/AutoAttack.h` parity review.
- `Utils/Cursor.h` parity review.
- `Events/Turret.h`, `Events/Dash.h`, `Events/Gapcloser.h` semantics.
- Spell/basic attack cast timing.
- Input/click control safety.
- Runtime confirmation that attack orders, movement orders, windup cancel, missile launch state, and mode switching do not cause unsafe input spam.

Recommended implementation order:
- [ ] Read-only attack timer and target selection.
- [ ] Movement/click issue API behind a safety toggle.
- [ ] Full combo/harass/laneclear modes.

### P5 - Items And Map Wrappers

- [x] Generate static CDragon item/rune database:
  - `SDK/Data/ItemData.h`
  - `SDK/Data/RuneData.h`
  - source script: `tools/generate_cdragon_item_rune_data.py`
- [x] Wire generated item/rune data to Core runtime ids:
  - `InventorySlot::DatabaseEntry()` / `CDragonData()`
  - `RuneManagerClient::RuneDataEntries()`, `PrimaryTreeData()`, `SecondaryTreeData()`
  - `GameData::GetItemInfoById` falls back to generated CDragon `ItemInfo` stats.
- [ ] Port `Wrappers/Items.h`.
- [ ] Port `Wrappers/Map.h`.
- [~] Use existing `NightSharp/SDK/Core/Map.h` as backend for `Wrappers/Map.h`.
- [CORE] Verify item charges/cooldown/cast offsets only if `Wrappers/Items.h` needs live active item state beyond static CDragon metadata and inventory item id.
- [DATA] Map metadata should come from `EnsoulSharp.SDK/Resources/Data/Map.json` if compatible.

### P6 - Core-Like Top-Level SDK Files

These are path/name parity tasks. NightSharp already has equivalent concepts in different files, so most work is facade/alias/API shape:

- [ ] `Bootstrap.h`
- [ ] `Constants.h`
- [~] `GameObjects.h`
  - Backend exists at `NightSharp/SDK/GameObjects/GameObjects.h`; top-level facade path still missing.
- [~] `Variables.h`
  - Backend exists at `NightSharp/SDK/Core/Variables.h`; top-level facade path still missing.

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
- [x] `Wrappers/Damages/Damage.h`
- [x] `Wrappers/Damages/DamageData.h` (NightSharp-only helper)
- [x] `Wrappers/Damages/DamageJson.h`
- [x] `Wrappers/Damages/DamageLibrary.h`
- [x] `Wrappers/Damages/DamageMastery.h`
- [x] `Wrappers/Damages/DamagePassives.h`

TODO:
- [x] Provide current backend entry point `SDK::Damage::GetSpellDamage`.
- [x] Add Locke from CDragon latest to static damage data and passive on-hit logic.
- [x] Compare public methods and overloads against EnsoulSharp.SDK.
- [x] Verify `DamageStage`, `DamageType`, raw stats, armor/mr, crit/on-hit fields.
- [x] Restore calls from `Spell::GetDamage` to `SDK::Damage::GetSpellDamage`.
- [x] Load/generate data from current NightSharp/CDragon data source.

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
- [~] `GameObjects.h`
  - Equivalent exists at `GameObjects/GameObjects.h`; top-level facade path is still missing.
- [x] `Math/ConvexHull.h`
  - File exists and current behavior is accepted; deterministic source-level rework is optional.
- [x] `Math/Geometry.h`
- [x] `Math/Prediction/Cluster.h`
- [x] `Math/Prediction/GamePath.h`
  - `GamePath::PathTracker` now lives in the separate header and is included by `Math/Prediction.h`.
- [x] `Math/Prediction/Health.h`
- [x] `Math/Prediction/Movement.h`
  - File exists, main backend works, and current runtime validation was reported OK.
- [ ] `UI/Utils.h`
- [~] `Variables.h`
  - Equivalent exists at `Core/Variables.h`; top-level facade path is still missing.
- [ ] `Wrappers/Items.h`
- [ ] `Wrappers/Map.h`
- [~] `Wrappers/Orbwalking/Orbwalker.h`
- [~] `Wrappers/Orbwalking/OrbwalkerBase.h`
- [~] `Wrappers/Orbwalking/OrbwalkerSelector.h`
- [ ] `Wrappers/Spells/Tracker/Skillshots/_ZiggsR.h`
- [x] `Wrappers/TargetSelector/HeroVisibleEntry.h`
- [x] `Wrappers/TargetSelector/ITargetSelectorMode.h`
- [x] `Wrappers/TargetSelector/TargetSelector.h`
- [x] `Wrappers/TargetSelector/TargetSelectorDrawing.h`
- [x] `Wrappers/TargetSelector/TargetSelectorHumanizer.h`
- [x] `Wrappers/TargetSelector/TargetSelectorMode.h`
- [x] `Wrappers/TargetSelector/TargetSelectorSelected.h`
- [x] `Wrappers/TargetSelector/Modes/Closest.h`
- [x] `Wrappers/TargetSelector/Modes/IWeightItem.h`
- [x] `Wrappers/TargetSelector/Modes/LeastHealth.h`
- [x] `Wrappers/TargetSelector/Modes/LessAttacksToKill.h`
- [x] `Wrappers/TargetSelector/Modes/LessCastsToKill.h`
- [x] `Wrappers/TargetSelector/Modes/MostAbilityPower.h`
- [x] `Wrappers/TargetSelector/Modes/MostAttackDamage.h`
- [x] `Wrappers/TargetSelector/Modes/NearMouse.h`
- [x] `Wrappers/TargetSelector/Modes/Priority.h`
- [x] `Wrappers/TargetSelector/Modes/PriorityCategory.h`
- [x] `Wrappers/TargetSelector/Modes/Weight.h`
- [x] `Wrappers/TargetSelector/Modes/WeightItemWrapper.h`
- [x] `Wrappers/TargetSelector/Modes/Weights/AbilityPower.h`
- [x] `Wrappers/TargetSelector/Modes/Weights/Aggro.h`
- [x] `Wrappers/TargetSelector/Modes/Weights/AttackDamage.h`
- [x] `Wrappers/TargetSelector/Modes/Weights/CrowdControl.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/FocusMe.h`
- [ ] `Wrappers/TargetSelector/Modes/Weights/Gold.h`
- [x] `Wrappers/TargetSelector/Modes/Weights/Killable.h`
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

3. [x] Wire `Spell.h` into the newly ported Math backends.
   - `Spell::GetPrediction` calls `SDK::Prediction::GetPrediction(input)`.
   - `Spell::GetCollision` calls `SDK::Collision::GetCollision`.
   - `Spell::GetHealthPrediction` calls `SDK::HealthPrediction`.
   - `Spell::GetDamage` calls `SDK::Damage::GetSpellDamage`.
   - Keep EzrealSemiPlugin as the first runtime probe for line prediction/collision.

4. [x] Cleanup Math/public surface.
   - Explicit `SDK.h` includes added for `Math/Prediction.h`, `Math/HealthPrediction.h`, `Math/Geometry.h`, `Math/ConvexHull.h`, and `Math/Collision.h`.
   - `GamePath::PathTracker` was moved out of `Prediction/Movement.h`; `Prediction/GamePath.h` is now the public source and is included by `Math/Prediction.h`.
   - Runtime-test line collision minion/hero, Yasuo Wind Wall, Samira W, Mel W, AoE circle/cone/line, and HealthPrediction timing was reported OK.
   - Only rework `ConvexHull` algorithm if deterministic source-level parity becomes necessary.

5. [SDK] Finish `TargetSelector`.
   - Existing tree is present, but still needs `SDK.h` exposure, missing weight modes, and runtime selection validation.
   - Add missing weights: `FocusMe`, `Gold`, `LowHealth`, `LowResists`, `ShortDistanceCursor`, `ShortDistancePlayer`, `TeamFocus`.
   - This unlocks `Spell::GetTarget` and common champion script usage.

6. [x] Finish Damage wrapper parity.
   - Locke data/passive, CDragon item/rune static databases, `SDK::Damage` public overloads, spell-stage damage type routing, rune modifiers, and `BonusHealth` scaling are wired.
   - Build is clean and `Spell::GetDamage` / `Spell::CanKill` route through the updated damage wrapper.
   - TargetSelector killable/casts-to-kill can now be wired.
   - Active item cast/cooldown wrappers remain part of item wrapper parity, not Damage wrapper parity.

7. [SDK] Finish Spell tracker special cases.
   - Port `Wrappers/Spells/Tracker/Skillshots/_ZiggsR.h`.
   - Add a read-only tracker debug plugin showing active skillshots and detection source.
   - This validates `GameObject.OnCreate` + `MissileClient.OnCreate` matching EnsoulSharp.SDK.

8. [SDK] Finish `Orbwalking` only after TargetSelector, AutoAttack, Cursor, and attack timing are stable.
   - Files exist, but Orbwalking is still not complete.
   - Keep read-only/runtime validation first.
   - Gate movement/click issuing behind a safety toggle.

9. [SDK/DATA] Port `Wrappers/Items.h`, `Wrappers/Map.h`, and data generation/copy.
   - Item/rune data generation from CDragon is complete; keep the generator as source of truth when Riot data updates.
   - Use current `Core/Map.h` backend for map facade.
   - Reverse item cooldown/charges/cast only if needed by public API.

10. [SDK] Review Events and Utils member-by-member.
   - Close `Turret`, `Stealth`, `Teleport`, `DelayAction`, `Minion`, `Jungle`, `Cursor`, and `Render` parity gaps.

After each item:
- update this TODO,
- update `EnsoulSharp_CoreParity_Audit.md` if native offsets or core behavior changed,
- expose new public headers through `NightSharp/SDK/SDK.h`,
- build `Release|x64`.
