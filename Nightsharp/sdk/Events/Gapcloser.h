#pragma once

#include "Events.h"
#include "../Data/GapcloserData.h"
#include "../Enumerations/GapcloserType.h"

#include <cstring>

namespace SDK::Events::Gapcloser {

struct GapCloser {
    SDK::GapcloserType SkillType = SDK::GapcloserType::Skillshot;
    int Slot = -1;
    char SpellName[64] = {};
    bool Invert = false;
};

struct GapCloserEventArgs {
    Vec3 Start = {};
    Vec3 End = {};
    uintptr_t Sender = 0;
    uint32_t NetworkId = 0;
    SDK::GapcloserType SkillType = SDK::GapcloserType::Skillshot;
    int Slot = -1;
    bool IsDirectedToPlayer = false;
    char SpellName[64] = {};
    int TickCount = 0;
    ::Core::Events::ProcessSpellEventArgs Raw = {};
};

using GapCloserHandler = void(*)(const GapCloserEventArgs&);

namespace detail {
    inline constexpr int MaxActiveGapclosers = 64;
    inline SDK::Events::detail::EventList<GapCloserEventArgs> GapCloserHandlers;
    inline GapCloserEventArgs ActiveSpells[MaxActiveGapclosers] = {};
    inline int ActiveSpellCount = 0;

    inline int TickCount() {
        return static_cast<int>(GetTickCount64() & 0x7FFFFFFF);
    }

    inline void Copy(char* dst, int dstCount, const char* src) {
        if (!dst || dstCount <= 0) {
            return;
        }
        dst[0] = 0;
        if (src) {
            strncpy_s(dst, static_cast<size_t>(dstCount), src, _TRUNCATE);
        }
    }

    inline const char* SpellNameOf(const ::Core::Events::ProcessSpellEventArgs& args) {
        return args.SpellName[0] ? args.SpellName : args.ScriptName;
    }

    inline ::Core::Events::ObjectInfo LocalPlayer() {
        const uintptr_t player = CoreRuntime::GetContext().localPlayer;
        return ::Core::Events::detail::ReadObject(player);
    }

    inline bool IsLocalPlayerTarget(const ::Core::Events::ProcessSpellEventArgs& args,
                                    const ::Core::Events::ObjectInfo& player) {
        if (!player.NetworkId) {
            return false;
        }

        return args.TargetNetworkId == player.NetworkId ||
               args.Target.NetworkId == player.NetworkId ||
               static_cast<uint32_t>(args.TargetIndex) == player.Index;
    }

    inline bool IsPathTowardPlayer(const ::Core::Events::ProcessSpellEventArgs& args,
                                   const ::Core::Events::ObjectInfo& player,
                                   bool invert) {
        if (!player.Position.IsValid() || !args.StartPosition.IsValid() || !args.EndPosition.IsValid()) {
            return false;
        }

        const float startDistance = args.StartPosition.Distance2D(player.Position);
        const float endDistance = args.EndPosition.Distance2D(player.Position);
        return invert ? endDistance > startDistance : endDistance < startDistance;
    }

    inline bool IsRelevantToPlayer(const GapCloserEventArgs& args,
                                   const ::Core::Events::ObjectInfo& player) {
        if (!player.Position.IsValid()) {
            return true;
        }

        if (args.IsDirectedToPlayer) {
            return true;
        }

        if (args.SkillType == SDK::GapcloserType::Targeted) {
            return false;
        }

        const float startDistance = args.Start.Distance2D(player.Position);
        const float endDistance = args.End.Distance2D(player.Position);
        return startDistance <= 900.0f || endDistance <= 900.0f;
    }

