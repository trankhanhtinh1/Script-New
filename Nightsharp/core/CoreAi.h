#pragma once

#include "LeagueObfuscation.h"
#include "Offsets.h"
#include "Vector.h"
#include "Globals.h"

#include <cstdint>

namespace CoreAi {

    inline int CopyWaypoints(uintptr_t obj, Vec3* out, int maxOut);

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

        const auto obf = Globals::Read<LeagueObfuscation<uint64_t>>(obj + Offset::AiManager::Offset);
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

        const auto inner = Globals::Read<uintptr_t>(raw + Offset::AiManager::InnerManager);
        return Globals::IsValidPtr(inner) ? inner : 0;
    }

    inline uintptr_t ResolveNavBase(uintptr_t inner) {
        if (!Globals::IsValidPtr(inner)) {
            return 0;
        }

        const auto typePtr = Globals::Read<uintptr_t>(inner + Offset::AiManager::NavBase::InnerTypePtr);
        if (!Globals::IsValidPtr(typePtr)) {
            return 0;
        }

        const auto adjust = Globals::Read<int>(typePtr + Offset::AiManager::NavBase::InnerTypeAdjust);
        const auto navBase = inner + static_cast<uintptr_t>(adjust) + Offset::AiManager::NavBase::FinalBaseAdd;
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
        const auto inner = ResolveInnerManager(obj);
        return Globals::Read<int>(inner + Offset::AiManager::IsMoving) != 0;
    }

    inline bool HasPath(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        if (!Globals::IsValidPtr(inner)) {
            return false;
        }

        const int hasPath = Globals::Read<int>(inner + Offset::AiManager::HasPath);
        if (hasPath != 0) {
            return true;
        }

        const auto navBase = ResolveNavBase(inner);
        const int count = Globals::Read<int>(navBase + Offset::AiManager::PathStateLayout::Count);
        return count > 0;
    }

    inline bool IsDashing(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        return Globals::Read<int>(inner + Offset::AiManager::NavBase::IsDashingInner) != 0;
    }

    inline int GetCurrentSegment(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        return Globals::Read<int>(inner + Offset::AiManager::CurrentSegment);
    }

    inline float GetDashSpeed(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        return Globals::Read<float>(inner + Offset::AiManager::NavBase::DashSpeedInner);
    }

    inline Vec3 GetVelocity(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        return Globals::Read<Vec3>(inner + Offset::AiManager::Velocity);
    }

    inline Vec3 GetPathStart(uintptr_t obj) {
        const auto inner = ResolveInnerManager(obj);
        return Globals::Read<Vec3>(inner + Offset::AiManager::PathStart);
    }

    inline Vec3 GetPathEnd(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        const auto fallback = Globals::Read<Vec3>(navBase + Offset::AiManager::NavBase::PathEndFallback);
        if (!fallback.IsZero()) {
            return fallback;
        }

        Vec3 points[64] = {};
        const int count = CopyWaypoints(obj, points, static_cast<int>(sizeof(points) / sizeof(points[0])));
        return count > 0 ? points[count - 1] : Vec3{};
    }

    inline Vec3 GetServerPosition(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<Vec3>(navBase + Offset::AiManager::NavBase::ServerPosition);
    }

    inline Vec3 GetMoveVector(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<Vec3>(navBase + Offset::AiManager::NavBase::MoveVector);
    }

    inline Vec3 GetPreviousPosition(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<Vec3>(navBase + Offset::AiManager::NavBase::PreviousPosition);
    }

    inline Vec3 GetOrderPosition(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<Vec3>(navBase + Offset::AiManager::NavBase::OrderPosition);
    }

    // ── New accessors from Offset reference (AiManagerNavDataLayout) ──

    inline bool HasArrived(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<uint8_t>(navBase + Offset::AiManagerNavData::ArrivedFlag) != 0;
    }

    inline uint32_t GetDashTargetNetId(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<uint32_t>(navBase + Offset::AiManagerNavData::DashTargetNetId);
    }

    inline uint32_t GetDashSecondaryNetId(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<uint32_t>(navBase + Offset::AiManagerNavData::DashSecondaryId);
    }

    inline float GetDashDuration(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<float>(navBase + Offset::AiManagerNavData::DashDuration);
    }

    inline float GetDashDistRemaining(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<float>(navBase + Offset::AiManagerNavData::DashDistRemain);
    }

    inline int GetWaypointCount(uintptr_t obj) {
        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        return Globals::Read<int>(navBase + Offset::AiManager::PathStateLayout::Count);
    }

    inline int CopyWaypoints(uintptr_t obj, Vec3* out, int maxOut) {
        if (!out || maxOut <= 0) {
            return 0;
        }

        const auto navBase = ResolveNavBase(ResolveInnerManager(obj));
        if (!Globals::IsValidPtr(navBase)) {
            return 0;
        }

        const auto pathState = navBase + Offset::AiManager::NavBase::PathState;
        const int count = Globals::Read<int>(pathState + Offset::AiManager::PathStateLayout::Count);
        const auto points = Globals::Read<uintptr_t>(pathState + Offset::AiManager::PathStateLayout::PointsPtr);
        if (count <= 0 || count > maxOut || count > 128 || !Globals::IsValidPtr(points)) {
            const Vec3 fallback = Globals::Read<Vec3>(pathState + Offset::AiManager::PathStateLayout::FallbackEnd);
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
