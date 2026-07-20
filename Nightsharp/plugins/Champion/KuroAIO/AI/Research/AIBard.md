# AIBard one-trick research dossier

Baseline date: 2026-07-18  
Game baseline: Riot 26.14 / CommunityDragon PC 16.14  
Controller: `AI/Controllers/AIBardController.h`  
Pure mechanics: `AI/Controllers/AIBardGeometry.h`  
Regression: `tests/bard_geometry_test.cpp`

## Existing-support audit

The KuroAIO dispatch, its ten preserved champion implementations, every local
champion plugin tree, project entries and the pre-AI source ports were searched
for Bard before registration. Bard appeared only in evade, damage, buff and
priority data; no KuroAIO Bard route, champion module, profile or controller
existed. `AIBard` therefore fills an unsupported route and does not replace,
shadow or take priority over an existing KuroAIO implementation.

## Pinned machine-readable data

- CommunityDragon champion JSON:
  `https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/432.json`
  - SHA-256:
    `4a3b53c32db2f74336e963b92bb381c2564b04fb19f974cc959641fa5e394bed`
  - Downloaded size: 36,734 bytes.
- CommunityDragon spell bin:
  `https://raw.communitydragon.org/16.14/game/data/characters/bard/bard.bin.json`
  - SHA-256:
    `bcc9b43bec99229f90ba52c0acbf525258680278787a675470fee1fa05cbaa9d`
  - Downloaded size: 78,809 bytes.
- Riot patch 26.13:
  `https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-13-notes/`
  is authoritative for the current passive nerf.
- Riot patch 25.21:
  `https://www.leagueoflegends.com/en-us/news/game-updates/patch-25-21-notes/`
  is authoritative for the current W AP and movement-speed scaling.

The controller declares 26.14 because that is the live release baseline. The
last relevant Bard balance change was 26.13: meep damage changed from 35 plus
10 per five chimes to 30 plus 6 per five chimes; its 40% AP ratio remained.

## Current mechanics reconciliation

### Traveler's Call

- Meep raw magic damage is `30 + 6 * floor(chimes / 5) + 0.40 AP`.
- Base meep recharge is eight seconds. It becomes seven at 20 chimes, six at
  40, five at 55 and four at 70.
- Maximum meeps become 2/3/4/5/6/7/8/9 at
  10/30/50/65/80/90/95/100 chimes.
- The slow begins at 25% at five chimes and becomes 35/45/55/65/75% at
  25/45/60/75/85 chimes.
- Splash begins at 15 chimes and expands at 35.
- A chime restores 12% maximum mana. Its initial out-of-combat movement bonus
  is 24%; each additional stack adds 14%, with a 150% cap, ten-stack cap and
  20-second duration.
- Chime experience is 20 and gains one per game minute after minute five.
- Chimes are map-tempo opportunities, not a command to leave the carry. The
  controller scores detour length, expiry, carry safety and objective urgency,
  then draws only a suggestion. It never moves Bard.

### Cosmic Binding

- Gameplay first-target range is 850, continuation is 300, half-width is 60,
  cast time is 0.25 seconds and missile speed is 1500.
- The bin exposes a 950 missile-resource range. KuroEvade and current gameplay
  documentation expose the effective first-target range as 850. The geometry
  therefore casts and validates the first body at 850 while documenting 950 as
  a resource value, rather than granting 100 fake gameplay units.
- Current damage is 80/120/160/200/240 plus 80% AP. The slow is 60%; slow and
  stun duration are 1/1.2/1.4/1.6/1.8 seconds.
- Q must hit a hostile unit before terrain can shackle it. After first contact,
  the next unit or terrain collision within 300 units determines the stun.
- A spell shield on the first target blocks its initial damage, but Q continues.
  A second unit then receives its own damage/stun while the shielded first body
  is not stunned. Terrain after the shielded first body still stuns that first
  body. A shielded second body blocks only its own damage/stun.
- Q can bind against player-created terrain. Runtime NavMesh sampling is used
  for terrain; live projectile-wall checks remain separate.
