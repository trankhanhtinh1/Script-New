#pragma once

#include "../../Core/CoreBuffs.h"
#include "../../Core/CoreEvents.h"
#include "../../CrashReporter.h"
#include "../../FpsDropDebug.h"
#include "../Data/Database.h"
#include "StructureLifecyclePolicy.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
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
    inline LONG LogEventHandlerException(const char* eventName,
                                         int index,
                                         const void* handler,
                                         EXCEPTION_POINTERS* exceptionPointers) {
        char stage[160] = {};
        _snprintf_s(stage,
                    sizeof(stage),
                    _TRUNCATE,
                    "SDK::Events::%s/handler[%d]",
                    eventName ? eventName : "?",
                    index);
        NightSharpDebug::Logf("[SDK::Events] handler crashed event=%s idx=%d fn=%p; disabling handler",
                              eventName ? eventName : "?",
                              index,
                              handler);
        return NightSharpDebug::CrashReporter::LogAndDumpException(
            stage,
            exceptionPointers);
    }

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

        const char* Name = "";
        Handler Handlers[MaxHandlers] = {};
        int Count = 0;

        EventList() = default;
        explicit EventList(const char* name) : Name(name ? name : "") {}

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

        void Fire(const T& args) {
            if (Count <= 0) {
                return;
            }
            const auto perfStart = NightSharpPerf::Now();
            bool compactHandlers = false;
            for (int i = 0; i < Count; ++i) {
                Handler handler = Handlers[i];
                if (!handler) {
                    continue;
                }
                const auto handlerPerfStart = NightSharpPerf::Now();
                __try {
                    handler(args);
                } __except (LogEventHandlerException(
                                 Name,
                                 i,
                                 reinterpret_cast<const void*>(handler),
                                 GetExceptionInformation())) {
                    Handlers[i] = nullptr;
                    compactHandlers = true;
                }
                NightSharpPerf::AddEventHandlerTiming(
                    Name,
                    i,
                    reinterpret_cast<const void*>(handler),
                    NightSharpPerf::MsSince(handlerPerfStart));
            }
            if (compactHandlers) {
                int write = 0;
                for (int read = 0; read < Count; ++read) {
                    if (Handlers[read]) {
                        Handlers[write++] = Handlers[read];
                    }
                }
                for (int clear = write; clear < Count; ++clear) {
                    Handlers[clear] = nullptr;
                }
                Count = write;
            }
            NightSharpPerf::AddEventTiming(
                Name,
                NightSharpPerf::MsSince(perfStart),
                Count);
        }

        void Clear() {
            for (int i = 0; i < Count; ++i) {
                Handlers[i] = nullptr;
            }
            Count = 0;
        }

        bool HasHandlers() const {
            return Count > 0;
        }
    };

    template <typename T, int MaxEvents>
    struct PendingEventQueue {
        SRWLOCK Lock = SRWLOCK_INIT;
        T Items[MaxEvents] = {};
        int Head = 0;
        int Count = 0;

        void Push(const T& item) {
            AcquireSRWLockExclusive(&Lock);
            if (Count >= MaxEvents) {
                Items[Head] = item;
                Head = (Head + 1) % MaxEvents;
                ReleaseSRWLockExclusive(&Lock);
                return;
            }

            const int tail = (Head + Count) % MaxEvents;
            Items[tail] = item;
            ++Count;
            ReleaseSRWLockExclusive(&Lock);
        }

        bool Pop(T& out) {
            AcquireSRWLockExclusive(&Lock);
            if (Count <= 0) {
                ReleaseSRWLockExclusive(&Lock);
                return false;
            }

            out = Items[Head];
            Items[Head] = {};
            Head = (Head + 1) % MaxEvents;
            --Count;
            if (Count == 0) {
                Head = 0;
            }
            ReleaseSRWLockExclusive(&Lock);
            return true;
        }

        void Clear() {
            AcquireSRWLockExclusive(&Lock);
            for (int i = 0; i < Count; ++i) {
                Items[(Head + i) % MaxEvents] = {};
            }
            Head = 0;
            Count = 0;
            ReleaseSRWLockExclusive(&Lock);
        }
    };

    inline bool Initialized = false;
    inline bool CoreHookRawSubscribed = false;
    inline bool GameUpdateRawSubscribed = false;
    inline bool ObjectCreateRawSubscribed = false;
    inline bool ObjectDeleteRawSubscribed = false;
    inline bool MissileCreateRawSubscribed = false;
    inline bool MissileDeleteRawSubscribed = false;
    inline bool BuffAddRawSubscribed = false;
    inline bool BuffRemoveRawSubscribed = false;
    inline bool BuffUpdateRawSubscribed = false;
    inline bool NewPathRawSubscribed = false;
    inline bool IntegerPropertyChangeRawSubscribed = false;
    inline bool TeleportRawSubscribed = false;
    inline bool DoCastRawSubscribed = false;
    inline bool ProcessSpellRawSubscribed = false;
    inline bool ProcessCastSpellRawSubscribed = false;
    inline bool FinishCastRawSubscribed = false;
    inline bool SpellImpactRawSubscribed = false;
    inline bool PlayAnimationWrapperRawSubscribed = false;
    inline bool PlayAnimationRawSubscribed = false;
    inline bool StopCastRawSubscribed = false;
    inline int DashConsumerRefs = 0;
    inline int StealthConsumerRefs = 0;
    inline int TeleportConsumerRefs = 0;
    inline int GapcloserConsumerRefs = 0;
    inline int InterruptableConsumerRefs = 0;
    inline int TurretConsumerRefs = 0;
    inline std::atomic_bool DeliveryEnabled{ false };

    inline bool CanDeliverRawEvents() {
        return DeliveryEnabled.load(std::memory_order_acquire);
    }

    inline EventList<CoreHookArgs> CoreHookHandlers{ "CoreHook" };
    inline EventList<GameUpdateEventArgs> GameUpdateHandlers{ "GameUpdate" };
    inline EventList<ObjectEventArgs> ObjectCreateHandlers{ "ObjectCreate" };
    inline EventList<ObjectEventArgs> ObjectDeleteHandlers{ "ObjectDelete" };
    inline EventList<ObjectEventArgs> MissileCreateHandlers{ "MissileCreate" };
    inline EventList<ObjectEventArgs> MissileDeleteHandlers{ "MissileDelete" };
    inline EventList<BuffEventArgs> BuffAddHandlers{ "BuffAdd" };
    inline EventList<BuffEventArgs> BuffRemoveHandlers{ "BuffRemove" };
    inline EventList<BuffEventArgs> BuffUpdateHandlers{ "BuffUpdate" };
    inline EventList<NewPathEventArgs> NewPathHandlers{ "NewPath" };
    inline EventList<IntegerPropertyChangeEventArgs> IntegerPropertyChangeHandlers{ "IntegerPropertyChange" };
    inline EventList<TeleportRawEventArgs> TeleportHandlers{ "Teleport" };
    inline EventList<ProcessSpellEventArgs> DoCastHandlers{ "DoCast" };
    inline EventList<ProcessSpellEventArgs> ProcessSpellHandlers{ "ProcessSpell" };
    inline EventList<CastSpellEventArgs> ProcessCastSpellHandlers{ "ProcessCastSpell" };
    inline EventList<ProcessSpellEventArgs> FinishCastHandlers{ "FinishCast" };
    inline EventList<ProcessSpellEventArgs> SpellImpactHandlers{ "SpellImpact" };
    inline EventList<PlayAnimationEventArgs> PlayAnimationHandlers{ "PlayAnimation" };
    inline EventList<StopCastEventArgs> StopCastHandlers{ "StopCast" };

    inline PendingEventQueue<CoreHookArgs, 1024> PendingCoreHooks;
    inline PendingEventQueue<ObjectEventArgs, 512> PendingMissileCreates;
    inline PendingEventQueue<ObjectEventArgs, 512> PendingMissileDeletes;
    inline PendingEventQueue<BuffEventArgs, 512> PendingBuffAdds;
    inline PendingEventQueue<BuffEventArgs, 512> PendingBuffRemoves;
    inline PendingEventQueue<BuffEventArgs, 512> PendingBuffUpdates;
    inline PendingEventQueue<NewPathEventArgs, 256> PendingNewPaths;
    inline PendingEventQueue<IntegerPropertyChangeEventArgs, 256> PendingIntegerPropertyChanges;
    inline PendingEventQueue<TeleportRawEventArgs, 128> PendingTeleports;
    inline PendingEventQueue<ProcessSpellEventArgs, 256> PendingDoCasts;
    inline PendingEventQueue<ProcessSpellEventArgs, 256> PendingProcessSpells;
    inline PendingEventQueue<CastSpellEventArgs, 128> PendingProcessCastSpells;
    inline PendingEventQueue<ProcessSpellEventArgs, 256> PendingFinishCasts;
    inline PendingEventQueue<ProcessSpellEventArgs, 256> PendingSpellImpacts;
    inline PendingEventQueue<PlayAnimationEventArgs, 256> PendingPlayAnimations;
    inline PendingEventQueue<StopCastEventArgs, 128> PendingStopCasts;

    inline constexpr int kMaxObjectLifecycleEventsPerFrame = 32;
    inline constexpr int kMaxQueuedEventsPerFrame = 64;

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
    inline bool HasLoadHandlers();
    inline bool HasDashConsumers() { return DashConsumerRefs > 0; }
    inline bool HasStealthConsumers() { return StealthConsumerRefs > 0; }
    inline bool HasTeleportConsumers() { return TeleportConsumerRefs > 0; }
    inline bool HasGapcloserConsumers() { return GapcloserConsumerRefs > 0; }
    inline bool HasInterruptableConsumers() { return InterruptableConsumerRefs > 0; }
    inline bool HasTurretConsumers() { return TurretConsumerRefs > 0; }
    inline bool HasNewPathConsumers() {
        return NewPathHandlers.HasHandlers() || HasDashConsumers();
    }
    inline bool HasIntegerPropertyChangeConsumers() {
        return IntegerPropertyChangeHandlers.HasHandlers() || HasStealthConsumers();
    }
    inline bool HasTeleportRawConsumers() {
        return TeleportHandlers.HasHandlers() || HasTeleportConsumers();
    }
    inline bool HasDoCastConsumers() {
        return DoCastHandlers.HasHandlers() ||
               HasGapcloserConsumers() ||
               HasInterruptableConsumers() ||
               HasTurretConsumers();
    }
    inline bool HasStopCastConsumers() {
        return StopCastHandlers.HasHandlers() || HasInterruptableConsumers();
    }

    inline void OnRawCoreHook(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !CoreHookHandlers.HasHandlers()) {
            return;
        }
        PendingCoreHooks.Push(raw);
    }

    inline void EnsureCoreHookRawSubscribed() {
        if (CoreHookRawSubscribed) {
            return;
        }

        CoreHookRawSubscribed = true;
        for (int i = 0; i < ::CoreHookTest::HookCount; ++i) {
            ::Core::Events::Add(static_cast<CoreHookId>(i), &OnRawCoreHook);
        }
    }

    inline void ReleaseCoreHookRawSubscribed() {
        if (!CoreHookRawSubscribed) {
            return;
        }

        CoreHookRawSubscribed = false;
        for (int i = 0; i < ::CoreHookTest::HookCount; ++i) {
            ::Core::Events::Remove(static_cast<CoreHookId>(i), &OnRawCoreHook);
        }
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

    inline bool EqualsIgnoreCase(const std::string& left, const char* right) {
        return right && SDK::Data::detail::EqualsIgnoreCase(left, right);
    }

    inline bool IsSpellNameAlias(const SDK::Data::SpellData& data,
                                 const char* value) {
        if (EqualsIgnoreCase(data.spellName, value)) {
            return true;
        }
        for (const auto& extra : data.extraSpellNames) {
            if (EqualsIgnoreCase(extra, value)) {
                return true;
            }
        }
        return false;
    }

    inline const std::string& PreferredMissileName(
        const SDK::Data::SpellData& data) {
        if (!data.missileName.empty()) {
            return data.missileName;
        }
        if (!data.missileSpellName.empty()) {
            return data.missileSpellName;
        }
        if (!data.extraMissileNames.empty()) {
            return data.extraMissileNames.front();
        }
        return data.spellName;
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
        const std::string& missileFallback = PreferredMissileName(*data);
        if (!missileFallback.empty() &&
            (!IsPlausibleIdentifier(args.MissileName) ||
             IsSpellNameAlias(*data, args.MissileName))) {
            CopyText(
                args.MissileName,
                static_cast<int>(sizeof(args.MissileName)),
                missileFallback.c_str());
        }
    }

    inline constexpr int MaxPendingObjectCreates = 512;
    inline constexpr int MaxPendingObjectDeletes = 512;
    inline SRWLOCK PendingObjectLock = SRWLOCK_INIT;
    inline ObjectEventArgs PendingObjectCreates[MaxPendingObjectCreates] = {};
    inline ObjectEventArgs PendingObjectDeletes[MaxPendingObjectDeletes] = {};
    inline int PendingObjectCreateCount = 0;
    inline int PendingObjectDeleteCount = 0;

    inline bool SameObjectIdentity(
        const ObjectEventArgs& pending,
        uintptr_t object,
        uint32_t networkId) {
        if (object != 0 && pending.Sender.Ptr == object) {
            return true;
        }
        return networkId != 0 &&
               networkId != 0xFFFFFFFFu &&
               pending.Sender.NetworkId != 0 &&
               pending.Sender.NetworkId != 0xFFFFFFFFu &&
               pending.Sender.NetworkId == networkId;
    }

    inline void RemovePendingObjectCreateLocked(uintptr_t object, uint32_t networkId) {
        for (int index = 0; index < PendingObjectCreateCount;) {
            if (!SameObjectIdentity(PendingObjectCreates[index], object, networkId)) {
                ++index;
                continue;
            }

            PendingObjectCreates[index] = PendingObjectCreates[--PendingObjectCreateCount];
            PendingObjectCreates[PendingObjectCreateCount] = {};
        }
    }

    inline void QueueObjectCreate(const CoreHookArgs& raw) {
        const ObjectEventArgs args = ::Core::Events::DecodeObjectLifecycleEvent(raw);
        if (!args.Sender.IsValid() ||
            !ShouldQueueObjectLifecycle(args.Sender.Type)) {
            return;
        }

        AcquireSRWLockExclusive(&PendingObjectLock);
        for (int index = 0; index < PendingObjectCreateCount; ++index) {
            if (SameObjectIdentity(
                    PendingObjectCreates[index],
                    args.Sender.Ptr,
                    args.Sender.NetworkId)) {
                PendingObjectCreates[index] = args;
                ReleaseSRWLockExclusive(&PendingObjectLock);
                return;
            }
        }

        if (PendingObjectCreateCount < MaxPendingObjectCreates) {
            PendingObjectCreates[PendingObjectCreateCount++] = args;
        }
        ReleaseSRWLockExclusive(&PendingObjectLock);
    }

    inline void QueueObjectDelete(const CoreHookArgs& raw) {
        const ObjectEventArgs args = ::Core::Events::DecodeObjectLifecycleEvent(raw);
        if (!args.Sender.IsValid() ||
            !ShouldQueueObjectLifecycle(args.Sender.Type)) {
            return;
        }

        AcquireSRWLockExclusive(&PendingObjectLock);
        RemovePendingObjectCreateLocked(args.Sender.Ptr, args.Sender.NetworkId);

        for (int index = 0; index < PendingObjectDeleteCount; ++index) {
            if (SameObjectIdentity(
                    PendingObjectDeletes[index],
                    args.Sender.Ptr,
                    args.Sender.NetworkId)) {
                PendingObjectDeletes[index] = args;
                ReleaseSRWLockExclusive(&PendingObjectLock);
                return;
            }
        }

        if (PendingObjectDeleteCount < MaxPendingObjectDeletes) {
            PendingObjectDeletes[PendingObjectDeleteCount++] = args;
        }
        ReleaseSRWLockExclusive(&PendingObjectLock);
    }

    inline bool PopPendingObjectCreate(ObjectEventArgs& out) {
        AcquireSRWLockExclusive(&PendingObjectLock);
        if (PendingObjectCreateCount <= 0) {
            ReleaseSRWLockExclusive(&PendingObjectLock);
            return false;
        }
        out = PendingObjectCreates[--PendingObjectCreateCount];
        PendingObjectCreates[PendingObjectCreateCount] = {};
        ReleaseSRWLockExclusive(&PendingObjectLock);
        return true;
    }

    inline bool PopPendingObjectDelete(ObjectEventArgs& out) {
        AcquireSRWLockExclusive(&PendingObjectLock);
        if (PendingObjectDeleteCount <= 0) {
            ReleaseSRWLockExclusive(&PendingObjectLock);
            return false;
        }
        out = PendingObjectDeletes[--PendingObjectDeleteCount];
        PendingObjectDeletes[PendingObjectDeleteCount] = {};
        ReleaseSRWLockExclusive(&PendingObjectLock);
        return true;
    }

    inline void FlushObjectLifecycleEvents() {
        ObjectEventArgs args = {};
        for (int i = 0; i < kMaxObjectLifecycleEventsPerFrame &&
                        PopPendingObjectCreate(args); ++i) {
            if (args.Sender.IsValid()) {
                ObjectCreateHandlers.Fire(args);
            }
        }
        for (int i = 0; i < kMaxObjectLifecycleEventsPerFrame &&
                        PopPendingObjectDelete(args); ++i) {
            if (args.Sender.IsValid()) {
                ObjectDeleteHandlers.Fire(args);
            }
        }
    }

    inline void FlushQueuedEvents() {
        CoreHookArgs coreArgs = {};
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingCoreHooks.Pop(coreArgs); ++i) {
            CoreHookHandlers.Fire(coreArgs);
        }

        FlushObjectLifecycleEvents();

        ObjectEventArgs objectArgs = {};
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingMissileCreates.Pop(objectArgs); ++i) {
            MissileCreateHandlers.Fire(objectArgs);
        }
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingMissileDeletes.Pop(objectArgs); ++i) {
            MissileDeleteHandlers.Fire(objectArgs);
        }

        BuffEventArgs buffArgs = {};
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingBuffAdds.Pop(buffArgs); ++i) {
            BuffAddHandlers.Fire(buffArgs);
        }
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingBuffRemoves.Pop(buffArgs); ++i) {
            BuffRemoveHandlers.Fire(buffArgs);
        }
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingBuffUpdates.Pop(buffArgs); ++i) {
            BuffUpdateHandlers.Fire(buffArgs);
        }

        NewPathEventArgs pathArgs = {};
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingNewPaths.Pop(pathArgs); ++i) {
            NewPathHandlers.Fire(pathArgs);
            if (HasDashConsumers()) {
                EventDash(pathArgs);
            }
        }

        IntegerPropertyChangeEventArgs integerArgs = {};
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingIntegerPropertyChanges.Pop(integerArgs); ++i) {
            IntegerPropertyChangeHandlers.Fire(integerArgs);
            if (HasStealthConsumers()) {
                EventStealth(integerArgs);
            }
        }

        TeleportRawEventArgs teleportArgs = {};
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingTeleports.Pop(teleportArgs); ++i) {
            TeleportHandlers.Fire(teleportArgs);
            if (HasTeleportConsumers()) {
                EventTeleport(teleportArgs);
            }
        }

        ProcessSpellEventArgs spellArgs = {};
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingDoCasts.Pop(spellArgs); ++i) {
            DoCastHandlers.Fire(spellArgs);
            if (HasGapcloserConsumers()) {
                EventGapcloser(spellArgs);
            }
            if (HasInterruptableConsumers()) {
                EventInterruptableSpell(spellArgs);
            }
            if (HasTurretConsumers()) {
                EventTurret(spellArgs);
            }
        }
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingProcessSpells.Pop(spellArgs); ++i) {
            ProcessSpellHandlers.Fire(spellArgs);
        }
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingFinishCasts.Pop(spellArgs); ++i) {
            FinishCastHandlers.Fire(spellArgs);
        }
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingSpellImpacts.Pop(spellArgs); ++i) {
            SpellImpactHandlers.Fire(spellArgs);
        }

        CastSpellEventArgs castArgs = {};
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingProcessCastSpells.Pop(castArgs); ++i) {
            ProcessCastSpellHandlers.Fire(castArgs);
        }

        PlayAnimationEventArgs animationArgs = {};
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingPlayAnimations.Pop(animationArgs); ++i) {
            PlayAnimationHandlers.Fire(animationArgs);
        }

        StopCastEventArgs stopArgs = {};
        for (int i = 0; i < kMaxQueuedEventsPerFrame &&
                        PendingStopCasts.Pop(stopArgs); ++i) {
            StopCastHandlers.Fire(stopArgs);
            if (HasInterruptableConsumers()) {
                EventInterruptableSpell(stopArgs);
            }
        }
    }

    inline void OnRawObjectCreate(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !ObjectCreateHandlers.HasHandlers()) {
            return;
        }
        QueueObjectCreate(raw);
    }

    inline void OnRawObjectDelete(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !ObjectDeleteHandlers.HasHandlers()) {
            return;
        }
        QueueObjectDelete(raw);
    }

    inline void OnRawGameUpdate(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents()) {
            return;
        }
        (void)CoreRuntime::RefreshReadState();
        FlushQueuedEvents();

        GameUpdateEventArgs args{};
        args.Raw = raw;
        GameUpdateHandlers.Fire(args);

        EventLoad();
        if (HasGapcloserConsumers()) {
            EventGapcloser();
        }
        if (HasInterruptableConsumers()) {
            EventInterruptableSpell();
        }
    }

    inline void OnRawMissileCreate(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !MissileCreateHandlers.HasHandlers()) {
            return;
        }
        ObjectEventArgs args = ::Core::Events::DecodeMissileEvent(raw);
        CanonicalizeMissileNames(args);
        PendingMissileCreates.Push(args);
    }

    inline void OnRawMissileDelete(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !MissileDeleteHandlers.HasHandlers()) {
            return;
        }
        ObjectEventArgs args = ::Core::Events::DecodeMissileEvent(raw);
        CanonicalizeMissileNames(args);
        PendingMissileDeletes.Push(args);
    }

    inline void OnRawBuffAdd(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents()) {
            return;
        }
        const BuffEventArgs args = ::Core::Events::DecodeBuffEvent(raw);
        CoreBuffs::ApplyBuffAddEvent(args.Sender.Ptr, args.BuffName, args.Count, args.BuffAddress);
        if (BuffAddHandlers.HasHandlers()) {
            PendingBuffAdds.Push(args);
        }
    }

    inline void OnRawBuffRemove(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents()) {
            return;
        }
        const BuffEventArgs args = ::Core::Events::DecodeBuffEvent(raw);
        CoreBuffs::ApplyBuffRemoveEvent(args.Sender.Ptr, args.BuffName, args.BuffAddress);
        if (BuffRemoveHandlers.HasHandlers()) {
            PendingBuffRemoves.Push(args);
        }
    }

    inline void OnRawBuffUpdate(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents()) {
            return;
        }
        const BuffEventArgs args = ::Core::Events::DecodeBuffEvent(raw);
        CoreBuffs::ApplyBuffUpdateEvent(args.Sender.Ptr, args.BuffName, args.Count, args.BuffAddress);
        if (BuffUpdateHandlers.HasHandlers()) {
            PendingBuffUpdates.Push(args);
        }
    }

    inline void OnRawNewPath(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !HasNewPathConsumers()) {
            return;
        }
        const NewPathEventArgs args = ::Core::Events::DecodeNewPath(raw);
        PendingNewPaths.Push(args);
    }

    inline void OnRawIntegerPropertyChange(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !HasIntegerPropertyChangeConsumers()) {
            return;
        }
        const IntegerPropertyChangeEventArgs args = ::Core::Events::DecodeIntegerPropertyChange(raw);
        PendingIntegerPropertyChanges.Push(args);
    }

    inline void OnRawTeleport(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !HasTeleportRawConsumers()) {
            return;
        }
        const TeleportRawEventArgs args = ::Core::Events::DecodeTeleport(raw);
        PendingTeleports.Push(args);
    }

    inline void OnRawDoCast(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !HasDoCastConsumers()) {
            return;
        }
        // OnDoCast (sub_97C290) is a vtable spell-instance method — RCX is a
        // SpellDataInstance, NOT a SpellBookClient. Use DecodeDoCast which
        // resolves the hero through ObjectManager::FindByIndex(*(RCX+0xA8))
        // exactly like the native routine does. The previous code reused
        // DecodeProcessSpell, which interpreted RCX as a spellbook and
        // produced bogus senders / failed IsLocalPlayer checks.
        const ProcessSpellEventArgs args = ::Core::Events::DecodeDoCast(raw);
        PendingDoCasts.Push(args);
    }


    inline void OnRawProcessSpell(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !ProcessSpellHandlers.HasHandlers()) {
            return;
        }
        PendingProcessSpells.Push(::Core::Events::DecodeProcessSpell(raw));
    }

    inline void OnRawProcessCastSpell(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !ProcessCastSpellHandlers.HasHandlers()) {
            return;
        }
        PendingProcessCastSpells.Push(::Core::Events::DecodeProcessCastSpell(raw));
    }

    inline void OnRawFinishCast(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !FinishCastHandlers.HasHandlers()) {
            return;
        }
        PendingFinishCasts.Push(::Core::Events::DecodeProcessSpell(raw));
    }

    inline void OnRawSpellImpact(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !SpellImpactHandlers.HasHandlers()) {
            return;
        }
        PendingSpellImpacts.Push(::Core::Events::DecodeProcessSpell(raw));
    }

    inline void OnRawPlayAnimation(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !PlayAnimationHandlers.HasHandlers()) {
            return;
        }
        PendingPlayAnimations.Push(::Core::Events::DecodePlayAnimation(raw));
    }

    inline void OnRawStopCast(const CoreHookArgs& raw) {
        if (!CanDeliverRawEvents() || !HasStopCastConsumers()) {
            return;
        }
        const StopCastEventArgs args = ::Core::Events::DecodeStopCast(raw);
        PendingStopCasts.Push(args);
    }

    inline bool EnsureRawSubscribed(
        CoreHookId id,
        CoreHookHandler handler,
        bool& subscribed) {
        if (subscribed) {
            return true;
        }
        if (!::Core::Events::Add(id, handler)) {
            return false;
        }
        subscribed = true;
        return true;
    }

    inline bool HasQueuedRawSubscriptions() {
        return CoreHookRawSubscribed ||
               ObjectCreateRawSubscribed ||
               ObjectDeleteRawSubscribed ||
               MissileCreateRawSubscribed ||
               MissileDeleteRawSubscribed ||
               BuffAddRawSubscribed ||
               BuffRemoveRawSubscribed ||
               BuffUpdateRawSubscribed ||
               NewPathRawSubscribed ||
               IntegerPropertyChangeRawSubscribed ||
               TeleportRawSubscribed ||
               DoCastRawSubscribed ||
               ProcessSpellRawSubscribed ||
               ProcessCastSpellRawSubscribed ||
               FinishCastRawSubscribed ||
               SpellImpactRawSubscribed ||
               PlayAnimationWrapperRawSubscribed ||
               PlayAnimationRawSubscribed ||
               StopCastRawSubscribed;
    }

    inline void ReleaseGameUpdateRawIfUnused() {
        if (!GameUpdateRawSubscribed ||
            GameUpdateHandlers.HasHandlers() ||
            HasLoadHandlers() ||
            HasQueuedRawSubscriptions()) {
            return;
        }

        (void)::Core::Events::Remove(Hooks::OnGameUpdate, &OnRawGameUpdate);
        GameUpdateRawSubscribed = false;
    }

    inline bool ReleaseRawSubscribed(
        CoreHookId id,
        CoreHookHandler handler,
        bool& subscribed) {
        if (!subscribed) {
            return true;
        }

        const bool removed = ::Core::Events::Remove(id, handler);
        subscribed = false;
        ReleaseGameUpdateRawIfUnused();
        return removed;
    }

    inline void ReleaseObjectCreateRawIfUnused() {
        if (!ObjectCreateHandlers.HasHandlers()) {
            (void)ReleaseRawSubscribed(
                Hooks::OnCreate,
                &OnRawObjectCreate,
                ObjectCreateRawSubscribed);
        }
    }

    inline void ReleaseObjectDeleteRawIfUnused() {
        if (!ObjectDeleteHandlers.HasHandlers()) {
            (void)ReleaseRawSubscribed(
                Hooks::OnDelete,
                &OnRawObjectDelete,
                ObjectDeleteRawSubscribed);
        }
    }

    inline void ReleaseMissileCreateRawIfUnused() {
        if (!MissileCreateHandlers.HasHandlers()) {
            (void)ReleaseRawSubscribed(
                Hooks::OnMissileCreate,
                &OnRawMissileCreate,
                MissileCreateRawSubscribed);
        }
    }

    inline void ReleaseMissileDeleteRawIfUnused() {
        if (!MissileDeleteHandlers.HasHandlers()) {
            (void)ReleaseRawSubscribed(
                Hooks::OnMissileDelete,
                &OnRawMissileDelete,
                MissileDeleteRawSubscribed);
        }
    }

    inline void ReleaseBuffAddRawIfUnused() {
        if (!BuffAddHandlers.HasHandlers() && !CoreBuffs::IsEventCacheEnabled()) {
            (void)ReleaseRawSubscribed(Hooks::OnBuffAdd, &OnRawBuffAdd, BuffAddRawSubscribed);
        }
    }

    inline void ReleaseBuffRemoveRawIfUnused() {
        if (!BuffRemoveHandlers.HasHandlers() && !CoreBuffs::IsEventCacheEnabled()) {
            (void)ReleaseRawSubscribed(Hooks::OnBuffRemove, &OnRawBuffRemove, BuffRemoveRawSubscribed);
        }
    }

    inline void ReleaseBuffUpdateRawIfUnused() {
        if (!BuffUpdateHandlers.HasHandlers() && !CoreBuffs::IsEventCacheEnabled()) {
            (void)ReleaseRawSubscribed(Hooks::OnBuffUpdate, &OnRawBuffUpdate, BuffUpdateRawSubscribed);
        }
    }

    inline void ReleaseNewPathRawIfUnused() {
        if (!HasNewPathConsumers()) {
            (void)ReleaseRawSubscribed(Hooks::OnNewPath, &OnRawNewPath, NewPathRawSubscribed);
        }
    }

    inline void ReleaseIntegerPropertyChangeRawIfUnused() {
        if (!HasIntegerPropertyChangeConsumers()) {
            (void)ReleaseRawSubscribed(
                Hooks::OnIntegerPropertyChange,
                &OnRawIntegerPropertyChange,
                IntegerPropertyChangeRawSubscribed);
        }
    }

    inline void ReleaseTeleportRawIfUnused() {
        if (!HasTeleportRawConsumers()) {
            (void)ReleaseRawSubscribed(Hooks::OnTeleport, &OnRawTeleport, TeleportRawSubscribed);
        }
    }

    inline void ReleaseDoCastRawIfUnused() {
        if (!HasDoCastConsumers()) {
            (void)ReleaseRawSubscribed(Hooks::OnDoCast, &OnRawDoCast, DoCastRawSubscribed);
        }
    }

    inline void ReleaseProcessSpellRawIfUnused() {
        if (!ProcessSpellHandlers.HasHandlers()) {
            (void)ReleaseRawSubscribed(
                Hooks::OnProcessSpell,
                &OnRawProcessSpell,
                ProcessSpellRawSubscribed);
        }
    }

    inline void ReleaseProcessCastSpellRawIfUnused() {
        if (!ProcessCastSpellHandlers.HasHandlers()) {
            (void)ReleaseRawSubscribed(
                Hooks::ProcessCastSpell,
                &OnRawProcessCastSpell,
                ProcessCastSpellRawSubscribed);
        }
    }

    inline void ReleaseFinishCastRawIfUnused() {
        if (!FinishCastHandlers.HasHandlers()) {
            (void)ReleaseRawSubscribed(Hooks::OnFinishCast, &OnRawFinishCast, FinishCastRawSubscribed);
        }
    }

    inline void ReleaseSpellImpactRawIfUnused() {
        if (!SpellImpactHandlers.HasHandlers()) {
            (void)ReleaseRawSubscribed(Hooks::OnSpellImpact, &OnRawSpellImpact, SpellImpactRawSubscribed);
        }
    }

    inline void ReleasePlayAnimationRawIfUnused() {
        if (PlayAnimationHandlers.HasHandlers()) {
            return;
        }

        (void)ReleaseRawSubscribed(
            Hooks::OnPlayAnimationWrapper,
            &OnRawPlayAnimation,
            PlayAnimationWrapperRawSubscribed);
        (void)ReleaseRawSubscribed(
            Hooks::OnPlayAnimation,
            &OnRawPlayAnimation,
            PlayAnimationRawSubscribed);
    }

    inline void ReleaseStopCastRawIfUnused() {
        if (!HasStopCastConsumers()) {
            (void)ReleaseRawSubscribed(Hooks::OnStopCast, &OnRawStopCast, StopCastRawSubscribed);
        }
    }

    inline bool EnsureGameUpdateRawSubscribed() {
        return EnsureRawSubscribed(Hooks::OnGameUpdate, &OnRawGameUpdate, GameUpdateRawSubscribed);
    }
    inline bool EnsureObjectCreateRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnCreate, &OnRawObjectCreate, ObjectCreateRawSubscribed);
    }
    inline bool EnsureObjectDeleteRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnDelete, &OnRawObjectDelete, ObjectDeleteRawSubscribed);
    }
    inline bool EnsureMissileCreateRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnMissileCreate, &OnRawMissileCreate, MissileCreateRawSubscribed);
    }
    inline bool EnsureMissileDeleteRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnMissileDelete, &OnRawMissileDelete, MissileDeleteRawSubscribed);
    }
    inline bool EnsureBuffAddRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnBuffAdd, &OnRawBuffAdd, BuffAddRawSubscribed);
    }
    inline bool EnsureBuffRemoveRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnBuffRemove, &OnRawBuffRemove, BuffRemoveRawSubscribed);
    }
    inline bool EnsureBuffUpdateRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnBuffUpdate, &OnRawBuffUpdate, BuffUpdateRawSubscribed);
    }

    inline void EnsureBuffEventCacheSubscribed() {
        const bool ok =
            EnsureBuffAddRawSubscribed() &&
            EnsureBuffRemoveRawSubscribed() &&
            EnsureBuffUpdateRawSubscribed();
        CoreBuffs::SetEventCacheEnabled(ok);
    }

    inline bool EnsureNewPathRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnNewPath, &OnRawNewPath, NewPathRawSubscribed);
    }
    inline bool EnsureIntegerPropertyChangeRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(
            Hooks::OnIntegerPropertyChange,
            &OnRawIntegerPropertyChange,
            IntegerPropertyChangeRawSubscribed);
    }
    inline bool EnsureTeleportRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnTeleport, &OnRawTeleport, TeleportRawSubscribed);
    }
    inline bool EnsureDoCastRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnDoCast, &OnRawDoCast, DoCastRawSubscribed);
    }
    inline bool EnsureProcessSpellRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnProcessSpell, &OnRawProcessSpell, ProcessSpellRawSubscribed);
    }
    inline bool EnsureProcessCastSpellRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::ProcessCastSpell, &OnRawProcessCastSpell, ProcessCastSpellRawSubscribed);
    }
    inline bool EnsureFinishCastRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnFinishCast, &OnRawFinishCast, FinishCastRawSubscribed);
    }
    inline bool EnsureSpellImpactRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnSpellImpact, &OnRawSpellImpact, SpellImpactRawSubscribed);
    }
    inline bool EnsurePlayAnimationRawSubscribed() {
        if (!EnsureGameUpdateRawSubscribed()) {
            return false;
        }
        const bool wrapper = EnsureRawSubscribed(
            Hooks::OnPlayAnimationWrapper,
            &OnRawPlayAnimation,
            PlayAnimationWrapperRawSubscribed);
        const bool packet = EnsureRawSubscribed(
            Hooks::OnPlayAnimation,
            &OnRawPlayAnimation,
            PlayAnimationRawSubscribed);
        return wrapper || packet;
    }
    inline bool EnsureStopCastRawSubscribed() {
        return EnsureGameUpdateRawSubscribed() &&
               EnsureRawSubscribed(Hooks::OnStopCast, &OnRawStopCast, StopCastRawSubscribed);
    }
} // namespace detail

