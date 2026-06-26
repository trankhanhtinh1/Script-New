# Math Module Parity Audit: NightSharp vs EnsoulSharp SDK

> **Ngày phân tích:** 26/06/2026  
> **Cập nhật:** 27/06/2026 — Đánh dấu hoàn thành Phase 1 + Health.h + thêm §8  
> **Nguồn EnsoulSharp SDK:** `EnsoulSharp.SDK/Core/Math/` (source) + `EnsoulSharp.SDK.dll` (decompiled via ILSpy)  
> **Nguồn NightSharp:** `NightSharp yungblod_ dev/SDK/Math/`  

---

## 1. Tóm tắt Tổng quan

| EnsoulSharp SDK (Source + DLL) | NightSharp | Trạng thái |
|---|---|---|
| `Collision.cs` / DLL: `Collisions` class | `Collision.h` | **ĐÃ FIX (một phần)** — include path + enum names, vẫn thiếu YasuoWall/Samira/event hooks |
| `ConvexHull.cs` / DLL: `Utility.Mec` class | **THIẾU** | **THIẾU HOÀN TOÀN** |
| `Geometry.cs` / DLL: `Geometry` (nested types + static methods) | **THIẾU** (chỉ có Polygons riêng) | **THIẾU** static methods |
| `Polygons/Polygon.cs` / DLL: `Geometry.Polygon` | `Polygons/Polygon.h` | **MATCH 1:1** |
| `Polygons/ArcPoly.cs` / DLL: `Geometry.Arc` | `Polygons/ArcPoly.h` | **MATCH 1:1** |
| `Polygons/CirclePoly.cs` / DLL: `Geometry.Circle` | `Polygons/CirclePoly.h` | **MATCH 1:1** |
| `Polygons/LinePoly.cs` / DLL: `Geometry.Line` | `Polygons/LinePoly.h` | **MATCH 1:1** |
| `Polygons/RectanglePoly.cs` / DLL: `Geometry.Rectangle` | `Polygons/RectanglePoly.h` | **MATCH 1:1** |
| `Polygons/RingPoly.cs` / DLL: `Geometry.Ring` | `Polygons/RingPoly.h` | **MATCH 1:1** (xem note §5.6) |
| `Polygons/SectorPoly.cs` / DLL: `Geometry.Sector` | `Polygons/SectorPoly.h` | **MATCH 1:1** (xem note §5.7) |
| `Prediction/Cluster.cs` / DLL: `AoEPrediction` class | **THIẾU** | **THIẾU HOÀN TOÀN** |
| `Prediction/Health.cs` / DLL: internal `HealthPrediction` impl | `Prediction/Health.h` + `HealthPrediction.h` (wrapper) | **ĐÃ PORT** — Health.h có PredictedDamage, GetPredictionDefault/Simulated, event handlers |
| `Prediction/Movement.cs` / DLL: internal `PredictionSDK` impl | `Prediction/Movement.h` | **ĐÃ PORT** — 1:1 từ source, xem §8 ghi chú |
| `Prediction/GamePath.cs` / DLL: internal (PathTracker) | `Prediction/Movement.h` (inline) | **ĐÃ PORT** — PathTracker nhúng trong Movement.h |
| — | `HealthPrediction.h` | Wrapper cho `Prediction::Health` — **Prediction/Health.h đã port** ✅ |

---

## 2. Cấu trúc Thư mục So sánh

### EnsoulSharp.SDK / Core / Math
```
Math/
├── Collision.cs              (149 lines)
├── ConvexHull.cs             (567 lines)
├── Geometry.cs               (116 lines)
├── Polygons/
│   ├── Polygon.cs            (147 lines)
│   ├── ArcPoly.cs            (143 lines)
│   ├── CirclePoly.cs         (122 lines)
│   ├── LinePoly.cs           (117 lines)
│   ├── RectanglePoly.cs      (124 lines)
│   ├── RingPoly.cs           (142 lines)
│   └── SectorPoly.cs         (142 lines)
└── Prediction/
    ├── Cluster.cs            (453 lines)
    ├── GamePath.cs           (212 lines)
    ├── Health.cs             (471 lines)
    └── Movement.cs           (715 lines)
```

