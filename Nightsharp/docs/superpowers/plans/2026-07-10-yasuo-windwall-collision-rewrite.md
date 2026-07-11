# Yasuo Windwall Collision Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Thay toàn bộ cơ chế detect Yasuo windwall bằng một nguồn duy nhất `WindwallTracker` (event-driven, lấy `EffectEmitter` thật), xóa sạch synthetic + code chết, và vẽ đúng bức tường gió trong debug overlay — để skillshot không còn bắn xuyên tường.

**Architecture:** Một pure-geometry header (`WindwallGeometry.h`, unit-test được standalone) + một registry event-driven (`WindwallTracker.h`, subscribe `OnCreateObject`/`OnDeleteObject`). `Collision.h` và debug plugin cùng đọc từ tracker và dùng chung geometry helper. Không còn `SyntheticWindwalls`, không còn `AddWindwall` chết, không còn block windwall chết trong `Movement.h`.

**Tech Stack:** C++17, header-only inline (giant single-TU DLL inject vào League), MSBuild Release x64, ImGui overlay, CheatEngine/ReClass để verify runtime.

## Global Constraints

- Ngôn ngữ code + comment giữ style hiện có của repo (comment tiếng Anh ngắn gọn, tham chiếu EnsoulSharp khi liên quan).
- **KHÔNG** đổi public API của `SDK::Collision` (Samira/Mel/Braum giữ nguyên hành vi).
- Runtime emitter name đã xác minh (CheatEngine, PID 19820): **`Yasuo_Base_W_windwall1`..`Yasuo_Base_W_windwall5`**. Match phải **lowercase**; chuỗi casing `_W_windWall` không tồn tại runtime.
- Name-match: `contains("yasuo") && contains("_w_windwall")` (lowercase). Level = hậu tố `windwall2..5`, mặc định 1.
- `emitter.Name()` có thể trả **rỗng** → mọi chỗ đọc tên emitter phải fallback `::Core::Objects::ReadCharacterName` rồi `::Core::Objects::ReadName`.
- Width tường theo level giữ đúng công thức hiện có: `widthBase + 50 * level (+ extraRadius)`.
- Geometry span direction: primary `Vec2(m[0][0], m[0][2])` từ `Orientation()`; nếu degenerate thì `perpendicular(Direction().To2D())`. Đây là **một** điểm chốt — sau khi verify in-game (Task 6) chỉ sửa **một** hàm nếu cần lật.
- Build fragile khi RAM thấp (đóng League/app nặng trước khi build DLL — xem memory `nightsharp-build-memory-fragile`).

---

## File Structure

- **Create** `sdk/Math/WindwallGeometry.h` — pure geometry, phụ thuộc duy nhất `core/Vector.h` + `<string>/<cmath>/<cstdio>`. Không đụng live memory. Unit-test được.
- **Create** `sdk/Math/WindwallTracker.h` — registry event-driven; phụ thuộc `sdk/Core/Objects.h`, `sdk/Events/Events.h`, `WindwallGeometry.h`.
- **Create** `tests/windwall_geometry_test.cpp` — standalone unit test cho `WindwallGeometry.h` (mirror `tests/polygon_guard_test.cpp`).
- **Modify** `sdk/Math/Collision.h` — `SegmentIntersectsWindwall` đọc từ tracker; xóa synthetic + `AddWindwall`/`Windwalls`/`RefreshWindwalls` chết.
- **Modify** `sdk/Math/Prediction/Movement.h` — xóa block YasuoWall chết trong `CollectLineCollisions`.
- **Modify** `plugins/Utility/YasuoWallDebugPlugin.h` — vẽ hình tường thật từ tracker; bỏ đường probe player→cursor; đọc từ tracker thay vì tự scan.

## Testing model (đọc trước khi làm)

Codebase này **không có** unit-test framework chạy tự động; chỉ có test standalone kiểu `tests/polygon_guard_test.cpp` (compile riêng bằng `cl.exe`). Vì tracker + `Orientation()` chạm live-memory nên:

- **Task 1** (pure geometry) → **unit test thật** (standalone `cl.exe`).
- **Task 2–5** → test cycle = **build DLL thành công** (`build.bat`) + **verify in-game** qua debug overlay ở Task 6.

Lệnh unit test standalone (chạy trong "x64 Native Tools Command Prompt for VS", hoặc gọi `cl.exe` cạnh MSBuild ở `E:\Visual Studio\Community\VC\...\bin\Hostx64\x64\cl.exe`):

```
cl /std:c++17 /EHsc /nologo tests\windwall_geometry_test.cpp /Fe:build\windwall_geometry_test.exe && build\windwall_geometry_test.exe
```

Expected khi PASS: in ra `ALL WINDWALL GEOMETRY TESTS PASSED` và exit code 0.

---

### Task 1: `WindwallGeometry.h` — pure geometry + unit test

**Files:**
- Create: `sdk/Math/WindwallGeometry.h`
- Test: `tests/windwall_geometry_test.cpp`

**Interfaces:**
- Consumes: `Vec2` từ `core/Vector.h` (`{x,y}`, `operator+`, `operator-`, `operator*(float)`, `LengthSqr()`, `Normalized()`).
- Produces (namespace `SDK::WindwallGeo`):
  - `std::string ToLower(std::string)`
  - `bool IsWindwallName(const std::string& name)` — true nếu lowercase chứa `yasuo` và `_w_windwall`.
  - `int ParseLevel(const std::string& name)` — 1..5 theo hậu tố `windwall2..5`, mặc định 1.
  - `Vec2 SpanDirection(const float m[4][4], Vec2 fallbackForward)` — hướng trải tường đã normalize; primary `Vec2(m[0][0],m[0][2])`, degenerate → `perpendicular(fallbackForward)`; cả hai degenerate → `Vec2(1,0)`.
  - `struct Segment { Vec2 start; Vec2 end; };`
  - `Segment BuildWall(Vec2 center, Vec2 spanDir, float width)` — `start = center + spanDir*(width/2)`, `end = start - spanDir*width`.

- [ ] **Step 1: Viết failing test** — tạo `tests/windwall_geometry_test.cpp`:

```cpp
#include <cstdio>
#include <string>

#include "../sdk/Math/WindwallGeometry.h"

using SDK::WindwallGeo::BuildWall;
using SDK::WindwallGeo::IsWindwallName;
using SDK::WindwallGeo::ParseLevel;
using SDK::WindwallGeo::Segment;
using SDK::WindwallGeo::SpanDirection;

static int g_failures = 0;

static void ExpectTrue(const char* name, bool cond) {
    if (!cond) { std::printf("FAIL: %s\n", name); ++g_failures; }
}

static void ExpectNear(const char* name, float a, float b) {
    const float d = a - b;
    if (d > 0.001f || d < -0.001f) {
        std::printf("FAIL: %s expected %.4f got %.4f\n", name, b, a);
        ++g_failures;
    }
}

static void ExpectEqInt(const char* name, int a, int b) {
    if (a != b) { std::printf("FAIL: %s expected %d got %d\n", name, b, a); ++g_failures; }
}

int main() {
    // Name matching (runtime name Yasuo_Base_W_windwallN)
    ExpectTrue("match level5", IsWindwallName("Yasuo_Base_W_windwall5"));
    ExpectTrue("match level1", IsWindwallName("yasuo_base_w_windwall1"));
    ExpectTrue("reject non-yasuo", !IsWindwallName("Malphite_Base_R_impact"));
    ExpectTrue("reject yasuo-non-wall", !IsWindwallName("Yasuo_Base_Q_effect"));

    // Level parsing
    ExpectEqInt("level5", ParseLevel("Yasuo_Base_W_windwall5"), 5);
    ExpectEqInt("level2", ParseLevel("Yasuo_Base_W_windwall2"), 2);
    ExpectEqInt("level default 1", ParseLevel("Yasuo_Base_W_windwall1"), 1);
    ExpectEqInt("level no-suffix", ParseLevel("Yasuo_Base_W_windwall"), 1);

    // SpanDirection: primary from matrix row0 (m[0][0], m[0][2])
    float mat[4][4] = {};
    mat[0][0] = 1.0f; mat[0][2] = 0.0f;
    Vec2 span = SpanDirection(mat, Vec2{0.0f, 1.0f});
    ExpectNear("span primary x", span.x, 1.0f);
    ExpectNear("span primary y", span.y, 0.0f);

    // SpanDirection: degenerate matrix -> perpendicular of fallback forward (0,1) -> (-1,0)
    float zero[4][4] = {};
    Vec2 spanFb = SpanDirection(zero, Vec2{0.0f, 1.0f});
    ExpectNear("span fallback x", spanFb.x, -1.0f);
    ExpectNear("span fallback y", spanFb.y, 0.0f);

    // BuildWall geometry
    Segment seg = BuildWall(Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}, 400.0f);
    ExpectNear("wall start x", seg.start.x, 200.0f);
    ExpectNear("wall end x", seg.end.x, -200.0f);

    if (g_failures == 0) {
        std::printf("ALL WINDWALL GEOMETRY TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
```

