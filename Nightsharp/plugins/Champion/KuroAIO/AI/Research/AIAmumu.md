# AIAmumu research dossier

Research date: 2026-07-18
Target game revision: League of Legends 26.14 / CommunityDragon 16.14
Controller: `Controllers/AIAmumuController.h`
Profile: `Profiles/AIAmumu.h`
Pure mechanics: `Controllers/AIAmumuGeometry.h`
Shared primitives: `AIGeometry.h`, `AIControllerHelpers.h`

## Source ledger

1. Riot champion page: <https://www.leagueoflegends.com/en-us/champions/amumu/>
   - First-party semantic baseline for Cursed Touch, Bandage Toss, Despair,
     Tantrum and Curse of the Sad Mummy.
   - Public display text is treated as kit intent; exact collision, timing and
     damage stages are pinned from the data records below.
2. Riot patch 25.18: <https://www.leagueoflegends.com/en-ph/news/game-updates/patch-25-18-notes/>
   - Current Q recharge is `16/15/14/13/12` seconds and Q costs 50 mana at
     every rank. Older guides with a rank-scaling cost are rejected.
3. Riot patch 10.4: <https://www.leagueoflegends.com/en-us/news/game-updates/patch-10-4-notes/>
   - Authoritative historical change for the current 1800 Q follow-dash speed
     and R's knockdown behavior against dashes.
4. Pinned CommunityDragon champion record:
   <https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/32.json>
   - SHA-256: `98BEF55966A27719231D791E747F060A5121BD2963AD67F7D13F4BD89390DF69`.
   - Confirms champion id 32, mana resource, public spell identities, ranks,
     cooldowns and client display values for revision 16.14.
5. Pinned CommunityDragon game-bin record:
   <https://raw.communitydragon.org/16.14/game/data/characters/amumu/amumu.bin.json>
   - SHA-256: `5F38A74418CD4A7E4A9C1CECE5CACAAE097DE8083185955279D5059C8C7AC7A5`.
   - Directly inspected spell/buff identities include `BandageToss`,
     `SadMummyBandageToss`, `AuraofDespair`, `Tantrum`,
     `CurseoftheSadMummy`, `AmumuPassiveDebuff` and Amumu W state aliases.
   - Used for cast timing, projectile/follow movement, effect radii, tick
     cadence and runtime names. Generic display range is not substituted for
     W's real 350 effect radius.
6. Meraki Analytics Amumu record:
   <https://cdn.merakianalytics.com/riot/lol/resources/latest/en-US/champions/Amumu.json>
   - Research copy SHA-256:
     `9B74808802E9CBC473C8E6BEDF59592D3EC9730D7508CD2D70C4D7CEF0D15DDA`.
   - Cross-check for detailed passive order, Q collision/follow behavior, W
     ticks, E attack refunds/reduction and R damage/Curse sequencing.
   - Riot patch notes and pinned CommunityDragon values win any conflict.
7. Current detailed mechanics page:
   <https://leagueoflegends.fandom.com/wiki/Amumu/LoL>
   - Cross-checks Q's first-unit collision, forced attack order, extended
     follow behavior and the fact that a spell shield stops Q damage/CC but
     does not stop Amumu's follow movement.
   - Also cross-checks that R deals damage before applying Curse.
8. Current mechanics and combo catalogue:
   <https://lolstats.gg/en/champions/amumu/guide>
   - Revision displayed as current 26.14 during research.
   - Cross-checks charge preservation, `Q -> AA -> E -> W`, immediate `Q -> R`
     and buffering R while the Q follow is in progress.
9. Rank-1 Season 16 Amumu guide/gameplay:
   <https://www.youtube.com/watch?v=mfzDy-8inT4>
   - Current specialist material from `@AngelKnight3M`, published roughly two
     months before this research pass. Used for live engage restraint, second-Q
     preservation, target access and team follow-up decisions.
