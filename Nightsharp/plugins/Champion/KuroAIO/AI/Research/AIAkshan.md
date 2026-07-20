# AIAkshan research dossier

Research date: 2026-07-17  
Target game revision: League of Legends 26.14 / CommunityDragon 16.14  
Controller: `Controllers/AIAkshanController.h`  
Profile: `Profiles/AIAkshan.h`  
Pure geometry: `Controllers/AIAkshanGeometry.h`  
Shared primitives: `AIGeometry.h`, `AIControllerHelpers.h`

## Source ledger

1. Riot release baseline: <https://www.leagueoflegends.com/en-au/news/game-updates/league-of-legends-patch-26-14-notes/>
   - Patch 26.14 was published on 2026-07-14 and is the release baseline used for the build.
2. Riot patch 26.1 notes: <https://www.leagueoflegends.com/en-us/news/game-updates/patch-26-1-notes/>
   - This is the authoritative rebalance that changes Akshan's present damage identity: passive shot-two critical strikes receive 100% bonus critical damage, Q becomes 45/75/105/135/165 plus 70% bonus AD, E becomes 8/16/24/32/40 plus 25% total AD with 50% bonus-crit scaling, and R receives 30% bonus-crit scaling.
   - The controller therefore favors autos and Q over historical E-first burst advice.
3. Pinned CommunityDragon champion record: <https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/166.json>
   - SHA-256: `6d93d5365da660a3b1f9769fcb70df3a975704f04ef651e1d645da6251c09d9c`
   - Confirms current displayed spell identities, rank values, costs and ranges.
4. Pinned CommunityDragon game-bin record: <https://raw.communitydragon.org/16.14/game/data/characters/akshan/akshan.bin.json>
   - Runtime identities inspected directly include `AkshanPassiveAttack`, `AkshanPassiveDebuff`, `AkshanPassiveShield`, `AkshanQMissile`, `AkshanQMissileReturn`, `AkshanW`, `AkshanWHuntMark`, `AkshanWWallPassive`, `AkshanE`, `AkshanE2`, `AkshanE3`, `AkshanEAttack`, `AkshanEOrbitalClockwise`, `AkshanEOrbitalCounterClockwise`, `AkshanR`, `AkshanRAmmo`, `AkshanRCancel`, and `AkshanRMissile`.
   - Q's gameplay reach is modeled from its 750 script range even though the targeting display/input reaches 850. Its missiles are 1500 outbound and 2400 returning.
   - E exposes an 800 hook search, 1200 orbital speed and 350 E3 dismount. R exposes a 2.5-second channel, 2500 acquisition range, 3200 projectile speed and 40 line width.
5. Riot's original ability rundown: <https://www.leagueoflegends.com/en-gb/event/akshan-abilities-rundown/>
   - Used for the stable semantic contract: optional passive second shot versus movement speed, Scoundrel/revive purpose, three-stage E and blocker-sensitive R.
   - Numerical values are taken from the current 26.1/16.14 records, not this historical launch article.
6. Meraki Analytics latest Akshan record: <https://cdn.merakianalytics.com/riot/lol/resources/latest/en-US/champions/Akshan.json>
   - The record's `patchLastChanged` remains 14.22, so its damage numbers are not accepted as current where Riot 26.1 differs.
   - Mechanic notes cross-check that shot two can be redirected or cancelled, deals 100% AD to minions, and prioritizes a visible champion/low-health minion when the original target dies.
   - It also cross-checks W's indefinite duration near terrain/brush, E's cursor-side direction, four-second recent-damage target priority, champion/terrain knockdown, initial/dismount shots, 0.5-second E3 lock, and E's 0.5-second takedown reset.
7. Fighto, Challenger/ex-pro “Infinite Swing” guide: <https://www.youtube.com/watch?v=aLVNscIzm24>
   - The full auto-caption transcript was retrieved and reviewed.
   - The repeatable setup is a practical geometry problem: use a sufficiently flat wall, approach it close to perpendicular, stand near the edge and choose the cursor side roughly perpendicular to the tether. Curved/irregular walls are less reliable.
   - The automatic planner uses real NavMesh wall collisions and both orbit directions; it does not hard-code montage-only map coordinates.
8. Menaki Heroic Swing guide: <https://www.youtube.com/watch?v=9RandWsJs7o>
   - Cross-checks the trade-off between a broad, safer swing and a small high-shot orbit that is easier to terminate on terrain or champions.
   - Critically, E during R is repositioning only: E attacks do not fire while Comeuppance channels. This is an explicit controller invariant.