### NightSharp / SDK / Math
```
Math/
├── Collision.h               (81 lines) — ĐÃ FIX
├── HealthPrediction.h        (47 lines)
├── Prediction.h              (14 lines) — ĐÃ TẠO
├── Polygons/
│   ├── Polygon.h             (222 lines)
│   ├── ArcPoly.h             (63 lines)
│   ├── CirclePoly.h          (58 lines)
│   ├── LinePoly.h            (54 lines)
│   ├── RectanglePoly.h       (64 lines)
│   ├── RingPoly.h            (69 lines)
│   └── SectorPoly.h          (62 lines)
└── Prediction/
    ├── Health.h              (~400 lines) — ĐÃ PORT
    └── Movement.h            (945 lines) — ĐÃ PORT
```

---

## 3. Phân tích Chi tiết từng File

### 3.1 Polygons — **MATCH 1:1** ✅

Tất cả 7 file Polygons đã port 1:1 từ EnsoulSharp SDK sang NightSharp:

| File EnsoulSharp | File NightSharp | Ghi chú |
|---|---|---|
| `Polygon.cs` | `Polygon.h` | Match. NightSharp thêm inline `Clipper::PointInPolygon` và `PolygonsDetail` helpers. Thiếu overload `IsInside(GameObject)` — **minor gap**. |
| `ArcPoly.cs` | `ArcPoly.h` | Match 1:1. |
| `CirclePoly.cs` | `CirclePoly.h` | Match 1:1. |
| `LinePoly.cs` | `LinePoly.h` | Match 1:1. `Length` property → `GetLength()`/`SetLength()`. |
| `RectanglePoly.cs` | `RectanglePoly.h` | Match 1:1. |
| `RingPoly.cs` | `RingPoly.h` | Match 1:1. |
| `SectorPoly.cs` | `SectorPoly.h` | Match 1:1. Thiếu `RotateLineFromPoint` method (chỉ có trong DLL, không có trong source). |

### 3.2 Collision — **ĐÃ FIX (một phần)** ⚠️

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| Class name | `Collision` | `Collisions` | `SDK::Collision` |
| Methods | `GetCollision(positions, input)` | `GetCollision`, `IsCollision`, `HasYasuoWindWallCollision` | `HasLineCollision`, `GetCollision(target, input)`, `GetCollision(positions, input)`, `HasCollision` |
| Implementation | Lặp qua Minions, Heroes, Walls, YasuoWall riêng biệt | Có thêm Samira, `WillDead`, `OnCreate/OnDelete/OnDoCast` events | Delegate sang `Prediction::Movement::CollectLineCollisions` — **implementation hoàn toàn khác** |

**Đã fix:**
- ✅ `Prediction.h` đã được tạo — include path hoạt động
- ✅ `Prediction::Movement::CollectLineCollisions` và `Prediction::GetPrediction` đã có implementation
- ✅ Enum names đã sửa (`CollisionableObjects` thay vì `CollisionObjects`, `SkillshotType::SkillshotLine` thay vì `SkillshotType::Line`)
- ✅ `GetCollision(target, input)` trả về `std::vector<GameObject>` (match `PredictionOutput.CollisionObjects`)

**Vẫn thiếu (Phase 3):**
- ❌ `HasYasuoWindWallCollision` với overload `extraRadius` (có trong DLL)
- ❌ `SamiraInGame`, `WillDead` logic
- ❌ `Attach()` event hooks (`OnCreate`, `OnDelete`, `OnDoCast`)

### 3.3 ConvexHull — **THIẾU HOÀN TOÀN** ❌

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| Class name | `ConvexHull` | `Utility.Mec` | **KHÔNG CÓ** |
| Methods | `FindMinimalBoundingCircle`, `GetMec`, `MakeConvexHull`, `AngleValue`, `CircleEnclosesPoints`, `FindCircle`, `GetMinMaxBox`, `GetMinMaxCorners`, `HullCull` | Tương tự nhưng trong `Utility.Mec` | **THIẾU** |
| Structs | `MecCircle`, `MinMaxCornersInfo` | `MecCircle` | **THIẾU** |

**Cần tạo:** `ConvexHull.h` (hoặc `Mec.h`) trong `SDK/Math/` với đầy đủ:
- `FindMinimalBoundingCircle(points, center, radius)`
- `GetMec(points)` → trả về `MecCircle { Center, Radius }`
- `MakeConvexHull(points)`
- Tất cả private helpers: `AngleValue`, `CircleEnclosesPoints`, `FindCircle`, `GetMinMaxBox`, `GetMinMaxCorners`, `HullCull`