    inline void RemoveExpired() {
        const int now = TickCount();
        for (int i = 0; i < ActiveSpellCount;) {
            if (now <= ActiveSpells[i].TickCount + 900) {
                ++i;
                continue;
            }

            for (int j = i; j + 1 < ActiveSpellCount; ++j) {
                ActiveSpells[j] = ActiveSpells[j + 1];
            }
            --ActiveSpellCount;
            ActiveSpells[ActiveSpellCount] = {};
        }
    }
} // namespace detail

inline bool AddOnGapCloser(GapCloserHandler handler) {
    if (!handler) {
        return false;
    }

    SDK::Events::Initialize();
    if (!SDK::Events::detail::EnsureDoCastRawSubscribed()) {
        return false;
    }

    const bool hadHandlers = detail::GapCloserHandlers.HasHandlers();
    const bool added = detail::GapCloserHandlers.Add(handler);
    if (added && !hadHandlers) {
        ++SDK::Events::detail::GapcloserConsumerRefs;
    } else if (!added && !hadHandlers) {
        SDK::Events::detail::ReleaseDoCastRawIfUnused();
    }
    return added;
}

inline bool RemoveOnGapCloser(GapCloserHandler handler) {
    const bool removed = detail::GapCloserHandlers.Remove(handler);
    if (removed && !detail::GapCloserHandlers.HasHandlers()) {
        if (SDK::Events::detail::GapcloserConsumerRefs > 0) {
            --SDK::Events::detail::GapcloserConsumerRefs;
        }
        SDK::Events::detail::ReleaseDoCastRawIfUnused();
    }
    return removed;
}

inline bool OnGapCloser(GapCloserHandler handler) {
    return AddOnGapCloser(handler);
}

inline int ActiveSpellCount() {
    return detail::ActiveSpellCount;
}

inline const GapCloserEventArgs* ActiveSpells() {
    return detail::ActiveSpells;
}

inline bool AddActiveSpell(const GapCloserEventArgs& args) {
    if (detail::ActiveSpellCount >= detail::MaxActiveGapclosers) {
        return false;
    }
    detail::ActiveSpells[detail::ActiveSpellCount++] = args;
    return true;
}

} // namespace SDK::Events::Gapcloser

namespace SDK::Events {
    inline bool AddOnGapCloser(Gapcloser::GapCloserHandler handler) { return Gapcloser::AddOnGapCloser(handler); }
    inline bool RemoveOnGapCloser(Gapcloser::GapCloserHandler handler) { return Gapcloser::RemoveOnGapCloser(handler); }
    inline bool OnGapCloser(Gapcloser::GapCloserHandler handler) { return Gapcloser::OnGapCloser(handler); }

namespace detail {
    inline void EventGapcloser(const ProcessSpellEventArgs& args) {
        if (!args.Sender.IsValid() || args.IsAutoAttack) {
            return;
        }

        const auto* data = SDK::Generated::GapcloserData::FindBySpellName(Gapcloser::detail::SpellNameOf(args));
        if (!data) {
            return;
        }

        const ::Core::Events::ObjectInfo player = Gapcloser::detail::LocalPlayer();
        Gapcloser::GapCloserEventArgs eventArgs{};
        eventArgs.Start = args.StartPosition;
        eventArgs.End = args.EndPosition;
        eventArgs.Sender = args.Sender.Ptr;
        eventArgs.NetworkId = args.Sender.NetworkId;
        eventArgs.SkillType = data->SkillType;
        eventArgs.Slot = static_cast<int>(data->Slot);
        eventArgs.IsDirectedToPlayer =
            Gapcloser::detail::IsLocalPlayerTarget(args, player) ||
            Gapcloser::detail::IsPathTowardPlayer(args, player, data->Invert);
        eventArgs.TickCount = Gapcloser::detail::TickCount();
        eventArgs.Raw = args;
        Gapcloser::detail::Copy(
            eventArgs.SpellName,
            static_cast<int>(sizeof(eventArgs.SpellName)),
            data->SpellName);

        (void)Gapcloser::AddActiveSpell(eventArgs);
    }

    inline void EventGapcloser() {
        Gapcloser::detail::RemoveExpired();
        const ::Core::Events::ObjectInfo player = Gapcloser::detail::LocalPlayer();
        for (int i = 0; i < Gapcloser::detail::ActiveSpellCount; ++i) {
            const auto& spell = Gapcloser::detail::ActiveSpells[i];
            if (Gapcloser::detail::IsRelevantToPlayer(spell, player)) {
                Gapcloser::detail::GapCloserHandlers.Fire(spell);
            }
        }
    }
} // namespace detail
} // namespace SDK::Events