- [ ] **Step 2: Chạy test để chắc nó fail** (chưa có header)

Run:
```
cl /std:c++17 /EHsc /nologo tests\windwall_geometry_test.cpp /Fe:build\windwall_geometry_test.exe
```
Expected: FAIL compile — `Cannot open include file: '../sdk/Math/WindwallGeometry.h'`.

- [ ] **Step 3: Viết `sdk/Math/WindwallGeometry.h`**

```cpp
#pragma once

// ============================================================================
// WindwallGeometry.h — pure geometry for Yasuo windwall collision.
// No live memory access; unit-testable standalone (tests/windwall_geometry_test.cpp).
// Runtime emitter name verified via CheatEngine: Yasuo_Base_W_windwall<1-5>.
// ============================================================================

#include "../../core/Vector.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace SDK::WindwallGeo {

inline std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline bool IsWindwallName(const std::string& name) {
    const std::string lower = ToLower(name);
    return lower.find("yasuo") != std::string::npos &&
           lower.find("_w_windwall") != std::string::npos;
}

inline int ParseLevel(const std::string& name) {
    const std::string lower = ToLower(name);
    for (int level = 5; level >= 2; --level) {
        char suffix[16] = {};
        std::snprintf(suffix, sizeof(suffix), "windwall%d", level);
        if (lower.find(suffix) != std::string::npos) {
            return level;
        }
    }
    return 1;
}

// Span (spread) direction of the wall, normalized.
// Primary: matrix row 0 (m[0][0], m[0][2]) — EnsoulSharp Vector2(Orientation.M11, M13).
// Fallback: perpendicular of the emitter forward vector.
inline Vec2 SpanDirection(const float m[4][4], Vec2 fallbackForward) {
    Vec2 primary{ m[0][0], m[0][2] };
    if (primary.LengthSqr() >= 0.001f) {
        return primary.Normalized();
    }
    if (fallbackForward.LengthSqr() >= 0.001f) {
        const Vec2 perp{ -fallbackForward.y, fallbackForward.x };
        return perp.Normalized();
    }
    return Vec2{ 1.0f, 0.0f };
}

struct Segment {
    Vec2 start;
    Vec2 end;
};

inline Segment BuildWall(Vec2 center, Vec2 spanDir, float width) {
    Segment seg;
    seg.start = center + spanDir * (width * 0.5f);
    seg.end = seg.start - spanDir * width;
    return seg;
}

} // namespace SDK::WindwallGeo
```

- [ ] **Step 4: Chạy test để chắc nó pass**

Run:
```
cl /std:c++17 /EHsc /nologo tests\windwall_geometry_test.cpp /Fe:build\windwall_geometry_test.exe && build\windwall_geometry_test.exe
```
Expected: `ALL WINDWALL GEOMETRY TESTS PASSED`, exit 0.