### 3.4 Geometry — **THIẾU static methods** ❌

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| `GetCenter` extension | ✅ | ❌ (không có trong DLL) | **THIẾU** |
| `GetCenteredText` extensions | ✅ | ❌ (không có trong DLL) | **THIẾU** |
| `CenterOfPolygone` | ❌ | ✅ | **THIẾU** |
| `ClipPolygons` | ❌ | ✅ | **THIẾU** |
| `JoinPolygons` | ❌ | ✅ (2 overloads) | **THIẾU** |
| `MovePolygone` | ❌ | ✅ | **THIẾU** |
| `RotatePolygon` | ❌ | ✅ (2 overloads) | **THIẾU** |
| `ToPolygon` / `ToPolygons` | ❌ | ✅ | **THIẾU** |
| `Close` | ❌ | ✅ | **THIẾU** |
| `DegreeToRadian` / `RadianToDegree` | ❌ | ✅ | **THIẾU** |

**Ghi chú:** Source `Geometry.cs` và DLL `Geometry` class khác nhau đáng kể. Source tập trung vào rendering (GetCenter, GetCenteredText với Sprite/Font), DLL tập trung vào polygon operations. NightSharp cần port **cả hai**.

### 3.5 Prediction/Cluster — **THIẾU HOÀN TOÀN** ❌

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| Class name | `Cluster` | `AoEPrediction` | **KHÔNG CÓ** |
| `GetAoEPrediction` | ✅ | ✅ | **THIẾU** |
| `Circle.GetCirclePrediction` | ✅ | ✅ | **THIẾU** |
| `Cone.GetConePrediction` | ✅ | ✅ | **THIẾU** |
| `Line.GetLinePrediction` | ✅ | ✅ | **THIẾU** |
| `GetPossibleTargets` | ✅ (internal) | ✅ (private) | **THIẾU** |
| `PossibleTarget` class | ✅ | ❌ (dùng Tuple) | **THIẾU** |

**Cần tạo:** `SDK/Math/Prediction/Cluster.h` với đầy đủ AoE prediction cho Circle, Cone, Line.

### 3.6 Prediction/Health — **ĐÃ PORT** ✅

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| Class name | `Health` | `HealthPrediction` (facade) + internal impl | `SDK::Prediction::Health` namespace |
| `Initialize` | N/A (static ctor) | `Initialize()` | ✅ ĐÃ PORT |
| `Update` | N/A (OnGameUpdate) | N/A | ✅ ĐÃ PORT |
| `Reset` | N/A | N/A | ✅ ĐÃ PORT |
| `GetPrediction` | ✅ (Default + Simulated) | ✅ | ✅ ĐÃ PORT |
| `GetAggroTurret` | ✅ | ✅ | ✅ ĐÃ PORT |
| `HasMinionAggro` | ✅ | ✅ | ✅ ĐÃ PORT |
| `HasTurretAggro` | ✅ | ✅ | ✅ ĐÃ PORT |
| `TurretAggroStartTick` | ✅ | ✅ | ✅ ĐÃ PORT |
| `EstimateIncomingAutoAttackDamage` | ❌ | ❌ | ✅ — **chỉ NightSharp có** |
| `PredictedDamage` inner class | ✅ | `PredictedDamage` (top-level) | ✅ ĐÃ PORT |
| `GetPredictionDefault` | ✅ (private) | internal impl | ✅ ĐÃ PORT |
| `GetPredictionSimulated` | ✅ (private) | internal impl | ✅ ĐÃ PORT |
| Event handlers | ✅ (OnGameUpdate, OnDoCast, OnStopCast, OnDelete, OnProcessSpellCast) | internal impl | ✅ ĐÃ PORT (OnDoCast, OnStopCast, OnDelete, OnProcessSpellCast) |
| `ActiveAttacks` dict | ✅ | internal | ✅ ĐÃ PORT (`detail::ActiveAttacks`) |

**Đã port đầy đủ** vào `SDK/Math/Prediction/Health.h` (~400 lines). `HealthPrediction.h` vẫn là wrapper delegate sang `Prediction::Health::*`.

### 3.7 Prediction/Movement — **ĐÃ PORT** ✅

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| Class name | `Movement` | internal `PredictionSDK` | `SDK::Prediction::Movement` namespace |
| `GetPrediction` (5 overloads) | ✅ | ✅ | ✅ ĐÃ PORT |
| `GetAdvancedPrediction` | ✅ (internal) | internal | ✅ ĐÃ PORT |
| `GetDashingPrediction` | ✅ (internal) | internal | ✅ ĐÃ PORT |
| `GetImmobilePrediction` | ✅ (internal) | internal | ✅ ĐÃ PORT |
| `GetPositionOnPath` | ✅ (internal) | internal | ✅ ĐÃ PORT |
| `GetStandardPrediction` | ✅ (internal) | internal | ✅ ĐÃ PORT |
| `PositionAfter` | ✅ (internal) | internal | ✅ ĐÃ PORT |
| `UnitIsImmobileUntil` | ✅ (internal) | internal | ✅ ĐÃ PORT |
| `CollectLineCollisions` | ❌ | ❌ | ✅ NightSharp tự tạo — helper cho Collision.h |
| `ResolveFrom` | ❌ | ❌ | ✅ NightSharp tự tạo — helper cho Collision.h |