inline void SetDeliveryEnabled(bool enabled) {
    detail::DeliveryEnabled.store(enabled, std::memory_order_release);
}

inline bool IsDeliveryEnabled() {
    return detail::CanDeliverRawEvents();
}

inline void Initialize() {
    if (detail::Initialized) {
        ::Core::Events::Initialize();
        detail::EnsureBuffEventCacheSubscribed();
        return;
    }

    detail::Initialized = true;
    ::Core::Events::Initialize();
    detail::EnsureBuffEventCacheSubscribed();
    detail::EventTurretConstruct();
}

inline void Reset() {
    SetDeliveryEnabled(false);
    CoreBuffs::SetEventCacheEnabled(false);
    CoreBuffs::ClearEventCache();
    detail::CoreHookHandlers.Clear();
    detail::GameUpdateHandlers.Clear();
    detail::ObjectCreateHandlers.Clear();
    detail::ObjectDeleteHandlers.Clear();
    AcquireSRWLockExclusive(&detail::PendingObjectLock);
    for (int i = 0; i < detail::PendingObjectCreateCount; ++i) {
        detail::PendingObjectCreates[i] = {};
    }
    detail::PendingObjectCreateCount = 0;
    for (int i = 0; i < detail::PendingObjectDeleteCount; ++i) {
        detail::PendingObjectDeletes[i] = {};
    }
    detail::PendingObjectDeleteCount = 0;
    ReleaseSRWLockExclusive(&detail::PendingObjectLock);
    detail::PendingCoreHooks.Clear();
    detail::PendingMissileCreates.Clear();
    detail::PendingMissileDeletes.Clear();
    detail::PendingBuffAdds.Clear();
    detail::PendingBuffRemoves.Clear();
    detail::PendingBuffUpdates.Clear();
    detail::PendingNewPaths.Clear();
    detail::PendingIntegerPropertyChanges.Clear();
    detail::PendingTeleports.Clear();
    detail::PendingDoCasts.Clear();
    detail::PendingProcessSpells.Clear();
    detail::PendingProcessCastSpells.Clear();
    detail::PendingFinishCasts.Clear();
    detail::PendingSpellImpacts.Clear();
    detail::PendingPlayAnimations.Clear();
    detail::PendingStopCasts.Clear();
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
    detail::CoreHookRawSubscribed = false;
    detail::GameUpdateRawSubscribed = false;
    detail::ObjectCreateRawSubscribed = false;
    detail::ObjectDeleteRawSubscribed = false;
    detail::MissileCreateRawSubscribed = false;
    detail::MissileDeleteRawSubscribed = false;
    detail::BuffAddRawSubscribed = false;
    detail::BuffRemoveRawSubscribed = false;
    detail::BuffUpdateRawSubscribed = false;
    detail::NewPathRawSubscribed = false;
    detail::IntegerPropertyChangeRawSubscribed = false;
    detail::TeleportRawSubscribed = false;
    detail::DoCastRawSubscribed = false;
    detail::ProcessSpellRawSubscribed = false;
    detail::ProcessCastSpellRawSubscribed = false;
    detail::FinishCastRawSubscribed = false;
    detail::SpellImpactRawSubscribed = false;
    detail::PlayAnimationWrapperRawSubscribed = false;
    detail::PlayAnimationRawSubscribed = false;
    detail::StopCastRawSubscribed = false;
    detail::DashConsumerRefs = 0;
    detail::StealthConsumerRefs = 0;
    detail::TeleportConsumerRefs = 0;
    detail::GapcloserConsumerRefs = 0;
    detail::InterruptableConsumerRefs = 0;
    detail::TurretConsumerRefs = 0;
}

