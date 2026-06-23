#pragma once

#include "CoreAiManager.h"
#include "CoreObjectManager.h"
#include "CoreRuntime.h"
#include "CoreSpellDataInst.h"
#include "Corehook.h"
#include "Vector.h"
#include "offset.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace Core::Events {

using HookId = ::CoreHookTest::HookId;

namespace Hooks {
    inline constexpr HookId OnIntegerPropertyChange = ::CoreHookTest::OnIntegerPropertyChange;
    inline constexpr HookId ClientMainLoop = ::CoreHookTest::ClientMainLoop;
    inline constexpr HookId CreateClientEffect = ::CoreHookTest::CreateClientEffect;
    inline constexpr HookId DispatchEvent = ::CoreHookTest::DispatchEvent;
    inline constexpr HookId OnTeleport = ::CoreHookTest::OnTeleport;
    inline constexpr HookId Hud_OnDisconnect = ::CoreHookTest::Hud_OnDisconnect;
    inline constexpr HookId OnBuffAdd = ::CoreHookTest::OnBuffAdd;
    inline constexpr HookId OnBuffRemove = ::CoreHookTest::OnBuffRemove;
    inline constexpr HookId OnBuffUpdate = ::CoreHookTest::OnBuffUpdate;
    inline constexpr HookId OnDamage = ::CoreHookTest::OnDamage;
    inline constexpr HookId OnDoCast = ::CoreHookTest::OnDoCast;
    inline constexpr HookId OnFinishCast = ::CoreHookTest::OnFinishCast;
    inline constexpr HookId OnGameUpdate = ::CoreHookTest::OnGameUpdate;
    inline constexpr HookId OnLevelUp = ::CoreHookTest::OnLevelUp;
    inline constexpr HookId OnPlayAnimation = ::CoreHookTest::OnPlayAnimation;
    inline constexpr HookId OnPlayAnimationWrapper = ::CoreHookTest::OnPlayAnimationWrapper;
    inline constexpr HookId OnProcessSpell = ::CoreHookTest::OnProcessSpell;
    inline constexpr HookId OnSpellImpact = ::CoreHookTest::OnSpellImpact;
    inline constexpr HookId OnStopCast = ::CoreHookTest::OnStopCast;
    inline constexpr HookId OnStealth = ::CoreHookTest::OnStealth;
    inline constexpr HookId OnSurrender = ::CoreHookTest::OnSurrender;
    inline constexpr HookId ProcessWorldEvent = ::CoreHookTest::ProcessWorldEvent;
    inline constexpr HookId ProcessCastSpell = ::CoreHookTest::ProcessCastSpell;
    inline constexpr HookId OnUpdateChargeableSpell = ::CoreHookTest::OnUpdateChargeableSpell;
    inline constexpr HookId OnBuffGain = ::CoreHookTest::OnBuffGain;
    inline constexpr HookId OnMissileCreate = ::CoreHookTest::OnMissileCreate;
    inline constexpr HookId OnMissileDelete = ::CoreHookTest::OnMissileDelete;
    inline constexpr HookId OnNewPath = ::CoreHookTest::OnNewPath;
} // namespace Hooks

struct ObjectInfo {
    uintptr_t Ptr = 0;
    uint32_t NetworkId = 0;
    uint32_t Index = 0;
    uint32_t Team = 0;
    ::Core::Objects::ObjectType Type =
        ::Core::Objects::ObjectType::Unknown;
    bool IsDead = false;
    bool IsVisible = false;
    bool IsClone = false;
    bool IsPet = false;
    bool IsZombie = false;
    Vec3 Position = {};
    char Name[64] = {};
    char CharacterName[64] = {};

    bool IsValid() const { return Ptr != 0; }
};

struct RawEventArgs {
    HookId Id = Hooks::OnIntegerPropertyChange;
    uintptr_t Rcx = 0;
    uint64_t Rdx = 0;
    uintptr_t R8 = 0;
    uintptr_t R9 = 0;
    float Xmm0 = 0.0f;
    float Xmm1 = 0.0f;
    float Xmm2 = 0.0f;
    float Xmm3 = 0.0f;
    uintptr_t Stack0 = 0;
    uintptr_t Stack1 = 0;
    uintptr_t Stack2 = 0;
    uintptr_t Stack3 = 0;
    uintptr_t Stack4 = 0;
    uintptr_t Stack5 = 0;
    uintptr_t Target = 0;
    long long HitCount = 0;
    ObjectInfo Object = {};
};

struct GameUpdateEventArgs {
    RawEventArgs Raw = {};
};

struct ObjectEventArgs {
    RawEventArgs Raw = {};
    ObjectInfo Sender = {};
    // Populated for missile events: the AIBaseClient that created the missile.
    ObjectInfo Source = {};
    uint32_t SourceIndex = 0;
    uint32_t SourceNetworkId = 0;
    uint32_t MissileNetworkId = 0;
    Vec3 StartPosition = {};
    Vec3 EndPosition = {};
    Vec3 CastEndPosition = {};
    char SpellName[96] = {};
    char MissileName[96] = {};
};

struct BuffEventArgs {
    RawEventArgs Raw = {};
    ObjectInfo Sender = {};
    uintptr_t EventBridge = 0;
    uintptr_t OwnerComponent = 0;
    int Count = 0;
    char BuffName[96] = {};
};

struct NewPathEventArgs {
    RawEventArgs Raw = {};
    ObjectInfo Sender = {};
    uintptr_t PathArray = 0;
    bool IsDash = false;
    float Speed = 0.0f;
    Vec3 Path[32] = {};
    int PathCount = 0;
};

struct IntegerPropertyChangeEventArgs {
    RawEventArgs Raw = {};
    ObjectInfo Sender = {};
    char Property[64] = {};
    int OldValue = 0;
    int NewValue = 0;
};

struct TeleportEventArgs {
    RawEventArgs Raw = {};
    ObjectInfo Sender = {};
    char RecallType[64] = {};
    char RecallName[64] = {};
};

struct ProcessSpellEventArgs {
    RawEventArgs Raw = {};
    ObjectInfo Sender = {};
    ObjectInfo Target = {};
    uintptr_t Spellbook = 0;
    uintptr_t CastInfo = 0;
    uintptr_t SpellInput = 0;
    uintptr_t SpellData = 0;
    uintptr_t SpellDataResource = 0;
    int Slot = -1;
    int SourceIndex = -1;
    int TargetIndex = -1;
    uint32_t TargetNetworkId = 0;
    uint32_t CasterNetworkId = 0;
    Vec3 StartPosition = {};
    Vec3 EndPosition = {};
    Vec3 CastPosition = {};
    float CastDelay = 0.0f;
    float MissileSpeed = 0.0f;
    bool IsSpell = false;
    bool IsAutoAttack = false;
    bool IsSpecialAttack = false;
    char SpellName[96] = {};
    char MissileName[96] = {};
    char ScriptName[96] = {};
    char SpellSlotName[96] = {};
    char PayloadSpellName[96] = {};
    char PayloadMissileName[96] = {};
};

struct CastSpellEventArgs {
    RawEventArgs Raw = {};
    ObjectInfo Sender = {};
    uintptr_t CastRequest = 0;
    uint32_t TargetNetworkId = 0;
    Vec3 StartPosition = {};
    Vec3 EndPosition = {};
};