10. Fz Frost Season 16 deep guide:
    <https://www.youtube.com/watch?v=C9k78umyUzE>
    - Published 2025-08-23; the mechanics/combo chapter at 24:01-29:34 was used
      to organize gank, bridge, fast-R and extended-lockdown families.
11. Current Challenger replay, patch 26.10:
    <https://www.youtube.com/watch?v=tovO4BzeazQ>
    - Cross-checks that strong live play does not spend both Q charges on first
      contact and that R value depends on allied arrival and target priority,
      not only body count.
12. Current Season 16 jungle-clear demonstrations:
    <https://www.youtube.com/watch?v=bRnOgsPyzRA> and
    <https://www.youtube.com/watch?v=56ZFiOv_uZ0>
    - Cross-check W contact toggling, multi-unit E refund tempo, camp target
      priority and why the controller must not invent movement between camps.
13. Current Amumu gameplay/decision reviews:
    <https://www.youtube.com/watch?v=sjUSxVn_e2U> and
    <https://www.youtube.com/watch?v=MhXf8DBpRig>
    - Independent cross-check for conservative gank angles, cursor-aligned
      access, objective mana and the difference between engage and peel R.
14. Specialist forced-AA discussion:
    <https://www.reddit.com/r/amumumains/comments/1gx9p6k/auto_attack_after_q/>
    - Specialist reports identify the practical problem: Q's game-issued AA
      can delay R enough for a Flash-ready victim to escape. The controller
      suppresses that attack only after arming the immediate Q-arrival R branch.
15. Specialist Q/R-Flash interaction discussion:
    <https://www.reddit.com/r/amumumains/comments/pnef0z>
    - Used as a hypothesis for manual R-Flash and Q-buffer timing, then limited
      to coaching/observation because the plugin never owns Flash input.
16. Local champion-plugin audit:
    - KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, ziblldev9898 and the older
      local C# ports were searched for an Amumu champion controller.
    - No local Amumu champion implementation exists. Matches are evade rows,
      target priority, damage reduction, spell data and assets only.
    - `AIAmumu` is additive and is reached only after the ten protected legacy
      KuroAIO routes; it does not shadow or replace an existing champion.
17. Local SDK audit:
    - `SDK/Data/Database.h`, EzEvade, ZDEvade and KuroEvade agree on
      `BandageToss`/`SadMummyBandageToss`, 0.25 cast time and 2000 projectile
      speed. The SDK damage-reduction wrapper already owns Tantrum's defensive
      passive, so the controller does not duplicate game-side mitigation.

Community/video claims are never numerical authority by themselves. They are
encoded only when they agree with pinned data or a reproducible game behavior.

## Current mechanical model

### Cursed Touch is an ordered damage state

- A basic attack applies Curse for three seconds. Magic damage dealt after
  that mark adds 10% of its pre-mitigation amount as true damage.
- W continually refreshes Curse on targets actually touching the aura. The
  controller tracks this contact instead of pretending that enabling W marks
  distant enemies.
- R applies Curse after its own magic-damage event. An uncursed target therefore
  gets no passive true-damage bonus on that same R; a previously cursed target
  does. The pure damage model has an explicit `cursedBeforeUltimate` input to
  prevent this common overestimate.
- Live buff state is preferred. Local AA/R/W event timing is a bounded fallback
  for same-frame or missing buff callbacks.

### Bandage Toss is collision, ammo and forced arrival

- Q owns two charges. Casts are separated by a three-second charge lockout;
  each stored charge recharges in `16/15/14/13/12` seconds and costs 50 mana.
- It has 1100 range, 80 half-width, 0.25 cast time, 2000 missile speed, a
  one-second stun and a 1800-speed follow dash.
- Range and collision are measured against gameplay circles. The solver finds
  the capsule entry distance, not center distance, so a large offset monster
  can correctly intercept before a minion whose center appears closer.
- Every visible enemy champion, lane minion and jungle monster is predicted at
  impact time. Direct and small angular alternatives are evaluated, but a cast
  is legal only when the intended unit is the real first collision.
