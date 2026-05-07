#pragma once

#include "LeagueObfuscation.h"
#include "offset.h"
#include "Vector.h"
#include "Globals.h"

#include <cstdint>

namespace CoreAi {

    inline int CopyWaypoints(uintptr_t obj, Vec3* out, int maxOut);
    inline Vec3 GetServerPosition(uintptr_t obj);
    inline Vec3 GetPathEnd(uintptr_t obj);
    inline Vec3 GetVelocity(uintptr_t obj);

    struct ManagerRef {
        uintptr_t raw = 0;
        uintptr_t inner = 0;
        uintptr_t navBase = 0;

        bool IsValid() const {
            return Globals::IsValidPtr(inner);
        }
    };

    inline uintptr_t ResolveEncryptedBlock(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) {
            return 0;
        }

        const auto obf = Globals::Read<LeagueObfuscation<uint64_t>>(obj + Offset::AiManagerInnerCompatLayout::Offset);
        if (!obf.isInit) {
            return 0;
        }

        const auto raw = static_cast<uintptr_t>(Decrypt(obf));
        return Globals::IsValidPtr(raw) ? raw : 0;
    }

    inline uintptr_t ResolveInnerManager(uintptr_t obj) {
        const auto raw = ResolveEncryptedBlock(obj);
        if (!Globals::IsValidPtr(raw)) {
            return 0;
        }

        const auto inner = Globals::Read<uintptr_t>(raw + Offset::AiManagerInnerCompatLayout::InnerManager);
        return Globals::IsValidPtr(inner) ? inner : 0;
    }

    inline uintptr_t ResolveNavBase(uintptr_t inner) {
        if (!Globals::IsValidPtr(inner)) {
            return 0;
        }

        const auto typePtr = Globals::Read<uintptr_t>(inner + Offset::AiManagerNavBaseLayout::InnerTypePtr);
        if (!Globals::IsValidPtr(typePtr)) {
            return 0;
        }

        const auto adjust = Globals::Read<int>(typePtr + Offset::AiManagerNavBaseLayout::InnerTypeAdjust);
        const auto navBase = inner + static_cast<uintptr_t>(adjust) + Offset::AiManagerNavBaseLayout::FinalBaseAdd;
        return Globals::IsValidPtr(navBase) ? navBase : 0;
    }

    inline ManagerRef Get(uintptr_t obj) {
        ManagerRef ref = {};
        ref.raw = ResolveEncryptedBlock(obj);
        ref.inner = ResolveInnerManager(obj);
        ref.navBase = ResolveNavBase(ref.inner);
        return ref;
    }

    inline bool IsMoving(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        if (!Globals::IsValidPtr(navBase)) {
            return false;
        }

        const int count = Globals::Read<int>(
            navBase + Offset::AiManagerNavBaseLayout::PathState + Offset::AiManagerPathStateLayout::Count);
        if (count > 0) {
            const Vec3 start = GetServerPosition(obj);
            const Vec3 end = GetPathEnd(obj);
            return !end.IsZero() && (start.IsZero() || start.Distance2D(end) > 5.0f);
        }

        return Globals::Read<uint8_t>(navBase + Offset::AiManagerNavDataLayout::NavFlag) != 0;
    }

    inline bool HasPath(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        if (!Globals::IsValidPtr(inner)) {
            return false;
        }

        const auto navBase = ResolveNavBase(inner);
        if (!Globals::IsValidPtr(navBase)) {
            return false;
        }

        const auto pathState = navBase + Offset::AiManagerNavBaseLayout::PathState;
        const int count = Globals::Read<int>(pathState + Offset::AiManagerPathStateLayout::Count);
        if (count > 0) {
            return true;
        }

        const Vec3 fallback = Globals::Read<Vec3>(pathState + Offset::AiManagerPathStateLayout::FallbackEnd);
        return !fallback.IsZero();
    }

    inline bool IsDashing(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        if (!Globals::IsValidPtr(inner)) {
            return false;
        }

        return Globals::Read<uint8_t>(inner + Offset::AiManagerInnerCompatLayout::IsDashing) != 0;
    }

    inline int GetCurrentSegment(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        if (!Globals::IsValidPtr(inner)) {
            return 0;
        }

        return Globals::Read<int>(inner + Offset::AiManagerInnerCompatLayout::CurrentSegment);
    }

    inline float GetDashSpeed(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        const float speed = Globals::Read<float>(inner + Offset::AiManagerInnerCompatLayout::DashSpeed);
        if (speed > 0.0f && speed < 5000.0f) {
            return speed;
        }

        return GetVelocity(obj).Length2D();
    }

    inline Vec3 GetVelocity(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        const auto navBase = ResolveNavBase(inner);
        if (!Globals::IsValidPtr(navBase)) {
            return {};
        }

        const float objMoveSpeed = Globals::Read<float>(obj + Offset::AIHeroClient::MoveSpeed);
        float moveSpeed = Globals::Read<float>(navBase + Offset::AiManagerNavDataLayout::MoveSpeed);
        if (moveSpeed <= 0.0f || moveSpeed > 5000.0f) {
            moveSpeed = (objMoveSpeed > 0.0f && objMoveSpeed < 5000.0f) ? objMoveSpeed : 0.0f;
        }

        Vec3 moveVector = Globals::Read<Vec3>(navBase + Offset::AiManagerNavBaseLayout::MoveVector);
        moveVector.y = 0.0f;
        if (moveVector.IsValid() && !moveVector.IsZero()) {
            const float len = moveVector.Length2D();
            if (len > 0.01f && len <= 2.0f && moveSpeed > 0.0f) {
                return moveVector.Normalized2D() * moveSpeed;
            }
            if (len > 2.0f && len < 5000.0f) {
                return moveVector;
            }
        }

        const Vec3 start = GetServerPosition(obj);
        const Vec3 end = GetPathEnd(obj);
        if (!start.IsZero() && !end.IsZero() && start.Distance2D(end) > 1.0f) {
            Vec3 dir = end - start;
            dir.y = 0.0f;
            return dir.Normalized2D() * moveSpeed;
        }

        return {};
    }

    inline Vec3 GetPathStart(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<Vec3>(navBase + Offset::AiManagerNavDataLayout::PathStart);
    }

    inline Vec3 GetPathEnd(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        Vec3 points[64] = {};
        const int count = CopyWaypoints(obj, points, static_cast<int>(sizeof(points) / sizeof(points[0])));
        if (count > 0) {
            return points[count - 1];
        }

        const auto pathState = navBase + Offset::AiManagerNavBaseLayout::PathState;
        const Vec3 pathFallback = Globals::Read<Vec3>(pathState + Offset::AiManagerPathStateLayout::FallbackEnd);
        if (!pathFallback.IsZero()) {
            return pathFallback;
        }

        return Globals::Read<Vec3>(navBase + Offset::AiManagerNavDataLayout::PathEndFallback);
    }

    inline Vec3 GetServerPosition(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<Vec3>(navBase + Offset::AiManagerNavBaseLayout::ServerPosition);
    }

    inline Vec3 GetMoveVector(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<Vec3>(navBase + Offset::AiManagerNavBaseLayout::MoveVector);
    }

    inline Vec3 GetPreviousPosition(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<Vec3>(navBase + Offset::AiManagerNavBaseLayout::PreviousPosition);
    }

    inline Vec3 GetOrderPosition(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<Vec3>(navBase + Offset::AiManagerNavBaseLayout::OrderPosition);
    }

    // ── New accessors from Offset reference (AiManagerNavDataLayout) ──

    inline bool HasArrived(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<uint8_t>(navBase + Offset::AiManagerNavDataLayout::ArrivedFlag) != 0;
    }

    inline uint32_t GetDashTargetNetId(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<uint32_t>(navBase + Offset::AiManagerNavDataLayout::DashTargetNetId);
    }

    inline uint32_t GetDashSecondaryNetId(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<uint32_t>(navBase + Offset::AiManagerNavDataLayout::DashSecondaryId);
    }

    inline float GetDashDuration(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<float>(navBase + Offset::AiManagerNavDataLayout::DashDuration);
    }

    inline float GetDashDistRemaining(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<float>(navBase + Offset::AiManagerNavDataLayout::DashDistRemain);
    }

    inline int GetWaypointCount(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<int>(navBase + Offset::AiManagerPathStateLayout::Count);
    }

    inline int CopyWaypoints(uintptr_t obj, Vec3* out, int maxOut) {
        if (!out || maxOut <= 0) {
            return 0;
        }

        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        if (!Globals::IsValidPtr(navBase)) {
            return 0;
        }

        const auto pathState = navBase + Offset::AiManagerNavBaseLayout::PathState;
        const int count = Globals::Read<int>(pathState + Offset::AiManagerPathStateLayout::Count);
        const auto points = Globals::Read<uintptr_t>(pathState + Offset::AiManagerPathStateLayout::PointsPtr);
        if (count <= 0 || count > maxOut || count > 128 || !Globals::IsValidPtr(points)) {
            const Vec3 fallback = Globals::Read<Vec3>(pathState + Offset::AiManagerPathStateLayout::FallbackEnd);
            if (fallback.IsZero()) {
                return 0;
            }
            out[0] = fallback;
            return 1;
        }

        for (int i = 0; i < count; ++i) {
            out[i] = Globals::Read<Vec3>(points + static_cast<uintptr_t>(i * sizeof(Vec3)));
        }
        return count;
    }

} // namespace CoreAi