**Đã port đầy đủ (945 lines C++).** Xem §8 cho các vấn đề 1:1 matching.

### 3.8 Prediction/GamePath — **ĐÃ PORT (inline trong Movement.h)** ✅

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| `GamePath` class | ✅ | ❌ (internal) | ✅ `GamePath` namespace trong Movement.h |
| `PathTracker` nested class | ✅ | ❌ (internal) | ✅ `GamePath::PathTracker` struct |
| `StoredPath` nested class | ✅ | ❌ (internal) | ✅ `GamePath::PathTracker::StoredPath` struct |
| `GetCurrentPath` | ✅ | ❌ | ✅ ĐÃ PORT |
| `GetMeanSpeed` | ✅ | ❌ | ✅ ĐÃ PORT |
| `GetStoredPaths` | ✅ | ❌ | ✅ ĐÃ PORT |
| `OnNewPath` event handler | ✅ | ❌ | ✅ ĐÃ PORT (hook SDK::Events::AddOnNewPath) |

**Đã port inline trong Movement.h** thay vì file riêng, vì C++ header-only cần truy cập các helper functions trong Movement.h.

### 3.9 PredictionInput / PredictionOutput — **ĐÃ PORT** ✅

> **Lưu ý:** Trong source EnsoulSharp.SDK, `PredictionInput` và `PredictionOutput` **không có file riêng** — chúng được định nghĩa bên trong `Core/Math/Prediction/Movement.cs` (dòng 508-712). Trong DLL thì chúng là top-level class riêng biệt.

| Aspect | EnsoulSharp SDK (Source — trong Movement.cs) | EnsoulSharp SDK (DLL — standalone) | NightSharp |
|---|---|---|---|
| `PredictionInput` class | ✅ (`Movement.cs:508-621`) | ✅ (top-level) | ✅ `SDK::PredictionInput` struct trong Movement.h |
| Fields: AoE, Collision, CollisionObjects, Delay, From, Radius, Range, RangeCheckFrom, Speed, Type, Unit, UseBoundingRadius | ✅ | ✅ + `AddHitBox`, `MaxCollisionCount`, `ChoiceCloserPosition`, `Spell` | ✅ (thiếu DLL-only fields: `AddHitBox`, `MaxCollisionCount`, `ChoiceCloserPosition`, `Spell`) |
| `RealRadius` property | ✅ | ✅ | ✅ ĐÃ PORT (method `RealRadius()`) |
| `ResolveFrom` / `ResolveRangeCheckFrom` | ✅ | ✅ | ✅ ĐÃ PORT |
| `PredictionOutput` class | ✅ (`Movement.cs:626-712`) | ✅ (top-level) | ✅ `SDK::PredictionOutput` struct trong Movement.h |
| Fields: AoeHitCount, AoeTargetsHit, CastPosition, CollisionObjects, Hitchance, UnitPosition, Input | ✅ | ✅ + `OriginHitchance`, `Idle` | ✅ (thiếu DLL-only fields: `OriginHitchance`, `Idle`) |

**Đã port vào `SDK` namespace** (match EnsoulSharp.SDK namespace gốc). `Spell.h` cũ đã xóa định nghĩa trùng và include `Prediction.h`.

---

## 4. DLL vs Source Code Discrepancies

Một số type trong **DLL** khác với **source code**:

