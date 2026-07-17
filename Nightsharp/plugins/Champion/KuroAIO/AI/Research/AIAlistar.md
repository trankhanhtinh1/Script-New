# AIAlistar research dossier

Research date: 2026-07-17  
Target game revision: League of Legends 26.14 / CommunityDragon 16.14  
Controller: `Controllers/AIAlistarController.h`  
Profile: `Profiles/AIAlistar.h`  
Pure mechanics: `Controllers/AIAlistarGeometry.h`  
Shared primitives: `AIGeometry.h`, `AIControllerHelpers.h`

## Source ledger

1. Riot release baseline: <https://www.leagueoflegends.com/en-au/news/game-updates/league-of-legends-patch-26-14-notes/>
   - Patch 26.14 was published on 2026-07-14 and is the release baseline for this controller.
   - Alistar has no entry in the 26.14 champion changes, so the pinned 16.14 game data and the last authoritative Riot changes remain the numerical baseline.
2. Pinned CommunityDragon champion record: <https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/en_us/v1/champions/12.json>
   - SHA-256: `C01A752FE50B6E34AC51185D51C4807C5EF0DE988C69DC0DB3211B11CA342B48`.
   - Confirms champion id 12, public spell identities, costs, cooldowns and display ranges.
3. Pinned CommunityDragon game-bin record: <https://raw.communitydragon.org/16.14/game/data/characters/alistar/alistar.bin.json>
   - SHA-256: `BF6654C39CF6F15838E5C94C606CAD81A111F30555D5B850BC8081B78B005A5B`.
   - Runtime identities inspected directly include `Pulverize`, `Headbutt`, `AlistarE`, `AlistarEAttack`, `AlistarPassive`, `AlistarPassiveStacks`, `AlistarPassiveHeal`, and `FerociousHowl`.
   - The character record exposes selection radius 140, pathfinding radius 50 and gameplay collision radius 80. W timing therefore cannot be modeled as a fixed 650 / 1200 travel time.
4. Meraki Analytics latest Alistar record: <https://cdn.merakianalytics.com/riot/lol/resources/latest/en-US/champions/Alistar.json>
   - Local research copy SHA-256: `C2FC3DCC54010DC6B9CDC782935545229E016362444A6C3606E2F39C30FA30AE`.
   - Used as an independent semantic cross-check. Whenever a cached row disagrees with the pinned Riot/CommunityDragon revision, the pinned data wins.
5. Riot patch 13.3 notes: <https://www.leagueoflegends.com/en-gb/news/game-updates/patch-13-3-notes/>
   - Authoritative source for the present 5% self-heal model, the mana reductions, and E's 70% AP total ratio.
   - It raised Q and W to 70% and 90% AP respectively; these were superseded one patch later.
6. Riot patch 13.4 notes: <https://www.leagueoflegends.com/en-us/news/game-updates/patch-13-4-notes/>
   - Authoritative source for the current 7% ally heal, Q 80% AP and W 100% AP values.
   - The controller uses explicit current formulas because several local SDK rows still reflect an older ratio.
7. Riot champion page: <https://www.leagueoflegends.com/en-us/champions/alistar/>
   - Stable first-party semantic description of Triumphant Roar, Pulverize, Headbutt, Trample and Unbreakable Will.
8. Current mechanics page: <https://leagueoflegends.fandom.com/wiki/Alistar/LoL>
   - Cross-checks Q buffering during W, W's radius-adjusted speed, 700 normal displacement, 200 W-Q displacement, original-cast-origin knockback direction, wall traversal/pinning, E pulse/stun rules and R behavior.
   - Numerical implementation still uses the pinned 16.14 bin rather than trusting rendered wiki values alone.
9. Current Season 26 combo video catalogue: <https://mobalytics.gg/lol/champions/alistar/combos>
   - Video demonstrations were reviewed for W-Q-E-AA-EAA, W-Q-Flash, Q-Flash-W, Flash-W-Q and Q-walk-behind-W families.
   - The controller does not convert those demonstrations into an unconditional key sequence; each family has displacement, follow-up, safety and player-intent gates.
