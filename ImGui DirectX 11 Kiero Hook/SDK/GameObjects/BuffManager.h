#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "Enums.h"
#include "Game.h"
#include <string>
#include <functional>

// ============================================================================
// BuffManager — Buff tracking system
// Reference: Script-New-main/SDK/BuffManager.h
// ============================================================================

namespace SDK {

    // ========================================================================
    // Buff — Single buff instance
    // ========================================================================
    class Buff {
    public:
        uintptr_t address;

        Buff() : address(0) {}
        Buff(uintptr_t addr) : address(addr) {}
        bool IsValid() const { return Globals::IsValidPtr(address); }

        // Buff name via BuffScript → name (SEH-safe helper)
        std::string GetName() const {
            if (!IsValid()) return "";
            char buf[64] = {};
            if (!ReadBuffNameRaw(address, buf, sizeof(buf)))
                return "";
            return std::string(buf);
        }

    private:
        static bool ReadBuffNameRaw(uintptr_t addr, char* out, int maxLen) {
            __try {
                uintptr_t scriptPtr = *(uintptr_t*)(addr + Offset::BuffManager::BuffNamePtr);
                if (scriptPtr < 0x10000 || scriptPtr > 0x7FFFFFFFFFFF) return false;
                uintptr_t namePtr = *(uintptr_t*)(scriptPtr + Offset::BuffManager::BuffNameStr);
                if (namePtr < 0x10000 || namePtr > 0x7FFFFFFFFFFF) return false;
                for (int i = 0; i < maxLen - 1; i++) {
                    out[i] = *(char*)(namePtr + i);
                    if (out[i] == 0) break;
                }
                out[maxLen - 1] = 0;
                return out[0] != 0;
            } __except(1) { out[0] = 0; return false; }
        }
    public:

        BuffType GetType() const {
            if (!IsValid()) return BuffType::Internal;
            return (BuffType)Globals::Read<int>(address + Offset::BuffManager::BuffType);
        }

        float GetStartTime() const {
            if (!IsValid()) return 0.0f;
            return Globals::Read<float>(address + Offset::BuffManager::BuffStartTime);
        }

        float GetEndTime() const {
            if (!IsValid()) return 0.0f;
            return Globals::Read<float>(address + Offset::BuffManager::BuffEndTime);
        }

        float GetRemainingTime() const {
            float end = GetEndTime();
            float now = Game::GetTime();
            return end > now ? end - now : 0.0f;
        }

        bool IsActive() const {
            return IsValid() && GetEndTime() > Game::GetTime();
        }

        int GetStacks() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::BuffManager::BuffStacks);
        }

        int GetStacksAlt() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::BuffManager::BuffStacksAlt);
        }
    };

    // ========================================================================
    // BuffManager — Manages all buffs on a game object
    // ========================================================================
    class BuffManager {
    public:
        uintptr_t base; // gameObject + BuffManager::Offset

        BuffManager() : base(0) {}
        BuffManager(uintptr_t gameObjAddr)
            : base(gameObjAddr + Offset::BuffManager::Offset) {}

        bool IsValid() const { return Globals::IsValidPtr(base); }

        // Iterate over all active buffs
        void ForEach(const std::function<void(Buff&)>& callback) const {
            if (!IsValid()) return;
            // Collect buff addresses with SEH, then iterate outside __try
            uintptr_t addrs[256] = {};
            int count = CollectBuffAddrs(base, addrs, 256);
            for (int i = 0; i < count; i++) {
                Buff buff(addrs[i]);
                if (buff.IsActive()) {
                    callback(buff);
                }
            }
        }

    private:
        // SEH-safe: collect buff addresses into raw array (no C++ objects)
        static int CollectBuffAddrs(uintptr_t bmBase, uintptr_t* out, int maxOut) {
            __try {
                uintptr_t arrayStart = *(uintptr_t*)(bmBase);
                uintptr_t arrayEnd   = *(uintptr_t*)(bmBase + Offset::BuffManager::EntriesEnd);
                if (arrayStart < 0x10000 || arrayEnd < 0x10000) return 0;
                if (arrayEnd <= arrayStart) return 0;

                int count = (int)((arrayEnd - arrayStart) / 8);
                if (count <= 0 || count > maxOut) return 0;

                int valid = 0;
                for (int i = 0; i < count && valid < maxOut; i++) {
                    uintptr_t entry = *(uintptr_t*)(arrayStart + i * 8);
                    if (entry < 0x10000) continue;
                    uintptr_t buffPtr = *(uintptr_t*)(entry + Offset::BuffManager::EntryBuff);
                    if (buffPtr < 0x10000) continue;
                    out[valid++] = buffPtr;
                }
                return valid;
            } __except(1) { return 0; }
        }
    public:

        // Check if a specific buff exists (by name, case-insensitive)
        bool HasBuff(const char* name) const {
            bool found = false;
            ForEach([&](Buff& buff) {
                if (found) return;
                std::string bname = buff.GetName();
                if (_stricmp(bname.c_str(), name) == 0)
                    found = true;
            });
            return found;
        }

        // Get remaining time of a specific buff
        float GetBuffRemainingTime(const char* name) const {
            float time = 0.0f;
            ForEach([&](Buff& buff) {
                if (time > 0.0f) return;
                std::string bname = buff.GetName();
                if (_stricmp(bname.c_str(), name) == 0)
                    time = buff.GetRemainingTime();
            });
            return time;
        }

        // Get stack count of a specific buff
        int GetBuffStacks(const char* name) const {
            int stacks = 0;
            ForEach([&](Buff& buff) {
                if (stacks > 0) return;
                std::string bname = buff.GetName();
                if (_stricmp(bname.c_str(), name) == 0)
                    stacks = buff.GetStacks();
            });
            return stacks;
        }

        // Check for CC types
        bool IsStunned() const { return HasBuffOfType(BuffType::Stun); }
        bool IsSilenced() const { return HasBuffOfType(BuffType::Silence); }
        bool IsCharmed() const { return HasBuffOfType(BuffType::Charm); }
        bool IsFeared() const { return HasBuffOfType(BuffType::Fear); }
        bool IsSuppressed() const { return HasBuffOfType(BuffType::Suppression); }
        bool IsSnared() const { return HasBuffOfType(BuffType::Snare); }
        bool IsSlowed() const { return HasBuffOfType(BuffType::Slow); }
        bool IsAsleep() const { return HasBuffOfType(BuffType::Asleep); }
        bool IsGrounded() const { return HasBuffOfType(BuffType::Grounded); }

        bool IsImmobile() const {
            return IsStunned() || IsCharmed() || IsFeared() ||
                   IsSuppressed() || IsSnared() || IsAsleep();
        }

    public:
        bool HasBuffOfType(BuffType type) const {
            bool found = false;
            ForEach([&](Buff& buff) {
                if (found) return;
                if (buff.GetType() == type)
                    found = true;
            });
            return found;
        }
    };

} // namespace SDK