inline bool AddOnCoreHook(CoreHookHandler handler) {
    Initialize();
    const bool added = detail::CoreHookHandlers.Add(handler);
    if (added) {
        detail::EnsureCoreHookRawSubscribed();
    }
    return added;
}

inline bool RemoveOnCoreHook(CoreHookHandler handler) {
    const bool removed = detail::CoreHookHandlers.Remove(handler);
    if (removed && !detail::CoreHookHandlers.HasHandlers()) {
        detail::ReleaseCoreHookRawSubscribed();
        detail::ReleaseGameUpdateRawIfUnused();
    }
    return removed;
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

inline bool AddOnGameUpdate(void(*handler)(const GameUpdateEventArgs&)) {
    Initialize();
    return detail::EnsureGameUpdateRawSubscribed() &&
           detail::GameUpdateHandlers.Add(handler);
}
inline bool RemoveOnGameUpdate(void(*handler)(const GameUpdateEventArgs&)) {
    const bool removed = detail::GameUpdateHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseGameUpdateRawIfUnused();
    }
    return removed;
}
inline bool OnGameUpdate(void(*handler)(const GameUpdateEventArgs&)) { return AddOnGameUpdate(handler); }

inline bool AddOnCreateObject(void(*handler)(const ObjectEventArgs&)) {
    Initialize();
    return detail::EnsureObjectCreateRawSubscribed() &&
           detail::ObjectCreateHandlers.Add(handler);
}
inline bool RemoveOnCreateObject(void(*handler)(const ObjectEventArgs&)) {
    const bool removed = detail::ObjectCreateHandlers.Remove(handler);
    if (removed && !detail::ObjectCreateHandlers.HasHandlers()) {
        detail::ReleaseObjectCreateRawIfUnused();
    }
    return removed;
}
inline bool OnCreateObject(void(*handler)(const ObjectEventArgs&)) { return AddOnCreateObject(handler); }

