#pragma once

#include "../../Core/CoreEvents.h"
#include "../Data/Database.h"

#include <cstdint>
#include <cstring>

namespace SDK::Events {

using CoreHookId = ::Core::Events::HookId;
using CoreHookArgs = ::Core::Events::RawEventArgs;
using CoreHookHandler = ::Core::Events::RawCallback;
namespace Hooks = ::Core::Events::Hooks;

using GameUpdateEventArgs = ::Core::Events::GameUpdateEventArgs;
using ObjectEventArgs = ::Core::Events::ObjectEventArgs;
using BuffEventArgs = ::Core::Events::BuffEventArgs;
using NewPathEventArgs = ::Core::Events::NewPathEventArgs;
using IntegerPropertyChangeEventArgs = ::Core::Events::IntegerPropertyChangeEventArgs;
using TeleportRawEventArgs = ::Core::Events::TeleportEventArgs;
using ProcessSpellEventArgs = ::Core::Events::ProcessSpellEventArgs;
using CastSpellEventArgs = ::Core::Events::CastSpellEventArgs;
using PlayAnimationEventArgs = ::Core::Events::PlayAnimationEventArgs;
using StopCastEventArgs = ::Core::Events::StopCastEventArgs;

namespace detail {
    inline float GameTime() {
        const auto& ctx = CoreRuntime::GetContext();
        if ((ctx.statusMask & CoreRuntime::Status_GameTimeReady) != 0) {
            return ctx.gameTime;
        }

        (void)CoreRuntime::RefreshReadState();
        return CoreRuntime::GetContext().gameTime;
    }

    inline bool IsGameLoaded() {
        const auto& ctx = CoreRuntime::GetContext();
        return (ctx.statusMask & CoreRuntime::BuildRequiredRuntimeMask()) == CoreRuntime::BuildRequiredRuntimeMask()
            && ctx.gameTime > 0.0f;
    }

    template <typename T, int MaxHandlers = 32>
    struct EventList {
        using Handler = void(*)(const T&);

        Handler Handlers[MaxHandlers] = {};
        int Count = 0;

        bool Add(Handler handler) {
            if (!handler) {
                return false;
            }
            for (int i = 0; i < Count; ++i) {
                if (Handlers[i] == handler) {
                    return true;
                }
            }
            if (Count >= MaxHandlers) {
                return false;
            }
            Handlers[Count++] = handler;
            return true;
        }

        bool Remove(Handler handler) {
            if (!handler) {
                return false;
            }
            for (int i = 0; i < Count; ++i) {
                if (Handlers[i] != handler) {
                    continue;
                }
                for (int j = i; j + 1 < Count; ++j) {
                    Handlers[j] = Handlers[j + 1];
                }
                Handlers[--Count] = nullptr;
                return true;
            }
            return false;
        }

        void Fire(const T& args) const {
            for (int i = 0; i < Count; ++i) {
                Handler handler = Handlers[i];
                if (!handler) {
                    continue;
                }
                __try {
                    handler(args);
                } __except (1) {}
            }
        }

        void Clear() {
            for (int i = 0; i < Count; ++i) {
                Handlers[i] = nullptr;
            }
            Count = 0;
        }
    };

    inline bool Initialized = false;

    inline EventList<CoreHookArgs> CoreHookHandlers;
    inline EventList<GameUpdateEventArgs> GameUpdateHandlers;
    inline EventList<ObjectEventArgs> ObjectCreateHandlers;
    inline EventList<ObjectEventArgs> ObjectDeleteHandlers;
    inline EventList<ObjectEventArgs> MissileCreateHandlers;
    inline EventList<ObjectEventArgs> MissileDeleteHandlers;
    inline EventList<BuffEventArgs> BuffAddHandlers;
    inline EventList<BuffEventArgs> BuffRemoveHandlers;
    inline EventList<BuffEventArgs> BuffUpdateHandlers;
    inline EventList<NewPathEventArgs> NewPathHandlers;
    inline EventList<IntegerPropertyChangeEventArgs> IntegerPropertyChangeHandlers;
    inline EventList<TeleportRawEventArgs> TeleportHandlers;
    inline EventList<ProcessSpellEventArgs> DoCastHandlers;
    inline EventList<ProcessSpellEventArgs> ProcessSpellHandlers;
    inline EventList<CastSpellEventArgs> ProcessCastSpellHandlers;
    inline EventList<ProcessSpellEventArgs> FinishCastHandlers;
    inline EventList<ProcessSpellEventArgs> SpellImpactHandlers;
    inline EventList<PlayAnimationEventArgs> PlayAnimationHandlers;
    inline EventList<StopCastEventArgs> StopCastHandlers;

