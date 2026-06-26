# Math Module Parity Audit: NightSharp vs EnsoulSharp SDK

> **Ngày phân tích:** 26/06/2026  
> **Nguồn EnsoulSharp SDK:** `EnsoulSharp.SDK/Core/Math/` (source) + `EnsoulSharp.SDK.dll` (decompiled via ILSpy)  
> **Nguồn NightSharp:** `NightSharp yungblod_ dev/SDK/Math/`  

---

## 1. Tóm tắt Tổng quan

| EnsoulSharp SDK (Source + DLL) | NightSharp | Trạng thái |
|---|---|---|
| `Collision.cs` / DLL: `Collisions` class | `Collision.h` | **KHÔNG MATCH** — implementation khác nhau |
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
| `Prediction/Health.cs` / DLL: internal `HealthPrediction` impl | `HealthPrediction.h` (wrapper) | **KHÔNG MATCH** — chỉ là wrapper, thiếu implementation |
| `Prediction/Movement.cs` / DLL: internal `PredictionSDK` impl | **THIẾU** | **THIẾU HOÀN TOÀN** |
| `Prediction/GamePath.cs` / DLL: internal (PathTracker) | **THIẾU** | **THIẾU HOÀN TOÀN** |
| — | `HealthPrediction.h` | Wrapper cho `Prediction::Health` — **nhưng Prediction/ rỗng** |

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
├── Collision.h               (76 lines)
├── HealthPrediction.h        (47 lines)
├── Polygons/
│   ├── Polygon.h             (222 lines)
│   ├── ArcPoly.h             (63 lines)
│   ├── CirclePoly.h          (58 lines)
│   ├── LinePoly.h            (54 lines)
│   ├── RectanglePoly.h       (64 lines)
│   ├── RingPoly.h            (69 lines)
│   └── SectorPoly.h          (62 lines)
└── Prediction/               (RỖNG — 0 files)
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

### 3.2 Collision — **KHÔNG MATCH** ❌

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| Class name | `Collision` | `Collisions` | `SDK::Collision` |
| Methods | `GetCollision(positions, input)` | `GetCollision`, `IsCollision`, `HasYasuoWindWallCollision` | `HasLineCollision`, `GetCollision(target, input)`, `GetCollision(positions, input)`, `HasCollision` |
| Implementation | Lặp qua Minions, Heroes, Walls, YasuoWall riêng biệt | Có thêm Samira, `WillDead`, `OnCreate/OnDelete/OnDoCast` events | Delegate sang `Prediction::Movement::CollectLineCollisions` — **implementation hoàn toàn khác** |

**Vấn đề:**
- NightSharp `Collision.h` include `"Prediction.h"` nhưng **file Prediction.h không tồn tại** trong toàn bộ project
- NightSharp dùng `Prediction::Movement::CollectLineCollisions` và `Prediction::GetPrediction` — **không có implementation**
- DLL có thêm `HasYasuoWindWallCollision` với overload `extraRadius`, `SamiraInGame`, `WillDead` — NightSharp thiếu
- DLL `Collisions` có `Attach()` method để hook events — NightSharp không có

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
| `RotatePolygon` | ❌ | ✅ (2 overloads) | **THIẾGỚI** |
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

### 3.6 Prediction/Health — **CHỈ WRAPPER, THIẾU IMPLEMENTATION** ❌

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| Class name | `Health` | `HealthPrediction` (facade) + internal impl | `HealthPrediction` (wrapper only) |
| `Initialize` | N/A (static ctor) | `Initialize()` | ✅ (delegate) |
| `Update` | N/A (OnGameUpdate) | N/A | ✅ (delegate) |
| `Reset` | N/A | N/A | ✅ (delegate) |
| `GetPrediction` | ✅ (Default + Simulated) | ✅ | ✅ (delegate) |
| `GetAggroTurret` | ✅ | ✅ | ✅ (delegate) |
| `HasMinionAggro` | ✅ | ✅ | ✅ (delegate) |
| `HasTurretAggro` | ✅ | ✅ | ✅ (delegate) |
| `TurretAggroStartTick` | ✅ | ✅ | ✅ (delegate) |
| `EstimateIncomingAutoAttackDamage` | ❌ | ❌ | ✅ (delegate) — **chỉ NightSharp có** |
| `PredictedDamage` inner class | ✅ | `PredictedDamage` (top-level) | **THIẾU** |
| `GetPredictionDefault` | ✅ (private) | internal impl | **THIẾU** |
| `GetPredictionSimulated` | ✅ (private) | internal impl | **THIẾU** |
| Event handlers | ✅ (OnGameUpdate, OnDoCast, OnStopCast, OnDelete, OnProcessSpellCast) | internal impl | **THIẾU** |

**Vấn đề:** `HealthPrediction.h` gọi `Prediction::Health::*` nhưng thư mục `Prediction/` rỗng — **không compile được**.

