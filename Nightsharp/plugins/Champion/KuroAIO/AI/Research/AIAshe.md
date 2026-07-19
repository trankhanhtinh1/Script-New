# AIAshe research and implementation dossier

Research date: 2026-07-18  
Target baseline: League of Legends 26.14 / CommunityDragon 16.14  
Controller: `AI/Controllers/AIAsheController.h`  
Pure mechanics: `AI/Controllers/AIAsheGeometry.h`  
Published scenarios: **141**

## Completion claim

This is not a generic marksman profile which casts W at a prediction point and
R at low health. `AIAsheController.h` owns Ashe's complete decision loop and
implements four independent systems:

- a Focus observer and real after-attack `AA -> Q -> AA` reset policy;
- a rank-dependent seven-through-eleven-ray Volley solver in which every ray
  has its own first hostile blocker;
- a two-charge information planner for Hawkshot which tracks the enemy Smite
  holder, scores path and destination vision, covers multiple camps and
  suppresses repeated routes; and
- an accelerating global Crystal Arrow trajectory solver which stops on the
  first enemy champion, derives distance stun, evaluates the real explosion
  set and requires a purpose-specific hit/follow-up contract.

The controller publishes 141 named scenarios, owns the loop with
`OwnsDecisionLoop=true`, has a standalone geometry/state test and is added only
after confirming that KuroAIO has no protected Ashe implementation. Player
movement and attacks remain player-owned because spacing, stutter-stepping and
which slowed target to kite are Ashe's highest-frequency skill expression.

## Source authority

### Primary live data

1. [Riot patch 26.14](https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-14-notes/)
   is the release baseline dated 2026-07-14. It contains no Ashe balance
   override, so the latest Ashe-specific changes below remain current.
2. [Riot patch 26.10](https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-10-notes/)
   changed Q total attack damage from 110/117.5/125/132.5/140% to the current
   110/115/120/125/130%.