    inline void EventLoad();
    inline void EventDash(const NewPathEventArgs& args);
    inline void EventStealth(const IntegerPropertyChangeEventArgs& args);
    inline void EventTeleport(const TeleportRawEventArgs& args);
    inline void EventGapcloser(const ProcessSpellEventArgs& args);
    inline void EventGapcloser();
    inline void EventInterruptableSpell(const ProcessSpellEventArgs& args);
    inline void EventInterruptableSpell(const StopCastEventArgs& args);
    inline void EventInterruptableSpell();
    inline void EventTurret(const ProcessSpellEventArgs& args);
    inline void EventTurretConstruct();
    inline void ResetDerivedEvents();

    inline void OnRawCoreHook(const CoreHookArgs& raw) {
        CoreHookHandlers.Fire(raw);
    }

    inline void CopyText(char* out, int outCount, const char* value) {
        if (!out || outCount <= 0) {
            return;
        }

        out[0] = 0;
        if (value) {
            strncpy_s(out, static_cast<std::size_t>(outCount), value, _TRUNCATE);
        }
    }

    inline bool IsPlausibleIdentifier(const char* value) {
        if (!value || !value[0]) {
            return false;
        }

        int length = 0;
        for (; value[length] && length < 95; ++length) {
            const unsigned char ch =
                static_cast<unsigned char>(value[length]);
            if (ch < 0x20 || ch > 0x7E) {
                return false;
            }
        }
        return length >= 2 && length < 95;
    }

    inline const SDK::Data::SpellData* ResolveMissileSpellData(
        const ObjectEventArgs& args) {
        const char* names[] = {
            args.MissileName,
            args.SpellName,
            args.Sender.Name,
            args.Sender.CharacterName,
        };

        for (const char* name : names) {
            if (!name || !name[0]) {
                continue;
            }

            if (const auto* data = SDK::Data::GetSpellByMissile(name)) {
                return data;
            }
            if (const auto* data = SDK::Data::GetSpellByName(name)) {
                return data;
            }
        }

        return nullptr;
    }

    inline void CanonicalizeMissileNames(ObjectEventArgs& args) {
        const auto* data = ResolveMissileSpellData(args);
        if (!data || data->spellName.empty()) {
            return;
        }

        // Keep runtime names intact when the client already exposed them.
        // SpellName is the cast/slot key (EzrealQ), while MissileName is the
        // missile runtime key (EzrealMysticShot). Database names are only a
        // fallback for older builds or partially decoded payloads.
        if (!IsPlausibleIdentifier(args.SpellName)) {
            const auto& fallback =
                !data->extraSpellNames.empty()
                    ? data->extraSpellNames.front()
                    : data->spellName;
            CopyText(
                args.SpellName,
                static_cast<int>(sizeof(args.SpellName)),
                fallback.c_str());
        }
        if (!IsPlausibleIdentifier(args.MissileName)) {
            CopyText(
                args.MissileName,
                static_cast<int>(sizeof(args.MissileName)),
                data->spellName.c_str());
        }
    }

    inline constexpr int MaxPendingObjectCreates = 512;
    inline CoreHookArgs PendingObjectCreates[MaxPendingObjectCreates] = {};
    inline int PendingObjectCreateCount = 0;

    inline void QueueObjectCreate(const CoreHookArgs& raw) {
        const ObjectEventArgs args = ::Core::Events::DecodeObjectLifecycleEvent(raw);
        if (!args.Sender.IsValid()) {
            return;
        }

        for (int index = 0; index < PendingObjectCreateCount; ++index) {
            auto& pending = PendingObjectCreates[index];
            if (pending.Rdx == raw.Rdx ||
                (pending.R8 != 0 && pending.R8 == raw.R8)) {
                pending = raw;
                return;
            }
        }

        if (PendingObjectCreateCount < MaxPendingObjectCreates) {
            PendingObjectCreates[PendingObjectCreateCount++] = raw;
            return;
        }

        ObjectCreateHandlers.Fire(args);
    }