inline bool AddOnDeleteObject(void(*handler)(const ObjectEventArgs&)) {
    Initialize();
    return detail::EnsureObjectDeleteRawSubscribed() &&
           detail::ObjectDeleteHandlers.Add(handler);
}
inline bool RemoveOnDeleteObject(void(*handler)(const ObjectEventArgs&)) {
    const bool removed = detail::ObjectDeleteHandlers.Remove(handler);
    if (removed && !detail::ObjectDeleteHandlers.HasHandlers()) {
        detail::ReleaseObjectDeleteRawIfUnused();
    }
    return removed;
}
inline bool OnDeleteObject(void(*handler)(const ObjectEventArgs&)) { return AddOnDeleteObject(handler); }

inline bool AddOnMissileCreate(void(*handler)(const ObjectEventArgs&)) {
    Initialize();
    return detail::EnsureMissileCreateRawSubscribed() &&
           detail::MissileCreateHandlers.Add(handler);
}
inline bool RemoveOnMissileCreate(void(*handler)(const ObjectEventArgs&)) {
    const bool removed = detail::MissileCreateHandlers.Remove(handler);
    if (removed && !detail::MissileCreateHandlers.HasHandlers()) {
        detail::ReleaseMissileCreateRawIfUnused();
    }
    return removed;
}
inline bool OnMissileCreate(void(*handler)(const ObjectEventArgs&)) { return AddOnMissileCreate(handler); }

