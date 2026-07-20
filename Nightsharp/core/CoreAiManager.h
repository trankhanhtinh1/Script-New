#pragma once

#include "CoreBypass.h"
#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"
#include "spoof/spoofcall.h"
#include "../imgui/imgui.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace CoreAiManager {

inline constexpr int kMaxWaypoints = 128;
inline constexpr int kMaxEventWaypoints = 256;

struct ManagerRef {
    uintptr_t address = 0;

    bool IsValid() const {
        return Globals::IsValidPtr(address);
    }

    template <typename T>
    T Read(uintptr_t offset) const {
        return IsValid() ? Globals::Read<T>(address + offset) : T{};
    }
};

struct Snapshot {
    uintptr_t owner = 0;
    uintptr_t manager = 0;
    uintptr_t navArray = 0;
    int currentSegment = 0;
    int segmentsCount = 0;
    int pathState = 0;
    bool isMoving = false;
    bool isDashing = false;
    float moveSpeed = 0.0f;
    float dashSpeed = 0.0f;
    Vec3 startPath = {};
    Vec3 targetPosition = {};
    Vec3 orderPosition = {};
    Vec3 serverPosition = {};
    Vec3 moveVector = {};
    Vec3 velocity = {};
};

inline Snapshot ReadSnapshot(uintptr_t object);

inline bool IsSaneFloat(float value, float minValue, float maxValue) {
    return value == value && value >= minValue && value <= maxValue;
}

inline bool IsSaneSpeed(float value, float maxValue = 10000.0f) {
    return IsSaneFloat(value, 0.0f, maxValue);
}

inline bool IsSaneWaypointCount(int count, int maxCount = kMaxWaypoints) {
    return count > 0 && count <= maxCount;
}

inline Vec3 Flatten(Vec3 value) {
    value.y = 0.0f;
    return value;
}

inline Vec3 Normalized2D(Vec3 value) {
    value.y = 0.0f;
    const float length = value.Length2D();
    if (length < 0.0001f) {
        return {};
    }
    return { value.x / length, 0.0f, value.z / length };
}

inline bool IsUsablePosition(const Vec3& value) {
    return value.IsValid() && !value.IsZero();
}

namespace detail {
    inline constexpr int kFrameCacheBuckets = 512;
    inline constexpr int kFrameCacheProbeCount = 4;

    struct FrameCacheEntry {
        uintptr_t object = 0;
        int frame = -1;

        bool managerCached = false;
        uintptr_t manager = 0;

        bool serverPositionCached = false;
        Vec3 serverPosition = {};

        bool pathEndCached = false;
        Vec3 pathEnd = {};

        bool waypointsCached = false;
        int waypointsCount = 0;
        Vec3 waypoints[kMaxWaypoints] = {};

        bool pathCached = false;
        int pathCount = 0;
        Vec3 path[kMaxWaypoints] = {};

        bool snapshotCached = false;
        Snapshot snapshot = {};
    };

    inline int CurrentFrameKey() {
        if (ImGui::GetCurrentContext()) {
            return ImGui::GetFrameCount();
        }

        const auto generation = CoreRuntime::GetContext().refreshGeneration;
        return -static_cast<int>((generation & 0x3FFFFFFFu) + 1u);
    }

    inline int CacheBucket(uintptr_t object) {
        return static_cast<int>((object >> 4) & (kFrameCacheBuckets - 1));
    }

    inline void ResetFrameEntry(FrameCacheEntry& entry, uintptr_t object, int frame) {
        entry.object = object;
        entry.frame = frame;
        entry.managerCached = false;
        entry.manager = 0;
        entry.serverPositionCached = false;
        entry.serverPosition = {};
        entry.pathEndCached = false;
        entry.pathEnd = {};
        entry.waypointsCached = false;
        entry.waypointsCount = 0;
        entry.pathCached = false;
        entry.pathCount = 0;
        entry.snapshotCached = false;
        entry.snapshot = {};
    }

