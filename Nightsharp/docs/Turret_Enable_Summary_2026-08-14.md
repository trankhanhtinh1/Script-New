# Enable Turret Object Support - 2026-08-14

## Tổng quan
Đã kích hoạt lại Turret object support trong NightSharp sau khi bị disable trước đó theo yêu cầu của user.

## Các file đã sửa

### Core Layer
1. **Nightsharp/core/CoreObjectManager.h**
   - Enable `ManagerKind::Turrets` → trả về `ctx.turretManager`
   - Bỏ filter loại bỏ `AITurretClient` trong enumeration
   - Enable `EnumerateTurrets()` function
   - Enable Turret type inference trong `InferType()`

2. **Nightsharp/core/CoreObjects.h**
   - Uncomment `ObjectType::AITurretClient` trong `IsAttackable()`
   - Uncomment `ObjectType::AITurretClient` trong `IsAIBase()`
   - Uncomment `ObjectType::AITurretClient` trong `IsLifecycleType()`
   - Uncomment return "AITurretClient" trong `TypeName()`

### SDK Layer
3. **Nightsharp/sdk/GameObjects/GameObjects.h**
   - Enable `TurretsList`, `AllyTurretsList`, `EnemyTurretsList` vectors
   - Enable `CleanInvalid()` cho Turret lists
   - Enable Turret classification trong `OnProcessObject()`
   - Enable Turret cleanup trong `OnObjectDelete()`
   - Enable Turret clear trong `Clear()`
   - Enable public API: `Turrets()`, `AllyTurrets()`, `EnemyTurrets()`
   - Enable `IsUnderAllyTurret()` method
   - Enable `IsUnderEnemyTurret()` method
   - Enable Turret trong `Get<T>()` template

### Plugins Layer
4. **Nightsharp/plugins/Champion/KuroAIO/AI/AIChampionEngine.h**
   - Enable `UnderEnemyTurret()` helper function

5. **Nightsharp/plugins/Champion/KuroAIO/AI/Champions/Akshan/AIAkshanController.h**
   - Enable Turret blocker detection trong R skill

6. **Nightsharp/plugins/Champion/KuroAIO/AI/Champions/Alistar/AIAlistarController.h**
   - Enable Turret positioning trong displacement logic

7. **Nightsharp/plugins/Champion/KuroAIO/AI/Champions/Bard/AIBardController.h**
   - Enable Turret trong stasis unit collection
   - Enable Turret dive logic trong R plan

8. **Nightsharp/plugins/Champion/KuroAIO/AI/Champions/Blitzcrank/AIBlitzcrankController.h**
   - Enable `UnderAlliedTurret()` helper function

9. **Nightsharp/plugins/Champion/KuroAIO/AI/Champions/Ryze/AIRyzeController.h**
   - Enable Turret proximity check trong rune prison logic

## Xác minh bằng Cheat Engine và ReClass
- Turret Manager global address: `League of Legends.exe+0x1EFA390`
- Live manager: `0x1D555141440`
- Manager layout: items `+0x08`, count `+0x10`; live count `24`
- Live Turret vtable: `League of Legends.exe+0x1A43C38`
- Object identity: Index `+0x20`, NetworkId `+0xBC`
- ObjectType enum: `AITurretClient = 4`
- Team split trong session kiểm tra: 12 ally / 12 enemy

## Sửa regression orbwalker
- Turret được seed từ typed Turret manager riêng.
- Turret bị loại khỏi general `GameObjectsList`, `AttackableUnitsList`,
  `AllyList` và `EnemyList`, giữ nguyên đường dữ liệu Hero/Minion legacy.
- Sửa constructor `AITurretClient` để giữ address/handle thay vì tạo object rỗng.
- SDK và Kuro orbwalker chỉ chọn Turret trong LaneClear/farm path; không chọn
  structure trong Combo.
- Turret `_P0_` là shrine/fountain, live `IsTargetable=0`, luôn bị loại khỏi
  target selection.
- Live `AttackRange` của Turret bằng 0 trên build này; vùng under-turret dùng
  fallback 775 trước khi cộng bounding radius.

## Chức năng đã kích hoạt
✅ Turret enumeration và tracking
✅ Ally/Enemy Turret lists
✅ Turret validation và cleanup
✅ `IsUnderAllyTurret()` / `IsUnderEnemyTurret()` methods
✅ `UnderEnemyTurret()` / `UnderAlliedTurret()` helpers
✅ Turret blockers trong skill targeting
✅ Turret dive logic
✅ Turret proximity checks

## Ghi chú
- Inhibitor và Nexus vẫn bị disable theo yêu cầu ban đầu
- Shop objects vẫn bị disable
- Turret manager global: `0x1EFA390`; Turret vtable RVA: `0x1A43C38`
  (patch-specific, cần re-verify khi game cập nhật)
