#pragma once

#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"

#include <cstdint>
#include <cstddef>

namespace CoreSpellBook {

    enum SpellSlotId : int {
        Slot_Q         = 0,
        Slot_W         = 1,
        Slot_E         = 2,
        Slot_R         = 3,
        Slot_Summoner1 = 4,
        Slot_Summoner2 = 5,
        Slot_Item1     = 6,
        Slot_Item2     = 7,
        Slot_Item3     = 8,
        Slot_Item4     = 9,
        Slot_Item5     = 10,
        Slot_Item6     = 11,
        Slot_Trinket   = 12,
        Slot_Recall    = 13,
        Slot_Other     = 14
    };

    enum SpellState : int {
        State_Unknown    = -1,
        State_Ready      = 0,
        State_NotLearned = 1,
        State_Cooldown   = 2,
        State_NoMana     = 3,
        State_Disabled   = 4
    };

    struct SlotRef {
        uintptr_t address = 0;

        static int ClampRank(int rank) {
            if (rank < 0) return 0;
            if (rank > 6) return 6;
            return rank;
        }

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        int GetLevel() const {
            return Globals::Read<int>(address + Offset::SpellSlotLayout::SlotLevel);
        }

        int GetLevelAlt() const {
            return Globals::Read<int>(address + Offset::SpellSlotLayout::SlotLevelAlt);
        }

        float GetReadyAt() const {
            return Globals::Read<float>(address + Offset::SpellSlotLayout::SlotCooldown);
        }

        float GetCooldown() const {
            return GetReadyAt();
        }

        float GetTotalCooldown() const {
            return Globals::Read<float>(address + Offset::SpellSlotLayout::SlotTotalCd);
        }

        float GetCooldownExpires() const {
            return Globals::Read<float>(address + Offset::SpellSlotLayout::SlotCooldownExpires);
        }

        float GetChargeTimer() const {
            return Globals::Read<float>(address + Offset::SpellSlotLayout::SlotChargeTimer);
        }

        int GetStacks() const {
            return Globals::Read<int>(address + Offset::SpellSlotLayout::SlotStacks);
        }

        uintptr_t GetSlotActiveSpellCast() const {
            return Globals::Read<uintptr_t>(address + Offset::SpellSlotLayout::SlotActiveSpellCast);
        }

        bool IsSlotCasting() const {
            return Globals::IsValidPtr(GetSlotActiveSpellCast());
        }

        uint32_t GetSpellNameHash() const {
            return Globals::Read<uint32_t>(address + Offset::SpellSlotLayout::SlotSpellNameHash);
        }

        uintptr_t GetSpellInstanceVars() const {
            return Globals::Read<uintptr_t>(address + Offset::SpellSlotLayout::SlotSpellInstanceVars);
        }

        // Alternative spell-name reader — walks SpellDataResource name SSO instead of DataSpellName.
        bool ReadSpellNameFromResource(char* out, int maxOut) const {
            if (!out || maxOut <= 0) { return false; }
            out[0] = 0;

            const auto sdr = Globals::Read<uintptr_t>(address + Offset::SpellSlotLayout::SlotSpellInput);
            if (!Globals::IsValidPtr(sdr)) {
                return false;
            }

            const auto cap = Globals::Read<size_t>(sdr + Offset::SpellDataResourceNameLayout::SpellNameCap);
            if (cap > 0xF) {
                const auto heapPtr = Globals::Read<uintptr_t>(sdr + Offset::SpellDataResourceNameLayout::SpellNameStr);
                if (!Globals::IsValidPtr(heapPtr)) {
                    return false;
                }
                return Globals::ReadCString(heapPtr, out, maxOut);
            }
            return Globals::ReadCString(sdr + Offset::SpellDataResourceNameLayout::SpellNameStr, out, maxOut);
        }

        uintptr_t GetSpellInput() const {
            return Globals::Read<uintptr_t>(address + Offset::SpellSlotLayout::SlotSpellInput);
        }

        bool HasSpellInput() const {
            return Globals::IsValidPtr(GetSpellInput());
        }

        uintptr_t GetSpellInfo() const {
            return Globals::Read<uintptr_t>(address + Offset::SpellSlotLayout::SlotSpellInfo);
        }

        bool HasSpellInfo() const {
            return Globals::IsValidPtr(GetSpellInfo());
        }

        uintptr_t GetSpellData() const {
            const auto info = GetSpellInfo();
            if (!Globals::IsValidPtr(info)) return 0;
            return Globals::Read<uintptr_t>(info + Offset::SpellInfoLayout::InfoSpellData);
        }

        uintptr_t GetSpellResource() const {
            const auto data = GetSpellData();
            if (!Globals::IsValidPtr(data)) return 0;
            return Globals::Read<uintptr_t>(data + Offset::SpellDataLayout::DataResource);
        }

