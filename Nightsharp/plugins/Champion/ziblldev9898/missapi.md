# missapi - ziblldev9898 (Locke)

Ghi lai cac API can nhung SDK chua bocc (theo skill port-champion, rule 4).

## Locke

### Liet ke TAT CA buff (chua biet ten) tren 1 unit
- **Can de:** detect ten buff dem so Chu Dinh tren Locke + buff dinh len dich khi trung Chu Dinh. Locke la champion custom nen chua biet truoc ten buff.
- **SDK hien co:** chi `AIBaseClient::HasBuff(name)` va `AIBaseClient::GetBuffCount(name)` -> deu YEU CAU biet ten buff truoc. Khong co wrapper "list all buffs".
- **Da search:** `Buffs`, `GetBuffs`, `EnumerateBuffs`, `BuffList` trong `SDK/` -> khong co. Chi tim thay `SDK::Extensions`/`Objects.h` cac ham theo-ten.
- **Giai phap dang dung:** Core `CoreBuffs::Enumerate(address, out[], max)` + `CoreBuffs::BuffRef::ReadName/GetStacks/GetType/GetEndTime/GetRemainingTime`. Day la cach chinh chu (first-party) `plugins/Core/PlayerBuffDebugPlugin.h` dang dung, khong phai API tu bia.
- **Trang thai:** OK (dung Core API co san). Khi SDK bo sung wrapper enumerate buff thi doi sang SDK.
- **Cho dung trong code:** `plugins/Champion/ziblldev9898/Locke.h` -> `ReadUnitBuffs()`.

### Ghi chu spell metadata (da verify CDragon 2026-07-08)
- Locke id=805, alias="Locke" (champion-summary.json)
- Spell names tu game runtime: LockeQ, LockeW, LockeE, LockeR
- CDragon .bin.json: `game/data/characters/locke/locke.bin.json`
- CastRange: Q=950, W=250, E=425, R=1000
- Q missile: width=60, speed=1650, castTime=0.25, ammo=3, targeting=Direction(Line)
- W: Self targeting, canCastWhileCC=yes
- E: LocationClamped, castTime=0.175, child spells: LockeE(teleport 425), LockeEDashOut(dash 475), LockeEAttack(empowered AA)
- R: Location(Circle), castRadius=425, missileTravelTime=0.5s, castTime=0.25

### Buff names (da verify CDragon locke.bin.json)
| Buff name | Loai | Mo ta |
|---|---|---|
| LockeQ | Debuff enemy | Q mark (Soul Nails), max 3 stacks, duration 4s |
| LockeQSlow | Debuff enemy | Q slow 25/25/60%, 1/1/2s |
| LockeQNailsReady | Buff self | Q ammo ready |
| LockeW | Buff self | W active: AS+MS, heal, 6s |
| LockeEAttackReady | Buff self | E empowered attack ready |
| LockeRStack | Buff self (permanent) | R execute threshold stacks (+0.5% each) |
| LockeRSlow | Debuff enemy | R slow 99%, 2s |
| LockeRExecuteTracker | Buff enemy | R execute mark, 5s |

### Bug fix: expired buff not resetting to 0
- **Nguyen nhan:** `ReadUnitBuffs` goi `CoreBuffs::Enumerate` (tra ve tat ca entry trong buff array) nhung KHONG goi `buff.IsActive(gameTime)` truoc khi include. `GetStacks()` fallback sang `BuffStacksAlt` (+0x3C) ma game KHONG clear khi remove -> buff het han van hien voi stack > 0.
- **Fix:** Them `if (!buff.IsActive(gameTime)) continue;` trong `ReadUnitBuffs` truoc khi doc stacks/remain. `IsActive` check raw `BuffStacks` (+0x38) > 0 AND `endTime > gameTime` (hoac permanent).
- **Cho dung trong code:** `Locke.h:124-130`