- [ ] **Step 5: Commit**

```
git add sdk/Math/WindwallGeometry.h tests/windwall_geometry_test.cpp
git commit -m "feat(windwall): add pure geometry helper + unit test"
```

---

### Task 2: `WindwallTracker.h` — event-driven registry

**Files:**
- Create: `sdk/Math/WindwallTracker.h`

**Interfaces:**
- Consumes: `SDK::EffectEmitter` (`sdk/Core/Objects.h`); `SDK::Events::{AddOnCreateObject,RemoveOnCreateObject,AddOnDeleteObject,RemoveOnDeleteObject,ObjectEventArgs}` (`sdk/Events/Events.h`, handler `void(*)(const ObjectEventArgs&)`, `args.Sender.{Ptr,NetworkId,Name}`); `::Core::Objects::{ReadCharacterName,ReadName}`; `SDK::WindwallGeo::{IsWindwallName,ParseLevel}`; `Variables::TickCount()`.
- Produces (namespace `SDK::WindwallTracker`):
  - `struct Wall { EffectEmitter emitter; int networkId; int level; int castTick; };`
  - `void EnsureInitialized();` — idempotent, subscribe create/delete một lần.
  - `const std::vector<Wall>& Active();` — prune (invalid/dead/quá 5000ms) rồi trả list.

- [ ] **Step 1: Viết `sdk/Math/WindwallTracker.h`**

```cpp
#pragma once

// ============================================================================
// WindwallTracker.h — single source of truth for live Yasuo windwall emitters.
// Event-driven: tracks EffectEmitter objects named Yasuo_Base_W_windwall<1-5>
// via OnCreateObject/OnDeleteObject. Replaces the dead per-call Get<EffectEmitter>
// scan and the synthetic OnDoCast fallback. Geometry lives in WindwallGeometry.h.
// ============================================================================

#include "../Core/Objects.h"
#include "../Core/Variables.h"
#include "../Events/Events.h"
#include "WindwallGeometry.h"

#include <algorithm>
#include <string>
#include <vector>

namespace SDK::WindwallTracker {

struct Wall {
    EffectEmitter emitter = {};
    int networkId = 0;
    int level = 1;
    int castTick = 0;
};

namespace detail {

inline std::vector<Wall> g_walls;
inline bool g_subscribed = false;

// emitter name is often empty via Name(); fall back to raw name reads.
inline std::string ResolveName(uintptr_t address, const char* eventName) {
    if (eventName && eventName[0]) {
        return eventName;
    }
    char buffer[128] = {};
    if (::Core::Objects::ReadCharacterName(address, buffer, static_cast<int>(sizeof(buffer))) ||
        ::Core::Objects::ReadName(address, buffer, static_cast<int>(sizeof(buffer)))) {
        return buffer;
    }
    return std::string();
}

inline void OnCreate(const Events::ObjectEventArgs& args) {
    if (!args.Sender.Ptr) {
        return;
    }
    const std::string name = ResolveName(args.Sender.Ptr, args.Sender.Name);
    if (!SDK::WindwallGeo::IsWindwallName(name)) {
        return;
    }

    const int networkId = static_cast<int>(args.Sender.NetworkId);
    const auto exists = std::find_if(g_walls.begin(), g_walls.end(), [&](const Wall& w) {
        return (networkId != 0 && w.networkId == networkId) ||
               (w.emitter.IsValid() && w.emitter.Address() == args.Sender.Ptr);
    });
    if (exists != g_walls.end()) {
        return;
    }

    Wall wall;
    wall.emitter = EffectEmitter(args.Sender.Ptr);
    wall.networkId = networkId;
    wall.level = SDK::WindwallGeo::ParseLevel(name);
    wall.castTick = Variables::TickCount();
    g_walls.push_back(wall);
}

inline void OnDelete(const Events::ObjectEventArgs& args) {
    const int networkId = static_cast<int>(args.Sender.NetworkId);
    g_walls.erase(
        std::remove_if(g_walls.begin(), g_walls.end(), [&](const Wall& w) {
            return (networkId != 0 && w.networkId == networkId) ||
                   (args.Sender.Ptr && w.emitter.IsValid() &&
                    w.emitter.Address() == args.Sender.Ptr);
        }),
        g_walls.end());
}

inline void Prune() {
    const int now = Variables::TickCount();
    g_walls.erase(
        std::remove_if(g_walls.begin(), g_walls.end(), [now](const Wall& w) {
            return !w.emitter.IsValid() || w.emitter.IsDead() ||
                   now - w.castTick > 5000;
        }),
        g_walls.end());
}

} // namespace detail

inline void EnsureInitialized() {
    if (detail::g_subscribed) {
        return;
    }
    detail::g_subscribed =
        Events::AddOnCreateObject(&detail::OnCreate) &&
        Events::AddOnDeleteObject(&detail::OnDelete);
}

inline const std::vector<Wall>& Active() {
    EnsureInitialized();
    detail::Prune();
    return detail::g_walls;
}

} // namespace SDK::WindwallTracker
```