    inline void RemovePendingObjectCreate(uintptr_t object, uint32_t networkId) {
        for (int index = 0; index < PendingObjectCreateCount;) {
            const auto& pending = PendingObjectCreates[index];
            const bool sameObject = pending.Rdx == object;
            const bool sameNetworkId = networkId != 0 &&
                networkId != 0xFFFFFFFFu &&
                static_cast<uint32_t>(pending.R8) == networkId;
            if (!sameObject && !sameNetworkId) {
                ++index;
                continue;
            }

            for (int moveIndex = index; moveIndex + 1 < PendingObjectCreateCount; ++moveIndex) {
                PendingObjectCreates[moveIndex] = PendingObjectCreates[moveIndex + 1];
            }
            PendingObjectCreates[--PendingObjectCreateCount] = {};
        }
    }

    inline void FlushObjectCreates() {
        const int count = PendingObjectCreateCount;
        PendingObjectCreateCount = 0;

        for (int index = 0; index < count; ++index) {
            const CoreHookArgs raw = PendingObjectCreates[index];
            PendingObjectCreates[index] = {};
            const ObjectEventArgs args = ::Core::Events::DecodeObjectLifecycleEvent(raw);
            if (args.Sender.IsValid()) {
                ObjectCreateHandlers.Fire(args);
            }
        }
    }

    inline void OnRawObjectCreate(const CoreHookArgs& raw) {
        QueueObjectCreate(raw);
    }

    inline void OnRawObjectDelete(const CoreHookArgs& raw) {
        const ObjectEventArgs args = ::Core::Events::DecodeObjectLifecycleEvent(raw);
        if (!args.Sender.IsValid()) {
            return;
        }

        RemovePendingObjectCreate(args.Sender.Ptr, args.Sender.NetworkId);
        ObjectDeleteHandlers.Fire(args);
    }

    inline void OnRawGameUpdate(const CoreHookArgs& raw) {
        (void)CoreRuntime::RefreshReadState();
        FlushObjectCreates();

        GameUpdateEventArgs args{};
        args.Raw = raw;
        GameUpdateHandlers.Fire(args);

        EventLoad();
        EventGapcloser();
        EventInterruptableSpell();
    }

    inline void OnRawMissileCreate(const CoreHookArgs& raw) {
        ObjectEventArgs args = ::Core::Events::DecodeMissileEvent(raw);
        CanonicalizeMissileNames(args);
        MissileCreateHandlers.Fire(args);
    }

    inline void OnRawMissileDelete(const CoreHookArgs& raw) {
        ObjectEventArgs args = ::Core::Events::DecodeMissileEvent(raw);
        CanonicalizeMissileNames(args);
        MissileDeleteHandlers.Fire(args);
    }

    inline void OnRawBuffAdd(const CoreHookArgs& raw) {
        BuffAddHandlers.Fire(::Core::Events::DecodeBuffEvent(raw));
    }

    inline void OnRawBuffRemove(const CoreHookArgs& raw) {
        BuffRemoveHandlers.Fire(::Core::Events::DecodeBuffEvent(raw));
    }

    inline void OnRawBuffUpdate(const CoreHookArgs& raw) {
        BuffUpdateHandlers.Fire(::Core::Events::DecodeBuffEvent(raw));
    }

    inline void OnRawNewPath(const CoreHookArgs& raw) {
        const NewPathEventArgs args = ::Core::Events::DecodeNewPath(raw);
        NewPathHandlers.Fire(args);
        EventDash(args);
    }

    inline void OnRawIntegerPropertyChange(const CoreHookArgs& raw) {
        const IntegerPropertyChangeEventArgs args = ::Core::Events::DecodeIntegerPropertyChange(raw);
        IntegerPropertyChangeHandlers.Fire(args);
        EventStealth(args);
    }

    inline void OnRawTeleport(const CoreHookArgs& raw) {
        const TeleportRawEventArgs args = ::Core::Events::DecodeTeleport(raw);
        TeleportHandlers.Fire(args);
        EventTeleport(args);
    }

    inline void OnRawDoCast(const CoreHookArgs& raw) {
        // OnDoCast (sub_97C290) is a vtable spell-instance method — RCX is a
        // SpellDataInstance, NOT a SpellBookClient. Use DecodeDoCast which
        // resolves the hero through ObjectManager::FindByIndex(*(RCX+0xA8))
        // exactly like the native routine does. The previous code reused
        // DecodeProcessSpell, which interpreted RCX as a spellbook and
        // produced bogus senders / failed IsLocalPlayer checks.
        const ProcessSpellEventArgs args = ::Core::Events::DecodeDoCast(raw);
        DoCastHandlers.Fire(args);
        EventGapcloser(args);
        EventInterruptableSpell(args);
        EventTurret(args);
    }


