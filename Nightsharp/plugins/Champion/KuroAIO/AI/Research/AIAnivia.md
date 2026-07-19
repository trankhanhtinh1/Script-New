# AIAnivia research dossier

Research date: 2026-07-18
Target game revision: League of Legends 26.14 / CommunityDragon 16.14
Controller: `Controllers/AIAniviaController.h`
Profile: `Profiles/AIAnivia.h`
Pure mechanics: `Controllers/AIAniviaGeometry.h`
Shared primitives: `AIGeometry.h`, `AIControllerHelpers.h`

## Source ledger

1. Riot champion page: <https://www.leagueoflegends.com/en-us/champions/anivia/>
   - First-party semantic baseline for Rebirth, Flash Frost, Crystallize,
     Frostbite and Glacial Storm.
   - Public prose establishes intended relationships; exact timing, radius,
     damage stages and runtime identities are pinned below.
2. Riot patch 25.20:
   <https://www.leagueoflegends.com/en-us/news/game-updates/patch-25-20-notes/>
   - Current Q cooldown is `11/10/9/8/7` and current E base damage is
     `55/80/105/130/155`. Older records retaining the prior values lose.
3. Riot patch 26.10:
   <https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-10-notes/>
   - Current base armor is 19 and armor growth is 4.1. These are relevant to
     passive/egg danger display, not reimplemented as controller defenses.
4. Riot patch 14.22:
   <https://www.leagueoflegends.com/en-sg/news/game-updates/patch-14-22-notes/>
   - Current Frostbite AP ratio is 55%; empowered E doubles the resulting
     damage package.
5. Riot patch 10.25:
   <https://www.leagueoflegends.com/en-au/news/game-updates/patch-10-25-notes/>
   - Authoritative Q modernization: 950 missile speed and pass-through Chill.
     This invalidates the old local port's 870-speed model.
6. Pinned CommunityDragon champion record:
   <https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/34.json>
   - SHA-256: `3D6FAE4097562EA00D0B635E7F7D400DC50F49E018A143887A955A57F39428BE`.
   - Confirms champion id 34, mana resource, public spell identities, ranks,
     costs and current client-facing records.
7. Pinned CommunityDragon game-bin record:
   <https://raw.communitydragon.org/16.14/game/data/characters/anivia/anivia.bin.json>
   - SHA-256: `737AC012523EB39C86383F73CEFA83A241DC761B9083C1885BA788D45AEBDE04`.
   - Directly inspected runtime identities include `FlashFrostSpell`,
     `GlacialStorm`, `ChilledAniviaUlt`, `Rebirth`, `RebirthReady` and
     `RebirthCooldown`.
   - Used for Q line/explosion geometry, wall segment data, E travel, R growth,
     tick/drain values, Chill duration and passive clocks.
8. Meraki Analytics Anivia record:
   <https://cdn.merakianalytics.com/riot/lol/resources/latest/en-US/champions/Anivia.json>
   - Research copy SHA-256:
     `24BED4DDE6933C6F13DC15801F0161F2FBBF31CF12D430857F369F220F05A0CD`.
   - Used for interaction and effect-stage cross-checking. Its Q cooldown and
     E base damage were stale during this pass, so Riot 25.20 and pinned
     CommunityDragon values explicitly win.
9. Current Master Anivia OTP combo guide:
   <https://www.mobafire.com/league-of-legends/build/master-anivia-otp-anivia-combo-guide-with-gifs-652204>
   - Documents double-hit Q, holding Q as pressure, wall catch, universal
     post-six peel/catch, double-E and longer “infinity” families with GIFs.
   - The key translation is stateful: the controller saves Q after E1 while R
     still controls the target, then spends Q on the exit or E2 window.
10. EUW Challenger Anivia OTP guide:
    <https://www.mobafire.com/league-of-legends/build/euw-challenger-anivia-otp-guide-606139>
    - Independent specialist cross-check for `W -> Q -> E`, `Q -> W -> E`,
      direct wall displacement, R/W containment and repeated empowered E.
11. Current combo video catalogue:
    <https://mobalytics.gg/lol/champions/anivia/combos>
    - Video-backed independent ordering cross-check. It is not numerical
      authority and does not replace impact-time validation.
12. Current Challenger replay, patch 26.13:
    <https://www.youtube.com/watch?v=KCCznkJR048>
    - DSG Callme Anivia mid versus Ryze; published 2026-07-01 by Anivia
      Challenger Replays. Used as a current high-elo context check for Q
      restraint, R placement ahead of movement and wall exit control.