struct PlayAnimationEventArgs {
    RawEventArgs Raw = {};
    ObjectInfo Sender = {};
    char Animation[64] = {};
    bool* Process = nullptr;
    int AnimationId = 0;
    bool Accepted = false;
};

struct StopCastEventArgs {
    RawEventArgs Raw = {};
    ObjectInfo Sender = {};
    uintptr_t Spellbook = 0;
    uintptr_t ProcessFlag = 0;
    int Slot = -1;
    uint32_t CasterNetworkId = 0;
    bool ForceStop = false;
};

using RawCallback = void(*)(const RawEventArgs&);

namespace detail {
    inline constexpr int MaxCallbacksPerHook = 32;
    inline RawCallback Callbacks[::CoreHookTest::HookCount][MaxCallbacksPerHook] = {};
    inline int CallbackCounts[::CoreHookTest::HookCount] = {};
    inline ::CoreHookTest::HookCallbackFn PreviousCallback = nullptr;
    inline volatile LONG Initialized = 0;
    inline volatile LONG ShuttingDown = 0;
    inline volatile LONG ActiveDispatches = 0;

    template <typename T>
    inline bool Read(uintptr_t address, T& out) {
        if (!address) {
            out = {};
            return false;
        }

        __try {
            out = *reinterpret_cast<const T*>(address);
            return true;
        } __except (1) {
            out = {};
            return false;
        }
    }

    inline bool CopyAnsi(uintptr_t address, char* out, int outCount) {
        if (!address || !out || outCount <= 0) {
            return false;
        }

        out[0] = 0;
        int i = 0;
        __try {
            const char* src = reinterpret_cast<const char*>(address);
            for (; i + 1 < outCount; ++i) {
                const char ch = src[i];
                out[i] = ch;
                if (ch == 0) {
                    return i > 0;
                }
            }
            out[i] = 0;
            return i > 0;
        } __except (1) {
            out[0] = 0;
            return false;
        }
    }

    inline bool CopyStlString(uintptr_t address, char* out, int outCount) {
        if (!address || !out || outCount <= 0) {
            return false;
        }

        out[0] = 0;

        uintptr_t heap = 0;
        uint64_t size = 0;
        uint64_t capacity = 0;
        Read(address, heap);
        Read(address + 0x10, size);
        Read(address + 0x18, capacity);

        uintptr_t data = address;
        if (capacity >= 16 && capacity < 4096 && heap) {
            data = heap;
        }

        if (size > 0 && size < 4096) {
            const int count = static_cast<int>((size < static_cast<uint64_t>(outCount - 1)) ? size : outCount - 1);
            __try {
                const char* src = reinterpret_cast<const char*>(data);
                for (int i = 0; i < count; ++i) {
                    out[i] = src[i];
                    if (out[i] == 0) {
                        return i > 0;
                    }
                }
                out[count] = 0;
                return count > 0;
            } __except (1) {
                out[0] = 0;
                return false;
            }
        }

        return CopyAnsi(data, out, outCount);
    }

    inline bool CopyRiotString(uintptr_t address, char* out, int outCount) {
        if (!address || !out || outCount <= 0) {
            return false;
        }

        out[0] = 0;

        int size = 0;
        int capacity = 0;
        Read(address + 0x10, size);
        Read(address + 0x14, capacity);
        if (size <= 0 || size >= outCount || capacity < 0 || capacity > 0x10000) {
            return false;
        }

        uintptr_t data = address;
        if (capacity >= 16) {
            Read(address, data);
        }

        return CopyAnsi(data, out, outCount);
    }

    inline bool CopyRuntimeStringField(uintptr_t address, char* out, int outCount) {
        if (!out || outCount <= 0) {
            return false;
        }

        out[0] = 0;
        if (!address) {
            return false;
        }

        if (CopyStlString(address, out, outCount) || CopyRiotString(address, out, outCount)) {
            return true;
        }

        uintptr_t ptr = 0;
        Read(address, ptr);
        if (ptr && CopyAnsi(ptr, out, outCount)) {
            return true;
        }

        return CopyAnsi(address, out, outCount);
    }

    inline bool IsPlausibleIdentifier(const char* value) {
        if (!value || !value[0]) {
            return false;
        }

        int length = 0;
        for (; value[length] && length < 95; ++length) {
            const unsigned char ch = static_cast<unsigned char>(value[length]);
            if (ch < 0x20 || ch > 0x7E) {
                return false;
            }
        }
        return length >= 2 && length < 95;
    }

    inline bool EqualsInsensitive(const char* left, const char* right) {
        if (!left || !right) {
            return false;
        }

        while (*left && *right) {
            char a = *left++;
            char b = *right++;
            if (a >= 'A' && a <= 'Z') {
                a = static_cast<char>(a + ('a' - 'A'));
            }
            if (b >= 'A' && b <= 'Z') {
                b = static_cast<char>(b + ('a' - 'A'));
            }
            if (a != b) {
                return false;
            }
        }
        return *left == 0 && *right == 0;
    }

    inline void CopyText(char* out, int outCount, const char* value) {
        if (!out || outCount <= 0) {
            return;
        }

        out[0] = 0;
        if (!value) {
            return;
        }

        int i = 0;
        for (; i + 1 < outCount && value[i]; ++i) {
            out[i] = value[i];
        }
        out[i] = 0;
    }

    inline bool IsValidAddress(uintptr_t address) {
        return address > 0x10000 && address < 0x7FFFFFFFFFFF;
    }

    inline bool CopyStringView(uintptr_t address, char* out, int outCount) {
        if (!address || !out || outCount <= 0) {
            return false;
        }

        out[0] = 0;
        uintptr_t data = 0;
        uint32_t size = 0;
        Read(address, data);
        Read(address + 0x8, size);
        if (!data || size == 0 || size > 4096) {
            return false;
        }

        const int count =
            size < static_cast<uint32_t>(outCount - 1)
                ? static_cast<int>(size)
                : outCount - 1;
        __try {
            const char* source = reinterpret_cast<const char*>(data);
            for (int i = 0; i < count; ++i) {
                out[i] = source[i];
            }
            out[count] = 0;
            return count > 0;
        } __except (1) {
            out[0] = 0;
            return false;
        }
    }

    inline bool HashLowerStringView(uintptr_t address, uint32_t& outHash) {
        outHash = 0;
        if (!address) {
            return false;
        }

        uintptr_t data = 0;
        uint32_t size = 0;
        Read(address, data);
        Read(address + 0x8, size);
        if (!data || size == 0 || size > 4096) {
            return false;
        }

        __try {
            uint32_t hash = 0x811C9DC5u;
            const auto* source = reinterpret_cast<const unsigned char*>(data);
            for (uint32_t i = 0; i < size; ++i) {
                unsigned char ch = source[i];
                if (ch >= 'A' && ch <= 'Z') {
                    ch = static_cast<unsigned char>(ch + ('a' - 'A'));
                }
                hash = (hash ^ ch) * 0x01000193u;
            }
            outHash = hash;
            return true;
        } __except (1) {
            outHash = 0;
            return false;
        }
    }

    inline ObjectInfo ReadObject(uintptr_t object) {
        ObjectInfo info{};
        info.Ptr = object;
        if (!object) {
            return info;
        }

        Read(object + Offset::All::NetId, info.NetworkId);
        Read(object + Offset::All::Index, info.Index);
        Read(object + Offset::All::Team, info.Team);
        Read(object + Offset::All::Dead, info.IsDead);
        Read(object + Offset::All::Visible, info.IsVisible);
        Read(object + Offset::All::Position, info.Position);
        CopyRuntimeStringField(
            object + Offset::All::Name,
            info.Name,
            static_cast<int>(sizeof(info.Name)));
        CopyRuntimeStringField(
            object + Offset::All::CharacterName,
            info.CharacterName,
            static_cast<int>(sizeof(info.CharacterName)));
        return info;
    }