10. Season 16 Alistar combo-guide publication: <https://www.reddit.com/r/alistarmains/comments/1qvsql3/alistar_combo_guide_season_16/>
    - Current community cross-check that practical ranked combo selection, rather than merely executing W-Q, remains the useful teaching boundary.
11. Challenger Alicopter guide publication: <https://www.reddit.com/r/alistarmains/comments/fqsrgd/perma_roam_champion_alicopters_ultimate_guide_to/>
    - Used for the one-way-engage, roam-angle and team-follow-up mindset. Historical build/rune advice is not encoded.
12. Current high-elo/OTP index: <https://www.onetricks.gg/champions/streamers/Alistar>
    - Confirms active Alistar specialist/pro sources including Alicopter and IgNar. This is used to select gameplay material, not as a numerical mechanics authority.
13. Challenger support video guide by ShoDesu: <https://www.mobafire.com/league-of-legends/build/13-22-challenger-alistar-guide-video-included-626671>
    - Cross-checks the champion's two real jobs: reliable engage when the team can convert, and displacement peel when engaging would abandon the carry.
    - Its stale item and ability prose is deliberately not copied into runtime logic.
14. Focused community interaction checks: <https://www.reddit.com/r/alistarmains/comments/f61oue/>, <https://www.reddit.com/r/alistarmains/comments/d9ttvh/>, and <https://www.reddit.com/r/supportlol/comments/mjxwpd/>
    - Cross-check Q-Flash before W for a less reactable insec, W-Flash-Q relocation, E-before/after engage trade-offs, wall-pin Q delay and Q-walk-behind-W.
15. Local plugin-system audit:
    - KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, ziblldev9898 and older C# ports were searched for Alistar champion logic.
    - No local Alistar champion controller exists. Matches are evade/windup records, damage-reduction checks, target priorities and Fiora reaction data only.
    - `AIAlistar` is additive and is dispatched only after the ten protected legacy KuroAIO champions.
16. Local SDK audit:
    - Evade records confirm `Pulverize` as a 250 ms self-following area and identify current spell aliases.
    - Damage and spell databases are useful for runtime plumbing but are not treated as current numerical authority. Q/W/E damage in this controller comes from the pinned 16.14/Riot formulas.

## Current mechanical model

### Triumphant Roar is automatic state, not a fifth spell

- `AlistarPassiveStacks` is tracked to its seven-stack threshold. Champion/epic takedown events can complete the stack bar immediately, while nearby deaths and Alistar crowd control advance it through game-owned logic.
- The pinned bin defines a three-second passive cooldown, a 5% Alistar maximum-health self heal and a 1.4 ally multiplier, producing the current 7% ally heal.
- At six observed stacks, the controller may use safe champion CC to trigger a heal for a genuinely low protected ally. It never fabricates a passive cast because Triumphant Roar is automatic.
- Passive heal setup is subordinate to peel, interrupts and survival. A low-value heal trigger cannot spend W in a direction that worsens carry safety.

### Pulverize is an area control resource

- The controller uses the 375 gameplay effect radius from the bin, not the 365 display radius, and adds the victim's live bounding radius when testing contact.
- Q has a 250 ms normal cast and a one-second knock-up. During a supported W engage it is issued in the real Headbutt event window so game-side buffering shortens the vulnerable gap.
- Close divers are Q'd before W when this holds them beside the protected carry long enough to choose the correct displacement direction.
- Q is reserved for channel interruption, anti-gapclose and multi-target disruption when those uses are more valuable than a routine engage.
- A wall-pinned target is not immediately Q'd by habit. The chain waits toward the end of Headbutt's disable, unless live movement shows the target escaping sooner.

### Headbutt has five distinct purposes

