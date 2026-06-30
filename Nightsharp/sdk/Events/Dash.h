#pragma once

#include "Events.h"

namespace SDK::Events::Dash {

struct DashArgs {
    uintptr_t Unit = 0;
    uint32_t NetworkId = 0;
    Vec3 Path[32] = {};
    int PathCount = 0;
    float Speed = 0.0f;
    Vec3 StartPos = {};
    Vec3 EndPos = {};
    float StartTime = 0.0f;
    float EndTime = 0.0f;
    int StartTick = 0;
    int EndTick = 0;
    int Duration = 0;
    bool IsDash = false;
    bool IsBlink = false;
    ::Core::Events::NewPathEventArgs Raw = {};
};

using DashHandler = void(*)(const DashArgs&);

namespace detail {
    inline SDK::Events::detail::EventList<DashArgs> DashHandlers;
    inline DashArgs DetectedDashes[64] = {};
    inline int DetectedDashCount = 0;

    inline int TickCount() {
        return static_cast<int>(GetTickCount64() & 0x7FFFFFFF);
    }

    inline uint32_t KeyFor(const ::Core::Events::ObjectInfo& object) {
        return object.NetworkId ? object.NetworkId : static_cast<uint32_t>(object.Ptr & 0xFFFFFFFFu);
    }

    inline float Distance2D(const Vec3& a, const Vec3& b) {
        return a.Distance2D(b);
    }

    inline DashArgs* Find(uint32_t networkId, bool create) {
        if (!networkId) {
            return nullptr;
        }

        for (int i = 0; i < DetectedDashCount; ++i) {
            if (DetectedDashes[i].NetworkId == networkId) {
                return &DetectedDashes[i];
            }
        }

        if (!create || DetectedDashCount >= 64) {
            return nullptr;
        }

        DashArgs& entry = DetectedDashes[DetectedDashCount++];
        entry = {};
        entry.NetworkId = networkId;
        return &entry;
    }
} // namespace detail

inline bool AddOnDash(DashHandler handler) {
    if (!handler) {
        return false;
    }

    SDK::Events::Initialize();
    if (!SDK::Events::detail::EnsureNewPathRawSubscribed()) {
        return false;
    }

    const bool hadHandlers = detail::DashHandlers.HasHandlers();
    const bool added = detail::DashHandlers.Add(handler);
    if (added && !hadHandlers) {
        ++SDK::Events::detail::DashConsumerRefs;
    }
    return added;
}

inline bool RemoveOnDash(DashHandler handler) {
    const bool removed = detail::DashHandlers.Remove(handler);
    if (removed && !detail::DashHandlers.HasHandlers()) {
        if (SDK::Events::detail::DashConsumerRefs > 0) {
            --SDK::Events::detail::DashConsumerRefs;
        }
        SDK::Events::detail::ReleaseNewPathRawIfUnused();
    }
    return removed;
}

inline bool OnDash(DashHandler handler) {
    return AddOnDash(handler);
}

inline DashArgs GetDashInfo(uint32_t networkId) {
    if (auto* dash = detail::Find(networkId, false)) {
        return *dash;
    }
    return {};
}

inline DashArgs GetDashInfo(uintptr_t unitPtr) {
    return GetDashInfo(static_cast<uint32_t>(unitPtr & 0xFFFFFFFFu));
}

template <typename T>
inline DashArgs GetDashInfo(const T& unit) {
    return GetDashInfo(static_cast<uint32_t>(unit.NetworkId()));
}

inline bool IsDashing(uint32_t networkId) {
    const auto dash = GetDashInfo(networkId);
    return dash.IsDash && dash.EndTick != 0 && dash.EndTick > detail::TickCount();
}

inline bool IsDashing(uintptr_t unitPtr) {
    return IsDashing(static_cast<uint32_t>(unitPtr & 0xFFFFFFFFu));
}

template <typename T>
inline bool IsDashing(const T& unit) {
    return IsDashing(static_cast<uint32_t>(unit.NetworkId()));
}

} // namespace SDK::Events::Dash

namespace SDK::Events {
    inline bool AddOnDash(Dash::DashHandler handler) { return Dash::AddOnDash(handler); }
    inline bool RemoveOnDash(Dash::DashHandler handler) { return Dash::RemoveOnDash(handler); }
    inline bool OnDash(Dash::DashHandler handler) { return Dash::OnDash(handler); }

namespace detail {
    inline void EventDash(const NewPathEventArgs& args) {
        const uint32_t key = Dash::detail::KeyFor(args.Sender);
        auto* dash = Dash::detail::Find(key, true);
        if (!dash) {
            return;
        }

        if (!args.IsDash) {
            dash->IsDash = false;
            dash->IsBlink = false;
            dash->Duration = 0;
            dash->EndTick = 0;
            dash->EndTime = 0.0f;
            return;
        }

        dash->Unit = args.Sender.Ptr;
        dash->NetworkId = key;
        dash->PathCount = 0;
        dash->Speed = args.Speed;
        dash->StartPos = args.Sender.Position;
        dash->StartTime = SDK::Events::GameTime();
        dash->StartTick = Dash::detail::TickCount();
        dash->IsDash = true;
        dash->Raw = args;

        if (dash->StartPos.IsValid()) {
            dash->Path[dash->PathCount++] = dash->StartPos;
        }

        for (int i = 0; i < args.PathCount && dash->PathCount < 32; ++i) {
            dash->Path[dash->PathCount++] = args.Path[i];
        }

        dash->EndPos = dash->PathCount > 0 ? dash->Path[dash->PathCount - 1] : args.Sender.Position;
        if (dash->Speed > 1.0f) {
            dash->Duration = static_cast<int>((Dash::detail::Distance2D(dash->StartPos, dash->EndPos) / dash->Speed) * 1000.0f);
            dash->EndTick = dash->StartTick + dash->Duration;
            dash->EndTime = dash->StartTime + (static_cast<float>(dash->Duration) / 1000.0f);
        } else {
            dash->Duration = 0;
            dash->EndTick = 0;
            dash->EndTime = 0.0f;
        }
        dash->IsBlink = dash->Speed > 5000.0f || dash->Duration <= 10;

        Dash::detail::DashHandlers.Fire(*dash);
    }
} // namespace detail
} // namespace SDK::Events
