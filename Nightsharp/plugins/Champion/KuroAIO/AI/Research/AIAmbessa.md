# AIAmbessa research dossier

Research date: 2026-07-17  
Target game revision: League of Legends 26.14 / CommunityDragon 16.14  
Controller: `Controllers/AIAmbessaController.h`  
Profile: `Profiles/AIAmbessa.h`  
Pure mechanics: `Controllers/AIAmbessaGeometry.h`  
Shared primitives: `AIGeometry.h`, `AIControllerHelpers.h`

## Source ledger

1. Riot release baseline: <https://www.leagueoflegends.com/en-au/news/game-updates/league-of-legends-patch-26-14-notes/>
   - Patch 26.14 is the pinned release baseline for this controller.
   - Ambessa has no 26.14 champion entry, so later authoritative changes through 26.10 plus pinned 16.14 game data define the current kit.
2. Riot Ambessa ability rundown: <https://www.leagueoflegends.com/en-us/news/game-updates/ambessa-abilities-rundown/>
   - First-party semantic source for Drakehound's Step, the two Q casts, Repudiation, Lacerate and Public Execution.
   - It is used to establish intent, not to override later numerical patch changes.
3. Pinned CommunityDragon champion record: <https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/799.json>
   - SHA-256: `B46537678D49C162A5E397963F0907AB999145A8AD9CA7E0068439F16ADB849A`.
   - Ambessa's 16.14 record exists under `global/default`; the usual `global/en_us` endpoint is absent for this revision.
   - Confirms champion id 799, energy model, public spell identities, cooldowns and display ranges.
4. Pinned CommunityDragon game-bin record: <https://raw.communitydragon.org/16.14/game/data/characters/ambessa/ambessa.bin.json>
   - SHA-256: `2EEF7B919DE358161A4DD3CCDCDEB5D21E75E09749E22740D4C2EB0C13E49091`.
   - Directly inspected runtime identities include `AmbessaPassive`, `AmbessaPassiveAttackEmpower`, `AmbessaPassiveDash`, `AmbessaQ`, `AmbessaQ1`, `AmbessaQ2`, `AmbessaQEmpowerReady`, `AmbessaW`, `AmbessaWShield`, `AmbessaE`, `AmbessaESlow`, `AmbessaR`, `AmbessaRBuffSuppressing`, and `AmbessaRSuppressionDebuff`.
   - Direct values include passive dash time 0.30, minimum/maximum travel 175/350, normal forgiveness 0.275, W forgiveness 0.25, three attack stacks for four seconds, +75 attack range, +50% attack speed and 40/55/70 energy refund.
   - Q client targeters expose a 385/375 semicircle indicator and a 40-half-width line targeter. R exposes a 65-half-width line targeter. The controller combines these records with the effect geometry below rather than trusting the generic SDK `mLineWidth=15` field.
5. Meraki Analytics Ambessa record: <https://cdn.merakianalytics.com/riot/lol/resources/latest/en-US/champions/Ambessa.json>
   - Local research copy SHA-256: `D2AA92B1182F6109D7944340C6E3D4CC363ECD45B7FB442DCCDE685987D34C52`.
   - Useful for detailed lockout/interactions and Q geometry: Q1 is a 180-degree sweep with body/edge/total radii represented as 275/135/400; Q2 width is 40; W and E radius are 325.
   - Its cached damage and R fields lag recent Riot patches. Pinned Riot/CommunityDragon values always win conflicts.
6. Riot patch 14.23: <https://www.leagueoflegends.com/en-au/news/game-updates/patch-14-23-notes/>
   - Q2 recast window increased to four seconds.
   - Passive Step no longer passes player-created terrain and R clears stale movement orders after its lockout.
7. Riot patch 14.24: <https://www.leagueoflegends.com/en-gb/news/game-updates/patch-14-24-notes/>
   - Authoritative W shield duration/value baseline and historical E adjustments.
   - Confirms why launch-era shield and E rows cannot be copied from old plugins.
