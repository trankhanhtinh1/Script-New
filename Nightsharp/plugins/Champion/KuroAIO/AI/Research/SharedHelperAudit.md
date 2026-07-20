# KuroAIO AI shared-helper audit

Audit date: 2026-07-18
Scope: the eighteen completed full controllers (`Aatrox` through `Ryze`),
`AIChampionEngine.h`, `AIControllerHelpers.h` and `AIGeometry.h`.

## Rule

Only champion-neutral observation, geometry and runtime plumbing belongs in a
shared helper. Spell ordering, combo transitions, target policy, damage gates,
postures and callback interpretation stay inside `AI<Champion>Controller.h`.
Identical orchestration names therefore do not prove duplicate behavior.

## Extracted in this pass

- `AIGeometry::RankValue`: one clamped rank-to-array lookup shared by Aurora,
  Azir and Bard geometry. Their rank tables and spell formulas remain local;
  the repeated clamp/index implementation no longer does.
- `Now`: one tick-count authority for controllers that use dense timing state.
- `CurrentResource`: safe mana/energy observation with an optional cap.
- `PlayerManaPercent`: one local-player lookup and invalid-player fallback,
  replacing the identical wrappers in Anivia, Annie, Aphelios, Ashe and
  Aurelion Sol while every champion keeps its own mana thresholds.
- `PlayerMobilityLocked`: one Grounded/Snare/Stun/Knockup/Knockback/Suppression
  query. This also closes Alistar's prior stun/suppression omission.
- `AutoAttackRange` and `InAutoAttackRange`: target-radius-aware base AA range.
  Champion bonuses such as Ambessa's passive range remain explicit arguments.
- `Ready`: one safe runtime-spell readiness query. This removed identical
  Ambessa/Amumu wrappers and is also the readiness gate used by the shared cast
  throttle.
- The responsive `CastThrottleReady(index, fastFollowup)` overload: one 38 ms
  ordinary/zero-delay follow-up policy shared by Ambessa and Amumu. Controllers
  with materially different pacing continue to pass their own values.
- `CaptureLocalAutoAttack` and `CaptureAfterAttack`: neutral target/tick facts;
  passive consumption and weave transitions remain champion-owned.
- `CaptureBeforeAttack`: the same neutral orbwalker target/tick payload with an
  auditable callback-specific name. This removed the identical Azir/Ryze
  callback bodies without moving either champion's attack policy.
- `CaptureInterruptable`: neutral target and normalized event lifetime only.
- `CaptureGapcloserEvent` and `CaptureInterruptableEvent`: callback-signature
  binders for neutral event facts. Each controller still supplies its own
  proximity/lifetime policy and owns every reaction.
- `CaptureLocalAutoAttackEvent`, `CaptureAfterAttackEvent` and
  `ForwardBuffStateEvent`: typed callback forwarders that removed repeated
  one-line adapters without moving passive or combo state into shared code.
- `HasCurrentResource`, `ReadySpellResource` and `HasResourceFor`: one safe
  ready-spell resource sum. This replaces repeated mana-package loops while
  preserving champion-specific reserves at each call site.
- `PredictionAtLeast`, `EnemyFlashReady` and `SpellEventNameContains`: neutral
  SDK observation helpers; hitchance, Flash punishment and spell-name meaning
  remain champion-owned policy.
- `IsLocalPlayer`: now replaces duplicated local-buff ownership functions.
- `AIChampionEngine::WasControllerCast`: one ownership-window check shared by
  manual-input arbitration and champion callback observers.
- `CountAlliedFollowup`: one range-squared ally count with explicit local-player
  inclusion. Engage, landing and bridge policy remain champion-owned.
- `AllyProtectionPriority` and `SelectProtectionAlly`: one offense/range/health
  ranking and targeted-threat adjustment shared by Alistar and Amumu. Threat
  selection and peel spell order remain inside each champion.
- Existing `CountEnemiesAt`/`CountAlliesAt` engine helpers are called directly
  instead of being hidden behind repeated one-line controller aliases.
- `CursorDirectionAgrees`: one player-to-destination versus player-to-cursor
  dot-product query. Amumu keeps its role-menu policy locally; Anivia and
  Annie call the neutral observation directly.
- `ValidHostileUnitInGameplayRange`: one hostile cast-range check including
  target gameplay radius, replacing identical Anivia/Annie farm wrappers.