- Side-hit Q is real Bard geometry: contact on a hitbox edge changes the first
  entry point and can place the final continuation edge on a unit or wall. The
  controller tests tangent offsets and ordered ray-circle contacts instead of
  using “target near wall” as a substitute.

### Caretaker's Shrine

- Range is 800, trigger radius 100, mana cost 70, maximum ammo two, recharge
  18 seconds and charge time five seconds. At most three owned ground shrines
  may coexist.
- Current minimum heal is 25/50/75/100/125 plus 40% AP. Fully charged heal is
  50/87.5/125/162.5/200 plus 70% AP.
- Movement speed is 20/22.5/25/27.5/30% plus 6% per 100 AP for 1.5 seconds.
- Direct ally cast heals immediately and does not create a ground shrine, so it
  does not destroy the oldest ground shrine. The controller models direct and
  ground casts as different plans.
- Ground shrines are spaced, kept out of terrain and prepared only when Bard is
  not under immediate pressure. One charge is normally reserved; targeted or
  lethal incoming pressure may override that reserve for a direct ally heal.

### Magical Journey

- Cast range is 900, portal lifetime ten seconds and supported tunnel length is
  capped at 2600. Enemy traversal speed is 900; Bard/allied speed is 1197.
- A cast needs real contiguous terrain. The controller scans from Bard through
  the requested wall, identifies entrance/exit, rejects an exit still in wall,
  then scores enemy count, allied follow-up, cursor direction, turret state,
  point-click lockdown and anti-dash hazards.
- Portal traversal is one-way and unlimited. The controller creates a portal
  only for a defensive emergency/flee or a player-held key; it never walks,
  clicks or commands an ally through it.
- An enemy traversing a confirmed owned portal creates an exact exit-Q window.
  Q flight time is aligned to the tracked portal movement buff and stored exit.

### Tempered Fate

- Range is 3400, radius 350, cast time 0.5 seconds and stasis lasts 2.5 seconds.
  Travel interpolates from roughly 0.65 seconds point-blank to 1.8 seconds at
  maximum range, for a total impact delay of roughly 1.15 to 2.3 seconds.
- R affects targetable champions, minions, monsters, turrets, wards and plants;
  it can stasis turrets and epic monsters despite their ordinary CC immunity.
- Every candidate is evaluated as a mixed-team set at predicted impact time.
  Allied channels, healthy allied champions, enemies already under allied
  focus, secured objectives and focused low-health enemies count as grief.
- Automatic use is limited to a short-range clean catch, an unfocused backline
  isolation, a tracked lethal ally save, an interrupt after Q cannot arrive,
  or multi-pursuer disengage. Turret dives and objective denial require explicit
  player keys.
- R-exit Q begins only after the actual `BardRStasis` buff is observed. The
  controller searches a real second body or wall and aligns Q arrival with the
  buff end. A manual R can opt into this timing assistance after the configured
  ownership window; its cast center remains completely player-owned.

## One-trick and pro study

### Lathyrus

- `The Ultimate Bard Bible`:
  `https://www.youtube.com/watch?v=qqEv6OnYsbk`
- Current 26.14 build update, published 2026-07-16:
  `https://www.youtube.com/watch?v=csrBgDbcLr0`

The transcript was inspected rather than using the title as evidence. The
durable controller rules extracted were:

1. Bard is the conductor/catcher, not a chime collector who abandons wave
   responsibility. Levels two through five are fragile; mid/late catch power
   is the real payoff.
2. Use meep auto before ordinary Q so the slow makes the binding reliable.
3. After level six, punish an overextended target with R and time Q at stasis
   exit only when a real wall/second unit exists.
4. For a turret dive, crash first, get behind, absorb the planned early shots
   and stasis the turret to reset it. This strategic sequence cannot be inferred
   safely from spell readiness, so the implementation requires the dive key.
5. R may engage, isolate a backline, deny an enemy jungler/objective, stasis a
   turret/plant or save an ally, but must not freeze the enemy the team is
   already killing.
6. W becomes as important for speed and temporary information as for raw heal.
7. E should be treated like a high-value dash, not spent for lazy travel.
8. Common errors are Q before meep auto, Q spam without a stun, lazy portals,
   maximum-range R and ulting a currently focused target.

