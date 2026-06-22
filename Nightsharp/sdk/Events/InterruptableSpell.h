#pragma once

#include "Events.h"
#include "../Data/InterruptableSpellData.h"
#include "../Enumerations/DangerLevel.h"
#include "../Enumerations/SpellSlot.h"

#include <cstring>

namespace SDK::Events::InterruptableSpell {

struct InterruptableSpellData {
    char Name[64] = {};
    SDK::DangerLevel DangerLevel = SDK::DangerLevel::Low;
    int Slot = -1;
    bool MovementInterrupts = true;
};

struct InterruptableTargetEventArgs {
    uintptr_t Sender = 0;
    uint32_t NetworkId = 0;
    SDK::DangerLevel DangerLevel = SDK::DangerLevel::Low;
    float EndTime = 0.0f;
    bool MovementInterrupts = true;
    InterruptableSpellData SpellData = {};
};

using InterruptableTargetHandler = void(*)(const InterruptableTargetEventArgs&);

namespace detail {
    inline constexpr int MaxCasting = 64;
    inline SDK::Events::detail::EventList<InterruptableTargetEventArgs> InterruptableHandlers;
    inline InterruptableTargetEventArgs Casting[MaxCasting] = {};
    inline int CastingCount = 0;

    inline uint32_t KeyFor(const ::Core::Events::ObjectInfo& object) {
        return object.NetworkId ? object.NetworkId : static_cast<uint32_t>(object.Ptr & 0xFFFFFFFFu);
    }

    inline SDK::SpellSlot ToSpellSlot(int slot) {
        switch (slot) {
        case 0: return SDK::SpellSlot::Q;
        case 1: return SDK::SpellSlot::W;
        case 2: return SDK::SpellSlot::E;
        case 3: return SDK::SpellSlot::R;
        case 4: return SDK::SpellSlot::Summoner1;
        case 5: return SDK::SpellSlot::Summoner2;
        case 6: return SDK::SpellSlot::Item1;
        case 7: return SDK::SpellSlot::Item2;
        case 8: return SDK::SpellSlot::Item3;
        case 9: return SDK::SpellSlot::Item4;
        case 10: return SDK::SpellSlot::Item5;
        case 11: return SDK::SpellSlot::Item6;
        case 12: return SDK::SpellSlot::Trinket;
        case 13: return SDK::SpellSlot::Recall;
        case 64: return SDK::SpellSlot::BasicAttack;
        default: return SDK::SpellSlot::Unknown;
        }
    }

    inline const char* SpellNameOf(const ::Core::Events::ProcessSpellEventArgs& args) {
        return args.SpellName[0] ? args.SpellName : args.ScriptName;
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

    inline float EstimateEndTime(const ::Core::Events::ProcessSpellEventArgs& args) {
        const float now = SDK::Events::GameTime();
        float duration = args.CastDelay;
        if (duration > 10.0f) {
            duration *= 0.001f;
        }
        if (duration <= 0.0f || duration > 12.0f) {
            duration = 1.5f;
        }
        return now + duration;
    }

    inline InterruptableTargetEventArgs* Find(uint32_t networkId, bool create) {
        if (!networkId) {
            return nullptr;
        }

        for (int i = 0; i < CastingCount; ++i) {
            if (Casting[i].NetworkId == networkId) {
                return &Casting[i];
            }
        }

        if (!create || CastingCount >= MaxCasting) {
            return nullptr;
        }

        InterruptableTargetEventArgs& entry = Casting[CastingCount++];
        entry = {};
        entry.NetworkId = networkId;
        return &entry;
    }

    inline void Remove(uint32_t networkId) {
        for (int i = 0; i < CastingCount; ++i) {
            if (Casting[i].NetworkId != networkId) {
                continue;
            }

            for (int j = i; j + 1 < CastingCount; ++j) {
                Casting[j] = Casting[j + 1];
            }
            --CastingCount;
            Casting[CastingCount] = {};
            return;
        }
    }
} // namespace detail

inline bool AddOnInterruptableTarget(InterruptableTargetHandler handler) {
    SDK::Events::Initialize();
    return detail::InterruptableHandlers.Add(handler);
}

inline bool RemoveOnInterruptableTarget(InterruptableTargetHandler handler) {
    return detail::InterruptableHandlers.Remove(handler);
}

inline bool OnInterruptableTarget(InterruptableTargetHandler handler) {
    return AddOnInterruptableTarget(handler);
}

inline InterruptableTargetEventArgs* GetInterruptableTargetData(uint32_t networkId) {
    return detail::Find(networkId, false);
}

template <typename T>
inline InterruptableTargetEventArgs* GetInterruptableTargetData(const T& target) {
    return GetInterruptableTargetData(static_cast<uint32_t>(target.NetworkId()));
}

inline bool IsCastingInterruptableSpell(uint32_t networkId, bool checkMovementInterruption = false) {
    auto* data = GetInterruptableTargetData(networkId);
    return data && (!checkMovementInterruption || data->MovementInterrupts);
}

template <typename T>
inline bool IsCastingInterruptableSpell(const T& target, bool checkMovementInterruption = false) {
    return IsCastingInterruptableSpell(static_cast<uint32_t>(target.NetworkId()), checkMovementInterruption);
}

inline bool MarkCasting(uintptr_t sender,
                        uint32_t networkId,
                        const InterruptableSpellData& spell,
                        float endTime = 0.0f) {
    auto* entry = detail::Find(networkId, true);
    if (!entry) {
        return false;
    }

    entry->Sender = sender;
    entry->NetworkId = networkId;
    entry->DangerLevel = spell.DangerLevel;
    entry->EndTime = endTime;
    entry->MovementInterrupts = spell.MovementInterrupts;
    entry->SpellData = spell;
    return true;
}

} // namespace SDK::Events::InterruptableSpell

namespace SDK::Events {
    inline bool AddOnInterruptableTarget(InterruptableSpell::InterruptableTargetHandler handler) {
        return InterruptableSpell::AddOnInterruptableTarget(handler);
    }

