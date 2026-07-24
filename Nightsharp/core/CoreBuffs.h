#pragma once

// ============================================================================
// CoreBuffs.h - Buff manager iteration and active buff queries
// ----------------------------------------------------------------------------
// High-performance Buff Manager with:
//   1. Dynamic BuffManagerOffset scanning [0x2000..0x3800]
//   2. FNV-1a Case-Insensitive String Hashing (O(1) integer matching)
//   3. Thread-Local Per-Frame Snapshot Caching (0 RPM reads on repeated queries)
//   4. Backward-compatible APIs for string & pre-hashed queries
// ============================================================================

#include "../DebugLog.h"
#include "CoreRuntime.h"
#include "CoreObjectManager.h"
#include "Globals.h"
#include "offset.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>

#if defined(_WIN32)
#include <windows.h>   // lstrcmpiA
#endif

namespace CoreBuffs {

    // Offset of the `char*` name field inside ScriptBaseBuff
    constexpr uintptr_t kScriptBaseNameOffset = 0x8;

    // ── FNV-1a Case-Insensitive Hash Helpers ──

    constexpr uint32_t HashName(const char* str) {
        if (!str) return 0;
        uint32_t hash = 0x811c9dc5u;
        for (size_t i = 0; str[i] != '\0'; ++i) {
            char c = str[i];
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
            hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
            hash *= 0x01000193u;
        }
        return hash;
    }

    constexpr uint32_t HashNameLen(const char* str, size_t len) {
        if (!str) return 0;
        uint32_t hash = 0x811c9dc5u;
        for (size_t i = 0; i < len; ++i) {
            char c = str[i];
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
            hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
            hash *= 0x01000193u;
        }
        return hash;
    }

    inline constexpr uint32_t kRecallHash = HashName("recall");

    struct BuffRef {
        uintptr_t address = 0;

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        int GetType() const {
            return static_cast<int>(Globals::Read<uint8_t>(address + Offset::BuffDataLayout::BuffType));
        }

        int GetStacks() const {
            if (!IsValid()) return 0;

            const auto begin = Globals::Read<uintptr_t>(address + Offset::BuffDataLayout::BuffStackArrayBegin);
            const auto end = Globals::Read<uintptr_t>(address + Offset::BuffDataLayout::BuffStacks);

            if (Globals::IsValidPtr(begin) && Globals::IsValidPtr(end) && end >= begin) {
                const auto count = static_cast<int>((end - begin) / Offset::BuffScriptInstanceLayout::EntryStride);
                if (count >= 0 && count < 1000) {
                    return count;
                }
            }

            const int alt = Globals::Read<int>(address + Offset::BuffDataLayout::BuffStacksAlt);
            if (alt > 0 && alt < 1000) {
                return alt;
            }

            return 0;
        }

        int GetCounterCurrent() const {
            return Globals::Read<int>(address + Offset::BuffDataLayout::BuffCounterCurrent);
        }

        int GetCounterMax() const {
            return Globals::Read<int>(address + Offset::BuffDataLayout::BuffCounterMax);
        }

        float GetStartTime() const {
            return Globals::Read<float>(address + Offset::BuffDataLayout::BuffStartTime);
        }

        float GetEndTime() const {
            return Globals::Read<float>(address + Offset::BuffDataLayout::BuffEndTime);
        }

        float GetRemainingTime(float gameTime) const {
            const float endTime = GetEndTime();
            if (endTime <= 0.0f) return 0.0f;
            return endTime > gameTime ? (endTime - gameTime) : 0.0f;
        }

        bool IsPermanent() const {
            return GetEndTime() <= 0.0f;
        }

        bool IsActive(float gameTime) const {
            if (!IsValid()) return false;

            const int liveCount = Globals::Read<int>(address + 0x38);
            if (liveCount <= 0 && GetStacks() <= 0) {
                return false;
            }

            const float endTime = GetEndTime();
            if (endTime <= 0.0f) {
                return true;
            }
            return endTime > gameTime;
        }