- Impact and arrival use separate clocks. Q's stun starts at missile impact;
  W/R follow-up is timed around the later Amumu arrival and upgraded by an
  observed live dash whenever the SDK exposes it.
- Q follows a shielded target even though damage and CC are blocked. That
  mobility is disabled by default and allowed only with an enabled R, allied
  follow-up and a safe arrival.
- Ordinary naked Q requires very high hitchance and a configurable maximum
  range. Dash, channel and peel branches lower the prediction requirement only
  because their endpoint/window is constrained.
- The second charge is a tactical reserve for Flash, dash, interrupt and peel.
  It is spent early only for lethal damage, a new dash or a narrow CC-layering
  window.
- A minion/monster bridge requires both charges, meaningful reach gain, cursor
  consent and a safe first arrival. The champion remains the sequence target;
  blockers are recomputed after landing before Q2.
- Q's forced automatic attack is normally preserved to apply Curse. It is
  suppressed only inside the already-validated immediate Q-to-R state, where
  allowing the order would create a Flash escape window.

### Despair is a contact-and-reserve toggle

- W's real effect radius is 350, it drains eight mana per second and ticks every
  0.5 seconds.
- Current damage is ten flat per second plus `1/1.25/1.5/1.75/2%` maximum
  health per second, with another 0.5% maximum health per 100 AP per second.
  The pure model halves the per-second package for each tick.
- W can be pre-toggled only after a confirmed Q collision/arrival plan and
  inside a short lead window. It stays on for champion contact or an enabled
  epic objective, then turns off after a no-contact grace period.
- Before each toggle the controller reserves ready Q/E/R costs plus a flat
  safety reserve. It never keeps W on when the next drain would consume the
  planned engage/peel sequence.
- Cast events and `AuraofDespair` buff events jointly own toggle state, with a
  synchronous-event guard so one controller cast cannot invert the state twice.

### Tantrum is a Curse weave and an incoming-attack clock

- E has 350 radius, 0.25 cast time, costs 35 mana and deals
  `65/95/125/155/185 + 50% AP` magic damage.
- Each basic attack that hits Amumu reduces E's current cooldown by exactly
  0.75 seconds. The controller records targeted autos, estimates ranged impact
  from cast delay/travel speed and deduplicates ProcessSpell/DoCast reports.
- It waits only when the observed incoming attacks can refresh E inside a
  configured tactical horizon. It never treats expected refunds as a castable
  spell until the runtime spell actually reports ready.
- On an ordinary champion contact, an available in-range AA is allowed to apply
  Curse before E. Lethal, peel, flee and time-critical branches skip that wait.
- Jungle E values multi-unit attack refunds; lane E has separate mana, minimum
  hit and last-hit gates. Tantrum's flat/bonus-resistance basic-attack reduction
  remains game-owned and is not reimplemented as fake controller state.

### Curse of the Sad Mummy is value-weighted lockdown

- R resolves after a 0.25 cast, uses a 550 radius, deals
  `200/300/400 + 80% AP`, stuns/knocks down for 1.5 seconds and then applies
  Curse.
- Every target is predicted at resolution and checked with gameplay radius.
  The plan records raw hits, effective non-shielded hits, target value, selected
  target inclusion and the protected ally's current diver.
- R discounts spell shields, ready QSS-family items, Olaf/Gangplank/Alistar
  cleanses and long existing hard CC. A dash whose endpoint enters the radius
  receives extra value because R knocks it down.
- Ordinary teamfight R requires both a minimum effective count and a quality
  score. Single-target R is reserved for unique lethal damage, a high-value
  followed pick, critical peel, interrupt or observed survival pressure.
- Q-arrival R is recomputed around the future arrival center. If the cluster
  leaves that radius, the stale plan is rejected instead of casting at the old
  target position.