inline bool AddOnMissileDelete(void(*handler)(const ObjectEventArgs&)) {
    Initialize();
    return detail::EnsureMissileDeleteRawSubscribed() &&
           detail::MissileDeleteHandlers.Add(handler);
}
inline bool RemoveOnMissileDelete(void(*handler)(const ObjectEventArgs&)) {
    const bool removed = detail::MissileDeleteHandlers.Remove(handler);
    if (removed && !detail::MissileDeleteHandlers.HasHandlers()) {
        detail::ReleaseMissileDeleteRawIfUnused();
    }
    return removed;
}
inline bool OnMissileDelete(void(*handler)(const ObjectEventArgs&)) { return AddOnMissileDelete(handler); }

inline bool AddOnBuffAdd(void(*handler)(const BuffEventArgs&)) {
    Initialize();
    return detail::EnsureBuffAddRawSubscribed() &&
           detail::BuffAddHandlers.Add(handler);
}
inline bool RemoveOnBuffAdd(void(*handler)(const BuffEventArgs&)) {
    const bool removed = detail::BuffAddHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseBuffAddRawIfUnused();
    }
    return removed;
}
inline bool OnBuffAdd(void(*handler)(const BuffEventArgs&)) { return AddOnBuffAdd(handler); }

inline bool AddOnBuffRemove(void(*handler)(const BuffEventArgs&)) {
    Initialize();
    return detail::EnsureBuffRemoveRawSubscribed() &&
           detail::BuffRemoveHandlers.Add(handler);
}
inline bool RemoveOnBuffRemove(void(*handler)(const BuffEventArgs&)) {
    const bool removed = detail::BuffRemoveHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseBuffRemoveRawIfUnused();
    }
    return removed;
}
inline bool OnBuffRemove(void(*handler)(const BuffEventArgs&)) { return AddOnBuffRemove(handler); }