### 3.7 Prediction/Movement — **THIẾU HOÀN TOÀN** ❌

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| Class name | `Movement` | internal `PredictionSDK` | **KHÔNG CÓ** |
| `GetPrediction` (5 overloads) | ✅ | ✅ | **THIẾU** |
| `GetAdvancedPrediction` | ✅ (internal) | internal | **THIẾU** |
| `GetDashingPrediction` | ✅ (internal) | internal | **THIẾU** |
| `GetImmobilePrediction` | ✅ (internal) | internal | **THIẾU** |
| `GetPositionOnPath` | ✅ (internal) | internal | **THIẾU** |
| `GetStandardPrediction` | ✅ (internal) | internal | **THIẾU** |
| `PositionAfter` | ✅ (internal) | internal | **THIẾU** |
| `UnitIsImmobileUntil` | ✅ (internal) | internal | **THIẾU** |
| `CollectLineCollisions` | ❌ | ❌ | **NightSharp tự tạo** — không có tương đương |
| `ResolveFrom` | ❌ | ❌ | **NightSharp tự tạo** — không có tương đương |

**Đây là file lớn nhất và quan trọng nhất (715 lines).** Cần port đầy đủ.

### 3.8 Prediction/GamePath — **THIẾU HOÀN TOÀN** ❌

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| `GamePath` class | ✅ | ❌ (internal) | **KHÔNG CÓ** |
| `PathTracker` nested class | ✅ | ❌ (internal) | **THIẾU** |
| `StoredPath` nested class | ✅ | ❌ (internal) | **THIẾU** |
| `GetCurrentPath` | ✅ | ❌ | **THIẾU** |
| `GetMeanSpeed` | ✅ | ❌ | **THIẾU** |
| `GetStoredPaths` | ✅ | ❌ | **THIẾU** |
| `OnNewPath` event handler | ✅ | ❌ | **THIẾU** |

**Lưu ý:** Trong DLL, `PathTracker` có thể được nhúng trong `PredictionSDK` internal. Source code có nó riêng biệt.

### 3.9 PredictionInput / PredictionOutput — **THIẾU HOÀN TOÀN** ❌

| Aspect | EnsoulSharp SDK (Source) | EnsoulSharp SDK (DLL) | NightSharp |
|---|---|---|---|
| `PredictionInput` class | ✅ (in Movement.cs) | ✅ (standalone) | **KHÔNG CÓ** |
| Fields: AoE, Collision, CollisionObjects, Delay, From, Radius, Range, RangeCheckFrom, Speed, Type, Unit, UseBoundingRadius | ✅ | ✅ + `AddHitBox`, `MaxCollisionCount`, `ChoiceCloserPosition`, `Spell` | **THIẾU** |
| `RealRadius` property | ✅ | ✅ | **THIẾU** |
| `PredictionOutput` class | ✅ (in Movement.cs) | ✅ (standalone) | **KHÔNG CÓ** |
| Fields: AoeHitCount, AoeTargetsHit, CastPosition, CollisionObjects, Hitchance, UnitPosition, Input | ✅ | ✅ + `OriginHitchance`, `Idle` | **THIẾU** |

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

| # | File cần tạo | Path đề xuất | Mô tả | Ưu tiên |
|---|---|---|---|---|
| 1 | **`ConvexHull.h`** | `SDK/Math/ConvexHull.h` | Port `ConvexHull.cs` — MEC (Minimum Enclosing Circle), `MakeConvexHull`, `FindMinimalBoundingCircle`, `MecCircle` struct | **CAO** |
| 2 | **`Geometry.h`** | `SDK/Math/Geometry.h` | Port static methods từ DLL: `CenterOfPolygone`, `ClipPolygons`, `JoinPolygons`, `MovePolygone`, `RotatePolygon`, `ToPolygon`, `ToPolygons`, `Close`, `DegreeToRadian`, `RadianToDegree` | **CAO** |
| 3 | **`Prediction.h`** | `SDK/Math/Prediction.h` | Header bao gồm tất cả Prediction sub-headers (đang bị `Collision.h` include nhưng không tồn tại) | **CRITICAL** |
| 4 | **`Prediction/Movement.h`** | `SDK/Math/Prediction/Movement.h` | Port `Movement.cs` (715 lines) — `GetPrediction`, `GetAdvancedPrediction`, `GetDashingPrediction`, `GetImmobilePrediction`, `GetPositionOnPath`, `GetStandardPrediction`, `PositionAfter`, `UnitIsImmobileUntil` | **CRITICAL** |
| 5 | **`Prediction/Cluster.h`** | `SDK/Math/Prediction/Cluster.h` | Port `Cluster.cs` (453 lines) — `GetAoEPrediction`, `Circle/Cone/Line.GetXxxPrediction`, `GetPossibleTargets`, `PossibleTarget` struct | **CAO** |
| 6 | **`Prediction/Health.h`** | `SDK/Math/Prediction/Health.h` | Port `Health.cs` (471 lines) — `PredictedDamage` class, `GetPredictionDefault`, `GetPredictionSimulated`, event handlers, `ActiveAttacks` dict | **CAO** |
| 7 | **`Prediction/GamePath.h`** | `SDK/Math/Prediction/GamePath.h` | Port `GamePath.cs` (212 lines) — `PathTracker` class, `StoredPath` class, `GetCurrentPath`, `GetMeanSpeed`, `GetStoredPaths` | **CAO** |
| 8 | **`Prediction/PredictionInput.h`** | `SDK/Math/Prediction/PredictionInput.h` | Port `PredictionInput` class — AoE, Collision, CollisionObjects, Delay, From, Radius, Range, RangeCheckFrom, Speed, Type, Unit, UseBoundingRadius, RealRadius | **CRITICAL** |
| 9 | **`Prediction/PredictionOutput.h`** | `SDK/Math/Prediction/PredictionOutput.h` | Port `PredictionOutput` class — AoeHitCount, AoeTargetsHit, CastPosition, CollisionObjects, Hitchance, UnitPosition, Input | **CRITICAL** |

