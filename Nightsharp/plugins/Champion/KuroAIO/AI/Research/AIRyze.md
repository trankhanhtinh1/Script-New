# AIRyze research and implementation dossier

Baseline: League of Legends / CommunityDragon PC **16.14**  
Controller: `AI/Controllers/AIRyzeController.h`  
Pure policy/mechanics: `AI/Controllers/AIRyzeGeometry.h`  
Regression: `tests/ryze_geometry_test.cpp` (**158 scenarios**)  
Published runtime scenarios: **205**

## Coverage decision

KuroAIO's dispatcher, legacy champion directory and AI catalog were checked
before implementation. KuroAIO had no Ryze route, profile or controller. The
other local plugin trees were also searched: SharpShooterAIO, OneKeyToWin and
ziblldev9898 have no Ryze controller; 7UPAIO contains only a commented
`#include "Ryze.h"` and commented dispatch line, with no implementation file.
`AIRyze` therefore adds support without shadowing or replacing an existing
KuroAIO champion.

The Fiora databases mention Ryze E/W only as enemy-parry vocabulary. Those
records are observations for Fiora and were not mistaken for a Ryze plugin.

## Pinned live data and reconciliation

- [CommunityDragon 16.14 Ryze bin](https://raw.communitydragon.org/16.14/game/data/characters/ryze/ryze.bin.json), SHA-256
  `2a5f45421dce6a4efe2239458afe7218e37b818dcc8f9dc49135bc32a6ca3b75`.
- [CommunityDragon 16.14 champion JSON](https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/13.json), SHA-256
  `0066421debe3dffc6d9375c2eebd8f25dd48ddb23621ccb6f56fa24e568fcd59`.
- [League Wiki Ryze mechanics](https://wiki.leagueoflegends.com/en-us/Ryze)
  was used for cast/missile timing, reset, Flux-consumption, splash and portal
  interaction details. Later Riot notes win whenever historical text differs.

Patch authority is deliberately explicit because several easily copied Ryze
guides still expose stale W or base-stat values:

- [Riot 25.11](https://www.leagueoflegends.com/en-gb/news/game-updates/patch-25-11-notes/)
  increased the Spell-Flux Q amplifier attached to Realm Warp to
  50/75/100%; pre-six remains 25%.
- [Riot 25.13](https://www.leagueoflegends.com/en-au/news/game-updates/patch-25-13-notes/)
  set W damage to 60/90/120/150/180 and its live mana progression to
  50/60/70/80/90.
- [Riot 26.3](https://www.leagueoflegends.com/en-gb/news/game-updates/patch-26-3-notes/)
  reduced W to 60% AP and 3% bonus mana.
- [Riot 26.12](https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-12-notes/)
  reduced base health from 645 to 620 and base attack damage from 58 to 55,
  explicitly moving early trading power away from a scaling combo mage.

The live arithmetic represented by the pure layer is:

- passive: spell ratios use bonus mana; maximum mana is multiplied by
  `1 + 0.001 * AP`;
- Q: 75/95/115/135/155 + 55% AP + 2% bonus mana;
- W: 60/90/120/150/180 + 60% AP + 3% bonus mana;
- E: 60/90/120/150/180 + 50% AP + 2% bonus mana;
- Flux Q: Q multiplied by 1.25 before level six, then 1.50/1.75/2.00 by R rank;
- two-Rune Q movement speed: 28/32/36/40/44% for two seconds.

The runtime removes passive amplification from observed maximum mana before
subtracting Ryze's level-scaled base mana. That avoids counting the passive's
own multiplication as item/bonus mana a second time.

## Live mechanics represented

### Overload first body

Q uses 0.25-second cast time, 1000 range, 55 missile radius and 1700 speed. It
stops on the first hostile champion, lane minion or jungle monster. The pure
solver models each body's velocity and solves missile/target relative motion;
it does not trust SDK container order or a static point-to-line collision.
Projectile-intercept walls remain a separate mandatory gate.

For every candidate angle, the controller requires the intended direct target
or chosen Flux detonation body to be the first contact. It then computes which
currently marked bodies are really inside the 350-radius splash (500 for a
large primary). A minion cannot be credited as an indirect champion hit merely
because it is visually near the champion.

### Rune and Spell Flux ledgers

Rune charges and Flux marks are deliberately separate state machines:

- W/E each reset Q, add one Rune up to two and refresh the four-second Rune
  window;
- Q consumes Rune charges and grants movement speed only at two;
- E marks its primary plus bodies inside 350, extended to 400 for a large
  primary;
- Flux lasts four seconds; W consumes the target's Flux to root for 1.5
  seconds; Flux Q consumes only its reached marked victims.

Predicted state is applied immediately after a controller cast so a buffered
follow-up can run on the next frame. Buff add/update/remove events then
confirm or correct it. Half/full Rune UI aliases and the movement-speed buff
are used as reconciliation signals, not as the only source of truth.

### Rune Prison and Spell Flux

W and E are targeted at 550 range and both reset Q. W is treated as a 50%
slow unless the target has live Flux. It is never labeled an interrupt when
unfluxed. E chooses between direct damage/reset, root setup, indirect champion
bridge, wave spread, refresh, peel, interrupt and objective policy.

An E-Q bridge requires all of the following: an E primary inside 550; the
priority champion inside the resulting mark spread; a distinct Q detonation
body that survives E; a clean moving first-body Q; and a splash that actually
reaches the champion. This is the wave-based range extension Ryze players use,
not a generic “cast E on nearest minion” rule.

### Realm Warp

R remains player-authorized. The G key requests an endpoint toward the cursor,
clamped to 3000 and rejected inside the 1000 minimum. The arrival gate checks
NavMesh, rooted/grounded state, enemy turret, local numbers, known vision
proxies, incoming line/point-click interruption and allied protected channels
standing in the 365-radius origin portal. Optional lethal-health automation is
off by default and still follows the player's cursor. The controller never
walks Ryze or an ally into the portal.

## One-trick decision model

The current [Mobalytics Ryze combo catalog](https://mobalytics.gg/lol/champions/ryze/combos)
lists the important families used here: Q-E-Q-AA-W-Q-E-Q-AA, E-E-Q waveclear,
E-W-AA-Q gank setup, Q-W-E-Q exit, Q-W-Q-E-Q and Q-E-Q. Current
[OP.GG Ryze data](https://op.gg/lol/champions/ryze/build) cross-checks the
E-Q-W opening and Q > E > W max order. These sources establish vocabulary;
the controller still decides when each branch is safe.

The runtime branches are not interchangeable macros:

- **Q-E-Q-W-Q-E-Q** is maximum DPS. It requires a committed, hard-CC'd or
  mobility-spent target, safe local numbers and real mana/cooldown continuity.
- **Q-E-W-Q** leads Q and applies E-W before the projectile arrives. At outer
  W/E spacing, the root can land before Q consumes Flux.
- **E-W-Q** spends one reset to guarantee root and then consumes two Runes for
  kite speed; this is preferred for peel, gapclose and urgent control.
- **W-Q-E-Q** consumes already-existing Flux immediately before a mobile
  target can leave.
- **Q-W-Q-E-Q** trades root for three Q casts only when verified lethal.
- **W-E-Q** uses immediate slow and two-Rune speed when escape is more valuable
  than a root.
- **Q-E-Q** is the ordinary W-preserving lane trade; **E-Q** is the lower-mana
  reset/Flux burst and may begin while Q is cooling because E resets it.
- **E-E-Q** waits for the real second-E cooldown and retargets the second E to
  the surviving detonation body instead of pretending the first body stayed
  alive.

The recent RyzeMains discussion
[Basic Combo for Ambushes](https://www.reddit.com/r/RyzeMains/comments/1q13tpy/basic_combo_for_ambushes/)
independently describes Q-E-Q-W-Q-E-Q, maximum-range W landing before the
travelling Q and auto weaving. The newer spacing thread
[empowered Q while rooting](https://www.reddit.com/r/RyzeMains/comments/1qhos2t/can_someone_explain_to_be_how_to_do_the_trick/)
clarifies that this is a max-range timing/extra-Q interaction rather than a
fictional second Flux amplification. The controller therefore uses Q-E-W-Q
for reliable root intent and reserves the longer Q-E-Q-W cadence for a
committed full-DPS window.

## Pro, OTP and matchup-video review

The July-2026 video pass deliberately included current full games and matchup
education rather than only montage clips:

- Strompest, *Season 16 Ryze vs Yasuo Guide – How to Beat Yasuo* (published
  two days before the research pass), plus his Season-2026 Actualizer and
  full-damage Ryze guides;
- Mysterias, *EDUCATIONAL Unranked to Grandmasters on Ryze* (11 February
  2026), a four-hour chaptered set covering Swain, Akshan, Naafiri, Galio,
  Viktor, Fizz, Rumble and Syndra;
- [Faker Ryze versus Galio, patch 26.11](https://www.youtube.com/watch?v=6sOOLx_9_Fg),
  5 June 2026;
- [Faker Ryze versus Viktor, patch 26.12](https://www.youtube.com/watch?v=z-Jf_Ua-mXA);
- [Faker Ryze versus Sylas, patch 26.12](https://www.youtube.com/watch?v=xz8arkzNH2Y);
- [Faker patch-26.12 1v2 sequence](https://www.youtube.com/shorts/DXV9MeHg3H8).

Portable behavior encoded from this review is patient 550-range discipline,
short reset trades before committed DPS, holding W for mobility/peel, casting
through real cooldown gaps, using the wave as a Flux bridge and preserving
manual control over macro Realm Warp. Lane pathing, tether movement, warding,
recall timing and portal macro remain player decisions because local spell
automation does not have the strategic information to replace them.

## Player cooperation

- Selected and locked targets are preferred but never bypass first-body,
  projectile-wall, immunity, mana or safety gates.
- Manual Q aim is never altered. A manual W/E can optionally receive one clean
  buffered Q during a bounded 680-ms window, only against a champion.
- Every manual spell cancels the controller's active cadence and produces a
  configurable ownership window.
- Orbwalker owns all attacks. The controller merely refrains from casting in a
  safe cooldown gap; it never forces an attack or movement command.
- Movement, attack-move, Hold, Stop, Flash, wards, target selection, R entry
  and unsafe macro routing remain player-owned.

## Verification and runtime telemetry

`ryze_geometry_test.cpp` compiles independently under MSVC C++17 and passes
158 assertions covering formulas, passive mana inversion, Rune state, moving
Q first contact, Flux spread/splash, bridge and wave plans, branch mana/reset
selection, auto-weave gates and Realm Warp safety. The complete `Release|x64`
solution builds successfully after profile/controller/catalog integration.

Runtime-only facts still need replay/live telemetry: production aliases for
every Rune pip/Flux/root/channel buff, event ordering when Q is buffered during
W/E cast time, Flux removal order across a multi-body splash, exact portal
occupancy at unusual bounding radii, and vision evidence outside observable
allied units/wards. Predicted state has short live-buff reconciliation,
bounded expiry fallbacks and a coach overlay exposing first body, Flux timers,
Runes, branch and ownership so these cases can be audited without silently
inventing certainty.

## Shared-helper boundary

Ryze reuses champion-neutral prediction, spell/buff observation, unit lookup,
resource/cast throttling, projectile-wall, protected-ally, point-click threat,
gapcloser/interrupt, lane-minion classification and the shared analytical
moving-circle contact solver. The contact solver was extracted from
Blitzcrank during this pass and is now shared by both Q implementations.
Rune/Flux ledgers, reset cadence, indirect E-Q topology, maximum-range root
timing, E-E-Q survival and Realm Warp no-abduction policy remain Ryze-local
because their semantics do not apply to another champion.