9. Chen Chen E guide: <https://www.youtube.com/watch?v=cUTtqk6UKQ0>
   - Cross-checks circular/orbital targeting, keeping the victim near the useful edge of the orbit, and how movement toward the orbit center reduces a maximum-circle route.
   - Q or another Akshan hit marks the intended victim so E does not waste its shots on nearby units.
   - R-to-E is treated as firing-angle movement, not combined R+E damage.
10. Current E mechanic page: <https://wiki.leagueoflegends.com/en-us/Template%3AData_Akshan/Heroic_Swing>
    - Independently confirms that R can be cast during E while E cannot fire its attacks during R.
11. Current combo catalogue: <https://mobalytics.gg/lol/champions/akshan/combos>
    - Used to cross-check AA-AA-Q weaving, Q/E marking, swing/dismount ordering and R cleanup families.
    - Combo order is never accepted without passive ownership, terrain, blocker, targetability and destination-safety gates.
12. Detailed Akshan guide (25.11 revision): <https://www.mobafire.com/league-of-legends/build/25-11-in-depth-akshan-guide-mid-top-632631>
    - Cross-checks lane patterns, W roam purpose, Q extension through waves and why E direction/landing is more important than a fixed key sequence.
    - Older damage emphasis is superseded by Riot 26.1.
13. Current skill-order/statistics cross-check: <https://op.gg/lol/champions/akshan/skills>
    - Used only as a current public cross-check for how the live player base prioritizes the kit; it is not a source for runtime geometry.
14. Current Akshan-main discussions: <https://www.reddit.com/r/AkshanMains/comments/1uluywo/akshan_infinite_swing_spots/> and <https://www.reddit.com/r/AkshanMains/comments/1tsa282/e_going_the_wrong_direction/>
    - Cross-check that infinite-swing terrain and cursor-side mistakes remain live concerns in 2026.
    - Community claims are treated as hypotheses unless they match current bin data or the reviewed specialist guides.
15. Local plugin-system audit:
    - KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, ziblldev9898 and the older C# ports were searched for Akshan champion logic.
    - No local Akshan champion implementation exists. Only SDK damage, spell and evade records were found.
    - `AIAkshan` is therefore additive and is dispatched only after the ten protected legacy KuroAIO champions.
16. Local SDK audit:
    - Evade/runtime records supplied current missile aliases and cast metadata.
    - The local damage table's Q values predate Riot 26.1. Controller damage uses explicit current formulas instead of silently inheriting stale SDK values.

## Current mechanical model

### Dirty Fighting and the 26.1 identity

- `AkshanBasicAttack*`/`AkshanCritAttack` begin the passive pair; `AkshanPassiveAttack` confirms shot two. E's automatic `AkshanEAttack` is deliberately excluded from this state.
- Shot two is not a generic “always fire” rule. Champion damage, a pending third passive stack and a predicted last hit preserve it. Flee, an incoming line or likely hard CC makes it cancellable for movement/safety.
- Preservation means unrelated controller casts and orbwalker retargets wait. Cancellation remains player-owned: the controller does not synthesize movement merely to obtain the speed burst.
- Against champions, shot two uses 50% total AD and current full bonus-crit scaling. Against non-champions it uses the documented 100% AD baseline, so last-hit prediction is not artificially halved.
- The primary neutral trade is AA-AA-Q. Q is inserted only after observed shot two and inside a configurable weave window. This reflects the 26.1 shift away from obsolete automatic E-first damage routes.
- Dirty Fighting stacks from attacks, Q, E and R feed short-combat and sequential R estimates. The third stack adds current magic proc damage and the live buff remains authoritative.

### Avengerang

- Q is not a fixed 850-unit line. The simulator begins with 750 gameplay reach and adds 500 every time the outbound segment intersects another valid unit in sequence.
- Candidate lines include the directly predicted champion and directions through lane units, jungle units and other champions. A candidate must actually reach the intended target after its ordered extensions.
- The return leg begins from the simulated outbound endpoint and homes toward Akshan's live position. Its target intersection is predicted at return travel time; it is not mirrored blindly back to cast origin.
- Harass can optionally require the return hit. Mana reserve, passive shot ownership and current action state remain gates.
- Dedicated missile lifecycle callbacks distinguish outbound and return aliases. A live-missile scan repairs state if a callback is missed.
- Return alignment is visual coaching. The controller scores nearby player-compatible positions but never moves Akshan, so the user can decide whether the second hit is worth changing their path.
- Farm Q scores unique units across both legs, last hits at predicted arrival, and the number of range extensions. W/E/R are not routine waveclear substitutes.