    inline bool LooksLikeObject(uintptr_t object) {
        const ObjectInfo info = ReadObject(object);
        if (!info.IsValid() || info.NetworkId == 0 || info.NetworkId == 0xFFFFFFFFu) {
            return false;
        }

        return info.Position.IsValid();
    }

    inline ObjectInfo ResolveObjectByNetworkId(uint32_t networkId) {
        if (networkId == 0 || networkId == 0xFFFFFFFFu) {
            return {};
        }

        const uintptr_t object = ::Core::ObjectManager::FindByNetworkId(networkId);
        return LooksLikeObject(object) ? ReadObject(object) : ObjectInfo{};
    }

    inline ObjectInfo ResolveObjectByIndex(int index) {
        if (index <= 0) {
            return {};
        }

        const uintptr_t object =
            ::Core::ObjectManager::FindByIndex(static_cast<uint32_t>(index));
        return LooksLikeObject(object) ? ReadObject(object) : ObjectInfo{};
    }

    struct BuffBridgeOwnerEntry {
        uintptr_t Bridge = 0;
        uintptr_t Owner = 0;
    };

    inline constexpr int MaxBuffBridgeOwners = 128;
    inline BuffBridgeOwnerEntry BuffBridgeOwners[MaxBuffBridgeOwners] = {};
    inline int BuffBridgeOwnerCursor = 0;

    inline void RememberBuffBridgeOwner(uintptr_t bridge, uintptr_t owner) {
        if (!bridge || !LooksLikeObject(owner)) {
            return;
        }

        for (auto& entry : BuffBridgeOwners) {
            if (entry.Bridge == bridge) {
                entry.Owner = owner;
                return;
            }
        }

        auto& entry =
            BuffBridgeOwners[BuffBridgeOwnerCursor++ % MaxBuffBridgeOwners];
        entry.Bridge = bridge;
        entry.Owner = owner;
    }

    inline ObjectInfo ResolveBuffBridgeOwner(uintptr_t bridge) {
        if (!bridge) {
            return {};
        }

        for (const auto& entry : BuffBridgeOwners) {
            if (entry.Bridge != bridge || !LooksLikeObject(entry.Owner)) {
                continue;
            }
            return ReadObject(entry.Owner);
        }
        return {};
    }

    inline ObjectInfo ReadSpellbookOwner(uintptr_t spellbook) {
        uint32_t casterNetworkId = 0;
        Read(spellbook + Offset::SpellBookLayout::CasterNetId, casterNetworkId);
        ObjectInfo owner = ResolveObjectByNetworkId(casterNetworkId);
        if (owner.IsValid()) {
            return owner;
        }

        uintptr_t ownerAddress = 0;
        Read(spellbook + Offset::SpellBookLayout::Owner, ownerAddress);
        return LooksLikeObject(ownerAddress) ? ReadObject(ownerAddress) : ObjectInfo{};
    }

    inline bool IsSlotValid(int slot) {
        return (slot >= 0 && slot <= 13) || slot == 64;
    }

    inline int ReadEventSlot(uintptr_t castInfo) {
        int slot = -1;
        Read(castInfo + Offset::SpellCastInfoEventLayout::Slot, slot);
        if (IsSlotValid(slot)) {
            return slot;
        }

        Read(castInfo + Offset::SpellCastInfoEventLayout::SlotAlt, slot);
        if (IsSlotValid(slot)) {
            return slot;
        }

        uint8_t activeSlot = 0xFF;
        Read(castInfo + Offset::SpellCastInfoLayout::SpellSlot, activeSlot);
        return IsSlotValid(static_cast<int>(activeSlot)) ? static_cast<int>(activeSlot) : -1;
    }

    inline bool ReadBoolByte(uintptr_t address) {
        uint8_t value = 0;
        Read(address, value);
        return value != 0;
    }

    inline bool ReadVector3(uintptr_t address, Vec3& out) {
        Vec3 native{};
        if (!Read(address, native)) {
            out = {};
            return false;
        }

        out = native;
        return out.IsValid();
    }

    inline bool IsPlausibleWorldPosition(const Vec3& value) {
        return value.IsValid() &&
               std::fabs(value.x) <= 30000.0f &&
               std::fabs(value.y) <= 5000.0f &&
               std::fabs(value.z) <= 30000.0f &&
               (std::fabs(value.x) > 1.0f || std::fabs(value.z) > 1.0f);
    }

    inline bool IsValidCastLine(const Vec3& start, const Vec3& end) {
        return IsPlausibleWorldPosition(start) &&
               IsPlausibleWorldPosition(end) &&
               start.Distance2D(end) >= 5.0f;
    }

    inline bool ReadActiveCastLine(uintptr_t cast, int expectedSlot,
                                   Vec3& start, Vec3& end) {
        start = {};
        end = {};
        if (!IsValidAddress(cast) || !IsSlotValid(expectedSlot)) {
            return false;
        }

        int slot = -1;
        Read(cast + Offset::SpellCastInfoLayout::SpellSlot, slot);
        if (slot != expectedSlot) {
            uint8_t slotByte = 0xFF;
            Read(cast + Offset::SpellCastInfoLayout::SpellSlot, slotByte);
            if (static_cast<int>(slotByte) != expectedSlot) {
                return false;
            }
        }

        ReadVector3(cast + Offset::SpellCastInfoLayout::StartPosition, start);
        ReadVector3(cast + Offset::SpellCastInfoLayout::EndPosition, end);
        return IsValidCastLine(start, end);
    }

    inline void ResolveActiveCastPositions(ProcessSpellEventArgs& args) {
        if (!IsSlotValid(args.Slot) || args.Slot == 64) {
            return;
        }

        Vec3 start = {};
        Vec3 end = {};
        uintptr_t activeCast = 0;
        if (args.Spellbook) {
            Read(args.Spellbook + Offset::SpellRuntime::ActiveSpellCast, activeCast);
            if (ReadActiveCastLine(activeCast, args.Slot, start, end)) {
                args.StartPosition = start;
                args.EndPosition = end;
                args.CastPosition = end;
                return;
            }
        }

        if (!args.Sender.IsValid()) {
            return;
        }

        Read(
            args.Sender.Ptr +
                Offset::SpellRuntime::SpellBookOffset +
                Offset::SpellRuntime::ActiveSpellCast,
            activeCast);
        if (ReadActiveCastLine(activeCast, args.Slot, start, end)) {
            args.StartPosition = start;
            args.EndPosition = end;
            args.CastPosition = end;
            return;
        }

        const auto ref = ::CoreSpellDataInst::Resolve(args.Sender.Ptr, args.Slot);
        if (!ref.IsValid()) {
            return;
        }

        Read(ref.slot + Offset::SpellSlotLayout::SlotActiveSpellCast, activeCast);
        if (ReadActiveCastLine(activeCast, args.Slot, start, end)) {
            args.StartPosition = start;
            args.EndPosition = end;
            args.CastPosition = end;
        }
    }