inline bool AddOnBuffUpdate(void(*handler)(const BuffEventArgs&)) {
    Initialize();
    return detail::EnsureBuffUpdateRawSubscribed() &&
           detail::BuffUpdateHandlers.Add(handler);
}
inline bool RemoveOnBuffUpdate(void(*handler)(const BuffEventArgs&)) {
    const bool removed = detail::BuffUpdateHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseBuffUpdateRawIfUnused();
    }
    return removed;
}
inline bool OnBuffUpdate(void(*handler)(const BuffEventArgs&)) { return AddOnBuffUpdate(handler); }

inline bool AddOnNewPath(void(*handler)(const NewPathEventArgs&)) {
    Initialize();
    return detail::EnsureNewPathRawSubscribed() &&
           detail::NewPathHandlers.Add(handler);
}
inline bool RemoveOnNewPath(void(*handler)(const NewPathEventArgs&)) {
    const bool removed = detail::NewPathHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseNewPathRawIfUnused();
    }
    return removed;
}
inline bool OnNewPath(void(*handler)(const NewPathEventArgs&)) { return AddOnNewPath(handler); }

inline bool AddOnIntegerPropertyChange(void(*handler)(const IntegerPropertyChangeEventArgs&)) {
    Initialize();
    return detail::EnsureIntegerPropertyChangeRawSubscribed() &&
           detail::IntegerPropertyChangeHandlers.Add(handler);
}
inline bool RemoveOnIntegerPropertyChange(void(*handler)(const IntegerPropertyChangeEventArgs&)) {
    const bool removed = detail::IntegerPropertyChangeHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseIntegerPropertyChangeRawIfUnused();
    }
    return removed;
}
inline bool OnIntegerPropertyChange(void(*handler)(const IntegerPropertyChangeEventArgs&)) { return AddOnIntegerPropertyChange(handler); }