    inline FrameCacheEntry& GetFrameEntry(uintptr_t object) {
        static FrameCacheEntry entries[kFrameCacheBuckets];
        const int frame = CurrentFrameKey();
        const int base = CacheBucket(object);
        FrameCacheEntry* reusable = nullptr;

        for (int i = 0; i < kFrameCacheProbeCount; ++i) {
            FrameCacheEntry& entry = entries[(base + i) & (kFrameCacheBuckets - 1)];
            if (entry.object == object && entry.frame == frame) {
                return entry;
            }
            if (!reusable && (entry.object == 0 || entry.frame != frame)) {
                reusable = &entry;
            }
        }

        FrameCacheEntry& entry = reusable ? *reusable : entries[base];
        ResetFrameEntry(entry, object, frame);
        return entry;
    }

    inline uintptr_t ResolveManagerSeh(uintptr_t object, uintptr_t function) {
        using GetAiManagerFn = uintptr_t(__fastcall*)(uintptr_t);
        const auto trampoline = CoreBypass::ResolveSpoofTrampoline();
        __try {
            const uintptr_t manager = Globals::IsValidPtr(trampoline)
                ? spoof_call(reinterpret_cast<void*>(trampoline), reinterpret_cast<GetAiManagerFn>(function), object)
                : reinterpret_cast<GetAiManagerFn>(function)(object);
            return Globals::IsValidPtr(manager) ? manager : 0;
        }
        __except (1) {
            return 0;
        }
    }
}

inline uintptr_t ResolveManager(uintptr_t object) {
    if (!Globals::IsValidPtr(object)) {
        return 0;
    }

    auto& cache = detail::GetFrameEntry(object);
    if (cache.managerCached) {
        return cache.manager;
    }
    cache.managerCached = true;

    (void)CoreRuntime::EnsureInitialized();
    uintptr_t function = CoreRuntime::GetContext().getAiManagerFn;
    if (!function) {
        function = CoreRuntime::ResolveRva(Offset::NavGridRuntime::GetAiManager);
    }
    if (!function && Globals::base) {
        function = Globals::base + Offset::NavGridRuntime::GetAiManager;
    }
    if (!Globals::IsExecutablePtr(function)) {
        return 0;
    }

    cache.manager = detail::ResolveManagerSeh(object, function);
    return cache.manager;
}

inline ManagerRef Get(uintptr_t object) {
    return { ResolveManager(object) };
}

inline uintptr_t Address(uintptr_t object) {
    return ResolveManager(object);
}

inline int FrameCacheKey() {
    return detail::CurrentFrameKey();
}

inline Vec3 GetObjectPosition(uintptr_t object) {
    return Globals::Read<Vec3>(object + Offset::All::Position);
}

inline float GetMoveSpeed(uintptr_t object) {
    return ReadSnapshot(object).moveSpeed;
}

inline int GetCurrentSegment(uintptr_t object) {
    return ReadSnapshot(object).currentSegment;
}

inline int GetTotalSegments(uintptr_t object) {
    return ReadSnapshot(object).segmentsCount;
}

inline int GetPathState(uintptr_t object) {
    return ReadSnapshot(object).pathState;
}

inline int GetWaypointCount(uintptr_t object) {
    const int total = GetTotalSegments(object);
    if (!IsSaneWaypointCount(total)) {
        return 0;
    }

    const int current = std::clamp(GetCurrentSegment(object), 0, total);
    return std::max(0, total - current);
}

inline Vec3 GetPathStart(uintptr_t object) {
    return ReadSnapshot(object).startPath;
}

inline Vec3 GetServerPosition(uintptr_t object) {
    auto& cache = detail::GetFrameEntry(object);
    if (cache.serverPositionCached) {
        return cache.serverPosition;
    }
    cache.serverPositionCached = true;

    Vec3 position = Get(object).Read<Vec3>(Offset::AiManager::ServerPos);
    if (IsUsablePosition(position)) {
        cache.serverPosition = position;
        return cache.serverPosition;
    }
    cache.serverPosition = GetObjectPosition(object);
    return cache.serverPosition;
}

inline Vec3 GetOrderPosition(uintptr_t object) {
    return ReadSnapshot(object).orderPosition;
}

inline Vec3 GetMoveVector(uintptr_t object) {
    return ReadSnapshot(object).moveVector;
}