- `HasNearbyJungleTarget`: one safe nearby-neutral query, replacing identical
  Anivia/Annie jungle-mode inference loops.
- `ObjectEventIsAllied`: one object-event team fallback for lifecycle payloads
  whose team is zero, replacing identical Anivia/Annie ownership functions.
- `DispatchLocalOrOtherSpellEvent`: a typed local-versus-other ProcessSpell
  dispatcher now wired directly by Amumu, Anivia, Annie, Aurelion Sol and
  Aurora. Each supplied callback still owns its champion-specific
  interpretation.
- `IsCommonUntargetableOrImmune`: one targetability, immunity, stasis and
  common untargetable-state predicate. Spell-shield policy remains separate
  because each champion values shield consumption differently.
- `SelectJungleTarget`: one range/health-weight/epic-priority neutral selector,
  replacing the near-identical Ambessa and Amumu jungle ranking loops.
- Annie reuses the existing `NearestEnemyToPlayer` helper instead of adding a
  local nearest-pursuer implementation.
- `TextContainsAny` and `AnyTextContains`: one null-safe, case-insensitive
  multi-token observation path. Ahri, Akshan, Anivia and Annie retain
  champion-vocabulary predicates, but their repeated chains of three to six
  `Engine::TextContains` calls now delegate to this helper.
- `HasAnyBuff`: one null-safe active-buff membership loop. Akali passive-ring
  and Akshan R-channel predicates now supply only their champion-specific
  aliases instead of repeating player validity and `HasBuff` plumbing.
- `SpellEventNameContainsAny`: one multi-token wrapper over the existing
  `SpellEventNameContains` payload query. It removed repeated spell/script/
  payload OR chains from Aphelios and the multi-alias Anivia/Annie observers.
- `SpellSlotOrEventNameContainsAny`: one slot-or-runtime-vocabulary query for
  spell observers. Aurelion Sol and Aurora now provide only their own Q/W/E/R
  aliases.
- `ProjectileWallBlocks` and `ProjectileWallBlocksFromPlayer`: one validity,
  nonzero-radius and SDK projectile-wall query. Aphelios' five local checks and
  Annie's Q wrapper now delegate to it; Ashe uses it for the exact selected W
  ray and the first-champion R segment.
- `ProjectileWallFirstContact` and
  `ProjectileWallFirstContactFromPlayer`: one shared prefix bisection for
  endpoint spells that detonate at the first Yasuo/Samira/Mel barrier rather
  than disappearing.
- `MaximumBuffCount`: one validity and maximum-alias stack query, replacing
  repeated mixed-case/manager count plumbing in Akshan, Alistar, Aphelios and
  Ashe. The semantic meaning of each stack remains controller-owned.
- `HasEnemyChampionNear`: one visibility/targetability/range scan, replacing
  identical Annie and Ashe farm/scout aliases.
- `BuffExpireTick`: one event-end/fallback normalization, replacing identical
  Aphelios and Ashe adapters while leaving every buff's fallback duration at
  its champion call site.
- `FindEnemyCastWindow`, `EnemyCastWindowCommitted` and
  `EnemyCastWindowHardCrowdControlSpent`: bounded neutral record reuse and
  expiry queries shared by Aurelion Sol and Aurora. Their CC meaning and all
  resulting W/R policy remain champion-owned.
- `FindValidRecordById`: one `Id`/`Valid` vector lookup, replacing Ashe's
  duplicated Volley/Arrow record scans.
- `IsLargeLaneMinion`: one Siege/Super flag interpretation shared by Aurora,
  Aurelion Sol and Azir. Champion-specific farm scoring and execute policy
  remain at their call sites.
- `HasNearbyEpicMonster`: one valid/live/range scan shared by Aphelios and
  Bard. Weapon rotation, shrine setup and objective commitment remain inside
  their respective controllers.
- `SpellInstanceContains`, `HeroHasSummonerSpellToken` and `HeroHasSmite`:
  one null-safe inspection of both summoner slots, including name, script name
  and icon fallback. This replaced Ashe's enemy-jungler implementation and
  Amumu's local-role implementation before Blitzcrank reused the same fact for
  objective-jungler displacement. Scouting, role selection and hook policy
  remain controller-owned.
