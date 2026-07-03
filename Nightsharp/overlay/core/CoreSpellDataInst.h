#pragma once

#include "CoreRuntime.h"
#include "Globals.h"
#include "offset.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

namespace CoreSpellBook {
    enum State : std::int32_t {
        State_Ready = 0,
        State_NoSpell = 2,
        State_NotLearned = 4,
        State_Disabled = 8,
        State_Unknown = 0x0A,
        State_Suppressed = 0x10,
        State_Surpressed = State_Suppressed,
        State_Cooldown = 0x20,
        State_NoMana = 0x40,
    };
} // namespace CoreSpellBook

namespace CoreSpellDataInst {

inline constexpr std::uint32_t StateFlag_NoSpell = 0x00000002u;
inline constexpr std::uint32_t StateFlag_NotLearned = 0x00000004u;
inline constexpr std::uint32_t StateFlag_Disabled = 0x00000008u;
inline constexpr std::uint32_t StateFlag_Suppressed = 0x00000010u;
inline constexpr std::uint32_t StateFlag_Cooldown = 0x00000020u;
inline constexpr std::uint32_t StateFlag_NoMana = 0x00000040u;
inline constexpr std::uint32_t StateFlag_AmmoCooldown = 0x00000200u;
inline constexpr std::uint32_t StateFlag_InvalidCast = 0x00200000u;
inline constexpr int kMaxSpellSlots = 64;

struct SpellSlotRef {
    uintptr_t owner = 0;
    uintptr_t spellbook = 0;
    uintptr_t slot = 0;
    std::int32_t slotId = -1;

    bool IsValid() const {
        return Globals::IsValidPtr(owner) &&
               Globals::IsValidPtr(spellbook) &&
               Globals::IsValidPtr(slot) &&
               slotId >= 0 &&
               slotId < kMaxSpellSlots;
    }
};

inline bool IsFinite(float value) {
    return std::isfinite(value);
}

inline bool IsSaneSeconds(float value) {
    return IsFinite(value) && value >= 0.0f && value < 1000000.0f;
}

inline bool IsSaneDuration(float value) {
    return IsFinite(value) && value >= 0.0f && value < 300.0f;
}

inline bool IsSaneDistance(float value) {
    return IsFinite(value) && value >= 0.0f && value < 100000.0f;
}

inline bool IsSaneMissileSpeed(float value) {
    return IsFinite(value) && value >= 0.0f && value < 1000000.0f;
}

inline bool IsValidSlot(std::int32_t slot) {
    return slot >= 0 && slot < kMaxSpellSlots;
}

inline uintptr_t Spellbook(uintptr_t owner) {
    if (!Globals::IsValidPtr(owner)) {
        return 0;
    }

    const uintptr_t spellbook = owner + Offset::SpellRuntime::SpellBookOffset;
    return Globals::IsValidPtr(spellbook) ? spellbook : 0;
}

inline uintptr_t ResolveSlot(uintptr_t spellbook, std::int32_t slot) {
    if (!Globals::IsValidPtr(spellbook) || !IsValidSlot(slot)) {
        return 0;
    }

    const uintptr_t slotPtr = Globals::Read<uintptr_t>(
        spellbook +
        Offset::SpellBookLayout::SpellSlotArray +
        static_cast<uintptr_t>(slot) * sizeof(uintptr_t));
    return Globals::IsValidPtr(slotPtr) ? slotPtr : 0;
}

inline SpellSlotRef Resolve(uintptr_t owner, std::int32_t slot) {
    SpellSlotRef ref{};
    ref.owner = owner;
    ref.slotId = slot;
    ref.spellbook = Spellbook(owner);
    ref.slot = ResolveSlot(ref.spellbook, slot);
    return ref;
}

inline bool IsValid(uintptr_t owner, std::int32_t slot) {
    return Resolve(owner, slot).IsValid();
}

inline uintptr_t SpellInfo(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0;
    }

    const uintptr_t info = Globals::Read<uintptr_t>(
        ref.slot + Offset::SpellSlotLayout::SlotSpellInfo);
    return Globals::IsValidPtr(info) ? info : 0;
}

inline uintptr_t SpellData(const SpellSlotRef& ref) {
    const uintptr_t info = SpellInfo(ref);
    if (!Globals::IsValidPtr(info)) {
        return 0;
    }

    const uintptr_t data = Globals::Read<uintptr_t>(
        info + Offset::SpellInfoLayout::InfoSpellData);
    return Globals::IsValidPtr(data) ? data : 0;
}

