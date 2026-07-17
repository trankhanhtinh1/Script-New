# AIAatrox research dossier

Research date: 2026-07-17  
Target game revision: League of Legends 26.14 / CommunityDragon 16.14  
Controller: `Controllers/AIAatroxController.h`  
Profile: `Profiles/AIAatrox.h`

## Source ledger

1. Riot patch baseline: <https://www.leagueoflegends.com/en-au/news/game-updates/league-of-legends-patch-26-14-notes/>
2. Pinned CommunityDragon champion record: <https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/266.json>
   - SHA-256: `0a4e9a03fae0b70512e9524475a4e4f724cc88a91651c8217acac510f644a606`
   - Confirms the 26.14 spell identity, W range (825), no-cost resource model, and live tooltip data.
3. League Wiki Q data template: <https://wiki.leagueoflegends.com/en-us/Template:Data_Aatrox/The_Darkin_Blade>
   - Used for the three distinct hitboxes, 0.6-second cast time, one-second recast lockout, four-second recast window, 70% sweetspot bonus, and the fact that E is available during Q.
4. Chovy pro tips: <https://www.oneesports.gg/league-of-legends/drx-chovys-5-pro-tips-to-improve-your-aatrox/>
   - Confirms Q1-W-Q2-Q3 as the standard connected combo, E-W-Q against a fleeing target, Q-E distance correction, and R as an extended-fight amplifier.
5. Naayil challenger guide video: <https://www.youtube.com/watch?v=MX8w5uQHPk0>
   - Auto-caption transcript was inspected, not merely the title/description.
   - 17:20-18:10: bait Fiora Riposte with Q1, angle Q2-E laterally, and avoid a nonlethal Q3 that leaves Aatrox without damage.
   - 20:03-21:05: punish last-hit/attack windups and use Q2-E into the passive attack.
   - 21:59-24:33: separates close, middle, and long-range combo families. Close range uses Q-AA-Q-AA-Q3-E backward; middle range uses max Q1-W, reactive Q2-E, AA, Q3 on pull; long range uses fast E-W, then Q toward the W escape side.
6. Current one-trick mechanics guide: <https://lolmatchups.gg/champion-guides/Aatrox>
   - Cross-checks holding E for a late Q-windup correction, preserving E by walking first, W-E chase/kite use, AA-E-AA reset, and dropping Q3 in short trades.
7. Local plugin-system audit:
   - KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, and ziblldev9898 were searched for Aatrox champion implementations.
   - No existing Aatrox champion plugin was found; only evade/damage databases contained Aatrox entries. Therefore this controller does not replace a KuroAIO-supported implementation.

## Mechanical model

### Q geometry

- Q1 is modeled as a 625-unit forward rectangle with a narrow far-edge sweetspot. Its preferred target-center distance is 565.
- Q2 is modeled as a widening trapezoid beginning behind Aatrox and extending forward. Its preferred far-edge distance is 410, while its lateral tolerance grows toward the outer edge.
- Q3 is modeled as a 300-radius body circle centered 200 units forward, with a 180-radius sweetspot.
- The sweetspot solver projects target movement to Q impact, solves the Aatrox source position that would center the current sweetspot, clamps the correction to E range, then re-scores the resulting geometry.
- E is rejected unless it improves sweetspot confidence by at least 0.14, reaches the configured minimum useful displacement, and passes wall/turret/enemy-count/cursor safety checks.
- Against a moving target, E is held until the configured late-windup commitment window so the correction reacts to the dodge rather than guessing at Q start.

### Combo families

- Close anti-dive: Q1, allow the orbwalker to attack, Q2, attack, Q3 with E backward/sideways. This creates spacing instead of losing an extended AA fight.
- Standard middle range: max-range Q1, W during the confirmed Q window, observe the W escape path, Q2-E, weave the available/passive attack, then time Q3 to land on W pull.
- Long chase: E into W when Q1-E cannot reach, observe the escape side, Q1 to prevent leaving the W zone, then Q2/Q3 as cooldown and geometry permit.
- Short trade: Q1/Q2 may be used, but Q3 is deliberately dropped when it is nonlethal and lacks W/R/safety support.
- Flee/peel: prefer W, use the current Q shape for space, then E toward a safety-scored cursor position. R is reserved for a critical retreat where its movement and healing amplification can matter.

### Matchup-aware behavior

- Fiora: query the live enemy W cooldown. While Riposte is ready, Q-E correction evaluates lateral destinations so the fixed blade can still hit while Aatrox's body leaves the parry line. Telegraphed Q3 is held if Riposte remains available.
- Jax: do not wait for an auto while Counter Strike is active; continue spacing with Q instead.
- Irelia/K'Sante/Pantheon and known untargetable states: do not donate Q damage into their active mitigation window.
- Tryndamere: do not spend Q3 as an execute while Undying Rage is active.
- Generic melee dive: use the dedicated Q-AA-Q-AA-Q3-E spacing branch instead of forcing the middle-range W chain.

## Player-cooperation rules

- Player-selected target is preferred by the shared target layer.
- Manual spell input updates the observed Q stage and resets planning without overwriting the player's cast.
- Valuable AA windups are preserved, except when starting one would miss the guaranteed Q3-on-W-pull timing.
- Aggressive E corrections that strongly oppose the cursor are rejected at vulnerable health.
- No automatic Flash-Q is issued. Flash remains player-owned; the controller will observe the resulting Q state and continue the correct branch.
- The plugin never spends E to improve lane-clear Q, preserving the player's escape/response tool.

## Acceptance scenarios

The controller publishes 32 baseline scenarios through its `ChampionController` metadata. Additional Aatrox-specific gates are:

1. Q stage remains correct after assisted and manual casts and after recast expiry.
2. Q1/Q2/Q3 produce different sweetspot scores for the same relative target position.
3. A correction is never cast when the uncorrected sweetspot already exceeds the configured confidence.
4. A correction cannot enter a wall, unapproved turret range, or an over-cap enemy cluster.
5. E-W-Q is chosen only in its long-range band and never consumes E when W/Q is unavailable.
6. Standard W chaining does not insert an AA before Q2, but allows the free AA after Q2 and before pull-timed Q3.
7. The close branch preserves E until Q3 unless safety or manual input changes the plan.
8. Fiora with W ready receives a lateral Q-E candidate and causes non-forced Q3 to be held.
9. R requires engage evidence, multiple targets, or the low-health sustain branch; it is saved when Q plus an attack already kills.
10. Lane clear optimizes per-stage hit geometry and never spends E.

## Known limits requiring live-game telemetry

- CommunityDragon exposes 25000 as the nominal cast range for directional Q/E; exact Q geometry therefore comes from the Wiki and is runtime-validated through spell/cast events.
- Enemy cooldown reads are authoritative only while the enemy spell instance is visible to the client. If unavailable, the controller falls back to observed buffs and conservative behavior.
- W buff aliases and Q runtime names are matched case-insensitively and stage state is also maintained from local cast events, avoiding dependence on a single alias.
- Final certification still requires replay/live telemetry for cast-event timing, Q hit particles, W pull timing under latency, and wall-edge E behavior.

