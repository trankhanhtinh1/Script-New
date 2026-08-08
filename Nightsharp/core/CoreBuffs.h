#pragma once

#include "CoreRuntime.h"
#include "CoreGame.h"
#include "CoreObjectManager.h"
#include "Globals.h"
#include "offset.h"

#include "../sdk/Utils/HashUtils.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace CoreBuffs {

    using SDK::Utils::HashName;
    using SDK::Utils::HashNameLen;
    using SDK::Utils::StrEqualInsensitive;

    inline constexpr uint32_t kRecallHash = HashName("recall");

    namespace Detail {
        inline constexpr uintptr_t kScriptBaseNameOffset = 0x8;
        inline constexpr int kMaxStackCount = 512;
        inline constexpr int kMaxManagerEntries = 512;

        inline bool ReadManagerVector(uintptr_t obj, uintptr_t& begin, uintptr_t& end, int& count) {
            if (!Globals::IsValidPtr(obj)) return false;

            const uintptr_t manager = obj + Offset::BuffManagerRuntime::BuffManagerOffset;
            if (!Globals::IsValidPtr(manager)) return false;

            begin = Globals::Read<uintptr_t>(manager + Offset::BuffManagerLayout::EntriesStart);
            end = Globals::Read<uintptr_t>(manager + Offset::BuffManagerLayout::EntriesEnd);
            if (!Globals::IsValidPtr(begin) || !Globals::IsValidPtr(end) || end < begin) {
                return false;
            }

            const uintptr_t bytes = end - begin;
            if (bytes == 0 || bytes % Offset::BuffEntryLayout::EntryStride != 0) {
                return false;
            }

            const uintptr_t capacityEnd = Globals::Read<uintptr_t>(
                manager + Offset::BuffManagerLayout::EntriesCapacityEnd);
            if (Globals::IsValidPtr(capacityEnd) && capacityEnd < end) {
                return false;
            }

            count = static_cast<int>(bytes / Offset::BuffEntryLayout::EntryStride);
            return count > 0 && count <= kMaxManagerEntries;
        }

        inline bool IsValidStackCount(int count) {
            return count >= 0 && count <= kMaxStackCount;
        }

    }

    struct BuffRef {
        uintptr_t address = 0;

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        int GetType() const {
            if (!IsValid()) return -1;
            return static_cast<int>(Globals::Read<uint8_t>(address + Offset::BuffDataLayout::BuffType));
        }

        int GetStacks() const {
            if (!IsValid()) return 0;

            const int count = Globals::Read<int>(address + Offset::BuffDataLayout::BuffStackCount);
            return Detail::IsValidStackCount(count) ? count : 0;
        }

        int GetLiveStackCount() const {
            return GetStacks();
        }

        int GetCounterCurrent() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::BuffDataLayout::BuffCounterCurrent);
        }

        int GetCounterMax() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::BuffDataLayout::BuffCounterMax);
        }

        float GetStartTime() const {
            if (!IsValid()) return 0.0f;
            return Globals::Read<float>(address + Offset::BuffDataLayout::BuffStartTime);
        }

        float GetEndTime() const {
            if (!IsValid()) return 0.0f;
            return Globals::Read<float>(address + Offset::BuffDataLayout::BuffEndTime);
        }

        float GetRemainingTime(float gameTime) const {
            const float endTime = GetEndTime();
            if (endTime <= 0.0f) return 0.0f;
            return endTime > gameTime ? endTime - gameTime : 0.0f;
        }

        bool IsPermanent() const {
            return GetEndTime() <= 0.0f;
        }

        bool IsActive(float gameTime) const {
            if (!IsValid() || GetLiveStackCount() <= 0) return false;

            const uintptr_t scriptBase = Globals::Read<uintptr_t>(
                address + Offset::BuffDataLayout::BuffScriptPtr);
            if (!Globals::IsValidPtr(scriptBase)) return false;

            const float endTime = GetEndTime();
            return endTime <= 0.0f || endTime > gameTime;
        }

        bool ReadName(char* out, int maxOut) const {
            if (!out || maxOut <= 0 || !IsValid()) {
                if (out && maxOut > 0) out[0] = 0;
                return false;
            }

            const uintptr_t scriptBase = Globals::Read<uintptr_t>(
                address + Offset::BuffDataLayout::BuffScriptPtr);
            if (!Globals::IsValidPtr(scriptBase)) {
                out[0] = 0;
                return false;
            }

            const uintptr_t namePtr = Globals::Read<uintptr_t>(
                scriptBase + Detail::kScriptBaseNameOffset);
            return Globals::ReadCString(namePtr, out, maxOut);
        }

        uint32_t GetCasterNetworkId() const {
            if (!IsValid() || GetLiveStackCount() <= 0) return 0;

            const uintptr_t arrayBegin = Globals::Read<uintptr_t>(
                address + Offset::BuffDataLayout::BuffStackArrayBegin);
            if (!Globals::IsValidPtr(arrayBegin)) return 0;

            const uintptr_t scriptInstance = Globals::Read<uintptr_t>(arrayBegin);
            if (!Globals::IsValidPtr(scriptInstance)) return 0;

            return Globals::Read<uint32_t>(
                scriptInstance + Offset::BuffScriptInstanceLayout::CasterNetworkId);
        }

        uintptr_t GetCaster() const {
            const uint32_t netId = GetCasterNetworkId();
            return netId == 0 ? 0 : ::Core::ObjectManager::FindByNetworkId(netId);
        }
    };

    struct CachedBuffEntry {
        uintptr_t object = 0;
        uintptr_t address = 0;
        int count = 0;
        int type = -1;
        float startTime = 0.0f;
        float endTime = 0.0f;
        char name[96] = {};
        uint32_t hash = 0;
        bool known = false;
    };

    inline constexpr int kMaxCachedBuffEntries = 1024;
    inline CachedBuffEntry CachedBuffEntries[kMaxCachedBuffEntries] = {};
    inline int CachedBuffCursor = 0;
    inline bool EventCacheEnabled = false;