- [ ] **Step 2: Build DLL để chắc compile sạch**

Run (đóng League trước): `build.bat`
Expected: `Build succeeded`, 0 error. (Chưa ai gọi tracker nên chỉ kiểm tra header hợp lệ — thêm `#include "WindwallTracker.h"` tạm vào `Collision.h` nếu MSBuild không compile header rời; Task 3 sẽ include thật.)

- [ ] **Step 3: Commit**

```
git add sdk/Math/WindwallTracker.h
git commit -m "feat(windwall): add event-driven WindwallTracker registry"
```

---

### Task 3: Wire `Collision.h` vào tracker + xóa synthetic/dead code

**Files:**
- Modify: `sdk/Math/Collision.h`

**Interfaces:**
- Consumes: `SDK::WindwallTracker::{Active,Wall}`; `SDK::WindwallGeo::{SpanDirection,BuildWall,Segment}`; `Prediction::Vec2Ext::Intersection`.
- Produces: `SegmentIntersectsWindwall(start,end,widthBase,extraRadius)` (chữ ký giữ nguyên) giờ đọc từ tracker. Public API `HasYasuoWindWallCollision`, `IsCollision`, `GetCollision` không đổi.

- [ ] **Step 1: Thêm include tracker** — đầu `Collision.h`, cạnh các include SDK hiện có:

```cpp
#include "WindwallGeometry.h"
#include "WindwallTracker.h"
```

- [ ] **Step 2: Xóa toàn bộ synthetic + dead state** trong `namespace detail` — bỏ các mục sau (dòng ~46-64 và các hàm liên quan):

Xóa: `YasuoInGame`/`SamiraInGame`/`MelInGame` giữ lại (Samira/Mel còn dùng); xóa `WallCastT`, `std::vector<EffectEmitter> Windwalls`, `DoCastHooked`, `struct SyntheticWindwall`, `std::vector<SyntheticWindwall> SyntheticWindwalls`.

Xóa nguyên các hàm: `IsWindWallName`, `RuntimeObjectName` (chỉ nếu không còn caller khác trong file — `IsWindWallName` dùng ở `AddWindwall`/`RefreshWindwalls` sắp xóa; `RuntimeObjectName` chỉ dùng bởi windwall, xóa được), `IsYasuoWCast`, `ResolveYasuoWLevel`, `TryBuildSyntheticWindwall`, `PruneSyntheticWindwalls`, `AddSyntheticWindwall`, `OnDoCast`, `EnsureEventHooks`, `IsWindWallEmitter`, `AddWindwall`, `RefreshWindwalls`, `GetWindWallLevel`, `IsWallCastActive`.

> Lưu ý: giữ `RefreshChampionFlags`, `IsChampionInGame`, `ToLower`, `HasCircularShieldCollision`, `HasSamiraCollision`, `HasMelCollision`, `ContainsAnyLower`, và toàn bộ phần minion/hero/building — không đụng.

