# Yasuo Wall Object-Composition Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Thay implementation Yasuo Wall bằng tracker ghép wall chính với hai `YasuoWChildMis`, dùng endpoint runtime thật cho debug drawing và collision.

**Architecture:** `YasuoWallModel.h` là state/model thuần C++ chịu trách nhiệm exact-name classification, delayed-name lifecycle, deterministic endpoint pairing và segment/capsule collision. `YasuoWallTracker.h` là adapter process-bound mỏng: nhận object events, resolve common `GameObject` name/position qua `ObjectManager`, rồi xuất snapshot theo value cho `Collision.h` và debug plugin.

**Tech Stack:** C++20 header-only SDK, NightSharp object/events APIs, standalone MSVC tests, Visual Studio Community toolset `v145`.

## Global Constraints

- Build Visual Studio từ `E:\Visual Studio`.
- Platform toolset phải là `v145`.
- Không có offset riêng cho YasuoWall và không thêm offset transform mới.
- Không dùng `EffectEmitterHandle`, `ProxyOrientation`, cast-position synthetic hoặc width formula.
- Main name chỉ chấp nhận `Yasuo_Base_W_windwall1`..`5`; endpoint chỉ chấp nhận `YasuoWChildMis`, case-insensitive.
- Drawing và collision phải đọc cùng `YasuoWallTracker` snapshot.
- Workspace hiện không có `.git/HEAD` hợp lệ. Không tự `git init`; commit steps phải in trạng thái skip thay vì thay đổi repository metadata.
- Design source: `docs/superpowers/specs/2026-07-10-yasuo-wall-object-composition-rewrite-design.md`.

## File Structure

- **Create** `sdk/Math/YasuoWallModel.h` — pure state, name parsing, endpoint pairing và capsule geometry.
- **Create** `sdk/GameObjects/YasuoWallTracker.h` — object-event/runtime adapter, tick guard và snapshot API.
- **Create** `tests/yasuo_wall_model_test.cpp` — behavioral tests cho toàn bộ pure model.
- **Create** `tests/yasuo_wall_tracker_compile_test.cpp` — compile contract cho tracker public API.
- **Create** `tests/yasuo_wall_collision_api_test.cpp` — compile/runtime contract cho barrier API và default flags.
- **Modify** `core/offset.h` — xóa hai offset EffectEmitter transform sai.
- **Modify** `sdk/Core/Objects.h` — giữ `EffectEmitter` type nhưng xóa transform methods sai.
- **Modify** `sdk/Math/Collision.h` — dùng tracker endpoints và tách semantics Yasuo/Samira/Mel.
- **Modify** `sdk/Enumerations/CollisionableObjects.h` — default bridge chứa đủ ba projectile barriers.
- **Modify** `sdk/Wrappers/Spells/Spell.h` — default spell collision chứa đủ ba projectile barriers.
- **Modify** `sdk/Wrappers/Orbwalking/OrbwalkerBase.h` — gọi combined projectile-barrier API.
- **Modify** `sdk/Math/Prediction/Movement.h` — xóa stale WindwallTracker comment.
- **Replace** `plugins/Utility/YasuoWallDebugPlugin.h` — renderer chỉ đọc tracker snapshots.
- **Delete** `sdk/Math/WindwallTracker.h`, `sdk/Math/WindwallGeometry.h`, `tests/windwall_geometry_test.cpp`.

---

### Task 1: Pure Yasuo Wall model — RED/GREEN

**Files:**
- Create: `tests/yasuo_wall_model_test.cpp`
- Create: `sdk/Math/YasuoWallModel.h`

**Interfaces:**
- Produces namespace `SDK::YasuoWallModel`.
- Produces `Identity`, `ObjectRole`, `ObjectState`, `WallSegment`, `Registry`.
- Produces `ClassifyName(std::string_view)`, `ParseMainLevel(std::string_view)` and `IntersectsProjectilePath(...)`.
- `Registry::ActiveWalls()` is consumed by the runtime tracker in Task 2.

- [ ] **Step 1: Write the failing behavioral test**

Create `tests/yasuo_wall_model_test.cpp`:

```cpp
#include <cmath>
#include <cstdio>

#include "../sdk/Math/YasuoWallModel.h"

namespace M = SDK::YasuoWallModel;

namespace {
int failures = 0;

void Expect(bool condition, const char* name) {
    if (!condition) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

void ExpectNear(float actual, float expected, const char* name) {
    if (std::fabs(actual - expected) > 0.01f) {
        std::printf("FAIL: %s expected %.2f got %.2f\n", name, expected, actual);
        ++failures;
    }
}

M::Identity Id(std::uint32_t networkId, std::uint32_t index = 1) {
    return {static_cast<std::uintptr_t>(0x100000 + networkId), networkId, index};
}

void AddSampleWall(M::Registry& registry, int tick = 1000) {
    registry.OnCreate(Id(100, 10), tick, "Yasuo_Base_W_windwall1", {922.76f, 644.08f});
    registry.OnCreate(Id(101, 11), tick + 5, "YasuoWChildMis", {907.37f, 803.34f});
    registry.OnCreate(Id(102, 12), tick + 7, "YasuoWChildMis", {939.40f, 484.95f});
}

void TestNames() {
    for (int level = 1; level <= 5; ++level) {
        const std::string name = "Yasuo_Base_W_windwall" + std::to_string(level);
        Expect(M::ClassifyName(name) == M::ObjectRole::Main, "main name accepted");
        Expect(M::ParseMainLevel(name) == level, "main level parsed");
    }
    Expect(M::ClassifyName("yAsUoWcHiLdMiS") == M::ObjectRole::Endpoint,
           "endpoint case-insensitive");
    Expect(M::ClassifyName("YasuoW_VisualMis") == M::ObjectRole::Visual,
           "visual classified");
    Expect(M::ClassifyName("Yasuo_Base_W_windwall_activate") == M::ObjectRole::Other,
           "activate rejected");
    Expect(M::ClassifyName("Yasuo_Base_W_windwall_big_impact") == M::ObjectRole::Other,
           "impact rejected");
    Expect(M::ClassifyName("Yasuo_Base_W_windwall_groud_crack.tex") == M::ObjectRole::Other,
           "ground crack rejected");
}

void TestMeasuredEndpoints() {
    M::Registry registry;
    AddSampleWall(registry);
    registry.Refresh(1010);
    const auto& walls = registry.ActiveWalls();
    Expect(walls.size() == 1, "one wall assembled");
    if (!walls.empty()) {
        ExpectNear(walls[0].Span(), 320.0f, "CE sample span");
        ExpectNear(walls[0].center.x, 922.76f, "main center x");
        ExpectNear(walls[0].center.y, 644.08f, "main center z");
    }
}

void TestDelayedNameAndLifecycle() {
    M::Registry registry;
    registry.OnCreate(Id(200, 20), 1000, "", {0.0f, 0.0f});
    registry.OnCreate(Id(201, 21), 1001, "YasuoWChildMis", {-160.0f, 0.0f});
    registry.OnCreate(Id(202, 22), 1002, "YasuoWChildMis", {160.0f, 0.0f});
    registry.Refresh(1010);
    Expect(registry.ActiveWalls().empty(), "pending main not published");

    registry.Update(Id(200, 20), 1100, "Yasuo_Base_W_windwall2", {0.0f, 0.0f});
    registry.Refresh(1100);
    Expect(registry.ActiveWalls().size() == 1, "late name publishes wall");

    registry.OnDelete(Id(201, 21));
    registry.Refresh(1101);
    Expect(registry.ActiveWalls().empty(), "delete invalidates wall");

    M::Registry pending;
    pending.OnCreate(Id(300, 30), 1000, "", {0.0f, 0.0f});
    pending.Refresh(1501);
    Expect(pending.Entries().empty(), "pending expires after 500 ms");

    M::Registry expired;
    AddSampleWall(expired, 1000);
    expired.Refresh(6001);
    Expect(expired.ActiveWalls().empty(), "wall hard-expires after 5000 ms");
}

void TestPairingAndTieBreak() {
    M::Registry registry;
    registry.OnCreate(Id(400, 40), 1000, "Yasuo_Base_W_windwall3", {0.0f, 0.0f});
    registry.OnCreate(Id(410, 41), 1000, "YasuoWChildMis", {-160.0f, 0.0f});
    registry.OnCreate(Id(411, 42), 1000, "YasuoWChildMis", {160.0f, 0.0f});
    registry.OnCreate(Id(420, 43), 1000, "YasuoWChildMis", {0.0f, -160.0f});
    registry.OnCreate(Id(421, 44), 1000, "YasuoWChildMis", {0.0f, 160.0f});
    registry.OnCreate(Id(430, 45), 1000, "YasuoWChildMis", {1000.0f, 1000.0f});
    registry.Refresh(1010);
    const auto& walls = registry.ActiveWalls();
    Expect(walls.size() == 1, "one deterministic pair");
    if (!walls.empty()) {
        Expect(walls[0].endpointA.networkId == 410, "lower-id endpoint A selected");
        Expect(walls[0].endpointB.networkId == 411, "lower-id endpoint B selected");
    }

    M::Registry missing;
    missing.OnCreate(Id(500, 50), 1000, "Yasuo_Base_W_windwall1", {0.0f, 0.0f});
    missing.OnCreate(Id(501, 51), 1000, "YasuoWChildMis", {160.0f, 0.0f});
    missing.Refresh(1001);
    Expect(missing.ActiveWalls().empty(), "one endpoint is insufficient");
}

void TestCollisionGeometry() {
    const M::WallSegment wall = {
        Id(600), Id(601), Id(602), 1, 1000,
        {0.0f, 0.0f}, {0.0f, -100.0f}, {0.0f, 100.0f}
    };
    Expect(M::IntersectsProjectilePath({-50.0f, 0.0f}, {50.0f, 0.0f}, wall, 0.0f),
           "crossing path collides");
    Expect(!M::IntersectsProjectilePath({-50.0f, 120.0f}, {50.0f, 120.0f}, wall, 10.0f),
           "outside radius misses");
    Expect(M::IntersectsProjectilePath({-50.0f, 110.0f}, {50.0f, 110.0f}, wall, 10.0f),
           "capsule radius collides");
    Expect(M::IntersectsProjectilePath({0.0f, 105.0f}, {0.0f, 105.0f}, wall, 5.0f),
           "endpoint radius collides");
}
} // namespace

int main() {
    TestNames();
    TestMeasuredEndpoints();
    TestDelayedNameAndLifecycle();
    TestPairingAndTieBreak();
    TestCollisionGeometry();
    if (failures == 0) {
        std::printf("ALL YASUO WALL MODEL TESTS PASSED\n");
    }
    return failures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Run RED and verify the missing model is the failure**

Run from `C:\Users\MR THINH\Downloads\New\NightSharp`:

```powershell
cmd /d /s /c ""E:\Visual Studio\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /std:c++20 /EHsc /nologo tests\yasuo_wall_model_test.cpp /Fe:build\yasuo_wall_model_test.exe"
```

Expected: compiler failure `cannot open include file: '../sdk/Math/YasuoWallModel.h'`.

- [ ] **Step 3: Implement the pure model**

Create `sdk/Math/YasuoWallModel.h` with these complete public contracts and algorithms:

```cpp
#pragma once

#include "../../core/Vector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace SDK::YasuoWallModel {

inline constexpr int kPendingNameLifetimeMs = 500;
inline constexpr int kWallLifetimeMs = 5000;
inline constexpr int kPairWindowMs = 500;
inline constexpr float kMaxMidpointDistance = 75.0f;

enum class ObjectRole { Other, Main, Endpoint, Visual };