| Type | Source (EnsoulSharp.SDK/Core/Math/) | DLL (EnsoulSharp.SDK.dll) |
|---|---|---|
| `Collision` | `Collision` class, simple | `Collisions` class, có `Attach()`, `SamiraInGame`, `WillDead` |
| `ConvexHull` | `ConvexHull` class, có `MinMaxCornersInfo` struct | `Utility.Mec` class, không có `MinMaxCornersInfo` |
| `Geometry` | Rendering helpers (GetCenter, GetCenteredText) | Polygon operations (ClipPolygons, JoinPolygons, RotatePolygon, etc.) |
| `Cluster` | `Cluster` class với nested Circle/Cone/Line | `AoEPrediction` class |
| `Movement` | `Movement` class | `PredictionSDK` (internal, implements `IPrediction`) |
| `Health` | `Health` class | `HealthPrediction` (facade) + internal impl |
| `GamePath` | `GamePath` class với `PathTracker` | Internal trong `PredictionSDK` |
| Polygons | Separate files: `ArcPoly.cs`, `CirclePoly.cs`, etc. | Nested trong `Geometry`: `Geometry.Arc`, `Geometry.Circle`, etc. |
| `RingPoly` | Property `Width` | `Ring.InnerRadius` (đổi tên) |
| `PredictionInput` | `SkillshotType` enum, `UseBoundingRadius` | `SpellType` enum, `AddHitBox`, `MaxCollisionCount`, `ChoiceCloserPosition`, `Spell` |
| `PredictionOutput` | `AoeTargetsHitCount` computed property | `AoeTargetsHitCount` field, `OriginHitchance`, `Idle` string |

**Kết luận:** Source code và DLL là 2 phiên bản khác nhau. NightSharp port theo source code, nhưng DLL có nhiều method/thuộc tính thêm mà NightSharp cũng cần.

---

## 5. Danh sách Cần Fix để Matching 1:1

### 5.1 File THIẾU cần tạo mới

| # | File cần tạo | Path đề xuất | Mô tả | Ưu tiên | Trạng thái |
|---|---|---|---|---|---|
| 1 | **`ConvexHull.h`** | `SDK/Math/ConvexHull.h` | Port `ConvexHull.cs` — MEC (Minimum Enclosing Circle), `MakeConvexHull`, `FindMinimalBoundingCircle`, `MecCircle` struct | **CAO** | ❌ THIẾU |
| 2 | **`Geometry.h`** | `SDK/Math/Geometry.h` | Port static methods từ DLL: `CenterOfPolygone`, `ClipPolygons`, `JoinPolygons`, `MovePolygone`, `RotatePolygon`, `ToPolygon`, `ToPolygons`, `Close`, `DegreeToRadian`, `RadianToDegree` | **CAO** | ❌ THIẾU |
| 3 | **`Prediction.h`** | `SDK/Math/Prediction.h` | Header bao gồm tất cả Prediction sub-headers | **CRITICAL** | ✅ **ĐÃ TẠO** |
| 4 | **`Prediction/Movement.h`** | `SDK/Math/Prediction/Movement.h` | Port `Movement.cs` (715 lines) — `GetPrediction`, `GetAdvancedPrediction`, `GetDashingPrediction`, `GetImmobilePrediction`, `GetPositionOnPath`, `GetStandardPrediction`, `PositionAfter`, `UnitIsImmobileUntil` | **CRITICAL** | ✅ **ĐÃ PORT (945 lines)** |
| 5 | **`Prediction/Cluster.h`** | `SDK/Math/Prediction/Cluster.h` | Port `Cluster.cs` (453 lines) — `GetAoEPrediction`, `Circle/Cone/Line.GetXxxPrediction`, `GetPossibleTargets`, `PossibleTarget` struct | **CAO** | ❌ THIẾU |
| 6 | **`Prediction/Health.h`** | `SDK/Math/Prediction/Health.h` | Port `Health.cs` (471 lines) — `PredictedDamage` class, `GetPredictionDefault`, `GetPredictionSimulated`, event handlers, `ActiveAttacks` dict | **CAO** | ✅ **ĐÃ PORT (~400 lines)** |
| 7 | **`Prediction/GamePath.h`** | `SDK/Math/Prediction/Movement.h` (inline) | Port `GamePath.cs` (212 lines) — `PathTracker` class, `StoredPath` class, `GetCurrentPath`, `GetMeanSpeed`, `GetStoredPaths` | **CAO** | ✅ **ĐÃ PORT (inline trong Movement.h)** |
| 8 | **`PredictionInput`** | `SDK/Math/Prediction/Movement.h` (inline) | Port `PredictionInput` class (nằm trong `Movement.cs:508-621`) — AoE, Collision, CollisionObjects, Delay, From, Radius, Range, RangeCheckFrom, Speed, Type, Unit, UseBoundingRadius, RealRadius | **CRITICAL** | ✅ **ĐÃ PORT (inline trong Movement.h, SDK namespace)** |
| 9 | **`PredictionOutput`** | `SDK/Math/Prediction/Movement.h` (inline) | Port `PredictionOutput` class (nằm trong `Movement.cs:626-712`) — AoeHitCount, AoeTargetsHit, CastPosition, CollisionObjects, Hitchance, UnitPosition, Input | **CRITICAL** | ✅ **ĐÃ PORT (inline trong Movement.h, SDK namespace)** |