### 5.2 File CẦN FIX (đã có nhưng chưa match)

| # | File | Vấn đề | Cách fix |
|---|---|---|---|
| 1 | **`Collision.h`** | Include `"Prediction.h"` không tồn tại → **compile error** | Tạo `Prediction.h` hoặc sửa include path |
| 2 | **`Collision.h`** | Thiếu `HasYasuoWindWallCollision` (có trong DLL) | Thêm method `HasYasuoWindWallCollision(start, end, extraRadius=0)` |
| 3 | **`Collision.h`** | Thiếu `IsCollision` overload (có trong DLL) | Thêm `IsCollision(positions, input)` |
| 4 | **`Collision.h`** | Thiếu `WillDead` logic (có trong DLL) | Port `WillDead` method |
| 5 | **`Collision.h`** | Thiếu Samira shield handling | Port `SamiraInGame` logic |
| 6 | **`Collision.h`** | Thiếu event hooks (`Attach`, `OnCreate`, `OnDelete`, `OnDoCast`) | Port event attachment |
| 7 | **`HealthPrediction.h`** | Delegate sang `Prediction::Health::*` nhưng implementation không tồn tại | Tạo `Prediction/Health.h` với đầy đủ implementation |
| 8 | **`HealthPrediction.h`** | Thiếu `PredictedDamage` inner class | Port `PredictedDamage` struct |
| 9 | **`Polygons/Polygon.h`** | Thiếu overload `IsInside(GameObject)` | Thêm `IsInside(const GameObject& obj)` overload |
| 10 | **`Polygons/SectorPoly.h`** | Thiếu `RotateLineFromPoint` method (có trong DLL) | Port `RotateLineFromPoint(point1, point2, value, radian)` |

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
1. `Prediction/PredictionInput.h` — cần cho mọi Prediction call
2. `Prediction/PredictionOutput.h` — cần cho mọi Prediction call
3. `Prediction/Movement.h` — core prediction engine (715 lines)
4. `Prediction.h` — umbrella header bao gồm tất cả Prediction sub-headers
5. Fix `Collision.h` include path

### Phase 2 — High (cần cho functionality đầy đủ)
6. `Prediction/Health.h` — health prediction implementation (471 lines)
7. `Prediction/Cluster.h` — AoE prediction (453 lines)
8. `Prediction/GamePath.h` — path tracking (212 lines)
9. `ConvexHull.h` — MEC calculation (567 lines)
10. `Geometry.h` — polygon operations từ DLL

### Phase 3 — Medium (hoàn thiện parity)
11. Fix `Collision.h` — thêm `HasYasuoWindWallCollision`, `IsCollision`, `WillDead`, Samira, event hooks
12. Fix `Polygons/Polygon.h` — thêm `IsInside(GameObject)` overload
13. Fix `Polygons/SectorPoly.h` — thêm `RotateLineFromPoint`
14. Fix `HealthPrediction.h` — thêm `PredictedDamage` struct

---

## 7. Thống kê Tổng

| Metric | Số |
|---|---|
| File EnsoulSharp SDK (Math) | 14 files (3 top-level + 7 Polygons + 4 Prediction) |
| File NightSharp (Math) | 9 files (2 top-level + 7 Polygons + 0 Prediction) |
| File MATCH 1:1 | 7/14 (50%) — chỉ Polygons |
| File THIẾU | 9 files |
| File CẦN FIX | 4 files |
| Total lines cần port | ~2,800+ lines (Movement 715 + Health 471 + Cluster 453 + ConvexHull 567 + GamePath 212 + Geometry ~200 + PredictionInput ~120 + PredictionOutput ~90 + misc) |
| Match percentage | **50%** (Polygons match, Prediction + ConvexHull + Geometry hoàn toàn thiếu) |