inline uintptr_t SpellDataResource(const SpellSlotRef& ref) {
    const uintptr_t data = SpellData(ref);
    if (!Globals::IsValidPtr(data)) {
        return 0;
    }

    const uintptr_t resource = Globals::Read<uintptr_t>(
        data + Offset::SpellDataLayout::DataResource);
    return Globals::IsValidPtr(resource) ? resource : 0;
}

inline uintptr_t SpellInput(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0;
    }

    const uintptr_t input = Globals::Read<uintptr_t>(
        ref.slot + Offset::SpellSlotLayout::SlotSpellInput);
    return Globals::IsValidPtr(input) ? input : 0;
}

inline std::int32_t Level(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0;
    }

    return Globals::Read<std::int32_t>(
        ref.slot + Offset::SpellSlotLayout::SlotLevel);
}

inline std::int32_t Level(uintptr_t owner, std::int32_t slot) {
    return Level(Resolve(owner, slot));
}

inline bool Learned(const SpellSlotRef& ref) {
    return Level(ref) > 0;
}

inline float CooldownExpires(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0.0f;
    }

    const float value = Globals::Read<float>(
        ref.slot + Offset::SpellSlotLayout::SlotCooldownExpires);
    return IsSaneSeconds(value) ? value : 0.0f;
}

inline float NextAmmoRechargeTime(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0.0f;
    }

    const float value = Globals::Read<float>(
        ref.slot + Offset::SpellSlotLayout::SlotChargeTimer);
    return IsSaneSeconds(value) ? value : 0.0f;
}

inline float AmmoRechargeTime(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0.0f;
    }

    const float slotValue = Globals::Read<float>(
        ref.slot + Offset::SpellSlotLayout::SlotChargeCooldownDuration);
    if (IsSaneDuration(slotValue) && slotValue > 0.0f) {
        return slotValue;
    }

    const uintptr_t resource = SpellDataResource(ref);
    const float resourceValue = Globals::Read<float>(
        resource + Offset::SpellDataResourceLayout::ResAmmoRecharge);
    return IsSaneDuration(resourceValue) ? resourceValue : 0.0f;
}

inline float Cooldown(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0.0f;
    }

    const float slotDuration = Globals::Read<float>(
        ref.slot + Offset::SpellSlotLayout::SlotCooldownDuration);
    if (IsSaneDuration(slotDuration) && slotDuration > 0.0f) {
        return slotDuration;
    }

    const float legacyTotal = Globals::Read<float>(
        ref.slot + Offset::SpellSlotLayout::SlotTotalCd);
    if (IsSaneDuration(legacyTotal) && legacyTotal > 0.0f) {
        return legacyTotal;
    }

    const uintptr_t resource = SpellDataResource(ref);
    const int levelIndex = std::max(0, std::min(5, Level(ref) - 1));
    const float resourceCooldown = Globals::Read<float>(
        resource +
        Offset::SpellDataResourceLayout::ResCooldownTime +
        static_cast<uintptr_t>(levelIndex) * sizeof(float));
    return IsSaneDuration(resourceCooldown) ? resourceCooldown : 0.0f;
}

inline std::int32_t Ammo(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0;
    }

    const std::int32_t value = Globals::Read<std::int32_t>(
        ref.slot + Offset::SpellSlotLayout::SlotStacks);
    return (value >= 0 && value < 1000) ? value : 0;
}

inline std::int32_t MaxAmmo(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0;
    }

    const std::int32_t value = Globals::Read<std::int32_t>(
        ref.slot + Offset::SpellSlotLayout::SlotMaxStacks);
    return (value >= 0 && value < 1000) ? value : 0;
}

inline float ManaCost(const SpellSlotRef& ref) {
    const uintptr_t data = SpellData(ref);
    if (!Globals::IsValidPtr(data)) {
        return 0.0f;
    }

    const int levelIndex = std::max(0, std::min(5, Level(ref) - 1));
    const float value = Globals::Read<float>(
        data +
        Offset::SpellDataLayout::DataManaCost +
        static_cast<uintptr_t>(levelIndex) * sizeof(float));
    if (IsSaneDuration(value)) {
        return value;
    }

    const float firstValue = Globals::Read<float>(
        data + Offset::SpellDataLayout::DataManaCost);
    return IsSaneDuration(firstValue) ? firstValue : 0.0f;
}

