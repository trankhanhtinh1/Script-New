#pragma once

#include "CoreRuntime.h"
#include "Vector.h"

#include <cstdint>

namespace CoreSpellBook {

    enum SpellSlotId : int {
        Slot_Q = 0,
        Slot_W = 1,
        Slot_E = 2,
        Slot_R = 3,
        Slot_Summoner1 = 4,
        Slot_Summoner2 = 5,
        Slot_Item1 = 6,
        Slot_Item2 = 7,
        Slot_Item3 = 8,
        Slot_Item4 = 9,
        Slot_Item5 = 10,
        Slot_Item6 = 11,
        Slot_Trinket = 12,
        Slot_Recall = 13,
        Slot_Other = 14
    };

    enum SpellState : int {
        State_Unknown = -1,
        State_Ready = 0,
        State_NotLearned = 1,
        State_Cooldown = 2,
        State_NoMana = 3,
        State_Disabled = 4
    };

    struct SlotRef {
        uintptr_t address = 0;

        static int ClampRank(int rank) {
            if (rank < 0) {
                return 0;
            }
            if (rank > 6) {
                return 6;
            }
            return rank;
        }

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        // Level — 0x1C=getter (sub_2993D0), 0x28=LevelAlt (LevelUp sub_36DB50)
        int GetLevel() const {
            return Globals::Read<int>(address + Offset::SpellBook::SlotLevel);
        }

        int GetLevelAlt() const {
            return Globals::Read<int>(address + Offset::SpellBook::SlotLevelAlt);
        }

        // Cooldown system 1: official getters (0=off CD)
        float GetReadyAt() const {
            return Globals::Read<float>(address + Offset::SpellBook::SlotCooldown);
        }

        float GetCooldown() const {
            return GetReadyAt();
        }

        float GetTotalCooldown() const {
            return Globals::Read<float>(address + Offset::SpellBook::SlotTotalCd);
        }

        // Cooldown system 2: expiration time (-1.0=off CD, else game time)
        float GetCooldownExpires() const {
            return Globals::Read<float>(address + Offset::SpellBook::SlotCooldownExpires);
        }

        // Charge/ammo recharge timer (Teemo R, Akali R, etc.)
        float GetChargeTimer() const {
            return Globals::Read<float>(address + Offset::SpellBook::SlotChargeTimer);
        }

        int GetStacks() const {
            return Globals::Read<int>(address + Offset::SpellBook::SlotStacks);
        }

        // Per-slot active cast ptr (NULL = not casting this spell)
        uintptr_t GetSlotActiveSpellCast() const {
            return Globals::Read<uintptr_t>(address + Offset::SpellBook::SlotActiveSpellCast);
        }

        bool IsSlotCasting() const {
            return Globals::IsValidPtr(GetSlotActiveSpellCast());
        }

        // Spell identity
        uint32_t GetSpellNameHash() const {
            return Globals::Read<uint32_t>(address + Offset::SpellBook::SlotSpellNameHash);
        }

        uintptr_t GetSpellInstanceVars() const {
            return Globals::Read<uintptr_t>(address + Offset::SpellBook::SlotSpellInstanceVars);
        }

        // Read spell name via SpellDataResource SSO path (alternative to DataSpellName)
        bool ReadSpellNameFromResource(char* out, int maxOut) const {
            if (!out || maxOut <= 0) { out[0] = 0; return false; }
            const auto sdr = Globals::Read<uintptr_t>(address + Offset::SpellBook::SlotSpellInput); // =SpellDataResource
            if (!Globals::IsValidPtr(sdr)) { out[0] = 0; return false; }
            const auto cap = Globals::Read<size_t>(sdr + Offset::SpellBook::SpellNameCap);
            if (cap > 0xF) {
                const auto heapPtr = Globals::Read<uintptr_t>(sdr + Offset::SpellBook::SpellNameStr);
                return Globals::IsValidPtr(heapPtr) ? Globals::ReadCString(heapPtr, out, maxOut) : (out[0] = 0, false);
            }
            return Globals::ReadCString(sdr + Offset::SpellBook::SpellNameStr, out, maxOut);
        }