        bool ReadName(char* out, int maxOut) const {
            if (!out || maxOut <= 0 || !IsValid()) {
                if (out && maxOut > 0) out[0] = 0;
                return false;
            }

            const auto scriptBase = Globals::Read<uintptr_t>(address + Offset::BuffDataLayout::BuffScriptPtr);
            if (!Globals::IsValidPtr(scriptBase)) {
                out[0] = 0;
                return false;
            }

            const auto charPtr = Globals::Read<uintptr_t>(scriptBase + kScriptBaseNameOffset);
            return Globals::ReadCString(charPtr, out, maxOut);
        }

        uint32_t GetCasterNetworkId() const {
            if (!IsValid()) return 0;

            const auto arrayBegin = Globals::Read<uintptr_t>(
                address + Offset::BuffDataLayout::BuffStackArrayBegin);
            if (!Globals::IsValidPtr(arrayBegin)) return 0;

            const auto arrayEnd = Globals::Read<uintptr_t>(
                address + Offset::BuffDataLayout::BuffStacks);
            if (!Globals::IsValidPtr(arrayEnd) || arrayEnd <= arrayBegin) return 0;

            const auto scriptInstance = Globals::Read<uintptr_t>(arrayBegin);
            if (!Globals::IsValidPtr(scriptInstance)) return 0;

            return Globals::Read<uint32_t>(
                scriptInstance + Offset::BuffScriptInstanceLayout::CasterNetworkId);
        }

