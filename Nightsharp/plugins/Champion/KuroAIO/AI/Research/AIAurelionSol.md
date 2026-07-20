# AIAurelionSol research and implementation dossier

Baseline date: 2026-07-18  
Target map/ruleset: Summoner's Rift  
Controller: `AI/Controllers/AIAurelionSolController.h`  
Pure model: `AI/Controllers/AIAurelionSolGeometry.h`  
Test: `tests/aurelionsol_geometry_test.cpp`

## Authority pin

- Current release baseline: [Riot patch 26.14](https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-14-notes/).
- Latest champion balance change: [Riot patch 25.22](https://www.leagueoflegends.com/en-us/news/game-updates/patch-25-22-notes/), which raised movement speed from 335 to 340 and Q burst base damage from 55/65/75/85/95 to 60/70/80/90/100.
- The Riot notes for 26.1 through 26.14 were checked. Aurelion Sol has no later Summoner's Rift balance change; 26.4 contains only a skin entry and 26.11 contains an ARAM Mayhem mana bug fix.
- Current champion overview: [Riot Aurelion Sol page](https://www.leagueoflegends.com/en-us/champions/aurelionsol/).
- Exact PC data: [CommunityDragon 16.14 champion JSON](https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/136.json), SHA-256 `8A4204342A48A50A92BD59339DF3EEF24313E72F7DF94005D4B5ED781381BDAF`.
- Exact spell data: [CommunityDragon 16.14 Aurelion Sol bin](https://raw.communitydragon.org/16.14/game/data/characters/aurelionsol/aurelionsol.bin.json), SHA-256 `8BA7C36871DFA66F3C85195302C749DA66C7E8732ECADF977A72701386FC151B`.
- Historical values were used only to disambiguate fields: [Riot 13.3 CGU](https://www.leagueoflegends.com/en-gb/news/game-updates/patch-13-3-notes/), [14.3 proactive-stacking update](https://www.leagueoflegends.com/en-gb/news/game-updates/patch-14-3-notes/), [14.9 fixes](https://www.leagueoflegends.com/en-us/news/game-updates/patch-14-9-notes/) and [14.21 E AP-ratio nerf](https://www.leagueoflegends.com/en-us/news/game-updates/patch-14-21-notes/).

Arena/Cherry and ARAM Mayhem overrides in the bin are deliberately excluded. The controller does not import their larger Q Stardust award, different max-health coefficient or other mode values into Summoner's Rift.

## Local implementation audit

The repository was searched across KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, ziblldev9898, EzEvade, ZDEvade and KuroEvade.

- KuroAIO has no existing Aurelion Sol champion route, so adding an `AI` controller does not shadow or replace supported code.
- No other local AIO contains a usable current-CGU controller.
- `SDK/Data/Database.h` and `SDK/Data/DamageData.h` contain simplified/stale spell entries and are not used as mechanical authority.
- Evade databases correctly treat Q as a special nonstandard beam and contain an R hazard entry, but they do not provide offensive combo decisions.
- KuroEvade already models first projectile-wall collision events. KuroAIO's shared SDK exposed only a boolean, so the reusable `ProjectileWallFirstContact` helper was added instead of duplicating Yasuo/Samira/Mel geometry inside this controller.

## Exact live mechanical model

### Cosmic Creator / Stardust

The controller reads several known passive aliases, checks passive-like auxiliary spell ammo, and then maintains an explicitly labeled estimate only when live telemetry is unavailable. The hashed CDragon buff is `{c9372c6b}`. Estimated E death value is never credited to the 75-stack R cycle at cast time.

The R upgrade is a separate post-learning/post-empowered-cast accumulator. It is not computed as `total Stardust % 75`. Runtime `AurelionSolR2`/ready-buff state is authoritative; predicted progress clamps at 75 and is reset only when an empowered R is consumed.

### Q — Breath of Light

- Range: `740 + 10 × champion level`, giving 750–920.
- Beam width: 140; turn rate: 180 degrees/second.
- First hostile body stops the primary beam. The solver orders collision-circle entry, not center distance, so a large offset body may intercept earlier.
- Maximum channel: 3.25 seconds at ranks 1–4; effectively indefinite at rank 5 and during W.
- Continuous same-body burst cadence: one second. Switching first body or losing contact resets partial progress immediately.
- Damage per second: 45/60/75/90/105 +55% AP.
- Burst: 60/70/80/90/100 +30% AP plus 0.031% target maximum health per Stardust; that percentage component caps at 300 against monsters.
- Each champion burst grants two Stardust.
- Secondary bodies take 50% splash damage.
- Initial mana is 30/35/40/45/50, followed by 35/40/45/50/55 per second. A release before 0.25 seconds uses the special one-second lockout; a substantive channel uses the three-second cooldown.
- Q is not a projectile and is not rejected by Wind Wall, Blade Whirl or Rebuttal.

Controller consequences:

1. Every Q plan publishes the actual first body.
2. Full Q begins only with cursor agreement, a plausible one-second contact window, enough mana including reserve, and no imminent hard-CC/melee veto.
3. Small angular candidates can legitimately thread a target's hitbox around a blocker.
4. A separate short splash-tap branch can proc lane pressure through a nearby first minion. It cannot masquerade as a full burst.
5. The controller never moves the player or cursor. It may release only a controller-owned Q; manual Q remains player-owned.

### W — Astral Flight

- Range: `1500 + 7.5 × Stardust`.
- Speed: 340 +100% bonus movement speed; Q halves flight speed.
- Recast is unavailable for the first 0.5 seconds.
- Q has no cooldown or duration cap during flight and flat damage is multiplied by 1.08/1.09/1.10/1.11/1.12.
- A takedown within three seconds of damage refunds 90% of remaining W cooldown.
- Immobilizing CC knocks Aurelion Sol down; grounded/rooted states prevent takeoff.
- E uses a special 1100 cast range during flight.

Each W candidate is sampled along the entire route. Score rewards continuous first-body Q coverage, allied follow-up and terrain separation. It heavily penalizes turret exposure, point-click lockdown, dash-denial zones and excess enemies at every sample. Offset lines are preferred to direct dives. The controller detects an abrupt cooldown reduction after recent champion contact as a reset signal, then chooses a reposition or escape; it does not automatically fly deeper.

Q-W, W-Q, W-E-Q and W-E-R-Q are separate branches. If Q is already active and the target leaves stationary range, a safe W can continue the same breath rather than releasing Q first.

### E — Singularity

- Cast time 0.2 seconds; appears after 0.5 seconds; lasts five seconds.
- Cost 90; current cooldown 12.
- Grounded range follows 750–920 level scaling; W range override is 1100.
- Outer radius: `sqrt(275² + Stardust × 900 / π)`.
- The 16.14 bin exports an inner starting radius of 120 and inner growth of 180 area units per Stardust, modeled as `sqrt(120² + Stardust × 180 / π)`.
- Damage per second: 10/15/20/25/30 +12% AP, or +60% AP over a full field after Riot 14.21.
- Execute threshold: 5% maximum health +0.026 percentage points per Stardust. Epic monsters are excluded.
- Damage checks a unit's edge; pull/execute checks its center.
- One Stardust is granted for each full second per champion inside.
- Death values on current PC live: small minion/monster 1; cannon/super minion, large monster, champion and epic monster 2.

The misleading CDragon field suffix `CountBonus` was resolved by comparing 14.2 and 14.3 bins against Riot's 14.3 notes. In 14.2 those fields directly held 3/3/5/5, and in 14.3 they directly became 2/2/2/2. They are final awards, not values added to `MinionMassDeath=1`. Therefore six ordinary minions plus one cannon yield `6×1 + 2 = 8` Stardust.

Combat E candidates include predicted target, forward/back path leads, enemy-pair midpoints, active gapcloser endpoint and R overlap. Farm E requires expected deaths and stack value, favors the cannon, and is held while a champion contests by default. Estimated E stacks influence placement score but not the R-ready state before live telemetry confirms them.

### R — Falling Star / The Skies Descend

- Range 1250; cost 100; cooldown 120/110/100.
- Falling Star delay 1.25 seconds, radius `sqrt(275² + Stardust × 900 / π)`, damage 150/250/350 +75% AP, one-second stun and five Stardust per unshielded champion hit.
- After accumulating 75 qualifying Stardust, the next R becomes The Skies Descend.
- Empowered delay two seconds.
- Empowered direct radius: `sqrt(2×275² + Stardust × 1500 / π)`.
- Empowered direct damage is 125% of Falling Star and knocks up instead of using the regular stun.
- Shockwave radius 5000, expansion time three seconds, damage 90% of Falling Star to champions and epic monsters, 50% slow to all enemies, and no duplicate shockwave damage on direct victims.

R centers include the primary prediction, active E center and clustered pair midpoints. Normal and empowered delays have independent interrupt gates. Regular R defaults to two direct hits or exact lethal/peel value. Empowered R can be held for objectives, spent for win-condition peel, or justified by large shockwave objective value.

Projectile-intercept effects require a nonstandard rule: R impacts immediately at the first barrier contact instead of simply disappearing. The shared helper bisects the SDK's collision predicate to find that contact. The controller re-evaluates direct hits at the relocated center. For empowered R it also keeps the shockwave evaluation, while ensuring direct victims are not double-counted.

## Current OTP/pro and combo research

- [Master OTP 4.3M mastery guide, updated for 26.14](https://www.mobafire.com/league-of-legends/build/26-14-mid-bot-master-otp-4-3m-mastery-aurelion-sol-guide-matchup-details-646996): Q taps for rune pressure, off-center W paths, Q-before-W continuation, cannon focus for an eight-stack wave, terrain-separated fight positioning, and stopping W when Q coverage ends.
- [Quantum, “HOW RANK 1 ASOL HAS 65% WINRATE!”, 2026-07-13](https://www.youtube.com/watch?v=OBe081NcuxQ): current high-elo visual reference for lane spacing, W patience and front-to-back fights. No transcript was available, so numerical mechanics were not inferred from the video.
- [Current Challenger Aurelion Sol notes](https://www.reddit.com/r/leagueoflegends/comments/1se1yym/quitting_the_game_so_im_sharing_everything_i_know/): front-to-back when Aurelion is the win condition; W-E-R engage only into immobile/no-CC targets; E can pull a wave away to open Q.
- [Current Challenger AMA](https://www.reddit.com/r/Aurelion_Sol_mains/comments/1rxukzs/challenger_asol_ama/): identify the opponent's key CC cooldown, then W-Q the carry after it is spent.
- [Current 1.3k-LP skill-order discussion](https://www.reddit.com/r/Aurelion_Sol_mains/comments/1rq8m05/which_spell_first_q_or_e/): Q level one and Q-W-E priority; early E farming is often less valuable than proactive Q pressure.
- [Current 1.2k-LP season discussion](https://www.reddit.com/r/Aurelion_Sol_mains/comments/1q906k5/thoughts_on_the_new_season_asol_from_a_12k_lp/): corroborates Q-W-E for the 2026 environment.
- [Current Q/blocker lane discussion](https://www.reddit.com/r/Aurelion_Sol_mains/comments/1szno6x/how_do_you_deal_with_minion_q_block_during_laning/): prepare low-health blockers with Q/E, use splash, bait the key ability, and value dodging over greedily finishing a burst.
- [Mobalytics combo catalog](https://mobalytics.gg/lol/champions/aurelionsol/combos): W-Q-E-R-Q, W-E-R-Q, E-R and E-Q families. Flash extensions remain player-owned and are intentionally absent.
- [OP.GG current ability/build page](https://op.gg/lol/champions/AurelionSol/skills) and [OneTricks.gg ranking](https://www.onetricks.gg/en/champions/ranking/AurelionSol) were used to cross-check current skill priority and specialist sources, not as numerical spell authority.

The main one-trick discriminator is not memorizing `W-E-R-Q`; it is deciding when W is allowed. The controller therefore spends more code on first-body continuity, opponent-CC windows, sampled W geometry and flight stop conditions than on fixed combo order.

## Player cooperation contract

- The selected target is preferred, but reactive peel/interrupt can override it.
- Player owns movement, attack-move, cursor steering, summoners and Flash extensions.
- Controller may protect an active Q from an accidental attack; this is menu-controlled.
- Manual Q/W/E/R casts are observed and reconciled. Manual Q is never auto-released.
- A manual R key targets the enemy nearest the cursor but still applies actual barrier-impact geometry.
- Engine manual-input arbitration yields after player casts.
- The controller does not exploit the historical chest-over-minion Q collision bug.

## Runtime-only telemetry and conservative fallbacks

- Hashed passive-buff visibility varies by bridge. The overlay labels Stardust as observed or estimated.
- E's final server death ledger is not exposed. Expected E stacks guide placement but are not prepaid into Calamity progress.
- W reset is inferred only from an abrupt cooldown drop following recent champion contact; current spell readiness remains authoritative.
- A projectile-wall contact returned through boolean-prefix bisection is approximate to a tiny fraction of the cast segment. Direct coverage is recomputed at that point.
- The SDK does not expose a separate Q beam-facing vector on every build; the live cursor direction is used because Q steering remains player-owned.

## Verification gates

The standalone test covers:

- level-scaled Q/E ranges and flying E override;
- Q mana, damage, tap lockout, first collision and continuous-target reset;
- W range, Q speed penalty, refund arithmetic and safe offset route dominance;
- exact E outer/inner radii, execute exclusion, current death awards and eight-stack cannon wave;
- both R radii/delays/damage multipliers, direct/shockwave exclusivity, interruption timing, barrier impact relocation and 75-stack state transitions.

The controller publishes 176 named scenarios, owns the entire decision loop and has no generic fallback.