3. [Riot patch 26.1](https://www.leagueoflegends.com/en-us/news/game-updates/patch-26-1-notes/)
   moved Frost Shot to the current crit system, reduced attack-speed and AD
   growth, and set Volley to 60/95/130/165/200 + 100% bonus AD.
4. [Riot's Ashe page](https://www.leagueoflegends.com/en-us/champions/ashe/)
   is the official identity check for Frost Shot, Ranger's Focus, Volley,
   Hawkshot and Enchanted Crystal Arrow.
5. [CommunityDragon champion JSON 16.14](https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/22.json)
   - SHA-256:
     `9932A2DBF1886C1A551202F9F4C008444DB61D7005043E755428A50762298361`
   - 66,042 UTF-8 bytes at audit time.
6. [CommunityDragon Ashe bin 16.14](https://raw.communitydragon.org/16.14/game/data/characters/ashe/ashe.bin.json)
   - SHA-256:
     `622AC79ED02676E59FCD0A1A3C0C70D08FB3F3634048DC27EC6A8912BE57EF15`
   - 96,196 UTF-8 bytes at audit time.

Riot patch notes override old guide numbers. CommunityDragon supplies the
runtime spell names, tags, ray count, missile geometry, charge data and
accelerating-missile payload which Riot's short public descriptions omit.

### Current pro, coaching and one-trick packet

- [Advanced Ashe Guide by a pro player, published 2026-04-15](https://ruclips.net/video/iHzA54f1Nyg/advance-ashe-guide-by-a-pro-player.html)
  was reviewed for max-range lane interaction, attack/Q timing, kiting cadence
  and when utility takes priority over raw uptime.
- A [current April 2026 AsheMains coaching discussion](https://www.reddit.com/r/AsheMains/comments/1spmknr/need_tips_as_a_new_ashe_player/)
  points to xFSN Saber, Neon, Daption and Bizyze review material and provides a
  useful high-level consensus: `AA-Q-AA` is the essential mechanic; W or an
  auto should slow before a short/medium R; a long R needs clear path intent;
  and a pick needs teammates who can collapse.
- A [May 2026 lane-behavior discussion](https://www.reddit.com/r/AsheMains/comments/1tdvvoo/what_is_default_ashe_lane_behaviour/)
  independently emphasizes 600-range spacing, passive chase/disengage, Q reset,
  W poke and a Hawkshot line which can reveal all six enemy camps.
- A [May 2026 Ashe OTP discussion](https://www.reddit.com/r/AsheMains/comments/1t4quqj/hi_im_interested_in_ashe/)
  describes Hawkshot's three practical jobs: early jungle tracking, scouting
  before a facecheck and objective information later in the game.
- [Mobalytics' current Season 26 combo catalog](https://mobalytics.gg/lol/champions/ashe/combos)
  cross-checks `AA-Q`, `R-AA-W-...-Q`, `AA-W-AA` and the full reset/burst
  family. Flash variants were deliberately not automated.
- [Current OP.GG Ashe ADC data](https://op.gg/lol/champions/ashe/build/adc) and
  [current pro activity](https://www.probuild.gg/champions/pros/Ashe) were used
  as current-role/skill-order sanity checks, not as spell-mechanics authority.

Guide material is translated into observable gates rather than copied as
unconditional combo strings. For example, “W before R” becomes Frost
confirmation, target path reliability and allied follow-up; it does not become
an always-on W-R macro.

## Complete local implementation audit

The audit searched KuroAIO, 7UPAIO, OneKeyToWin, SharpShooterAIO,
ziblldev9898, EzEvade, SDK damage/orbwalker data and all C# source ports.
KuroAIO has no Ashe champion file, so adding `AIAshe` does not shadow or
replace a protected route.

The local Ashe-specific implementations were:

1. `plugins/Champion/SharpShooterAIO/Ashe.h` (526 lines);
2. `plugins/Champion/OneKeyToWin/Champions/Ashe.h` (295 lines);
3. `plugins/Champion/OneKeyToWin/OKTW_CSharp/Champions/Ashe.cs` (349 lines);
4. `plugins/EzEvade/EzEvade_CSharp/SpecialSpells/Ashe.cs` (45 lines, Volley
   fan evasion only); and
5. SDK orbwalker/damage/database entries for Q reset, Frost Shot, W and R.

The older champion controllers provided useful SDK vocabulary but were not a
quality baseline. They generally model W as one ordinary line, omit meaningful
Hawkshot intelligence, use obsolete 2500-unit E/R assumptions or cast Q/R from
simple readiness/health conditions. The new controller keeps the SDK's native
Q-reset/passive support and replaces those tactical assumptions.

## Live kit packet and implementation consequence

### Frost Shot and spacing

- Ashe has 600 base attack range.
- Ordinary attacks Frost for two seconds and deal amplified damage to already
  Frosted targets.
- The ordinary slow scales from 20% toward 30% by level; the empowered crit
  slow scales from 40% toward 60%.
- Crit changes slow/control and Frost damage rather than creating a normal
  marksman crit burst pattern.

Implementation consequence:

- target-scoped Frost records reconcile confirmed buff events with conservative
  two-second auto/W predictions;
- Frost changes expected Q follow-up attacks and R reliability;
- chase/kite posture is exposed, but the controller never moves or issues an
  attack; and
- the selected target remains the player's front-to-back intent.

### Ranger's Focus

- Four Focus stacks enable Q.
- Q costs 30 mana, is tagged as an attack reset and lasts four seconds.
- It grants 20/30/40/50/60% attack speed.
- Each empowered attack is a five-part flurry whose current total ratio is
  110/115/120/125/130% AD.

Implementation consequence:

- `asheqcastready`/`AsheQCastReady` is authoritative ready state;
- `AsheQBuff` is authoritative active state;
- local attack and buff-count events bridge telemetry gaps;
- only the after-attack callback can activate Q;
- a champion requires at least two expected follow-up attacks unless lethal;
- ordinary camp and wave branches require three and four remaining attacks;
- structure/objective branches require a real player attack first; and
- short flurries are held into Amumu/Fizz and active Leona per-hit flat damage
  reduction unless the flurry is lethal.

The last rule matters because Q splits one attack into several physical damage
instances. A simplistic “more Q damage is always better” policy can lose value
against flat per-instance reduction.

### Volley

- Range 1200, cast time 0.25 seconds, missile speed 1500 and per-arrow radius
  20.
- W fires 7/8/9/10/11 arrows by rank at five-degree spacing.
- Every arrow stops on its first hostile unit.
- Several arrows can geometrically meet the same large unit, but that unit is
  damaged only once.
- Current damage is 60/95/130/165/200 + 100% bonus AD and applies Frost.

Implementation consequence:

- every hostile champion, lane minion and monster is predicted separately;
- every ray receives a separate closest-blocker calculation;
- candidate center headings align every live ray with every predicted unit;
- a side ray may thread around a center-blocking minion;
- unique hits, champion hits, killability and primary-ray identity are scored;
- projectile walls reject the actual selected ray rather than only the fan's
  center line; and
- farm thresholds count unique first hits rather than polygon overlap.

### Hawkshot

- Global cast range is 25,000 and flight speed is 1400.
- It has two charges, rank-dependent 90/80/70/60/50-second recharge, no mana
  cost, path vision and a much larger destination reveal.
- Live data gives 325 path-vision and 1000 destination-vision radii.

Implementation consequence:

- `Ammo()/MaxAmmo()` is accepted only when it has a one/two-charge signature;
- either summoner slot is searched for Smite to identify the enemy jungler;
- visible enemy position/path history persists after vision is lost;
- Summoner's Rift camps, entrances, river points and objectives form an
  auditable landmark table;
- candidate lines are scored for covered/priority/objective landmarks;
- recently revealed landmarks receive a repeat penalty;
- automatic use preserves one charge and stops during nearby combat; and
- a manual cursor key can spend the reserve for a facecheck or immediate team
  call on every map.

### Enchanted Crystal Arrow

- Global range 25,000 and cast time 0.25 seconds.
- Radius 130; the missile accelerates from 1500 to 2100 at 200 units/s².
- It stops on the first enemy champion, not on minions.
- The first champion is stunned from 1.0 to 3.5 seconds, reaching the cap at
  2800 travel distance.
- The impact affects enemies in a 400 radius.
- Current damage is 200/400/600 + 120% AP.

Implementation consequence:

- travel time solves the acceleration phase and capped-speed phase;
- prediction is iterated against that calculated travel time;
- every visible enemy champion becomes a collision capsule;
- the nearest capsule along the line owns the impact and explosion center;
- a desired target behind another champion is rejected outside a deliberate
  teamfight plan;
- stationary, controlled, dashing, Frosted and path-parallel targets receive
  different reliability treatment;
- projectile walls and spell shields are checked before commitment;
- interrupt R must land before the channel end;
- pick R requires distance stun and allied collapse;
- teamfight R requires real explosion count;
- self/ally peel has a separate point-blank contract; and
- automatic cross-map pick and R execute are disabled by default.

## Combo families encoded as state transitions

1. Player W/auto poke -> player auto -> Q reset -> continued player kiting.
2. Side-ray W through a wave -> max-range auto -> Q only if the trade remains
   extended.
3. W Frost -> R when the target commits to a path or leaves local attack range
   and an ally can follow.
4. R catch -> W during/after stun -> player AA -> Q reset -> player AA.
5. W peel -> continue player movement; R only if the diver remains critical.
6. Point-blank R peel -> W/attacks only after the player retains safe spacing.
7. Hawkshot multi-camp line -> update jungler uncertainty -> preserve the
   second charge for objective or no-facecheck information.

These are state transitions, not forced button orders. W/R branches depend on
ray/collision geometry, target path, distance stun, spell shields, projectile
walls, nearby enemies and teammate follow-up.

## Player cooperation contract

The controller may:

- activate Q in the narrow after-attack reset window;
- cast W on a verified ray plan;
- cast E on a scored information route or the player's manual cursor route;
- cast R on a verified first-champion trajectory;
- continue around observed manual Q/W/E/R casts; and
- draw Focus, Volley rays, scout routes, Arrow impact and peel state.

The controller never:

- moves Ashe or chooses a kite direction;
- issues an ordinary attack or attack-move;
- presses Flash, Heal, Barrier, Cleanse or another summoner spell;
- forces the player into range after a long Arrow;
- assumes the player will facecheck after a failed scout; or
- spends R on a wave or jungle camp.

Mobalytics documents useful W-Flash/R-Flash variants. They remain intentionally
player-owned because automating Flash would violate both safety and the
requested cooperation model.

## Runtime telemetry gaps

1. Q stack-manager count and ready/active aliases need live traces across base
   and modern skins. Unknown state is exposed and handled conservatively.
2. E `Ammo()/MaxAmmo()` is build-dependent. If unavailable, readiness supports
   only a conservative one-charge fallback; it never invents two charges.
3. The SDK has no universal fog-of-war point query or ward coverage query.
   Automatic E uses map landmarks and last-seen evidence, while immediate
   facecheck judgment stays on the manual cursor key.
4. Camp coordinates are explicit Summoner's Rift heuristics. Terrain/map
   updates require a landmark audit even when spell geometry is unchanged.
5. The enemy spellbook may not expose Smite before that hero's replicated data
   arrives. Jungler identity is upgraded when observed and never guessed from
   role stereotypes.
6. Predicted Frost after W is lower confidence than the real target buff. A
   disappeared/blocked projectile must be confirmed in live replay traces.
7. The flat-reduction Q hold has explicit current champion cases. A future
   item or champion that adds physical flat per-hit reduction requires an
   audited shared damage-instance registry.
8. Missile create/delete events expose R lifecycle but not guaranteed
   continuous position. The decision remains based on cast-time geometry and
   the overlay labels the plan rather than claiming live homing telemetry.

These are explicit validation boundaries, not claims of perfect hidden-state
knowledge.

## Verification

- `tests/ashe_geometry_test.cpp`: **pass**.
- Pure coverage includes rank-dependent W fan geometry, first blockers,
  side-ray threading, unique damage, Q ratios/reset policy, ordinary-camp and
  wave holds, Frost scaling, accelerating R travel/stun, first-champion/AoE
  collision and Hawkshot path/destination coverage.
- `Release|x64` full `NightSharp.sln` build: **pass**.
- Output: `bin/Release/NightSharp.dll`.
- Published controller scenarios: **141**.
- Catalog declared entries / actual entries: **11 / 11**.
- Live roster / protected legacy / completed AI / remaining queue:
  **173 / 10 / 11 / 152**.