        float GetAmmoRechargeTime() const {
            const auto data = GetSpellData();
            return Globals::Read<float>(data + Offset::SpellBook::ResAmmoRecharge);
        }

        float GetBaseCooldownTime(int rank = 0) const {
            const auto data = GetSpellData();
            const auto safeRank = ClampRank(rank);
            return Globals::Read<float>(data + Offset::SpellBook::ResCooldownTime + static_cast<uintptr_t>(safeRank * sizeof(float)));
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

        uintptr_t GetSpellInput() const {
            return Globals::Read<uintptr_t>(address + Offset::SpellBook::SlotSpellInput);
        }

        bool HasSpellInput() const {
            return Globals::IsValidPtr(GetSpellInput());
        }

        uintptr_t GetSpellInfo() const {
            return Globals::Read<uintptr_t>(address + Offset::SpellBook::SlotSpellInfo);
        }

        bool HasSpellInfo() const {
            return Globals::IsValidPtr(GetSpellInfo());
        }

        uintptr_t GetSpellData() const {
            const auto info = GetSpellInfo();
            return Globals::Read<uintptr_t>(info + Offset::SpellBook::InfoSpellData);
        }

        uintptr_t GetSpellResource() const {
            const auto data = GetSpellData();
            return Globals::Read<uintptr_t>(data + Offset::SpellBook::DataResource);
        }

        float GetManaCost() const {
            const auto data = GetSpellData();
            return Globals::Read<float>(data + Offset::SpellBook::DataManaCost);
        }

        int GetAmmo() const {
            const int stacks = GetStacks();
            if (stacks > 0) {
                return stacks;
            }

            const int maxAmmo = GetMaxAmmo();
            return maxAmmo > 0 ? maxAmmo : 0;
        }

        uintptr_t GetCastArgument() const {
            const auto input = GetSpellInput();
            return Globals::IsValidPtr(input) ? (input + 0x8) : 0;
        }

        uint32_t GetInputTargetNetId() const {
            const auto input = GetSpellInput();
            return Globals::Read<uint32_t>(input + Offset::SpellBook::InputTargetNetId);
        }

        Vec3 GetInputStartPos() const {
            const auto input = GetSpellInput();
            return Globals::Read<Vec3>(input + Offset::SpellBook::InputStartPos);
        }

        Vec3 GetInputEndPos() const {
            const auto input = GetSpellInput();
            return Globals::Read<Vec3>(input + Offset::SpellBook::InputEndPos);
        }

        bool SetInputData(uint32_t targetNetId, const Vec3& start, const Vec3& end) const {
            const auto input = GetSpellInfo();  // position lives in SpellInfo (0x128)
            if (!Globals::IsValidPtr(input)) {
                return false;
            }

            bool ok = true;
            ok &= Globals::Write<uint32_t>(input + Offset::SpellBook::InputTargetNetId, targetNetId);
            ok &= Globals::Write<Vec3>(input + Offset::SpellBook::InputStartPos, start);
            ok &= Globals::Write<Vec3>(input + Offset::SpellBook::InputEndPos, end);
            ok &= Globals::Write<Vec3>(input + Offset::SpellBook::InputEndPos + sizeof(Vec3), end);
            ok &= Globals::Write<Vec3>(input + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2, end);
            return ok;
        }

        float GetCastRange(int rank = 0) const {
            const auto data = GetSpellData();
            const auto safeRank = ClampRank(rank);
            return Globals::Read<float>(data + Offset::SpellBook::ResCastRange + static_cast<uintptr_t>(safeRank * sizeof(float)));
        }

        float GetMissileSpeed() const {
            const auto resource = GetSpellResource();
            return Globals::Read<float>(resource + Offset::SpellBook::DataResourceBase + Offset::SpellBook::ResMissileSpeed);
        }

        float GetLineWidth() const {
            const auto resource = GetSpellResource();
            return Globals::Read<float>(resource + Offset::SpellBook::DataResourceBase + Offset::SpellBook::ResLineWidth);
        }

        int GetCastType() const {
            const auto resource = GetSpellResource();
            return Globals::Read<int>(resource + Offset::SpellBook::DataResourceBase + Offset::SpellBook::ResCastType);
        }

        int GetMaxAmmo(int rank = 0) const {
            const auto data = GetSpellData();
            const auto safeRank = ClampRank(rank);
            return Globals::Read<int>(data + Offset::SpellBook::ResMaxAmmo + static_cast<uintptr_t>(safeRank * sizeof(int)));
        }

        bool ReadSpellName(char* out, int maxOut) const {
            if (!out || maxOut <= 0) {
                return false;
            }

            const auto data = GetSpellData();
            if (!Globals::IsValidPtr(data)) {
                out[0] = 0;
                return false;
            }

            return Globals::ReadRuntimeStringField(data + Offset::SpellBook::DataSpellName, out, maxOut);
        }

        bool ReadScriptName(char* out, int maxOut) const {
            if (!out || maxOut <= 0) {
                return false;
            }

            const auto data = GetSpellData();
            if (!Globals::IsValidPtr(data)) {
                out[0] = 0;
                return false;
            }

            return Globals::ReadRuntimeStringField(data + Offset::SpellBook::ResScriptName, out, maxOut);
        }

        bool ReadIconName(char* out, int maxOut) const {
            if (!out || maxOut <= 0) {
                return false;
            }

            const auto resource = GetSpellResource();
            if (!Globals::IsValidPtr(resource)) {
                out[0] = 0;
                return false;
            }

            return Globals::ReadGameString(
                resource + Offset::SpellBook::DataResourceBase + Offset::SpellBook::ResImgIconName,
                out,
                maxOut);
        }

        bool CanCastNow(float gameTime) const {
            return IsValid() && IsLearned() && GetRemainingCooldown(gameTime) <= 0.0f;
        }

        SpellState GetApproxState(float currentMana, float gameTime) const {
            if (!IsValid() || !HasSpellInfo() || !HasSpellInput()) {
                return State_Disabled;
            }

            if (!IsLearned()) {
                return State_NotLearned;
            }

            if (IsOnCooldown(gameTime)) {
                return State_Cooldown;
            }

            const float manaCost = GetManaCost();
            if (manaCost > 0.0f && currentMana < manaCost) {
                return State_NoMana;
            }

            return State_Ready;
        }
    };