- [ ] **Step 3: Thay `detail::Initialize()`** — bỏ gọi `EnsureEventHooks()`/`RefreshWindwalls()`, thay bằng khởi tạo tracker:

```cpp
inline void Initialize() {
    if (Initialized) {
        WindwallTracker::EnsureInitialized();
        return;
    }
    Initialized = true;
    RefreshChampionFlags();
    WindwallTracker::EnsureInitialized();
}
```

- [ ] **Step 4: Viết lại `SegmentIntersectsWindwall`** — đọc từ tracker, dùng geometry helper, bỏ nhánh synthetic:

```cpp
inline bool SegmentIntersectsWindwall(const Vector3& start,
                                      const Vector3& end,
                                      float widthBase,
                                      float extraRadius) {
    Initialize();
    RefreshChampionFlags();
    if (!YasuoInGame) {
        return false;
    }

    const Vec2 start2D = start.To2D();
    const Vec2 end2D = end.To2D();
    for (const auto& wall : WindwallTracker::Active()) {
        if (!wall.emitter.IsValid() || wall.emitter.IsDead()) {
            continue;
        }

        const float width = widthBase + 50.0f * static_cast<float>(wall.level) + extraRadius;
        const auto mat = wall.emitter.Orientation();
        const Vec2 fallbackForward = wall.emitter.Direction().To2D();
        const Vec2 spanDir = SDK::WindwallGeo::SpanDirection(mat.m, fallbackForward);
        const Vec2 center = wall.emitter.Position().To2D();
        const auto seg = SDK::WindwallGeo::BuildWall(center, spanDir, width);

        if (Prediction::Vec2Ext::Intersection(seg.start, seg.end, end2D, start2D).Valid) {
            return true;
        }
    }
    return false;
}
```

- [ ] **Step 5: Build DLL**

Run (đóng League trước): `build.bat`
Expected: `Build succeeded`, 0 error. Nếu lỗi "undeclared identifier" cho symbol đã xóa → tìm caller còn sót và dọn.

- [ ] **Step 6: Commit**

```
git add sdk/Math/Collision.h
git commit -m "refactor(windwall): route collision through WindwallTracker, drop synthetic + dead paths"
```

---

### Task 4: Xóa block windwall chết trong `Movement.h`

**Files:**
- Modify: `sdk/Math/Prediction/Movement.h:1102-1156` (block `// ── Yasuo Wind Wall ──` bên trong `CollectLineCollisions`).

**Interfaces:**
- Consumes: không thêm gì.
- Produces: `CollectLineCollisions` không còn logic windwall riêng (nó vốn là dead code, và windwall giờ do `Collision.h`/tracker lo).

- [ ] **Step 1: Xóa nguyên block YasuoWall** — bỏ đoạn từ comment `// ── Yasuo Wind Wall ──` (dòng ~1102) đến hết `}` đóng của `if (SDK::HasFlag(flags, CollisionableObjects::YasuoWall)) { ... }` (dòng ~1156), thay bằng comment 1 dòng:

```cpp
    // Yasuo/Samira/Mel projectile walls are handled centrally in Collision.h
    // via WindwallTracker; not duplicated here.
```

> Giữ nguyên block Samira (dòng ~1159+) nếu nó cũng nằm trong hàm này — chỉ xóa block YasuoWall. Nếu block Samira/Mel cũng là bản sao chết và không được gọi, xóa luôn cho sạch (cùng lý do), nhưng chỉ khi chắc `CollectLineCollisions` không caller (đã xác nhận: `grep CollectLineCollisions` chỉ ra 1 định nghĩa, 0 lời gọi).

- [ ] **Step 2: Build DLL**

Run (đóng League trước): `build.bat`
Expected: `Build succeeded`, 0 error.

- [ ] **Step 3: Commit**

```
git add sdk/Math/Prediction/Movement.h
git commit -m "chore(windwall): remove dead duplicated windwall block in Movement.CollectLineCollisions"
```

