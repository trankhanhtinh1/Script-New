# Báo Cáo Phân Tích NightSharp 26.7

> Repository ID (mapcode): `0576b920-7665-5602-946c-91c6b552a63a` — 335 files, 4066 functions, 59332 calls.
> UI: http://127.0.0.1:8765

## Tổng Quan

13 vấn đề chia thành 5 nhóm, ưu tiên theo mức độ ảnh hưởng.

| Legend | Nghĩa |
| --- | --- |
| ✅ | Fixed & verified |
| 🔧 | In progress |
| 🟡 | Ack / plan approved, chưa code |
| ⏳ | Pending — chưa touch |

---

## Nhóm 1 — Tính Năng Hỏng / Stub (Critical)

| # | Status | Issue | File | Fix note |
| --- | --- | --- | --- | --- |
| 1.1 | ✅ | `HasItem()` là stub luôn trả về `false` → item on-hit damage = 0 | `sdk/Core/Objects.h:458-493` | Catalog + unique-buff fallback (`ItemCatalog::GetInternal`, `ItemCatalog::GetUniqueBuffFor`) đã wired |
| 1.2 | ✅ | `IsInvulnerable()` bị comment out → target champ đang Kayle R / Trynd R / Zhonya's | `core/CoreObjects.h:467-486, 629-633` | Thay raw mem read bằng `HasBuffType(17)` Invulnerability + `HasBuffType(15)` SpellImmunity + re-enable check trong `IsValidTarget` |
| 1.3 | ✅ | `IsStructure()` chỉ fallback `IsTurret()` — native `IsBuilding` stub (`xor al,al; ret`) | `core/RuntimeAPI.h:229-271` | Thêm `IsInhibitor` (prefix "Barracks") + `IsNexus` (prefix "HQ") + compose `IsStructure = IsTurret \|\| IsInhibitor \|\| IsNexus` |
| 1.4 | ✅ | `OrbwalkerPlugin::GetStructure()` TODO bỏ EnemyInhibitors/EnemyNexus | `plugins/core/OrbwalkerPlugin.h:1076-1101` | Always-on (mandatory end-game behavior) — không có menu toggle, chỉ turret còn toggle |

---

## Nhóm 2 — Offset Sai / Không Nhất Quán

| # | Status | Issue | File | Fix note |
| --- | --- | --- | --- | --- |
| 2.1 | ⏳ | Dual `Offsets.generated.h` (root + `Nightsharp/core/`) | `NightSharp 26.7/` root | Xóa bản root — giữ bản duy nhất trong `core/` |
| 2.2 | ✅ | `ItemRuntime` chain cũ chết — chain mới `All::ItemList=0x4DB8` (`InventoryComponent`) | `core/Offsets.generated.h:362` | Fixed cùng `HasItem` — legacy aliases (`ItemList/SlotInfo/InfoData`) vẫn giữ cho source compat |
| 2.3 | ✅ | `MySpellState` mismatch: legacy `0x1DA7FC8` (0 xrefs) vs generated `0x1DDDDD8` (9 xrefs) | `core/Offsets.h:38-41` | IDA-verified generated đúng — legacy cũ route về `GameRuntime::MySpellState` |
| 2.4 | � | Spell chain verify | `core/Offsets.generated.h:163-219` | CE runtime + IDA verified (2026-04-17). Direct-field offsets OK (SpellBookOffset, SpellSlotArray, SlotCooldown, SlotSpellInfo, InfoSpellData, DataResourceBase). **`Res{CastRange,MissileSpeed,LineWidth}=0x478/0x518/0x568` CONFIRMED STALE** — game dùng indexed-getter dispatch (`GetFloatParam(resource, paramIdx)`), param enums `CASTRANGE=0x0F / LINEWIDTH=0x1D / MISSILESPEED=0x27` từ `sub_339430`. Runtime CE đọc `[resource+0x518]` = ASCII `"Stealth\0RevealSpelpSuppresser"` (tag strings), `[resource+0x478]` = function pointer, không phải float. **Impact LOW** vì plugin code supply static `Range/MissileSpeed/Width` qua `Spell` ctor từ `SpellDatabaseData.generated.h`; `CoreSpellBook::Get{CastRange,MissileSpeed,LineWidth}` trả garbage nhưng hiếm khi được gọi. **Fix**: deprecate 3 getters đó hoặc reimplement qua indexed getter (require finding `GetFloatParam` function + calling convention). |

---

## Nhóm 3 — Anti-Cheat Detection Surface (Critical)

| # | Status | Issue | File | Fix note |
| --- | --- | --- | --- | --- |
| 3.1 | ⏳ | **KeyAuth DISABLED** — license check không enforce | `dllmain.cpp:150-207` | Uncomment `/* ... */` bọc KeyAuth |
| 3.2 | ⏳ | `Beep(800,200)`, `SetWindowTextA(hGame, "...[NightSharp]")`, `CreateFile("C:\\nightsharp_injected.txt")` trong ManualMapEntry | `dllmain.cpp` | Xóa sạch — IOC rõ ràng cho Vanguard |
| 3.3 | ⏳ | 7 file marker hardcoded `C:\Users\Public\ns_*.txt` + `nightsharp_*.txt` | toàn source | Grep + bọc `#ifdef DEBUG_TRACE` hoặc xóa |