inline bool AddOnTeleportRaw(void(*handler)(const TeleportRawEventArgs&)) {
    Initialize();
    return detail::EnsureTeleportRawSubscribed() &&
           detail::TeleportHandlers.Add(handler);
}
inline bool RemoveOnTeleportRaw(void(*handler)(const TeleportRawEventArgs&)) {
    const bool removed = detail::TeleportHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseTeleportRawIfUnused();
    }
    return removed;
}
inline bool OnTeleportRaw(void(*handler)(const TeleportRawEventArgs&)) { return AddOnTeleportRaw(handler); }

inline bool AddOnDoCast(void(*handler)(const ProcessSpellEventArgs&)) {
    Initialize();
    return detail::EnsureDoCastRawSubscribed() &&
           detail::DoCastHandlers.Add(handler);
}
inline bool RemoveOnDoCast(void(*handler)(const ProcessSpellEventArgs&)) {
    const bool removed = detail::DoCastHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseDoCastRawIfUnused();
    }
    return removed;
}
inline bool OnDoCast(void(*handler)(const ProcessSpellEventArgs&)) { return AddOnDoCast(handler); }

inline bool AddOnProcessSpell(void(*handler)(const ProcessSpellEventArgs&)) {
    Initialize();
    return detail::EnsureProcessSpellRawSubscribed() &&
           detail::ProcessSpellHandlers.Add(handler);
}
inline bool RemoveOnProcessSpell(void(*handler)(const ProcessSpellEventArgs&)) {
    const bool removed = detail::ProcessSpellHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseProcessSpellRawIfUnused();
    }
    return removed;
}
inline bool OnProcessSpell(void(*handler)(const ProcessSpellEventArgs&)) { return AddOnProcessSpell(handler); }