    inline void OnRawProcessSpell(const CoreHookArgs& raw) {
        ProcessSpellHandlers.Fire(::Core::Events::DecodeProcessSpell(raw));
    }

    inline void OnRawProcessCastSpell(const CoreHookArgs& raw) {
        ProcessCastSpellHandlers.Fire(::Core::Events::DecodeProcessCastSpell(raw));
    }

    inline void OnRawFinishCast(const CoreHookArgs& raw) {
        FinishCastHandlers.Fire(::Core::Events::DecodeProcessSpell(raw));
    }

    inline void OnRawSpellImpact(const CoreHookArgs& raw) {
        SpellImpactHandlers.Fire(::Core::Events::DecodeProcessSpell(raw));
    }

    inline void OnRawPlayAnimation(const CoreHookArgs& raw) {
        PlayAnimationHandlers.Fire(::Core::Events::DecodePlayAnimation(raw));
    }

    inline void OnRawStopCast(const CoreHookArgs& raw) {
        const StopCastEventArgs args = ::Core::Events::DecodeStopCast(raw);
        StopCastHandlers.Fire(args);
        EventInterruptableSpell(args);
    }
} // namespace detail

inline void Initialize() {
    if (detail::Initialized) {
        ::Core::Events::Initialize();
        return;
    }

    detail::Initialized = true;
    ::Core::Events::Initialize();
    for (int i = 0; i < ::CoreHookTest::HookCount; ++i) {
        ::Core::Events::Add(static_cast<CoreHookId>(i), &detail::OnRawCoreHook);
    }
    ::Core::Events::AddOnGameUpdate(&detail::OnRawGameUpdate);
    ::Core::Events::AddOnCreateObject(&detail::OnRawObjectCreate);
    ::Core::Events::AddOnDeleteObject(&detail::OnRawObjectDelete);
    ::Core::Events::AddOnMissileCreate(&detail::OnRawMissileCreate);
    ::Core::Events::AddOnMissileDelete(&detail::OnRawMissileDelete);
    ::Core::Events::AddOnBuffAdd(&detail::OnRawBuffAdd);
    ::Core::Events::AddOnBuffRemove(&detail::OnRawBuffRemove);
    ::Core::Events::AddOnBuffUpdate(&detail::OnRawBuffUpdate);
    ::Core::Events::AddOnNewPath(&detail::OnRawNewPath);
    ::Core::Events::AddOnIntegerPropertyChange(&detail::OnRawIntegerPropertyChange);
    ::Core::Events::AddOnTeleport(&detail::OnRawTeleport);
    ::Core::Events::AddOnDoCast(&detail::OnRawDoCast);
    ::Core::Events::AddOnProcessSpell(&detail::OnRawProcessSpell);
    ::Core::Events::AddOnProcessCastSpell(&detail::OnRawProcessCastSpell);
    ::Core::Events::AddOnFinishCast(&detail::OnRawFinishCast);
    ::Core::Events::AddOnSpellImpact(&detail::OnRawSpellImpact);
    // Subscribe BOTH animation hook IDs:
    //   - OnPlayAnimationWrapper (sub_E15D10): central dispatch wrapper used by
    //     local-player animation paths. RCX = wrapper, *(RCX+0x08) = AIBaseClient.
    //   - OnPlayAnimation (sub_29BF90): packet callback for net packet ids 0x11D
    //     and 0x1F1 (server-driven animations on any unit, incl. enemies).
    //     RCX = AIBaseClient directly, string-view at RDX+0x18 (data) / +0x20 (size).
    // DecodePlayAnimation in CoreEvents.h branches on raw.Id to handle both
    // layouts. Subscribing only the wrapper missed every packet-driven animation.
    ::Core::Events::AddOnPlayAnimationWrapper(&detail::OnRawPlayAnimation);
    ::Core::Events::AddOnPlayAnimation(&detail::OnRawPlayAnimation);
    ::Core::Events::AddOnStopCast(&detail::OnRawStopCast);
    detail::EventTurretConstruct();
}

inline void Reset() {
    detail::CoreHookHandlers.Clear();
    detail::GameUpdateHandlers.Clear();
    detail::ObjectCreateHandlers.Clear();
    detail::ObjectDeleteHandlers.Clear();
    detail::PendingObjectCreateCount = 0;
    detail::MissileCreateHandlers.Clear();
    detail::MissileDeleteHandlers.Clear();
    detail::BuffAddHandlers.Clear();
    detail::BuffRemoveHandlers.Clear();
    detail::BuffUpdateHandlers.Clear();
    detail::NewPathHandlers.Clear();
    detail::IntegerPropertyChangeHandlers.Clear();
    detail::TeleportHandlers.Clear();
    detail::DoCastHandlers.Clear();
    detail::ProcessSpellHandlers.Clear();
    detail::ProcessCastSpellHandlers.Clear();
    detail::FinishCastHandlers.Clear();
    detail::SpellImpactHandlers.Clear();
    detail::PlayAnimationHandlers.Clear();
    detail::StopCastHandlers.Clear();
    detail::ResetDerivedEvents();
    detail::Initialized = false;
}

inline bool AddOnCoreHook(CoreHookHandler handler) {
    Initialize();
    return detail::CoreHookHandlers.Add(handler);
}

inline bool RemoveOnCoreHook(CoreHookHandler handler) {
    return detail::CoreHookHandlers.Remove(handler);
}

inline bool AddOnCoreHook(CoreHookId id, CoreHookHandler handler) {
    Initialize();
    return ::Core::Events::Add(id, handler);
}

inline bool RemoveOnCoreHook(CoreHookId id, CoreHookHandler handler) {
    return ::Core::Events::Remove(id, handler);
}

inline float GameTime() {
    return detail::GameTime();
}

inline bool IsLocalPlayer(const Core::Events::ObjectInfo& sender) {
    if (!sender.IsValid()) {
        return false;
    }

    const uintptr_t playerAddress = ::Core::ObjectManager::PlayerAddress();
    if (!playerAddress) {
        return false;
    }
    if (sender.Ptr == playerAddress) {
        return true;
    }

    const uint32_t playerNetworkId = ::Core::Objects::ReadNetworkId(playerAddress);
    return sender.NetworkId != 0 &&
           sender.NetworkId != 0xFFFFFFFFu &&
           playerNetworkId != 0 &&
           sender.NetworkId == playerNetworkId;
}

inline bool AddOnGameUpdate(void(*handler)(const GameUpdateEventArgs&)) { Initialize(); return detail::GameUpdateHandlers.Add(handler); }
inline bool RemoveOnGameUpdate(void(*handler)(const GameUpdateEventArgs&)) { return detail::GameUpdateHandlers.Remove(handler); }
inline bool OnGameUpdate(void(*handler)(const GameUpdateEventArgs&)) { return AddOnGameUpdate(handler); }

inline bool AddOnCreateObject(void(*handler)(const ObjectEventArgs&)) { Initialize(); return detail::ObjectCreateHandlers.Add(handler); }
inline bool RemoveOnCreateObject(void(*handler)(const ObjectEventArgs&)) { return detail::ObjectCreateHandlers.Remove(handler); }
inline bool OnCreateObject(void(*handler)(const ObjectEventArgs&)) { return AddOnCreateObject(handler); }

inline bool AddOnDeleteObject(void(*handler)(const ObjectEventArgs&)) { Initialize(); return detail::ObjectDeleteHandlers.Add(handler); }
inline bool RemoveOnDeleteObject(void(*handler)(const ObjectEventArgs&)) { return detail::ObjectDeleteHandlers.Remove(handler); }
inline bool OnDeleteObject(void(*handler)(const ObjectEventArgs&)) { return AddOnDeleteObject(handler); }

inline bool AddOnMissileCreate(void(*handler)(const ObjectEventArgs&)) { Initialize(); return detail::MissileCreateHandlers.Add(handler); }
inline bool RemoveOnMissileCreate(void(*handler)(const ObjectEventArgs&)) { return detail::MissileCreateHandlers.Remove(handler); }
inline bool OnMissileCreate(void(*handler)(const ObjectEventArgs&)) { return AddOnMissileCreate(handler); }

inline bool AddOnMissileDelete(void(*handler)(const ObjectEventArgs&)) { Initialize(); return detail::MissileDeleteHandlers.Add(handler); }
inline bool RemoveOnMissileDelete(void(*handler)(const ObjectEventArgs&)) { return detail::MissileDeleteHandlers.Remove(handler); }
inline bool OnMissileDelete(void(*handler)(const ObjectEventArgs&)) { return AddOnMissileDelete(handler); }

inline bool AddOnBuffAdd(void(*handler)(const BuffEventArgs&)) { Initialize(); return detail::BuffAddHandlers.Add(handler); }
inline bool RemoveOnBuffAdd(void(*handler)(const BuffEventArgs&)) { return detail::BuffAddHandlers.Remove(handler); }
inline bool OnBuffAdd(void(*handler)(const BuffEventArgs&)) { return AddOnBuffAdd(handler); }

inline bool AddOnBuffRemove(void(*handler)(const BuffEventArgs&)) { Initialize(); return detail::BuffRemoveHandlers.Add(handler); }
inline bool RemoveOnBuffRemove(void(*handler)(const BuffEventArgs&)) { return detail::BuffRemoveHandlers.Remove(handler); }
inline bool OnBuffRemove(void(*handler)(const BuffEventArgs&)) { return AddOnBuffRemove(handler); }

inline bool AddOnBuffUpdate(void(*handler)(const BuffEventArgs&)) { Initialize(); return detail::BuffUpdateHandlers.Add(handler); }
inline bool RemoveOnBuffUpdate(void(*handler)(const BuffEventArgs&)) { return detail::BuffUpdateHandlers.Remove(handler); }
inline bool OnBuffUpdate(void(*handler)(const BuffEventArgs&)) { return AddOnBuffUpdate(handler); }

inline bool AddOnNewPath(void(*handler)(const NewPathEventArgs&)) { Initialize(); return detail::NewPathHandlers.Add(handler); }
inline bool RemoveOnNewPath(void(*handler)(const NewPathEventArgs&)) { return detail::NewPathHandlers.Remove(handler); }
inline bool OnNewPath(void(*handler)(const NewPathEventArgs&)) { return AddOnNewPath(handler); }

inline bool AddOnIntegerPropertyChange(void(*handler)(const IntegerPropertyChangeEventArgs&)) { Initialize(); return detail::IntegerPropertyChangeHandlers.Add(handler); }
inline bool RemoveOnIntegerPropertyChange(void(*handler)(const IntegerPropertyChangeEventArgs&)) { return detail::IntegerPropertyChangeHandlers.Remove(handler); }
inline bool OnIntegerPropertyChange(void(*handler)(const IntegerPropertyChangeEventArgs&)) { return AddOnIntegerPropertyChange(handler); }

inline bool AddOnTeleportRaw(void(*handler)(const TeleportRawEventArgs&)) { Initialize(); return detail::TeleportHandlers.Add(handler); }
inline bool RemoveOnTeleportRaw(void(*handler)(const TeleportRawEventArgs&)) { return detail::TeleportHandlers.Remove(handler); }
inline bool OnTeleportRaw(void(*handler)(const TeleportRawEventArgs&)) { return AddOnTeleportRaw(handler); }

inline bool AddOnDoCast(void(*handler)(const ProcessSpellEventArgs&)) { Initialize(); return detail::DoCastHandlers.Add(handler); }
inline bool RemoveOnDoCast(void(*handler)(const ProcessSpellEventArgs&)) { return detail::DoCastHandlers.Remove(handler); }
inline bool OnDoCast(void(*handler)(const ProcessSpellEventArgs&)) { return AddOnDoCast(handler); }

inline bool AddOnProcessSpell(void(*handler)(const ProcessSpellEventArgs&)) { Initialize(); return detail::ProcessSpellHandlers.Add(handler); }
inline bool RemoveOnProcessSpell(void(*handler)(const ProcessSpellEventArgs&)) { return detail::ProcessSpellHandlers.Remove(handler); }
inline bool OnProcessSpell(void(*handler)(const ProcessSpellEventArgs&)) { return AddOnProcessSpell(handler); }

inline bool AddOnProcessCastSpell(void(*handler)(const CastSpellEventArgs&)) { Initialize(); return detail::ProcessCastSpellHandlers.Add(handler); }
inline bool RemoveOnProcessCastSpell(void(*handler)(const CastSpellEventArgs&)) { return detail::ProcessCastSpellHandlers.Remove(handler); }
inline bool OnProcessCastSpell(void(*handler)(const CastSpellEventArgs&)) { return AddOnProcessCastSpell(handler); }

inline bool AddOnFinishCast(void(*handler)(const ProcessSpellEventArgs&)) { Initialize(); return detail::FinishCastHandlers.Add(handler); }
inline bool RemoveOnFinishCast(void(*handler)(const ProcessSpellEventArgs&)) { return detail::FinishCastHandlers.Remove(handler); }
inline bool OnFinishCast(void(*handler)(const ProcessSpellEventArgs&)) { return AddOnFinishCast(handler); }

inline bool AddOnSpellImpact(void(*handler)(const ProcessSpellEventArgs&)) { Initialize(); return detail::SpellImpactHandlers.Add(handler); }
inline bool RemoveOnSpellImpact(void(*handler)(const ProcessSpellEventArgs&)) { return detail::SpellImpactHandlers.Remove(handler); }
inline bool OnSpellImpact(void(*handler)(const ProcessSpellEventArgs&)) { return AddOnSpellImpact(handler); }

inline bool AddOnPlayAnimation(void(*handler)(const PlayAnimationEventArgs&)) { Initialize(); return detail::PlayAnimationHandlers.Add(handler); }
inline bool RemoveOnPlayAnimation(void(*handler)(const PlayAnimationEventArgs&)) { return detail::PlayAnimationHandlers.Remove(handler); }
inline bool OnPlayAnimation(void(*handler)(const PlayAnimationEventArgs&)) { return AddOnPlayAnimation(handler); }

inline bool AddOnStopCast(void(*handler)(const StopCastEventArgs&)) { Initialize(); return detail::StopCastHandlers.Add(handler); }
inline bool RemoveOnStopCast(void(*handler)(const StopCastEventArgs&)) { return detail::StopCastHandlers.Remove(handler); }
inline bool OnStopCast(void(*handler)(const StopCastEventArgs&)) { return AddOnStopCast(handler); }

template <typename T>
struct EventSlot {
    using Handler = void(*)(const T&);
    using Fn = bool(*)(Handler);