struct Identity {
    std::uintptr_t address = 0;
    std::uint32_t networkId = 0;
    std::uint32_t index = 0;
    bool IsValid() const { return networkId != 0 && networkId != 0xFFFFFFFFu; }
};

inline bool SameIdentity(const Identity& a, const Identity& b) {
    if (a.IsValid() && b.IsValid()) return a.networkId == b.networkId;
    return a.address != 0 && a.address == b.address && a.index == b.index;
}

inline bool IdentityLess(const Identity& a, const Identity& b) {
    return std::tie(a.networkId, a.index, a.address) <
           std::tie(b.networkId, b.index, b.address);
}

inline std::string LowerAscii(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch);
    });
    return out;
}

inline int ParseMainLevel(std::string_view name) {
    const std::string lower = LowerAscii(name);
    constexpr std::string_view prefix = "yasuo_base_w_windwall";
    if (lower.size() != prefix.size() + 1 || lower.compare(0, prefix.size(), prefix) != 0) {
        return 0;
    }
    const char suffix = lower.back();
    return suffix >= '1' && suffix <= '5' ? suffix - '0' : 0;
}

inline ObjectRole ClassifyName(std::string_view name) {
    const std::string lower = LowerAscii(name);
    if (ParseMainLevel(lower) != 0) return ObjectRole::Main;
    if (lower == "yasuowchildmis") return ObjectRole::Endpoint;
    if (lower == "yasuow_visualmis") return ObjectRole::Visual;
    return ObjectRole::Other;
}

struct ObjectState {
    Identity identity = {};
    ObjectRole role = ObjectRole::Other;
    int level = 0;
    int createdTick = 0;
    Vec2 position = {};
    bool pendingName = false;
};

struct WallSegment {
    Identity main = {};
    Identity endpointA = {};
    Identity endpointB = {};
    int level = 1;
    int spawnTick = 0;
    Vec2 center = {};
    Vec2 start = {};
    Vec2 end = {};
    float Span() const { return start.Distance(end); }
};

inline float PointSegmentDistanceSqr(Vec2 point, Vec2 start, Vec2 end) {
    const Vec2 segment = end - start;
    const float lengthSqr = segment.LengthSqr();
    if (lengthSqr <= 0.000001f) return point.DistanceSqr(start);
    const float t = std::clamp((point - start).Dot(segment) / lengthSqr, 0.0f, 1.0f);
    return point.DistanceSqr(start + segment * t);
}

inline float Cross(Vec2 a, Vec2 b, Vec2 c) { return (b - a).Cross(c - a); }

inline bool OnSegment(Vec2 a, Vec2 b, Vec2 point) {
    constexpr float epsilon = 0.0001f;
    return std::fabs(Cross(a, b, point)) <= epsilon &&
           point.x >= std::min(a.x, b.x) - epsilon &&
           point.x <= std::max(a.x, b.x) + epsilon &&
           point.y >= std::min(a.y, b.y) - epsilon &&
           point.y <= std::max(a.y, b.y) + epsilon;
}

inline bool SegmentsIntersect(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    const float abC = Cross(a, b, c), abD = Cross(a, b, d);
    const float cdA = Cross(c, d, a), cdB = Cross(c, d, b);
    if (((abC > 0.0f && abD < 0.0f) || (abC < 0.0f && abD > 0.0f)) &&
        ((cdA > 0.0f && cdB < 0.0f) || (cdA < 0.0f && cdB > 0.0f))) return true;
    return OnSegment(a, b, c) || OnSegment(a, b, d) ||
           OnSegment(c, d, a) || OnSegment(c, d, b);
}

inline float SegmentDistanceSqr(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    if (SegmentsIntersect(a, b, c, d)) return 0.0f;
    return std::min({PointSegmentDistanceSqr(a, c, d),
                     PointSegmentDistanceSqr(b, c, d),
                     PointSegmentDistanceSqr(c, a, b),
                     PointSegmentDistanceSqr(d, a, b)});
}

inline bool IntersectsProjectilePath(Vec2 pathStart, Vec2 pathEnd,
                                     const WallSegment& wall, float radius) {
    if (!pathStart.IsValid() || !pathEnd.IsValid() ||
        !wall.start.IsValid() || !wall.end.IsValid() ||
        !std::isfinite(radius) || radius < 0.0f) return false;
    return SegmentDistanceSqr(pathStart, pathEnd, wall.start, wall.end) <=
           radius * radius + 0.0001f;
}

class Registry {
public:
    void OnCreate(Identity identity, int tick, std::string_view name, Vec2 position) {
        if (!identity.IsValid()) return;
        const ObjectRole role = ClassifyName(name);
        if (!name.empty() && role == ObjectRole::Other) return;
        auto it = Find(identity);
        if (it == entries_.end()) {
            entries_.push_back({identity, role, ParseMainLevel(name), tick, position, name.empty()});
            return;
        }
        Update(identity, tick, name, position);
    }

    void Update(Identity identity, int tick, std::string_view name, Vec2 position) {
        auto it = Find(identity);
        if (it == entries_.end()) {
            OnCreate(identity, tick, name, position);
            return;
        }
        if (!name.empty()) {
            const ObjectRole role = ClassifyName(name);
            if (role == ObjectRole::Other) {
                entries_.erase(it);
                return;
            }
            it->role = role;
            it->level = ParseMainLevel(name);
            it->pendingName = false;
        }
        if (position.IsValid()) it->position = position;
        it->identity = identity;
    }

    void OnDelete(const Identity& identity) {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
            [&](const ObjectState& item) { return SameIdentity(item.identity, identity); }),
            entries_.end());
        RefreshWalls();
    }

    void Refresh(int now) {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
            [&](const ObjectState& item) {
                const int age = now - item.createdTick;
                return (item.pendingName && age > kPendingNameLifetimeMs) ||
                       (!item.pendingName && age > kWallLifetimeMs);
            }), entries_.end());
        RefreshWalls();
    }

    const std::vector<ObjectState>& Entries() const { return entries_; }
    const std::vector<WallSegment>& ActiveWalls() const { return walls_; }