inline int CopyWaypoints(uintptr_t object, Vec3* out, int maxOut) {
    if (!out || maxOut <= 0) {
        return 0;
    }

    auto& cache = detail::GetFrameEntry(object);
    if (!cache.waypointsCached) {
        cache.waypointsCached = true;
        cache.waypointsCount = 0;

        const auto manager = Get(object);
        if (manager.IsValid()) {
            const uintptr_t points = manager.Read<uintptr_t>(Offset::AiManager::NavArray);
            const int total = manager.Read<int>(Offset::AiManager::SegmentsCount);
            if (Globals::IsValidPtr(points) && IsSaneWaypointCount(total)) {
                const int current = std::clamp(
                    manager.Read<int>(Offset::AiManager::CurrentSegment), 0, total);
                const int count = std::min(total - current, kMaxWaypoints);

                for (int i = 0; i < count; ++i) {
                    const Vec3 point = Globals::Read<Vec3>(
                        points + static_cast<uintptr_t>(current + i) * sizeof(Vec3));
                    if (!point.IsValid()) {
                        break;
                    }
                    cache.waypoints[cache.waypointsCount++] = point;
                }
            }
        }
    }

    const int count = std::min(cache.waypointsCount, maxOut);
    for (int i = 0; i < count; ++i) {
        out[i] = cache.waypoints[i];
    }
    return count;
}

inline Vec3 GetPathEnd(uintptr_t object) {
    auto& cache = detail::GetFrameEntry(object);
    if (cache.pathEndCached) {
        return cache.pathEnd;
    }
    cache.pathEndCached = true;

    const Vec3 target = Get(object).Read<Vec3>(Offset::AiManager::TargetPosition);
    if (IsUsablePosition(target)) {
        cache.pathEnd = target;
        return cache.pathEnd;
    }

    Vec3 points[64] = {};
    const int count = CopyWaypoints(
        object, points, static_cast<int>(sizeof(points) / sizeof(points[0])));
    cache.pathEnd = count > 0 ? points[count - 1] : Vec3{};
    return cache.pathEnd;
}

inline Vec3 GetPreviousPosition(uintptr_t object) {
    const Vec3 start = GetPathStart(object);
    return IsUsablePosition(start) ? start : GetServerPosition(object);
}

inline bool HasPath(uintptr_t object) {
    if (GetWaypointCount(object) > 0) {
        return true;
    }

    const Vec3 start = GetServerPosition(object);
    const Vec3 end = GetPathEnd(object);
    return IsUsablePosition(start) && IsUsablePosition(end) &&
           start.Distance2D(end) > 5.0f;
}

inline bool IsMoving(uintptr_t object) {
    return ReadSnapshot(object).isMoving;
}

inline bool IsDashing(uintptr_t object) {
    return ReadSnapshot(object).isDashing;
}

inline float GetDashSpeed(uintptr_t object) {
    return ReadSnapshot(object).dashSpeed;
}

inline Vec3 GetVelocity(uintptr_t object) {
    return ReadSnapshot(object).velocity;
}

inline Vec3 GetDirection(uintptr_t object) {
    Vec3 direction = Normalized2D(GetVelocity(object));
    if (!direction.IsZero()) {
        return direction;
    }

    const Vec3 start = GetServerPosition(object);
    const Vec3 end = GetPathEnd(object);
    if (IsUsablePosition(start) && IsUsablePosition(end) &&
        start.Distance2D(end) > 1.0f) {
        return Normalized2D(end - start);
    }

    return {};
}

inline bool HasArrived(uintptr_t object) {
    if (IsMoving(object) || HasPath(object)) {
        return false;
    }

    const Vec3 current = GetServerPosition(object);
    const Vec3 end = GetPathEnd(object);
    return end.IsZero() ||
           (IsUsablePosition(current) && current.Distance2D(end) <= 5.0f);
}

inline float GetDashDistRemaining(uintptr_t object) {
    if (!IsDashing(object)) {
        return 0.0f;
    }

    const Vec3 current = GetServerPosition(object);
    const Vec3 end = GetPathEnd(object);
    return IsUsablePosition(current) && IsUsablePosition(end)
        ? current.Distance2D(end)
        : 0.0f;
}

inline std::uint32_t GetDashTargetNetId(uintptr_t) {
    return 0;
}

inline std::uint32_t GetDashSecondaryNetId(uintptr_t) {
    return 0;
}

inline float GetDashDuration(uintptr_t object) {
    const float speed = GetDashSpeed(object);
    return speed > 1.0f ? GetDashDistRemaining(object) / speed : 0.0f;
}