    inline uintptr_t GetSpellBook(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) {
            return 0;
        }
        return obj + Offset::SpellBook::Offset;
    }

    inline uintptr_t GetActiveSpellCast(uintptr_t obj) {
        const auto spellBook = GetSpellBook(obj);
        return Globals::Read<uintptr_t>(spellBook + Offset::SpellBook::ActiveSpellCast);
    }

    inline SlotRef GetSlot(uintptr_t obj, int slotId) {
        const auto spellBook = GetSpellBook(obj);
        const auto slot = Globals::Read<uintptr_t>(spellBook + Offset::SpellBook::SpellSlotArray + static_cast<uintptr_t>(slotId * sizeof(uintptr_t)));
        return { slot };
    }

    inline bool HasEnoughMana(uintptr_t obj, int slotId) {
        const auto slot = GetSlot(obj, slotId);
        if (!slot.IsValid()) {
            return false;
        }

        const float manaCost = slot.GetManaCost();
        if (manaCost <= 0.0f) {
            return true;
        }

        return Globals::Read<float>(obj + Offset::Mana::MP) >= manaCost;
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
        if (!slot.IsValid()) {
            return State_Disabled;
        }

        const float currentMana = Globals::Read<float>(obj + Offset::Mana::MP);
        return slot.GetApproxState(currentMana, gameTime);
    }

} // namespace CoreSpellBook