inline float ResourceFloat(const SpellSlotRef& ref,
                           uintptr_t offset,
                           bool (*validator)(float),
                           float fallback = 0.0f) {
    if (!ref.IsValid() || offset == 0 || !validator) {
        return fallback;
    }

    const uintptr_t resource = SpellDataResource(ref);
    if (!Globals::IsValidPtr(resource)) {
        return fallback;
    }

    const float value = Globals::Read<float>(resource + offset);
    return validator(value) ? value : fallback;
}

inline std::uint8_t CastType(const SpellSlotRef& ref) {
    const uintptr_t resource = SpellDataResource(ref);
    if (!Globals::IsValidPtr(resource)) {
        return 0;
    }

    return Globals::Read<std::uint8_t>(
        resource + Offset::SpellDataResourceLayout::ResCastType);
}

inline float CastRange(const SpellSlotRef& ref) {
    return ResourceFloat(
        ref,
        Offset::SpellDataResourceLayout::ResCastRange,
        &IsSaneDistance);
}

inline float LineWidth(const SpellSlotRef& ref) {
    return ResourceFloat(
        ref,
        Offset::SpellDataResourceLayout::ResLineWidth,
        &IsSaneDistance);
}

inline float MissileSpeed(const SpellSlotRef& ref) {
    return ResourceFloat(
        ref,
        Offset::SpellDataResourceLayout::ResMissileSpeed,
        &IsSaneMissileSpeed);
}

inline float CastRadius(const SpellSlotRef& ref) {
    (void)ref;
    // IDA 13339 confirms the SPELLPARAM_CASTRADIUS enum value, but not a
    // stable direct float field. Keep this explicit until the native getter
    // path is reversed; SDK Spell falls back to SpellDatabase radius.
    return 0.0f;
}

inline bool ReadResourceString(const SpellSlotRef& ref,
                               uintptr_t offset,
                               char* out,
                               int maxOut) {
    if (!out || maxOut <= 1 || offset == 0) {
        return false;
    }
    out[0] = 0;

    const uintptr_t resource = SpellDataResource(ref);
    return Globals::IsValidPtr(resource) &&
           Globals::ReadRuntimeStringField(resource + offset, out, maxOut);
}

inline std::string ScriptName(const SpellSlotRef& ref) {
    char buffer[128] = {};
    return ReadResourceString(
            ref,
            Offset::SpellDataResourceLayout::ResScriptName,
            buffer,
            static_cast<int>(sizeof(buffer)))
        ? std::string(buffer)
        : std::string();
}

inline std::string IconName(const SpellSlotRef& ref) {
    char buffer[128] = {};
    return ReadResourceString(
            ref,
            Offset::SpellDataResourceLayout::ResImgIconName,
            buffer,
            static_cast<int>(sizeof(buffer)))
        ? std::string(buffer)
        : std::string();
}

inline std::uint32_t SpellNameHash(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0;
    }

    return Globals::Read<std::uint32_t>(
        ref.slot + Offset::SpellSlotLayout::SlotSpellNameHash);
}

inline bool ReadName(const SpellSlotRef& ref, char* out, int maxOut) {
    if (!out || maxOut <= 1) {
        return false;
    }
    out[0] = 0;

    const uintptr_t data = SpellData(ref);
    if (Globals::IsValidPtr(data) &&
        Globals::ReadRuntimeStringField(
            data + Offset::SpellDataLayout::DataSpellName,
            out,
            maxOut)) {
        return true;
    }

    const uintptr_t info = SpellInfo(ref);
    if (Globals::IsValidPtr(info) &&
        Globals::ReadRuntimeStringField(
            info + Offset::SpellInfoLayout::SpellInfoNamePtr,
            out,
            maxOut)) {
        return true;
    }

    const uintptr_t input = SpellInput(ref);
    return Globals::IsValidPtr(input) &&
           Globals::ReadRuntimeStringField(
               input + Offset::SpellInputLayout::SpellNameKey,
               out,
               maxOut);
}

inline std::string Name(const SpellSlotRef& ref) {
    char buffer[128] = {};
    return ReadName(ref, buffer, static_cast<int>(sizeof(buffer)))
        ? std::string(buffer)
        : std::string();
}

