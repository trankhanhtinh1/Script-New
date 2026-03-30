# Event System — Dependency & Parity Notes (Updated)

## Parity Status: 14/15 events matching EnsoulSharp (93%)

## Đã hoàn thành ✅

| NightSharp Event | EnsoulSharp Event | File | Core API dùng |
|---|---|---|---|
| `SpellCast::OnProcessSpellCast` | `AIBaseClient.OnProcessSpellCast` | SpellCastTracker.h | `GetActiveSpellCast()` |
| `SpellCast::OnDoCast` | `AIBaseClient.OnDoCast` | SpellCastTracker.h | `GetActiveSpellCast()` |
| `SpellCast::OnStopCast` | `Spellbook.OnStopCast` | SpellCastTracker.h | `GetActiveSpellCast()` |
| `Path::OnNewPath` | `AIBaseClient.OnNewPath` | PathTracker.h | `GetPathEnd()`, `CopyWaypoints()`, `IsDashing()` |
| `ObjectTracker::OnAssign` | `GameObject.OnAssign` | ObjectTracker.h | `ObjectManager::*()` list diff |
| `ObjectTracker::OnDelete` | `GameObject.OnDelete` | ObjectTracker.h | `ObjectManager::*()` list diff |
| `PropertyTracker::OnIntegerPropertyChange` | `GameObject.OnIntegerPropertyChange` | PropertyTracker.h | `GetActionState()` |
| `Events::OnLoad` | `Events.OnLoad` | Load.h | N/A |
| `Dash::OnDash` | `Events.OnDash` | Dash.h | `IsDashingOnPath()` |
| `Stealth::OnStealth` | `Events.OnStealth` | Stealth.h | `IsVisible()` |
| `Teleport::OnTeleport` | `Events.OnTeleport` | Teleport.h | `IsRecalling()`, `HasBuff()` |
| `Turret::OnTurretAttack` | `Events.OnTurretAttack` | Turret.h | `GetActiveSpellCast()` |
| `Gapcloser::OnGapcloser` | `Events.OnGapCloser` | AntiGapcloser.h | `GetActiveSpellCast()` + DB |
| `Interrupter::OnInterruptableTarget` | `Events.OnInterruptableTarget` | Interrupter.h | `GetActiveSpellCast()` + DB |

## Chưa hoàn thành ❌

| EnsoulSharp Event | Lý do | Giải pháp |
|---|---|---|
| `AIBaseClient.OnPlayAnimation` | **Core chưa có animation offset** | Cần thêm animation data offset vào `Offsets.generated.h` |

## Core Dependencies — TẤT CẢ ĐÃ CÓ SẴN

Không cần bổ sung core cho bất kỳ event nào đã implement:

| Core API | File | Dùng bởi |
|---|---|---|
| `CoreSpellCastInfo::CastRef` | core/CoreSpellCastInfo.h | SpellCastTracker, Gapcloser, Interrupter, Turret |
| `ObjectRef::GetActiveSpellCast()` | core/CoreObjects.h:304 | SpellCastTracker, Gapcloser, Interrupter, Turret |
| `ObjectRef::GetActionState()` | core/CoreObjects.h:236 | PropertyTracker |
| `CoreAi::GetPathEnd/CopyWaypoints/IsDashing` | core/CoreAi.h | PathTracker, Dash |
| `ObjectManager::Heroes/Minions/Turrets/Missiles` | sdk/Core/Objects.h | ObjectTracker |

## 1 event cần bổ sung core

### OnPlayAnimation
- **Cần offset**: Animation name / animation hash từ game object
- **Vị trí cần thêm**: `Offsets.generated.h` → `namespace AnimationLayout { constexpr auto CurrentAnimation = ???; }`
- **Core wrapper cần thêm**: `CoreObjects.h` → `ObjectRef::GetCurrentAnimation(char* out, int maxOut)`
- **Sau khi core có**: tạo `Events/AnimationTracker.h` theo cùng pattern poll-based

## Lưu ý cho các module khác

### Orbwalker (Wrappers/Orbwalking/Orbwalker.h)
- Subscribe `Events::SpellCast::OnDoCast` → detect AA completion (`args.IsAutoAttack`)
- Subscribe `Events::SpellCast::OnProcessSpellCast` → AA animation start = BeforeAttack
- `args.IsSpecialAttack` → detect AA reset (Vayne Q, Jax W)

### Prediction (Math/Prediction.h)
- Subscribe `Events::Path::OnNewPath` → track path history for UnitTracker
- `NewPathEventArgs.IsDash` → accurate dash prediction

### Gapcloser / Interrupter
- Có thể chuyển sang subscribe `OnProcessSpellCast` thay vì tự poll
- Hiện tại vẫn self-poll, hoạt động đúng