private:
    std::vector<ObjectState> entries_;
    std::vector<WallSegment> walls_;

    auto Find(const Identity& identity) {
        return std::find_if(entries_.begin(), entries_.end(),
            [&](const ObjectState& item) { return SameIdentity(item.identity, identity); });
    }

    void RefreshWalls() {
        walls_.clear();
        std::vector<const ObjectState*> mains;
        std::vector<const ObjectState*> endpoints;
        for (const auto& item : entries_) {
            if (item.role == ObjectRole::Main) mains.push_back(&item);
            if (item.role == ObjectRole::Endpoint) endpoints.push_back(&item);
        }
        std::sort(mains.begin(), mains.end(), [](const auto* a, const auto* b) {
            if (a->createdTick != b->createdTick) return a->createdTick < b->createdTick;
            return IdentityLess(a->identity, b->identity);
        });
        std::vector<bool> used(endpoints.size(), false);
        for (const auto* main : mains) {
            std::optional<std::tuple<float, int, std::uint32_t, std::uint32_t,
                                     std::size_t, std::size_t>> best;
            for (std::size_t i = 0; i < endpoints.size(); ++i) {
                if (used[i]) continue;
                for (std::size_t j = i + 1; j < endpoints.size(); ++j) {
                    if (used[j] || SameIdentity(endpoints[i]->identity, endpoints[j]->identity)) continue;
                    const int dtA = std::abs(endpoints[i]->createdTick - main->createdTick);
                    const int dtB = std::abs(endpoints[j]->createdTick - main->createdTick);
                    if (dtA > kPairWindowMs || dtB > kPairWindowMs ||
                        endpoints[i]->position.DistanceSqr(endpoints[j]->position) < 1.0f) continue;
                    const Vec2 midpoint = (endpoints[i]->position + endpoints[j]->position) * 0.5f;
                    const float midpointDistance = midpoint.DistanceSqr(main->position);
                    if (midpointDistance > kMaxMidpointDistance * kMaxMidpointDistance) continue;
                    const std::uint32_t low = std::min(endpoints[i]->identity.networkId,
                                                       endpoints[j]->identity.networkId);
                    const std::uint32_t high = std::max(endpoints[i]->identity.networkId,
                                                        endpoints[j]->identity.networkId);
                    const auto score = std::make_tuple(midpointDistance, dtA + dtB, low, high, i, j);
                    if (!best || score < *best) best = score;
                }
            }
            if (!best) continue;
            const std::size_t i = std::get<4>(*best), j = std::get<5>(*best);
            const ObjectState* a = endpoints[i];
            const ObjectState* b = endpoints[j];
            if (IdentityLess(b->identity, a->identity)) std::swap(a, b);
            walls_.push_back({main->identity, a->identity, b->identity,
                              main->level, main->createdTick, main->position,
                              a->position, b->position});
            used[i] = true;
            used[j] = true;
        }
    }
};

} // namespace SDK::YasuoWallModel
```

- [ ] **Step 4: Run GREEN**

```powershell
cmd /d /s /c ""E:\Visual Studio\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /std:c++20 /EHsc /nologo tests\yasuo_wall_model_test.cpp /Fe:build\yasuo_wall_model_test.exe && build\yasuo_wall_model_test.exe"
```

Expected: `ALL YASUO WALL MODEL TESTS PASSED` and exit code `0`.

- [ ] **Step 5: Commit or record the no-Git constraint**

```powershell
if (Test-Path '.git\HEAD') { git add sdk/Math/YasuoWallModel.h tests/yasuo_wall_model_test.cpp; git commit -m "feat: add pure Yasuo wall model" } else { Write-Output 'SKIP COMMIT: no valid .git/HEAD' }
```

Expected in this workspace: `SKIP COMMIT: no valid .git/HEAD`.

---

### Task 2: Runtime tracker adapter

**Files:**
- Create: `tests/yasuo_wall_tracker_compile_test.cpp`
- Create: `sdk/GameObjects/YasuoWallTracker.h`

**Interfaces:**
- Consumes `YasuoWallModel::Registry`.
- Produces `SDK::YasuoWallTracker::WallSnapshot`.
- Produces `EnsureInitialized()`, `Refresh()`, `ActiveWalls()` by value and `Intersects(...)`.

- [ ] **Step 1: Write the failing compile contract**

Create `tests/yasuo_wall_tracker_compile_test.cpp`:

```cpp
#include <type_traits>
#include <vector>

#include "../sdk/GameObjects/YasuoWallTracker.h"

static_assert(std::is_same_v<
    decltype(SDK::YasuoWallTracker::ActiveWalls()),
    std::vector<SDK::YasuoWallTracker::WallSnapshot>>);
static_assert(std::is_same_v<
    decltype(SDK::YasuoWallTracker::Intersects(Vec3{}, Vec3{}, 0.0f)), bool>);

int main() { return 0; }
```

- [ ] **Step 2: Run RED**

```powershell
cmd /d /s /c ""E:\Visual Studio\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /std:c++20 /EHsc /nologo /c tests\yasuo_wall_tracker_compile_test.cpp /Fo:build\yasuo_wall_tracker_compile_test.obj"
```

Expected: missing `sdk/GameObjects/YasuoWallTracker.h`.

- [ ] **Step 3: Implement the process-bound adapter**

Create `sdk/GameObjects/YasuoWallTracker.h`. The implementation must use exactly this state flow:

```cpp
#pragma once

#include "../../core/CoreObjectManager.h"
#include "../../core/CoreObjects.h"
#include "../../core/Globals.h"
#include "../Events/Events.h"
#include "../Math/YasuoWallModel.h"
#include "../Variables.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace SDK::YasuoWallTracker {

