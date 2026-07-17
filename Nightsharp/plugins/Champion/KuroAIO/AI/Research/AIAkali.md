# AIAkali research dossier

Research date: 2026-07-17  
Target game revision: League of Legends 26.14 / CommunityDragon 16.14  
Controller: `Controllers/AIAkaliController.h`  
Profile: `Profiles/AIAkali.h`  
Pure geometry: `Controllers/AIAkaliGeometry.h`  
Shared primitives: `AIGeometry.h`, `AIControllerHelpers.h`

## Source ledger

1. Riot release baseline: <https://www.leagueoflegends.com/en-au/news/game-updates/league-of-legends-patch-26-14-notes/>
2. Pinned CommunityDragon champion record: <https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/84.json>
   - SHA-256: `715e041c7cf3bc93612c3e79597e8f3d0e5709dc8cf0618acd7d74d310724695`
   - Confirms current spell identities, displayed ranges and rank data for Five Point Strike, Twilight Shroud, Shuriken Flip and Perfect Execution.
3. Pinned CommunityDragon game-bin record: <https://raw.communitydragon.org/16.14/game/data/characters/akali/akali.bin.json>
   - Runtime identities inspected directly: `AkaliP`, `AkaliPWeapon`, `AkaliPZoneGround`, `AkaliW`, `AkaliWSmokeParent`, `AkaliWSmokeMissile`, `AkaliWStealthTracker`, `AkaliE`, `AkaliEb`, `AkaliEMis`, `AkaliR`, and `AkaliRb`.
   - The controller uses these names as observed state, not as unconditional proof that a predicted spell hit.
4. Meraki Analytics live Akali record: <https://cdn.merakianalytics.com/riot/lol/resources/latest/en-US/champions/Akali.json>
   - The record reports energy as the resource and `25.08` as the most recent kit-change patch; it was re-read under the 26.14 baseline rather than treated as the game revision.
   - Passive: a damaging champion ability creates one four-second ring; its center is offset 120 units from the victim toward Akali. Exiting grants the four-second Swinging Kama, doubled attack range and bonus magic damage.
   - Q: 500 target range, 350 far width, 1.5-second cooldown, 110-to-70 energy cost, and 50% slow at maximum range. Cast time scales from roughly 0.25 to 0.175 seconds.
   - W: restores 100 energy, temporarily adds 100 maximum energy, supplies movement speed and an expanding five-to-seven-second shroud. Attacking/casting reveals Akali and delays re-stealth.
   - E1: 825-unit shuriken plus a backwards movement up to 400. It can mark the first enemy or the last shroud section; the mark lasts three seconds. E2 tracks the mark globally, and Q can buffer during its dash.
   - R1: targeted champion entry, passes through the target and travels at least 750, up to 900 in long cases. R2 unlocks after 2.5 seconds, remains in a ten-second recast window, travels 800 and scales from 1x to 3x damage with missing health.
5. Coach Mysterias, “AKALI Guide - How To LEARN and Carry With AKALI Step by Step”: <https://www.youtube.com/watch?v=KIzTw2RMPso>
   - The complete auto-caption transcript was retrieved and inspected, including the ability, combo, energy and late teamfight/VOD sections.
   - 20:00-23:49: each damaging spell can open passive; W is both invisibility and extra energy; E can mark shroud; R2 gains value as health drops.
   - 23:49 onward: combo section distinguishes short passive trades, slower maximum-passive sequences and fast all-ins rather than prescribing one fixed rotation.
   - 28:52-30:58: R1-E is the dependable all-in family; the slower version extracts more passive attacks, while the fast version sacrifices some damage for certainty and speed.
   - 32:40 onward: R1-Q is a preferred branch when E is unavailable or unnecessary. E2-AA-R1 and E/R cancellation families preserve a later R2 route.
   - 34:31-36:29: passive pathing and maximum-range R-to-E timing are separate micro decisions; they are not modeled as simultaneous random casts.
   - 45:57 onward: high energy costs punish forced, zero-energy trades.
   - 2:04:13-2:05:53: enter with R1, preserve W/R2 to survive, and do not rush R2 merely because it unlocked.
   - 2:55:04 onward: rushing R2 into hooks/knock-ups cancels the dash and commonly loses the fight.