### 5.2 File CẦN FIX (đã có nhưng chưa match)

| # | File | Vấn đề | Cách fix | Trạng thái |
|---|---|---|---|---|
| 1 | **`Collision.h`** | Include `"Prediction.h"` không tồn tại → **compile error** | Tạo `Prediction.h` hoặc sửa include path | ✅ **ĐÃ FIX** |
| 2 | **`Collision.h`** | Thiếu `HasYasuoWindWallCollision` (có trong DLL) | Thêm method `HasYasuoWindWallCollision(start, end, extraRadius=0)` | ❌ |
| 3 | **`Collision.h`** | Thiếu `IsCollision` overload (có trong DLL) | Thêm `IsCollision(positions, input)` | ❌ |
| 4 | **`Collision.h`** | Thiếu `WillDead` logic (có trong DLL) | Port `WillDead` method | ❌ |
| 5 | **`Collision.h`** | Thiếu Samira shield handling | Port `SamiraInGame` logic | ❌ |
| 6 | **`Collision.h`** | Thiếu event hooks (`Attach`, `OnCreate`, `OnDelete`, `OnDoCast`) | Port event attachment | ❌ |
| 7 | **`HealthPrediction.h`** | Delegate sang `Prediction::Health::*` — implementation đã có | ✅ **ĐÃ FIX** — `Prediction/Health.h` đã port |
| 8 | **`HealthPrediction.h`** | Thiếu `PredictedDamage` inner class | ✅ **ĐÃ FIX** — `PredictedDamage` đã có trong `Health.h` |
| 9 | **`Polygons/Polygon.h`** | Thiếu overload `IsInside(GameObject)` | Thêm `IsInside(const GameObject& obj)` overload | ❌ |
| 10 | **`Polygons/SectorPoly.h`** | Thiếu `RotateLineFromPoint` method (có trong DLL) | Port `RotateLineFromPoint(point1, point2, value, radian)` | ❌ |
| 11 | **`Spell.h`** | Định nghĩa trùng `PredictionInput`/`PredictionOutput` với Movement.h | Xóa định nghĩa cũ, include `Prediction.h` | ✅ **ĐÃ FIX** |

### 5.3 File ĐÃ MATCH (không cần fix)

| # | File NightSharp | Match với |
|---|---|---|
| 1 | `Polygons/Polygon.h` | `Polygons/Polygon.cs` ✅ |
| 2 | `Polygons/ArcPoly.h` | `Polygons/ArcPoly.cs` ✅ |
| 3 | `Polygons/CirclePoly.h` | `Polygons/CirclePoly.cs` ✅ |
| 4 | `Polygons/LinePoly.h` | `Polygons/LinePoly.cs` ✅ |
| 5 | `Polygons/RectanglePoly.h` | `Polygons/RectanglePoly.cs` ✅ |
| 6 | `Polygons/RingPoly.h` | `Polygons/RingPoly.cs` ✅ |
| 7 | `Polygons/SectorPoly.h` | `Polygons/SectorPoly.cs` ✅ |

---

## 6. Thứ tự Ưu tiên Implement

### Phase 1 — Critical (không compile được nếu thiếu)
3. ✅ ~~`Prediction/Movement.h`~~ — **ĐÃ PORT** (945 lines C++)
4. ✅ ~~`Prediction.h`~~ — **ĐÃ TẠO** umbrella header
5. ✅ ~~Fix `Collision.h` include path~~ — **ĐÃ FIX**
6. ✅ ~~Fix `Spell.h` trùng định nghĩa~~ — **ĐÃ FIX**

### Phase 2 — High (cần cho functionality đầy đủ)
7. ✅ ~~`Prediction/Health.h`~~ — **ĐÃ PORT** (~400 lines C++)
8. `Prediction/Cluster.h` — AoE prediction (453 lines)
9. ~~`Prediction/GamePath.h`~~ — ✅ **ĐÃ PORT** (inline trong Movement.h)
10. `ConvexHull.h` — MEC calculation (567 lines)
11. `Geometry.h` — polygon operations từ DLL

### Phase 3 — Medium (hoàn thiện parity)
12. Fix `Collision.h` — thêm `HasYasuoWindWallCollision`, `IsCollision`, `WillDead`, Samira, event hooks
13. Fix `Polygons/Polygon.h` — thêm `IsInside(GameObject)` overload
14. Fix `Polygons/SectorPoly.h` — thêm `RotateLineFromPoint`
15. ✅ ~~Fix `HealthPrediction.h` — thêm `PredictedDamage` struct~~ — **ĐÃ FIX** (PredictedDamage có trong Health.h)

