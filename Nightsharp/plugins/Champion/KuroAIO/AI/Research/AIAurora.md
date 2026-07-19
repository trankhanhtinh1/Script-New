# AIAurora research and implementation dossier

Baseline date: 2026-07-18  
Target map/ruleset: Summoner's Rift  
Controller: `AI/Controllers/AIAuroraController.h`  
Pure model: `AI/Controllers/AIAuroraGeometry.h`  
Test: `tests/aurora_geometry_test.cpp`

## Authority pin

- Current release baseline: [Riot patch 26.14](https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-14-notes/), released 2026-07-14. Aurora has no 26.14 balance entry.
- Current champion overview: [Riot Aurora page](https://www.leagueoflegends.com/en-us/champions/aurora/).
- Original ability contract: [Riot Aurora abilities rundown](https://www.leagueoflegends.com/en-au/news/game-updates/aurora-abilities-rundown/).
- Exact current champion data: [CommunityDragon PC 16.14 champion JSON](https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/893.json), 25,853 UTF-8 bytes, SHA-256 `0A0566FCEBB4AEC0B1A15ED8F2D0E311582BB2D0A59C43D0DEF92A6BD18891E6`, upstream last-modified 2026-06-26.
- Exact current spell data: [CommunityDragon PC 16.14 Aurora bin](https://raw.communitydragon.org/16.14/game/data/characters/aurora/aurora.bin.json), 49,100 UTF-8 bytes, SHA-256 `EAC19C73F4E2A9D870B3AF1FC582745000F8A3A584450917C2ED62300F4785B1`, upstream last-modified 2026-07-15.
- Historical notes used to reconcile the current bin: [14.15](https://www.leagueoflegends.com/en-gb/news/game-updates/patch-14-15-notes/), [14.17](https://www.leagueoflegends.com/en-us/news/game-updates/patch-14-17-notes/), [14.18](https://www.leagueoflegends.com/en-us/news/game-updates/patch-14-18-notes/), [14.23](https://www.leagueoflegends.com/en-gb/news/game-updates/patch-14-23-notes/), [25.12](https://www.leagueoflegends.com/en-gb/news/game-updates/patch-25-12-notes/) and [25.18](https://www.leagueoflegends.com/en-sg/news/game-updates/patch-25-18-notes/).

Patch 14.23 is the important semantic boundary. It removed passive movement speed, increased Q/E reach, put movement speed on W, and changed R from trapping enemies to slowing them as they cross the boundary. Any release-era guide or local database that still treats current R as a cage is rejected.

## Local implementation audit

The repository was searched across KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, ziblldev9898, EzEvade, ZDEvade and KuroEvade.

- KuroAIO has no existing Aurora route. `AIAurora` therefore adds coverage without shadowing, replacing or changing a supported controller.
- No other local champion plugin implements Aurora's combat decision loop.
- `sdk/Data/Database.h` and evade tables contain Q/E threat shapes. They are useful collision cross-checks but have no Q2 return-bolt, passive-weave, W-reset, E-recoil or R-portal policy.
- `sdk/Data/DamageData.h` and `sdk/Wrappers/Damages/DamagePassives.h` were inspected. Live CDragon and Riot patches take precedence when local generated values differ.
- The shared AI engine already owns prediction, cast ownership, manual-input arbitration, point-click threat registries, projectile-wall queries, cursor agreement and target selection. Aurora composes those helpers instead of cloning them.
- Five controller-local `ManaPercent()` wrappers in Anivia, Annie, Aphelios, Ashe and Aurelion Sol were found during this pass. They now call the shared `PlayerManaPercent()` helper; champion-specific mana thresholds remain local policy.

## Exact live mechanical model

### Spirit Abjuration

- Every attack or damaging spell applies one stack for four seconds. The third application consumes the stack cycle and procs against that target.
- Champion proc damage is `1% + 0.027% × AP` of target maximum health: 3.7% at 100 AP. The current monster curve caps that component from 10 to 90 by champion level.
- A champion proc creates a spirit for four seconds. Up to four spirits heal Aurora each second.
- Each spirit heals 3–20 by level +2% AP per second, so four level-18 spirits heal 80 +8% AP per second.
- Passive movement speed no longer exists on Summoner's Rift.

The controller keeps a record per target, not one global counter. It understands both buff bridges that expose applications as 1/2/3 and older bridges that expose 2/4/6. Auto attacks are observed, never generated. If live buff telemetry appears, it overrides the estimate.

### Q — Twofold Hex

- Cost 60; cooldown 9/8.5/8/7.5/7; cast time 0.25 seconds; range 900.
- Outbound speed 1600 and gameplay missile width 90. It pierces and marks every body hit for 3.5 seconds.
- Q1 damage: 45/70/95/120/145 +40% AP.
- Q2 is a free self recast and automatically fires at mark expiry. Since 14.15, the original marked target cannot dodge its own returning bolt.
- Return speed 2000. Every marked body's bolt travels from that body to Aurora's current position. Unmarked units crossed by a bolt can be hit.
- Q2 uses the same base damage and grows continuously to 1.5× based on missing health, for a maximum 67.5/105/142.5/180/217.5 +60% AP.
- The first bolt damaging a target is full; each additional return bolt contributes 20%. Minion Q2 damage has its own 40% modifier.
- Q2 can interrupt Aurora's current auto attack. E, W or R can be used around the pullback firing window.

Controller consequences:

1. Q1 generates small angular candidates and scores every champion/minion it pierces. Projectile walls veto the selected Q1 line.
2. Each mark has an ID, position, radius, expiry and observed/estimated confidence.
3. Q2 evaluates every mark-to-Aurora segment and orders hits by arrival distance before applying full/additional-bolt modifiers.
4. Q2 waits for autos or E to raise missing-health damage when time permits. Lethal, passive proc, escaping target and expiry are explicit immediate branches.
5. A player auto windup is preserved unless Q2 is lethal or about to expire.
6. On a marked wave, Q2 fires before E if E would delete return-bolt sources. This produces the OTP `Q1-Q2-E` clear rather than a generic `Q-E-Q2` sequence.
7. The player's current path endpoint is evaluated as a better return alignment, but no movement or cursor command is issued.

### W — Across the Veil

- Cost 80; cooldown 22/21/20/19/18; base dash 300.
- A valid terrain hop may resolve at an exit up to 450 units away.
- Invisibility lasts 1/1.15/1.3/1.45/1.6 seconds. An attack or spell breaks it; an auto breaks it at windup completion.
- Realm Hopper grants 20/25/30/35/40% movement speed for four seconds.
- A takedown within three seconds of damaging an enemy champion resets W.
- If the dash is interrupted, stealth is normally lost. During the dash Aurora is locked out of ordinary spell/item/summoner use.

Every W direction resolves a real navmesh endpoint. The route score includes cursor agreement, wall exit, distance from threat, retained Q/E angle, ally presence, enemy count, turret, anti-dash zones and ready point-click lockdown. Offensive W waits for the opponent's key CC to be spent or for the target to be controlled. A likely takedown adds value only after recent champion damage; it never excuses a dangerous endpoint. Abrupt cooldown reduction after contact is recorded as a reset and biases the next W toward re-angle or escape.

### E — The Weirding

- Cost 80; cooldown 15/14/13/12/11; cast time 0.35 seconds.
- Range 825 and total width 175. E is not a projectile, so Wind Wall, Blade Whirl and Rebuttal do not delete it.
- Damage: 70/110/150/190/230 +70% AP.
- Slow starts at 80%, decays over one second, and begins decaying after roughly 0.15 seconds.
- Aurora recoils 250 units opposite the cast. Current recoil speed is 150 +200% current movement speed.
- Grounded/rooted Aurora can cast E, but does not recoil. A root/ground arriving during cast can likewise suppress the dash.
- E may cross thin walls only when its final endpoint is navigable. R can be cast during the recoil; most other casts are locked.

The planner models the damage line and recoil endpoint separately. It rejects new turret exposure, terrain, dash denial and point-click lockdown behind Aurora. E is normally late in an all-in because premature recoil ends the chase. Separate branches exist for long-range poke, gapcloser peel, one-instance displacement buffering, marked-wave follow-up and inward E whose recoil crosses an R portal.

### R — Between Worlds

- Cost 100; cooldown 140/120/100; cast range 700.
- Aurora's initial leap is capped at 250. The damaging arena is approximately 425 units ahead and has radius 700; these are not one generic endpoint.
- Damage: 175/275/375 +70% AP. The initial wave slows by 30% for two seconds.
- Arena duration: 2.5/3.25/4 seconds.
- Crossing the boundary slows enemies by the current 50% for 1.5/1.75/2 seconds. Enemies are not trapped and can walk, blink or Flash out.
- Aurora touching an arena edge is transported to the diametrically opposite edge and is untargetable during transit.
- Initial leap is unstoppable while airborne. It can buffer a single incoming CC instance, but long CC persists after landing and suppression is not cleansed.
- R recast ends the arena early.

The runtime model separates four decisions:

1. **Leap:** validate turret, terrain exit, point-click lockdown and anti-dash zones. Directly unjumpable terrain invalidates the plan instead of letting server relocation corrupt the intended direction.
2. **Arena:** score priority hits, allied follow-up, enemy blink readiness and whether the follow-up can realistically produce one or two passive procs.
3. **Portal:** calculate boundary contact and exact opposite destination. Reject wall, turret, excess-enemy and lockdown destinations. A large position discontinuity confirms transit and starts a conservative internal cooldown.
4. **End:** recast only a controller-owned R when the player is heading into an unsafe forced portal, or after targets leave and no defensive portal value remains. Manual R is never ended automatically.

The controller can use E inward so recoil touches a safe boundary during incoming one-instance CC. W-hidden portal is available only behind an explicit off-by-default option. Normally W, E and portal remain three separate mobility resources.

## Current OTP/pro research

- [Rebrrt, Rank 1 Aurora, “How to Play Aurora Like a Challenger,” 2026-07-07](https://www.youtube.com/watch?v=vuL-r4AhyZ0). The complete 10:56 transcript was inspected. It demonstrates `Q1-Q2-E` waveclear, `Q1-E-Q2` single-target damage, waiting Q2 for missing health/autos, repositioning to align return bolts, E-last chase discipline, W deception/wall hops/reset reuse, E displacement buffering, R terrain failures, separate mobility-resource usage and portal untargetability.
- [Rebrrt channel](https://www.youtube.com/@rebrrtt) and [Rebrrt's site](https://rebrrt.com/) identify the source as a Master–Challenger player and Rank 1 Aurora finisher. Current 2026 VODs cross-checked include [2400 LP Challenger with Aurora](https://www.youtube.com/watch?v=Rhzh7RHJbAo), [How a 2000 LP Aurora Destroys Lobbies](https://www.youtube.com/watch?v=KSphKhJaJFg) and [How Aurora Destroys Challenger Players](https://www.youtube.com/watch?v=L6wWj1XlafE).
- [Shok, former pro/Rank 1, full Season 15 Aurora guide](https://www.youtube.com/watch?v=eImpYuwu4vk). The complete 34:20 transcript was inspected. It corroborates Q-auto-Q2 level one, Q-E-Q2 level two, saving W mana before first base, using E/Comet in long-range lanes, following jungle/skirmish setup, flanking rather than blind primary engage, and turret-aggro portal resets.
- [Current Skill-Capped Aurora guide](https://www.skill-capped.com/lol/guides/builds/aurora/mid) corroborates short `Q-AA-Q2`/`Q-E-Q2` trades and the extended `Q-AA-AA-Q2-AA-E` two-passive sequence.
- [Current Rank 1 EUNE AuroraAddict guide](https://www.mobafire.com/league-of-legends/build/26-10-rank-1-aurora-eune-advanced-combos-the-only-grandmaster-aurora-guide-youll-ever-need-646363), [Shok Rank 1 guide](https://www.mobafire.com/league-of-legends/build/26-1-shoks-rank-1-challenger-aurora-guide-645849), [Peng 10× Challenger guide](https://www.mobafire.com/league-of-legends/build/25-22-10x-challenger-pengs-aurora-guide-649256), [OneTricks streamers](https://www.onetricks.gg/champions/streamers/Aurora) and [OP.GG Aurora mid](https://op.gg/champions/Aurora/build/mid) were used to cross-check current specialist identity and ability priority, not as numerical spell authority.

The one-trick discriminator is not a fixed `W-R-Q-E`. It is deciding whether Q2 should wait, whether W will reset, whether E recoil gives up the chase, whether R creates enough time for two passive procs, and whether the opposite portal is safer than preserving W/E. Those decisions dominate the controller.

## Player cooperation contract

- Player owns movement, cursor, attack/attack-move, orbwalker target, summoners and Flash extensions.
- Controller yields while a useful auto can be woven and observes the completed hit through `OnAfterAttack`.
- Selected orbwalker target is preferred. Immediate incoming CC, a directed gapcloser or exact kill secure can temporarily override it.
- Every manual Q/W/E/R is observed. Manual Q2 and R recast reconcile state; a player-owned Q or arena is not forcibly recast.
- Manual R key selects the enemy nearest the player's cursor but still validates leap/terrain/arena geometry.
- The overlay exposes Q marks/return paths, W endpoint, E recoil, R portal destination and predicted versus observed passive state.
- The controller never automates Q-Flash, E-Flash, Q-E-Flash, attack movement or arena-boundary walking.

## Runtime-only telemetry and conservative fallbacks

- Some bridges expose Aurora passive counts doubled. The model changes encoding only after evidence and labels predicted state until a buff is observed.
- Q hit/mark buffs can arrive under hashed or patch-specific aliases. Cast geometry seeds an estimate; buff add/remove is authoritative when present.
- W takedown events are not exposed directly. A large cooldown drop within the recent-damage window confirms the reset.
- R portal has no stable public cooldown field in the SDK. A >900-unit position discontinuity during the arena confirms transit; a conservative 350 ms lock is then used.
- Pending turret-shot ownership is only partially exposed. Portal automation requires a current incoming-threat signal and safe destination; otherwise it remains coaching-only.
- Exact wall-forgiveness relocation is server-owned. W/R terrain plans accept only a sampled non-wall exit inside their live forgiveness range.
- R damage has a small server wave delay. Prediction uses a conservative 0.42-second impact sample rather than treating it as instant.

## Verification gates

`tests/aurora_geometry_test.cpp` independently verifies:

- passive AP/max-health arithmetic, monster cap, spirit healing, telemetry normalization, expiry and proc cycles;
- Q1 capsule hitbox, pierce list, Q2 missing-health/additional/minion modifiers, guaranteed own bolt, crossing-bolt alignment, auto-windup preservation and marked-wave exception;
- W ranks, base/wall endpoints, reset value, turret veto and offensive/defensive windup policy;
- E damage, non-projectile line, recoil/no-recoil state, endpoint veto, displacement buffer and chase preservation;
- R damage/durations, leap versus arena center, ray/circle boundary, opposite destination, portal threat policy, mobility-resource separation, early end and terrain rejection;
- Q/auto/E/Q2 and R-opening combo state transitions.

The controller publishes 161 champion-specific scenarios, sets `OwnsDecisionLoop=true`, is registered only after the preserved KuroAIO routes, and passed a full `Release|x64` build.