---

### Task 5: Debug plugin — vẽ chính bức tường gió từ tracker

**Files:**
- Modify: `plugins/Utility/YasuoWallDebugPlugin.h`

**Interfaces:**
- Consumes: `SDK::WindwallTracker::{Active,Wall}`; `SDK::WindwallGeo::{SpanDirection,BuildWall}`; `SDK::Drawing::{DrawLine,DrawCircleAlways,WorldToScreen,DrawText}`.
- Produces: overlay vẽ **hình tường** (đoạn tường theo geometry thật) + nhãn level/networkId. **Không** còn đường probe player→cursor.

- [ ] **Step 1: Thêm include** đầu file:

```cpp
#include "../../SDK/Math/WindwallTracker.h"
#include "../../SDK/Math/WindwallGeometry.h"
```

- [ ] **Step 2: Thay nguồn dữ liệu** — trong `RefreshWalls()`/`OnRender()`, bỏ `directScanEnabled_`/`syntheticWalls_`/event tự-track của plugin; đọc thẳng `SDK::WindwallTracker::Active()`. Rút gọn `OnRender`:

```cpp
    void OnRender() override {
        if (!drawEnabled_ || !ImGui::GetCurrentContext() || !SDK::Drawing::IsEnabled()) {
            return;
        }
        for (const auto& wall : SDK::WindwallTracker::Active()) {
            if (!wall.emitter.IsValid() || wall.emitter.IsDead()) {
                continue;
            }
            DrawWall(wall);
        }
    }
```

- [ ] **Step 3: Viết lại `DrawWall`** — vẽ **bức tường thật** (đoạn span + đánh dấu tâm), bỏ line probe:

```cpp
    void DrawWall(const SDK::WindwallTracker::Wall& wall) const {
        const Vec3 pos = wall.emitter.Position();
        const float width = 250.0f + 50.0f * static_cast<float>(wall.level);
        const auto mat = wall.emitter.Orientation();
        const Vec2 fallbackForward = wall.emitter.Direction().To2D();
        const Vec2 spanDir = SDK::WindwallGeo::SpanDirection(mat.m, fallbackForward);
        const auto seg = SDK::WindwallGeo::BuildWall(pos.To2D(), spanDir, width);

        const Vec3 wallStart(seg.start.x, pos.y, seg.start.y);
        const Vec3 wallEnd(seg.end.x, pos.y, seg.end.y);

        // The wall itself.
        SDK::Drawing::DrawLine(wallStart, wallEnd, kColorPredictionWall, 5.0f);
        SDK::Drawing::DrawCircleAlways(pos, 45.0f, kColorEmitter, 2.0f, 32);

        char label[128] = {};
        std::snprintf(label, sizeof(label), "YasuoWall L%d id=%d w=%.0f",
                      wall.level, wall.networkId, width);
        DrawTextWorld(pos, label, kColorText, 0.0f, -18.0f);
    }
```

- [ ] **Step 4: Bỏ probe** — xóa `DrawPlayerProbe`, `HasProbeCollision`, `drawPlayerProbe_`, `probeExtraRadius_`, và mọi tham chiếu trong `OnMenu`/`OnRender`. Xóa các helper/hằng chỉ phục vụ probe/synthetic không còn dùng (`MakeSyntheticSnapshot`, `HandleDoCast`, `DirectScanWalls`, `syntheticWalls_`, `walls_`, event handlers tự-track) nếu đã chuyển hết sang `WindwallTracker::Active()`.

- [ ] **Step 5: Build DLL**

Run (đóng League trước): `build.bat`
Expected: `Build succeeded`, 0 error.

- [ ] **Step 6: Commit**

```
git add plugins/Utility/YasuoWallDebugPlugin.h
git commit -m "feat(windwall): debug overlay draws the actual wall from tracker, drop probe line"
```

---

### Task 6: Verify in-game + chốt geometry

**Files:**
- Có thể Modify: `sdk/Math/WindwallGeometry.h` (chỉ hàm `SpanDirection` nếu geometry sai).

