# AIAphelios research and implementation dossier

Research date: 2026-07-18  
Target baseline: League of Legends 26.14 / CommunityDragon 16.14  
Controller: `AI/Controllers/AIApheliosController.h`  
Pure mechanics: `AI/Controllers/AIApheliosGeometry.h`  
Published scenarios: **305**

## Completion claim

This is not a generic spell profile with five Q names. Aphelios changes the
meaning of his attacks, Q, R, ammo, range, target access and next combo every
time a hand or gun changes. `AIApheliosController.h` therefore owns the entire
decision loop and implements:

- a five-weapon main/off-hand/three-gun queue state machine;
- hybrid ammo observation: live spell ammo is accepted only when its maximum
  identifies the 50-shot Moonlight reservoir, then attack/Q events bridge
  gaps;
- a separate Q cooldown for every weapon;
- standard and green-blue cycles plus contextual holds for survival, catch,
  peel and objectives;
- separate Calibrum, Severum, Gravitum, Infernum and Crescendum Q planners;
- all main/off-hand interaction families through pair scoring, mark delivery,
  root setup, chakram creation and Sentry snapshots;
- low-ammo three/four-gun chains and an explicit Incoming Weapon/R-cancel
  window;
- target-scoped Calibrum and Gravitum buff lifecycles;
- live mini-chakram and multi-Sentry state;
- a Moonlight Vigil trajectory model which finds the first champion hit,
  places the real explosion there and scores all enemies within its radius;
- five independently scored R variants, including W-R hidden-weapon setup;
- reactive interrupt, anti-gapcloser, peel and incoming-spell pressure gates;
  and
- explicit cooperation rules which leave movement, ordinary attacks,
  attack-move, Flash and every summoner spell to the player.

The controller publishes 305 independently named scenarios, owns its decision
loop, has a standalone pure mechanics test and is catalogued only because the
complete workspace audit found no KuroAIO or other local Aphelios controller.

## Source authority

### Primary live data