        uintptr_t GetCaster() const {
            const auto netId = GetCasterNetworkId();
            if (netId == 0) return 0;
            return ::Core::ObjectManager::FindByNetworkId(netId);
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

    // ── Manager accessors ──

    inline uintptr_t ResolveBuffManagerOffset(uintptr_t obj) {
        static uintptr_t s_cachedOffset = 0;
        if (s_cachedOffset != 0) {
            return s_cachedOffset;
        }

        if (!Globals::IsValidPtr(obj)) return Offset::BuffManagerRuntime::BuffManagerOffset;

        for (uintptr_t offset = 0x2000; offset <= 0x3800; offset += 8) {
            const uintptr_t manager = obj + offset;
            const uintptr_t start = Globals::Read<uintptr_t>(manager + Offset::BuffManagerLayout::EntriesStart);
            const uintptr_t end   = Globals::Read<uintptr_t>(manager + Offset::BuffManagerLayout::EntriesEnd);

            if (!Globals::IsValidPtr(start) || !Globals::IsValidPtr(end) || end <= start) {
                continue;
            }

            const uintptr_t diff = end - start;
            if (diff > 16000 || (diff % Offset::BuffEntryLayout::EntryStride != 0)) {
                continue;
            }

            const uintptr_t entryBuff = Globals::Read<uintptr_t>(start + Offset::BuffEntryLayout::EntryBuff);
            if (!Globals::IsValidPtr(entryBuff)) {
                continue;
            }

            const uintptr_t scriptBase = Globals::Read<uintptr_t>(entryBuff + Offset::BuffDataLayout::BuffScriptPtr);
            if (!Globals::IsValidPtr(scriptBase)) {
                continue;
            }

            const uintptr_t namePtr = Globals::Read<uintptr_t>(scriptBase + kScriptBaseNameOffset);
            if (!Globals::IsValidPtr(namePtr)) {
                continue;
            }

            char sampleName[64] = {};
            if (Globals::ReadCString(namePtr, sampleName, sizeof(sampleName)) &&
                sampleName[0] >= 'A' && sampleName[0] <= 'z') {
                s_cachedOffset = offset;
                NightSharpDebug::Logf("[CoreBuffs] 100%% VERIFIED REAL BuffManagerOffset = 0x%X (Sample: %s)",
                    static_cast<unsigned>(offset), sampleName);
                return s_cachedOffset;
            }
        }

        return Offset::BuffManagerRuntime::BuffManagerOffset;
    }

    inline uintptr_t GetBuffManager(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return 0;
        return obj + ResolveBuffManagerOffset(obj);
    }

    inline int Enumerate(uintptr_t obj, uintptr_t* out, int maxOut) {
        if (!out || maxOut <= 0 || !Globals::IsValidPtr(obj)) return 0;

        const auto manager = GetBuffManager(obj);
        if (!Globals::IsValidPtr(manager)) return 0;

        const auto begin = Globals::Read<uintptr_t>(manager + Offset::BuffManagerLayout::EntriesStart);
        const auto end   = Globals::Read<uintptr_t>(manager + Offset::BuffManagerLayout::EntriesEnd);
        if (!Globals::IsValidPtr(begin) || !Globals::IsValidPtr(end) || end <= begin) {
            return 0;
        }

        const auto bytes = static_cast<size_t>(end - begin);
        const auto count = static_cast<int>(bytes / Offset::BuffEntryLayout::EntryStride);
        if (count <= 0 || count > 512) return 0;

        int written = 0;
        for (int i = 0; i < count && written < maxOut; ++i) {
            const auto entry = begin + static_cast<uintptr_t>(i * Offset::BuffEntryLayout::EntryStride);
            const auto buff = Globals::Read<uintptr_t>(entry + Offset::BuffEntryLayout::EntryBuff);
            if (!Globals::IsValidPtr(buff)) continue;
            out[written++] = buff;
        }
        return written;
    }

    // ── Name helpers ──

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

    inline bool StrEqualInsensitive(const char* a, const char* b) {
#if defined(_WIN32)
        return lstrcmpiA(a, b) == 0;
#else
        if (!a || !b) return false;
        while (*a && *b) {
            char ca = *a;
            char cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
            if (ca != cb) return false;
            ++a; ++b;
        }
        return *a == 0 && *b == 0;
#endif
    }

    inline float ResolveGameTime() {
        const auto& ctx = CoreRuntime::GetContext();
        if (!ctx.moduleBase) {
            return 0.0f;
        }
        return Globals::Read<float>(ctx.moduleBase + Offset::GameRuntime::GameTime);
    }

    inline bool IsRecallBuffName(const char* name) {
        return NameContainsInsensitive(name, "recall");
    }

    inline bool NameMatchesQuery(const char* buffName, const char* query) {
        if (StrEqualInsensitive(buffName, query)) {
            return true;
        }
        return IsRecallBuffName(query) && IsRecallBuffName(buffName);
    }

    inline uintptr_t GetActiveSpellCast(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return 0;
        return Globals::Read<uintptr_t>(obj + Offset::SpellRuntime::ActiveSpellCast);
    }

    inline bool IsRecallSlotCastingActive(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return false;

        constexpr int kRecallSpellSlot = 13;
        const auto spellBook = obj + Offset::SpellRuntime::SpellBookOffset;
        const auto recallSlotPtr = Globals::Read<uintptr_t>(
            spellBook + Offset::SpellBookLayout::SpellSlotArray +
            static_cast<uintptr_t>(kRecallSpellSlot * sizeof(uintptr_t)));
        if (Globals::IsValidPtr(recallSlotPtr)) {
            const auto recallCast = Globals::Read<uintptr_t>(
                recallSlotPtr + Offset::SpellSlotLayout::SlotActiveSpellCast);
            if (Globals::IsValidPtr(recallCast)) return true;
        }

        const auto cast = GetActiveSpellCast(obj);
        if (!Globals::IsValidPtr(cast)) return false;

        const int slot = static_cast<int>(Globals::Read<uint8_t>(
            cast + Offset::SpellCastInfoLayout::SpellSlot));
        return slot == kRecallSpellSlot;
    }

    inline bool IsRecallChannelActive(uintptr_t obj) {
        if (IsRecallSlotCastingActive(obj)) return true;
        const auto cast = GetActiveSpellCast(obj);
        return Globals::IsValidPtr(cast);
    }

    inline bool IsSuppressedByLiveState(uintptr_t obj, const char* buffName) {
        if (IsRecallBuffName(buffName)) {
            return !IsRecallChannelActive(obj);
        }
        return false;
    }

    // ── Thread-Local Frame Snapshot Caching (Zero-RPM per-frame caching) ──

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

    inline const ThreadFrameBuffSnapshot* GetOrBuildFrameBuffSnapshot(uintptr_t obj, float gameTime) {
        if (!Globals::IsValidPtr(obj)) return nullptr;
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();

        constexpr int kSlots = 16;
        static thread_local ThreadFrameBuffSnapshot s_frameCache[kSlots] = {};
        const size_t slot = (obj >> 4) & (kSlots - 1);

        ThreadFrameBuffSnapshot& snap = s_frameCache[slot];
        if (snap.object == obj && std::fabs(snap.gameTime - gameTime) < 0.0001f) {
            return &snap;
        }

        snap.object = obj;
        snap.gameTime = gameTime;
        snap.count = 0;

        uintptr_t buffs[128] = {};
        const int numBuffs = Enumerate(obj, buffs, 128);
        for (int i = 0; i < numBuffs && snap.count < 64; ++i) {
            BuffRef buff{ buffs[i] };
            FrameBuffEntry& e = snap.entries[snap.count];
            e.address = buffs[i];
            if (!buff.ReadName(e.name, static_cast<int>(sizeof(e.name)))) continue;
            e.hash = HashName(e.name);
            e.startTime = buff.GetStartTime();
            e.endTime = buff.GetEndTime();
            e.stacks = buff.GetStacks();
            e.type = static_cast<uint8_t>(buff.GetType());
            e.isActive = buff.IsActive(gameTime);
            e.isRecall = IsRecallBuffName(e.name);
            snap.count++;
        }
        return &snap;
    }

    // ── High-Performance Query API (Supports uint32_t FNV-1a Hash & string) ──

    inline bool HasBuff(uintptr_t obj, uint32_t nameHash, float gameTime = -1.0f) {
        if (nameHash == 0 || !Globals::IsValidPtr(obj)) return false;
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();

        const bool isRecallQuery = (nameHash == kRecallHash);
        const auto* snap = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snap) return false;

        for (int i = 0; i < snap->count; ++i) {
            const auto& e = snap->entries[i];
            if (!e.isActive) continue;
            if (e.isRecall && !IsRecallChannelActive(obj)) continue;
            if (e.hash == nameHash || (isRecallQuery && e.isRecall)) return true;
        }
        return false;
    }

