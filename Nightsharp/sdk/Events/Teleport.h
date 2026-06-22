#pragma once

#include "Events.h"
#include "../Enumerations/TeleportStatus.h"
#include "../Enumerations/TeleportType.h"

#include <cstring>

namespace SDK::Events::Teleport {

struct TeleportEventArgs {
    uintptr_t Object = 0;
    uint32_t NetworkId = 0;
    int Duration = 0;
    int Start = 0;
    bool IsTarget = false;
    SDK::TeleportStatus Status = SDK::TeleportStatus::Unknown;
    SDK::TeleportType Type = SDK::TeleportType::Unknown;
    char RecallType[64] = {};
    char RecallName[64] = {};
    ::Core::Events::TeleportEventArgs Raw = {};
};

using TeleportHandler = void(*)(const TeleportEventArgs&);

namespace detail {
    inline constexpr int ErrorBuffer = 100;
    inline SDK::Events::detail::EventList<TeleportEventArgs> TeleportHandlers;
    inline TeleportEventArgs TeleportData[64] = {};
    inline int TeleportDataCount = 0;

    inline int TickCount() {
        return static_cast<int>(GetTickCount64() & 0x7FFFFFFF);
    }

    inline uint32_t KeyFor(const ::Core::Events::ObjectInfo& object) {
        return object.NetworkId ? object.NetworkId : static_cast<uint32_t>(object.Ptr & 0xFFFFFFFFu);
    }

    inline TeleportEventArgs* Find(uint32_t networkId, bool create) {
        if (!networkId) {
            return nullptr;
        }

        for (int i = 0; i < TeleportDataCount; ++i) {
            if (TeleportData[i].NetworkId == networkId) {
                return &TeleportData[i];
            }
        }

        if (!create || TeleportDataCount >= 64) {
            return nullptr;
        }

        TeleportEventArgs& entry = TeleportData[TeleportDataCount++];
        entry = {};
        entry.NetworkId = networkId;
        return &entry;
    }

    inline bool EqualsIgnoreCase(const char* a, const char* b) {
        return a && b && _stricmp(a, b) == 0;
    }

    inline void Copy(char* dst, int dstCount, const char* src) {
        if (!dst || dstCount <= 0) {
            return;
        }
        dst[0] = 0;
        if (!src) {
            return;
        }
        strncpy_s(dst, static_cast<size_t>(dstCount), src, _TRUNCATE);
    }

    inline int RecallDuration(const char* recallName) {
        if (EqualsIgnoreCase(recallName, "recall")) return 8000;
        if (EqualsIgnoreCase(recallName, "recallimproved")) return 7000;
        if (EqualsIgnoreCase(recallName, "odinrecall")) return 4500;
        if (EqualsIgnoreCase(recallName, "odinrecallimproved")) return 4000;
        if (EqualsIgnoreCase(recallName, "superrecall")) return 4000;
        if (EqualsIgnoreCase(recallName, "superrecallimproved")) return 4000;
        return 0;
    }

    inline SDK::TeleportType TypeFrom(const char* recallType) {
        if (EqualsIgnoreCase(recallType, "Recall")) return SDK::TeleportType::Recall;
        if (EqualsIgnoreCase(recallType, "Teleport")) return SDK::TeleportType::Teleport;
        if (EqualsIgnoreCase(recallType, "Gate")) return SDK::TeleportType::TwistedFate;
        if (EqualsIgnoreCase(recallType, "Shen")) return SDK::TeleportType::Shen;
        return SDK::TeleportType::Unknown;
    }

    inline int DurationFor(SDK::TeleportType type, const char* recallName) {
        switch (type) {
        case SDK::TeleportType::Recall:      return RecallDuration(recallName);
        case SDK::TeleportType::Teleport:    return 4000;
        case SDK::TeleportType::TwistedFate: return 1500;
        case SDK::TeleportType::Shen:        return 3000;
        default:                             return 0;
        }
    }

    inline bool IsTarget(SDK::TeleportType type, const char* recallName) {
        switch (type) {
        case SDK::TeleportType::Teleport:
            return !EqualsIgnoreCase(recallName, "summonerteleport");
        case SDK::TeleportType::TwistedFate:
            return !EqualsIgnoreCase(recallName, "gate");
        case SDK::TeleportType::Shen:
            return !EqualsIgnoreCase(recallName, "shenrchannelmanager");
        default:
            return false;
        }
    }
} // namespace detail

inline bool AddOnTeleport(TeleportHandler handler) {
    SDK::Events::Initialize();
    return detail::TeleportHandlers.Add(handler);
}

inline bool RemoveOnTeleport(TeleportHandler handler) {
    return detail::TeleportHandlers.Remove(handler);
}

inline bool OnTeleport(TeleportHandler handler) {
    return AddOnTeleport(handler);
}

inline TeleportEventArgs GetTeleportData(uint32_t networkId) {
    if (auto* data = detail::Find(networkId, false)) {
        return *data;
    }
    return {};
}

inline TeleportEventArgs GetTeleportData(uintptr_t unitPtr) {
    return GetTeleportData(static_cast<uint32_t>(unitPtr & 0xFFFFFFFFu));
}

template <typename T>
inline TeleportEventArgs GetTeleportData(const T& unit) {
    return GetTeleportData(static_cast<uint32_t>(unit.NetworkId()));
}

} // namespace SDK::Events::Teleport

namespace SDK::Events {
    inline bool AddOnTeleport(Teleport::TeleportHandler handler) { return Teleport::AddOnTeleport(handler); }
    inline bool RemoveOnTeleport(Teleport::TeleportHandler handler) { return Teleport::RemoveOnTeleport(handler); }
    inline bool OnTeleport(Teleport::TeleportHandler handler) { return Teleport::OnTeleport(handler); }

namespace detail {
    inline void EventTeleport(const TeleportRawEventArgs& args) {
        const uint32_t key = Teleport::detail::KeyFor(args.Sender);
        auto* eventArgs = Teleport::detail::Find(key, true);
        if (!eventArgs) {
            return;
        }

        if (args.RecallType[0] != 0) {
            // TODO(EnsoulSharp parity): confirm RecallType/RecallName mapping for
            // every current teleport source. Newer map gates/portal-like systems
            // may require extra status/type fields beyond old FOWRecall strings.
            eventArgs->Object = args.Sender.Ptr;
            eventArgs->NetworkId = key;
            eventArgs->Type = Teleport::detail::TypeFrom(args.RecallType);
            eventArgs->Status = SDK::TeleportStatus::Start;
            eventArgs->Start = Teleport::detail::TickCount();
            eventArgs->Duration = Teleport::detail::DurationFor(eventArgs->Type, args.RecallName);
            eventArgs->IsTarget = Teleport::detail::IsTarget(eventArgs->Type, args.RecallName);
            eventArgs->Raw = args;
            Teleport::detail::Copy(eventArgs->RecallType, static_cast<int>(sizeof(eventArgs->RecallType)), args.RecallType);
            Teleport::detail::Copy(eventArgs->RecallName, static_cast<int>(sizeof(eventArgs->RecallName)), args.RecallName);
        } else {
            const int elapsed = Teleport::detail::TickCount() - eventArgs->Start;
            eventArgs->Status = elapsed < eventArgs->Duration - Teleport::detail::ErrorBuffer
                ? SDK::TeleportStatus::Abort
                : SDK::TeleportStatus::Finish;
            eventArgs->Raw = args;
        }

        Teleport::detail::TeleportHandlers.Fire(*eventArgs);
    }
} // namespace detail
} // namespace SDK::Events
