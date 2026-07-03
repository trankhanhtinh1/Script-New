#pragma once

#include "Events.h"

#include <cstring>

namespace SDK::Events::Stealth {

struct OnStealthEventArgs {
    uintptr_t Sender = 0;
    uint32_t NetworkId = 0;
    bool IsStealthed = false;
    float Time = 0.0f;
    int OldValue = 0;
    int NewValue = 0;
    ::Core::Events::IntegerPropertyChangeEventArgs Raw = {};
};

using StealthHandler = void(*)(const OnStealthEventArgs&);

namespace detail {
    inline SDK::Events::detail::EventList<OnStealthEventArgs> StealthHandlers;
    inline constexpr int IsStealthedMask = 0x20;

    inline bool EqualsIgnoreCase(const char* a, const char* b) {
        if (!a || !b) {
            return false;
        }
        return _stricmp(a, b) == 0;
    }
} // namespace detail

inline bool AddOnStealth(StealthHandler handler) {
    if (!handler) {
        return false;
    }

    SDK::Events::Initialize();
    if (!SDK::Events::detail::EnsureIntegerPropertyChangeRawSubscribed()) {
        return false;
    }

    const bool hadHandlers = detail::StealthHandlers.HasHandlers();
    const bool added = detail::StealthHandlers.Add(handler);
    if (added && !hadHandlers) {
        ++SDK::Events::detail::StealthConsumerRefs;
    }
    return added;
}

inline bool RemoveOnStealth(StealthHandler handler) {
    const bool removed = detail::StealthHandlers.Remove(handler);
    if (removed && !detail::StealthHandlers.HasHandlers()) {
        if (SDK::Events::detail::StealthConsumerRefs > 0) {
            --SDK::Events::detail::StealthConsumerRefs;
        }
        SDK::Events::detail::ReleaseIntegerPropertyChangeRawIfUnused();
    }
    return removed;
}

inline bool OnStealth(StealthHandler handler) {
    return AddOnStealth(handler);
}

} // namespace SDK::Events::Stealth

namespace SDK::Events {
    inline bool AddOnStealth(Stealth::StealthHandler handler) { return Stealth::AddOnStealth(handler); }
    inline bool RemoveOnStealth(Stealth::StealthHandler handler) { return Stealth::RemoveOnStealth(handler); }
    inline bool OnStealth(Stealth::StealthHandler handler) { return Stealth::OnStealth(handler); }

namespace detail {
    inline void EventStealth(const IntegerPropertyChangeEventArgs& args) {
        if (!Stealth::detail::EqualsIgnoreCase(args.Property, "ActionState")) {
            return;
        }

        if (args.Sender.Type != ::Core::Objects::ObjectType::AIHeroClient) {
            return;
        }

        const bool wasStealthed = (args.OldValue & Stealth::detail::IsStealthedMask) != 0;
        const bool isStealthed = (args.NewValue & Stealth::detail::IsStealthedMask) != 0;
        if (wasStealthed == isStealthed) {
            return;
        }

        Stealth::OnStealthEventArgs eventArgs{};
        eventArgs.Sender = args.Sender.Ptr;
        eventArgs.NetworkId = args.Sender.NetworkId;
        eventArgs.IsStealthed = isStealthed;
        eventArgs.Time = isStealthed ? SDK::Events::GameTime() : 0.0f;
        eventArgs.OldValue = args.OldValue;
        eventArgs.NewValue = args.NewValue;
        eventArgs.Raw = args;
        Stealth::detail::StealthHandlers.Fire(eventArgs);
    }
} // namespace detail
} // namespace SDK::Events