**Interfaces:** không đổi.

- [ ] **Step 1: Inject + bật overlay** — build.bat (đóng app nặng), inject DLL, bật plugin "Yasuo Wall Debug", tick "Draw Yasuo walls".

- [ ] **Step 2: Cho Yasuo (enemy) bấm W tạo wall** — quan sát overlay:
  - Emitter được track? (đường tường vẽ ra ngay khi wall xuất hiện).
  - Đường tường vẽ có **trùng khít** tường gió thật (đúng vị trí + hướng + độ dài)?

- [ ] **Step 3: Nếu tường vẽ lệch 90°** so với tường thật → geometry primary sai, đổi `SpanDirection` trong `WindwallGeometry.h`: dùng perpendicular của primary thay vì primary trực tiếp:

```cpp
inline Vec2 SpanDirection(const float m[4][4], Vec2 fallbackForward) {
    Vec2 forward{ m[0][0], m[0][2] };
    if (forward.LengthSqr() >= 0.001f) {
        const Vec2 perp{ -forward.y, forward.x };   // wall spans perpendicular to facing
        return perp.Normalized();
    }
    if (fallbackForward.LengthSqr() >= 0.001f) {
        return fallbackForward.Normalized();
    }
    return Vec2{ 1.0f, 0.0f };
}
```
Cập nhật unit test tương ứng, chạy lại Task 1 Step 4, rebuild, verify lại Step 2.

- [ ] **Step 4: Test chặn skillshot** — cast skillshot line (champ dùng YasuoWall collision) xuyên qua tường: bot **không** được bắn xuyên (collision trả true). Sau khi wall hết → emitter delete → tường biến mất khỏi overlay → skillshot bắn lại bình thường.

- [ ] **Step 5: Nếu offset `Orientation()` trả matrix 0** (tường không vẽ / hướng random) → dùng CheatEngine/ReClass verify `Offset::All::EffectEmitterHandle` + `Offset::EffectEmitterLayout::ProxyOrientation` trên object emitter sống; cập nhật offset nếu stale. (Emitter name đã verify là `Yasuo_Base_W_windwall<N>` — dùng search_string để định vị object.)

- [ ] **Step 6: Commit** (nếu có sửa geometry/offset)

```
git add sdk/Math/WindwallGeometry.h tests/windwall_geometry_test.cpp
git commit -m "fix(windwall): finalize verified wall span geometry"
```

---

## Self-Review

**Spec coverage:**
- Component `WindwallTracker` event-driven → Task 2. ✓
- Chốt geometry một nguồn → Task 1 (`SpanDirection`) + Task 6 (verify/chốt). ✓
- Xóa synthetic + `AddWindwall`/`Windwalls` chết trong Collision.h → Task 3. ✓
- Dead path Movement.h `CollectLineCollisions` → Task 4. ✓
- Wire `SegmentIntersectsWindwall`/`HasYasuoWindWallCollision`/`ProcessProjectileWalls`, giữ public API → Task 3. ✓
- Visualization vẽ chính bức tường, bỏ probe → Task 5. ✓
- Tên emitter đã verify (Global Constraints) → dùng ở Task 1/2. ✓
- CommunityDragon chỉ tra tên (đã thay bằng verify CE trực tiếp) → note trong spec, không cần task. ✓
- Test plan in-game → Task 6. ✓

**Placeholder scan:** không có TBD/TODO; mọi step code có nội dung thật; lệnh + expected cụ thể.

**Type consistency:** `WindwallGeo::SpanDirection(const float[4][4], Vec2)`, `BuildWall(Vec2,Vec2,float)->Segment{start,end}`, `WindwallTracker::Wall{emitter,networkId,level,castTick}`, `Active()->const vector<Wall>&` — dùng nhất quán ở Task 3 và Task 5. `emitter.Orientation().m` là `float[4][4]` (nested trong `EffectEmitter::D3DMatrix`) khớp tham số `SpanDirection`. ✓