6. Mobalytics current Akali combo catalogue: <https://mobalytics.gg/lol/champions/akali/combos>
   - Used as an independent order cross-check for `R-E-W-E-AA-Q-R`, `R-E-E-AA-Q-R`, `R-E-Q-E-AA-R`, extended `R-AA-Q-W-AA-E-E-AA-Q-R`, `E-E-Q-AA`, and `Q-AA-E-E-AA` families.
   - Catalogue order alone was not used to decide safety, energy, passive or recast timing.
7. Current matchup/mechanics guide: <https://lolmatchups.gg/champion-guides/Akali>
   - Cross-checks assassin/disruptor/cleanup teamfight roles, Q-AA-Q spacing, defensive E backflip, preserving W, and taking E2 only when the destination remains playable.
8. Anguish, Season 16 Grandmaster EUW/Challenger EUNE guide (26.12 at research time): <https://www.mobafire.com/league-of-legends/build/26-12-anguish-grandmasters-euw-challenger-eune-akali-mid-top-s16-guide-every-matchup-item-rune-explained-625064>
   - Cross-checks Q's longer outer edges, E-to-shroud, Q buffering after E2, E/R dash cancellation families, R1's pass-through hit, R2's wide path, and E2 as an objective-secure tool.
   - Flash extensions/cancels are documented but deliberately remain player-owned.
9. Current Akali-main discussions: <https://www.reddit.com/r/akalimains/comments/1s0rymn/> and <https://www.reddit.com/r/akalimains/comments/1tao0ut/>
   - Repeated high-value guidance: do not autopilot W; conserve E/W/R2 when a simpler kill exists; Q-passive spacing is core; E2 contains most E damage but must not be followed into certain death; classic full route is `R1-E1-E2-Q-P-Q-R2`.
   - Community claims were accepted only where live data or the current long-form guide supported them.
10. Current high-elo player cross-check: <https://www.onetricks.gg/champions/ranking/Akali>
    - Used to confirm that the reviewed lane/teamfight material came from currently active high-elo Akali specialists, not only historical montage mechanics.
11. Local plugin-system audit:
    - KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, ziblldev9898 and older C# ports were searched for an Akali implementation.
    - No current Akali champion implementation exists in those local plugin systems. Only SDK damage, gapcloser, spell and evade database entries were found.
    - KuroAIO therefore had no legacy Akali route to preserve or replace. `AIAkali` is additive and dispatches only after the ten protected legacy champions.
12. Local SDK cross-check:
    - Spell database: Q cone range 500/radius 140; E line range 825/speed 1800/radius 60 with champion/minion collision; R1 range 675.
    - Damage database exposes E and R `SecondCast` stages. R2's damage stage already includes missing-health scaling where runtime data is available.
    - `DamagePassives.h` confirms `akalipweapon` as the passive empowered-AA buff.

## Mechanical model

### Passive ring and player cooperation

- A predicted Q/E/R hit creates only a short candidate target. It does not hard-lock the controller into passive behavior.
- `AkaliPZoneGround` is the confirmation that starts the ring state. The center is calculated from the victim toward Akali by 120 units and the remaining exit distance is recomputed from live position.
- The controller draws the ring and recommended exit point but does not move Akali. Movement, juking and Flash remain player inputs.
- `AkaliPWeapon` is authoritative for Swinging Kama. When the intended champion is inside doubled AA range, spells are held so the orbwalker/player can cash the empowered attack.
- A prepared kama is protected from an incidental minion AA while its champion target is already in range. The hold clears on the real buff removal, attack or expiry.

### Five Point Strike