    inline bool RemoveOnInterruptableTarget(InterruptableSpell::InterruptableTargetHandler handler) {
        return InterruptableSpell::RemoveOnInterruptableTarget(handler);
    }

    inline bool OnInterruptableTarget(InterruptableSpell::InterruptableTargetHandler handler) {
        return InterruptableSpell::OnInterruptableTarget(handler);
    }

namespace detail {
    inline void EventInterruptableSpell(const ProcessSpellEventArgs& args) {
        if (!args.Sender.IsValid() || args.IsAutoAttack) {
            return;
        }

        const SDK::SpellSlot slot = InterruptableSpell::detail::ToSpellSlot(args.Slot);
        const char* spellName = InterruptableSpell::detail::SpellNameOf(args);
        const auto* data = SDK::Generated::InterruptableSpellData::FindBySpell(
            args.Sender.CharacterName,
            slot,
            spellName);
        if (!data) {
            return;
        }

        InterruptableSpell::InterruptableSpellData spell{};
        spell.DangerLevel = data->DangerLevel;
        spell.Slot = static_cast<int>(data->Slot == SDK::SpellSlot::Unknown ? slot : data->Slot);
        spell.MovementInterrupts = data->MovementInterrupts;
        InterruptableSpell::detail::Copy(
            spell.Name,
            static_cast<int>(sizeof(spell.Name)),
            data->Name && data->Name[0] ? data->Name : spellName);

        (void)InterruptableSpell::MarkCasting(
            args.Sender.Ptr,
            InterruptableSpell::detail::KeyFor(args.Sender),
            spell,
            InterruptableSpell::detail::EstimateEndTime(args));
    }

    inline void EventInterruptableSpell(const StopCastEventArgs& args) {
        uint32_t key = InterruptableSpell::detail::KeyFor(args.Sender);
        if (!key) {
            key = args.CasterNetworkId;
        }
        if (!key) {
            key = static_cast<uint32_t>(args.Spellbook & 0xFFFFFFFFu);
        }
        InterruptableSpell::detail::Remove(key);
    }

    inline void EventInterruptableSpell() {
        const float now = SDK::Events::GameTime();
        for (int i = 0; i < InterruptableSpell::detail::CastingCount;) {
            const auto& casting = InterruptableSpell::detail::Casting[i];
            if (casting.EndTime > 0.0f && casting.EndTime < now) {
                InterruptableSpell::detail::Remove(casting.NetworkId);
                continue;
            }

            InterruptableSpell::detail::InterruptableHandlers.Fire(casting);
            ++i;
        }
    }
} // namespace detail
} // namespace SDK::Events

namespace SDK::Interrupter {
    using InterruptableSpellData = Events::InterruptableSpell::InterruptableSpellData;
    using InterruptableTargetEventArgs = Events::InterruptableSpell::InterruptableTargetEventArgs;

    inline InterruptableTargetEventArgs* GetInterruptableTargetData(uint32_t networkId) {
        return Events::InterruptableSpell::GetInterruptableTargetData(networkId);
    }

    template <typename T>
    inline InterruptableTargetEventArgs* GetInterruptableTargetData(const T& target) {
        return Events::InterruptableSpell::GetInterruptableTargetData(target);
    }

    inline bool IsCastingInterruptableSpell(uint32_t networkId, bool checkMovementInterruption = false) {
        return Events::InterruptableSpell::IsCastingInterruptableSpell(networkId, checkMovementInterruption);
    }

    template <typename T>
    inline bool IsCastingInterruptableSpell(const T& target, bool checkMovementInterruption = false) {
        return Events::InterruptableSpell::IsCastingInterruptableSpell(target, checkMovementInterruption);
    }
} // namespace SDK::Interrupter