---

## Nhóm 4 — Memory Safety / Concurrency

| # | Status | Issue | File | Fix note |
| --- | --- | --- | --- | --- |
| 4.1 | ✅ | ~~CoreEventHook length disassembler thô sơ~~ | `core/CoreEventHook.h` | Đã rewrite thành ShadowVMT — không còn length disassembler |
| 4.2 | ✅ | ~~Install/Uninstall không có mutex~~ | `core/CoreEventHook.h` | Single-caller ở SDK bootstrap, không race — đơn giản hóa khi rewrite |
| 4.3 | ✅ | ~~`__except(1)` cleanup thiếu NOP fill~~ | `core/CoreEventHook.h` | Không còn inline detour patch game code |
| 4.4 | ✅ | `HkOnStopCast` pass `sender=0` → SDK tự resolve | (đã xóa khỏi CoreEventHook) | Không còn relevant — StopCast poll qua SpellCastTracker |
| 4.5 | ✅ | `ClampRank(6)` out-of-bounds — spell arrays dimensioned rank 0..5 | `core/CoreSpellBook.h:47-55` | Cap `rank > 5` → `5` (maximum real array slot) |
| 4.6 | ✅ | `SetInputData` ghi 3 Vec3 không có offset mapping | `core/CoreSpellBook.h:183-196`, `core/CoreControl.h:627-665` | IDA byte-pattern scan xac nhận `+0x30/+0x3C` được game read → thêm `InputEndPos2=0x30`, `InputEndPos3=0x3C` + thay raw arithmetic bằng named constants |

---

## Nhóm 5 — Code Quality / Port Bugs

| # | Status | Issue | File | Fix note |
| --- | --- | --- | --- | --- |
| 5.1 | ✅ | XerathPlugin `EDamage` đánh chỉ số `R.Level` (R max rank 3 → E damage cap); buff `"Undying Rage"` (có space) | `plugins/champions/XerathPlugin.h:239-246, 394-401` | `R.Level` → `E.Level`; `"Undying Rage"` → `"UndyingRage"` |
| 5.2 | ✅ | `IsCastingImporantSpell` typo | `sdk/Core/Objects.h:622` | Rename → `IsCastingImportantSpell` (0 callers) |
| 5.3 | ✅ | `GetSpellSlot` loop `<= SpellSlot::Unknown` | `sdk/Core/Objects.h:560` | Đổi `<=` → `<` — không iterate sentinel value (14) |
| 5.4 | ✅ | `ReadStdString` SSO `> 15` vs `ReadRiotString` `>= 16` | `core/Globals.h:80` | Thống nhất `>= 16` |

---

## Khuyến Nghị Ưu Tiên

1. **CRITICAL** — Bật lại KeyAuth + xóa Beep/SetWindowText/CreateFile trong `dllmain.cpp` (Nhóm 3.1, 3.2).
2. **CRITICAL** — Port lại `CoreItem.h` với chain mới + implement `ObjectRef::HasItem()` (1.1 ✅, 2.2 còn).
3. **HIGH** — Fix `IsInvulnerable` bằng buff-based fallback (1.2 🔧).
4. **HIGH** — Audit native predicate `IsVulnerable/IsFleeing/IsBuilding/IsSelectable/IsNoRender` trong IDA xem còn nào là stub.
5. **HIGH** — Thống nhất offset `ItemRuntime` ↔ `All::ItemList`; xóa `Offsets.generated.h` root.
6. **MEDIUM** — Thay length disassembler bằng hde64/Zydis nếu có nhu cầu inline detour lại (hiện tại đã loại bỏ — 4.1~4.3 ✅).
7. **LOW** — Bọc toàn bộ `WriteStage`/`DbgLog` trong `#ifdef DEBUG` hoặc xóa (3.3).

---

## Lịch Sử Fix

| Ngày | Issue | Hạng mục |
| --- | --- | --- |
| 2026-04-17 | `CoreEventHook` rewrite: xóa inline detour fail, chỉ giữ ShadowVMT OnProcessSpell | 4.1, 4.2, 4.3 |
| 2026-04-17 | `HasItem()` fixed (catalog + unique-buff fallback) + `ItemRuntime` chain mới `InventoryComponent=0x4DB8` | 1.1, 2.2 |
| 2026-04-17 | `IsInvulnerable()` + `IsStructure()` + `OrbwalkerPlugin::GetStructure()` | 1.2, 1.3, 1.4 |
| 2026-04-17 | `MySpellState` legacy `0x1DA7FC8` (stale) → `GameRuntime::MySpellState=0x1DDDDD8` (IDA: 9 xrefs) | 2.3 |
| 2026-04-17 | Spell chain: direct offsets IDA-verified; `SpellDataResourceLayout` flagged stale (indexed getter, cần CE) | 2.4 partial |
| 2026-04-17 | Xerath EDamage R→E + buff `"UndyingRage"`; typo `IsCastingImportantSpell`; `GetSpellSlot` loop off-by-one; SSO boundary unified | 5.1, 5.2, 5.3, 5.4 |
| 2026-04-17 | `ClampRank(6)`→5; `SetInputData` raw arithmetic → named `InputEndPos2/3=0x30/0x3C` (IDA byte-pattern verified) | 4.5, 4.6 |