### Going Rogue and Scoundrels

- W is a strategic roam/revive spell, not a combat steroid or a mandatory combo key.
- Automatic W requires a live Scoundrel, no nearby enemy, a configurable out-of-combat interval, cursor agreement with the hunt direction, and enough mana left for the intended Q/E/R kit.
- Near terrain is preferred because the live camouflage persists indefinitely there; away from terrain, the game-owned buff/expiry remains authoritative.
- Scoundrel score combines victim count, target missing health, distance, allies/enemies and ready point-click lockdown. A revive target does not justify a mechanically losing dive.
- Flee W is allowed only after immediate pressure has cleared and nearby terrain can sustain the escape. Attacking out of W remains controlled by the player/orbwalker.

### Heroic Swing geometry and state machine

- E1, E2 and E3 are independent runtime phases. The controller never treats E as one point dash.
- E1 scans 48 radial directions plus cursor/target-derived candidates and asks the real NavMesh for the first wall collision. Anchors with tiny or excessive radii are rejected.
- For every anchor, both clockwise and counter-clockwise orbits are sampled at 50 ms using 1200 linear speed. Target motion is interpolated while terrain, intended-target collision and other-champion collision are evaluated separately.
- A damage swing is rejected when nearby minions/champions can steal shots unless Q, an auto or another recent Akshan hit marked the intended champion. This implements E's real priority rule rather than hoping it chooses correctly.
- Expected E shots combine safe exposure duration with the current firing model, including initial and dismount shots. Current 26.1 base/AD/crit scaling is used for lethal checks.
- Large safe orbits score well for escape/angle control. A smaller high-shot orbit is accepted only when its exposure and destination justify the higher collision risk.
- E3 is locked for 0.5 seconds. When unlocked, destination candidates include cursor, target, away-from-target and radial safety points. Wall, turret, enemy count, point-click lockdown and Poppy/Taliyah/Cassiopeia dash denial are checked.
- Incoming hard CC, a wall on the next orbit sample or critical health can force an emergency E3. A normal damage E waits for its planned shot/exposure window.
- Only the orbwalker's next synthetic move is paused briefly after E1, because any movement/attack order would auto-start E2. Physical player commands remain authoritative.
- Scoundrel cleanup uses a separate reset route. Takedown-reset intent never bypasses target marking or landing safety.

### Comeuppance ammo and blockers

- R stores 5/6/7 bullets by rank across its 2.5-second channel. The controller recomputes stored ammo rather than assuming a full magazine at cast time.
- Per-bullet current raw damage is 25/35/45 plus 15% total AD, includes 30% bonus-crit scaling, and multiplies from 1x to 3x with current missing health.
- Damage is evaluated sequentially: each bullet changes health for the next bullet's missing-health multiplier and can advance/trigger Dirty Fighting.
- Blockers are ordered geometrically between Akshan and the predicted target. Every minion/monster consumes one bullet because R executes that unit; a champion or turret is a hard blocker for the remaining firing line.
- R is not started into an empty line, untargetability, invulnerability, a hopeless turret/minion screen, or an obviously better in-range auto pattern.
- During channel, an open lethal magazine releases immediately after the minimum useful storage. If a currently open line is predicted to close, cover-aware early release can preserve the available shots.
- E can reposition only inside a bounded mid-channel window when a hard blocker or excessive minion losses justify it. E damage is explicitly zero during R.
- While swinging during R, the controller can dismount before releasing inside AA range to avoid a legacy orbit-breaking attack-order interaction.
- Otherwise it releases an open full magazine near expiry. Manual R enters the same blocker/ammo state machine.

## Shared helper boundary and duplicate audit

The duplicate pass now covers Aatrox, Ahri, Akali and Akshan:

- `AIGeometry.h` owns normalized direction, 2D cross product, rotation and segment projection.
- `AIControllerHelpers.h` owns local-player/missile ownership, runtime-name checks, spell cost, prediction, hero/unit lookup, hostile-unit validation, Epic-monster classification, normalized cast-delay/remaining-buff time, neutral gapcloser capture, hard-CC name classification, enemy-cast commitment/line analysis, cast throttling and the point-click-lockdown registry.
- Akshan-specific Q range extension, moving-return intersection, wall/orbit simulation, E target-priority policy, R ammo, blocker consumption and sequential damage remain in Akshan files.
- Identical function names such as `TryCombo` or `TryFarm` are not extracted when their champion mechanics differ.

