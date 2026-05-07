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
// Active conditions (all three must hold):
//   1. `entry` must still be inside `[EntriesStart, EntriesEnd)`
//      during iteration — expired buffs are slid out of the range)
//   2. `stacks > 0` (or `stacksAlt > 0` as fallback for single-instance buffs)
//   3. `endTime == 0` (permanent buff) OR `endTime > gameTime`
//
// Channel buffs such as Recall can stay in BuffManager after cancel with
// non-zero stacks and the original future endTime. Public HasBuff() therefore
// also checks a live state source for those known cases.
//
// The buff manager layout is a contiguous array of 16-byte entries at
// `BuffManagerRuntime::BuffManagerOffset` inside each AIBaseClient. Each
// entry stores {buff*, aux*}; we only consume `buff*`.
// ============================================================================

#include "CoreRuntime.h"
#include "Globals.h"
#include "offset.h"

#include <cstdint>
#include <cstddef>

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
            const int stacks = Globals::Read<int>(address + Offset::BuffDataLayout::BuffStacks);
            if (stacks > 0) return stacks;
            return Globals::Read<int>(address + Offset::BuffDataLayout::BuffStacksAlt);
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
            if (GetStacks() <= 0) return false;

            const float endTime = GetEndTime();
            if (endTime <= 0.0f) {
                return true;   // permanent buff — always active when stacks > 0
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
            if (!Globals::IsValidPtr(charPtr)) {
                out[0] = 0;
                return false;
            }
            return Globals::ReadCString(charPtr, out, maxOut);
        }
    };

    // ── Manager accessors ──

    inline uintptr_t GetBuffManager(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return 0;
        return obj + Offset::BuffManagerRuntime::BuffManagerOffset;
    }

    // Walk `[EntriesStart, EntriesEnd)` writing each entry's `buff*` into `out`.
    // Condition 1 of the active-check is implicitly applied here: the game
    // maintains the entries array so only live buffs are inside the range.
    inline int Enumerate(uintptr_t obj, uintptr_t* out, int maxOut) {
        if (!out || maxOut <= 0) return 0;

        const auto manager = GetBuffManager(obj);
        if (!Globals::IsValidPtr(manager)) return 0;

        const auto begin = Globals::Read<uintptr_t>(manager + Offset::BuffManagerLayout::EntriesStart);
        const auto end   = Globals::Read<uintptr_t>(manager + Offset::BuffManagerLayout::EntriesEnd);
        if (!Globals::IsValidPtr(begin) || !Globals::IsValidPtr(end) || end < begin) {
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

    inline int GetBuffStacks(uintptr_t obj, const char* name) {
        const auto buff = FindByName(obj, name);
        return buff.IsValid() ? buff.GetStacks() : 0;
    }

    inline int GetActiveBuffStacks(uintptr_t obj, const char* name, float gameTime) {
        const auto buff = FindActiveByName(obj, name, gameTime);
        return buff.IsValid() ? buff.GetStacks() : 0;
    }

    inline float GetBuffRemainingTime(uintptr_t obj, const char* name, float gameTime) {
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
