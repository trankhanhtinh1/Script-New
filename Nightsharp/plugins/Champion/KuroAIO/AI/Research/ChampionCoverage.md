# KuroAIO AI champion coverage manifest

Manifest date: 2026-07-18
Game baseline: League of Legends 26.14 / CommunityDragon 16.14  
Live roster count: **173**

## Authority and invariants

- Roster authority is the intersection of the 173 local champion assets and the CommunityDragon 16.14 champion endpoint. Riot's 26.14 patch notes are the release baseline.
- The existing KuroAIO champion dispatch was inspected before adding the AI catalog.
- Existing KuroAIO implementations are immutable for this task: no AI file may shadow, replace, rename, or dispatch before one of them.
- A champion is **not complete** merely because it has a Q/W/E/R profile. Completion requires an `AI<Champion>.h` profile, an `AI<Champion>Controller.h` with `OwnsDecisionLoop=true`, a research dossier, published champion-specific scenarios, catalog integration, and a passing Release build.
- Mechanics that can be isolated from live memory receive a pure helper and standalone test. Runtime-only mechanics remain explicitly marked for replay/live telemetry.
- Champion-neutral duplication is governed by `SharedHelperAudit.md`; one-trick orchestration remains controller-local even when callback names match.

## Preserved KuroAIO implementations (10)

These champions stay on their original KuroAIO route and are never added to the AI catalog:

`Fiora, Katarina, Kindred, Lucian, Samira, Senna, Syndra, TwistedFate, Viktor, Yasuo`

## Completed AI one-trick controllers (18)

| Champion | Controller | Published scenarios | Pure test | Release build |
|---|---|---:|---|---|
| Aatrox | `AIAatroxController.h` | 32 | `aatrox_q_geometry_test.cpp` | Pass |
| Ahri | `AIAhriController.h` | 40 | `ahri_return_geometry_test.cpp` | Pass |
| Akali | `AIAkaliController.h` | 44 | `akali_geometry_test.cpp` | Pass |
| Akshan | `AIAkshanController.h` | 69 | `akshan_geometry_test.cpp` | Pass |
| Alistar | `AIAlistarController.h` | 78 | `alistar_geometry_test.cpp` | Pass |
| Ambessa | `AIAmbessaController.h` | 110 | `ambessa_geometry_test.cpp` | Pass |
| Amumu | `AIAmumuController.h` | 143 | `amumu_geometry_test.cpp` | Pass |
| Anivia | `AIAniviaController.h` | 170 | `anivia_geometry_test.cpp` | Pass |
| Annie | `AIAnnieController.h` | 239 | `annie_geometry_test.cpp` | Pass |
| Aphelios | `AIApheliosController.h` | 305 | `aphelios_geometry_test.cpp` | Pass |
| Ashe | `AIAsheController.h` | 141 | `ashe_geometry_test.cpp` | Pass |
| Aurelion Sol | `AIAurelionSolController.h` | 176 | `aurelionsol_geometry_test.cpp` | Pass |
| Aurora | `AIAuroraController.h` | 161 | `aurora_geometry_test.cpp` | Pass |
| Azir | `AIAzirController.h` | 166 | `azir_geometry_test.cpp` + Orbwalker soldier test | Pass |
| Bard | `AIBardController.h` | 135 | `bard_geometry_test.cpp` | Pass |
| Bel'Veth | `AIBelvethController.h` | 195 | `belveth_geometry_test.cpp` | Pass |
| Blitzcrank | `AIBlitzcrankController.h` | 167 | `blitzcrank_geometry_test.cpp` | Pass |
| Ryze | `AIRyzeController.h` | 205 | `ryze_geometry_test.cpp` | Pass |

## Research/implementation queue (145)

The queue order is the required implementation order supplied by the user; completed champions are removed only from the front without re-sorting the remainder:

`Taliyah, Qiyana, Sylas, Yone, Cassiopeia, Jayce, Gnar, KSante, Jax, Rumble, LeeSin, Corki, Ezreal, Yunara, XinZhao, JarvanIV, Vi, Naafiri, Pantheon, Nocturne, Irelia, Leblanc, Viego, Gwen, Camille, Orianna, Hwei, Mel, Caitlyn, Varus, Xayah, Kaisa, Zeri, Kalista, Ziggs, Gragas, Poppy, Skarner, Renekton, Sion, MonkeyKing, Trundle, Galio, Nidalee, Riven, Gangplank, Locke, Zaahen, RekSai, Pyke, Rakan, Neeko, Vayne, Tristana, Jhin, Sivir, Smolder, Draven, Jinx, KogMaw, MissFortune, Zoe, Lissandra, Vex, Kennen, Ornn, Olaf, Kled, Mordekaiser, Garen, Darius, Ekko, Diana, Fizz, Kayn, Khazix, Rengar, Talon, Zed, Evelynn, Shaco, Briar, Hecarim, Lillia, Graves, Elise, FiddleSticks, Karthus, Vladimir, Kassadin, Kayle, Swain, Heimerdinger, Velkoz, Xerath, Veigar, Malzahar, Brand, Lux, Nilah, Twitch, Quinn, Tryndamere, Shyvana, Udyr, Volibear, Warwick, Sett, Urgot, Yorick, Illaoi, Singed, Teemo, Nasus, DrMundo, Chogath, Malphite, Rammus, Nunu, MasterYi, Maokai, Sejuani, Zac, Shen, Ivern, Thresh, Nautilus, Rell, Leona, Braum, Renata, Seraphine, Karma, Morgana, Zyra, Lulu, Nami, Milio, Janna, Zilean, Taric, TahmKench, Sona, Soraka, Yuumi`

Next champion: **Taliyah**.

## Required research packet per queued champion

1. Pin current CommunityDragon champion JSON and record revision/hash when available.
2. Cross-check live spell rules and damage stages with Meraki/Riot data and the local SDK databases.
3. Read every local implementation for that champion across KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, ziblldev9898, and older source ports.
4. Inspect a current high-elo/pro/one-trick guide transcript with useful timestamps; reject historical mechanics that no longer match the live kit.
5. Cross-check combo families with at least one independent guide/catalog and document matchup or teamfight decision gates.
6. Define player-cooperation rules: selected target, manual casts, AA/channel preservation, cursor agreement, and which inputs remain player-owned.
7. Publish scenario names in controller metadata and document runtime-only telemetry gaps.
8. Run standalone mechanics tests where applicable and rebuild `Release|x64` after catalog integration.