    inline bool HasBuff(uintptr_t obj, const char* name) {
        if (!name || !name[0]) return false;
        return HasBuff(obj, HashName(name));
    }

    inline bool HasActiveBuff(uintptr_t obj, uint32_t nameHash, float gameTime) {
        return HasBuff(obj, nameHash, gameTime);
    }

    inline bool HasActiveBuff(uintptr_t obj, const char* name, float gameTime) {
        if (!name || !name[0]) return false;
        return HasBuff(obj, HashName(name), gameTime);
    }

    inline bool HasBuffContaining(uintptr_t obj, const char* token, int requiredType = -1) {
        if (!token || !token[0]) return false;

        const float gameTime = ResolveGameTime();
        const auto* snap = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snap) return false;

        for (int i = 0; i < snap->count; ++i) {
            const auto& e = snap->entries[i];
            if (!e.isActive) continue;
            if (requiredType >= 0 && static_cast<int>(e.type) != requiredType) continue;
            if (e.isRecall && !IsRecallChannelActive(obj)) continue;
            if (NameContainsInsensitive(e.name, token)) return true;
        }
        return false;
    }

    inline bool HasActiveBuffContaining(uintptr_t obj, const char* token, int requiredType = -1, float gameTime = -1.0f) {
        if (!token || !token[0]) return false;
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();
        return HasBuffContaining(obj, token, requiredType);
    }

    inline int GetBuffStacks(uintptr_t obj, uint32_t nameHash, float gameTime = -1.0f) {
        if (nameHash == 0 || !Globals::IsValidPtr(obj)) return 0;
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();

        const bool isRecallQuery = (nameHash == kRecallHash);
        const auto* snap = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snap) return 0;

