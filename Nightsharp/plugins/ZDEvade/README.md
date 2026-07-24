# ZDEvade

Active ZDEvade pipeline for NightSharp: threat detection, geometry evaluation, route planning, and controller-driven evade movement. Runtime policy assumes only one evade engine is loaded at a time.

## Active architecture

```
SDK events / missiles / objects
        |
        v
ThreatDetector  --->  Threat store (snapshot + change serial)
        |                      |
        |                      v
ThreatDetectionPolicy   SpellDatabase / SpellData schema
(pure cast/bind helpers)         |
                                 v
EvadeController  <---  EvadePlanner + EvadeGeometry
        |                      ^
        |                      |
EvadeRoutingPolicy (pure) -----+  coverage, strict lock, intent, release rules
        |
        v
EvadeCommandEngine  --->  local guarded orbwalker/CoreEvadeState control
        |
        v
SDK Orbwalker move/attack flags
```

Runtime classes translate SDK state into pure policy inputs. Standalone tests compile the policy and geometry headers directly without live game objects.

| Layer | Primary headers | Role |
| --- | --- | --- |
| Plugin shell | `ZDEvade.h` | Menu, render overlays, wires `ThreatDetector` + `EvadeController` |
| Detection | `ThreatDetector.h`, `ThreatDetectionPolicy.h`, `Threat.h` | Cast/missile/object ingestion, coalescing, threat snapshots |
| Schema | `SpellData.h`, `SpellDatabase.h` | Authored spell geometry, timing, missile route mode |
| Geometry | `EvadeGeometry.h`, `EvadePlanner.h` | Safe/unsafe evaluation, candidate seed generation |
| Policy | `EvadeRoutingPolicy.h`, `EvadeMoveResultAdapter.h` | Pure decision helpers consumed by controller/tests |
| Control | `EvadeController.h`, `EvadeCommandEngine.h` | Plan/lock/release loop and orbwalker command issuance |
| Shared state | `core/CoreEvadeState.h` | Fixed-capacity process-wide aggregation; ZDEvade uses an explicit generation-safe owner while legacy callers retain a reserved slot |

## Standalone regression tests

From `Nightsharp`, after initializing VS18 x64 tools:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && powershell -ExecutionPolicy Bypass -File tests\run_zdevade_tests.ps1'
```

The runner requires all four suites, compiles each source with `/std:c++20 /EHsc /W4 /DNOMINMAX /I .`, writes artifacts only to `build\tests\`, fails fast on the first compile/run failure, and exits non-zero on any failure.

Expected success markers:

```
ALL ZDEVADE ROUTING POLICY TESTS PASSED
ALL ZDEVADE GEOMETRY RUNTIME TESTS PASSED
ALL ZDEVADE CONTROLLER POLICY TESTS PASSED
ALL ZDEVADE DETECTOR POLICY TESTS PASSED
ZDEVADE TEST RUNNER PASSED: all 4 suites green
```

## Release build fallback

Default project build:

```bat
build1.bat
```

When parallel MSBuild causes flaky or hard-to-read failures, use a serial Release x64 build:

```bat
"%ProgramFiles%\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" NightSharp.sln /p:Configuration=Release /p:Platform=x64 /m:1
```

Adjust the VS install path if Community is not installed.

## Coverage matrix

| Suite | Source | Modules under test | Regression focus |
| --- | --- | --- | --- |
| Routing policy | `tests/zdevade_routing_policy_test.cpp` | `EvadeRoutingPolicy.h` | `ThreatCoverage` ordering, detour/resume rules, strict-route commitment, deferred unsafe intent |
| Geometry runtime | `tests/zdevade_geometry_runtime_test.cpp` | `EvadeGeometry.h`, `EvadePlanner.h`, `Threat.h`, `SpellDatabase.h` | Line/circle/cone/ring geometry, blink vs dash, polyline threat dedup, arrival/observed route timing, candidate quotas, nav sampling |
| Controller policy | `tests/zdevade_controller_policy_test.cpp` | `EvadeRoutingPolicy.h`, `EvadeMoveResultAdapter.h` | No-plan release/stop, spell-hold suppression, move-result lock invalidation, manual-over-orb intent, strict-lock retention, local conditional restore |
| Detector policy | `tests/zdevade_detector_policy_test.cpp` | `ThreatDetectionPolicy.h` | ProcessSpell/DoCast coalescing, queue FIFO/replacement, delayed missile bind window, multi-projectile binding, projected speed / steering route helpers, Sion charge facing |

Shared fixtures live in `tests/ZDEvadeTestSupport.h`. Test executables are standalone `main()` programs and are not linked into `NightSharp.dll`.

## Unsupported Arc gate

Arc is conservatively unsupported until schema explicitly opts in:

- `SpellData::arcSupported=false` is the default; only verified database entries may set `true`.
- Detector admission drops unsupported Arc and exposes a diagnostic counter.
- Planner returns no Arc intersection boundary, so unsupported Arc cannot produce safe candidate seeds.
- Chord heuristics must not declare an Arc route safe.

Current active database has no verified Arc entries.

## One-evade runtime semantics

ZDEvade does not modify KuroEvade or EzEvade ownership code. Instead:

- `CanLoad()` refuses initial ZDEvade load while KuroEvade or EzEvade is loaded.
- ZDEvade acquires and reuses one explicit `CoreEvadeState` owner token. Control begin/end updates only that owner, and destruction releases it.
- KuroEvade remains an unchanged legacy caller whose compatibility functions update only the reserved legacy owner; EzEvade remains unchanged and is not assigned an explicit token.
- If another evade appears later, ZDEvade suspends movement/input intervention and clears only its explicit owner state and local bookkeeping. After that clear, it leaves orb/Core state untouched when the aggregate still reports another active owner; otherwise it conditionally restores only unchanged flags on the same orbwalker implementation.
- Normal local release restores orbwalker flags only for the same implementation and only when values still match what ZDEvade imposed.
- Fixed owner-slot exhaustion makes ZDEvade control acquisition fail without writing legacy or aggregate globals.
- Users must unload the current evade before selecting another one.

Manual intent always wins over later orbwalker steps; controller targets never become deferred user intent.

## Manual in-game acceptance matrix

Standalone tests prove numerical/state policy. Before calling a wave complete in live play, manually verify:

| Scenario | Pass criteria |
| --- | --- |
| Line skillshot | Evade triggers before impact; chosen exit stays outside hitbox |
| Circular AoE | Center/radius timing matches drawn danger; no false-safe stand-in-center |
| Ring | Inner-hole routes detour; outer band respected |
| Cone | Sector angle/padding match observed hitbox; no radius-as-expansion false safe |
| Overlapping threats | Planner prefers lower coverage tuple; strict lock only changes on real invalidation |
| Blink spell | Origin occupancy through cast completion; no swept segment false safe |
| Dash spell | Continuous segment collision behaves conservatively |
| Manual click while evading | Manual destination executes and is not overwritten next frame |
| Orbwalker step during evade | Orb step ignored when newer manual intent exists |
| Sion R / charge facing | Detected travel direction matches champion facing, not legacy perpendicular wiring |
| Delayed end explosion | Explosion danger starts at unified arrival helper + delay |
| Wall corner / tight nav | Straight segment rejected when hero-radius clearance fails (Wave 4 nav sweep) |

Record champion, spell, and observed behavior when any row fails; add a RED standalone fixture before changing production logic.
