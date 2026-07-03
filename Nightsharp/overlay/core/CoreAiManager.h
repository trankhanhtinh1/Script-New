#pragma once

#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"

#include <algorithm>
#include <cstdint>
#include <vector>
#include "../SectionProfiler.h"

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
    inline uintptr_t ResolveManagerSeh(uintptr_t object, uintptr_t function) {
        using GetAiManagerFn = uintptr_t(__fastcall*)(uintptr_t);
        __try {
            const uintptr_t manager =
                reinterpret_cast<GetAiManagerFn>(function)(object);
            return Globals::IsValidPtr(manager) ? manager : 0;
        }
        __except (1) {
            return 0;
        }
    }
}

inline uintptr_t ResolveManager(uintptr_t object) {
    NS_PROFILE("ai.ResolveManager");
    if (!Globals::IsValidPtr(object)) {
        return 0;
    }

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

    return detail::ResolveManagerSeh(object, function);
}

inline ManagerRef Get(uintptr_t object) {
    return { ResolveManager(object) };
}

inline uintptr_t Address(uintptr_t object) {
    return ResolveManager(object);
}

inline Vec3 GetObjectPosition(uintptr_t object) {
    return Globals::IsValidPtr(object)
        ? Globals::Read<Vec3>(object + Offset::All::Position)
        : Vec3{};
}

inline float GetMoveSpeed(uintptr_t object) {
    const float speed = Get(object).Read<float>(Offset::AiManager::Velocity);
    return IsSaneSpeed(speed, 5000.0f) ? speed : 0.0f;
}

inline int GetCurrentSegment(uintptr_t object) {
    return Get(object).Read<int>(Offset::AiManager::CurrentSegment);
}

inline int GetTotalSegments(uintptr_t object) {
    return Get(object).Read<int>(Offset::AiManager::SegmentsCount);
}

inline int GetPathState(uintptr_t object) {
    return Get(object).Read<int>(Offset::AiManager::PathState);
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
    return Get(object).Read<Vec3>(Offset::AiManager::StartPath);
}

inline Vec3 GetServerPosition(uintptr_t object) {
    Vec3 position = Get(object).Read<Vec3>(Offset::AiManager::ServerPos);
    if (IsUsablePosition(position)) {
        return position;
    }
    return GetObjectPosition(object);
}

inline Vec3 GetOrderPosition(uintptr_t object) {
    return Get(object).Read<Vec3>(Offset::AiManager::TargetPos);
}

inline Vec3 GetMoveVector(uintptr_t object) {
    const Vec3 vector = Flatten(Get(object).Read<Vec3>(Offset::AiManager::MoveVec3));
    return vector.IsValid() ? vector : Vec3{};
}

inline int CopyWaypoints(uintptr_t object, Vec3* out, int maxOut) {
    NS_PROFILE("ai.CopyWaypoints");
    if (!out || maxOut <= 0) {
        return 0;
    }

    const auto manager = Get(object);
    if (!manager.IsValid()) {
        return 0;
    }

    const uintptr_t points = manager.Read<uintptr_t>(Offset::AiManager::NavArray);
    const int total = manager.Read<int>(Offset::AiManager::SegmentsCount);
    if (!Globals::IsValidPtr(points) || !IsSaneWaypointCount(total)) {
        return 0;
    }

    const int current = std::clamp(
        manager.Read<int>(Offset::AiManager::CurrentSegment), 0, total);
    const int count = std::min(total - current, maxOut);
    if (count <= 0) {
        return 0;
    }

    int copied = 0;
    for (int i = 0; i < count; ++i) {
        const Vec3 point = Globals::Read<Vec3>(
            points + static_cast<uintptr_t>(current + i) * sizeof(Vec3));
        if (!point.IsValid()) {
            break;
        }
        out[copied++] = point;
    }
    return copied;
}

// CopyWaypoints overload using a pre-resolved ManagerRef — avoids a second Get() call
// when the caller already holds the manager reference (e.g. from ReadSnapshot cache).
inline int CopyWaypointsFromRef(const ManagerRef& manager, Vec3* out, int maxOut) {
    NS_PROFILE("ai.CopyWaypoints");
    if (!out || maxOut <= 0 || !manager.IsValid()) {
        return 0;
    }

    const uintptr_t points = manager.Read<uintptr_t>(Offset::AiManager::NavArray);
    const int total = manager.Read<int>(Offset::AiManager::SegmentsCount);
    if (!Globals::IsValidPtr(points) || !IsSaneWaypointCount(total)) {
        return 0;
    }

    const int current = std::clamp(
        manager.Read<int>(Offset::AiManager::CurrentSegment), 0, total);
    const int count = std::min(total - current, maxOut);
    if (count <= 0) {
        return 0;
    }

    int copied = 0;
    for (int i = 0; i < count; ++i) {
        const Vec3 point = Globals::Read<Vec3>(
            points + static_cast<uintptr_t>(current + i) * sizeof(Vec3));
        if (!point.IsValid()) {
            break;
        }
        out[copied++] = point;
    }
    return copied;
}

inline Vec3 GetPathEnd(uintptr_t object) {
    const Vec3 target = Get(object).Read<Vec3>(Offset::AiManager::TargetPosition);
    if (IsUsablePosition(target)) {
        return target;
    }

    Vec3 points[64] = {};
    const int count = CopyWaypoints(
        object, points, static_cast<int>(sizeof(points) / sizeof(points[0])));
    return count > 0 ? points[count - 1] : Vec3{};
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
    const auto manager = Get(object);
    if (!manager.IsValid()) {
        return false;
    }

    if (manager.Read<std::uint8_t>(Offset::AiManager::IsMoving) != 0) {
        return true;
    }
    return HasPath(object);
}