inline bool AddOnProcessCastSpell(void(*handler)(const CastSpellEventArgs&)) {
    Initialize();
    return detail::EnsureProcessCastSpellRawSubscribed() &&
           detail::ProcessCastSpellHandlers.Add(handler);
}
inline bool RemoveOnProcessCastSpell(void(*handler)(const CastSpellEventArgs&)) {
    const bool removed = detail::ProcessCastSpellHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseProcessCastSpellRawIfUnused();
    }
    return removed;
}
inline bool OnProcessCastSpell(void(*handler)(const CastSpellEventArgs&)) { return AddOnProcessCastSpell(handler); }

inline bool AddOnFinishCast(void(*handler)(const ProcessSpellEventArgs&)) {
    Initialize();
    return detail::EnsureFinishCastRawSubscribed() &&
           detail::FinishCastHandlers.Add(handler);
}
inline bool RemoveOnFinishCast(void(*handler)(const ProcessSpellEventArgs&)) {
    const bool removed = detail::FinishCastHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseFinishCastRawIfUnused();
    }
    return removed;
}
inline bool OnFinishCast(void(*handler)(const ProcessSpellEventArgs&)) { return AddOnFinishCast(handler); }

inline bool AddOnSpellImpact(void(*handler)(const ProcessSpellEventArgs&)) {
    Initialize();
    return detail::EnsureSpellImpactRawSubscribed() &&
           detail::SpellImpactHandlers.Add(handler);
}
inline bool RemoveOnSpellImpact(void(*handler)(const ProcessSpellEventArgs&)) {
    const bool removed = detail::SpellImpactHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseSpellImpactRawIfUnused();
    }
    return removed;
}
inline bool OnSpellImpact(void(*handler)(const ProcessSpellEventArgs&)) { return AddOnSpellImpact(handler); }

inline bool AddOnPlayAnimation(void(*handler)(const PlayAnimationEventArgs&)) {
    Initialize();
    return detail::EnsurePlayAnimationRawSubscribed() &&
           detail::PlayAnimationHandlers.Add(handler);
}
inline bool RemoveOnPlayAnimation(void(*handler)(const PlayAnimationEventArgs&)) {
    const bool removed = detail::PlayAnimationHandlers.Remove(handler);
    if (removed) {
        detail::ReleasePlayAnimationRawIfUnused();
    }
    return removed;
}
inline bool OnPlayAnimation(void(*handler)(const PlayAnimationEventArgs&)) { return AddOnPlayAnimation(handler); }

inline bool AddOnStopCast(void(*handler)(const StopCastEventArgs&)) {
    Initialize();
    return detail::EnsureStopCastRawSubscribed() &&
           detail::StopCastHandlers.Add(handler);
}
inline bool RemoveOnStopCast(void(*handler)(const StopCastEventArgs&)) {
    const bool removed = detail::StopCastHandlers.Remove(handler);
    if (removed) {
        detail::ReleaseStopCastRawIfUnused();
    }
    return removed;
}
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
