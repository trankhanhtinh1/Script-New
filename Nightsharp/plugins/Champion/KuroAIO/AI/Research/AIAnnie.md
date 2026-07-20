# AIAnnie research and implementation dossier

Research date: 2026-07-18  
Target baseline: League of Legends 26.14 / CommunityDragon 16.14  
Controller: `AI/Controllers/AIAnnieController.h`  
Pure mechanics: `AI/Controllers/AIAnnieGeometry.h`  
Published scenarios: **239**

## Completion claim

This is not a generic Q-W-E-R profile. `AIAnnieController.h` owns the full
decision loop and models the parts of Annie that change spell order in real
play:

- Pyromania stacks are separated into gain-on-cast and gain-on-Q-impact
  events.
- A pending Q, W and R each has a resolve clock, so the first primed damage
  landing—not the first button pressed—owns the stun.
- Three-stack Q-E hidden stun and the riskier two-stack Q-E-W landing race are
  explicit state machines.
- Q refund, half cooldown, targeted missile travel, spell shields and
  projectile-destruction walls are separate decisions.
- W is a cone resolved from Annie's position at cast end. A player Flash
  during the buffer changes the origin; the plugin never presses Flash.
- E chooses between a threatened ally, self-trade, flee speed, hidden-stun
  priming and safe passive building using predicted impact time.
- R placement scores the actual enemy set, protected-ally threat, spell
  shields, target priority and summon follow-up value.
- Tibbers has tracked summon/enrage/lifetime/target state and a throttled
  turret/leash/manual-lock soft autopilot.

The controller publishes 239 independently named scenarios, owns its decision
loop, has a standalone pure mechanics test and is catalogued only because
Annie had no protected KuroAIO implementation.

## Source authority

### Primary live data

1. [Riot patch 26.4](https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-4-notes/)
   - Q base damage became 80/125/170/215/260.
   - E cooldown became 10 seconds at every rank.
