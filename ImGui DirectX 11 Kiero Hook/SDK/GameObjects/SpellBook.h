#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "Enums.h"
#include "Game.h"
#include <string>

// ============================================================================
// SpellBook / SpellSlot / SpellData — Spell system wrappers
// Reference: EnsoulSharp.SDK Spell.cs + Script-New-main/SDK/Spell.h
// ============================================================================

namespace SDK {

    // ========================================================================
    // SpellData — Static spell information (name, mana cost)
    // ========================================================================
    class SpellData {
    public:
        uintptr_t address;

        SpellData() : address(0) {}
        SpellData(uintptr_t addr) : address(addr) {}
        bool IsValid() const { return Globals::IsValidPtr(address); }

        std::string GetName() const {
            if (!IsValid()) return "";
            char buf[128] = {};
            if (!Globals::ReadGameString(address + Offset::SpellBook::DataSpellName, buf, sizeof(buf)))
                return "";
            return std::string(buf);
        }

        float GetManaCost() const {
            if (!IsValid()) return 0.0f;
            return Globals::Read<float>(address + Offset::SpellBook::DataManaCost);
        }

        // ====================================================================
        // SpellDataResource access (SpellData + 0x60 → SpellDataResource ptr)
        // ====================================================================
    private:
        uintptr_t GetResource() const {
            if (!IsValid()) return 0;
            return Globals::Read<uintptr_t>(address + Offset::SpellBook::DataResourceBase);
        }

    public:
        // Cast range per rank (array of 7 floats)
        float GetCastRange(int rank = 0) const {
            uintptr_t res = GetResource();
            if (!Globals::IsValidPtr(res)) return 0.0f;
            if (rank < 0) rank = 0;
            if (rank > 6) rank = 6;
            return Globals::Read<float>(res + Offset::SpellBook::ResCastRange + rank * sizeof(float));
        }

        // Missile speed
        float GetMissileSpeed() const {
            uintptr_t res = GetResource();
            if (!Globals::IsValidPtr(res)) return 0.0f;
            return Globals::Read<float>(res + Offset::SpellBook::ResMissileSpeed);
        }

        // Line width / radius
        float GetLineWidth() const {
            uintptr_t res = GetResource();
            if (!Globals::IsValidPtr(res)) return 0.0f;
            return Globals::Read<float>(res + Offset::SpellBook::ResLineWidth);
        }

        // Cooldown per rank (array of 7 floats)
        float GetCooldown(int rank = 0) const {
            uintptr_t res = GetResource();
            if (!Globals::IsValidPtr(res)) return 0.0f;
            if (rank < 0) rank = 0;
            if (rank > 6) rank = 6;
            return Globals::Read<float>(res + Offset::SpellBook::ResCooldownTime + rank * sizeof(float));
        }

        // Max ammo per rank
        int GetMaxAmmo(int rank = 0) const {
            uintptr_t res = GetResource();
            if (!Globals::IsValidPtr(res)) return 0;
            if (rank < 0) rank = 0;
            if (rank > 6) rank = 6;
            return Globals::Read<int>(res + Offset::SpellBook::ResMaxAmmo + rank * sizeof(int));
        }

        // Cast type (targeting type enum)
        int GetCastType() const {
            uintptr_t res = GetResource();
            if (!Globals::IsValidPtr(res)) return 0;
            return Globals::Read<int>(res + Offset::SpellBook::ResCastType);
        }

        // Script name
        std::string GetScriptName() const {
            uintptr_t res = GetResource();
            if (!Globals::IsValidPtr(res)) return "";
            char buf[128] = {};
            if (!Globals::ReadGameString(res + Offset::SpellBook::ResScriptName, buf, sizeof(buf)))
                return "";
            return std::string(buf);
        }

        // Icon name
        std::string GetIconName() const {
            uintptr_t res = GetResource();
            if (!Globals::IsValidPtr(res)) return "";
            char buf[128] = {};
            if (!Globals::ReadGameString(res + Offset::SpellBook::ResImgIconName, buf, sizeof(buf)))
                return "";
            return std::string(buf);
        }
    };