inline int CopyPath(uintptr_t object, Vec3* out, int maxOut) {
    if (!out || maxOut <= 0 || !Globals::IsValidPtr(object)) {
        return 0;
    }

    auto& cache = detail::GetFrameEntry(object);
    if (!cache.pathCached) {
        cache.pathCached = true;
        cache.pathCount = 0;

        const auto pushPoint = [&](const Vec3& point) {
            if (cache.pathCount >= kMaxWaypoints || !IsUsablePosition(point)) {
                return;
            }
            if (cache.pathCount > 0 &&
                point.Distance2D(cache.path[cache.pathCount - 1]) <= 1.0f) {
                return;
            }
            cache.path[cache.pathCount++] = point;
        };

        pushPoint(GetServerPosition(object));

        Vec3 waypoints[kMaxWaypoints] = {};
        const int waypointCount = CopyWaypoints(object, waypoints, kMaxWaypoints);
        for (int i = 0; i < waypointCount; ++i) {
            pushPoint(waypoints[i]);
        }

        if (cache.pathCount <= 1) {
            pushPoint(GetPathEnd(object));
        }
    }

    const int count = std::min(cache.pathCount, maxOut);
    for (int i = 0; i < count; ++i) {
        out[i] = cache.path[i];
    }
    return count;
}

inline const Vec3* CachedPathData(uintptr_t object, int& count) {
    count = 0;
    if (!Globals::IsValidPtr(object)) {
        return nullptr;
    }

    auto& cache = detail::GetFrameEntry(object);
    if (!cache.pathCached) {
        Vec3 discard[kMaxWaypoints] = {};
        (void)CopyPath(object, discard, kMaxWaypoints);
    }

    count = cache.pathCount;
    return count > 0 ? cache.path : nullptr;
}

inline const Vec3* CachedWaypointsData(uintptr_t object, int& count) {
    count = 0;
    if (!Globals::IsValidPtr(object)) {
        return nullptr;
    }

    auto& cache = detail::GetFrameEntry(object);
    if (!cache.waypointsCached) {
        Vec3 discard[kMaxWaypoints] = {};
        (void)CopyWaypoints(object, discard, kMaxWaypoints);
    }

    count = cache.waypointsCount;
    return count > 0 ? cache.waypoints : nullptr;
}

inline std::vector<Vec3> GetPath(uintptr_t object, int maxPoints = 32) {
    std::vector<Vec3> result;
    if (maxPoints <= 0) {
        return result;
    }

    maxPoints = std::clamp(maxPoints, 1, kMaxWaypoints);
    Vec3 points[kMaxWaypoints] = {};
    const int count = CopyPath(object, points, maxPoints);
    result.reserve(static_cast<std::size_t>(std::max(0, count)));
    for (int i = 0; i < count; ++i) {
        result.push_back(points[i]);
    }
    return result;
}