- Q uses a five-ray cone model rather than the SDK's generic center line. Direct, +/-5-to-6-degree and +/-9.5-degree candidates are scored.
- The middle knife retains the documented 500 range. Outer rays receive a conservative 45-unit world-space edge extension; they are chosen only if they make an otherwise unavailable hit or tip slow more reliable.
- Tip slow becomes an explicit E setup. A normal moving target requires high/very-high prediction; hard CC, dash endpoint, E mark or tip slow lowers the required uncertainty without bypassing range.
- Q checks the live energy cost. A neutral trade reserves enough energy for the intended second Q or E route instead of draining to zero.
- Lane clear evaluates the actual cone against predicted minion positions and health. It scores last hits separately and never spends W/R to farm.

### Twilight Shroud

- W is modeled as one scarce fight resource, not a mandatory combo step.
- Defensive W responds to a line crossing Akali, a recent hard-CC path, a directed gapcloser, multiple close enemies, low health or explicit Flee.
- Offensive W is allowed only after real contact: R1 window, E mark/recast, close-range commitment or allied/enemy CC. It must restore enough energy to unlock the planned Q/E branch.
- W is withheld as a neutral opener, against a ready Mordekaiser R when configured, and before an immediate reliable reveal threat.
- Smoke missile/object callbacks update the live shroud center. E-to-shroud is attempted only when its backwards movement creates a meaningful route; E2-to-shroud is reserved for escape/emergency return.

### Shuriken Flip

- E1 first solves the forward projectile and 400-unit backwards landing as separate concerns. High prediction and collision-free flight are insufficient if the backflip ends in wall, turret, excess enemies or ready point-click lockdown.
- Harass E requires Q tip, hard CC, dash endpoint or an observed committed cast. This avoids donating the most important escape spell to an ordinary sidestep.
- An intentional E-backwards entry is a distinct branch for targets just outside Q. It requires W/R/another exit and a safe landing.
- The controller observes `AkaliEb` before treating the mark as recastable. Target buff events refine the target/expiry; a prediction by itself cannot create E2.
- E2 is optional. Enemy E2 requires visibility, targetability, arrival safety and either lethal damage, retained W/R escape or a low-enemy destination. A mark expiring soon does not override a suicidal landing.
- After E2 begins, Q is issued in the documented buffer window so it fires on arrival without taking ownership of movement.

### Epic-objective E2 secure

- Only `Epic`/`Legendary` jungle units are eligible; ordinary camps and lane minions cannot enter this branch.
- E1 is placed below a configurable objective-health threshold and still checks collision plus the backwards landing.
- E2 estimates travel time from current distance at the live 1500 speed, queries health prediction at arrival, and requires damage to exceed predicted health plus a configurable buffer.
- A contested arrival beyond the enemy limit requires W or unlocked R2 as an exit. This implements the specialist “E2 smite” mechanic without turning E into ordinary jungle spam.

### Perfect Execution

- R1 calculates the pass-through landing, at least 750 and up to 900. Wall, turret, enemy count, cursor/safety score and ready point-click lockdown gate the entry.
- R1-E1 fast and R1-Q are separate states. E prediction is recomputed from Akali's live post-R position; the original click direction is never replayed blindly.
- When an E mark exists, R1-before-E2 is chosen only if it improves the chase/return route and the later E2 landing remains safe.
- R2 is unavailable for 2.5 seconds even when the runtime recast name appears early. The ten-second expiry is tracked independently.
- R2 candidate lines score target intersection and destination safety. Recently observed hooks/knock-ups and the shared point-click-lockdown registry reject unsafe routes.
- Missing-health damage drives the execute threshold. Nonlethal R2 is normally held until the recast window is nearly over; an unlocked charge becomes an exit under threat or explicit Flee.

### Shared helper boundary

The duplicate audit across Aatrox, Ahri and Akali moved only champion-neutral code:

- `AIGeometry.h`: normalized 2D direction, cross product, rotation and point-to-segment projection.
- `AIControllerHelpers.h`: local-player/missile ownership, prediction, spell readiness, mode-slot menu gating, cast throttling, remaining-time conversion and one auditable point-click-lockdown registry.
- Champion-specific `TryCombo`, `CastQ`, `TryFarm`, state transitions, hitboxes, damage policy and callback interpretation stay in each controller even where names match. Same function names are not treated as duplicate behavior.