8. Riot patch 25.24: <https://www.leagueoflegends.com/en-us/news/game-updates/patch-25-24-notes/>
   - Passive empowered-attack ratio is now 25% bonus AD.
   - E uses flat 50% bonus AD on each hit at every rank.
9. Riot patch 26.9: <https://www.leagueoflegends.com/en-gb/news/game-updates/league-of-legends-patch-26-9-notes/>
   - Public Execution cast time increased from 0.55 to 0.70 seconds. All R candidate prediction uses 0.70.
10. Riot patch 26.10: <https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-10-notes/>
    - Q full-damage base max-health ratio is now 4/4.5/5/5.5/6%.
    - Q monster flat bonus is now 75.
    - R ability healing is now 15/17.5/20%, while monster healing effectiveness is 25%.
11. Current detailed mechanics page: <https://lol.fandom.com/wiki/Ambessa>
    - Used for buffer cancellation, last-input ownership, W brace/dash timing, E double-hit conditions, R suppression failure and farthest-target semantics.
    - Several rendered numerical rows remain stale (passive ratio, E, R cast/heal). Those numbers are deliberately not used.
12. Current patch-history cross-check: <https://www.riftpatchnotes.com/lol/champion/ambessa>
    - Used to audit that no post-26.10 Ambessa number silently superseded the pinned formula before 26.14.
13. Heywil, `ULTIMATE AMBESSA GUIDE FOR 2026`: <https://www.youtube.com/watch?v=1b4aDd4lmDs>
    - Current Season 16 specialist guide by the `@I_Main_Ambessa` channel. The dedicated combo chapter runs 19:20-24:11; laning starts at 24:11 and mid/late at 27:39.
    - The guide's identity is controlled aggression, sideline pressure, exact energy/AA pacing and adapting dash direction rather than mechanically dashing after every cast.
14. Heywil, `AMBESSA ADVANCED COMBO GUIDE 2025`: <https://www.youtube.com/watch?v=JpZsxMTGtqQ>
    - Reviewed for full weave, fast burst, chase and R follow-up families. The controller translates them into state branches, never a fixed macro.
15. Heywil, `AMBESSA ANIMATION CANCEL GUIDE 2025`: <https://www.youtube.com/watch?v=k6t1VCXl8ZI>
    - Source for replacing the pending passive movement buffer with a fast ability input. The implementation's quick-QE branch deliberately marks Q's Step as no-dash and gives E a fresh player-owned Step window.
16. Coach Chippys detailed Challenger guide: <https://www.youtube.com/watch?v=Qwjxigtf6os>
    - Cross-checks tempo, spacing, ability preservation, target access and extended-fight energy economy from a high-elo coaching perspective.
17. Current Chinese high-elo gameplay review: <https://www.youtube.com/watch?v=Pigh61UHMR8>
    - Used to cross-check that strong Ambessa play frequently holds the passive dash to preserve spacing or avoid predictable denial rather than maximizing dash count.
18. Current high-elo/OTP index: <https://www.onetricks.gg/champions/ranking/Ambessa>
    - Used to select active specialist material and validate that the gameplay model is not based only on launch-week demonstrations.
19. Current expert/video guides: <https://mobalytics.gg/lol/champions/ambessa/expert-videoguide>, <https://www.skill-capped.com/lol/guides/builds/ambessa/jungle>, and <https://lolmatchups.gg/champion-guides/Ambessa>
    - Cross-check short-trade, all-in, jungle and matchup decision families. Build/rune prescriptions are not encoded into spell logic.
20. Current combo catalogues: <https://lolstats.gg/en/champions/ambessa/guide> and <https://www.youtube.com/watch?v=OdttUIfSm9A>
    - Cross-check Q1-edge to Step/AA/E/Q2, E-first catch, W-counter and R-chain demonstrations.