struct WallSnapshot {
    ::Core::Objects::ObjectHandle main = {};
    ::Core::Objects::ObjectHandle endpointA = {};
    ::Core::Objects::ObjectHandle endpointB = {};
    int level = 1;
    int spawnTick = 0;
    Vec3 center = {};
    Vec3 start = {};
    Vec3 end = {};
    float Span() const { return start.To2D().Distance(end.To2D()); }
};

namespace detail {
inline std::mutex g_mutex;
inline YasuoWallModel::Registry g_registry;
inline std::vector<WallSnapshot> g_active;
inline bool g_createSubscribed = false;
inline bool g_deleteSubscribed = false;
inline bool g_seeded = false;
inline std::uintptr_t g_runtimeBase = 0;
inline int g_lastRefreshTick = -1;

inline YasuoWallModel::Identity ToIdentity(const ::Core::Objects::ObjectHandle& handle) {
    return {handle.address, handle.networkId, handle.index};
}

inline YasuoWallModel::Identity ToIdentity(const ::Core::Events::ObjectInfo& info) {
    return {info.Ptr, info.NetworkId, info.Index};
}

inline ::Core::Objects::ObjectHandle ToHandle(const YasuoWallModel::Identity& identity) {
    return {identity.address, identity.networkId, identity.index,
            ::Core::Objects::ObjectType::GameObject};
}

inline std::string ResolveName(std::uintptr_t address, const char* eventName) {
    if (eventName && eventName[0]) return eventName;
    char buffer[128] = {};
    return ::Core::Objects::ReadName(address, buffer, static_cast<int>(sizeof(buffer)))
        ? std::string(buffer) : std::string();
}

inline void ObserveLocked(const ::Core::Events::ObjectInfo& info, int now) {
    if (!info.Ptr) return;
    const std::string name = ResolveName(info.Ptr, info.Name);
    const Vec3 position = info.Position.IsValid()
        ? info.Position : ::Core::Objects::ReadPosition(info.Ptr);
    g_registry.OnCreate(ToIdentity(info), now, name, position.To2D());
}

inline void OnCreate(const Events::ObjectEventArgs& args) {
    std::lock_guard<std::mutex> lock(g_mutex);
    ObserveLocked(args.Sender, Variables::TickCount());
}

inline void OnDelete(const Events::ObjectEventArgs& args) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_registry.OnDelete(ToIdentity(args.Sender));
    g_active.clear();
}

inline void SeedLocked(int now) {
    std::uintptr_t objects[16384] = {};
    const int count = ::Core::ObjectManager::EnumerateAll(objects, 16384);
    if (count <= 0) return;
    for (int i = 0; i < count; ++i) {
        const auto handle = ::Core::ObjectManager::MakeHandle(objects[i]);
        char name[128] = {};
        if (!handle.IsValid() ||
            !::Core::Objects::ReadName(objects[i], name, static_cast<int>(sizeof(name)))) continue;
        g_registry.OnCreate(ToIdentity(handle), now, name,
                            ::Core::Objects::ReadPosition(objects[i]).To2D());
    }
    g_seeded = true;
}

inline void ResetLocked(int now) {
    g_registry = {};
    g_active.clear();
    g_lastRefreshTick = -1;
    g_runtimeBase = Globals::base;
    g_seeded = false;
    SeedLocked(now);
}

inline void RefreshLocked(int now) {
    if (g_runtimeBase != Globals::base) ResetLocked(now);
    if (!g_seeded) SeedLocked(now);
    if (g_lastRefreshTick == now) return;
    g_lastRefreshTick = now;

    const auto entries = g_registry.Entries();
    for (const auto& entry : entries) {
        auto handle = ToHandle(entry.identity);
        if (!::Core::ObjectManager::Resolve(handle)) {
            g_registry.OnDelete(entry.identity);
            continue;
        }
        std::string name;
        if (entry.pendingName) name = ResolveName(handle.address, nullptr);
        g_registry.Update(ToIdentity(handle), now, name,
                          ::Core::Objects::ReadPosition(handle.address).To2D());
    }
    g_registry.Refresh(now);

    g_active.clear();
    for (const auto& wall : g_registry.ActiveWalls()) {
        auto main = ToHandle(wall.main);
        auto endpointA = ToHandle(wall.endpointA);
        auto endpointB = ToHandle(wall.endpointB);
        if (!::Core::ObjectManager::Resolve(main) ||
            !::Core::ObjectManager::Resolve(endpointA) ||
            !::Core::ObjectManager::Resolve(endpointB)) continue;
        const Vec3 mainPosition = ::Core::Objects::ReadPosition(main.address);
        g_active.push_back({main, endpointA, endpointB, wall.level, wall.spawnTick,
                            mainPosition,
                            Vec3(wall.start.x, mainPosition.y, wall.start.y),
                            Vec3(wall.end.x, mainPosition.y, wall.end.y)});
    }
}
} // namespace detail

inline void EnsureInitialized() {
    std::lock_guard<std::mutex> lock(detail::g_mutex);
    if (!detail::g_createSubscribed)
        detail::g_createSubscribed = Events::AddOnCreateObject(&detail::OnCreate);
    if (!detail::g_deleteSubscribed)
        detail::g_deleteSubscribed = Events::AddOnDeleteObject(&detail::OnDelete);
    const int now = Variables::TickCount();
    if (detail::g_runtimeBase != Globals::base) detail::ResetLocked(now);
    if (!detail::g_seeded) detail::SeedLocked(now);
}

inline void Refresh() {
    EnsureInitialized();
    std::lock_guard<std::mutex> lock(detail::g_mutex);
    detail::RefreshLocked(Variables::TickCount());
}

inline std::vector<WallSnapshot> ActiveWalls() {
    Refresh();
    std::lock_guard<std::mutex> lock(detail::g_mutex);
    return detail::g_active;
}