The shared lockdown registry also corrects a classification error found during audit: Warwick R and Skarner R are skillshots, not point-click spells, so they are handled by path/CC observation rather than the point-click table.

## Player-cooperation rules

- Player-selected target preference remains owned by the shared selector.
- The controller never controls movement and never casts Flash. Q-Flash, E1-Flash, R/E cancels requiring Flash and wall-specific manual tricks remain player-owned.
- Manual Q/W/E/R update the same passive, shroud, mark and recast states, allowing the controller to continue rather than restart a canned rotation.
- Valuable AA windups are preserved. A confirmed passive kama takes priority over ordinary spell follow-up.
- W/R2 are normally conserved as player escape resources. Semi-manual R uses the safety solver around the player's target/cursor.
- Flee order is explicit: defensive W, E2 back to shroud, safe R2 exit, defensive E1 backflip, then R1 through a threat if enabled.

## Acceptance scenarios

The controller publishes 44 Akali-specific scenarios. Core gates are:

1. Passive behavior begins from observed buffs, not a guessed hit.
2. Ring center/exit geometry matches the live 120/500-unit model.
3. A confirmed kama is not overwritten by Q/E or accidentally spent on a minion with its champion in range.
4. Outer-edge Q can recover a narrow long hit while the middle knife cannot pretend to exceed range.
5. Energy budget protects the next Q/E route; W is used only when its 100 energy materially unlocks that route.
6. E1 projectile confidence and E1 backwards-landing safety must both pass.
7. E2 is rejected when the marked target becomes invisible, untargetable, turret-trapped, crowd-controlled bait or surrounded without an exit.
8. E2-Q buffer, R1-E1 and R1-Q remain independent timed branches.
9. R2 cannot fire before the 2.5-second lock and is held for execute/exit inside the ten-second window.
10. R2 line candidates must cross the victim and land safely; observed hooks/knock-ups and targeted lockdown are brakes.
11. Objective E2 requires Epic/Legendary type and lethal arrival-time health prediction.
12. Q farm uses cone/health geometry; W/R never farm.
13. Standalone geometry tests and the full Release DLL build pass after shared-helper extraction.

## Verification completed

- `tests/akali_geometry_test.cpp` compiles independently with MSVC C++20.
- Tests cover Q center/tip, outer-edge extension, passive center and exit distance, E1 backflip, R1 pass-through, R2 path intersection and missing-health multiplier.
- Result: `ALL AKALI GEOMETRY TESTS PASSED`.
- Aatrox and Ahri geometry tests also pass after their duplicate primitives moved into `AIGeometry.h`.
- Duplicate-definition scan shows one implementation for shared direction, segment projection, local-player, prediction, spell-readiness and missile-owner helpers.
- Full `Release|x64` build produces `bin/Release/NightSharp.dll`.

## Known limits requiring live/replay telemetry

- `AkaliPZoneGround`, E target-buff and W smoke aliases come from current bin/SDK data; live 26.14 logs must confirm event order across skins and reconnect/load-mid-cast cases.
- Q's five visual knives are approximated by a conservative cone/edge model. Replay hit/miss telemetry should calibrate the 45-unit edge allowance and per-skin particle-independent cast endpoint.
- The exact frame in which `AkaliEb`/`AkaliRb` replaces the first-cast runtime name can vary by ping/client update. State windows intentionally require both runtime readiness and static timing.
- The game owns stealth reveal/re-entry timing. The controller observes known buffs/events but cannot prove every enemy true-sight source without live visibility telemetry.
- E2 objective health prediction cannot include an unseen enemy Smite cast that has not entered the event stream.
- E/R cancellation and wall interactions are highly latency/terrain dependent. The automatic branches use safe documented order; player-owned Flash/wall variants are not simulated.

