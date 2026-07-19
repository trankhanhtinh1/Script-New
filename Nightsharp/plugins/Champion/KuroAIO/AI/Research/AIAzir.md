# AIAzir research and implementation dossier

Date: 2026-07-18  
Controller: `AI/Controllers/AIAzirController.h`  
Pure model: `AI/Controllers/AIAzirGeometry.h`  
Profile: `AI/Profiles/AIAzir.h`  
Orbwalker extension: `Plugins/Core/OrbwalkerKuro/AzirSoldierSupport.h` and `OrbwalkerAzir.inl`  
Live baseline: League of Legends 26.14 / CommunityDragon PC 16.14

## Sources and revision pin

- [Riot patch 26.6](https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-6-notes/) is the numerical change authority for the current rank-scaled Q AP ratio and W level/AP scaling.
- [Riot patch 26.14](https://www.leagueoflegends.com/en-au/news/game-updates/league-of-legends-patch-26-14-notes/) is the release baseline. It fixes W item/rune interactions and makes R knock jungle monsters back.
- [Riot's Azir champion page](https://www.leagueoflegends.com/en-us/champions/azir/) is the current public kit overview.
- [CommunityDragon champion 268 JSON](https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/268.json), SHA-256 `4A97A961D0F00AE4D6A8FB61AE514D549CB0D2D36AD29B634673F4649A2E3469`.
- [CommunityDragon Azir bin](https://raw.communitydragon.org/16.14/game/data/characters/azir/azir.bin.json), SHA-256 `3088E86E8B8BDBAEBED4D5224A2B05BA772983EED4223DB168932B786CB2570F`.
- [CommunityDragon Sand Soldier bin](https://raw.communitydragon.org/16.14/game/data/characters/azirsoldier/azirsoldier.bin.json), SHA-256 `2B0C27D2D7E76A813F61443E8776B3055C6D9BEDE26C3C931D5486CC30DB83CA`.
- [Shok's Rank-1 Challenger guide](https://www.mobafire.com/league-of-legends/build/26-1-shoks-rank-1-challenger-azir-guide-636495), updated 2026-01-09, supplies the current high-level front-to-back/engage decision model.
- [Shok's Azir video guide](https://www.youtube.com/watch?v=KwSkv0iQ4Js), [Shok's channel](https://www.youtube.com/channel/UCmZO82KD3WDjx7A8QvRDzNA), and [PowerOfEvil's concise pro guide](https://www.youtube.com/watch?v=6mXJ2kIKPwo) were used for execution, spacing and ultimate-use examples. Their historical numbers are not used as live data.
- [The Most In-Depth Azir Guide Ever](https://www.mobafire.com/league-of-legends/build/the-most-in-depth-azir-guide-ever-645628) supplies explicit extended-trade, drift, shuffle, follow-up-soldier, revenant and breakpoint explanations.
- [BodyThoseFools' Season 16 Challenger/10M mastery guide](https://www.mobafire.com/league-of-legends/build/season-16-bodythosefools-challenger-azir-guide-in-depth-10-000-000-mastery-points-beat-faker-614215), updated 2026-03-25, cross-checks matchup-specific E conservation, rear escape soldiers during siege, and side-lane Sun Disc use.
- [Mobalytics current combo catalog](https://mobalytics.gg/lol/champions/azir/combos) cross-checks `W-E-Q-AA`, `E-Q-R`, `E-Q-Flash-R`, `W-AA-E-Q-AA`, `W-AA-Q-AA`, and `R-W-AA-Q-AA-W-AA` input families.
- [LoLStats patch 26.14 Azir guide](https://lolstats.gg/en/champions/azir/guide) cross-checks the current backline-DPS and self-peel identity.
- [OP.GG current skill order](https://op.gg/lol/champions/azir/skills) is used only to verify the current W-Q-E public progression, never as spell-data authority.
- Current specialist discussions include [July 2026 new-player advice](https://www.reddit.com/r/azirmains/comments/1uv5wgg/tips_for_a_new_azir_main/), [July 2026 revenant timing discussion](https://www.reddit.com/r/azirmains/comments/1uxb74j/how_did_i_do_this_so_smoothly_and_how_can_i_do_it/), and the [2026 regularly updated Challenger replay recommendation](https://www.reddit.com/r/azirmains/comments/1qnd4rb/gameplays_to_watch/). Community claims are accepted only when consistent with live data or observable geometry.

## Existing-support preflight

KuroAIO's original champion dispatch supports ten champions: Fiora, Katarina, Kindred, Lucian, Samira, Senna, Syndra, Twisted Fate, Viktor and Yasuo. Azir is not one of them. No `Azir` route, profile or controller existed in KuroAIO before this work, so `AIAzir` does not replace or shadow a supported plugin.

Other local systems were still inspected:

- EzEvade's C# Azir special-spell tracker keeps soldier positions for Q avoidance but has no combat controller.
- The SDK generic auto-attack vocabulary explicitly lists `azirbasicattacksoldier` among special non-player attacks. That is correct for generic champions but meant OrbwalkerKuro could not infer Azir target reach from the player body.
- No other local plugin supplied a complete Azir decision loop that could safely be reused.

## Mandatory OrbwalkerKuro audit

The user required Sand Soldier kiting support before AIAzir. The audit found no Azir-specific branch in OrbwalkerKuro:

1. attack range and target validation always originated at the local champion;
2. the final attack guard rejected a target reachable only by a soldier;
3. last-hit/wait/clear used physical player auto damage;
4. the local-attack detector rejected soldier senders; and
5. soldier attack aliases were intentionally excluded from the SDK's generic auto-attack detector.

The dedicated extension now owns these rules:

- commandable soldier: within 660 of Azir;
- legal primary target: within 375 of a commandable soldier plus the target bounding radius;
- the spear's extra 50 units is collateral reach only and never legalizes the attack command;
- structures, wards and traps cannot be commanded through a soldier;
- first soldier contributes full W damage, every additional soldier 25%;
- direct on-hit effects use 50% effectiveness, while patch-26.14 item/rune fixes remain separate;
- last-hit, wait, lane clear, jungle clear, turret farming, target selection and the final attack guard all use the same soldier-aware query;
- player and soldier attack events are normalized and simultaneous soldier events are deduplicated;
- OrbwalkerKuro still owns attack and movement commands. AIAzir never issues either.

Standalone test `tests/orbwalker_kuro_azir_soldier_test.cpp` covers command tether, primary/collateral reach, forbidden targets, multi-soldier damage, on-hit effectiveness, secondary-line scaling and event aliases.

## Exact live model

### Passive — Shurima's Legacy

- passive cooldown: 90 seconds;
- Sun Disc lifetime: 45 seconds;
- cast range 700 and channel range 850;
- tower raw attack damage: 230, plus 15 per level from level 7, plus 40% AP;
- bonus resists: 30, plus five per level from level 7.

The SDK does not expose a trustworthy selected ruin/click contract to a champion controller. AIAzir therefore tracks likely ruin objects and draws only a macro suggestion when the channel is safe, allies can use the zone and an objective/side-lane/base-defense window exists. It never clicks or channels the passive.

### Q — Conquering Sands

- base damage: 60/80/100/120/140;
- AP ratio: 35/40/45/50/55%;
- mana: 70/80/90/100/110;
- cooldown: 14/12/10/8/6;
- cast time 0.25 seconds, command range 720, display range 740, soldier move speed 1600;
- current data width is represented as a 70-wide path capsule;
- one cast moves all commandable soldiers into a lateral formation roughly 50 beyond the requested point;
- overlapping soldier paths do not multiply Q damage;
- the target is slowed 25% for one second.

`AIAzirGeometry::EvaluateQ` builds a separate old-position-to-new-position capsule for every soldier, but credits each unit once. It scores current attackers, future attackers, primary hit, retreat alignment, wave bodies and whether the only E anchor would be lost.

The controller implements late-Q discipline. After W, it waits for the player's/orbwalker's soldier stab. Q is used when the target begins leaving coverage, when Q creates new coverage, on verified lethal, or on a controlled target with useful future coverage. A stationary target already being stabbed does not trigger cosmetic Q spam.

### W — Arise!

- rank base: 50/65/80/95/110;
- level bonus: +8 per level from 10 through 18, reaching +72;
- AP ratio: 35/42.5/50/57.5/65%;
- mana: 40/35/30/25/20;
- spawn range 525, cast time 0.25, soldier lifetime ten seconds;
- live recharge: 12/10.5/9/7.5/6;
- soldier command tether 660 and primary attack reach 375 plus target radius;
- secondary line starts at 20% and gains eight percentage points per level from 9 through 18.

The W planner knows real ammo, reserves a charge when E needs an escape anchor, evaluates target retreat, cursor agreement, terrain, turret-shortened lifetime and whether an existing anchor already covers escape. Ordinary lane trade is `W-AA-Q-AA`, not `W-Q` before the target has a chance to walk.

### E — Shifting Sands

- damage and shield: 70/110/150/190/230 +60% AP;
- cooldown: 22/20.5/19/17.5/16;
- mana 60, soldier selection range 1100, shield duration 1.5 seconds;
- dash speed is modeled at approximately 1700;
- Azir stops at the first champion body and regains a W charge;
- abilities may be cast during the dash; moving the soldier with Q redirects and extends Azir's path.

The direct dash resolver projects every champion capsule onto the Azir-to-soldier segment and selects the earliest collision. The drift resolver then recomputes first collision over the two-stage E-Q route. Endpoint policy rejects terrain, new turret exposure, ready point-click control, Poppy/Taliyah/Cassiopeia dash hazards, unfavorable enemy count and projectile-intercept walls.

E is not a generic gap close. Automatic offensive use is restricted to verified lethal first collision or a committed collision-refund window with allied follow-up. Defensive E and flee E choose a cursor-agreeing safe soldier. Manual E starts a player-owned window; the controller does not append Q or R.

### R — Emperor's Divide

- damage: 200/400/600 +75% AP;
- cooldown 120/105/90, mana 100, cast time 0.5 seconds;
- six/seven/eight soldiers;
- wall targeter lengths 700/810/930 and depth 190;
- wall duration five seconds;
- patch 26.14 also knocks jungle monsters back.

R geometry is a directional rectangle extending behind and in front of Azir. Every hit preserves lateral offset and receives a projected landing point. Scoring values total hits, priority hits, allied follow-up, pushing the main target toward allies and separating front from back.

## One-trick decision model

### Default identity: DPS and peel

Shok's current guidance is explicit: most teamfights are front-to-back, soldiers hit whoever is in front, Q must not move the formation somewhere the team cannot hold, and R is commonly saved for self/ADC peel. New Azir players over-shuffle. AIAzir follows that hierarchy:

1. maintain soldier coverage and let OrbwalkerKuro/player kite;
2. late-Q only to retain coverage;
3. R a committed diver or high-value multi-hit opportunity;
4. use E only as collision kill/refund or safe escape;
5. consider shuffle last.

### Shuffle gate

Automatic shuffle is off by default. Both automatic and manual starts still require a target, a W charge or live anchor, E/Q/R ready, cursor agreement, allied follow-up, an exit, no new turret/terrain risk and an answer to ready Flash/dash/control. Automatic shuffle additionally loses to front-to-back DPS unless it catches the configured multi-target count.

Manual Shurima shuffle and manual revenant use separate keys. Manual intent may override the front-to-back preference, but not endpoint safety. The controller never casts Flash. The E-Q-R state machine waits until Q's 0.25-second cast lock has elapsed before R; collision or missed timing aborts. After a valid R, W is placed on the projected landing route for the airborne and wall-path autos.

### Player cooperation

- target selection honors the shared selected-target policy;
- cursor agreement is required for offensive Q/W/E/R and every shuffle;
- the player/orbwalker owns every attack and movement command;
- attack callbacks and soldier spell events advance `W-AA-Q-AA` without fabricating an auto;
- manual Q/W/E/R starts a configurable player-owned lock and cannot be auto-completed;
- Flash, attack-move, passive ruin click and passive channel remain player-owned;
- reactive spell logic never changes the player's cursor or movement path.

## Controller architecture

`AIAzirController.h` owns the entire decision loop and publishes named scenarios. Major state families are:

- `WAutoQAuto` and `ExtendedSoldierDps` for late-Q trading;
- `DriftEscape`/`DriftEngage` for direct and buffered E-Q routes;
- `ShurimaShuffle`/`RevenantShuffle` with distinct redirect anchors;
- `CollisionRefund`, `DefensivePeel`, `FarmFormation` and `PlayerLed`;
- explicit phases for soldier spawn, first attack, target exit, Q buffer, R window, follow-up soldier and DPS recovery.

The controller composes `AIControllerHelpers` for event normalization, enemy cast windows, projectile walls, cursor agreement, mobility/point-click hazards, prediction, target lookup and resource checks. Current soldier constants and damage come directly from the shared pure `OrbwalkerKuro::AzirSoldierSupport` helper, avoiding a second copy in AIAzir.

## Verification

- `tests/orbwalker_kuro_azir_soldier_test.cpp`: pass.
- `tests/azir_geometry_test.cpp`: pass; covers live arithmetic, command tether, forbidden targets, Q formation/one-damage semantics/late policy, W reserve, first E collision, drift endpoint, commit policy, rank-scaled R wall/landing/peel/shuffle, passive values and sequence timing.
- Visual Studio project/filter XML: each Azir controller/geometry/profile entry occurs exactly once.
- `NightSharp.sln`, `Release|x64`: pass after catalog integration.

## Runtime telemetry limits

- Object-name/source bridges can differ by game build. Soldier ownership uses source id first and a bounded recent-W spawn fallback; live replay should confirm all aliases.
- The passive ruin object vocabulary is advisory. No unsafe fallback clicks a guessed object.
- E projectile-wall behavior is modeled conservatively from specialist documentation; live replay should retain event evidence for bridge-specific exceptions.
- Revenant timing is intentionally strict and manual-only. It is not represented as a reliable default combo.
- Patch-26.14 on-hit/rune bug fixes affect item/rune proc systems outside the champion controller. Orbwalker base W damage and direct-on-hit effectiveness remain separately modeled.