2. [Riot patch 25.18](https://www.leagueoflegends.com/en-ph/news/game-updates/patch-25-18-notes/)
   - W base damage became 70/110/150/190/230.
   - R passive magic penetration became 10/15/20.
3. [Riot patch 25.8](https://www.leagueoflegends.com/en-us/news/game-updates/patch-25-08-notes/)
   - W cooldown became 7 seconds.
   - Power shifted from Tibbers' persistent damage into Annie's spells.
   - This is why older Tibbers aura/attack values were rejected.
4. [CommunityDragon champion JSON 16.14](https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/1.json)
   - SHA-256:
     `5C64647E42B44407724736B2E70FC48BEA4FA8ED349267113EC29247F777FC89`
5. [CommunityDragon Annie bin 16.14](https://raw.communitydragon.org/16.14/game/data/characters/annie/annie.bin.json)
   - SHA-256:
     `0855A300D65ADEEAB1EE1DBF3D04ED77F993E2FA19FDFC46E3E8B44A05153D9F`
   - Main `AnnieR` data confirms current initial damage, aura 8/12/16,
     Tibbers attacks 30/45/60, 10% AP attack ratio, 3-second enrage,
     10-second avenger enrage, 45-second lifetime and internal -0.1
     attack-speed decay data.
6. [Riot Annie champion page](https://www.leagueoflegends.com/en-us/champions/annie/)
   - Used for current official ability identity and wording, not detailed
     runtime geometry.

### Secondary mechanics cross-checks

- [Current Annie mechanics page](https://leagueoflegends.fandom.com/wiki/Annie/LoL)
  supplied cast-order notes, Q refund behavior, E targeting forgiveness,
  cast-end W origin, Tibbers recovery/revenge and the observed five enrage
  attack-speed stages: 1.736, 1.536, 1.307, 1.043 and 0.739.
- [Current Summon: Tibbers reference](https://www.mobafire.com/league-of-legends/ability/summon-tibbers-6)
  independently cross-checked summon damage and enrage behavior.
- [Meraki Annie data](https://cdn.merakianalytics.com/riot/lol/resources/latest/en-US/champions/Annie.json),
  SHA-256
  `73494176D08BAC3EBFDF4968A5787E1643F51096C52EFFC8C366A8B658391C11`,
  was used only as a structural cross-check. Its lagging numeric fields do not
  override Riot patch notes or the current CDragon bin.

The community mechanics page still displayed pre-25.8 Tibbers attack and aura
numbers during this audit. Those values were deliberately rejected in favor
of Riot's 25.8 change and CDragon's current main `AnnieR` payload.

## Local implementation audit

The complete workspace search found two Annie controllers:

1. `plugins/Champion/OneKeyToWin/Champions/Annie.h`
2. `plugins/Champion/OneKeyToWin/OKTW_CSharp/Champions/Annie.cs`

The C++ file is a port of the same OneKeyToWin lineage. They contributed useful
historical intent—basic stun awareness, Flash-R and Tibbers orders—but were not
reusable as a one-trick implementation because they:

- use obsolete ranges and damage;
- treat W like an old pseudo-line rather than a cast-end cone;
- reduce Pyromania to `HasBuff("pyromania_particle")`;
- do not model Q-impact stack gain or landing races;
- cannot reserve Q/W/R as distinct stun consumers;
- do not distinguish spell-shield break from stun loss;
- do not predict E impact or select a protected ally;
- do not model current Tibbers enrage, turret, leash or manual ownership; and
- use fixed combo priorities instead of state-dependent spell order.

Other workspace occurrences of `Annie` are threat/skill databases, assets or
auto-attack spell-name tables, not champion controllers.

No existing KuroAIO Annie route exists. The protected legacy dispatch remains
untouched.

## High-elo, one-trick and combo research

### Current play and player cooperation

- [2026 Challenger Annie AMA](https://www.reddit.com/r/AnnieMains/comments/1truczo/hit_challenger_for_the_first_time_with_annie/)
  emphasizes Q-E-auto/Electrocute short trades, E-auto-auto level one,
  E-Q-auto level two, disciplined Flash use, grouping and avoiding poor side
  lanes.
- [2026 Annie gameplay discussion](https://www.reddit.com/r/AnnieMains/comments/1qnyqes/how_do_i_play_as_annie/)
  reinforces Q farming, reactive E, squishy-target selection and the weakness
  of forcing fights without R.
- [Current Skill Capped build/guide](https://www.skill-capped.com/lol/guides/builds/annie/mid/build-1)
  was used to cross-check the contemporary mid-lane context.
- [Mobalytics Annie combos](https://mobalytics.gg/lol/champions/annie/combos)
  independently lists Flash-R-W-Q-auto and a two-stack Q-W-E-auto family.

The implementation does not issue player attacks. Instead it recognizes the
player's auto timing and adds E/Q/W only when that continues the short-trade
window. This preserves the player's spacing and cancels no ordinary attack
windup.

### Landing-order and hidden-stun mechanics

- [Advanced Annie mechanics discussion](https://www.reddit.com/r/AnnieMains/comments/shct0b/)
  documents two-stack Q-minion/E/R ideas, three-stack Q-E, W anti-dash and
  different Q-E-W/R orderings.
- [Three-stack Q-E timing discussion](https://www.reddit.com/r/AnnieMains/comments/kpy21r)
  confirms that Q's stack occurs on landing, enabling E before impact.
- [W anti-mobility and Q follow discussion](https://www.reddit.com/r/AnnieMains/comments/bzag4q)
  supports choosing fast W against movement and point-click Q when following
  a dash matters more.

The controller does not copy combo strings blindly. It simulates the current
landing clocks and decides whether Q or W actually consumes the stun. If the
race changes because distance changes, the state changes with it.

### Current video/replay packet

- [Faker Annie vs Sylas, 2026](https://www.youtube.com/watch?v=bDcaZDV0MGI)
- [EUW Challenger Annie vs Viktor, patch 26.13](https://www.youtube.com/watch?v=2ZL15EkeVQQ)
- [Current Annie combo guide](https://www.youtube.com/watch?v=QbdDfKcHNVA)
- [Detailed current Annie gameplay guide](https://www.youtube.com/watch?v=vvce6AxUmto)
- [PekinWoof on current Annie buffs](https://www.youtube.com/watch?v=sWOtHaQmocc)

Useful repeated decisions across these sources were:

- hold stun rather than spending it merely because a target is in maximum
  range;
- use Q last hits to control mana and passive state;
- use E as a reaction and as concealed engage preparation;
- W before Q when its short resolve punishes mobility;
- R without stun only when another control source or summon value makes it
  worthwhile;
- shield the ally who is actually entering the fight;
- weave Annie's unusually long 625-range auto attacks; and
- decline a bad Flash angle rather than forcing a memorized combo.

The plugin therefore never casts Flash. Manual W-Flash and R-Flash are
observed, visualized and continued only after the player has supplied Flash.

## Live kit packet

### Pyromania

- Maximum four stacks.
- Annie starts and respawns primed.
- W/E/R add a stack on cast; Q adds a stack on hit.
- At four stacks, the next Q/W/R damage landing consumes all stacks and stuns.
- The first damage event to land owns the stun even if cast later.
- E cannot consume the stun.
- A spell shield blocks the stun but consumes the primed proc.
- A blocked non-primed spell can still grant its passive stack.
- Stun duration scales 1.25/1.5/1.75 seconds with level breakpoints.

Implementation consequence: `PassiveStacks`, Q/W/R pending clocks and
`ReservedStunIntent` are separate fields. The pure `SimulatePyromania` helper
tests cast gains, Q-impact gains, shield consumption and landing order.

### Q — Disintegrate

- Target range: 625.
- Cast time: 0.25 seconds.
- Missile speed: 1400.
- Mana: 60/65/70/75/80.
- Damage: 80/125/170/215/260 + 80% AP.
- A kill refunds mana and halves the remaining base cooldown.
- Projectile destruction can stop the missile.

Implementation consequence:

- exact impact clock and missile lifecycle tracking;
- pure refund/cooldown test;
- current raw damage rather than SDK historical fallback;
- live projectile-wall collision query;
- health-predicted last hits only; and
- no visible-stun farm Q while an enemy champion is close by default.

### W — Incinerate

- Server range used: 600.
- Cast time: 0.25 seconds.
- Full server cone angle used: 49.52 degrees.
- Cooldown: 7 seconds.
- Damage: 70/110/150/190/230 + 80% AP.
- The cone originates at Annie's position when cast time ends.

Implementation consequence:

- circle-versus-sector geometry includes gameplay radius at the angular edge;
- the apex can clip a gameplay circle slightly behind Annie;
- candidates include predicted enemy bearings and pair midpoints;
- a Flash inside the buffer relocates the cone origin; and
- Flash itself remains entirely player-owned.

### E — Molten Shield

- Ally range: 800.
- No cast time.
- Cooldown: 10 seconds at every rank.
- Shield: 60/95/130/165/200 + 40% AP for 3 seconds.
- Movement speed: 20% to 50% by champion level, decaying over 1.5 seconds.
- Reaction damage: 25/35/45/55/65 + 40% AP, once per enemy per shield.
- Tibbers is also shielded whenever active.

Implementation consequence:

- incoming targeted and line threats receive an impact clock;
- allies are ranked by damage, CC, lethality, priority and time to impact;
- critical defense overrides an offensive stack reservation;
- noncritical defense can preserve the concealed three-stack threat; and
- E supplies hidden Q priming only when it resolves before Q impact.

### R — Summon: Tibbers

- Cast range: 600.
- Initial server hit radius: 250 plus gameplay radius.
- Cast time: 0.25 seconds.
- Initial damage: 150/275/400 + 75% AP.
- Passive magic penetration: 10/15/20.
- Tibbers lifetime: 45 seconds.
- Current aura: 8/12/16 + 4% AP per second, quarter-second ticks.
- Current attack: 30/45/60 + 10% AP magic damage.
- Enrage lasts 3 seconds on summon or champion stun.
- The next five attack-speed stages are approximately
  1.736/1.536/1.307/1.043/0.739 before returning to 0.625.
- On Annie death, the exposed enrage duration becomes 10 seconds; Tibbers
  heals 50% missing health and pursues the killer or another low-health enemy.
- After five seconds out of combat, Tibbers regenerates 6% maximum health per
  second and gains catch-up movement toward Annie.

Implementation consequence:

- R is not spent merely as a low-health check;
- placement scores target set, priority, dash, spell shield, protected threat
  and allied follow-up;
- summon, stun-triggered enrage and five pet attacks are tracked separately;
- pet orders are throttled and reject ordinary enemy-turret pursuit; and
- owner-distance and critical-health return paths are explicit.

## One-trick state machine

### Passive families

1. **HoldThree**: remain visually unprimed while threatening Q-E, W or R.
2. **HiddenQPrime**: Q at three, E before impact, Q owns the stun.
3. **TwoStackRace**: Q at two, add E/W cast stacks, compare Q and W landings.
4. **QStunCatch**: reliable single-target point-click catch.
5. **WStunCone**: close or multi-target fast AoE catch.
6. **RStunEngage**: best-scored multi-target summon catch.
7. **QShieldBreak**: non-primed Q consumes the shield and still advances
   passive; spending visible stun is separately disabled by default.

### Combat postures

The controller publishes eleven postures:

- Neutral
- LaneControl
- ShortTrade
- Catch
- Ambush
- Teamfight
- Peel
- Disengage
- Siege
- Objective
- TibbersControl

These labels are diagnostic; the decisions still come from live target,
passive, landing, incoming-threat and pet state.

### Stun-consumer selection

- **R outranks W/Q** for a sufficiently valuable multi-target circle.
- **W outranks Q** for close mobility, a better multi-target cone or faster
  anti-gapcloser resolve.
- **Q outranks W/R** for one carry, dash-follow reliability or a channel whose
  remaining time excludes the larger spell.
- Pending spells can veto a new consumer when they would land first.
- A spell shield reduces AoE placement score and blocks ordinary primed Q.

### Player cooperation

- Selected-target preference comes from the shared engine.
- Annie movement, spacing and attacks remain player-owned.
- Ordinary casts preserve an auto windup.
- Reactive casts use a fast follow-up path only for a real timing window.
- Manual Q/W/R events seed the same champion state machine.
- Manual Flash is observed; never cast.
- The controller continues after manual W-Flash/R-Flash rather than trying to
  perform Flash itself.
- Tibbers commands yield after every observed manual R order.

## Tibbers manual-ownership limitation

NightSharp exposes `PetAttack` and `PetMove` output but does not expose an
inbound `OnIssueOrder` callback for player Alt-clicks. Therefore the controller
cannot prove ownership of every pet order.

The safe compromise is intentionally called **soft autopilot**:

- it is enabled by a separate toggle;
- every observed manual R command creates a 1350 ms lock;
- controller commands are tagged with a short ownership window;
- repeated orders are throttled to at least 575 ms;
- only selected combat, peel, farm, turret-return and leash-return cases issue
  an order; and
- the player can disable it completely.

The dossier does not claim perfect arbitration until the SDK gains a real
inbound order event. This is a runtime telemetry gap, not hidden behavior.

## Menu policy

Important conservative defaults:

- build passive only to three;
- hold visible stun during noncommittal harass;
- preserve stun while farming near an enemy champion;
- W harass disabled;
- automatic R kill-secure disabled;
- Tibbers turret dive disabled;
- soft pet autopilot enabled but manually lockable;
- Flash always player-owned; and
- movement/attack-move always player-owned.

## Pure verification

`tests/annie_geometry_test.cpp` verifies:

- three-stack Q without E primes but does not stun;
- Q-E hidden stun;
- first-landing stun ownership;
- W stealing a pending Q stun and Q rebuilding one stack;
- spell-shield stun consumption;
- Q impact, live damage, mana refund and half cooldown;
- W angular/apex geometry and cast-end Flash relocation;
- E shield, reaction and movement-speed decay;
- R radius and current initial damage;
- Tibbers current aura tick and attack damage;
- five live Tibbers enrage attack-speed stages;
- primed R spell-shield placement penalty; and
- pet manual lock, turret, enrage, recovery and zone command policy.

## Runtime replay checklist

Pure tests cannot prove live memory names and event order. Replay/live telemetry
must cover:

1. exact passive buff names/count on spawn, death and respawn;
2. Q missile create/delete payload names and shield/wall destruction;
3. W resolve origin across several Flash timings and latency values;
4. manual R summon versus R pet-command names;
5. Tibbers object name/team/network ID and enrage buff name;
6. pet order acceptance in the current write phase;
7. E targeted ally and line-threat timing;
8. R initial radius against targets with different gameplay radii;
9. current magic penetration reflected in live player stats; and
10. manual Alt-click conflicts until an inbound order hook exists.

## Shared-helper decision

The controller reuses champion-neutral helpers for tick time, resources,
readiness, spell costs, prediction, auto capture, protected-ally ranking,
gapcloser/interrupt capture, local-event ownership, spell names and projectile
walls.

Pyromania landing simulation, W cone geometry, R placement, shield policy and
Tibbers orchestration remain Annie-specific. They are not moved into shared
helpers merely because future champions may also have stacks, cones, shields
or pets; their semantics and ordering are different.

## Verification status

- Standalone Annie geometry test: pass.
- `Release|x64` full solution build after catalog integration: pass.
- All nine completed-controller geometry suites: pass.
- Exact duplicate-body scan: 604 bodies at the 100-character threshold, zero
  repeated runtime implementation groups.
- Coverage reconciliation: 10 preserved legacy routes + 9 completed AI
  controllers + 154 queued champions = 173 live champions.