13. Current pro replay, patch 26.11:
    <https://www.youtube.com/watch?v=KpE-qOhJGUk>
    - GEN Chovy Anivia mid versus Darius; published 2026-06-05. Cross-checks
      the difference between zoning with held abilities and consuming the
      whole kit on first contact.
14. Current pro replay, patch 26.10:
    <https://www.youtube.com/watch?v=BKVB7kGhdpE>
    - GEN Chovy Anivia mid versus GAM Aress's Cassiopeia; published
      2026-05-22. Used as an independent current-patch positioning and
      short-trade reference.
15. Current 1500-LP Challenger Anivia OTP gameplay:
    <https://www.youtube.com/watch?v=HX5uVTcxhag>
    - SanSiroBro, published 2026-01-26. Used for specialist pacing, retained Q
      pressure and the practical value of R/W space control over raw spell spam.
16. Current Challenger Anivia AMA:
    <https://www.reddit.com/r/AniviaMains/comments/1tuugwj/best_anivia_na/>
    - Specialist guidance emphasizes holding Q after wall/R control, taking two
      empowered Frostbites and using Q when the target leaves the storm.
17. Current specialist tips:
    <https://www.reddit.com/r/AniviaMains/comments/1s4jrbd/picking_up_anivia/>
    - Cross-check for lane restraint, Q pressure, wall mastery and mana-aware
      R use. Community claims remain subordinate to pinned mechanics.
18. Specialist hidden-mechanics discussion:
    <https://www.reddit.com/r/AniviaMains/comments/1kjx52n>
    - Used to enumerate testable Q pass/detonation, spell-shield, wall and
      storm interactions; only reproducible/pinned claims were encoded.
19. Current strategy reference:
    <https://leagueoflegends.fandom.com/wiki/Anivia/Strategy>
    - Cross-check for delaying Q detonation, wall displacement, R expansion
      and Frostbite timing. No long text is copied into the implementation.
20. Local champion-plugin audit:
    - KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, ziblldev9898 and old C#
      sources were searched for Anivia champion logic.
    - The only local champion implementation is the old OneKeyToWin C++ port
      and its C# source. The port still has TODO Q/R object tracking, Q range
      1000, Q speed 870, R range 685 and coarse W/R decisions.
    - KuroAIO has no protected legacy Anivia plugin. `AIAnivia` is additive and
      cannot preempt any of the ten preserved legacy routes.

Numerical authority order is Riot patch note, pinned CommunityDragon game-bin,
then corroborating data. Guides, videos and community discussion determine
decision families and edge cases, never patch values by themselves.

## Current mechanical model

### Rebirth is a six-second no-input state

- Fatal damage with Rebirth available creates an egg for six seconds and
  restores all health before the egg can be killed. Rebirth then has a
  240-second cooldown unaffected by ability haste.
- The controller tracks `RebirthReady`, `RebirthCooldown` and the actual egg
  state independently. Every cast is suspended in egg; it does not pretend it
  can move, attack or select a safe egg position.
- Chronoshift and Guardian Angel resolve before Rebirth. This matters to player
  coaching only; the plugin never manufactures a passive bait.
- Egg armor/MR scaling remains game-owned. It is not duplicated as a fake
  damage-reduction simulation.

### Flash Frost is pressure plus two hit regions

- Q has 1075 effective range, 0.25 cast time, 950 speed, 220 full line width
  and a 225-radius explosion.
- Pass damage is `50/70/90/110/130 + 25% AP`; explosion damage is
  `60/95/130/165/200 + 45% AP`. Both apply/refresh Chill, while only the
  explosion stuns for `1.1/1.2/1.3/1.4/1.5` seconds.
- The controller binds the actual local missile by lifecycle event, scans the
  missile collection as recovery and uses the cryo particle only as fallback.
  A manual Q receives the same pass/detonation state machine.
- Detonation normally waits for a positive target-center overshoot while the
  explosion still overlaps. Target/missile movement is projected one reaction
  interval ahead so the controller can detonate before a recoverable window is
  lost.
- Q is deliberately held when an ordinary target has not committed, especially
  at long range or with Flash available. Dashes, hard CC, movement toward
  Anivia, wall path forcing, storm exits, channels and peel are distinct gates.
- An ordinary spell shield can consume the pass and leave the explosion as the
  next spell instance. The controller remembers that target and waits for the
  shield to disappear before exploding. Spell immunity, invulnerability and
  Black Shield-like semantics are not flattened into that branch.
- Q cannot be recast while Anivia is silenced/stunned. The plugin checks action
  state instead of issuing a doomed detonation request.

### Crystallize is occupied terrain, not target.position