1. [Riot patch 26.14](https://www.leagueoflegends.com/en-au/news/game-updates/league-of-legends-patch-26-14-notes/)
   - Release baseline. It contains no later Aphelios override, so the 26.13
     changes below remain current.
2. [Riot patch 26.13](https://www.leagueoflegends.com/en-au/news/game-updates/league-of-legends-patch-26-13-notes/)
   - Calibrum mark: 15 + 15% bonus AD.
   - Severum Q per hit: 20/25.25/30.5/35.75/41% total AD at Riot's displayed
     breakpoints.
   - Infernum Q: 20-110 + 15-21% bonus AD.
   - Crescendum Sentry: 35-125 + 34-52% bonus AD.
3. [Riot patch 26.4](https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-4-notes/)
   - Passive AD ranks changed to 4/8/12/16/20/24.
4. [Riot patch 26.1](https://www.leagueoflegends.com/en-us/news/game-updates/patch-26-1-notes/)
   - Severum Q uses total AD and the current full crit treatment.
   - Infernum Q and Crescendum Sentry received their current formulas and
     full-crit follow-ups.
   - Moonlight Vigil follow-up crit uses 30% bonus crit damage.
   - The mid-patch update reduced lethality per E rank from 5.5 to 4.5.
5. [Riot Aphelios champion page](https://www.leagueoflegends.com/en-us/champions/aphelios/)
   - Official current ability identity and high-level weapon behavior.
6. [CommunityDragon champion JSON 16.14](https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/523.json)
   - SHA-256:
     `5458394DE2967871B5A307522ECC434989731E73EC2A7CA52B78032725A1A641`
   - 33,165 bytes at audit time.
7. [CommunityDragon Aphelios bin 16.14](https://raw.communitydragon.org/16.14/game/data/characters/aphelios/aphelios.bin.json)
   - SHA-256:
     `5FA6CE00C99EB698225F812BE71475B8F189162983EB84FE97973FB148509CBE`
   - 112,116 bytes at audit time.

Riot patch notes override secondary guides and older SDK damage ports. The
CommunityDragon bin supplies exact runtime geometry, spell/buff/missile names
and the seven odd-level breakpoints that Riot's condensed notes do not print.

### Current one-trick and rotation packet

- [The Book of Aphelios](https://www.mobafire.com/league-of-legends/build/the-book-of-aphelios-629052),
  updated 2026-06-24, is a 48k-word Master/OTP guide. It was the main source
  for pair-specific tradeoffs, oldest-gun cycle maintenance, red-first setup,
  Calibrum/Gravitum versus Calibrum/Infernum cycles, off-hand interactions,
  low-ammo swap combos, mark resets and why late showboating can lose DPS.
- [The guide's ApheliosMains discussion](https://www.reddit.com/r/ApheliosMains/comments/1b6fkdw/the_book_of_aphelios_the_most_indepth_aphelios/)
  supplied community corrections and edge-case discussion.
- [Current optimal rotation discussion](https://www.reddit.com/r/ApheliosMains/comments/1lpg063/what_is_the_current_optimal_gun_rotation_on/)
  independently documents both green-purple/blue-red and green-blue/purple-red
  branches and the red-first then oldest-gun rule.
- [July 2026 objective-preparation discussion](https://www.reddit.com/r/ApheliosMains/comments/1ukkiin/weapon_rotation_prepping_and_situational_swapping/)
  supports stopping routine ammo burn before a decisive fight and switching
  green-blue toward green-purple around the item/teamfight transition.
- [2026 rotation notes](https://www.reddit.com/r/ApheliosMains/comments/1rgdgn6/notes_for_memorizing_gun_rotation/)
  supplied a second practical repair procedure when the cycle is lost.
- [2026 rotation/combination Q&A](https://www.reddit.com/r/ApheliosMains/comments/1smd1qh/help_me_understand_how_to_use_the_weapon_rotation/)
  was used as dissenting evidence: different experienced players value
  red-white, white R and green-blue differently by game state. The controller
  consequently scores context instead of canonizing one opinion.
- [Mobalytics Season 26 combo catalog](https://mobalytics.gg/lol/champions/Aphelios/combos)
  independently cross-checked common Sentry, Phase, Q and R orderings.
- [Current Aleksis007 Rank-1/Challenger video index](https://aleksis007.ruclips.net/)
  exposes the 1:10:18 “The ONLY Aphelios Guide You Need (Season 16)” and
  current high-elo review material. It was used for player spacing, weapon
  concealment, objective preparation and when a clean auto sequence beats an
  extra swap animation.
- [Current pro-player recommendations](https://www.reddit.com/r/ApheliosMains/comments/1qz1zho/pro_aphelios_players/)
  consistently point to Peyz, Viper and Gumayusi for contemporary VOD review.
- [Aphelios 2026 pro statistics](https://lol.fandom.com/wiki/Aphelios/Statistics/2026)
  was used to identify current professional samples rather than treating old
  release-era montage play as the target behavior.

The video/guide packet was used for decisions, not copied combo strings. A
pro's weapon choice only transfers into code when the corresponding target
range, ammo, pair, health, grouping and follow-up conditions are observable.

## Complete local implementation audit

The audit searched every champion-plugin tree, including:

- `plugins/Champion/KuroAIO`;
- `plugins/Champion/7UPAIO`;
- `plugins/Champion/OneKeyToWin`;
- `plugins/Champion/SharpShooterAIO`;
- `plugins/Champion/ziblldev9898`; and
- SDK/core spell, damage, evade and orbwalker databases.

There is **no local Aphelios champion controller**. The only gameplay-aware
occurrences were:

- Kuro and SDK orbwalker support for
  `ApheliosCalibrumBonusRangeBuff` plus
  `ApheliosCalibrumBonusRangeDebuff`, extending a marked attack to 1800; and
- an old generic `DamagePassives.h` approximation for Severum/Infernum.

The controller reuses the orbwalker's mark range behavior. It does not reuse
the old damage approximation because Riot 26.13 and 26.1 replaced its numeric
assumptions. No protected KuroAIO champion route was edited or shadowed.

## Live kit packet and implementation consequence

### Passive, Phase and ammo

- Each gun starts a cycle with 50 ammo.
- Ordinary main-hand attacks spend one; Q spends ten or the remaining
  low-ammo reservoir.
- Depletion moves off-hand to main, queue head to off-hand and exhausted gun
  to queue tail.
- W swaps only main and off-hand.
- Each gun's Q has its own cooldown.
- Incoming Weapon briefly disarms Aphelios; R remains the important
  animation-cancel exception.
- Slot E is a passive next-weapon/stat interface, not a combat cast.

Implementation consequence:

- `WeaponState` owns five unique guns and per-gun ammo;
- Q runtime identity and off-hand buffs are authoritative pair observations;
- `Ammo()/MaxAmmo()` is accepted only for a Moonlight-shaped 40-60 maximum;
- local ordinary attacks and weapon-Q events form the fallback ledger;
- `ApheliosPReload` confirms the disarm window;
- every Q event updates only that weapon's cooldown; and
- the combat profile marks E `CastKind::None`, preventing accidental E casts.

### Calibrum — Moonshot

- Q range 1450, cast time 0.35, width 60 and missile speed 1800.
- A hit applies a 4.5-second mark.
- Mark attacks reach 1800 and deliver the current off-hand effect.
- A mark attack is an independent attack-reset opportunity.
- Q before R can create sequential marks; refreshing an unconsumed ordinary
  mark is often wasteful.

Implementation consequence:

- high/very-high prediction according to target commitment;
- projectile-wall and spell-shield gates;
- target-scoped mark events and expiry;
- mark attacks excluded from ordinary ammo spending;
- contextual off-hand choice for purple catch, white close DPS, blue grouping
  or red survival; and
- pre-mark-AA timing is coached but the attack remains player-owned.

### Severum — Onslaught

- Q lasts 1.75 seconds, grants movement speed and alternates red/off-hand
  attacks.
- Attack count scales with bonus attack speed.
- Current damage is 20-41% total AD per hit across the level breakpoints.
- Severum attacks are non-projectile.
- Red-white reliably builds mini-chakrams; red-purple reliably tags targets
  for a root.
- Late-game Onslaught can be a DPS loss when plain autos are already stronger.

Implementation consequence:

- no cast without a viable nearby unit;
- health, dive, projectile wall, red-white and red-purple are distinct gates;
- high attack speed plus in-range target rejects a purposeless channel;
- minion-based rotation burns require an enemy-free safety window; and
- red R is reserved for lethal pressure rather than treated as a damage R.

### Gravitum — Binding Eclipse

- Gravitum attacks apply a 30% initial slow for 2.5 seconds.
- Q affects marked targets globally and roots for one second.
- R applies a 99% slow and its follow-up root lasts 1.35 seconds.

Implementation consequence:

- Binding Eclipse is impossible with zero observed debuffs;
- marked and priority targets are counted separately;
- lone harmless marks can be held as pressure;
- interrupt, committed dash and peel release that hold;
- an off-hand mark creates a pending Phase-to-purple-root sequence; and
- a lone spell-shielded root is rejected by default.

### Infernum — Duskwave

- Q cast time 0.40, range 850, outer width/radius data 375.
- Current base is 20-110 with 15-21% bonus AD and 70% AP, followed by an
  off-hand attack.
- Infernum is the unconditional grouped-damage weapon and its R scales sharply
  with a real multi-target explosion.

Implementation consequence:

- target circles are tested against a cone, not a line;
- candidate directions include predicted enemy bearings and pair midpoints;
- ordinary combo requires the selected target or a better multi-hit set;
- lane/jungle Q thresholds are separate;
- ammo is held for a nearby objective when appropriate; and
- blue R is chosen only from the actual first-hit explosion set.

### Crescendum — Sentry and mini-chakrams

- Sentry placement range 475 and attack range 500.
- It can idle for 20 seconds, then remains active for roughly four seconds.
- It snapshots and fires the off-hand weapon.
- Crescendum attack rate depends on blade return distance.
- Mini-chakram bonus damage diminishes from 15% toward a 5% floor.
- Ability casts help preserve the mini-chakram window.

Implementation consequence:

- target, retreat, path, cursor and objective placements are scored;
- turret placement, target contact and off-hand value are separate inputs;
- flee placement is rejected when it gives Yasuo, Samira or Nilah a free dash
  target;
- multiple sentries retain network ID, position, snapshot, idle and active
  lifetime;
- Sentry precedes Crescendum R so it arms during the R cast;
- zero-stack max-range white autos can be vetoed only when a ready off-hand is
  clearly better; and
- the controller never moves Aphelios toward a returning blade.

### Moonlight Vigil

- Cast range 1300, line width 110, cast time 0.50 and speed 1000.
- The missile explodes on the first enemy champion hit; the explosion radius
  is 210 before gameplay radius.
- Base damage is 125/175/225 + 20% bonus AD + 100% AP, then the current
  main-hand follow-up attack is applied to every hit champion.
- Calibrum adds 50/80/110 mark damage.
- Severum heals 250/350/450.
- Gravitum supplies the 99% slow and 1.35-second root.
- Infernum adds 50/100/150 + 25% bonus AD and grouped overlap damage.
- Crescendum grants at least five mini-chakrams.

Implementation consequence:

- every candidate is a trajectory;
- the earliest predicted champion intersection owns the explosion center;
- secondary hits are counted only around that real center;
- current and off-hand weapon variants score the same trajectory;
- off-hand variants pay a Phase cost and use W-R when still superior;
- spell shield, projectile wall, priority targets, player HP, target range,
  catch/peel and close follow-up all change the selected R; and
- Flash remains entirely player-owned.

## Combo families encoded as state transitions

The following are not unconditional button orders:

1. Calibrum Q -> player auto/mark reset -> Calibrum R -> next mark.
2. Calibrum Q at low ammo -> incoming Infernum R -> player mark -> Duskwave.
3. Severum Q with Gravitum off-hand -> Phase -> Binding Eclipse -> incoming
   weapon/R only if the root and next gun remain valuable.
4. Gravitum Q at low ammo -> incoming Crescendum R -> close white DPS.
5. Infernum Q at low ammo -> incoming Crescendum R -> white DPS.
6. Sentry -> Crescendum R during arm time -> player-owned close attacks.
7. W -> R to conceal and select the off-hand R variant.
8. Severum or Gravitum R first when survival/control is worth more than an
   Infernum montage hit.

Every transition is conditional on observed ammo, known incoming gun, Q/R
cooldowns, target geometry, projectile wall, health, grouping, objective and
whether the player can actually follow at the required range.

## Player cooperation contract

The controller may:

- cast Q, W and R after champion-specific state and geometry checks;
- continue a manual Q/W/R inside the same ammo/weapon state machine;
- delay a spell to preserve an ordinary attack windup;
- narrowly veto an impossible reload attack, a projectile-wall attack when
  Severum is ready, or a zero-stack max-range white auto with a clearly better
  off-hand; and
- draw mark, root, queue, ammo, cooldown, Sentry, R and blade-return coaching.

The controller never:

- moves Aphelios;
- issues an ordinary attack or attack-move;
- presses Flash, Barrier, Cleanse, Heal or any summoner spell;
- automatically levels passive stats;
- assumes the player will walk into close Crescendum range; or
- spends R on an ordinary wave/camp.

This division is deliberate. Aphelios' highest-skill input is frequently the
player's spacing and attack timing; automating movement would invalidate the
very one-trick decisions the controller is intended to support.

## Runtime telemetry gaps

The pure state machine is deterministic, but live replay verification remains
required for the following bridge details:

1. Some game builds may not expose weapon ammo through Q `Ammo()`. The
   controller therefore accepts it only with a 40-60 `MaxAmmo()` signature and
   otherwise exposes `event-predicted` confidence.
2. Slot E's next-weapon UI identity is not a documented SDK field. Queue order
   is reconstructed from the known start, depletion events and observed pair;
   a contradiction drops confidence instead of inventing certainty.
3. Off-hand Q follow-up attack event flags can vary. `IsSpecialAttack`, spell
   tokens and live mark buffs prevent ordinary ammo decrement, but traces from
   all five Q/off-hand combinations should be captured in practice tool.
4. Mini-chakram count is read from Crescendum manager/orbit-manager stack
   buffs. Skin/patch aliases should be added only from captured live names.
5. Sentry ownership is inferred from allied object lifecycle and the pending
   Q snapshot. Object traces must confirm name/team payload for every skin.
6. Incoming Weapon duration varies with observed animation/event ordering. R
   remains allowed, but no late-game swap cancel is forced without tactical
   value.
7. The SDK has no universal “enemy can dash to this attackable object” query.
   Yasuo, Samira and Nilah are explicit current high-risk Sentry cases; future
   object-target dash mechanics require an audited shared registry.

These are documented telemetry boundaries, not claims of perfect observation.
When confidence is low, the controller falls back to conservative current-pair
play and does not attempt an unknown-gun swap combo.

## Verification

- `tests/aphelios_geometry_test.cpp`: **pass**.
- Geometry coverage includes weapon parsing, five-gun depletion, unknown-queue
  safety, both cycles, pair/context scoring, holds, all live damage formulas,
  Moonshot/Duskwave/R geometry, chakram DPS, Sentry safety, five R variants,
  low-ammo combos, mark-reset timing and zero-mark root rejection.
- `Release|x64` full `NightSharp.sln` build: **pass**.
- Output: `bin/Release/NightSharp.dll`.
- Published controller scenarios: **305**.
- All ten completed champion geometry executables: **pass**.
- Catalog declared entries / actual entries: **10 / 10**.
- Live image roster / protected legacy / completed AI / remaining queue:
  **173 / 10 / 10 / 153**.
- Coverage queue exact diff against roster minus protected/completed:
  **no missing and no extra champion**.
- Comment/whitespace-normalized, string-preserving controller body scan:
  **694 functions at least 100 characters, zero exact cross-controller
  implementation groups**.
- The two string-agnostic vocabulary-wrapper groups delegate their iteration
  to `TextContainsAny`/`AnyTextContains` and `HasAnyBuff`; details are recorded
  in `SharedHelperAudit.md`.