    Fn Add = nullptr;
    Fn Remove = nullptr;

    bool operator+=(Handler handler) const {
        return Add ? Add(handler) : false;
    }

    bool operator-=(Handler handler) const {
        return Remove ? Remove(handler) : false;
    }

    bool operator()(Handler handler) const {
        return (*this += handler);
    }
};

} // namespace SDK::Events

#include "Load.h"
#include "Dash.h"
#include "Stealth.h"
#include "Teleport.h"
#include "Gapcloser.h"
#include "InterruptableSpell.h"
#include "Turret.h"

namespace SDK::Events {

struct LoadEventSlot {
    using Handler = LoadHandler;
    using Fn = bool(*)(Handler);

    Fn Add = nullptr;
    Fn Remove = nullptr;

    bool operator+=(Handler handler) const {
        return Add ? Add(handler) : false;
    }

    bool operator-=(Handler handler) const {
        return Remove ? Remove(handler) : false;
    }

    bool operator()(Handler handler) const {
        return (*this += handler);
    }
};

struct HookEvents {
    EventSlot<CoreHookArgs> OnCoreHook{
        static_cast<bool(*)(CoreHookHandler)>(&AddOnCoreHook),
        static_cast<bool(*)(CoreHookHandler)>(&RemoveOnCoreHook)
    };
    EventSlot<GameUpdateEventArgs> OnUpdate{ &AddOnGameUpdate, &RemoveOnGameUpdate };
    EventSlot<GameUpdateEventArgs> OnGameUpdate{ &AddOnGameUpdate, &RemoveOnGameUpdate };
    EventSlot<ObjectEventArgs> OnCreateObject{ &AddOnCreateObject, &RemoveOnCreateObject };
    EventSlot<ObjectEventArgs> OnDeleteObject{ &AddOnDeleteObject, &RemoveOnDeleteObject };
    EventSlot<ObjectEventArgs> OnMissileCreate{ &AddOnMissileCreate, &RemoveOnMissileCreate };
    EventSlot<ObjectEventArgs> OnMissileDelete{ &AddOnMissileDelete, &RemoveOnMissileDelete };
    EventSlot<BuffEventArgs> OnBuffAdd{ &AddOnBuffAdd, &RemoveOnBuffAdd };
    EventSlot<BuffEventArgs> OnBuffRemove{ &AddOnBuffRemove, &RemoveOnBuffRemove };
    EventSlot<BuffEventArgs> OnBuffUpdate{ &AddOnBuffUpdate, &RemoveOnBuffUpdate };
    EventSlot<NewPathEventArgs> OnNewPath{ &AddOnNewPath, &RemoveOnNewPath };
    EventSlot<IntegerPropertyChangeEventArgs> OnIntegerPropertyChange{
        &AddOnIntegerPropertyChange,
        &RemoveOnIntegerPropertyChange
    };
    EventSlot<TeleportRawEventArgs> OnTeleportRaw{ &AddOnTeleportRaw, &RemoveOnTeleportRaw };
    EventSlot<ProcessSpellEventArgs> OnDoCast{ &AddOnDoCast, &RemoveOnDoCast };
    EventSlot<ProcessSpellEventArgs> OnProcessSpell{ &AddOnProcessSpell, &RemoveOnProcessSpell };
    EventSlot<CastSpellEventArgs> OnProcessCastSpell{ &AddOnProcessCastSpell, &RemoveOnProcessCastSpell };
    EventSlot<ProcessSpellEventArgs> OnFinishCast{ &AddOnFinishCast, &RemoveOnFinishCast };
    EventSlot<ProcessSpellEventArgs> OnSpellImpact{ &AddOnSpellImpact, &RemoveOnSpellImpact };
    EventSlot<PlayAnimationEventArgs> OnPlayAnimation{ &AddOnPlayAnimation, &RemoveOnPlayAnimation };
    EventSlot<StopCastEventArgs> OnStopCast{ &AddOnStopCast, &RemoveOnStopCast };