- Headbutt target range is 650. Its exposed travel distance subtracts Alistar's and the target's gameplay radii before dividing by base speed 1200. At maximum range against a 65-radius target this produces about 0.421 seconds of travel in the deterministic model.
- A normal W displaces 700 units. A Q buffered during W reduces that displacement to 200. This difference is strategic, not cosmetic.
- The controller labels each W as one of five purposes: buffered engage, peel, insec, wall pin/interrupt, or escape. Only the first normally wants immediate Q.
- Peel W must increase separation between the diver and the protected ally. A W that knocks a threat closer to the carry is rejected even if it deals damage.
- Insec W must improve displacement toward an allied centroid or allied turret. The controller can Q first, expose the exact walk-behind coach point, then spend full W only after the player creates the angle.
- Wall W traces the full 700-unit endpoint through the real NavMesh, identifies the first collision and measures terrain thickness. Thin/passable walls are not falsely scored as a pin; thick impassable terrain creates a chain-CC branch.
- Knockback direction is built from Alistar's original W cast position to the target contact point, matching the live mechanic rather than recalculating from a later dash position.
- Escape W searches enemy champions, minions and monsters aligned with the player's flee cursor. It rejects endpoints under an enemy turret or inside a ready anti-dash hazard.
- Poppy W, Taliyah E and Cassiopeia W danger checks are shared champion-neutral helpers. Alistar-specific grounded, displacement-immunity and wall decisions remain local.

### W-Q is conditional, never the entire champion

- Standard W-Q requires cursor consent, a target that will accept W/Q, enough mana, safe destination density and an ally able to follow. A solo branch is accepted only when conservative current damage is lethal.
- The W event stores cast origin, target, radius-adjusted expected contact and a short sequence expiry. Q is issued immediately when buffering is desired; it is not delayed until a generic update after manual-input arbitration.
- Manual W is observed and classified. Peel gain, wall-pin value and insec value can deliberately suppress Q; a supported neutral engage can receive the Q buffer.
- Spell shields, spell immunity, parry, untargetability and narrow displacement-immune casts prevent a wasteful engage. The common shield registry lives in `AIControllerHelpers.h`; Alistar keeps only its displacement-specific exclusions.
- Full W is preserved during ordinary Q-E harass because a support without Headbutt cannot disengage the return trade.

### Trample is a target-selection and attack-timing problem

- E lasts five seconds, pulses on the live 0.5-second cadence, has a 350 effect radius, becomes ghosted and caps at five champion-contact stacks.
- `AlistarE` and `AlistarEAttack` buff events are authoritative. A deterministic pulse fallback repairs missed/zero-count events but never advances stacks while no champion remains in contact.
- The empowered basic attack gains 50 range, stuns for one second and deals `20 + 15 × (level - 1)` raw damage: 20 at level 1 through 275 at level 18.
- At four stacks the controller predicts attack impact versus the next pulse. It begins the AA early only when pulse five will occur before impact and the chosen champion will remain in E range.
- Minion attacks and attacks on the wrong champion are blocked while the empowered stun is being preserved. Force-target ownership is released immediately after consumption or expiry.
- E's stun victim is selected independently from the initial W-Q target: interrupt target first, then carry diver, then high-value enemy carry. This allows one engage to control two different champions.
- E begins only with stable champion contact or a wall-contact branch. It is not a generic “press E whenever combo mode is held” action.

### Unbreakable Will has cleanse and tank economies

- R costs 100 mana, lasts seven seconds, and reduces physical/magic damage by 55/65/75%. True damage is added after the reduction and is never reported as mitigated.
- The 16.14 bin exposes both `cannotBeSuppressed=true` and `canCastWhileDisabled=true`. The controller therefore handles critical CC directly from buff callbacks because the engine's ordinary combat update may be suspended while disabled.
- Suppression can always justify R when enabled. Charm, taunt, fear/flee, sleep and polymorph require meaningful danger; long stun/root/silence require danger or a critical sequence. A trivial slow does not consume the ultimate.
- R does not cancel ordinary airborne motion, so airborne alone is rejected. Lethal burst or observed turret danger can still justify casting while airborne for the mitigation window.
- Tank R is delayed until real incoming burst, multiple attackers below a configured health gate, or observed turret aggro. Dive logic records turret attacks rather than pre-casting R solely because an engage endpoint is under a tower.
- The live `FerociousHowl` buff owns the seven-second active window. Repeated controller decisions cannot double-spend or pretend the reduction persists after expiry.