### Polypuff

- `36 Bard Tricks`:
  `https://www.youtube.com/watch?v=t8Cn-d0_ejM`

The useful mechanical findings were side-contact Q extension, Q buffering into
dash/CC, projectile arrival over stasis, W as a temporary entrance/vision tool,
portal vision/bait/kidnap/fake patterns, casting R during a portal, and R on a
turret, Herald, plant, objective or enemy jungler. Flash and movement-dependent
tricks are documented but never automated. Portal bait/kidnap requires opponent
intent that the current SDK cannot validate, so it remains player-led rather
than becoming a speculative automatic branch.

### Pro and current community cross-checks

- Keria 2026 Bard pro view: `https://www.youtube.com/watch?v=nQWIRUh5-kk`
- Keria voice-communication highlights:
  `https://www.youtube.com/watch?v=y_KLmI5ssJg`
- Current combo catalog: `https://mobalytics.gg/lol/champions/bard/combos`
- Current build context: `https://op.gg/lol/champions/bard/build/support`
- Current BardMains starting guidance:
  `https://www.reddit.com/r/bardmains/comments/1uis44l/new_bard_main/`
- Current Bard guide discussion:
  `https://www.reddit.com/r/bardmains/comments/1tdr1wz/bard_guide/`
- Mechanics cross-check: `https://leagueoflegends.fandom.com/wiki/Bard/LoL`

The combo catalog confirms R-E-Q pursuit, R-Q-AA, Q-Flash-AA, AA-Q-AA and
E-Q-AA. The controller implements only the spell-state/geometry portions:
AA-Q-AA is coordinated with Orbwalker callbacks; R-Q uses the actual stasis
buff; E-Q needs an observed enemy traveller. Q-Flash and portal entry are
explicitly player-owned.

## Runtime policy and player cooperation

Priority is reactive safety first, then exact continuation, kill secure and
mode behavior:

1. Exact R/portal exit Q already in flight planning.
2. Emergency direct W on a threatened ally.
3. Q interrupt, anti-gapcloser or peel.
4. R lethal save or channel interruption only when Q cannot do it.
5. Safe defensive E if the player is fleeing or pressure is critical.
6. Clean R catch/isolation, then meep-AA to Q weaving.
7. Spaced setup shrines only outside combat.

The player retains movement, attacks, Orbwalker target choice, cursor intent,
portal entry, chime collection, Flash, Smite and all strategic dive/objective
commitments. Any player-cast Q/W/E/R starts a configurable ownership window.
Manual Q/W/E are never auto-completed. Manual R exit assistance is optional and
waits for the real stasis event.

## Deterministic validation

`bard_geometry_test.cpp` covers live passive values and breakpoints, chime
route scoring, ordered Q collisions, early terrain rejection, side extension,
spell-shield asymmetry, shrine spacing/charge/ammo, emergency heal scoring,
portal trace/safety/travel times, distance-scaled R impact, stasis-exit Q timing,
catch, no-grief rejection, ally save, turret dive and objective denial.

After catalog integration:

- `bard_geometry_test.cpp`: pass.
- `aurora_geometry_test.cpp`: pass after shared `RankValue` extraction.
- `azir_geometry_test.cpp`: pass after shared `RankValue` extraction.
- `Release|x64` full solution build: pass.

## Live telemetry still required

- Confirm every client-runtime alias for chime, meep ammo, shrine, portal
  movement and R stasis across skin/localization payloads. The controller keeps
  multiple aliases and safe fallbacks, but replay telemetry is the authority.
- Validate W ammo reporting when the spellbook exposes transient recharge state.
- Measure portal movement buff end-time accuracy at very short and maximum
  terrain thickness.
- Validate whether a local lifecycle event always exposes shrine/portal source
  network id; recent cast-position ownership is the conservative fallback.
- Measure Q tangent candidate performance against live moving hitboxes and
  latency. The pure geometry is deterministic; runtime prediction is not.
- Validate live target sets for every ward/plant subtype before expanding
  automatic plant denial. Current plant/objective/turret R remains keyed or
  conservative.