inline bool Intersects(const Vec3& pathStart, const Vec3& pathEnd, float radius) {
    for (const auto& wall : ActiveWalls()) {
        YasuoWallModel::WallSegment segment;
        segment.start = wall.start.To2D();
        segment.end = wall.end.To2D();
        if (YasuoWallModel::IntersectsProjectilePath(
                pathStart.To2D(), pathEnd.To2D(), segment, std::max(radius, 0.0f))) return true;
    }
    return false;
}

} // namespace SDK::YasuoWallTracker
```

- [ ] **Step 4: Run tracker compile GREEN and model regression**

```powershell
cmd /d /s /c ""E:\Visual Studio\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /std:c++20 /EHsc /nologo /c tests\yasuo_wall_tracker_compile_test.cpp /Fo:build\yasuo_wall_tracker_compile_test.obj && build\yasuo_wall_model_test.exe"
```

Expected: compile succeeds; model test prints `ALL YASUO WALL MODEL TESTS PASSED`.

- [ ] **Step 5: Commit or record skip**

```powershell
if (Test-Path '.git\HEAD') { git add sdk/GameObjects/YasuoWallTracker.h tests/yasuo_wall_tracker_compile_test.cpp; git commit -m "feat: track Yasuo wall object composition" } else { Write-Output 'SKIP COMMIT: no valid .git/HEAD' }
```

---

### Task 3: Remove invalid EffectEmitter transform API

**Files:**
- Modify: `tests/yasuo_wall_tracker_compile_test.cpp`
- Modify: `core/offset.h:963-977`
- Modify: `sdk/Core/Objects.h:1899-1949`

**Interfaces:**
- Keeps `SDK::EffectEmitter` constructors and object type.
- Removes all unverified transform access.

- [ ] **Step 1: Add a failing absence contract**

Append before `main()` in `tests/yasuo_wall_tracker_compile_test.cpp`:

```cpp
template <typename T>
concept HasOrientation = requires(const T& value) { value.Orientation(); };
static_assert(!HasOrientation<SDK::EffectEmitter>);
```

- [ ] **Step 2: Run RED**

Use the Task 2 compile command.

Expected: static assertion fails because `EffectEmitter::Orientation()` still exists.

- [ ] **Step 3: Delete only the invalid API**

In `core/offset.h`, delete:

```cpp
constexpr auto EffectEmitterHandle = 0x258;
```

and delete the complete `namespace EffectEmitterLayout` block containing `ProxyOrientation`.

Replace the `EffectEmitter` class body in `sdk/Core/Objects.h` with:

```cpp
class EffectEmitter : public GameObject {
public:
    EffectEmitter() = default;
    explicit EffectEmitter(uintptr_t address)
        : GameObject(address, ::Core::Objects::ObjectType::EffectEmitter) {}
    explicit EffectEmitter(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {
        handle_.type = ::Core::Objects::ObjectType::EffectEmitter;
    }
};
```

- [ ] **Step 4: Run GREEN and legacy-symbol guard**

```powershell
cmd /d /s /c ""E:\Visual Studio\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /std:c++20 /EHsc /nologo /c tests\yasuo_wall_tracker_compile_test.cpp /Fo:build\yasuo_wall_tracker_compile_test.obj"
if (rg -n "EffectEmitterHandle|ProxyOrientation|ResolveProxy\(|Orientation\(\) const" core sdk plugins) { throw 'legacy EffectEmitter transform remains' }
```

Expected: compile succeeds and `rg` has no matches.

- [ ] **Step 5: Commit or record skip**

```powershell
if (Test-Path '.git\HEAD') { git add core/offset.h sdk/Core/Objects.h tests/yasuo_wall_tracker_compile_test.cpp; git commit -m "fix: remove invalid EffectEmitter transform layout" } else { Write-Output 'SKIP COMMIT: no valid .git/HEAD' }
```

---

### Task 4: Collision API integration and semantics cleanup

**Files:**
- Create: `tests/yasuo_wall_collision_api_test.cpp`
- Modify: `sdk/Math/Collision.h:24-25,82-90,328-357,620-640,741-764`
- Modify: `sdk/Enumerations/CollisionableObjects.h:92-95`
- Modify: `sdk/Wrappers/Spells/Spell.h:109-115`
- Modify: `sdk/Wrappers/Orbwalking/OrbwalkerBase.h:3108-3110`
- Modify: `sdk/Math/Prediction/Movement.h:1102-1104`

**Interfaces:**
- Consumes `YasuoWallTracker::Intersects`.
- Keeps `HasYasuoWindWallCollision` but makes it Yasuo-only.
- Produces `HasProjectileWallCollision` for the combined Yasuo/Samira/Mel policy.

- [ ] **Step 1: Write the failing API/default test**

Create `tests/yasuo_wall_collision_api_test.cpp`:

```cpp
#include <type_traits>

#include "../sdk/Math/Collision.h"

static_assert(std::is_same_v<decltype(SDK::Collision::HasProjectileWallCollision(
    Vec3{}, Vec3{}, 0.0f)), bool>);

int main() {
    SDK::CollisionObjectsBridge defaults;
    return defaults.contains(SDK::CollisionableObjects::YasuoWall) &&
           defaults.contains(SDK::CollisionableObjects::SamiraWall) &&
           defaults.contains(SDK::CollisionableObjects::MelWall) ? 0 : 1;
}
```

- [ ] **Step 2: Run RED**

```powershell
cmd /d /s /c ""E:\Visual Studio\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /std:c++20 /EHsc /nologo tests\yasuo_wall_collision_api_test.cpp /Fe:build\yasuo_wall_collision_api_test.exe"
```

Expected: compile failure because `HasProjectileWallCollision` does not exist.

- [ ] **Step 3: Replace old tracker includes and initialization**

In `sdk/Math/Collision.h`, replace old Windwall includes with:

```cpp
#include "../GameObjects/YasuoWallTracker.h"
```

In `detail::Initialize()`, replace both `WindwallTracker::EnsureInitialized()` calls with:

```cpp
YasuoWallTracker::EnsureInitialized();
```

Replace `SegmentIntersectsWindwall(...)` completely with:

```cpp
inline bool SegmentIntersectsYasuoWall(const Vector3& start,
                                       const Vector3& end,
                                       float projectileRadius) {
    Initialize();
    RefreshChampionFlags();
    return YasuoInGame &&
           YasuoWallTracker::Intersects(start, end, std::max(projectileRadius, 0.0f));
}
```

- [ ] **Step 4: Give each collision flag exact semantics**

Replace the barrier section of `ProcessProjectileWalls` with:

```cpp
if (ContainsCollisionObject(input, CollisionableObjects::YasuoWall) &&
    SegmentIntersectsYasuoWall(from, position, input.Radius)) {
    AddPlayerSentinel(result);
}
if (ContainsCollisionObject(input, CollisionableObjects::SamiraWall) &&
    HasSamiraCollision(from, position, input.Radius)) {
    AddPlayerSentinel(result);
}
if (ContainsCollisionObject(input, CollisionableObjects::MelWall) &&
    HasMelCollision(from, position, input.Radius)) {
    AddPlayerSentinel(result);
}
```

Replace the three public barrier functions/`IsCollision` tail with:

```cpp
inline bool HasYasuoWindWallCollision(const Vector3& start,
                                      const Vector3& end) {
    return HasYasuoWindWallCollision(start, end, 50.0f);
}

inline bool HasYasuoWindWallCollision(const Vector3& start,
                                      const Vector3& end,
                                      float extraRadius) {
    Initialize();
    return detail::SegmentIntersectsYasuoWall(start, end, extraRadius);
}

inline bool HasProjectileWallCollision(const Vector3& start,
                                       const Vector3& end,
                                       float extraRadius = 0.0f) {
    return HasYasuoWindWallCollision(start, end, extraRadius) ||
           HasSamiraWallCollision(start, end, extraRadius) ||
           HasMelWallCollision(start, end, extraRadius);
}

inline bool IsCollision(const Vector3& position, float radius = 50.0f) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;
    return HasProjectileWallCollision(
        detail::ServerPositionOrPosition(player), position, radius);
}
```

Update the top forward declarations to include both overloads plus:

```cpp
inline bool HasProjectileWallCollision(const Vector3& start,
                                       const Vector3& end,
                                       float extraRadius);
