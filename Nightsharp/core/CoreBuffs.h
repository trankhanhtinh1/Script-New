#pragma once

// ============================================================================
// CoreBuffs.h - Buff manager iteration and active buff queries
// ----------------------------------------------------------------------------
// Unlike the old NightSharp implementation which treated any buff with
// `stacks > 0 && endTime > gameTime` as active, this version uses the three
// conditions that match how the game client itself decides whether a buff is
// live. This was needed to fix the Xerath R passive-stacking bug where the
// stack-counter buff has `endTime == 0` (permanent) and the old check
// incorrectly filtered it out as expired.
//
// Active conditions (all must hold):
//   1. `entry` must still be inside `[EntriesStart, EntriesEnd)`
//      during iteration — empty pool slots have a null script + zero count.
//   2. LIVE count `*(int*)(buff + 0x38)` > 0. The game zeroes this field the
//      instant a buff is removed — INCLUDING abruptly-cancelled channel/toggle
//      buffs (Xerath R LocusOfPower2, recall) that linger in the array with a
//      stale future endTime. Verified in live memory: +0x38 goes 1 -> 0 on R
//      cancel. Do NOT fall back to stacksAlt (+0x3C) for liveness — the game
//      does NOT clear it on removal, which previously kept cancelled buffs
//      "active" forever (Xerath could not cast after ending R). GetStacks()
//      keeps the 0x3C fallback ONLY for stack-COUNT (damage) purposes.
//   3. `endTime == 0` (permanent buff) OR `endTime > gameTime`.
//
// Recall still also has a spell-state cross-check (IsSuppressedByLiveState) as
// belt-and-suspenders, but the +0x38 live-count check above is what generally
// catches abruptly-cancelled channel/toggle buffs.
//
// The buff manager layout is a contiguous array of 16-byte entries at
// `BuffManagerRuntime::BuffManagerOffset` inside each AIBaseClient. Each
// entry stores {buff*, aux*}; we only consume `buff*`.
// ============================================================================

#include "CoreRuntime.h"
#include "CoreObjectManager.h"
#include "Globals.h"
#include "offset.h"

#include <cstdint>
#include <cstddef>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>   // lstrcmpiA
#endif

namespace CoreBuffs {

    // Offset of the `char*` name field inside ScriptBaseBuff (buff + 0x10 -> script,
    // script + 0x8 -> char*). Kept local because Nightsharp `offset.h` does not yet
    // expose a ScriptBaseBuffLayout namespace.
    constexpr uintptr_t kScriptBaseNameOffset = 0x8;

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

        // Returns 0 for permanent buffs (endTime == 0) or when the buff has expired.
        float GetRemainingTime(float gameTime) const {
            const float endTime = GetEndTime();
            if (endTime <= 0.0f) {
                return 0.0f;   // permanent buff — no finite remaining time
            }
            return endTime > gameTime ? (endTime - gameTime) : 0.0f;
        }

        bool IsPermanent() const {
            return GetEndTime() <= 0.0f;
        }

        // 3-condition active check (see header comment for rationale).
        // Caller guarantees condition 1 by iterating only live entries; we verify
        // conditions 2 and 3 here.
        bool IsActive(float gameTime) const {
            if (!IsValid()) return false;

            const int liveCount = Globals::Read<int>(address + 0x38);
            if (liveCount <= 0 && GetStacks() <= 0) {
                return false;
            }

            const float endTime = GetEndTime();
            if (endTime <= 0.0f) {
                return true;   // permanent buff — always active when live count > 0
            }
            return endTime > gameTime;
        }

        bool ReadName(char* out, int maxOut) const {
            if (!out || maxOut <= 0 || !IsValid()) {
                if (out && maxOut > 0) out[0] = 0;
                return false;
            }

            // Chain: buff + BuffScriptPtr (0x10) -> ScriptBaseBuff*
            //        ScriptBaseBuff + 0x8        -> char* (raw C-string, NOT SSO)
            const auto scriptBase = Globals::Read<uintptr_t>(address + Offset::BuffDataLayout::BuffScriptPtr);
            if (!Globals::IsValidPtr(scriptBase)) {
                out[0] = 0;
                return false;
            }

            const auto charPtr = Globals::Read<uintptr_t>(scriptBase + kScriptBaseNameOffset);
            return Globals::ReadCString(charPtr, out, maxOut);
        }

