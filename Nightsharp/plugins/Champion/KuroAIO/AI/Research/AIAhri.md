# AIAhri research dossier

Research date: 2026-07-17  
Target game revision: League of Legends 26.14 / CommunityDragon 16.14  
Controller: `Controllers/AIAhriController.h`  
Profile: `Profiles/AIAhri.h`  
Pure geometry: `Controllers/AIAhriGeometry.h`

## Source ledger

1. Riot patch baseline: <https://www.leagueoflegends.com/en-au/news/game-updates/league-of-legends-patch-26-14-notes/>
2. Pinned CommunityDragon champion record: <https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/103.json>
   - SHA-256: `c50bfc7446179e9825b9cdfadf1859df9e892c3dc7df94ef7642139501a24bd6`
   - Confirms the live spell identities and displayed ranges: Q 970, W acquisition 700, E 975, and R dash 450.
3. Meraki Analytics live champion record: <https://cdn.merakianalytics.com/riot/lol/resources/latest/en-US/champions.json>
   - Cross-checks Q's 0.25-second cast, 200-diameter path, 1550 outbound speed and accelerating return; W movement/target priority and reduced follow-up flame damage; E's 120-diameter collision projectile, knockdown and Charm; and R's recast/ammo behavior.
4. Shok, current Season 16 Ahri guide: <https://www.youtube.com/watch?v=lznNW28CZ-I>
   - The auto-caption transcript was inspected directly.
   - 09:51-10:50: do not donate raw Charm. Wait for allied CC, dash close, or create an E-Flash angle; missing E commonly loses Q and the rest of the damage.
   - 13:01-13:47: the teamfight objective is not simply to cast every spell; guarantee Charm and route return Q through the target. R can pull the returning Orb through a victim and a short R can make E effectively guaranteed.
   - 19:34 onward: choose between forward assassination and backward peel from team state. Wait out ready point-click lockdown before crossing the frontline, then prioritize the valuable carry.
   - 26:59 onward: against difficult ranged lanes, use W movement to dodge, make a quick Q-W/AA trade, and avoid extending a weak trade.
5. Pro mechanics compilation, “Ahri Tips and Tricks That PRO Players Use”: <https://www.youtube.com/watch?v=Rfgd22tnlHU>
   - Transcript inspected. Relevant current mechanics were independently cross-checked against current data before implementation.
   - Q at the turnaround point can apply both passes almost together; R or player-owned Flash can redirect Q2.
   - W reaches a marked/recently attacked target more reliably and close flames arrive faster.
   - E-R masks the Charm animation; E also knocks down a dash.
   - Trade families include E-W-Q-AA, AA-W-AA, and R1-E-Q-W-R2-AA, while the last R is normally the exit.
   - The video's historical passive description was not used because it no longer represents the live kit.
6. Current matchup/mechanics guide: <https://lolmatchups.gg/champion-guides/Ahri>
   - Cross-checks using R primarily to improve E/Q2 geometry, preserving mobility, and selecting pick/peel posture rather than treating R as unconditional damage.
7. Combo catalogue: <https://mobalytics.gg/lol/champions/ahri/combos>
   - Used only as a combo-order cross-check; live timings and spell rules come from CommunityDragon/Meraki and runtime spell data.
8. Local plugin-system audit:
   - Read the OneKeyToWin C++ port at `OneKeyToWin/Champions/Ahri.h` and its older C# source at `OneKeyToWin/OKTW_CSharp/Champions/Ahri.cs`.
   - The C# source contains an important feature lost by its C++ port: outbound/return Orb missile tracking and an R destination computed to pull Q2 back through the target.
   - KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, and ziblldev9898 were inventoried. KuroAIO had no existing Ahri champion implementation, so this AI-prefixed controller does not replace a supported plugin.
9. Local evade databases were used only to make runtime name matching resilient:
   - Outbound aliases include `AhriOrbMissile` and `AhriQMissile`.
   - Return aliases include `AhriOrbReturn`, `AhriQReturnMissile`, and `AhriOrbofDeception2`.
   - Charm aliases include `AhriSeduceMissile` and `AhriEMissile`.

## Mechanical model

### Orb of Deception

- The controller subscribes to the dedicated missile-create/delete hooks instead of treating Q as an ordinary line spell. It also scans live missiles as a recovery path if a lifecycle event was missed during load or hook transition.
- Outbound and return states are separate. The intended champion target survives the outbound phase and is cleared only when the return Orb ends.
- Tip-double-hit logic scores whether the predicted target center lies in the Orb's turnaround band and remains within the combined Orb/target radius. Only then does it deliberately cast along the full travel line.
- Q1 and Q2 damage are queried as separate damage stages. Q1 uses the magic-damage stage; Q2 uses the `WayBack`/return stage, which preserves its true-damage calculation in the SDK damage library.

### Geometric Q2 redirection

The central one-trick mechanic is implemented as a segment-interception solver:

1. Read the live return Orb position.
2. Treat every safe post-R Ahri position as the end of the return segment.
3. Project the target onto `Orb position -> Ahri destination`.
4. Estimate when the Orb reaches that projection, predict the target at that time, and solve once more with the refined position.
5. Combine target bounding radius and Orb radius to score the hit.
6. Compare against the current no-dash return line. R is rejected if the current line already hits, the candidate fails the minimum post-R score, or the improvement is too small.
7. Apply wall, turret, enemy-count, cursor, and ready-lockdown safety before spending a charge.

The exact functions used by the controller are in `AIAhriGeometry.h` and are exercised by `tests/ahri_return_geometry_test.cpp`.

### Charm certainty