- W has 1000 range, a 0.25 cast and lasts five seconds. Rank 1-5 owns
  `4/5/6/7/8` segments and outer segment distances
  `400/500/600/700/800`.
- Each end contributes its 100 gameplay radius, yielding occupied widths
  `600/700/800/900/1000`. The pure model builds the perpendicular wall segment
  and tests its capsule rather than drawing an arbitrary width.
- A champion overlapping W is displaced 120 units to one side after clearing
  wall and target radii. Multiple slightly offset candidates avoid relying on
  an undefined exactly-centered tie.
- Candidate value distinguishes keeping a target in R, forcing a Q route,
  interrupting through displacement, splitting a team, peeling a carry and
  cutting a committed gapcloser.
- A wall is rejected if its capsule overlaps an ally, crosses a pressured
  ally's retreat, intersects an allied dash, pushes the target out of R or
  creates avoidable turret aggression. Peel separation is the intentional
  exception, not a generic “ignore allies” switch.

### Frostbite is an arrival race

- E has 650 public range, 0.25 cast time and 1600 missile speed. Current damage
  is `55/80/105/130/155 + 55% AP`; the complete package doubles only if Chill
  exists when E arrives.
- Every E computes its impact tick. Live/confirmed Chill, a scheduled Q
  explosion and the future full-storm tick are compared against that impact
  with a safety margin.
- This permits the real advanced race—E first, Q detonates before E arrives—
  without granting double damage merely because the combo intends to Chill.
- Growing R does not Chill. An E scheduled before the 1.5-second full-storm
  boundary is accepted only if the target will still be inside and the full
  tick beats E impact.
- Ordinary E is reserved for conservative lethal damage by default. Optional
  AA-E poke is explicit and high-mana. Auto attacks are otherwise preserved.

### Glacial Storm is persistent geometry and economy

- R has 750 cast range, starts at radius 200 and grows linearly to radius 400
  over 1.5 seconds. It ticks immediately and every 0.5 seconds.
- Normal ticks deal `15/22.5/30 + 6.25% AP`; fully formed ticks deal exactly
  three times that package and are the ticks that apply Chill.
- Activation costs 60 mana. Rank drain is `35/45/55` mana per second and the
  recast is locked for the first second.
- Placement candidates include current/cast-delay/full-growth predictions,
  capped movement leads, path leads, gapcloser endpoints and enemy-pair
  midpoints. Score uses target value, selected-target inclusion, control state,
  dashes and allied follow-up.
- R is kept through a pending impact-time E race and through a short no-contact
  grace. It ends when drain would consume the Q/E reserve or the zone stays
  empty beyond that grace.
- A mature empty storm can be ended for a substantially better scored center.
  The replacement still waits for real R readiness; no movement or cooldown is
  invented. W/Q containment is preferred before relocation.
- A player-cast storm is observed and preserved through its first setup window,
  then receives the same mana/contact safety instead of being blindly toggled.

## Posture and decision state machine

| Posture | Primary obligation | Main veto |
|---|---|---|
| Lane control | Hold Q pressure, take impact-valid E, conserve mana | No speculative long Q or cheap R drain |
| Catch | Convert commitment into Q/W/R control | No fixed spell order when the target still owns an exit |
| Zone | Preserve productive R and rotate W/E/Q around exits | No early E from a growing storm |
| Peel | Split/stun the diver threatening the selected carry | No fresh damage sequence before carry safety |
| Disengage | Q the committed pursuer, wall the route, leave R behind | No invented movement or unsafe ally wall |
| Siege | Control approach space without turret-triggering W | No decorative wall or automatic dive |
| Objective | Sustain R/E/Q around real neutral units | Keep spell/mana reserve when enemies are near |
| Egg | Observe passive timer only | All casts are forbidden |
| Neutral | Continue manual Q/R and reactive events | No generic Q-W-E-R fallback |

## Combo-family translation

1. Pre-six short trade: `AA -> Q pass -> Q explosion -> AA -> empowered E`.
   - Q is fired only on commitment/guarantee; both Q hits are independently
     verified.
2. Pre-six extended pressure: `AA -> E/AA pressure -> hold Q -> Q -> E`.
   - “Hold Q” is an actual controller state, not missing implementation.
3. Wall catch: `Q -> W route/displacement -> Q explosion -> E` or
   `W real block -> Q -> E`.
   - W must either intersect the escape route or improve the Q geometry.
4. Universal post-six catch/peel: `led R -> Q -> E -> optional W -> E`.
   - E waits for pass/detonation/full-R Chill at its impact tick.
