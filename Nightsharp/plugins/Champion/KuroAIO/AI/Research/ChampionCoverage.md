# KuroAIO AI champion coverage manifest

Manifest date: 2026-07-17  
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

## Completed AI one-trick controllers (6)

| Champion | Controller | Published scenarios | Pure test | Release build |
|---|---|---:|---|---|
| Aatrox | `AIAatroxController.h` | 32 | `aatrox_q_geometry_test.cpp` | Pass |
| Ahri | `AIAhriController.h` | 40 | `ahri_return_geometry_test.cpp` | Pass |
| Akali | `AIAkaliController.h` | 44 | `akali_geometry_test.cpp` | Pass |
| Akshan | `AIAkshanController.h` | 69 | `akshan_geometry_test.cpp` | Pass |
| Alistar | `AIAlistarController.h` | 78 | `alistar_geometry_test.cpp` | Pass |
| Ambessa | `AIAmbessaController.h` | 110 | `ambessa_geometry_test.cpp` | Pass |

## Research/implementation queue (157)

The queue is alphabetical so coverage can be mechanically diffed against the live roster:

`Amumu, Anivia, Annie, Aphelios, Ashe, AurelionSol, Aurora, Azir, Bard, Belveth, Blitzcrank, Brand, Braum, Briar, Caitlyn, Camille, Cassiopeia, Chogath, Corki, Darius, Diana, Draven, DrMundo, Ekko, Elise, Evelynn, Ezreal, FiddleSticks, Fizz, Galio, Gangplank, Garen, Gnar, Gragas, Graves, Gwen, Hecarim, Heimerdinger, Hwei, Illaoi, Irelia, Ivern, Janna, JarvanIV, Jax, Jayce, Jhin, Jinx, Kaisa, Kalista, Karma, Karthus, Kassadin, Kayle, Kayn, Kennen, Khazix, Kled, KogMaw, KSante, Leblanc, LeeSin, Leona, Lillia, Lissandra, Locke, Lulu, Lux, Malphite, Malzahar, Maokai, MasterYi, Mel, Milio, MissFortune, MonkeyKing, Mordekaiser, Morgana, Naafiri, Nami, Nasus, Nautilus, Neeko, Nidalee, Nilah, Nocturne, Nunu, Olaf, Orianna, Ornn, Pantheon, Poppy, Pyke, Qiyana, Quinn, Rakan, Rammus, RekSai, Rell, Renata, Renekton, Rengar, Riven, Rumble, Ryze, Sejuani, Seraphine, Sett, Shaco, Shen, Shyvana, Singed, Sion, Sivir, Skarner, Smolder, Sona, Soraka, Swain, Sylas, TahmKench, Taliyah, Talon, Taric, Teemo, Thresh, Tristana, Trundle, Tryndamere, Twitch, Udyr, Urgot, Varus, Vayne, Veigar, Velkoz, Vex, Vi, Viego, Vladimir, Volibear, Warwick, Xayah, Xerath, XinZhao, Yone, Yorick, Yunara, Yuumi, Zaahen, Zac, Zed, Zeri, Ziggs, Zilean, Zoe, Zyra`

Next champion: **Amumu**.

## Required research packet per queued champion

1. Pin current CommunityDragon champion JSON and record revision/hash when available.
2. Cross-check live spell rules and damage stages with Meraki/Riot data and the local SDK databases.
3. Read every local implementation for that champion across KuroAIO, 7UPAIO, SharpShooterAIO, OneKeyToWin, ziblldev9898, and older source ports.
4. Inspect a current high-elo/pro/one-trick guide transcript with useful timestamps; reject historical mechanics that no longer match the live kit.
5. Cross-check combo families with at least one independent guide/catalog and document matchup or teamfight decision gates.
6. Define player-cooperation rules: selected target, manual casts, AA/channel preservation, cursor agreement, and which inputs remain player-owned.
7. Publish scenario names in controller metadata and document runtime-only telemetry gaps.
8. Run standalone mechanics tests where applicable and rebuild `Release|x64` after catalog integration.