inline Snapshot ReadSnapshot(uintptr_t object) {
    auto& cache = detail::GetFrameEntry(object);
    if (cache.snapshotCached) {
        return cache.snapshot;
    }
    cache.snapshotCached = true;

    Snapshot snapshot{};
    snapshot.owner = object;
    const auto manager = Get(object);
    snapshot.manager = manager.address;
    if (!manager.IsValid()) {
        cache.snapshot = snapshot;
        return snapshot;
    }

    snapshot.navArray = manager.Read<uintptr_t>(Offset::AiManager::NavArray);
    snapshot.currentSegment = manager.Read<int>(Offset::AiManager::CurrentSegment);
    snapshot.segmentsCount = manager.Read<int>(Offset::AiManager::SegmentsCount);
    snapshot.pathState = manager.Read<int>(Offset::AiManager::PathState);
    snapshot.isDashing = manager.Read<std::uint8_t>(Offset::AiManager::IsDashing) != 0;
    const float moveSpeed = manager.Read<float>(Offset::AiManager::Velocity);
    snapshot.moveSpeed = IsSaneSpeed(moveSpeed, 5000.0f) ? moveSpeed : 0.0f;
    snapshot.startPath = manager.Read<Vec3>(Offset::AiManager::StartPath);
    snapshot.targetPosition = manager.Read<Vec3>(Offset::AiManager::TargetPosition);
    snapshot.orderPosition = manager.Read<Vec3>(Offset::AiManager::TargetPos);
    snapshot.serverPosition = GetServerPosition(object);
    snapshot.moveVector = Flatten(manager.Read<Vec3>(Offset::AiManager::MoveVec3));
    if (!snapshot.moveVector.IsValid()) {
        snapshot.moveVector = {};
    }

    const float rawDashSpeed = manager.Read<float>(Offset::AiManager::DashSpeed);
    if (IsSaneSpeed(rawDashSpeed)) {
        snapshot.dashSpeed = rawDashSpeed;
    } else {
        const float vectorSpeed = snapshot.moveVector.Length2D();
        snapshot.dashSpeed =
            IsSaneSpeed(vectorSpeed) ? vectorSpeed : snapshot.moveSpeed;
    }

    int remainingSegments = 0;
    if (IsSaneWaypointCount(snapshot.segmentsCount)) {
        const int current = std::clamp(
            snapshot.currentSegment,
            0,
            snapshot.segmentsCount);
        remainingSegments = std::max(0, snapshot.segmentsCount - current);
    }

    const Vec3 pathEnd = IsUsablePosition(snapshot.targetPosition)
        ? snapshot.targetPosition
        : GetPathEnd(object);
    const bool hasTargetPath =
        IsUsablePosition(snapshot.serverPosition) &&
        IsUsablePosition(pathEnd) &&
        snapshot.serverPosition.Distance2D(pathEnd) > 5.0f;
    snapshot.isMoving =
        manager.Read<std::uint8_t>(Offset::AiManager::IsMoving) != 0 ||
        remainingSegments > 0 ||
        hasTargetPath;

    const float vectorLength = snapshot.moveVector.Length2D();
    const float speed = snapshot.isDashing ? snapshot.dashSpeed : snapshot.moveSpeed;
    if (vectorLength > 0.01f && speed > 0.0f) {
        snapshot.velocity = Normalized2D(snapshot.moveVector) * speed;
    } else if (vectorLength > 0.01f && vectorLength < 10000.0f) {
        snapshot.velocity = snapshot.moveVector;
    } else if (speed > 0.0f &&
               IsUsablePosition(snapshot.serverPosition) &&
               IsUsablePosition(pathEnd) &&
               snapshot.serverPosition.Distance2D(pathEnd) > 1.0f) {
        snapshot.velocity = Normalized2D(pathEnd - snapshot.serverPosition) * speed;
    }

    cache.snapshot = snapshot;
    return cache.snapshot;
}

inline int CopyNativePathArray(
    uintptr_t pathArray,
    int explicitCount,
    Vec3* out,
    int maxOut) {
    if (!Globals::IsValidPtr(pathArray) || !out || maxOut <= 0) {
        return 0;
    }

    const uintptr_t data = Globals::Read<uintptr_t>(pathArray);
    int count = Globals::Read<int>(pathArray + 0x8);
    if (!IsSaneWaypointCount(count, kMaxEventWaypoints)) {
        count = explicitCount;
    }
    if (!IsSaneWaypointCount(count, kMaxEventWaypoints)) {
        count = Globals::Read<int>(pathArray + 0x4);
    }
    if (!Globals::IsValidPtr(data) ||
        !IsSaneWaypointCount(count, kMaxEventWaypoints)) {
        return 0;
    }

    const int clipped = std::min(count, maxOut);
    int copied = 0;
    for (int i = 0; i < clipped; ++i) {
        const Vec3 point = Globals::Read<Vec3>(
            data + static_cast<uintptr_t>(i) * sizeof(Vec3));
        if (!point.IsValid()) {
            break;
        }
        out[copied++] = point;
    }
    return copied;
}

inline float DecodePathPayloadDashSpeed(uintptr_t payload) {
    if (!Globals::IsValidPtr(payload)) {
        return 0.0f;
    }

    // IDA 13337: sub_562E60 passes R9 as the path-state payload to
    // sub_300890. sub_300890 writes payload+0x2C to AiManager+0x360.
    const float speed = Globals::Read<float>(payload + 0x2C);
    return IsSaneSpeed(speed) ? speed : 0.0f;
}

inline float DecodeNewPathSpeed(uintptr_t object, uintptr_t payload, bool isDash) {
    if (isDash) {
        const float payloadSpeed = DecodePathPayloadDashSpeed(payload);
        if (payloadSpeed > 0.0f) {
            return payloadSpeed;
        }
        return GetDashSpeed(object);
    }
    return GetMoveSpeed(object);
}

} // namespace CoreAiManager