21. Specialist/community interaction checks: <https://www.reddit.com/r/ambessamains/comments/1np4l9o/>, <https://www.reddit.com/r/ambessamains/comments/1srxvhp/>, <https://www.reddit.com/r/ambessamains/comments/1sy8z0s/>, <https://www.reddit.com/r/ambessamains/comments/1ubznxq/>, and <https://www.reddit.com/r/ambessamains/comments/1sa72qn/>
    - Cross-check reactive matchup trading, AA-cancel discussion, fast short trades, high-elo no-dash choices and burst sequencing.
    - Community claims are used as hypotheses only when they agree with game data or reproducible video behavior.
22. Local plugin-system audit:
    - KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, ziblldev9898 and older C# ports were searched for Ambessa champion logic.
    - No local Ambessa champion controller exists. Matches are evade records, gapcloser data, target priority, assets and damage-passive support.
    - `AIAmbessa` is additive and dispatches only after the ten protected legacy KuroAIO champions.
23. Local SDK audit:
    - `SDK/Data/Database.h` contains Q1/R evade rows; `GapcloserData.h` contains E/R records; `DamagePassives.h` recognizes `AmbessaPassiveAttackEmpower`.
    - These rows are useful runtime plumbing, not the numerical authority for the champion controller.

## Current mechanical model

### Drakehound's Step is an input protocol, not an automatic dash button

- Every ability lockout creates a short opportunity for the player's most recent attack or movement order to become Step. The controller never sends movement, attack-move, Hold, Stop or Flash input.
- The pure model exposes 175 minimum and 350 maximum travel, 0.30 timing, and speed `770/830/890/950 + movement speed` at the level 6/11/16 breakpoints. The live layer clips the planned endpoint at the first NavMesh wall.
- A dash is counted only after `AmbessaPassiveDash`, `IsDashing`, or meaningful position displacement inside the correct pending window. A spell cast alone is insufficient evidence.
- Grounded/immobilized state cancels the buffer. Poppy W, Taliyah E and Cassiopeia W are treated as ready endpoint hazards through the shared helper, while the choice to bait them with no-dash remains Ambessa-specific.
- No-dash is a first-class branch. It preserves Q1 edge range, avoids moving into denial, prevents a predictable chase line and enables fast ability-input cancels.
- The 2025 Hold/Stop behavior is represented as coaching only. The controller can mark `NO-DASH`, but pressing the cancel key for the player would violate cooperation and could cancel a manual attack order.
- Each cast grants a Medarda Maxim stack after its lockout, up to three for four seconds. Live buff count is authoritative; a cast-event fallback repairs same-batch event loss.
- Empowered attacks gain range/attack speed, have an uncancellable windup, add `5-30 + 25% bonus AD` and refund 40/55/70 energy at levels 1/7/13.
- Energy is planned sequentially. At level 6, three 70-energy spells from 130 energy are impossible without woven refunds; at level 13 two successful empowered autos can turn the same chain into a legal sequence.
- The controller waits for a passive AA when the refund is required, but skips it when Q2 is expiring or a fast QE/Q2 branch is conservatively lethal. It does not cancel an empowered windup after committing.

### Q1 and Q2 solve different geometry problems