- A player-owned Flash causes a fresh R evaluation. Manual `R -> Flash` is
  coached and observed through the final-position resolution window; the
  controller never casts Flash.

## Posture and decision state machine

| Posture | Primary obligation | Key veto |
|---|---|---|
| Gank | Find a high-confidence Q access/bridge and retain Q2 | Reject blockers, weak cursor agreement or an unsafe arrival |
| Follow-up | Layer Q/R after allied control and weave Curse | Do not overlap substantial remaining CC |
| Teamfight engage | Convert Q arrival into a scored R cluster | Do not ult on raw body count without effective value |
| Peel | Protect the highest-value pressured ally | Handle the diver before starting a new engage |
| Disengage | E/R the pursuer, then Q a safe escape anchor | The endpoint must increase separation and avoid denial |
| Jungle | Maintain W contact and exploit E refunds | Preserve combat Q/mana near enemy champions |
| Support lane | Trade only with mana and allied follow-up | Do not spend both charges on speculative harass |
| Neutral | Observe events, W state and reactive threats | Do not invent a generic Q-W-E-R loop |

The same spell has posture-specific meaning. Q can be engage access, a
minion bridge, CC layering, peel, interrupt or escape; those branches remain
inside Amumu instead of being flattened into a shared `CastQ` helper.

## Combo-family translation

1. Safe Curse trade: `Q -> forced AA -> E -> contact W`, retaining Q2.
   - Used when the AA will land safely and no urgent R/Flash window exists.
2. Fast carry lock: `Q -> buffered R -> E`, then layer Q2 near R expiry.
   - The forced Q AA is suppressed only after future R coverage and urgency
     have already been validated.
3. Multi-target engage: `Q collision/arrival simulation -> future-center R ->
   W contact -> E -> Q2 layering`.
   - R is canceled if the predicted cluster leaves the arrival center.
4. Extended lockdown: `Q1 -> AA/E -> R -> Q2 near remaining-CC boundary`.
   - Q2 is held while the target still has meaningful stun duration.
5. Bridge access: `Q minion/monster -> land -> recompute -> Q champion`.
   - Requires two charges and does not manufacture movement between casts.
6. Peel chain: `R` for an immediate critical radius threat, otherwise
   `Q diver -> E contact -> layer Q2`.
   - The protected carry and current diver are independently selected.
7. Interrupt: choose Q only when its missile impact beats the channel expiry;
   otherwise use in-range R when enabled.
8. Disengage: `E/R pursuer -> Q cursor-aligned escape unit`.
   - The anchor must improve separation and pass terrain/turret/hazard checks.
9. Jungle cycle: contact W, repeated E through real incoming-auto refunds and
   Q only for a distant camp unit when the last combat charge need not be kept.

## Shared helper boundary and duplicate audit

- Reused champion-neutral helpers include local-player identity, spell/menu
  state, current resource, runtime spell readiness/cast throttle, AA range and
  attack capture, prediction, target lookup, case-insensitive names, spell
  shields, enemy spell readiness, terrain/dash hazards, interrupt/gapcloser
  capture and generic enemy-cast analysis.
- The Amumu pass exposed two more genuine cross-champion units: allied follow-up
  counting and protected-ally selection. They now live in
  `AIControllerHelpers.h`; Alistar and Amumu both call them.
- The audit also removed identical Ambessa/Amumu runtime-readiness and responsive
  cast-throttle wrappers. Readiness and the shared 38 ms/zero-delay policy now
  have one implementation in `AIControllerHelpers.h`.
- Kept Amumu-local because semantics are kit-specific: Q ammo reserve, capsule
  entry ordering, follow-arrival clock, spell-shield mobility, bridge/escape
  anchors, forced-AA suppression, Curse damage order, W reserve/contact, E
  refund forecasting and R reason/value policy.
- A post-change exact body scan covered 516 controller functions. Only the
  documented one-line callback adapters remain exact across files. A structural
  scan found only enum label renderers and champion-specific spell/buff name
  predicates; extracting those would erase kit meaning without sharing logic.