    inline int ReadPath(uintptr_t pathArray, Vec3* out, int maxCount) {
        if (!pathArray || !out || maxCount <= 0) {
            return 0;
        }

        uintptr_t data = 0;
        uint32_t count = 0;
        Read(pathArray, data);
        Read(pathArray + 0x8, count);

        if (count == 0 || count > 256) {
            Read(pathArray + 0x4, count);
        }
        if (!data || count == 0 || count > 256) {
            return 0;
        }

        const int clipped = count < static_cast<uint32_t>(maxCount)
            ? static_cast<int>(count)
            : maxCount;

        int copied = 0;
        for (int i = 0; i < clipped; ++i) {
            Vec3 point{};
            if (!ReadVector3(data + static_cast<uintptr_t>(i) * sizeof(Vec3), point)) {
                break;
            }
            out[copied++] = point;
        }
        return copied;
    }

    inline RawEventArgs MakeRaw(HookId id,
                                uintptr_t rcx,
                                uint64_t rdx,
                                uintptr_t r8,
                                uintptr_t r9,
                                float xmm0,
                                float xmm1,
                                float xmm2,
                                float xmm3,
                                uintptr_t stack0,
                                uintptr_t stack1,
                                uintptr_t stack2,
                                uintptr_t stack3,
                                uintptr_t stack4,
                                uintptr_t stack5) {
        RawEventArgs args{};
        args.Id = id;
        args.Rcx = rcx;
        args.Rdx = rdx;
        args.R8 = r8;
        args.R9 = r9;
        args.Xmm0 = xmm0;
        args.Xmm1 = xmm1;
        args.Xmm2 = xmm2;
        args.Xmm3 = xmm3;
        args.Stack0 = stack0;
        args.Stack1 = stack1;
        args.Stack2 = stack2;
        args.Stack3 = stack3;
        args.Stack4 = stack4;
        args.Stack5 = stack5;
        args.Target = ::CoreHookTest::TargetAddress(id);
        args.HitCount = ::CoreHookTest::HitCount(id);

        args.Object = ReadObject(rcx);
        return args;
    }

    inline void FireCallbacks(const RawEventArgs& args) {
        const int id = static_cast<int>(args.Id);
        if (id < 0 || id >= ::CoreHookTest::HookCount) {
            return;
        }

        const int count = CallbackCounts[id];
        for (int i = 0; i < count; ++i) {
            RawCallback callback = Callbacks[id][i];
            if (!callback) {
                continue;
            }

            __try {
                callback(args);
            } __except (1) {}
        }
    }

    inline void Fire(const RawEventArgs& args) {
        FireCallbacks(args);
    }

    // Shared CastInfo decoder used by every spell-style hook (OnDoCast,
    // OnProcessSpell, OnFinishCast, OnSpellImpact). Fills SpellData/Slot/
    // positions/names but leaves Sender/Spellbook to the caller because
    // those are hook-specific. See PlayerEvent_Hooks_IDA_Analysis.md for
    // why each hook resolves its sender differently.
    inline void FillCastInfoFields(uintptr_t castInfo, ProcessSpellEventArgs& args) {
        if (!castInfo) {
            return;
        }
        args.CastInfo = castInfo;
        args.Slot = ReadEventSlot(castInfo);
        Read(castInfo + Offset::SpellCastInfoEventLayout::SpellData, args.SpellInput);
        args.SpellData = args.SpellInput;
        Read(castInfo + Offset::SpellCastInfoEventLayout::SrcIndex, args.SourceIndex);
        Read(castInfo + Offset::SpellCastInfoEventLayout::TargetIndex, args.TargetIndex);
        ReadVector3(
            castInfo + Offset::SpellCastInfoEventLayout::StartPos,
            args.StartPosition);
        ReadVector3(
            castInfo + Offset::SpellCastInfoEventLayout::EndPos,
            args.EndPosition);
        ReadVector3(
            castInfo + Offset::SpellCastInfoEventLayout::CastPos,
            args.CastPosition);
        Read(castInfo + Offset::SpellCastInfoEventLayout::CastDelay, args.CastDelay);
        args.IsSpell = ReadBoolByte(castInfo + Offset::SpellCastInfoEventLayout::IsSpell);
        args.IsSpecialAttack =
            ReadBoolByte(castInfo + Offset::SpellCastInfoEventLayout::IsSpecialAttack) ||
            ReadBoolByte(castInfo + Offset::SpellCastInfoEventLayout::IsSpecialAttackAlt);
        args.IsAutoAttack =
            args.Slot == 64 ||
            ReadBoolByte(castInfo + Offset::SpellCastInfoEventLayout::IsAuto) ||
            ReadBoolByte(castInfo + Offset::SpellCastInfoEventLayout::IsAutoAlt);

        if (args.SpellInput) {
            uintptr_t innerData = 0;
            Read(args.SpellInput + Offset::SpellInfoLayout::InfoSpellData, innerData);
            if (IsValidAddress(innerData)) {
                args.SpellData = innerData;
            }
            Read(
                args.SpellInput + Offset::SpellDataLayout::DataResource,
                args.SpellDataResource);
            char inputSpellName[sizeof(args.SpellName)] = {};
            if (CopyRuntimeStringField(
                    args.SpellInput + Offset::SpellInputLayout::SpellNameKey,
                    inputSpellName,
                    static_cast<int>(sizeof(inputSpellName))) &&
                IsPlausibleIdentifier(inputSpellName)) {
                CopyText(
                    args.SpellName,
                    static_cast<int>(sizeof(args.SpellName)),
                    inputSpellName);
            }

            if (args.SpellDataResource) {
                Read(
                    args.SpellDataResource + Offset::SpellDataResourceLayout::ResMissileSpeed,
                    args.MissileSpeed);
                CopyRuntimeStringField(
                    args.SpellDataResource + Offset::SpellDataResourceLayout::ResScriptName,
                    args.ScriptName,
                    static_cast<int>(sizeof(args.ScriptName)));
            }
        }

        // The missile/cast payload copied into MissileClient+0x2C0 still keeps
        // two legacy runtime strings. On 26.6 these are not canonical names
        // (`+0x20` resolves to "Ezreal" for Ezreal Q), so expose them for debug
        // but do not let them overwrite the SpellBook-derived name.
        CopyRuntimeStringField(
            castInfo + Offset::MissileEventLayout::SpellName,
            args.PayloadSpellName,
            static_cast<int>(sizeof(args.PayloadSpellName)));
        CopyRuntimeStringField(
            castInfo + Offset::MissileEventLayout::MissileName,
            args.PayloadMissileName,
            static_cast<int>(sizeof(args.PayloadMissileName)));

        if (args.TargetIndex > 0) {
            args.Target = ResolveObjectByIndex(args.TargetIndex);
            if (args.Target.IsValid()) {
                args.TargetNetworkId = args.Target.NetworkId;
            }
        }
        if (args.TargetNetworkId == 0 && args.TargetIndex >= 0) {
            args.TargetNetworkId = static_cast<uint32_t>(args.TargetIndex);
        }
    }