- Cunning Sweep is a 180-degree semicircle. The model uses a 275 body region and a 275-400 blade edge, with target gameplay radius included in hit testing.
- Q1 body damage is half of the entire package. Edge logic is therefore not a cosmetic score: it changes flat, bonus-AD and max-health components together.
- A normal trade holds Q1 when only the body can hit. Close all-in/lethal branches may accept body damage because waiting for an impossible edge would be worse.
- Q1 predicts through the 0.225-second lockout and aims using Ambessa's live facing model. The cast creates only a player-owned Step plan; it does not manufacture the movement command.
- Hitting any enemy unlocks Q2 for four seconds. Runtime `AmbessaQ2` and `AmbessaQEmpowerReady` own recast state; local timing is fallback/expiry protection.
- Sundering Slam is a 650 line with 40 half-width. The first enemy unit takes full damage and all later units take half.
- The Q2 solver enumerates heroes, lane minions and jungle monsters, then tests direct and offset aim directions. A cast is preferred only when the intended champion is the actual first collision.
- A blocked Q2 may still fire if its half damage is lethal or the four-second recast is about to disappear. An attack whose predicted impact would lose Q2 can be withheld.
- Current full-damage formulas are:
  - Q1 edge: `40/60/80/100/120 + 60% bAD + (4/4.5/5/5.5/6% + 0.03% per bAD point) target max HP`.
  - Q2 first: `50/75/100/125/150 + 90% bAD + (4/4.5/5/5.5/6% + 0.04% per bAD point) target max HP`.
  - Q1 body/Q2 later target: exactly half of their corresponding complete package.
- Monsters add 75 flat damage and cap the max-health component at 100-300 by champion level.

### Repudiation is a timed counter window

- W costs 70 energy, has 325 radius, braces for 0.5 seconds, shields for 1.5 seconds and uses a 0.25-second passive forgiveness window.
- The shield is `50-320 by level + 150% bAD`. Slam damage is `50/75/100/125/150 + 50% bAD` and becomes 150% of that value only after blocking champion, large-monster or turret damage before the slam.
- Enemy cast analysis identifies targeted attacks/spells and line crossings, estimates live post-mitigation damage where possible, normalizes cast delay and stores an impact window.
- W is cast when incoming damage crosses configured health/shield thresholds, on real turret aggro, on a committed gapcloser or on an enabled epic-monster counter. Ordinary minion chip does not trigger the empowered-damage model.
- A Step during W can relocate the slam. The endpoint remains player-owned; the controller evaluates whether that endpoint also places the target inside 325.
- W is preserved during an ordinary Q trade unless counter-damage is committed. This is more valuable than treating shield and slam as routine combo filler.

### Lacerate owns two states, not guaranteed double damage

- E costs 70 energy, has 325 radius, locks Ambessa for 0.225 seconds, deals `40/60/80/100/120 + 50% bAD`, and applies a 99% slow decaying over one second.
- E always performs its first strike. It performs the second strike only when a Step is initiated from E; that strike occurs at the actual dash endpoint.
- `ESecondStrikeExpected` can become true from a path that agrees with the planned endpoint, but final live state is upgraded by `AmbessaPassiveDash`/movement observation. Without either, combo damage remains one hit.
- E-first is a catch/kite branch when Q1 edge cannot be created. E during Q's forgiveness window is a separate fast no-dash QE branch.
- A gapcloser endpoint inside E radius is slowed before Ambessa's player-directed retreat. E is not cast simply because combo mode is held while a target is out of radius.

### Public Execution must simulate the farthest target

- R has 1250 range, 65 half-width and the current 0.70-second cast time. It selects the farthest enemy champion inside the final line, not the nearest champion and not necessarily the target selected by the player.
- Every R plan predicts all visible enemy champions 0.70 seconds ahead. It tests several small angular offsets, computes the farthest hit for each, and accepts a normal cast only if the intended target is the actual selected target.
- A rear champion can steal the cast. This is exposed visually instead of silently ulting the wrong enemy.
- Ambessa blinks behind the seized target. Safety is evaluated at that predicted behind-target position, including terrain, turret range, enemy density and allied follow-up.
- Spell shield, spell immunity, Fiora parry and known suppression/displacement-immunity states veto proactive R. If suppression fails, the attachment, damage and stun fail even though Ambessa may still blink; the controller therefore cannot value only raw R damage.
- R branches are execute, carry isolation, long catch, channel interrupt and displacement-immune CC dodge. Execute requires R damage to be uniquely necessary; isolation requires target value and follow-up/solo lethality; catch requires cursor consent.
- A dangerous predicted hard CC can justify using the 0.70 cast/lockout as displacement immunity, but R cannot be started once Ambessa is already rooted/grounded.
- Active damage is `150/250/350 + 80% bAD`. Passive armor penetration is handled by the game's damage calculator. Ability healing is 15/17.5/20% of post-mitigation damage plus half life-steal contribution, reduced to 25% effectiveness against minions and monsters.