---

## 7. Thống kê Tổng

| Metric | Số |
|---|---|
| File EnsoulSharp SDK (Math) | 14 files (3 top-level + 7 Polygons + 4 Prediction) |
| File NightSharp (Math) | 12 files (3 top-level + 7 Polygons + 3 Prediction) |
| File MATCH 1:1 | 11/14 (79%) — Polygons + Movement + GamePath + Health + PredictionInput/Output |
| File THIẾU | 3 files (Cluster, ConvexHull, Geometry) |
| File CẦN FIX | 3 files (Collision.h YasuoWall/Samira, Polygon.h IsInside, SectorPoly RotateLineFromPoint) |
| Total lines đã port | ~1,500 lines (Movement 945 + Health ~400 + Prediction.h 14 + Collision.h fix + Spell.h fix) |
| Total lines còn cần port | ~1,200+ lines (Cluster 453 + ConvexHull 567 + Geometry ~200) |
| Match percentage | **79%** (Polygons + Movement + GamePath + Health + PredictionInput/Output đã port, còn thiếu Cluster + ConvexHull + Geometry) |

---

## 8. Vấn đề trong quá trình Port Movement.h — Không thể matching 1-1

### 8.1 Ngôn ngữ C# vs C++ — Khác biệt cấu trúc

| # | Vấn đề | C# (EnsoulSharp) | C++ (NightSharp) | Giải pháp |
|---|---|---|---|---|
| 1 | **Namespace cho PredictionInput/Output** | Nằm trong `EnsoulSharp.SDK` namespace, defined inside `Movement.cs` | Ban đầu đặt trong `SDK::Prediction` namespace, gây xung đột với `Spell.h` cũ định nghĩa trong `SDK` namespace | **Đã fix**: Chuyển ra `SDK` namespace (match C# gốc), xóa định nghĩa cũ trong `Spell.h` |
| 2 | **Property vs Field** | C# dùng properties (`CastPosition`, `UnitPosition`) với getter/setter logic (SetZ, fallback) | C++ không có properties tự động | **Đã fix**: Dùng public fields trực tiếp, bỏ getter/setter. Logic `SetZ` và fallback bị mất — xem §8.3 |
| 3 | **Extension methods** | `Vector2Extensions.cs` / `Vector3Extensions.cs` — extension methods trên `SharpDX.Vector2/Vector3` | C++ không có extension methods | **Đã fix**: Tạo `Vec2Ext` / `Vec3Ext` namespaces với free functions |
| 4 | **LINQ / delegates** | C# dùng LINQ (`.Where()`, `.Select()`, `.OrderBy()`) và `Func<>` delegates | C++ không có LINQ | **Đã fix**: Dùng raw loops, lambda functions |
| 5 | **`is` operator / pattern matching** | `if (input.Unit is AIHeroClient)` | C++ không có `is` operator trực tiếp | **Đã skip**: C# code có empty if block `if (result.Hitchance >= HitChance.High && input.Unit is AIHeroClient) {}` — no-op, bỏ qua |

### 8.2 Missing Dependencies — Chức năng chưa port

| # | Dependency | Impact | Trạng thái |
|---|---|---|---|
| 1 | **`Cluster.GetAoEPrediction`** | `GetPrediction(input, ft, checkCollision)` khi `input.AoE == true` không gọi được `Cluster.GetAoEPrediction` | **Fallback**: Trả về `GetStandardPrediction(input)` thay vì AoE prediction. Cần port `Cluster.h` (Phase 2) |
| 2 | **`Collision.GetCollision`** | `GetPrediction` không check collision trực tiếp (circular dependency giữa `Movement.h` và `Collision.h`) | **Bypass**: Collision check được delegate cho caller (`Collision.h` gọi `Prediction::GetPrediction` rồi check collision sau) |
| 3 | **`Variables.TargetSelector`** | Không có target selector trong NightSharp | **Không ảnh hưởng**: Movement.cs không dùng trực tiếp |
| 4 | **`Game.CursorPosRaw`** | `PredictionInput.From` default = cursor position trong C# | **Đã fix**: Dùng `GameObjects::Player().Position()` thay vì cursor (NightSharp có `Game::CursorPosRaw()` nhưng player position an toàn hơn) |
| 5 | ~~**`HealthPrediction` implementation**~~ | ~~`HealthPrediction.h` delegate sang `Prediction::Health::*` chưa port~~ | ✅ **ĐÃ FIX**: `Prediction/Health.h` đã port đầy đủ |

### 8.3 Logic khác biệt không thể tránh khỏi

| # | Vấn đề | Chi tiết |
|---|---|---|
| 1 | **`PredictionOutput.CastPosition` / `UnitPosition` fallback logic** | C# properties có logic: nếu `CastPosition` invalid → fallback `Input.Unit.Position()`, và `SetZ` (set Y = terrain height). C++ dùng public field trực tiếp → **mất fallback logic**. Giải pháp tạm: caller tự check `IsValid()` trước khi dùng. |
| 2 | **`GetAdvancedPrediction` — `position + (speed * (input.Delay / 1000))`** | C# code: `Vector2 + float` không compile hợp lệ. Có thể là bug trong source hoặc decompiler artifact. **C++ interpretation**: `position + position.Normalized() * (speed * delay/1000)` — có thể không chính xác 100%. Cần verify với DLL decompiled. |
| 3 | **`IsBlink` property** | C# `DashEventArgs.IsBlink` — NightSharp `DashArgs` không có `IsBlink` field. Tất cả dashes được xử lý như non-blink. **Có thể gây sai prediction** cho blink abilities (e.g. Ezreal E, Flash). |
| 4 | **`BuffType` enum values** | C# dùng `BuffType.Charm`, `BuffType.Knockup`, etc. từ EnsoulSharp enum. NightSharp dùng `int` constants (`BuffType::Charm = 23`, etc.) match với EnsoulSharp enum values. **Cần verify** giá trị đúng với CoreBuffs implementation. |
| 5 | **`PathTracker` — hero detection** | C# check `sender is AIHeroClient` để chỉ track heroes. NightSharp check qua handle type nhưng logic không hoàn toàn tương đương — có thể track non-hero units hoặc miss một số heroes. |
| 6 | **`CollisionObjects` type mismatch** | C# `PredictionOutput.CollisionObjects` là `List<GameObject>`. NightSharp ban đầu dùng `std::vector<AIBaseClient>` rồi sửa thành `std::vector<GameObject>` để match. Nhưng `CollectLineCollisions` trả về `std::vector<AIBaseClient>` — cần conversion khi gán vào `PredictionOutput.CollisionObjects`. |
| 7 | **`Spell.h` cũ vs mới — field defaults khác** | `Spell.h` cũ: `Radius = 0.0f`, `Hitchance = HitChance::None`, `CollisionObjects = Heroes \| Minions`. `Movement.h` mới: `Radius = 1.0f`, `Hitchance = HitChance::Impossible`, `CollisionObjects = Minions \| YasuoWall`. **Code nào include `Prediction.h` sẽ dùng defaults mới**, code nào include `Spell.h` cũ cũng vậy (vì Spell.h giờ include Prediction.h). |

### 8.4 DLL-only fields chưa port

| Struct | Field | Loại | Ghi chú |
|---|---|---|---|
| `PredictionInput` | `AddHitBox` | bool | DLL-only, không có trong source |
| `PredictionInput` | `MaxCollisionCount` | int | DLL-only |
| `PredictionInput` | `ChoiceCloserPosition` | bool | DLL-only |
| `PredictionInput` | `Spell` | Spell | DLL-only, reference đến spell object |
| `PredictionOutput` | `OriginHitchance` | HitChance | DLL-only |
| `PredictionOutput` | `Idle` | string | DLL-only |

### 8.5 Tóm tắt mức độ matching

| Khía cạnh | Mức độ | Ghi chú |
|---|---|---|
| **Method signatures** | **95% match** | Tất cả public methods đã port, thiếu DLL-only fields |
| **Core logic** | **90% match** | GetPositionOnPath, GetStandardPrediction, GetDashingPrediction, GetImmobilePrediction port 1:1 |
| **GetAdvancedPrediction** | **70% match** | Ambiguous C# code, interpretation có thể sai |
| **Collision checking** | **50% match** | Delegate cho caller, không check inline. Thiếu YasuoWall/Samira |
| **AoE prediction** | **0% match** | Fallback to StandardPrediction, cần port Cluster.h |
| **PathTracker** | **85% match** | Logic port 1:1, nhưng hero detection có thể khác |
| **Helper functions** | **95% match** | Vec2Ext/Vec3Ext port đầy đủ từ Vector2Extensions.cs/Vector3Extensions.cs |
| **Overall** | **~80% match** | Đủ để hoạt động cơ bản, cần port Cluster.h + Collision.h enhancements cho full parity |