## Posture and decision state machine

The controller chooses one posture every combat tick; posture changes the meaning of the same spell:

| Posture | Primary obligation | Key veto |
|---|---|---|
| Peel | Keep the protected carry alive | Never W a diver closer to the carry |
| Engage | Start a convertible W-Q or close Q | No ally follow-up, cursor disagreement, unsafe numbers |
| Insec | Create full-displacement team/turret gain | No valid behind-target geometry or displacement gain |
| Disrupt | Interrupt/channel break or multi-target Q | Do not spend Q/W on lower-value damage |
| Dive | Layer CC, then tank observed return damage | No R, no follow-up, or premature R before aggro |
| Escape | Q pursuer, W an aligned hostile unit, retain cleanse | Turret/dash-hazard destination or wrong cursor direction |
| Neutral | Preserve cooldowns and AA ownership | No meaningful tactical opportunity |

Protected-ally selection combines carry value, health, distance and nearby threat. It is not simply the nearest ally. A targeted hostile cast or gapclose can immediately override a planned engage and switch the controller to peel.

## Combo-family translation

1. `W-Q-E-AA-EAA`: allowed as the standard supported engage; the ordinary AA is omitted when it would jeopardize E contact or stun ownership.
2. `W-Q-Flash`: the controller observes and continues after player Flash relocation but never casts Flash.
3. `Q-Flash-W`: a player-created Q-Flash is observed, then the controller recomputes the full W insec angle from the new position.
4. `Flash-W-Q`: player Flash remains authoritative; a subsequent manual W enters the same buffer classifier.
5. `Q -> walk behind -> W`: the controller draws the behind-target point and waits for real geometric gain before full W.
6. `W into wall -> E -> delayed Q -> EAA`: wall thickness and contact are validated, E starts at contact, and Q layers near the end of W disable.
7. `Q-E trade -> preserve W`: short harass keeps Headbutt for disengage unless full W creates a large safe displacement toward allied control.
8. `Q pursuer -> W minion/monster`: flee branch chooses an aligned hostile unit and applies destination safety independently of combat target selection.

Fixed key sequences are deliberately absent. The same W-Q input can be correct engage, catastrophic anti-peel, wasted wall displacement or a missed insec depending on origin, target and team geometry.

## Shared helper boundary and duplicate audit

The duplicate pass now covers Aatrox, Ahri, Akali, Akshan and Alistar:

- `AIGeometry.h` owns normalized direction, rotation, cross products and point-to-segment projection.
- `AIControllerHelpers.h` owns local-player/missile ownership, runtime-name checks, case-insensitive name equality, spell rank/cost, prediction, network-id lookup, hostile-unit validation, Epic-monster classification, nearest-enemy lookup, terrain proximity, common spell shields/immunity, common anti-dash hazards, cast throttling, cast-delay/buff-time normalization, enemy-cast analysis, neutral gapcloser capture and the point-click-lockdown registry.
- Ahri's duplicate Sivir/Nocturne/Morgana/Banshee/Edge spell-shield list now calls the shared registry while retaining Ahri-specific parry/untargetable/no-death states.
- Alistar uses the same rank, nearest-enemy, spell-shield, terrain and dash-hazard primitives. Its W displacement purposes, ally selection, NavMesh wall thickness, E pulse/AA timing, passive setup and R economy remain champion-specific.
- Identically named routines such as `TryCombo`, `TryFlee`, `TryHarass` and `OnUpdate` are orchestration hooks, not duplicate behavior. Extracting their bodies would erase the onechamp state machines the task requires.

## Player-cooperation contract