## Posture and decision state machine

| Posture | Primary obligation | Key veto |
|---|---|---|
| Space | Hold Q1 blade-edge distance and observe player's intended Step | Do not dash out of the sweetspot merely because a spell was cast |
| ShortTrade | Q1 edge, energy-aware AA/E, Q2, then leave | Do not spend W without real counter-damage |
| AllIn | Convert current damage/energy into a full chain | Do not begin a sequence that stalls before the next 70-energy cast |
| Chase | Use Q2/E/Step and cursor-approved R access | Do not Step into anti-dash terrain or excess enemy density |
| Kite | E slow, backward/cursor Step and reactive W | Do not force a forward auto when survival requires spacing |
| Isolate | Find an R angle whose farthest champion is the carry | Reject a rear-target steal or unsupported landing |
| Escape | E contact, shield real damage, expose a safe Step | Never issue movement or Hold/Stop for the player |
| Jungle | Weave refunds, use current monster Q values and counter epic hits | Do not drain the energy reserve or invent E2 |

The same spell changes meaning by posture. For example, E is contact damage in all-in, a slow-and-retreat tool in kite, a real-double-hit route in jungle and a gapcloser reaction in automatic mode. These branches are intentionally not extracted into a generic `TryE` policy.

## Combo-family translation

1. Blade-edge trade: `Q1 edge -> player Step/no-Step -> empowered AA -> E -> player Step -> empowered AA -> Q2`.
   - Every AA is conditional on energy need, target range and Q2 expiry.
   - E2 is valued only after an observed Step.
2. Fast burst: `Q1 -> E input cancel -> Q2`, optionally omitting AA.
   - Used only when target health/incoming danger makes speed more valuable than a refund.
   - The E input replaces Q's queued movement buffer; E then owns a fresh Step decision.
3. Counter-trade: `W into committed damage -> optional player Step -> AA/Q/E/Q2`.
   - W is timed to shield and empower its slam, not pre-cast as generic damage.
4. E-first catch: `E slow -> player Step -> Q1/Q2 routing`.
   - Used when no stable Q1 edge exists or a gapcloser enters contact range.
5. Full weave: `W/Q -> AA -> Q/E -> AA -> Q2`, with sequence legality recomputed from current energy and level-specific refund.
6. R isolate: `multi-angle R validation -> behind-target landing -> player Hold/Stop or Step choice -> AA/Q/E/Q2`.
   - The controller never presses Hold/Stop. It only prevents an automatic follow-up from assuming a dash that the player has not chosen.
7. R interrupt/CC dodge: cast only when the exact farthest-target line is legal and the immunity window has value beyond raw damage.
8. Disengage: `E slow -> player-directed Step away`; W is added only for real incoming damage, and Q is spent for a Step only with an energy reserve.
9. Jungle: `Q on monster -> player kite Step -> passive AA -> E -> observed second strike -> AA -> Q2`, with W held for an epic hit.

## Shared helper boundary and duplicate audit

- Reused champion-neutral helpers: local-player event identity, current resource, mobility-lock state, AA range/attack-event capture, case-insensitive runtime/buff matching, spell rank/toggles, prediction, target lookup, nearest pursuer, spell-shield registry, interrupt/gapcloser capture, cast-delay normalization, controller-cast ownership, generic enemy-cast analysis, terrain proximity and anti-dash hazard queries.
- Reused pure geometry primitives: 2D direction, rotation and point-to-segment projection.
- Kept Ambessa-local because semantics are kit-specific: Step/no-Step choice, 175/350 endpoint protocol, Q1 region classification, Q2 first-unit ordering, sequential energy refunds, W empowerment economics, E2 observed state, R farthest-target selection and behind-target landing.
- The post-Ambessa audit extracted duplicated resource/mobility/AA-event/interrupt/cast-ownership plumbing. `SharedHelperAudit.md` records the mechanical result and the callback-adapter exception.
- The tempting common names `TryCombo`, `TryFlee`, `CastQ` and `OnProcessSpell` are orchestration hooks whose decisions differ materially across champions and must not be collapsed.

