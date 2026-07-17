# KuroAIO AI shared-helper audit

Audit date: 2026-07-17  
Scope: the six completed full controllers (`Aatrox` through `Ambessa`),
`AIChampionEngine.h`, `AIControllerHelpers.h` and `AIGeometry.h`.

## Rule

Only champion-neutral observation, geometry and runtime plumbing belongs in a
shared helper. Spell ordering, combo transitions, target policy, damage gates,
postures and callback interpretation stay inside `AI<Champion>Controller.h`.
Identical orchestration names therefore do not prove duplicate behavior.

## Extracted in this pass

- `Now`: one tick-count authority for controllers that use dense timing state.
- `CurrentResource`: safe mana/energy observation with an optional cap.
- `PlayerMobilityLocked`: one Grounded/Snare/Stun/Knockup/Knockback/Suppression
  query. This also closes Alistar's prior stun/suppression omission.
- `AutoAttackRange` and `InAutoAttackRange`: target-radius-aware base AA range.
  Champion bonuses such as Ambessa's passive range remain explicit arguments.
- `CaptureLocalAutoAttack` and `CaptureAfterAttack`: neutral target/tick facts;
  passive consumption and weave transitions remain champion-owned.
- `CaptureInterruptable`: neutral target and normalized event lifetime only.
- `IsLocalPlayer`: now replaces duplicated local-buff ownership functions.
- `AIChampionEngine::WasControllerCast`: one ownership-window check shared by
  manual-input arbitration and champion callback observers.
- Existing `CountEnemiesAt`/`CountAlliesAt` engine helpers are called directly
  instead of being hidden behind repeated one-line controller aliases.

## Mechanical result

After extraction, the exact cross-file function-body scan reports no repeated
runtime implementation. The only remaining exact match is the one-line
`OnInterruptable` adapter in Aatrox, Ahri and Akali. Each adapter binds that
controller's own state variables to `CaptureInterruptable`; it is callback
wiring, not another implementation of the lifetime logic.

Near-match review also covered AA-range checks, buff-state queries, unload
callbacks, missile refreshers, target-immunity filters and posture labels.
Only the champion-neutral AA base was extracted. The rest differ in live spell
names, state ownership, immunity semantics or cleanup responsibilities and are
intentionally local.

## Guardrail for later champions

Before adding a new helper, prove that its inputs and output mean the same
thing for at least two controllers. Do not parameterize an entire combo merely
to make source text look shorter. Re-run exact and near-match scans after every
new champion and extend this file only when a shared semantic unit is found.