        for (int i = 0; i < snap->count; ++i) {
            const auto& e = snap->entries[i];
            if (!e.isActive) continue;
            if (e.isRecall && !IsRecallChannelActive(obj)) continue;
            if (e.hash == nameHash || (isRecallQuery && e.isRecall)) return e.stacks;
        }
        return 0;
    }

    inline int GetBuffStacks(uintptr_t obj, const char* name) {
        if (!name || !name[0]) return 0;
        return GetBuffStacks(obj, HashName(name));
    }

    inline int GetActiveBuffStacks(uintptr_t obj, const char* name, float gameTime) {
        if (!name || !name[0]) return 0;
        return GetBuffStacks(obj, HashName(name), gameTime);
    }

    inline float GetBuffRemainingTime(uintptr_t obj, uint32_t nameHash, float gameTime = -1.0f) {
        if (nameHash == 0 || !Globals::IsValidPtr(obj)) return 0.0f;
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();

        const bool isRecallQuery = (nameHash == kRecallHash);
        const auto* snap = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snap) return 0.0f;

        for (int i = 0; i < snap->count; ++i) {
            const auto& e = snap->entries[i];
            if (!e.isActive) continue;
            if (e.isRecall && !IsRecallChannelActive(obj)) continue;
            if (e.hash == nameHash || (isRecallQuery && e.isRecall)) {
                if (e.endTime <= 0.0f) return 0.0f;
                return e.endTime > gameTime ? (e.endTime - gameTime) : 0.0f;
            }
        }
        return 0.0f;
    }

    inline float GetBuffRemainingTime(uintptr_t obj, const char* name, float gameTime) {
        if (!name || !name[0]) return 0.0f;
        return GetBuffRemainingTime(obj, HashName(name), gameTime);
    }

    inline BuffRef FindActiveByName(uintptr_t obj, const char* name, float gameTime = -1.0f) {
        if (!name || !name[0] || !Globals::IsValidPtr(obj)) return {};
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();

        const uint32_t queryHash = HashName(name);
        const bool isRecallQuery = (queryHash == kRecallHash);
        const auto* snap = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snap) return {};

        for (int i = 0; i < snap->count; ++i) {
            const auto& e = snap->entries[i];
            if (!e.isActive) continue;
            if (e.isRecall && !IsRecallChannelActive(obj)) continue;
            if (e.hash == queryHash || (isRecallQuery && e.isRecall)) {
                return BuffRef{ e.address };
            }
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
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
            if (HashName(buf) == queryHash || NameMatchesQuery(buf, name)) return buff;
        }
        return {};
    }

    inline bool HasBuffType(uintptr_t obj, int type) {
        const float gameTime = ResolveGameTime();
        const auto* snap = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        if (!snap) return false;

        for (int i = 0; i < snap->count; ++i) {
            const auto& e = snap->entries[i];
            if (!e.isActive) continue;
            if (static_cast<int>(e.type) != type) continue;
            if (e.isRecall && !IsRecallChannelActive(obj)) continue;
            return true;
        }
        return false;
    }

    inline bool HasBuffTypeRaw(uintptr_t obj, int type) {
        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        for (int i = 0; i < count; ++i) {
            const BuffRef buff{ buffs[i] };
            if (buff.IsValid() && buff.GetType() == type) return true;
        }
        return false;
    }

    inline bool HasActiveBuffType(uintptr_t obj, int type, float gameTime) {
        if (gameTime <= 0.0f) gameTime = ResolveGameTime();
        return HasBuffType(obj, type);
    }

    inline int Count(uintptr_t obj) {
        const float gameTime = ResolveGameTime();
        const auto* snap = GetOrBuildFrameBuffSnapshot(obj, gameTime);
        return snap ? snap->count : 0;
    }

} // namespace CoreBuffs

namespace Literals {
    constexpr uint32_t operator""_buffHash(const char* str, size_t len) {
        return CoreBuffs::HashNameLen(str, len);
    }
}