inline bool IsDashing(uintptr_t object) {
    return Get(object).Read<std::uint8_t>(Offset::AiManager::IsDashing) != 0;
}

inline float GetDashSpeed(uintptr_t object) {
    const float speed = Get(object).Read<float>(Offset::AiManager::DashSpeed);
    if (IsSaneSpeed(speed)) {
        return speed;
    }

    const float velocitySpeed = GetMoveVector(object).Length2D();
    return IsSaneSpeed(velocitySpeed) ? velocitySpeed : GetMoveSpeed(object);
}

inline Vec3 GetVelocity(uintptr_t object) {
    const Vec3 vector = GetMoveVector(object);
    const float length = vector.Length2D();
    const float speed = IsDashing(object) ? GetDashSpeed(object) : GetMoveSpeed(object);

    if (length > 0.01f && speed > 0.0f) {
        return Normalized2D(vector) * speed;
    }
    if (length > 0.01f && length < 10000.0f) {
        return vector;
    }

    const Vec3 start = GetServerPosition(object);
    const Vec3 end = GetPathEnd(object);
    if (speed > 0.0f && IsUsablePosition(start) && IsUsablePosition(end) &&
        start.Distance2D(end) > 1.0f) {
        return Normalized2D(end - start) * speed;
    }

    return {};
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
    NS_PROFILE("ai.CopyPath");
    if (!out || maxOut <= 0 || !Globals::IsValidPtr(object)) {
        return 0;
    }

    int written = 0;
    const auto pushPoint = [&](const Vec3& point) {
        if (written >= maxOut || !IsUsablePosition(point)) {
            return;
        }
        if (written > 0 && point.Distance2D(out[written - 1]) <= 1.0f) {
            return;
        }
        out[written++] = point;
    };

    pushPoint(GetServerPosition(object));

    Vec3 waypoints[64] = {};
    const int waypointCount = CopyWaypoints(
        object,
        waypoints,
        std::min<int>(static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])), maxOut));
    for (int i = 0; i < waypointCount; ++i) {
        pushPoint(waypoints[i]);
    }

    if (written <= 1) {
        pushPoint(GetPathEnd(object));
    }

    return written;
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
    Snapshot snapshot{};
    snapshot.owner = object;
    // ONE manager resolve for the entire snapshot — no sub-function calls that re-call Get().
    const ManagerRef mgr = Get(object);
    snapshot.manager = mgr.address;
    if (!mgr.IsValid()) {
        return snapshot;
    }

    // Read all fields from the single resolved manager reference.
    snapshot.navArray       = mgr.Read<uintptr_t>(Offset::AiManager::NavArray);
    snapshot.currentSegment = mgr.Read<int>(Offset::AiManager::CurrentSegment);
    snapshot.segmentsCount  = mgr.Read<int>(Offset::AiManager::SegmentsCount);
    snapshot.pathState      = mgr.Read<int>(Offset::AiManager::PathState);

    // isMoving: flag OR has remaining waypoints (mirrors IsMoving logic, no second Get)
    const bool movingFlag = mgr.Read<std::uint8_t>(Offset::AiManager::IsMoving) != 0;
    const bool hasWaypoints = IsSaneWaypointCount(snapshot.segmentsCount) &&
                              snapshot.currentSegment < snapshot.segmentsCount;
    snapshot.isMoving  = movingFlag || hasWaypoints;
    snapshot.isDashing = mgr.Read<std::uint8_t>(Offset::AiManager::IsDashing) != 0;

    // moveSpeed / dashSpeed
    const float rawSpeed = mgr.Read<float>(Offset::AiManager::Velocity);
    snapshot.moveSpeed = IsSaneSpeed(rawSpeed, 5000.0f) ? rawSpeed : 0.0f;
    const float rawDash = mgr.Read<float>(Offset::AiManager::DashSpeed);
    snapshot.dashSpeed = IsSaneSpeed(rawDash) ? rawDash : snapshot.moveSpeed;

    snapshot.startPath      = mgr.Read<Vec3>(Offset::AiManager::StartPath);
    snapshot.targetPosition = mgr.Read<Vec3>(Offset::AiManager::TargetPosition);
    snapshot.orderPosition  = mgr.Read<Vec3>(Offset::AiManager::TargetPos);

    // serverPosition: prefer the manager's server pos; fall back to object position.
    Vec3 sv = mgr.Read<Vec3>(Offset::AiManager::ServerPos);
    snapshot.serverPosition = IsUsablePosition(sv) ? sv : GetObjectPosition(object);

    // moveVector and velocity: inline from already-read data, no second Get.
    const Vec3 rawMove = Flatten(mgr.Read<Vec3>(Offset::AiManager::MoveVec3));
    snapshot.moveVector = rawMove.IsValid() ? rawMove : Vec3{};
    const float moveLen = snapshot.moveVector.Length2D();
    const float effSpeed = snapshot.isDashing ? snapshot.dashSpeed : snapshot.moveSpeed;
    if (moveLen > 0.01f && effSpeed > 0.0f) {
        snapshot.velocity = Normalized2D(snapshot.moveVector) * effSpeed;
    } else if (moveLen > 0.01f && moveLen < 10000.0f) {
        snapshot.velocity = snapshot.moveVector;
    } else {
        // Fall back to direction from serverPosition → targetPosition
        const Vec3& start = snapshot.serverPosition;
        const Vec3& end   = snapshot.targetPosition;
        if (effSpeed > 0.0f && IsUsablePosition(start) && IsUsablePosition(end) &&
            start.Distance2D(end) > 1.0f) {
            snapshot.velocity = Normalized2D(end - start) * effSpeed;
        }
    }

    return snapshot;
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