    inline void ResolveSpellBookFields(ProcessSpellEventArgs& args) {
        if (!args.Sender.IsValid() || !IsSlotValid(args.Slot) || args.Slot == 64) {
            return;
        }

        const auto ref = ::CoreSpellDataInst::Resolve(args.Sender.Ptr, args.Slot);
        if (!ref.IsValid()) {
            return;
        }

        const uintptr_t slotInput = ::CoreSpellDataInst::SpellInput(ref);
        char slotName[sizeof(args.SpellSlotName)] = {};
        if (IsValidAddress(slotInput) &&
            CopyRuntimeStringField(
                slotInput + Offset::SpellInputLayout::SpellNameKey,
                slotName,
                static_cast<int>(sizeof(slotName))) &&
            IsPlausibleIdentifier(slotName)) {
            CopyText(
                args.SpellSlotName,
                static_cast<int>(sizeof(args.SpellSlotName)),
                slotName);
        } else if (::CoreSpellDataInst::ReadName(
                       ref,
                       slotName,
                       static_cast<int>(sizeof(slotName))) &&
                   IsPlausibleIdentifier(slotName)) {
            CopyText(
                args.SpellSlotName,
                static_cast<int>(sizeof(args.SpellSlotName)),
                slotName);
        }

        if (IsPlausibleIdentifier(args.SpellSlotName)) {
            const bool missingName = !args.SpellName[0];
            const bool invalidName = !IsPlausibleIdentifier(args.SpellName);
            const bool championName =
                EqualsInsensitive(args.SpellName, args.Sender.CharacterName) ||
                EqualsInsensitive(args.SpellName, args.Sender.Name);
            if (missingName || invalidName || championName) {
                CopyText(
                    args.SpellName,
                    static_cast<int>(sizeof(args.SpellName)),
                    args.SpellSlotName);
            }
        }

        if (IsValidAddress(slotInput)) {
            args.SpellInput = slotInput;
            uintptr_t innerData = 0;
            Read(slotInput + Offset::SpellInfoLayout::InfoSpellData, innerData);
            if (IsValidAddress(innerData)) {
                args.SpellData = innerData;
            }
            uintptr_t resource = 0;
            Read(slotInput + Offset::SpellDataLayout::DataResource, resource);
            if (IsValidAddress(resource)) {
                args.SpellDataResource = resource;
            }
        }
    }
} // namespace detail


inline NewPathEventArgs DecodeNewPath(const RawEventArgs& raw) {
    NewPathEventArgs args{};
    args.Raw = raw;
    args.Sender = raw.Object;
    args.PathArray = static_cast<uintptr_t>(raw.Rdx);
    // IDA 13337: sub_562E60(AIBaseClient*, pathArray, count, payload,
    //                      isDash, flags, ...)
    // RDX is the native path-array wrapper, R8D is the path count, R9 is the
    // path-state payload, and stack0 is the dash flag. The hook fires before
    // the native apply call, so dash speed is decoded from payload+0x2C.
    args.IsDash = (raw.Stack0 & 0xFF) != 0;
    const int explicitCount =
        raw.R8 > 0 && raw.R8 <= ::CoreAiManager::kMaxEventWaypoints
            ? static_cast<int>(raw.R8)
            : 0;
    args.Speed = ::CoreAiManager::DecodeNewPathSpeed(
        raw.Rcx, static_cast<uintptr_t>(raw.R9), args.IsDash);
    args.PathCount = ::CoreAiManager::CopyNativePathArray(
        args.PathArray, explicitCount, args.Path, 32);
    return args;
}

inline BuffEventArgs DecodeBuffEvent(const RawEventArgs& raw) {
    // sub_223920 / sub_223A40:
    //   RDX = per-unit event bridge
    //   R8  = buff-name string-view
    //   R9  = owner + 0x2B0
    //
    // sub_2234A0:
    //   RDX = same per-unit event bridge
    //   R8  = buff-name string-view
    //   R9  = current stack/count value
    BuffEventArgs args{};
    args.Raw = raw;
    args.EventBridge = static_cast<uintptr_t>(raw.Rdx);

    detail::CopyStringView(
        raw.R8,
        args.BuffName,
        static_cast<int>(sizeof(args.BuffName)));

    if (raw.Id == Hooks::OnBuffAdd || raw.Id == Hooks::OnBuffRemove) {
        args.OwnerComponent = raw.R9;
        if (raw.R9 > Offset::BuffEventLayout::OwnerComponent) {
            const uintptr_t owner =
                raw.R9 - Offset::BuffEventLayout::OwnerComponent;
            if (detail::LooksLikeObject(owner)) {
                args.Sender = detail::ReadObject(owner);
                detail::RememberBuffBridgeOwner(args.EventBridge, owner);
            }
        }
    } else if (raw.Id == Hooks::OnBuffUpdate) {
        args.Count = static_cast<int>(raw.R9);
        args.Sender = detail::ResolveBuffBridgeOwner(args.EventBridge);
    }

    return args;
}

inline ObjectEventArgs DecodeMissileEvent(const RawEventArgs& raw) {
    // OnMissileCreate hooks the init routine before the packet has been
    // copied into MissileClient+0x2C0, so resolve the caster from RDX+0xA0.
    // OnMissileDelete runs at destructor entry after that copy and resolves
    // the same source index from MissileClient+0x360.
    ObjectEventArgs args{};
    args.Raw = raw;
    args.Sender = detail::ReadObject(raw.Rcx);

    uintptr_t payload = 0;
    if (raw.Id == Hooks::OnMissileCreate) {
        payload = static_cast<uintptr_t>(raw.Rdx);
        detail::Read(
            payload +
                Offset::MissileEventLayout::CreatePacketCasterIndex,
            args.SourceIndex);
        detail::Read(
            payload + Offset::MissileEventLayout::CreatePacketMissileNetId,
            args.MissileNetworkId);
    } else if (raw.Id == Hooks::OnMissileDelete) {
        payload = raw.Rcx + Offset::MissileClient::CastInfoBase;
        detail::Read(
            raw.Rcx + Offset::MissileClient::CasterIndex,
            args.SourceIndex);
        detail::Read(
            raw.Rcx + Offset::MissileClient::ObjectNetId,
            args.MissileNetworkId);
        if (args.MissileNetworkId == 0 ||
            args.MissileNetworkId == 0xFFFFFFFFu) {
            detail::Read(
                raw.Rcx + Offset::MissileClient::MissileNetId,
                args.MissileNetworkId);
        }
    }

    if (args.MissileNetworkId == 0 ||
        args.MissileNetworkId == 0xFFFFFFFFu) {
        args.MissileNetworkId = args.Sender.NetworkId;
    }

    if (payload) {
        detail::CopyRuntimeStringField(
            payload + Offset::MissileEventLayout::SpellName,
            args.SpellName,
            static_cast<int>(sizeof(args.SpellName)));
        detail::CopyRuntimeStringField(
            payload + Offset::MissileEventLayout::MissileName,
            args.MissileName,
            static_cast<int>(sizeof(args.MissileName)));
        detail::ReadVector3(
            payload + Offset::MissileEventLayout::StartPos,
            args.StartPosition);
        detail::ReadVector3(
            payload + Offset::MissileEventLayout::EndPos,
            args.EndPosition);
        detail::ReadVector3(
            payload + Offset::MissileEventLayout::CastEndPos,
            args.CastEndPosition);
    }

    args.Source =
        detail::ResolveObjectByIndex(static_cast<int>(args.SourceIndex));
    if (!args.Source.IsValid()) {
        args.Source =
            detail::ResolveObjectByNetworkId(args.SourceIndex);
    }
    if (args.Source.IsValid()) {
        args.SourceNetworkId = args.Source.NetworkId;
    }

    if (!args.MissileName[0]) {
        if (detail::IsPlausibleIdentifier(args.Sender.Name)) {
            detail::CopyText(
                args.MissileName,
                static_cast<int>(sizeof(args.MissileName)),
                args.Sender.Name);
        } else if (detail::IsPlausibleIdentifier(args.Sender.CharacterName)) {
            detail::CopyText(
                args.MissileName,
                static_cast<int>(sizeof(args.MissileName)),
                args.Sender.CharacterName);
        }
    }
    if ((!args.SpellName[0] ||
         (args.Source.IsValid() &&
          detail::EqualsInsensitive(args.SpellName, args.Source.CharacterName))) &&
        detail::IsPlausibleIdentifier(args.MissileName)) {
        detail::CopyText(
            args.SpellName,
            static_cast<int>(sizeof(args.SpellName)),
            args.MissileName);
    }
    return args;
}