    // ========================================================================
    // SpellInfo — Spell metadata (points to SpellData)
    // ========================================================================
    class SpellInfo {
    public:
        uintptr_t address;

        SpellInfo() : address(0) {}
        SpellInfo(uintptr_t addr) : address(addr) {}
        bool IsValid() const { return Globals::IsValidPtr(address); }

        SpellData GetSpellData() const {
            if (!IsValid()) return SpellData();
            uintptr_t ptr = Globals::Read<uintptr_t>(address + Offset::SpellBook::InfoSpellData);
            return SpellData(ptr);
        }

        std::string GetName() const {
            SpellData data = GetSpellData();
            return data.GetName();
        }
    };

    // ========================================================================
    // SpellSlot — Individual spell slot (Q/W/E/R/D/F)
    // ========================================================================
    class SpellSlot {
    public:
        uintptr_t address;

        SpellSlot() : address(0) {}
        SpellSlot(uintptr_t addr) : address(addr) {}
        bool IsValid() const { return Globals::IsValidPtr(address); }

        // Level (0 = not learned, 1-5 = spell level)
        int GetLevel() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::SpellBook::SlotLevel);
        }

        // Cooldown as absolute game time when spell becomes ready
        float GetReadyAt() const {
            if (!IsValid()) return 0.0f;
            return Globals::Read<float>(address + Offset::SpellBook::SlotCooldown);
        }

        // Remaining cooldown in seconds
        float GetRemainingCooldown() const {
            float remaining = GetReadyAt() - Game::GetTime();
            return remaining > 0.0f ? remaining : 0.0f;
        }

        // Is spell ready? (learned + off cooldown)
        bool IsReady() const {
            if (!IsValid()) return false;
            if (GetLevel() <= 0) return false;
            return GetRemainingCooldown() <= 0.0f;
        }

        // Total cooldown duration
        float GetTotalCooldown() const {
            if (!IsValid()) return 0.0f;
            return Globals::Read<float>(address + Offset::SpellBook::SlotTotalCd);
        }

        // Stacks/charges
        int GetStacks() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::SpellBook::SlotStacks);
        }

        // SpellInfo → SpellData → Name
        SpellInfo GetSpellInfo() const {
            if (!IsValid()) return SpellInfo();
            uintptr_t ptr = Globals::Read<uintptr_t>(address + Offset::SpellBook::SlotSpellInfo);
            return SpellInfo(ptr);
        }

        std::string GetName() const {
            return GetSpellInfo().GetName();
        }
    };

    // ========================================================================
    // SpellBook — Contains all spell slots for a GameObject
    // ========================================================================
    class SpellBook {
    public:
        uintptr_t base; // gameObject + SpellBook::Offset

        SpellBook() : base(0) {}
        SpellBook(uintptr_t gameObjAddr)
            : base(gameObjAddr + Offset::SpellBook::Offset) {}

        bool IsValid() const { return Globals::IsValidPtr(base); }

        // Get a specific spell slot
        SpellSlot GetSpell(SpellSlotId slot) const {
            int idx = (int)slot;
            if (idx < 0 || idx > 13) return SpellSlot();
            uintptr_t slotPtr = Globals::Read<uintptr_t>(
                base + Offset::SpellBook::SpellSlotArray + idx * 8);
            return SpellSlot(slotPtr);
        }

        // Convenience accessors
        SpellSlot Q() const { return GetSpell(SpellSlotId::Q); }
        SpellSlot W() const { return GetSpell(SpellSlotId::W); }
        SpellSlot E() const { return GetSpell(SpellSlotId::E); }
        SpellSlot R() const { return GetSpell(SpellSlotId::R); }
        SpellSlot D() const { return GetSpell(SpellSlotId::Summoner1); }
        SpellSlot F() const { return GetSpell(SpellSlotId::Summoner2); }

        // Quick checks
        bool IsReady(SpellSlotId slot) const {
            return GetSpell(slot).IsReady();
        }

        // Active spell cast
        uintptr_t GetActiveSpellCast() const {
            return Globals::Read<uintptr_t>(base + 0x38); // SpellBook + 0x38
        }

        bool IsCasting() const {
            return Globals::IsValidPtr(GetActiveSpellCast());
        }
    };

} // namespace SDK