- The player's selected target and cursor direction are treated as intent. The controller will not force an engage opposite the cursor merely because W is ready.
- The controller never issues movement or Flash. It draws the insec/knockback geometry so the player owns the positional commitment.
- Manual W, Q, E and R casts update the same state machine. Manual W is classified before the short Q-buffer window closes.
- Valuable AA windups are preserved except when a four/five-stack E stun must be protected from a minion or wrong champion.
- The controller can force the orbwalker target only for the chosen empowered-E champion and clears that ownership immediately after the attack or expiry.
- Ally follow-up and protected-carry safety are first-class gates. “Combo key held” is not permission to abandon a carry or start a numerically losing one-way engage.
- Lane spells are never cast solely for farm. Jungle use is explicitly opt-in so support resources and displacement cooldowns are not consumed unexpectedly.

## Acceptance scenarios

The controller publishes 78 Alistar-specific scenarios. Core acceptance gates are:

1. Every tick selects peel, engage, insec, disrupt, dive, escape or neutral posture.
2. Protected carry and peel threat are value-scored; carry danger preempts engage.
3. W travel uses live radii and full versus buffered displacement remains distinct.
4. Q is buffered only for a supported standard W engage, including manual-W event timing.
5. Full W remains available for peel, wall pin, insec, interrupt and escape.
6. Wall plans use first collision and terrain thickness, then layer E/Q instead of collapsing into W-Q.
7. Insec requires real gain toward allies/turret and leaves movement/Flash to the player.
8. E tracks live buffs plus pulse fallback, primes the four-stack AA and protects the chosen stun target.
9. Passive setup can trigger a six-stack low-ally heal without pretending the passive is castable.
10. R cleanses critical CC from buff callbacks, delays tank usage to real danger and excludes true damage from mitigation.
11. Spell shields, displacement immunity, anti-dash zones, turrets, enemy density, mana and ally follow-up all veto unsafe branches.
12. Harass, flee, interrupt, gapcloser, farm and manual-cast recovery remain separate state paths.
13. Standalone mechanics regressions and the full Release DLL build pass after catalog integration.

## Verification completed

- `tests/alistar_geometry_test.cpp` compiles independently with MSVC C++17.
- Tests cover radius-adjusted W travel/speed, buffered contact timing, 700 versus 200 displacement, wall stopping, insec/peel gain, Q radius and target radius, E pulse/four-stack AA timing, level-scaled empowered damage, R 55/65/75 mitigation, true-damage exclusion and ally-centroid averaging.
- Result: `ALL ALISTAR GEOMETRY TESTS PASSED`.
- Aatrox, Ahri, Akali and Akshan geometry regressions also pass after the shared-helper extraction.
- Full `Release|x64` build produces `bin/Release/NightSharp.dll`.
- The only build diagnostics are the workspace's pre-existing `LNK4020` warnings for `obj/release/vc145.pdb`; no controller compile or link error is present.

## Known limits requiring live/replay telemetry

- Runtime names and values come from pinned 16.14 data. A live 26.14 replay should confirm buff-event order across skins, reconnects and loading into already-active E/R state.
- Exact W contact can change when either unit moves or changes collision radius after cast. The controller stores the original cast origin and recomputes live target state, but prediction cannot guarantee future movement.
- NavMesh wall thickness is deterministic for loaded terrain. Player-created terrain and very thin diagonal corners need replay calibration before expanding automatic wall-pin tolerances.
- Displacement-immunity runtime buffs are intentionally a narrow list. Each additional champion interaction should be proven in live telemetry rather than inferred from a spell name.
- E buff count may be unavailable on some event paths. The fallback is conservative and contact-gated, but live `AlistarEAttack` timing remains the preferred authority.
- Incoming-damage telemetry cannot perfectly classify future true damage. The controller never reduces already identified true damage and uses conservative health gates for unknown future damage.
- Q-Flash, W-Flash-Q, Hexflash angles and walk-behind positioning remain player-owned. The controller can observe/continue and draw geometry, not guarantee a summoner-spell input it does not control.
- No static model can know whether an ally will actually follow after the cast. The controller uses distance, state and local numerical advantage as evidence and rejects weak evidence instead of assuming coordination.