#if defined(_WIN32)
    inline SRWLOCK CachedBuffLock = SRWLOCK_INIT;

    struct CachedBuffSharedLock {
        CachedBuffSharedLock() { AcquireSRWLockShared(&CachedBuffLock); }
        ~CachedBuffSharedLock() { ReleaseSRWLockShared(&CachedBuffLock); }
    };

    struct CachedBuffExclusiveLock {
        CachedBuffExclusiveLock() { AcquireSRWLockExclusive(&CachedBuffLock); }
        ~CachedBuffExclusiveLock() { ReleaseSRWLockExclusive(&CachedBuffLock); }
    };
#endif

    inline bool IsEventCacheEnabled() {
        return EventCacheEnabled;
    }

    inline void SetEventCacheEnabled(bool enabled) {
        EventCacheEnabled = enabled;
    }

    inline void ClearEventCache() {
#if defined(_WIN32)
        CachedBuffExclusiveLock lock;
#endif
        std::memset(CachedBuffEntries, 0, sizeof(CachedBuffEntries));
        CachedBuffCursor = 0;
    }

    inline uintptr_t ResolveBuffManagerOffset(uintptr_t) {
        return Offset::BuffManagerRuntime::BuffManagerOffset;
    }

    inline uintptr_t GetBuffManager(uintptr_t obj) {
        return Globals::IsValidPtr(obj)
            ? obj + Offset::BuffManagerRuntime::BuffManagerOffset
            : 0;
    }

    inline int Enumerate(uintptr_t obj, uintptr_t* out, int maxOut) {
        if (!out || maxOut <= 0) return 0;

        uintptr_t begin = 0;
        uintptr_t end = 0;
        int count = 0;
        if (!Detail::ReadManagerVector(obj, begin, end, count)) return 0;

        int written = 0;
        for (int i = 0; i < count && written < maxOut; ++i) {
            const uintptr_t entry = begin + static_cast<uintptr_t>(
                i * Offset::BuffEntryLayout::EntryStride);
            const uintptr_t buff = Globals::Read<uintptr_t>(
                entry + Offset::BuffEntryLayout::EntryBuff);
            if (Globals::IsValidPtr(buff)) out[written++] = buff;
        }
        return written;
    }

    inline bool NameContainsInsensitive(const char* text, const char* token) {
        if (!text || !token || !text[0] || !token[0]) return false;

        for (const char* p = text; *p; ++p) {
            const char* a = p;
            const char* b = token;
            while (*a && *b) {
                char ca = *a;
                char cb = *b;
                if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
                if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
                if (ca != cb) break;
                ++a;
                ++b;
            }
            if (!*b) return true;
        }
        return false;
    }

    inline void CopyStringSafe(char* dest, const char* src, size_t destSize) {
        if (!dest || destSize == 0) return;
        if (!src) {
            dest[0] = 0;
            return;
        }
        size_t i = 0;
        for (; i + 1 < destSize && src[i] != 0; ++i) dest[i] = src[i];
        dest[i] = 0;
    }

    inline void ApplyBuffAddEvent(uintptr_t obj, const char* name, int count, uintptr_t buffAddress) {
        if (!EventCacheEnabled || !Globals::IsValidPtr(obj) || !name || !name[0]) return;
#if defined(_WIN32)
        CachedBuffExclusiveLock lock;
#endif
        for (int i = 0; i < kMaxCachedBuffEntries; ++i) {
            if (CachedBuffEntries[i].object == obj && CachedBuffEntries[i].address == buffAddress) {
                CachedBuffEntries[i].count = count;
                CopyStringSafe(CachedBuffEntries[i].name, name, sizeof(CachedBuffEntries[i].name));
                CachedBuffEntries[i].hash = HashName(name);
                return;
            }
        }
        for (int i = 0; i < kMaxCachedBuffEntries; ++i) {
            if (CachedBuffEntries[i].object == 0) {
                CachedBuffEntries[i].object = obj;
                CachedBuffEntries[i].address = buffAddress;
                CachedBuffEntries[i].count = count;
                CopyStringSafe(CachedBuffEntries[i].name, name, sizeof(CachedBuffEntries[i].name));
                CachedBuffEntries[i].hash = HashName(name);
                return;
            }
        }
    }

    inline void ApplyBuffRemoveEvent(uintptr_t obj, const char* name, uintptr_t buffAddress) {
        if (!EventCacheEnabled || !Globals::IsValidPtr(obj)) return;
#if defined(_WIN32)
        CachedBuffExclusiveLock lock;
#endif
        for (int i = 0; i < kMaxCachedBuffEntries; ++i) {
            if (CachedBuffEntries[i].object == obj &&
                (buffAddress != 0
                    ? CachedBuffEntries[i].address == buffAddress
                    : (name && StrEqualInsensitive(CachedBuffEntries[i].name, name)))) {
                CachedBuffEntries[i] = {};
                return;
            }
        }
    }

    inline void ApplyBuffUpdateEvent(uintptr_t obj, const char* name, int count, uintptr_t buffAddress) {
        ApplyBuffAddEvent(obj, name, count, buffAddress);
    }

    inline float ResolveGameTime() {
        // Same cached time source as SDK::Game::Time() — keeps the frame
        // snapshot cache key consistent no matter which API is used.
        return ::CoreGame::GetTime();
    }

    inline bool IsRecallBuffName(const char* name) {
        return NameContainsInsensitive(name, "recall");
    }

    inline bool NameMatchesQuery(const char* buffName, const char* query) {
        if (StrEqualInsensitive(buffName, query)) return true;
        return IsRecallBuffName(query) && IsRecallBuffName(buffName);
    }

    inline uintptr_t GetActiveSpellCast(uintptr_t obj) {
        return Globals::IsValidPtr(obj)
            ? Globals::Read<uintptr_t>(obj + Offset::SpellRuntime::ActiveSpellCast)
            : 0;
    }

    inline bool IsRecallSlotCastingActive(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return false;

        constexpr int kRecallSpellSlot = 13;
        const uintptr_t spellBook = obj + Offset::SpellRuntime::SpellBookOffset;
        const uintptr_t recallSlotPtr = Globals::Read<uintptr_t>(
            spellBook + Offset::SpellBookLayout::SpellSlotArray +
            static_cast<uintptr_t>(kRecallSpellSlot * sizeof(uintptr_t)));
        if (Globals::IsValidPtr(recallSlotPtr)) {
            const uintptr_t recallCast = Globals::Read<uintptr_t>(
                recallSlotPtr + Offset::SpellSlotLayout::SlotActiveSpellCast);
            if (Globals::IsValidPtr(recallCast)) return true;
        }

        const uintptr_t cast = GetActiveSpellCast(obj);
        if (!Globals::IsValidPtr(cast)) return false;
        return Globals::Read<uint8_t>(cast + Offset::SpellCastInfoLayout::SpellSlot) == kRecallSpellSlot;
    }

    inline bool IsRecallChannelActive(uintptr_t obj) {
        if (IsRecallSlotCastingActive(obj)) return true;
        return Globals::IsValidPtr(GetActiveSpellCast(obj));
    }

    inline bool IsSuppressedByLiveState(uintptr_t obj, const char* buffName) {
        return IsRecallBuffName(buffName) && !IsRecallChannelActive(obj);
    }

    struct FrameBuffEntry {
        uintptr_t address = 0;
        uint32_t hash = 0;
        char name[64] = {};
        float startTime = 0.0f;
        float endTime = 0.0f;
        int stacks = 0;
        uint8_t type = 0;
        bool isActive = false;
        bool isRecall = false;
    };

    struct ThreadFrameBuffSnapshot {
        uintptr_t object = 0;
        float gameTime = -1.0f;
        int count = 0;
        FrameBuffEntry entries[64] = {};
    };

    inline bool IsMatchingEntry(const FrameBuffEntry& entry, uint32_t queryHash) {
        return entry.hash == queryHash || (queryHash == kRecallHash && entry.isRecall);
    }

    inline const ThreadFrameBuffSnapshot* GetOrBuildFrameBuffSnapshot(uintptr_t obj, float gameTime) {
        if (!Globals::IsValidPtr(obj)) return nullptr;
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();

        constexpr size_t kSlots = 32;
        static thread_local ThreadFrameBuffSnapshot frameCache[kSlots] = {};
        const size_t slot = (obj >> 4) & (kSlots - 1);
        ThreadFrameBuffSnapshot& snapshot = frameCache[slot];

        if (snapshot.object == obj && std::fabs(snapshot.gameTime - gameTime) < 0.0001f) {
            return &snapshot;
        }

        snapshot.object = obj;
        snapshot.gameTime = gameTime;
        snapshot.count = 0;

        uintptr_t buffs[128] = {};
        const int numBuffs = Enumerate(obj, buffs, 128);
        for (int i = 0; i < numBuffs && snapshot.count < 64; ++i) {
            BuffRef buff{buffs[i]};
            FrameBuffEntry entry{};
            entry.address = buffs[i];
            if (!buff.ReadName(entry.name, static_cast<int>(sizeof(entry.name)))) continue;
            entry.hash = HashName(entry.name);
            entry.startTime = buff.GetStartTime();
            entry.endTime = buff.GetEndTime();
            entry.stacks = buff.GetStacks();
            entry.type = static_cast<uint8_t>(buff.GetType());
            entry.isActive = buff.IsActive(gameTime);
            entry.isRecall = IsRecallBuffName(entry.name);
            snapshot.entries[snapshot.count++] = entry;
        }
        return &snapshot;
    }

    inline bool HasBuff(uintptr_t obj, uint32_t nameHash, float gameTime = -1.0f) {
        if (nameHash == 0 || !Globals::IsValidPtr(obj)) return false;
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();

        const auto* snapshot = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snapshot) return false;

        for (int i = 0; i < snapshot->count; ++i) {
            const auto& entry = snapshot->entries[i];
            if (!entry.isActive) continue;
            if (entry.isRecall && !IsRecallChannelActive(obj)) continue;
            if (IsMatchingEntry(entry, nameHash)) return true;
        }
        return false;
    }

    inline bool HasBuff(uintptr_t obj, const char* name) {
        return name && name[0] && HasBuff(obj, HashName(name));
    }

    inline bool HasActiveBuff(uintptr_t obj, uint32_t nameHash, float gameTime) {
        return HasBuff(obj, nameHash, gameTime);
    }

    inline bool HasActiveBuff(uintptr_t obj, const char* name, float gameTime) {
        return name && name[0] && HasBuff(obj, HashName(name), gameTime);
    }

    inline bool HasBuffContaining(uintptr_t obj, const char* token, int requiredType = -1) {
        if (!token || !token[0]) return false;
        const auto* snapshot = GetOrBuildFrameBuffSnapshot(obj, ResolveGameTime());
        if (!snapshot) return false;

        for (int i = 0; i < snapshot->count; ++i) {
            const auto& entry = snapshot->entries[i];
            if (!entry.isActive) continue;
            if (requiredType >= 0 && static_cast<int>(entry.type) != requiredType) continue;
            if (entry.isRecall && !IsRecallChannelActive(obj)) continue;
            if (NameContainsInsensitive(entry.name, token)) return true;
        }
        return false;
    }

    inline bool HasActiveBuffContaining(uintptr_t obj, const char* token, int requiredType = -1, float gameTime = -1.0f) {
        if (!token || !token[0]) return false;
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();
        const auto* snapshot = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snapshot) return false;

        for (int i = 0; i < snapshot->count; ++i) {
            const auto& entry = snapshot->entries[i];
            if (!entry.isActive) continue;
            if (requiredType >= 0 && static_cast<int>(entry.type) != requiredType) continue;
            if (entry.isRecall && !IsRecallChannelActive(obj)) continue;
            if (NameContainsInsensitive(entry.name, token)) return true;
        }
        return false;
    }

    inline int GetBuffStacks(uintptr_t obj, uint32_t nameHash, float gameTime = -1.0f) {
        if (nameHash == 0 || !Globals::IsValidPtr(obj)) return 0;
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();

        const auto* snapshot = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snapshot) return 0;

        int bestStacks = 0;
        for (int i = 0; i < snapshot->count; ++i) {
            const auto& entry = snapshot->entries[i];
            if (!entry.isActive) continue;
            if (entry.isRecall && !IsRecallChannelActive(obj)) continue;
            if (IsMatchingEntry(entry, nameHash) && entry.stacks > bestStacks) {
                bestStacks = entry.stacks;
            }
        }
        return bestStacks;
    }

    inline int GetBuffStacks(uintptr_t obj, const char* name) {
        return name && name[0] ? GetBuffStacks(obj, HashName(name)) : 0;
    }

    inline int GetActiveBuffStacks(uintptr_t obj, const char* name, float gameTime) {
        return name && name[0] ? GetBuffStacks(obj, HashName(name), gameTime) : 0;
    }

    inline float GetBuffRemainingTime(uintptr_t obj, uint32_t nameHash, float gameTime = -1.0f) {
        if (nameHash == 0 || !Globals::IsValidPtr(obj)) return 0.0f;
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();

        const auto* snapshot = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snapshot) return 0.0f;

        for (int i = 0; i < snapshot->count; ++i) {
            const auto& entry = snapshot->entries[i];
            if (!entry.isActive) continue;
            if (entry.isRecall && !IsRecallChannelActive(obj)) continue;
            if (IsMatchingEntry(entry, nameHash)) {
                return entry.endTime > 0.0f && entry.endTime > gameTime
                    ? entry.endTime - gameTime
                    : 0.0f;
            }
        }
        return 0.0f;
    }

    inline float GetBuffRemainingTime(uintptr_t obj, const char* name, float gameTime) {
        return name && name[0] ? GetBuffRemainingTime(obj, HashName(name), gameTime) : 0.0f;
    }

    inline BuffRef FindActiveByName(uintptr_t obj, const char* name, float gameTime = -1.0f) {
        if (!name || !name[0] || !Globals::IsValidPtr(obj)) return {};
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();

        const uint32_t queryHash = HashName(name);
        const auto* snapshot = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snapshot) return {};

        for (int i = 0; i < snapshot->count; ++i) {
            const auto& entry = snapshot->entries[i];
            if (!entry.isActive) continue;
            if (entry.isRecall && !IsRecallChannelActive(obj)) continue;
            if (IsMatchingEntry(entry, queryHash)) return BuffRef{entry.address};
        }
        return {};
    }

    inline BuffRef FindByName(uintptr_t obj, const char* name) {
        return FindActiveByName(obj, name, -1.0f);
    }

    inline BuffRef FindRawByName(uintptr_t obj, const char* name) {
        if (!name || !name[0]) return {};

        const uint32_t queryHash = HashName(name);
        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buffer[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{buffs[i]};
            if (!buff.ReadName(buffer, static_cast<int>(sizeof(buffer)))) continue;
            if (HashName(buffer) == queryHash || NameMatchesQuery(buffer, name)) return buff;
        }
        return {};
    }

    inline bool HasBuffRaw(uintptr_t obj, const char* name) {
        return FindRawByName(obj, name).IsValid();
    }

    inline bool HasBuffType(uintptr_t obj, int type) {
        const auto* snapshot = GetOrBuildFrameBuffSnapshot(obj, ResolveGameTime());
        if (!snapshot) return false;

        for (int i = 0; i < snapshot->count; ++i) {
            const auto& entry = snapshot->entries[i];
            if (entry.isActive && static_cast<int>(entry.type) == type) {
                if (!entry.isRecall || IsRecallChannelActive(obj)) return true;
            }
        }
        return false;
    }

    inline bool HasBuffTypeRaw(uintptr_t obj, int type) {
        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        for (int i = 0; i < count; ++i) {
            const BuffRef buff{buffs[i]};
            if (buff.IsValid() && buff.GetType() == type) return true;
        }
        return false;
    }

    inline bool HasActiveBuffType(uintptr_t obj, int type, float gameTime) {
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();
        const auto* snapshot = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snapshot) return false;

        for (int i = 0; i < snapshot->count; ++i) {
            const auto& entry = snapshot->entries[i];
            if (entry.isActive && static_cast<int>(entry.type) == type) {
                if (!entry.isRecall || IsRecallChannelActive(obj)) return true;
            }
        }
        return false;
    }

    inline int Count(uintptr_t obj) {
        const auto* snapshot = GetOrBuildFrameBuffSnapshot(obj, ResolveGameTime());
        return snapshot ? snapshot->count : 0;
    }

} // namespace CoreBuffs

namespace Literals {
    constexpr uint32_t operator""_buffHash(const char* str, size_t len) {
        return CoreBuffs::HashNameLen(str, len);
    }
}
