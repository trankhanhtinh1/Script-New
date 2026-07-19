# OrbwalkerKuro — Azir Sand Soldier support

Snapshot date: 2026-07-18. Runtime target: live 26.14 / CommunityDragon 16.14.

## Audit result

OrbwalkerKuro previously had no Azir-specific attack path:

- every target was measured only from `player.Position()` and Azir's ordinary
  525 attack range;
- the final `Attack()` guard repeated that same player-centered check, so a
  target found by a soldier-aware target selector was rejected anyway;
- last-hit and lane-clear thresholds used Azir's physical basic attack rather
  than the magic damage dealt by W;
- `SDK::Utils::AutoAttack::NoAttacks` intentionally contains
  `azirbasicattacksoldier`, while OrbwalkerKuro accepted only
  `args.IsAutoAttack` from the local player.  Soldier commands therefore could
  not reliably confirm OnAttack/AfterAttack or release the windup gate.

## Live mechanics used

- W soldier lifetime: 10 seconds.
- Azir-to-soldier command/tether radius: 660.
- Soldier primary target attack range: 375.  The current soldier attack spell
  uses bounding boxes, so target radius is added for command legality.
- Spear continuation: 50 beyond the primary target.  This is collateral
  geometry only and never expands legal primary-target selection.
- Soldiers replace Azir's ordinary basic attack whenever at least one eligible
  soldier can reach the chosen target.
- Structures, wards, and traps cannot be attacked through soldiers.  They use
  Azir's ordinary range and damage.  Gangplank barrels remain ordinary unit
  targets, matching the historical soldier/barrel fix.
- All in-range soldiers stab.  The first deals full W damage and every
  additional soldier deals 25%.
- Live W raw damage is
  `rank base + level bonus + rank AP ratio`:
  - base: 50 / 65 / 80 / 95 / 110;
  - level bonus: +8 per level from champion level 10 through 18 (0–72);
  - AP ratio: 35 / 42.5 / 50 / 57.5 / 65%.
- Direct on-hit damage is applied once to the primary target at 50%.  The
  last-hit estimator intentionally does not pre-credit delayed burn damage.
- Secondary bodies take 20% through level 8, then gain 8 percentage points per
  level from level 9, reaching 100% at level 18.

## Runtime behavior

`OrbwalkerAzir.inl` scans raw allied `AIMinionClient` objects instead of relying
on `AllyPets`/`AllySpecialMinions`; the shared classifier does not guarantee
that `AzirSoldier` is present in either curated list.  Results are cached for
the current game tick.

Target selection, force-target validation, hero fallback, jungle/lane farming,
turret farming, `ShouldWait`, final attack revalidation, and killable-minion
drawing now use the same disjoint attack geometry:

1. Azir's ordinary range, or
2. any live allied Sand Soldier inside the 660 tether whose 375 range reaches
   the primary target.

The final validation is important: when a target leaves every soldier between
selection and order emission, OrbwalkerKuro declines the order instead of
making Azir path toward a formerly reachable target.

Event recognition accepts both the local command spell
`AzirBasicAttackSoldier` and soldier-side `AzirSoldierBasicAttack` aliases
across spell/script/payload fields.  Owned soldier senders are checked by
team, object kind, name, cached network id, and tether fallback.  Existing
duplicate-event suppression then collapses simultaneous stabs from multiple
soldiers into one orbwalker attack cycle.  A late soldier-side ProcessSpell is
anchored to the original attack-order tick, so the next-attack timer is not
artificially delayed by an extra windup.

The drawing menu includes `Azir Sand Soldier Ranges`: commandable soldiers get
a 375 ring and Azir gets a 660 tether ring.  Azir's ordinary AA ring remains
visible because structures still require his own basic attack.

## Reproducible data

- Riot patch 26.6 notes (W level/AP scaling):
  <https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-6-notes/>
- Riot patch 26.14 notes (current soldier on-hit/rune fixes):
  <https://www.leagueoflegends.com/en-au/news/game-updates/league-of-legends-patch-26-14-notes/>
- Current mechanics reference:
  <https://leagueoflegends.fandom.com/wiki/Azir/LoL>
- CommunityDragon champion client JSON:
  <https://raw.communitydragon.org/16.14/plugins/rcp-be-lol-game-data/global/default/v1/champions/268.json>
  - SHA-256: `4A97A961D0F00AE4D6A8FB61AE514D549CB0D2D36AD29B634673F4649A2E3469`
- CommunityDragon Azir bin JSON:
  <https://raw.communitydragon.org/16.14/game/data/characters/azir/azir.bin.json>
  - SHA-256: `3088E86E8B8BDBAEBED4D5224A2B05BA772983EED4223DB168932B786CB2570F`
- CommunityDragon Sand Soldier bin JSON:
  <https://raw.communitydragon.org/16.14/game/data/characters/azirsoldier/azirsoldier.bin.json>
  - SHA-256: `2B0C27D2D7E76A813F61443E8776B3055C6D9BEDE26C3C931D5486CC30DB83CA`

## Verification

Pure policy/geometry/damage coverage lives in
`tests/orbwalker_kuro_azir_soldier_test.cpp`.  It locks down tether edges,
bounding-radius reach, forbidden target classes, live W damage, multi-soldier
scaling, secondary-body scaling, and both event-name families.  The complete
Release x64 DLL is also built after integration.