## Player-cooperation contract

- Selected-target preference and cursor consent preserve player intent.
- The controller casts champion abilities only; it never sends movement, attack-move, Hold, Stop, Flash or summoner-spell input.
- Planned Step endpoints are coaching/validation signals. If the player's current path disagrees, E2 and dash-dependent damage are not guaranteed.
- Manual Q/W/E/R casts are observed and continued. The shared engine's manual-input lock yields before creating another automatic decision.
- Valuable attack windups are preserved. Combat passive stacks are protected from wrong-unit attacks only when a desired champion is in actual enhanced range.
- R visualization identifies both the requested target and the champion the game would truly select as farthest.
- Menus expose risk thresholds and individual branches. Disabling a branch does not route to a generic fallback because the controller owns the full decision loop.

## Acceptance scenarios

`AIAmbessaController.h` publishes 110 auditable scenarios. They cover:

- eight posture transitions;
- player-owned dash/no-dash, terrain, grounding and anti-dash denial;
- passive stack, attack-windup and level-specific energy refund state;
- Q1 edge versus body geometry;
- Q2 first-target blockers and expiry;
- quick QE animation-cancel versus full weave;
- reactive W threat/impact timing and empowerment accounting;
- observed-only E second strike;
- multi-angle R farthest-target, suppression rejection and landing safety;
- channel interruption and R CC-dodge timing;
- jungle monster formula/energy behavior;
- lane clear/last-hit reserves;
- manual input continuation and visual coaching.

The published array is part of the catalog contract and is not documentation-only: the controller reports its exact scenario count at runtime metadata level.

## Verification completed

- `tests/ambessa_geometry_test.cpp` compiles independently under C++17 and passes.
- Regression assertions cover passive breakpoints/endpoints/energy, Q1 classification, Q2 blockers, R farthest selection/landing, all current damage formulas, monster caps, W shield/empower, conditional E2 and R healing.
- Full `Release|x64` MSBuild succeeds and emits `bin/Release/NightSharp.dll` with Ambessa in the six-entry AI catalog.
- Existing Aatrox, Ahri, Akali, Akshan and Alistar geometry tests remain part of the next full-suite gate.
- The only persistent linker diagnostics are the pre-existing `LNK4020` warnings for `obj/release/vc145.pdb`; they are unrelated to Ambessa code.
- `KuroAIO.h` legacy dispatch order remains unchanged. Ambessa is reached only through the additive AI catalog after all ten protected legacy champions.

## Known limits requiring live/replay telemetry

- The SDK has no dedicated player-order event in the champion-controller contract. Step intent is inferred from path end/cursor and then confirmed by dash buff/movement; the controller intentionally stays conservative when those disagree.
- W does not expose a clean standalone “shield absorbed valid source” event in this layer. Incoming cast/impact windows provide a conservative empowerment estimate, while live damage telemetry would make the exact empowered-slam flag stronger.
- R's game-side landing resolver can move Ambessa around collision/terrain. The controller evaluates the documented behind-target point and rejects terrain instead of inventing a guaranteed alternate landing.
- Fog-of-war champions cannot be enumerated as R steal candidates. A cast through unexplored fog remains inherently less certain and should be covered by replay telemetry before loosening safety gates.
- Patch updates after 26.14 require repinning CommunityDragon and reviewing Riot notes before changing formulas. Cached wiki/Meraki rows must never silently overwrite the pinned revision.