5. Double E: `R grow -> W containment -> empowered E1 -> hold Q -> Q exit
   denial -> empowered E2`.
   - Q stays unused while R safely contains the target, matching current OTP
     guidance instead of front-loading every button.
6. Extended control: relocate a mature empty R only when the next center wins
   by the configured score, then repeat impact-valid E/Q windows.
7. Spell-shield line: `Q pass consumes shield -> delayed Q explosion -> E`.
   - No explosion is sent while the shield remains visible.
8. Peel chain: detonate an existing Q, otherwise Q the diver, wall a real
   ally-threat separation, then R only for a critical close threat.
9. Disengage: `Q pursuer -> W barrier -> R behind Anivia`; each spell is
   independently gated and the controller never moves the player.

## Shared-helper and duplicate boundary

- Reused neutral helpers cover local-player identity, target lookup, readiness,
  current resource, spell cost/menu state, responsive cast throttling, target
  prediction, AA capture/range, allied follow-up, protected-ally selection,
  enemy-cast analysis and gapcloser/interrupt capture.
- `MissileEventIsLocal` remains the single local missile ownership check. Q
  pass/explosion interpretation remains Anivia-local.
- `AIGeometry.h` supplies direction, rotation, cross product and point/segment
  projection. Anivia composes those into Q and wall shapes instead of cloning
  vector math.
- Kept local because the semantics are champion-specific: Q overshoot and
  shield sequencing, wall displacement/path safety, E scheduled-Chill race,
  R growth/drain/relocation and Rebirth suspension.
- The post-Anivia duplicate scan is recorded in `SharedHelperAudit.md`. Neutral
  missile scanning/event-id helpers are extracted only if two controllers have
  the same ownership and cleanup semantics; matching loop syntax alone is not
  sufficient.

## Player-cooperation contract

- The selected target is preferred. Proactive Q checks cursor agreement;
  reactive peel and interrupt may override it.
- Only Anivia Q/W/E/R are cast. Movement, attack-move, Flash, summoner spells,
  Hold and Stop remain player-owned.
- Ordinary auto attacks are kept. One attack is canceled only when its windup
  would lose an already-present Q detonation or E-Chill impact race.
- Manual Q receives live missile/pass/detonation management. Manual W updates
  wall timing. Manual E updates the impact clock. Manual R is preserved through
  its setup and later managed by real contact/mana pressure.
- Drawings expose Q missile/explosion, actual W segment, growing/full R,
  protected ally/diver, Chill confidence and passive state so the player can
  understand why the controller is holding or spending a spell.

## Acceptance scenarios and verification

`AIAniviaController.h` publishes 170 champion-specific acceptance scenarios
(the exact catalog audit is authoritative). They cover nine postures, real Q
missile state, pass/explosion geometry, spell-shield sequencing, W rank/ally
safety, impact-time E races, double-E state, R growth/ticks/mana/relocation,
passive suspension, manual continuation, farm/objective policy and input
ownership.

`tests/anivia_geometry_test.cpp` isolates:

- Q pass and explosion edges, travel and double-hit overshoot;
- recoverable detonation forecasting;
- E travel and scheduled-Chill races;
- current Q/E damage stages;
- W ranks, orientation, capsule, displacement and ally-path intersection;
- R growth, full edge, tick cadence, mana drain, damage and movement lead;
- value-weighted multi-target storm scoring.

The standalone C++17 geometry test passes. Full `Release|x64` MSBuild succeeds
with Anivia in the additive AI catalog and emits `bin/Release/NightSharp.dll`.
The complete regression suite and coverage/duplicate audits are rerun after the
dossier/manifest update before this champion is marked complete.

## Known limits requiring live/replay telemetry

- Runtime particle/buff aliases can change by skin or patch. Dedicated missile
  payload is primary; particle and predicted clocks are bounded fallbacks.
- W's server choice for a perfectly centered displacement is not relied on;
  plans offset the wall. Rare terrain correction still requires replay logs.
- Path safety sees current ally/dash paths, not player intent several seconds
  ahead. W therefore remains conservative and exposes its segment visually.
- Scheduled Q/R Chill is geometry-based until the server buff callback arrives.
  E uses a safety margin, but extreme latency/server correction needs telemetry.
- R object lifecycle can transiently lag. The cast center continues state until
  the allied object binds; enemy storm objects are team-filtered.
- Fog-of-war targets and routes cannot be predicted. No Q/W/R guarantee is
  claimed through hidden information.
- Patch changes after 26.14 require repinning Riot/CommunityDragon values and
  reviewing current OTP/pro material before loosening any gate.