## Player-cooperation contract

- The player's selected target is preferred and proactive access requires
  cursor agreement.
- The controller casts Amumu's Q/W/E/R only. Movement, attack-move, Hold, Stop,
  Flash, Smite and all other summoner inputs remain player-owned.
- Q's game-issued ordinary AA is preserved. Suppression is tightly scoped to a
  validated buffered-R branch and ends when that arrival window expires.
- Manual Q reconstructs the actual first collision and continues its landing
  state. Manual W/E/R update their respective state machines instead of being
  overwritten by a generic combo.
- The engine's manual-input arbitration window and ordinary attack windups are
  respected unless an enabled emergency interrupt/peel branch owns the moment.
- Drawings expose first Q collision/arrival safety, R hit set/value, protected
  ally/diver, ammo reserve, W state, E refund forecast and the manual R-Flash
  coaching window.

## Acceptance scenarios

`AIAmumuController.h` publishes 143 auditable scenarios. They cover:

- seven active combat/role postures plus neutral observation;
- live two-charge ammo, reserve and three-second recast locking;
- radius-aware Q collision entry, blockers, prediction and angular alternatives;
- impact versus follow-arrival timing and live dash confirmation;
- Q spell-shield mobility, turret/density/terrain/anti-dash safety;
- forced-AA Curse weave versus immediate buffered R;
- minion/monster bridges, escape anchors and second-Q CC layering;
- contact/mana-aware W state and exact half-second tick math;
- E prediction, Curse wait, incoming-auto dedup/refund forecasting;
- jungle, lane-clear and last-hit resource gates;
- R target value, cleanse/shield/CC overlap, dash knockdown, peel, interrupt,
  lethal, survival and manual Flash branches;
- manual cast continuation, player input ownership and visual coaching.

The scenario array is runtime controller metadata, not a documentation-only
checklist; its exact count is published through the seven-entry catalog.

## Verification completed

- `tests/amumu_geometry_test.cpp` compiles independently under C++17 and passes.
- Regression assertions cover Q collision boundaries/order/range, impact and
  arrival clocks, W/E/R radii, R value, W tick/damage math, E cooldown refunds,
  E/R damage, Curse-before-damage order, CC layering, arrival safety and bridge
  reach.
- All seven AI champion geometry tests plus the shared windwall geometry
  regression pass in the same Visual Studio developer environment.
- Full `Release|x64` MSBuild succeeds and emits `bin/Release/NightSharp.dll`
  with Amumu in the seven-entry AI catalog.
- The full build also exposed a pre-existing offset-update regression: current
  contiguous `All::Position` is now read as `Vec3`, and the accidentally removed
  shared renderer layout was restored for its existing consumers.
- Legacy KuroAIO dispatch remains unchanged. Amumu has no protected legacy
  implementation and is loaded only through the additive AI catalog.

## Known limits requiring live/replay telemetry

- Q's exact follow endpoint can move with a target for a large distance. The
  controller predicts the followed unit and confirms the live dash when exposed,
  but server correction around extreme displacement still needs replay telemetry.
- Buff callbacks can vary by runtime alias. The controller recognizes pinned
  names and has bounded event fallbacks; live logs should be used before adding
  any new alias.
- E refund prediction observes incoming basic-attack casts, not a dedicated
  post-hit event. It therefore waits conservatively and trusts `IsReady()` before
  casting, but projectile cancellation/untargetability can reduce forecast hits.
- R cleanse scoring knows current item and champion self-cleanse states, not
  every allied cleanse or future spell cast. It is a value discount, never a
  claim that the stun is guaranteed.
- Fog-of-war blockers and enemies cannot be enumerated. Q/R plans through
  unexplored space remain inherently less certain and should stay conservative.
- Patch changes after 26.14 require repinning CommunityDragon and reviewing
  Riot notes before changing formulas or loosening any one-trick gate.