## Player-cooperation contract

- The shared selected target remains preferred. Scoundrel priority is a safety-scored fallback, not a forced camera/path takeover.
- The controller never moves Akshan and never casts Flash. Return-Q alignment, unusual wall setup, Flash swing extensions and montage-only infinite spots stay player-owned.
- Manual Q/W/E/R casts feed the same projectile, buff, recast and channel state; the controller resumes from observed reality rather than restarting a canned combo.
- Valuable AA windups and the passive second shot are preserved. The controller inserts Q only after the real second-shot event.
- E1 creates one short orbwalker-move pause to prevent an accidental automatic E2. The user's physical command can still choose the direction or dismount early.
- R movement remains player-owned. Automatic E during R is used only when geometric blocker analysis shows a better line.
- No branch pretends Akshan can interrupt a channel. Interruptable events provide commitment confidence for damage only.

## Acceptance scenarios

The controller publishes 69 Akshan-specific scenarios. Core acceptance gates are:

1. Passive shot two is observed by runtime attack identity, protected when valuable, cancellable under danger, and never confused with E shots.
2. AA-AA-Q sequencing waits for the real second shot and uses current 26.1 damage identity.
3. Q can chain multiple 500-unit extensions, predicts both legs, and homes its return to live Akshan position.
4. Q return coaching does not issue movement; manual pathing remains authoritative.
5. W requires a safe, cursor-aligned Scoundrel roam with combat and mana reserves.
6. E requires a real wall, simulates both directions, predicts moving targets, rejects target pollution and respects champion/terrain collisions.
7. E3 cannot fire during its 0.5-second lock and must choose a safe live destination.
8. Turret, enemy density, point-click lockdown and anti-dash zones veto nonlethal swing routes.
9. R stored ammo, sequential missing-health damage and Dirty Fighting procs update throughout the channel.
10. R minions consume bullets while champions/turrets hard-block; early release and E reposition use different reasons/windows.
11. E during R contributes position only, never phantom E shots.
12. Gapcloser, flee, farm, manual-cast recovery and Scoundrel-reset branches retain separate state and safety gates.
13. Standalone geometry regressions and the full Release DLL build pass after catalog integration.

## Verification completed

- `tests/akshan_geometry_test.cpp` compiles independently with MSVC C++20.
- Tests cover chained/broken Q extensions, moving return origin, E cursor direction, orbital position, closest approach, collision, shot estimate and E3 distance.
- R tests cover first blocker order, 5/6/7 ammo, channel storage, missing-health scaling and current raw bullet damage.
- Result: `ALL AKSHAN GEOMETRY TESTS PASSED`.
- Aatrox, Ahri and Akali regressions also pass after the expanded shared-helper extraction.
- Full `Release|x64` build produces `bin/Release/NightSharp.dll`.
- The only build diagnostics are the workspace's pre-existing `LNK4020` warnings for a corrupted `obj/release/vc145.pdb`; no new compile or link error is present.

## Known limits requiring live/replay telemetry

- Runtime aliases come from CommunityDragon 16.14 and local SDK records. Live 26.14 telemetry must confirm exact buff event order across skins, reconnects and loading into an already-active W/E/R state.
- Q range growth is deterministic, but live unit collision order can change between prediction and missile arrival. The controller re-plans each cast; it cannot foresee unseen units entering the line.
- W's two-second away-from-terrain expiry is game-owned. The controller observes known buffs but does not emulate every brush/terrain boundary.
- E hook lollipop size, the exact automatic-shot search radius and extremely irregular/player-created terrain need replay calibration. The current model uses conservative bin/wiki values and real NavMesh samples.
- E target priority also recognizes recent controller-observed Akshan damage. External item/summoner damage aliases should be confirmed with live event logs before broadening that registry.
- Exact R ammo UI cadence should be compared against live `AkshanRAmmo` stacks if the SDK exposes them; the pure model currently distributes rank ammo over the documented 2.5-second channel.
- A future patch can change base crit damage or item bonus crit. Current formulas intentionally encode the 26.1/26.14 baseline and must be revision-audited.
- No static model can guarantee a target will remain visible/targetable or that an unseen blocker will not enter R after release. The controller recomputes until the release event and documents this residual risk.