```

- [ ] **Step 5: Update defaults and Orbwalker consumer**

Set `CollisionObjectsBridge::Flags` in `sdk/Enumerations/CollisionableObjects.h` to:

```cpp
CollisionableObjects::Minions |
CollisionableObjects::YasuoWall |
CollisionableObjects::SamiraWall |
CollisionableObjects::MelWall;
```

Set `Spell::CollisionObjects` default in `sdk/Wrappers/Spells/Spell.h` to:

```cpp
CollisionableObjects::Minions |
CollisionableObjects::Heroes |
CollisionableObjects::YasuoWall |
CollisionableObjects::SamiraWall |
CollisionableObjects::MelWall;
```

In `OrbwalkerBase.h`, replace the Yasuo-only call with:

```cpp
!Collisions::HasProjectileWallCollision(
    player.ServerPosition(), target.Position(), 0.0f);
```

Delete the stale `WindwallTracker` comment in `Movement.h`; do not add a second Yasuo collision path there.

- [ ] **Step 6: Run GREEN and model regression**

```powershell
cmd /d /s /c ""E:\Visual Studio\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /std:c++20 /EHsc /nologo tests\yasuo_wall_collision_api_test.cpp /Fe:build\yasuo_wall_collision_api_test.exe && build\yasuo_wall_collision_api_test.exe && build\yasuo_wall_model_test.exe"
```

Expected: both executables exit `0`.

- [ ] **Step 7: Commit or record skip**

```powershell
if (Test-Path '.git\HEAD') { git add sdk/Math/Collision.h sdk/Enumerations/CollisionableObjects.h sdk/Wrappers/Spells/Spell.h sdk/Wrappers/Orbwalking/OrbwalkerBase.h sdk/Math/Prediction/Movement.h tests/yasuo_wall_collision_api_test.cpp; git commit -m "feat: use live Yasuo wall endpoints for collision" } else { Write-Output 'SKIP COMMIT: no valid .git/HEAD' }
```

---

### Task 5: Rewrite the debug plugin as a tracker-only renderer

**Files:**
- Replace: `plugins/Utility/YasuoWallDebugPlugin.h`

**Interfaces:**
- Consumes `YasuoWallTracker::ActiveWalls()` by value.
- Keeps plugin ID `utility.yasuo_wall_debug`.

- [ ] **Step 1: Run a failing legacy-dependency guard**

```powershell
if (rg -n "WindwallTracker|WindwallGeo|Orientation\(|Direction\(" plugins\Utility\YasuoWallDebugPlugin.h) { throw 'legacy debug plugin still active' }
```

Expected: command throws because the old plugin still contains those symbols.

- [ ] **Step 2: Replace the plugin implementation**

Replace the file with:

```cpp
#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../sdk/GameObjects/YasuoWallTracker.h"
#include "../../imgui/imgui.h"

#include <cstdio>
#include <cstdint>

namespace Plugins {

class YasuoWallDebugPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Yasuo Wall Debug"; }
    const char* GetInternalId() const override { return "utility.yasuo_wall_debug"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        SDK::YasuoWallTracker::EnsureInitialized();
        NightSharpDebug::Logf("[YasuoWallDebug] loaded");
    }

    void OnUnload() override {
        NightSharpDebug::Logf("[YasuoWallDebug] unloaded");
    }

    void OnUpdate() override {}

    void OnRender() override {
        if (!drawEnabled_ || !ImGui::GetCurrentContext() || !SDK::Drawing::IsEnabled()) return;
        for (const auto& wall : SDK::YasuoWallTracker::ActiveWalls()) DrawWall(wall);
    }