inline bool TryGetRawState(const SpellSlotRef& ref,
                           std::uint32_t& state,
                           std::uint8_t* auxOut = nullptr) {
    state = StateFlag_NoSpell;
    if (auxOut) {
        *auxOut = 0;
    }
    if (!ref.IsValid()) {
        return false;
    }

    const uintptr_t fn = CoreRuntime::ResolveRva(
        Offset::ControlRuntime::GetSpellState);
    if (!Globals::IsExecutablePtr(fn, 16)) {
        return false;
    }

    using FnGetSpellState = std::uint32_t(__fastcall*)(
        uintptr_t spellbook,
        std::uint32_t slot,
        std::uint8_t* aux);

    std::uint8_t aux = 0;
    __try {
        state = reinterpret_cast<FnGetSpellState>(fn)(
            ref.spellbook,
            static_cast<std::uint32_t>(ref.slotId),
            &aux);
        if (auxOut) {
            *auxOut = aux;
        }
        return true;
    }
    __except (1) {
        state = StateFlag_NoSpell;
        if (auxOut) {
            *auxOut = 0;
        }
        return false;
    }
}

inline std::uint32_t RawState(const SpellSlotRef& ref) {
    std::uint32_t state = StateFlag_NoSpell;
    (void)TryGetRawState(ref, state, nullptr);
    return state;
}

inline float RemainingCooldownNative(const SpellSlotRef& ref) {
    if (!ref.IsValid()) {
        return 0.0f;
    }

    const uintptr_t fn = CoreRuntime::ResolveRva(
        Offset::ControlRuntime::GetSpellRemainingCooldown);
    if (!Globals::IsExecutablePtr(fn, 16)) {
        return 0.0f;
    }

    using FnRemainingCooldown = float(__fastcall*)(uintptr_t spellSlot);

    __try {
        const float value = reinterpret_cast<FnRemainingCooldown>(fn)(ref.slot);
        return IsSaneSeconds(value) ? value : 0.0f;
    }
    __except (1) {
        return 0.0f;
    }
}

inline float RemainingCooldownFallback(const SpellSlotRef& ref, float gameTime) {
    if (!ref.IsValid()) {
        return 0.0f;
    }

    if (!IsSaneSeconds(gameTime) || gameTime <= 0.0f) {
        gameTime = CoreRuntime::GetContext().gameTime;
    }

    const float expires = CooldownExpires(ref);
    if (!IsSaneSeconds(expires) || expires <= gameTime) {
        return 0.0f;
    }
    return expires - gameTime;
}

inline float RemainingCooldown(const SpellSlotRef& ref, float gameTime = 0.0f) {
    const float nativeValue = RemainingCooldownNative(ref);
    if (nativeValue > 0.0f) {
        return nativeValue;
    }
    return RemainingCooldownFallback(ref, gameTime);
}

inline CoreSpellBook::State SimpleState(const SpellSlotRef& ref,
                                        float gameTime = 0.0f) {
    if (!ref.IsValid()) {
        return CoreSpellBook::State_NoSpell;
    }

    std::uint32_t rawState = StateFlag_NoSpell;
    const bool hasRawState = TryGetRawState(ref, rawState, nullptr);
    if (hasRawState) {
        if ((rawState & StateFlag_NoSpell) != 0) {
            return CoreSpellBook::State_NoSpell;
        }
        if ((rawState & StateFlag_NotLearned) != 0) {
            return CoreSpellBook::State_NotLearned;
        }
        if ((rawState & StateFlag_Disabled) != 0) {
            return CoreSpellBook::State_Disabled;
        }
        if ((rawState & StateFlag_Suppressed) != 0) {
            return CoreSpellBook::State_Suppressed;
        }
        if ((rawState & StateFlag_NoMana) != 0) {
            return CoreSpellBook::State_NoMana;
        }
        if ((rawState & (StateFlag_Cooldown | StateFlag_AmmoCooldown)) != 0) {
            return CoreSpellBook::State_Cooldown;
        }
    }

    if (!Learned(ref) && ref.slotId <= 3) {
        return CoreSpellBook::State_NotLearned;
    }

    return RemainingCooldown(ref, gameTime) > 0.0f
        ? CoreSpellBook::State_Cooldown
        : CoreSpellBook::State_Ready;
}

inline CoreSpellBook::State State(uintptr_t owner,
                                  std::int32_t slot,
                                  float gameTime = 0.0f) {
    return SimpleState(Resolve(owner, slot), gameTime);
}

} // namespace CoreSpellDataInst