inline IntegerPropertyChangeEventArgs DecodeIntegerPropertyChange(const RawEventArgs& raw) {
    IntegerPropertyChangeEventArgs args{};
    args.Raw = raw;
    args.Sender = raw.Object;
    args.OldValue = static_cast<int>(raw.R8);
    args.NewValue = static_cast<int>(raw.R9);
    detail::CopyAnsi(static_cast<uintptr_t>(raw.Rdx), args.Property, static_cast<int>(sizeof(args.Property)));
    return args;
}

inline TeleportEventArgs DecodeTeleport(const RawEventArgs& raw) {
    TeleportEventArgs args{};
    args.Raw = raw;
    args.Sender = raw.Object;
    // TODO(EnsoulSharp parity): verify the current FOWRecall payload for every
    // teleport source, including newer map gates/portal style recalls. The bridge
    // currently copies RecallType/RecallName only.
    detail::CopyStlString(static_cast<uintptr_t>(raw.Rdx), args.RecallType, static_cast<int>(sizeof(args.RecallType)));
    detail::CopyStlString(raw.R8, args.RecallName, static_cast<int>(sizeof(args.RecallName)));
    return args;
}

inline ProcessSpellEventArgs DecodeProcessSpell(const RawEventArgs& raw) {
    // OnProcessSpell (sub_982120).
    //
    // VERIFIED June/2026 from PlayerEventFilter live logs:
    //   - RCX is a SpellBookClient controller. For the player path,
    //     RCX == `Player() + 0x3128`. Confirmed: player at 0x20AB845E010
    //     produced RCX = 0x20AB8461138 (delta = 0x3128) in 4 separate hits.
    //   - Other RCX values correspond to enemy heroes / minions / turrets.
    //   - The function body (sub_982120) does NOT read any NetworkId field
    //     from RCX itself — it dereferences `*(QWORD*)(RCX + 0xAD8)` for
    //     internal state, then uses fields in CastInfo (RDX) for slot/flags.
    //     So no offset on RCX directly stores the caster NetId.
    //
    // Strategy: identify the owning hero by comparing RCX to every live
    // hero pointer + 0x3128. The first match wins. This is O(N) over
    // heroes (≤10 in a normal game) per hook invocation and is the only
    // way that survives reset across builds.
    ProcessSpellEventArgs args{};
    args.Raw = raw;
    args.Spellbook = raw.Rcx;
    args.CastInfo = static_cast<uintptr_t>(raw.Rdx);

    if (args.CastInfo) {
        detail::FillCastInfoFields(args.CastInfo, args);
    }

    // 1) Primary: RCX - 0x3128 should be a hero pointer. Validate by reading
    //    NetId from that candidate and resolving through ObjectManager.
    if (raw.Rcx > 0x3128) {
        const uintptr_t candidateHero = raw.Rcx - 0x3128;
        if (detail::LooksLikeObject(candidateHero)) {
            args.Sender = detail::ReadObject(candidateHero);
        }
    }

    // 2) Fallback: SpellBook owner pointer (+0x08) — works on some builds.
    if (!args.Sender.IsValid() && raw.Rcx) {
        uintptr_t ownerPtr = 0;
        detail::Read(raw.Rcx + Offset::SpellBookLayout::Owner, ownerPtr);
        if (detail::LooksLikeObject(ownerPtr)) {
            args.Sender = detail::ReadObject(ownerPtr);
        }
    }

    // 3) Fallback: probe known NetId offsets on RCX.
    if (!args.Sender.IsValid() && raw.Rcx) {
        constexpr uint32_t kOffsets[] = { 0xA8, 0x98, 0x2DC };
        for (uint32_t off : kOffsets) {
            uint32_t netId = 0;
            detail::Read(raw.Rcx + off, netId);
            if (netId == 0 || netId == 0xFFFFFFFFu) {
                continue;
            }
            ObjectInfo candidate = detail::ResolveObjectByNetworkId(netId);
            if (!candidate.IsValid()) {
                candidate = detail::ResolveObjectByIndex(static_cast<int>(netId));
            }
            if (candidate.IsValid()) {
                args.Sender = candidate;
                break;
            }
        }
    }

    // 4) Last resort: CastInfo.SrcIndex.
    if (!args.Sender.IsValid() && args.SourceIndex > 0) {
        args.Sender = detail::ResolveObjectByIndex(args.SourceIndex);
    }

    if (args.Sender.IsValid()) {
        args.CasterNetworkId = args.Sender.NetworkId;
    } else if (args.Spellbook) {
        detail::Read(args.Spellbook + Offset::SpellBookLayout::CasterNetId, args.CasterNetworkId);
    }

    detail::ResolveActiveCastPositions(args);
    detail::ResolveSpellBookFields(args);

    return args;
}




// Decoder for OnDoCast (sub_97C290). Unlike DecodeProcessSpell, RCX here is a
// SpellDataInstance* (one of the per-slot spell objects on the hero), NOT a
// SpellBookClient. Hero is resolved via the source-index field at RCX+0xA8 —
// the same lookup the routine itself performs through ObjectManager::FindObject.
// See PlayerEvent_Hooks_IDA_Analysis.md §1 for the IDA reference.
inline ProcessSpellEventArgs DecodeDoCast(const RawEventArgs& raw) {
    ProcessSpellEventArgs args{};
    args.Raw = raw;
    args.Spellbook = 0;            // not a spellbook in this hook
    args.CastInfo = static_cast<uintptr_t>(raw.Rdx);

    // Resolve the hero from the SpellDataInstance source-index. Field offset
    // 0xA8 was extracted directly from sub_97C290's prologue:
    //     mov edx, [rcx+0xA8] ; call ObjectManager::FindObject
    if (raw.Rcx) {
        uint32_t srcIndex = 0;
        detail::Read(raw.Rcx + 0xA8, srcIndex);
        args.SourceIndex = static_cast<int>(srcIndex);
        args.Sender = detail::ResolveObjectByIndex(args.SourceIndex);
        if (!args.Sender.IsValid()) {
            args.Sender = detail::ResolveObjectByNetworkId(srcIndex);
        }
    }

    if (args.CastInfo) {
        detail::FillCastInfoFields(args.CastInfo, args);
    }
    if (args.Sender.IsValid()) {
        args.CasterNetworkId = args.Sender.NetworkId;
    }
    detail::ResolveSpellBookFields(args);
    return args;
}