        // Returns the caster's network ID by reading the first BuffScriptInstance
        // from the buff's stack array. IDA 13337: sub_91BC60 confirms that
        // BuffScriptInstance+0x4 stores the caster's network ID.
        // Returns 0 if the buff has no stack entries or is invalid.
        uint32_t GetCasterNetworkId() const {
            if (!IsValid()) return 0;

            const auto arrayBegin = Globals::Read<uintptr_t>(
                address + Offset::BuffDataLayout::BuffStackArrayBegin);
            if (!Globals::IsValidPtr(arrayBegin)) return 0;

            const auto arrayEnd = Globals::Read<uintptr_t>(
                address + Offset::BuffDataLayout::BuffStacks);
            if (!Globals::IsValidPtr(arrayEnd) || arrayEnd <= arrayBegin) return 0;

            // First entry: {BuffScriptInstance*, refcount*}
            const auto scriptInstance = Globals::Read<uintptr_t>(arrayBegin);
            if (!Globals::IsValidPtr(scriptInstance)) return 0;

            return Globals::Read<uint32_t>(
                scriptInstance + Offset::BuffScriptInstanceLayout::CasterNetworkId);
        }

        // Returns the native address of the caster GameObject by looking up
        // the caster's network ID in the ObjectManager.
        // Returns 0 if not found or invalid.
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

    // Walk `[EntriesStart, EntriesEnd)` writing each entry's `buff*` into `out`.
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

    inline float ResolveGameTime();

    inline bool IsRecallBuffName(const char* name) {
        return NameContainsInsensitive(name, "recall");
    }

    inline bool NameMatchesQuery(const char* buffName, const char* query) {
        if (StrEqualInsensitive(buffName, query)) {
            return true;
        }
        return IsRecallBuffName(query) && IsRecallBuffName(buffName);
    }

    inline void SetEventCacheEnabled(bool enabled) {
        EventCacheEnabled = enabled;
    }

    inline bool IsEventCacheEnabled() {
        return EventCacheEnabled;
    }

    inline void ClearEventCache() {
#if defined(_WIN32)
        CachedBuffExclusiveLock lock;
#endif
        for (auto& entry : CachedBuffEntries) {
            entry = {};
        }
        CachedBuffCursor = 0;
    }

    inline bool IsCachedBuffActive(const CachedBuffEntry& entry, float gameTime) {
        if (!entry.known || !entry.name[0]) {
            return false;
        }

        // The event cache goes stale when a buff is removed without a matching
        // OnBuffRemove hook — exactly what happens to abruptly-cancelled
        // channel/toggle buffs (Xerath R LocusOfPower2, recall). The game itself
        // zeroes the live count (buff+0x38) on removal, so re-derive liveness from
        // live memory whenever we still hold the buff address. The name match
        // guards against buff-slot reuse (the freed object recycled by another
        // buff). This keeps the cache fast (it still supplies the address, so we
        // skip the full BuffManager scan) while never reporting a removed buff.
        if (Globals::IsValidPtr(entry.address)) {
            BuffRef live{ entry.address };
            char buf[96] = {};
            if (live.ReadName(buf, static_cast<int>(sizeof(buf))) &&
                NameMatchesQuery(buf, entry.name)) {
                return live.IsActive(gameTime);
            }
        }

        // Address unknown or reused: fall back to the cached snapshot.
        if (entry.count <= 0) {
            return false;
        }
        if (entry.endTime <= 0.0f) {
            return true;
        }
        return entry.endTime > gameTime;
    }

    inline bool TryReadLiveBuffSnapshot(uintptr_t obj,
                                        const char* name,
                                        int eventCount,
                                        CachedBuffEntry& out) {
        if (!Globals::IsValidPtr(obj) || !name || !name[0]) {
            return false;
        }

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) {
                continue;
            }
            if (!NameMatchesQuery(buf, name)) {
                continue;
            }