        float GetManaCost() const {
            const auto data = GetSpellData();
            if (!Globals::IsValidPtr(data)) return 0.0f;
            return Globals::Read<float>(data + Offset::SpellDataLayout::DataManaCost);
        }

        float GetAmmoRechargeTime() const {
            const auto data = GetSpellData();
            if (!Globals::IsValidPtr(data)) return 0.0f;
            return Globals::Read<float>(data + Offset::SpellDataResourceLayout::ResAmmoRecharge);
        }

        float GetBaseCooldownTime(int rank = 0) const {
            const auto data = GetSpellData();
            if (!Globals::IsValidPtr(data)) return 0.0f;
            const auto safeRank = ClampRank(rank);
            return Globals::Read<float>(data + Offset::SpellDataResourceLayout::ResCooldownTime +
                                        static_cast<uintptr_t>(safeRank * sizeof(float)));
        }

        float GetRemainingCooldown(float gameTime) const {
            const float remaining = GetReadyAt() - gameTime;
            return remaining > 0.0f ? remaining : 0.0f;
        }

        bool IsOnCooldown(float gameTime) const {
            return GetRemainingCooldown(gameTime) > 0.0f;
        }

        bool IsLearned() const {
            return IsValid() && GetLevel() > 0;
        }

        bool IsReady(float gameTime) const {
            return IsLearned() && GetRemainingCooldown(gameTime) <= 0.0f;
        }

        uintptr_t GetCastArgument() const {
            const auto input = GetSpellInput();
            return Globals::IsValidPtr(input) ? (input + 0x8) : 0;
        }

        uint32_t GetInputTargetNetId() const {
            const auto input = GetSpellInput();
            if (!Globals::IsValidPtr(input)) return 0;
            return Globals::Read<uint32_t>(input + Offset::SpellInputLayout::InputTargetNetId);
        }

        Vec3 GetInputStartPos() const {
            const auto input = GetSpellInput();
            if (!Globals::IsValidPtr(input)) return {};
            return Globals::Read<Vec3>(input + Offset::SpellInputLayout::InputStartPos);
        }

        Vec3 GetInputEndPos() const {
            const auto input = GetSpellInput();
            if (!Globals::IsValidPtr(input)) return {};
            return Globals::Read<Vec3>(input + Offset::SpellInputLayout::InputEndPos);
        }

        // NOTE (preserved from old NightSharp): the mutable spell input actually
        // lives inside the SpellInfo struct (base = SlotSpellInfo value), NOT the
        // SlotSpellInput pointer. The name `SpellInput*` here is kept for parity
        // with the legacy API so downstream call sites don't need refactoring.
        bool SetInputData(uint32_t targetNetId, const Vec3& start, const Vec3& end) const {
            const auto input = GetSpellInfo();
            if (!Globals::IsValidPtr(input)) {
                return false;
            }

            bool ok = true;
            ok &= Globals::Write<uint32_t>(input + Offset::SpellInputLayout::InputTargetNetId, targetNetId);
            ok &= Globals::Write<Vec3>(input + Offset::SpellInputLayout::InputStartPos, start);
            ok &= Globals::Write<Vec3>(input + Offset::SpellInputLayout::InputEndPos, end);
            ok &= Globals::Write<Vec3>(input + Offset::SpellInputLayout::InputEndPos + sizeof(Vec3), end);
            ok &= Globals::Write<Vec3>(input + Offset::SpellInputLayout::InputEndPos + sizeof(Vec3) * 2, end);
            return ok;
        }

        float GetCastRange(int rank = 0) const {
            const auto data = GetSpellData();
            if (!Globals::IsValidPtr(data)) return 0.0f;
            const auto safeRank = ClampRank(rank);
            return Globals::Read<float>(data + Offset::SpellDataResourceLayout::ResCastRange +
                                        static_cast<uintptr_t>(safeRank * sizeof(float)));
        }

        float GetMissileSpeed() const {
            const auto resource = GetSpellResource();
            if (!Globals::IsValidPtr(resource)) return 0.0f;
            return Globals::Read<float>(resource +
                                        Offset::SpellDataResourceLayout::DataResourceBase +
                                        Offset::SpellDataResourceLayout::ResMissileSpeed);
        }

        float GetLineWidth() const {
            const auto resource = GetSpellResource();
            if (!Globals::IsValidPtr(resource)) return 0.0f;
            return Globals::Read<float>(resource +
                                        Offset::SpellDataResourceLayout::DataResourceBase +
                                        Offset::SpellDataResourceLayout::ResLineWidth);
        }

        int GetCastType() const {
            const auto resource = GetSpellResource();
            if (!Globals::IsValidPtr(resource)) return 0;
            return Globals::Read<int>(resource +
                                      Offset::SpellDataResourceLayout::DataResourceBase +
                                      Offset::SpellDataResourceLayout::ResCastType);
        }

