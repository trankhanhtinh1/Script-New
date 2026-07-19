# AIBelveth research dossier

## Live-data boundary

- Runtime contract: Riot live 26.14 / CommunityDragon PC 16.14, 18 July 2026.
- CommunityDragon champion JSON SHA-256: `872a68564998d4d6167fff3ca379a4c4676eb887801e7fcf477740431ca46e0c`.
- CommunityDragon champion bin SHA-256: `625f99d68e53d3a04acd78b045f041ffdf0adb9797ff2d8f7547a7cf3cf7351f`.
- The Bel'Veth midscope datamined on PBE on 14-16 July 2026 is future test content. It is deliberately not implemented in this live controller.

Primary/current references:

- [Riot: Bel'Veth abilities rundown](https://www.leagueoflegends.com/en-gb/event/bel-veth-abilities-rundown/)
- [Riot patch 25.15 notes](https://www.leagueoflegends.com/en-us/news/game-updates/patch-25-15-notes/)
- [OP.GG Bel'Veth skills, live 16.14](https://op.gg/lol/champions/belveth/skills)
- [League mechanics reference](https://leagueoflegends.fandom.com/wiki/Bel%27Veth/LoL)
- [CommunityDragon PC 16.14 champion data](https://raw.communitydragon.org/16.14/game/data/characters/belveth/)
- [Sinerias: complete Season 16 Bel'Veth guide](https://www.youtube.com/watch?v=6JmuPoTVaRo)
- [Sawyer: Season 16 Bel'Veth guide](https://www.youtube.com/watch?v=6fCTS6mbXp8)
- [KingKong: Bel'Veth combo guide](https://www.youtube.com/watch?v=G98an3k3-t4)
- [PBE datamine excluded from live implementation](https://www.reddit.com/r/leagueoflegends/comments/1uwwh0l/pbe_datamine_july_14_2026_classic_belveth/)

The implementation uses the current OP.GG Q-E-W skill order only as build context. Cast policy comes from live mechanics and OTP play, not aggregate win-rate tables.

## Passive and attack ownership

Ability casts grant two temporary attacks, up to six, for five seconds. The controller estimates these from local cast/attack events and repairs the estimate from `BelvethPassiveStacks`. Attacks and movement remain Orbwalker/player-owned.

Endless Banquet's passive is tracked independently: every second attack against the same target procs increasing true damage. Epic monsters cap at five actual procs. The bin's ten alternating internal states are not treated as ten procs. In combo mode only, the before-attack hook may reject a stray minion target if it would break the currently selected champion chain; it never forces an attack and yields outside that narrow case.

## Q — Void Surge

Q is represented as four map-fixed diagonal 90-degree sectors with independent cooldowns plus a one-second global lock. The controller does not reduce them to four fixed aim rays: it samples precise aim offsets inside each legal quadrant and deliberately biases near boundaries, preserving the OTP technique where two adjacent arrows travel almost parallel during a chase.

The runtime reads `BelvethQHudIcon0` through `BelvethQHudIcon15`. Because the four HUD bits are an implementation detail, the controller learns bit-to-world-sector mapping from observed spend and W-refresh transitions. It then uses learned bits to repair local estimates after manual casts, W multi-hits, cooldown changes or missed events.

Every candidate traces NavMesh samples. Normal form stops at the last open point before terrain. True form can travel up to 625 units only when the trace enters terrain and finds a legal exit. This supports real wall flanks while rejecting endpoints that remain in terrain.

OTP rules encoded:

- Prefer `AA-Q-AA`; Q resets the attack timer.
- Preserve a ready auto rather than opening with Q in attack range.
- Do not spend both forward sectors merely to enter range.
- Preserve the final sector unless it kills, evades, flees or W can refund it.
- Use a wall-shortened Q after a jungle attack to reduce unnecessary travel.
- Use true-form wall Q only across verified terrain, with cursor and endpoint safety.
- Reject ordinary offensive endpoints under turret, in anti-dash zones, in ready point-click lockdown or badly outnumbered.
- Defensive Q must leave the actual tracked incoming line; it is not a generic random sidestep.

Live Q damage uses `0/5/10/15/20 + 100% total AD`, the current monster bonus, and the current minion modifier.

## W — Above and Below

W uses the live 660 gameplay range, 200 width and 0.5-second cast time. Each enemy is predicted independently. Candidate bisectors are retained only when the real line still intersects all intended hitboxes.

W is not thrown raw at a freely mobile target. Ordinary casts require sufficient prediction or a mechanically reliable window: hard crowd control, slow, spent dash or committed cast. Interrupt and directed-gapcloser reactions bypass ordinary conservatism. The controller computes the quadrant from the W cast origin to every champion hit, so one multi-hit W can refresh multiple independent Q sectors. A refund is valuable only if the sector was actually spent and follow-up exists.

## E — Royal Maelstrom

E attacks the nearest unit among those tied for lowest current-health percentage. Therefore the runtime builds one combined set of champions, lane minions and monsters in 500 range, selects the actual forced victim, and refuses an offensive champion E if another unit would steal it.

Damage is simulated strike by strike:

- Six strikes plus one per 33.333% bonus attack speed.
- Current rank base plus 8% total AD per strike.
- Missing-health multiplier recomputed after each prior strike, from 1x to 4x.
- Current reduced on-hit effectiveness model.
- 150% modifier against monsters.
- Physical mitigation calculated against the actual forced victim.

E is held against a freely escaping target. It becomes valid late in a secured combo, on an executable forced victim, during objective secure/sustain conditions, or against meaningful incoming reducible burst. Blind and Disarm reject E because it cannot declare its attacks. True-damage-only pressure never receives fictitious DR credit.

The channel is never canceled before 0.75 seconds. Afterwards it can be canceled by a safe Q that dodges a lethal tracked skillshot, by a safe Q that follows the desired target after it leaves E, or by an explicit cursor retreat after the defensive threat has ended. A blind early self-recast is prohibited.

## R — Endless Banquet

Corals are tracked by `BelvethSpore` network id and repaired by scanning allied minions if object creation was missed. Baron, Rift Herald and Voidgrub deaths mark the next nearby coral as enhanced. Form state is also verified through `BelvethRSteroid`; a remaining duration above 90 seconds confirms enhanced form.

The controller uses rank-scaled cast ranges 275/375/450, one-second channel time and a 500-radius explosion. It calculates the current true damage (`150/200/250 + 100% AP + 25% target missing health`, capped at 1500 against monsters), kill count and heal (`100/150/200 + 120% bonus AD + 90% AP`) for every castable coral.

One R consumes all existing corals globally. Evaluation therefore includes:

- Number and soonest expiry of every coral lost.
- Ordinary versus enhanced source.
- Current normal/enhanced form time and refresh waste.
- Predicted AoE hits, true-damage executes and healing.
- Objective and enhanced-wave macro windows.
- Enemy/allied bodies at the destination.
- Turret, anti-dash and point-click lockdown danger.
- Cursor agreement and manual T intent.

An ordinary coral grants 60 seconds; while enhanced it extends the form by 60 seconds. Any enhanced coral resets it to 180 seconds. The controller holds wasteful ordinary refreshes while substantial form remains unless expiry, execution or survival makes the cast worthwhile.

## Fight choreography

The controller distinguishes duel, short trade, second entry, chase, execute, kite, flee, objective and clear postures. In a multi-enemy fight Bel'Veth waits for enemy commitment, hard crowd control, a spent dash, or allied follow-up before entering. This is the consistent high-level lesson in the reviewed OTP material: Bel'Veth usually follows the engage rather than becoming the first body.

Core branches include:

- `AA-Q-AA`, then repeat the passive/R ramp.
- `Q-W-Q` only where W can reliably refund the spent chase sector.
- `Q-W-E` as a secured short finish, with E late.
- Wall-shortened jungle `AA-Q`.
- True-form wall flank after a traced exit and cursor agreement.
- W interrupt/anti-gapcloser, Q line evade, defensive E, then coral heal as escalating reactions.

No branch issues movement, attacks, Flash or Smite. The plugin supplies spell timing and rejects a very narrow bad orbwalker target swap; the player remains the conductor.