inline CastSpellEventArgs DecodeProcessCastSpell(const RawEventArgs& raw) {
    CastSpellEventArgs args{};
    args.Raw = raw;
    args.Sender = raw.Object;
    args.CastRequest = static_cast<uintptr_t>(raw.Rdx);

    if (args.CastRequest) {
        detail::Read(
            args.CastRequest + Offset::SpellInputLayout::InputTargetNetId,
            args.TargetNetworkId);
        detail::Read(
            args.CastRequest + Offset::SpellInputLayout::InputStartPos,
            args.StartPosition);
        detail::Read(
            args.CastRequest + Offset::SpellInputLayout::InputEndPos,
            args.EndPosition);
    }

    return args;
}

inline PlayAnimationEventArgs DecodePlayAnimation(const RawEventArgs& raw) {
    // OnPlayAnimationWrapper (sub_E15D10):
    //   RCX = central request wrapper, *(RCX+0x08) = AIBaseClient,
    //   RDX = input string-view { data, size, ... }.
    //
    // OnPlayAnimation (sub_29BF90):
    //   packet callback for ids 0x11D/0x1F1, RCX = AIBaseClient,
    //   RDX+0x18 = internal string-view { data, size }.
    PlayAnimationEventArgs args{};
    args.Raw = raw;
    args.Process = nullptr;
    args.Accepted = true;

    uintptr_t animationView = 0;
    if (raw.Id == Hooks::OnPlayAnimationWrapper) {
        uintptr_t sender = 0;
        if (raw.Rcx) {
            detail::Read(raw.Rcx + 0x08, sender);
        }
        if (detail::LooksLikeObject(sender)) {
            args.Sender = detail::ReadObject(sender);
        }
        animationView = static_cast<uintptr_t>(raw.Rdx);
    } else {
        if (detail::LooksLikeObject(raw.Rcx)) {
            args.Sender = detail::ReadObject(raw.Rcx);
        }
        if (raw.Rdx) {
            animationView = static_cast<uintptr_t>(raw.Rdx) + 0x18;
        }
    }

    if (animationView) {
        detail::CopyStringView(
            animationView,
            args.Animation,
            static_cast<int>(sizeof(args.Animation)));

        // sub_117F860/sub_1115BF0: ASCII case-insensitive FNV-1a.
        uint32_t hash = 0;
        if (detail::HashLowerStringView(animationView, hash)) {
            args.AnimationId = static_cast<int>(hash);
        }
    }
    return args;
}




inline StopCastEventArgs DecodeStopCast(const RawEventArgs& raw) {
    // OnStopCast (sub_982430).
    //
    // Runtime behaviour observed June/2026: the hook's prologue belongs to a
    // SpellBookClient method (`sub_982430(hero+0x3128, ...)`), but the inline
    // detour is also triggered through the four vtable slots that share this
    // entry point (4 data xrefs into the spell-handler vtables). In those
    // cases RCX is one of the SpellCast/spell-instance objects in the hero's
    // active-cast vector — NOT a SpellBookClient — so reading +0x08/+0xA8 as
    // SpellBook layout yields garbage.
    //
    // Strategy: try several candidate offsets to recover the caster network
    // id, then validate by resolving each candidate through ObjectManager.
    // First valid hero wins. If nothing resolves, the sender stays empty and
    // IsLocalPlayer will (correctly) reject the event.
    StopCastEventArgs args{};
    args.Raw = raw;
    args.Spellbook = raw.Rcx;
    args.ProcessFlag = raw.R8;
    args.ForceStop = (raw.Rdx & 0xFF) == 0;

    if (raw.Rcx) {
        // Try every offset where a 32-bit CasterNetId/SrcIndex is known to
        // live across the SpellBook/SpellCast variants we may end up on:
        //   +0xA8  → SpellBookLayout::CasterNetId  (canonical spellbook)
        //   +0x98  → SpellCastInfoEventLayout::SrcIndex (active cast struct)
        //   +0x2DC → SpellCastInfoLayout::CasterNetId (active-cast variant)
        constexpr uint32_t kOffsets[] = { 0xA8, 0x98, 0x2DC };
        for (uint32_t off : kOffsets) {
            uint32_t netId = 0;
            detail::Read(raw.Rcx + off, netId);
            if (netId == 0 || netId == 0xFFFFFFFFu) {
                continue;
            }
            // SrcIndex (+0x98) is an index, not a NetId, so try both lookups.
            ObjectInfo candidate = detail::ResolveObjectByNetworkId(netId);
            if (!candidate.IsValid()) {
                candidate = detail::ResolveObjectByIndex(static_cast<int>(netId));
            }
            if (candidate.IsValid()) {
                args.Sender = candidate;
                args.CasterNetworkId = candidate.NetworkId;
                break;
            }
        }

        // Fallback: classic spellbook owner pointer.
        if (!args.Sender.IsValid()) {
            args.Sender = detail::ReadSpellbookOwner(args.Spellbook);
            if (args.Sender.IsValid()) {
                args.CasterNetworkId = args.Sender.NetworkId;
            }
        }
    }

    // ActiveSlot field is at +0xB4 on a real spellbook; harmless to read
    // (errors caught by detail::Read SEH wrapper) even when RCX isn't one.
    detail::Read(args.Spellbook + Offset::SpellBookLayout::ActiveSlot, args.Slot);
    return args;
}


inline void __fastcall Dispatch(int id,
                                uintptr_t rcx,
                                uint64_t rdx,
                                uintptr_t r8,
                                uintptr_t r9,
                                float xmm0,
                                float xmm1,
                                float xmm2,
                                float xmm3,
                                uintptr_t stack0,
                                uintptr_t stack1,
                                uintptr_t stack2,
                                uintptr_t stack3,
                                uintptr_t stack4,
                                uintptr_t stack5) {
    if (InterlockedCompareExchange(&detail::ShuttingDown, 0, 0) != 0) {
        return;
    }
    InterlockedIncrement(&detail::ActiveDispatches);

    const bool writePhase = id == static_cast<int>(Hooks::OnGameUpdate);
    if (writePhase) {
        CoreRuntime::BeginWritePhase();
    }

    auto previous = detail::PreviousCallback;
    if (previous && previous != &Dispatch) {
        __try {
            previous(
                id, rcx, rdx, r8, r9, xmm0, xmm1, xmm2, xmm3,
                stack0, stack1, stack2, stack3, stack4, stack5);
        } __except (1) {}
    }

    if (id < 0 || id >= ::CoreHookTest::HookCount) {
        if (writePhase) {
            CoreRuntime::EndWritePhase();
        }
        InterlockedDecrement(&detail::ActiveDispatches);
        return;
    }

    detail::Fire(detail::MakeRaw(
        static_cast<HookId>(id),
        rcx,
        rdx,
        r8,
        r9,
        xmm0,
        xmm1,
        xmm2,
        xmm3,
        stack0,
        stack1,
        stack2,
        stack3,
        stack4,
        stack5));

    if (writePhase) {
        CoreRuntime::EndWritePhase();
    }
    InterlockedDecrement(&detail::ActiveDispatches);
}