    LoadEventSlot OnLoad{ &AddOnLoad, &RemoveOnLoad };
    EventSlot<Dash::DashArgs> OnDash{ &AddOnDash, &RemoveOnDash };
    EventSlot<Stealth::OnStealthEventArgs> OnStealth{ &AddOnStealth, &RemoveOnStealth };
    EventSlot<Teleport::TeleportEventArgs> OnTeleport{ &AddOnTeleport, &RemoveOnTeleport };
    EventSlot<Gapcloser::GapCloserEventArgs> OnGapCloser{ &AddOnGapCloser, &RemoveOnGapCloser };
    EventSlot<InterruptableSpell::InterruptableTargetEventArgs> OnInterruptableTarget{
        &AddOnInterruptableTarget,
        &RemoveOnInterruptableTarget
    };
    EventSlot<InterruptableSpell::InterruptableTargetEventArgs> OnInterruptableSpell{
        &AddOnInterruptableTarget,
        &RemoveOnInterruptableTarget
    };
    EventSlot<Turret::TurretArgs> OnTurretAttack{ &AddOnTurretAttack, &RemoveOnTurretAttack };
};

inline HookEvents hook{};

namespace detail {
    inline void ResetDerivedEvents() {
        for (int i = 0; i < LoadHandlerCount; ++i) {
            LoadHandlers[i] = nullptr;
            LoadInvoked[i] = false;
        }
        LoadHandlerCount = 0;

        Dash::detail::DashHandlers.Clear();
        for (int i = 0; i < Dash::detail::DetectedDashCount; ++i) {
            Dash::detail::DetectedDashes[i] = {};
        }
        Dash::detail::DetectedDashCount = 0;

        Stealth::detail::StealthHandlers.Clear();

        Teleport::detail::TeleportHandlers.Clear();
        for (int i = 0; i < Teleport::detail::TeleportDataCount; ++i) {
            Teleport::detail::TeleportData[i] = {};
        }
        Teleport::detail::TeleportDataCount = 0;

        Gapcloser::detail::GapCloserHandlers.Clear();
        for (int i = 0; i < Gapcloser::detail::ActiveSpellCount; ++i) {
            Gapcloser::detail::ActiveSpells[i] = {};
        }
        Gapcloser::detail::ActiveSpellCount = 0;

        InterruptableSpell::detail::InterruptableHandlers.Clear();
        for (int i = 0; i < InterruptableSpell::detail::CastingCount; ++i) {
            InterruptableSpell::detail::Casting[i] = {};
        }
        InterruptableSpell::detail::CastingCount = 0;

        Turret::detail::TurretHandlers.Clear();
        for (int i = 0; i < Turret::detail::TurretCount; ++i) {
            Turret::detail::Turrets[i] = {};
        }
        Turret::detail::TurretCount = 0;
    }
} // namespace detail

} // namespace SDK::Events