            out = {};
            out.object = obj;
            out.address = buff.address;
            out.count = eventCount > 0 ? eventCount : buff.GetStacks();
            out.type = buff.GetType();
            out.startTime = buff.GetStartTime();
            out.endTime = buff.GetEndTime();
            out.known = true;
            strncpy_s(out.name, sizeof(out.name), buf, _TRUNCATE);
            return true;
        }
        return false;
    }

    inline bool TryReadBuffSnapshot(uintptr_t obj,
                                    uintptr_t buffAddress,
                                    const char* fallbackName,
                                    int eventCount,
                                    CachedBuffEntry& out) {
        if (!Globals::IsValidPtr(obj) || !Globals::IsValidPtr(buffAddress)) {
            return false;
        }

        BuffRef buff{ buffAddress };
        char buf[96] = {};
        if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) {
            if (!fallbackName || !fallbackName[0]) {
                return false;
            }
            strncpy_s(buf, sizeof(buf), fallbackName, _TRUNCATE);
        }

        out = {};
        out.object = obj;
        out.address = buff.address;
        out.count = eventCount > 0 ? eventCount : buff.GetStacks();
        out.type = buff.GetType();
        out.startTime = buff.GetStartTime();
        out.endTime = buff.GetEndTime();
        out.known = true;
        strncpy_s(out.name, sizeof(out.name), buf, _TRUNCATE);
        return out.name[0] != 0;
    }

    inline int FindCachedBuffIndexLocked(uintptr_t obj, const char* name) {
        if (!Globals::IsValidPtr(obj) || !name || !name[0]) {
            return -1;
        }

        for (int i = 0; i < kMaxCachedBuffEntries; ++i) {
            const auto& entry = CachedBuffEntries[i];
            if (!entry.known || entry.object != obj || !entry.name[0]) {
                continue;
            }
            if (NameMatchesQuery(entry.name, name)) {
                return i;
            }
        }
        return -1;
    }

    inline int FindFreeCachedBuffIndexLocked() {
        for (int i = 0; i < kMaxCachedBuffEntries; ++i) {
            if (!CachedBuffEntries[i].known) {
                return i;
            }
        }
        return CachedBuffCursor++ % kMaxCachedBuffEntries;
    }

    inline bool TryGetCachedBuff(uintptr_t obj,
                                 const char* name,
                                 CachedBuffEntry& out);

    inline void StoreCachedBuff(const CachedBuffEntry& value) {
        if (!Globals::IsValidPtr(value.object) || !value.name[0]) {
            return;
        }

#if defined(_WIN32)
        CachedBuffExclusiveLock lock;
#endif
        int index = FindCachedBuffIndexLocked(value.object, value.name);
        if (index < 0) {
            index = FindFreeCachedBuffIndexLocked();
        }
        CachedBuffEntries[index] = value;
        CachedBuffEntries[index].known = true;
    }

    inline void ApplyBuffEvent(uintptr_t obj,
                               const char* name,
                               int count,
                               bool forceInactive,
                               uintptr_t buffAddress = 0) {
        if (!EventCacheEnabled || !Globals::IsValidPtr(obj) || !name || !name[0]) {
            return;
        }

        CachedBuffEntry entry = {};
        if (!forceInactive && count > 0 &&
            TryReadBuffSnapshot(obj, buffAddress, name, count, entry)) {
            StoreCachedBuff(entry);
            return;
        }

        if (!forceInactive && count > 0 &&
            TryReadLiveBuffSnapshot(obj, name, count, entry)) {
            StoreCachedBuff(entry);
            return;
        }

        entry.object = obj;
        entry.count = forceInactive ? 0 : count;
        entry.known = true;
        strncpy_s(entry.name, sizeof(entry.name), name, _TRUNCATE);
        StoreCachedBuff(entry);
    }

    inline void ApplyBuffAddEvent(uintptr_t obj, const char* name, int count = 1, uintptr_t buffAddress = 0) {
        ApplyBuffEvent(obj, name, count > 0 ? count : 1, false, buffAddress);
    }

    inline void ApplyBuffRemoveEvent(uintptr_t obj, const char* name, uintptr_t buffAddress = 0) {
        ApplyBuffEvent(obj, name, 0, true, buffAddress);
    }

    inline void ApplyBuffUpdateEvent(uintptr_t obj, const char* name, int count, uintptr_t buffAddress = 0) {
        ApplyBuffEvent(obj, name, count, count <= 0, buffAddress);
    }

    inline bool TryGetCachedBuff(uintptr_t obj,
                                 const char* name,
                                 CachedBuffEntry& out) {
        if (!EventCacheEnabled || !Globals::IsValidPtr(obj) || !name || !name[0]) {
            return false;
        }

#if defined(_WIN32)
        CachedBuffSharedLock lock;
#endif
        const int index = FindCachedBuffIndexLocked(obj, name);
        if (index < 0) {
            return false;
        }
        out = CachedBuffEntries[index];
        return true;
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

    // ── Query API ──

    // Raw API: returns true if a buff matching `name` exists in the list at all
    // (does NOT verify active state). Prefer `HasBuff` for gameplay logic.
    inline bool HasBuffRaw(uintptr_t obj, const char* name) {
        if (!name || !name[0]) return false;

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
            if (NameMatchesQuery(buf, name)) return true;
        }
        return false;
    }

    inline bool HasBuff(uintptr_t obj, const char* name) {
        if (!name || !name[0]) return false;

        const float gameTime = ResolveGameTime();
        CachedBuffEntry cached = {};
        if (TryGetCachedBuff(obj, name, cached)) {
            return IsCachedBuffActive(cached, gameTime) &&
                   !IsSuppressedByLiveState(obj, cached.name);
        }

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.IsActive(gameTime)) continue;
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
            if (IsSuppressedByLiveState(obj, buf)) continue;
            if (NameMatchesQuery(buf, name)) return true;
        }
        return false;
    }

    // Explicit active API for callers that already have a gameTime snapshot.
    inline bool HasActiveBuff(uintptr_t obj, const char* name, float gameTime) {
        if (!name || !name[0]) return false;

        CachedBuffEntry cached = {};
        if (TryGetCachedBuff(obj, name, cached)) {
            return IsCachedBuffActive(cached, gameTime) &&
                   !IsSuppressedByLiveState(obj, cached.name);
        }

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.IsActive(gameTime)) continue;
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
            if (IsSuppressedByLiveState(obj, buf)) continue;
            if (NameMatchesQuery(buf, name)) return true;
        }
        return false;
    }

    inline bool HasBuffContaining(uintptr_t obj, const char* token, int requiredType = -1) {
        if (!token || !token[0]) return false;

        const float gameTime = ResolveGameTime();
        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.IsActive(gameTime)) continue;
            if (requiredType >= 0 && buff.GetType() != requiredType) continue;
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
            if (IsSuppressedByLiveState(obj, buf)) continue;
            if (NameContainsInsensitive(buf, token)) return true;
        }
        return false;
    }

    inline float ResolveGameTime() {
        const auto& ctx = CoreRuntime::GetContext();
        if (!ctx.moduleBase) {
            return 0.0f;
        }
        return Globals::Read<float>(ctx.moduleBase + Offset::GameRuntime::GameTime);
    }

    inline bool HasActiveBuffContaining(uintptr_t obj, const char* token, int requiredType = -1, float gameTime = -1.0f) {
        if (!token || !token[0]) return false;
        if (gameTime < 0.0f) {
            gameTime = ResolveGameTime();
        }

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.IsActive(gameTime)) continue;
            if (requiredType >= 0 && buff.GetType() != requiredType) continue;
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
            if (IsSuppressedByLiveState(obj, buf)) continue;
            if (NameContainsInsensitive(buf, token)) return true;
        }
        return false;
    }

    inline int Count(uintptr_t obj) {
        uintptr_t buffs[256] = {};
        return Enumerate(obj, buffs, static_cast<int>(sizeof(buffs) / sizeof(buffs[0])));
    }

    inline BuffRef FindRawByName(uintptr_t obj, const char* name) {
        if (!name || !name[0]) return {};

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
            if (NameMatchesQuery(buf, name)) return buff;
        }
        return {};
    }

    inline BuffRef FindByName(uintptr_t obj, const char* name) {
        if (!name || !name[0]) return {};

        const float gameTime = ResolveGameTime();
        CachedBuffEntry cached = {};
        if (TryGetCachedBuff(obj, name, cached)) {
            if (!IsCachedBuffActive(cached, gameTime) ||
                IsSuppressedByLiveState(obj, cached.name)) {
                return {};
            }
            if (Globals::IsValidPtr(cached.address)) {
                return BuffRef{ cached.address };
            }
        }

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.IsActive(gameTime)) continue;
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
            if (IsSuppressedByLiveState(obj, buf)) continue;
            if (NameMatchesQuery(buf, name)) return buff;
        }
        return {};
    }

    // Active-filtered lookup — returns {} for expired or zero-stack matches.
    inline BuffRef FindActiveByName(uintptr_t obj, const char* name, float gameTime) {
        if (!name || !name[0]) return {};

        CachedBuffEntry cached = {};
        if (TryGetCachedBuff(obj, name, cached)) {
            if (!IsCachedBuffActive(cached, gameTime) ||
                IsSuppressedByLiveState(obj, cached.name)) {
                return {};
            }
            if (Globals::IsValidPtr(cached.address)) {
                return BuffRef{ cached.address };
            }
        }

        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        char buf[96] = {};
        for (int i = 0; i < count; ++i) {
            BuffRef buff{ buffs[i] };
            if (!buff.IsActive(gameTime)) continue;
            if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
            if (IsSuppressedByLiveState(obj, buf)) continue;
            if (NameMatchesQuery(buf, name)) return buff;
        }
        return {};
    }

    // The event cache stores the stack count captured at the last buff EVENT
    // (add/update). For buffs whose count changes WITHOUT a matching update
    // event — Kalista spears (kalistaexpungemarker), Dead Man's Plate momentum,
    // Kai'Sa/Kindred markers, etc. — that snapshot goes stale (typically frozen
    // at the add-time value of 1), which corrupts damage calculations that scale
    // with stacks. Verified live (ReClass, League 26.x): a dummy carrying 5 rend
    // spears has the LIVE count at buff+0x38 == 5 while the cached event count
    // was still 1. IsCachedBuffActive already re-derives LIVENESS from live
    // memory whenever we still hold the buff address; do the same for the COUNT
    // so callers always see the true stack value. Falls back to a live by-name
    // scan, then finally the cached snapshot, so it is never worse than before.
    inline int ResolveLiveStacks(uintptr_t obj, const CachedBuffEntry& entry, float gameTime) {
        if (Globals::IsValidPtr(entry.address)) {
            BuffRef live{ entry.address };
            char buf[96] = {};
            if (live.ReadName(buf, static_cast<int>(sizeof(buf))) &&
                NameMatchesQuery(buf, entry.name)) {
                return live.GetStacks();
            }
        }

        const auto buff = FindActiveByName(obj, entry.name, gameTime);
        if (buff.IsValid()) {
            return buff.GetStacks();
        }
        return entry.count;
    }

    inline int GetBuffStacks(uintptr_t obj, const char* name) {
        const float gameTime = ResolveGameTime();
        CachedBuffEntry cached = {};
        if (TryGetCachedBuff(obj, name, cached)) {
            if (!IsCachedBuffActive(cached, gameTime) ||
                IsSuppressedByLiveState(obj, cached.name)) {
                return 0;
            }
            return ResolveLiveStacks(obj, cached, gameTime);
        }

        const auto buff = FindByName(obj, name);
        return buff.IsValid() ? buff.GetStacks() : 0;
    }

    inline int GetActiveBuffStacks(uintptr_t obj, const char* name, float gameTime) {
        CachedBuffEntry cached = {};
        if (TryGetCachedBuff(obj, name, cached)) {
            if (!IsCachedBuffActive(cached, gameTime) ||
                IsSuppressedByLiveState(obj, cached.name)) {
                return 0;
            }
            return ResolveLiveStacks(obj, cached, gameTime);
        }

        const auto buff = FindActiveByName(obj, name, gameTime);
        return buff.IsValid() ? buff.GetStacks() : 0;
    }

    inline float GetBuffRemainingTime(uintptr_t obj, const char* name, float gameTime) {
        CachedBuffEntry cached = {};
        if (TryGetCachedBuff(obj, name, cached)) {
            if (!IsCachedBuffActive(cached, gameTime) ||
                IsSuppressedByLiveState(obj, cached.name)) {
                return 0.0f;
            }
            if (cached.endTime <= 0.0f) {
                return 0.0f;
            }
            return cached.endTime > gameTime ? (cached.endTime - gameTime) : 0.0f;
        }

        const auto buff = FindActiveByName(obj, name, gameTime);
        return buff.IsValid() ? buff.GetRemainingTime(gameTime) : 0.0f;
    }

    inline bool HasBuffType(uintptr_t obj, int type) {
        const float gameTime = ResolveGameTime();
        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        for (int i = 0; i < count; ++i) {
            const BuffRef buff{ buffs[i] };
            if (!buff.IsActive(gameTime)) continue;
            if (buff.GetType() != type) continue;
            char buf[96] = {};
            if (buff.ReadName(buf, static_cast<int>(sizeof(buf))) &&
                IsSuppressedByLiveState(obj, buf)) {
                continue;
            }
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
        uintptr_t buffs[256] = {};
        const int count = Enumerate(obj, buffs, 256);
        for (int i = 0; i < count; ++i) {
            const BuffRef buff{ buffs[i] };
            if (!buff.IsActive(gameTime)) continue;
            if (buff.GetType() != type) continue;
            char buf[96] = {};
            if (buff.ReadName(buf, static_cast<int>(sizeof(buf))) &&
                IsSuppressedByLiveState(obj, buf)) {
                continue;
            }
            return true;
        }
        return false;
    }

} // namespace CoreBuffs