- `RawEnemyHeroByNetworkId` and `RawAllyHeroByNetworkId`: identity-preserving
  hero lookup that intentionally does not apply visibility/targetability
  gameplay gates. Blitzcrank and Ryze now share it for event reconciliation;
  ordinary targeting continues to use the stricter valid-enemy helper.
- `SolveMovingCircleContactTime2D`: one bounded analytical moving-circle
  contact solver shared by Blitzcrank Rocket Grab and Ryze Overload. Each
  controller still owns missile radius/range, valid bodies, first-contact
  semantics, splash effects and the decision to cast.
- Azir's controller and geometry reuse the pure
  `OrbwalkerKuro::AzirSoldierSupport` constants, target legality and damage
  arithmetic instead of cloning a second Sand Soldier ruleset under `AI`.
- Bel'Veth reuses shared cast-window allocation, spell-slot/event-name
  normalization, local/manual ownership observation, prediction, target/unit
  lookup, cursor agreement, point-click and anti-dash hazard registries,
  object-team fallback, gapcloser/interrupt capture, cast throttling, Epic
  classification and `AIGeometry::RankValue`. Its four-sector HUD
  calibration, W sector refunds, forced E victim and global coral economy stay
  local because those semantics do not exist on another completed champion.
- Blitzcrank reuses shared prediction, projectile-wall, spell/buff vocabulary,
  protected-ally, cast-window, Smite-holder, gapcloser, interrupt, resource and
  cast-throttle plumbing. Moving first-body/lollipop collision, pull archetype
  safety, W-E pressure, exact Power Fist timing and per-target Static Field
  queues remain local kit semantics.
- Ryze reuses shared moving contact, prediction, projectile-wall, raw/valid
  unit lookup, before/after-attack capture, protected-ally selection,
  point-click threat, gapcloser/interrupt, resource and cast-throttle
  plumbing. Independent Rune/Flux ledgers, Q reset cadence, indirect E-Q,
  delayed E-E-Q and Realm Warp no-abduction policy remain local kit semantics.

## Mechanical result

After Ryze integration and extraction, the brace-aware rerun examined 1,310
eligible inline bodies at or above the 100-character threshold across eighteen
controllers. A comment/whitespace-normalized, string-literal-preserving scan
reports **zero** repeated runtime implementation groups. Callback wiring is now
bound directly to typed helper templates wherever no champion behavior is
attached.

A second string-literal-agnostic structural pass examined 1,291 bodies after
normalization and reports eleven cross-file groups. They are thin semantic
vocabulary adapters already delegating to shared primitives: Q/W/E/R event
aliases for Aurelion Sol/Aurora/Bel'Veth and Azir/Bard; Ahri/Akshan/Anivia
missile-name predicates; Akali/Akshan champion-buff predicates;
Aurora/Bel'Veth spell-classification predicates; and Bard-shrine/Bel'Veth-coral
object predicates. Their string vocabularies and downstream meaning differ.
Removing the named adapters would make event call sites less auditable without
removing runtime implementation.

Near-match review also covered AA-range checks, resource-package loops,
gapcloser/interrupt adapters, nearest-threat aliases, buff-state queries,
unload callbacks, missile refreshers, target-immunity filters, jungle target
selection, Aphelios five-gun parsing, ammo reconciliation, first-hit R
collision, Sentry ownership, Azir soldier ownership/Q formation/E collision,
and posture labels. Bard's R-stasis and E-portal exit Q share one local
`BuildAnchoredQPlan`, while their event/timing policy remains distinct. The
neutral aliases/adapters were removed. The
remaining structural matches are enum-to-label switches, champion-specific
spell/buff/name predicates, spell-specific damage wrappers, line-collision
unit builders, target-priority coefficients and threat observers whose state
updates differ by kit. Akshan and Aatrox retain local gapcloser handlers
because they attach extra hard-CC/target-direction behavior to the captured
facts; they are not neutral duplicates.
Blitzcrank's target-archetype and mobility registries were specifically
reviewed against existing threat maps: the shared observations stay neutral,
while deciding that a pull delivers a dangerous body onto a carry is unique
to Rocket Grab and was not generalized.

## Guardrail for later champions

Before adding a new helper, prove that its inputs and output mean the same
thing for at least two controllers. Do not parameterize an entire combo merely
to make source text look shorter. Re-run exact and near-match scans after every
new champion and extend this file only when a shared semantic unit is found.
