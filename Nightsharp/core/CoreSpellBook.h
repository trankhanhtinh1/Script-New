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

        // LoL spell data rank arrays are dimensioned for 6 entries (rank 0..5),
        // which covers both regular spells (max rank 5) and ults (max rank 3).
        // A previous version capped at 6 which read one element past the array
        // end — returning garbage for `GetCastRange(6)` / `GetMaxAmmo(6)` /
        // `GetBaseCooldownTime(6)` and potentially faulting if the spell data
        // struct is short. Cap at 5 so the maximum accessible index matches the
        // last real array slot.
        static int ClampRank(int rank) {
            if (rank < 0) {
                return 0;
            }
            if (rank > 5) {
                return 5;
            }
            return rank;
        }

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        int GetLevel() const {
            return Globals::Read<int>(address + Offset::SpellBook::SlotLevel);
        }

        float GetReadyAt() const {
            return Globals::Read<float>(address + Offset::SpellBook::SlotCooldown);
        }

        float GetCooldown() const {
            return GetReadyAt();
        }

        float GetTotalCooldown() const {
            return Globals::Read<float>(address + Offset::SpellBook::SlotTotalCd);
        }

        int GetStacks() const {
            return Globals::Read<int>(address + Offset::SpellBook::SlotStacks);
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

        // Stage the four position slots the engine consumes during a cast.
        //
        // Position data lives in SpellInfo (slot+0x128) rather than SpellInput
        // (slot+0x130); both structures share the same relative layout and the
        // engine keeps them in sync. Writing through SpellInfo matches the
        // save/restore flow in `CoreControl::CastSpellPacket` and is what the
        // live game reads back when dispatching the cast.
        //
        // IDA verified (byte-pattern scan, 2026-04-17):
        //   +0x18 InputStartPos  — 5 `movss XMM,[rcx+0x18]` hits
        //   +0x24 InputEndPos    — 5 `movss XMM,[rcx+0x24]` hits
        //   +0x30 InputEndPos2   — 4 `movss XMM,[rcx+0x30]` hits
        //   +0x3C InputEndPos3   — 5 `movss XMM,[rcx+0x3C]` hits
        bool SetInputData(uint32_t targetNetId, const Vec3& start, const Vec3& end) const {
            const auto input = GetSpellInfo();
            if (!Globals::IsValidPtr(input)) {
                return false;
            }

            bool ok = true;
            ok &= Globals::Write<uint32_t>(input + Offset::SpellBook::InputTargetNetId, targetNetId);
            ok &= Globals::Write<Vec3>(input + Offset::SpellBook::InputStartPos, start);
            ok &= Globals::Write<Vec3>(input + Offset::SpellBook::InputEndPos,   end);
            ok &= Globals::Write<Vec3>(input + Offset::SpellBook::InputEndPos2,  end);
            ok &= Globals::Write<Vec3>(input + Offset::SpellBook::InputEndPos3,  end);
            return ok;
        }

        // ─────────────────────────────────────────────────────────────────
        // Res{CastRange,MissileSpeed,LineWidth} offsets are STALE in 26.7
        // (verified via CE runtime + IDA decompile 2026-04-17). The game
        // resolves these via indexed-getter dispatch (GetFloatParam(res, idx))
        // where CASTRANGE=0x0F, LINEWIDTH=0x1D, MISSILESPEED=0x27.
        //
        // Reading at the legacy direct offsets returns garbage (strings or
        // function pointers interpreted as float). We clamp the result to a
        // sane range and return 0.0f on garbage so callers fall back to
        // compile-time static values from SpellDatabaseData.generated.h (the
        // `Spell` constructor in every plugin supplies range/speed/width
        // directly, so these getters are almost never the source of truth).
        //
        // TODO: implement indexed-getter via game function trampoline.
        // ─────────────────────────────────────────────────────────────────
        static float ClampSpellStat(float v, float hi) {
            // Reject NaN / inf / negative / out-of-range garbage
            if (!(v == v) || v < 0.0f || v > hi) return 0.0f;
            return v;
        }

        float GetCastRange(int rank = 0) const {
            const auto data = GetSpellData();
            const auto safeRank = ClampRank(rank);
            const float raw = Globals::Read<float>(data + Offset::SpellBook::ResCastRange + static_cast<uintptr_t>(safeRank * sizeof(float)));
            return ClampSpellStat(raw, 50000.0f); // Karthus R = 25000 is the real max
        }

        float GetMissileSpeed() const {
            const auto resource = GetSpellResource();
            const float raw = Globals::Read<float>(resource + Offset::SpellBook::DataResourceBase + Offset::SpellBook::ResMissileSpeed);
            return ClampSpellStat(raw, 50000.0f);
        }

        float GetLineWidth() const {
            const auto resource = GetSpellResource();
            const float raw = Globals::Read<float>(resource + Offset::SpellBook::DataResourceBase + Offset::SpellBook::ResLineWidth);
            return ClampSpellStat(raw, 2000.0f); // widest skillshots ~400u
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