inline void Initialize() {
    InterlockedExchange(&detail::ShuttingDown, 0);
    if (InterlockedCompareExchange(&detail::Initialized, 1, 0) == 0) {
        if (::CoreHookTest::g_callback != &Dispatch) {
            detail::PreviousCallback = ::CoreHookTest::g_callback;
        }
        ::CoreHookTest::g_callback = &Dispatch;
    }

    (void)CoreRuntime::EnsureInitialized();
#if NIGHTSHARP_COREHOOK_AUTO_INSTALL_ALL
    (void)::CoreHookTest::EnsureInstalled(true);
    static volatile LONG s_hookStatusDumped = 0;
    if (InterlockedCompareExchange(&s_hookStatusDumped, 1, 0) == 0) {
        ::CoreHookTest::DumpHookStatus("CoreEvents::Initialize");
    }
#else
    static volatile LONG s_autoInstallLogged = 0;
    if (InterlockedCompareExchange(&s_autoInstallLogged, 1, 0) == 0) {
        ::CoreHookTest::Logf(
            "CoreEvents Initialize: auto InstallAllHooks disabled");
    }
#endif
}

inline void BeginShutdown() {
    InterlockedExchange(&detail::ShuttingDown, 1);
    if (::CoreHookTest::g_callback == &Dispatch) {
        ::CoreHookTest::g_callback = detail::PreviousCallback;
    }
}

inline void ResetCallbacks() {
    for (int hook = 0; hook < ::CoreHookTest::HookCount; ++hook) {
        detail::CallbackCounts[hook] = 0;
        for (int i = 0; i < detail::MaxCallbacksPerHook; ++i) {
            detail::Callbacks[hook][i] = nullptr;
        }
    }

    detail::BuffBridgeOwnerCursor = 0;
    for (auto& entry : detail::BuffBridgeOwners) {
        entry = {};
    }
}

inline void Shutdown() {
    BeginShutdown();

    ::CoreHookTest::DumpHookStatus("CoreEvents::Shutdown");

    const DWORD waitStart = GetTickCount();
    while (InterlockedCompareExchange(&detail::ActiveDispatches, 0, 0) != 0 &&
           GetTickCount() - waitStart < 2000) {
        Sleep(1);
    }

    ResetCallbacks();
    detail::PreviousCallback = nullptr;
    InterlockedExchange(&detail::ActiveDispatches, 0);
    InterlockedExchange(&detail::Initialized, 0);
}

inline bool Add(HookId id, RawCallback callback) {
    if (!callback || id < 0 || id >= ::CoreHookTest::HookCount) {
        return false;
    }

    Initialize();

    const int index = static_cast<int>(id);
    int& count = detail::CallbackCounts[index];
    for (int i = 0; i < count; ++i) {
        if (detail::Callbacks[index][i] == callback) {
            return true;
        }
    }

    if (count >= detail::MaxCallbacksPerHook) {
        return false;
    }

    detail::Callbacks[index][count++] = callback;
    return true;
}

inline bool Remove(HookId id, RawCallback callback) {
    if (!callback || id < 0 || id >= ::CoreHookTest::HookCount) {
        return false;
    }

    const int index = static_cast<int>(id);
    int& count = detail::CallbackCounts[index];
    for (int i = 0; i < count; ++i) {
        if (detail::Callbacks[index][i] != callback) {
            continue;
        }

        for (int j = i; j + 1 < count; ++j) {
            detail::Callbacks[index][j] = detail::Callbacks[index][j + 1];
        }
        detail::Callbacks[index][--count] = nullptr;
        return true;
    }

    return false;
}

inline bool IsInstalled(HookId id) {
    return id >= 0 && id < ::CoreHookTest::HookCount && ::CoreHookTest::IsInstalled(id);
}

inline long long HitCount(HookId id) {
    return id >= 0 && id < ::CoreHookTest::HookCount ? ::CoreHookTest::HitCount(id) : 0;
}

inline const char* Name(HookId id) {
    return id >= 0 && id < ::CoreHookTest::HookCount ? ::CoreHookTest::kHookSpecs[id].name : "";
}

inline uintptr_t Rva(HookId id) {
    return id >= 0 && id < ::CoreHookTest::HookCount ? ::CoreHookTest::kHookSpecs[id].rva : 0;
}

inline uintptr_t TargetAddress(HookId id) {
    return id >= 0 && id < ::CoreHookTest::HookCount ? ::CoreHookTest::TargetAddress(id) : 0;
}

inline const char* StatusLabel(HookId id) {
    return id >= 0 && id < ::CoreHookTest::HookCount
        ? ::CoreHookTest::StatusLabel(::CoreHookTest::Status(id))
        : "bad-args";
}

#define NS_CORE_EVENT_FORWARD(aliasName, hookName)                                    \
    inline bool Add##aliasName(RawCallback callback) {                                \
        return Add(Hooks::hookName, callback);                                        \
    }                                                                                 \
    inline bool aliasName(RawCallback callback) {                                     \
        return Add##aliasName(callback);                                              \
    }                                                                                 \
    inline bool Remove##aliasName(RawCallback callback) {                             \
        return Remove(Hooks::hookName, callback);                                     \
    }

NS_CORE_EVENT_FORWARD(OnIntegerPropertyChange, OnIntegerPropertyChange)
NS_CORE_EVENT_FORWARD(OnClientMainLoop, ClientMainLoop)
NS_CORE_EVENT_FORWARD(OnCreateClientEffect, CreateClientEffect)
NS_CORE_EVENT_FORWARD(OnDispatchEvent, DispatchEvent)
NS_CORE_EVENT_FORWARD(OnTeleport, OnTeleport)
NS_CORE_EVENT_FORWARD(OnHudDisconnect, Hud_OnDisconnect)
NS_CORE_EVENT_FORWARD(OnHud_OnDisconnect, Hud_OnDisconnect)
NS_CORE_EVENT_FORWARD(OnBuffAdd, OnBuffAdd)
NS_CORE_EVENT_FORWARD(OnBuffRemove, OnBuffRemove)
NS_CORE_EVENT_FORWARD(OnBuffUpdate, OnBuffUpdate)
NS_CORE_EVENT_FORWARD(OnDamage, OnDamage)
NS_CORE_EVENT_FORWARD(OnDoCast, OnDoCast)
NS_CORE_EVENT_FORWARD(OnFinishCast, OnFinishCast)
NS_CORE_EVENT_FORWARD(OnGameUpdate, OnGameUpdate)
NS_CORE_EVENT_FORWARD(OnLevelUp, OnLevelUp)
NS_CORE_EVENT_FORWARD(OnPlayAnimation, OnPlayAnimation)
NS_CORE_EVENT_FORWARD(OnPlayAnimationWrapper, OnPlayAnimationWrapper)
NS_CORE_EVENT_FORWARD(OnProcessSpell, OnProcessSpell)
NS_CORE_EVENT_FORWARD(OnSpellImpact, OnSpellImpact)
NS_CORE_EVENT_FORWARD(OnStopCast, OnStopCast)
NS_CORE_EVENT_FORWARD(OnStealth, OnStealth)
NS_CORE_EVENT_FORWARD(OnSurrender, OnSurrender)
NS_CORE_EVENT_FORWARD(OnProcessWorldEvent, ProcessWorldEvent)
NS_CORE_EVENT_FORWARD(OnProcessCastSpell, ProcessCastSpell)
NS_CORE_EVENT_FORWARD(OnUpdateChargeableSpell, OnUpdateChargeableSpell)
NS_CORE_EVENT_FORWARD(OnBuffGain, OnBuffGain)
NS_CORE_EVENT_FORWARD(OnMissileCreate, OnMissileCreate)
NS_CORE_EVENT_FORWARD(OnMissileDelete, OnMissileDelete)
NS_CORE_EVENT_FORWARD(OnNewPath, OnNewPath)

#undef NS_CORE_EVENT_FORWARD

} // namespace Core::Events