        int GetMaxAmmo(int rank = 0) const {
            const auto data = GetSpellData();
            if (!Globals::IsValidPtr(data)) return 0;
            const auto safeRank = ClampRank(rank);
            return Globals::Read<int>(data + Offset::SpellDataResourceLayout::ResMaxAmmo +
                                      static_cast<uintptr_t>(safeRank * sizeof(int)));
        }

        int GetAmmo() const {
            const int stacks = GetStacks();
            if (stacks > 0) {
                return stacks;
            }
            const int maxAmmo = GetMaxAmmo();
            return maxAmmo > 0 ? maxAmmo : 0;
        }

        bool ReadSpellName(char* out, int maxOut) const {
            if (!out || maxOut <= 0) return false;
            const auto data = GetSpellData();
            if (!Globals::IsValidPtr(data)) { out[0] = 0; return false; }
            return Globals::ReadRuntimeStringField(data + Offset::SpellDataLayout::DataSpellName, out, maxOut);
        }

        bool ReadScriptName(char* out, int maxOut) const {
            if (!out || maxOut <= 0) return false;
            const auto data = GetSpellData();
            if (!Globals::IsValidPtr(data)) { out[0] = 0; return false; }
            return Globals::ReadRuntimeStringField(data + Offset::SpellDataResourceLayout::ResScriptName, out, maxOut);
        }

        bool ReadIconName(char* out, int maxOut) const {
            if (!out || maxOut <= 0) return false;
            const auto resource = GetSpellResource();
            if (!Globals::IsValidPtr(resource)) { out[0] = 0; return false; }
            return Globals::ReadGameString(resource +
                                           Offset::SpellDataResourceLayout::DataResourceBase +
                                           Offset::SpellDataResourceLayout::ResImgIconName,
                                           out, maxOut);
        }

        bool CanCastNow(float gameTime) const {
            return IsValid() && IsLearned() && GetRemainingCooldown(gameTime) <= 0.0f;
        }

        SpellState GetApproxState(float currentMana, float gameTime) const {
            if (!IsValid() || !HasSpellInfo() || !HasSpellInput()) {
                return State_Disabled;
            }
            if (!IsLearned())          return State_NotLearned;
            if (IsOnCooldown(gameTime)) return State_Cooldown;

            const float manaCost = GetManaCost();
            if (manaCost > 0.0f && currentMana < manaCost) {
                return State_NoMana;
            }
            return State_Ready;
        }
    };

    // ── Top-level accessors ──

    // Phase 2 (Apr 26/2026): SDK alias used by sdk/GameObjects/SpellbookClient.h.
    // Same as `GetSpellBook` - returns the spellbook embedded address inside obj.
    inline uintptr_t Get(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) {
            return 0;
        }
        return obj + Offset::SpellRuntime::SpellBookOffset;
    }

    inline uintptr_t GetSpellBook(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return 0;
        return obj + Offset::SpellRuntime::SpellBookOffset;
    }

    inline uintptr_t GetActiveSpellCast(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return 0;
        // Offset::SpellRuntime::ActiveSpellCast is hero-relative (0x3158), not
        // spellbook-relative, so read directly from the hero.
        return Globals::Read<uintptr_t>(obj + Offset::SpellRuntime::ActiveSpellCast);
    }

    inline SlotRef GetSlot(uintptr_t obj, int slotId) {
        const auto spellBook = GetSpellBook(obj);
        if (!Globals::IsValidPtr(spellBook)) return {};
        const auto slotAddr = spellBook + Offset::SpellBookLayout::SpellSlotArray +
                              static_cast<uintptr_t>(slotId * sizeof(uintptr_t));
        const auto slot = Globals::Read<uintptr_t>(slotAddr);
        return { slot };
    }

    inline bool HasEnoughMana(uintptr_t obj, int slotId) {
        const auto slot = GetSlot(obj, slotId);
        if (!slot.IsValid()) return false;

        const float manaCost = slot.GetManaCost();
        if (manaCost <= 0.0f) return true;

        return Globals::Read<float>(obj + Offset::AIHeroClient::MP) >= manaCost;
    }

    inline bool CanCast(uintptr_t obj, int slotId, float gameTime) {
        const auto slot = GetSlot(obj, slotId);
        if (!slot.IsValid() || !slot.HasSpellInfo() || !slot.HasSpellInput()) {
            return false;
        }
        return slot.CanCastNow(gameTime) && HasEnoughMana(obj, slotId);
    }

    inline SpellState GetSpellState(uintptr_t obj, int slotId, float gameTime) {
        const auto slot = GetSlot(obj, slotId);
        if (!slot.IsValid()) return State_Disabled;

        const float currentMana = Globals::Read<float>(obj + Offset::AIHeroClient::MP);
        return slot.GetApproxState(currentMana, gameTime);
    }

} // namespace CoreSpellBook