- Normal E requires a quality window rather than merely a target inside range. Quality rises for hard CC, a dash endpoint, an observed spell/AA commitment, immobility prediction, or a short post-R angle.
- Ordinary long-range E has a separate maximum range unless the target is committed or controlled. Collision is always checked.
- Known spell shields, untargetability, parry, and no-death windows prevent donating Charm.
- E cancels directed dashes and is the first interrupt tool.
- A confirmed Charm buff starts Q-W follow-up. Therefore a player-created E-Flash is supported without the controller ever taking ownership of Flash.

### E-R versus R-E

- E-R: when the current E line is already high confidence and collision-free, cast Charm first and R during the missile's early flight. This masks the animation and changes Ahri's position without corrupting the independent Charm projectile.
- R-E: when current E is blocked, out of practical range, or unreliable, evaluate radial and side R candidates. Predict E from each candidate, reject collision/danger/lockdown, dash, wait for landing, then cast E from the new source.
- A ready Malzahar/Lissandra/Vi/Nautilus/Pantheon/Maokai/Fiddlesticks control tool and relevant Annie/Twisted Fate/Renekton states are explicit dive brakes. The risky lethal override is disabled by default.

### Spirit Rush charge economy

- Before activation, spell readiness represents the initial three-cast window because some runtime builds expose recast ammo as zero until the first dash.
- Once active, live `Ammo`/`MaxAmmo` are authoritative. Cast events provide a fallback counter, and buff end-time maintains the 15-second window.
- The one-second static recast lockout is enforced with a small ping allowance.
- One charge is reserved by default. Q2 redirection cannot consume it unless the return plus R is lethal. R itself may spend the reserve to secure a takedown because the takedown can create another recast.
- Under nearby threat, an expiring reserved charge is converted into a safety-scored exit. Explicit Flee mode may spend any available charge.

### Fox-Fire and AA cooperation

- W is not fired simply because an enemy is inside 700. It waits for Charm, the recent AA target, the tracked Q target, close range, or lethal damage so the flames acquire the intended champion.
- The short lane trade is AA-W-AA. The quick ranged trade is Q followed by W/AA while W movement helps Ahri disengage.
- W movement can be used defensively only after an enemy spell segment is detected crossing Ahri's current body radius; it is not an unconditional speed-button spam.
- AA windups are preserved. The only cancellation is a very late AA that would consume the remaining guaranteed Charm window before Q can launch.

### Teamfight posture

- The controller estimates whether an allied high-offense/low-health carry is being dived. If so, peel E/W/Q takes precedence over an ordinary forward engage.
- An isolated target becomes an assassination candidate only when the non-reserved damage budget is sufficient or the configured health condition is met.
- Low Ahri health switches to escape posture. Destination scoring then maximizes enemy separation and cursor agreement rather than damage proximity.

## Player-cooperation rules

- Player-selected targets remain preferred by the shared target selector.
- Manual Q/E/R events are observed and update projectile, Charm, and ammo state; they do not get overwritten by a generic combo plan.
- No automatic Flash is cast. Manual E-Flash, Q-Flash, and Flash-Q2 routing remain player-owned; the controller continues from the observed missile/buff state.
- Aggressive R that strongly opposes the player's cursor is rejected at vulnerable health.
- The last R is normally protected for the player's exit.
- The controller never changes player movement or takes orbwalker ownership; it only times spells around the active mode and AA windup.

## Acceptance scenarios

The controller publishes 40 champion-specific scenarios through its `ChampionController` metadata. The non-negotiable gates are:

1. Both current and legacy outbound/return missile aliases drive the same state machine.
2. Deleting the outbound missile cannot erase a newly created return missile.
3. R is never cast for Q2 if the current segment already intersects the predicted target.
4. A chosen Q2 redirect must improve the geometric hit score by the configured threshold and pass every destination safety gate.
5. E-R and R-E are distinct timed branches and cannot collapse into simultaneous random casts.
6. The initial R works even if pre-activation ammo reads zero; active recasts use live ammo.
7. The reserved charge survives normal damage and Q2 routing, yet can execute or exit before expiry.
8. Raw E below minimum quality is held even in Combo.
9. Confirmed Charm, including manual E-Flash, produces Q then marked W follow-up.
10. Peel posture can replace a forward assassination choice when a valuable ally is under melee/dash pressure.
11. Q farm evaluates the same path twice and R is never used for farming.
12. The standalone geometry test and the full Release DLL build both pass.

## Verification completed

- `tests/ahri_return_geometry_test.cpp` compiles independently with MSVC C++20.
- Tests cover segment projection, centered hit, radius miss, a side dash converting a miss to a hit, tip-double qualification, and return travel timing.
- Result: `ALL AHRI RETURN GEOMETRY TESTS PASSED`.
- Full `Release|x64` build produces `bin/Release/NightSharp.dll`.

## Known limits requiring live-game telemetry

- Return Q accelerates toward Ahri. The solver uses an average speed for arrival prediction and the live position every update; replay telemetry should calibrate the average across short/long returns and high ping.
- Runtime ammo is intentionally preferred after R activation, but takedown/reset timing should be replay-tested because the exact client field update may arrive one frame after the takedown.
- W target selection is owned by the game. The controller can create the documented priority conditions but cannot force an individual flame target.
- Ready point-click threat coverage is deliberately conservative and finite. It should expand from matchup telemetry, not from a generic “all spells are dangerous” rule.
- Final certification still requires live/replay logs for missile aliases, buff aliases, E-R cast ordering, Q2 redirect under player movement, and R reset ammo on the 26.14 client.