    void OnMenu() override {
        ImGui::Checkbox("Draw Yasuo walls", &drawEnabled_);
        const auto walls = SDK::YasuoWallTracker::ActiveWalls();
        ImGui::Text("Active walls: %d", static_cast<int>(walls.size()));
    }

private:
    bool drawEnabled_ = true;

    static void DrawTextWorld(const Vec3& world, const char* text,
                              std::uint32_t color, float offsetY) {
        Vec2 screen = {};
        if (SDK::Drawing::WorldToScreen(world, screen))
            SDK::Drawing::DrawText(screen.x, screen.y + offsetY, color, text);
    }

    static void DrawWall(const SDK::YasuoWallTracker::WallSnapshot& wall) {
        SDK::Drawing::DrawLine(wall.start, wall.end, kWallColor, 5.0f);
        SDK::Drawing::DrawCircleAlways(wall.start, 30.0f, kEndpointColor, 2.0f, 24);
        SDK::Drawing::DrawCircleAlways(wall.end, 30.0f, kEndpointColor, 2.0f, 24);
        SDK::Drawing::DrawCircleAlways(wall.center, 40.0f, kCenterColor, 2.0f, 24);
        char label[160] = {};
        std::snprintf(label, sizeof(label), "YasuoWall L%d span=%.1f main=%u A=%u B=%u",
                      wall.level, wall.Span(), wall.main.networkId,
                      wall.endpointA.networkId, wall.endpointB.networkId);
        DrawTextWorld(wall.center, label, kTextColor, -18.0f);
    }

    static constexpr std::uint32_t kWallColor = 0xFFFF5050u;
    static constexpr std::uint32_t kEndpointColor = 0xFF50FF50u;
    static constexpr std::uint32_t kCenterColor = 0xFFFFD24Au;
    static constexpr std::uint32_t kTextColor = 0xFFFFFFFFu;
};

} // namespace Plugins
```

- [ ] **Step 3: Run GREEN guard and compile the full solution**

```powershell
if (rg -n "WindwallTracker|WindwallGeo|Orientation\(|Direction\(" plugins\Utility\YasuoWallDebugPlugin.h) { throw 'legacy debug plugin still active' }
& 'E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe' '.\NightSharp.sln' /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

Expected: guard emits no match; MSBuild succeeds with `0 Error(s)`.

- [ ] **Step 4: Commit or record skip**

```powershell
if (Test-Path '.git\HEAD') { git add plugins/Utility/YasuoWallDebugPlugin.h; git commit -m "feat: draw live Yasuo wall endpoints" } else { Write-Output 'SKIP COMMIT: no valid .git/HEAD' }
```

---

### Task 6: Delete old implementation and run final verification

**Files:**
- Delete: `sdk/Math/WindwallTracker.h`
- Delete: `sdk/Math/WindwallGeometry.h`
- Delete: `tests/windwall_geometry_test.cpp`

**Interfaces:**
- No new interfaces; this task proves only the new source of truth remains.

- [ ] **Step 1: Run the failing repository cleanliness guard**

```powershell
$legacy = rg -n --glob '!docs/**' --glob '!bin/**' --glob '!obj/**' --glob '!*.obj' "WindwallTracker|WindwallGeo|EffectEmitterHandle|ProxyOrientation" .
if ($legacy) { $legacy; throw 'legacy Yasuo wall implementation remains' }
```

Expected: guard fails and lists the three old implementation/test files until they are deleted.

- [ ] **Step 2: Delete the three old files with `apply_patch`**

Use three `*** Delete File` patch entries for the exact paths above. Do not use recursive filesystem deletion.

- [ ] **Step 3: Run all standalone tests**

```powershell
cmd /d /s /c ""E:\Visual Studio\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /std:c++20 /EHsc /nologo tests\yasuo_wall_model_test.cpp /Fe:build\yasuo_wall_model_test.exe && cl /std:c++20 /EHsc /nologo /c tests\yasuo_wall_tracker_compile_test.cpp /Fo:build\yasuo_wall_tracker_compile_test.obj && cl /std:c++20 /EHsc /nologo tests\yasuo_wall_collision_api_test.cpp /Fe:build\yasuo_wall_collision_api_test.exe && build\yasuo_wall_model_test.exe && build\yasuo_wall_collision_api_test.exe"
```

Expected: model prints `ALL YASUO WALL MODEL TESTS PASSED`; collision API test exits `0`.

- [ ] **Step 4: Run cleanliness checks**

```powershell
$legacy = rg -n --glob '!docs/**' --glob '!bin/**' --glob '!obj/**' --glob '!*.obj' "WindwallTracker|WindwallGeo|EffectEmitterHandle|ProxyOrientation|ResolveProxy\(" .
if ($legacy) { $legacy; throw 'legacy implementation remains' }
rg -n "YasuoWallTracker|YasuoWallModel|HasProjectileWallCollision" sdk plugins tests
```

Expected: first command has no matches; second lists only the new tracker/model and intended consumers.

- [ ] **Step 5: Build Release x64 using required toolset**

```powershell
& 'E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe' '.\NightSharp.sln' /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /m /v:minimal
```

Expected: `Build succeeded`, `0 Error(s)`.

- [ ] **Step 6: Runtime verification**

Load the rebuilt NightSharp, enable `Yasuo Wall Debug`, then:

1. Cast W horizontally and diagonally.
2. Verify green endpoint markers sit on both visible ends and the red segment overlays the wall.
3. Verify label span follows live endpoints (CE level-1 sample: approximately `320.0`).
4. Verify active count returns to zero after W despawns.
5. Verify a projectile path crossing the segment reports collision, a path within projectile radius near an endpoint reports collision, and a farther path does not.

Expected: all five observations pass; no synthetic or orientation fallback is used.

- [ ] **Step 7: Commit or record final skip**

```powershell
if (Test-Path '.git\HEAD') { git add -A; git commit -m "refactor: replace Yasuo wall implementation" } else { Write-Output 'SKIP COMMIT: no valid .git/HEAD' }
```

Expected in this workspace: skip message.
