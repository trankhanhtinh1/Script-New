#pragma once

#include "CoreCastSpell.h"
#include "CoreNewCastSpell.h"
#include "CoreSpellDataInst.h"
#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"

#include <cstdint>

namespace CoreSpellBook {

struct CastInfoRef {
    uintptr_t address = 0;

    bool IsValid() const {
        return Globals::IsValidPtr(address);
    }
};

inline uintptr_t Spellbook(uintptr_t owner) {
    return CoreSpellDataInst::Spellbook(owner);
}

inline bool IsValidSlot(std::int32_t slot) {
    return CoreSpellDataInst::IsValidSlot(slot);
}

inline uintptr_t Owner(uintptr_t spellbook) {
    if (!Globals::IsValidPtr(spellbook)) {
        return 0;
    }

    const uintptr_t owner = Globals::Read<uintptr_t>(
        spellbook + Offset::SpellBookLayout::Owner);
    return Globals::IsValidPtr(owner) ? owner : 0;
}

inline std::uint32_t CasterNetworkId(uintptr_t spellbook) {
    if (!Globals::IsValidPtr(spellbook)) {
        return 0;
    }

    return Globals::Read<std::uint32_t>(
        spellbook + Offset::SpellBookLayout::CasterNetId);
}

inline std::int32_t ActiveSlot(uintptr_t owner) {
    const uintptr_t spellbook = Spellbook(owner);
    if (!Globals::IsValidPtr(spellbook)) {
        return -1;
    }

    return Globals::Read<std::int32_t>(
        spellbook + Offset::SpellBookLayout::ActiveSlot);
}

inline CoreSpellDataInst::SpellSlotRef GetSpell(uintptr_t owner,
                                                std::int32_t slot) {
    return CoreSpellDataInst::Resolve(owner, slot);
}

inline int GetSpells(uintptr_t owner,
                     CoreSpellDataInst::SpellSlotRef* out,
                     int maxOut) {
    if (!out || maxOut <= 0) {
        return 0;
    }

    int written = 0;
    for (int slot = 0;
         slot < CoreSpellDataInst::kMaxSpellSlots && written < maxOut;
         ++slot) {
        const auto ref = GetSpell(owner, slot);
        if (!ref.IsValid() || !Globals::IsValidPtr(CoreSpellDataInst::SpellData(ref))) {
            continue;
        }
        out[written++] = ref;
    }
    return written;
}

inline State CanUseSpell(uintptr_t owner,
                         std::int32_t slot,
                         float gameTime = 0.0f) {
    return CoreSpellDataInst::State(owner, slot, gameTime);
}

inline std::uint32_t RawState(uintptr_t owner, std::int32_t slot) {
    return CoreSpellDataInst::RawState(GetSpell(owner, slot));
}

inline CastInfoRef ActiveSpell(uintptr_t owner) {
    CastInfoRef ref{};
    const uintptr_t spellbook = Spellbook(owner);
    if (!Globals::IsValidPtr(spellbook)) {
        return ref;
    }

    ref.address = Globals::Read<uintptr_t>(
        spellbook + Offset::SpellRuntime::ActiveSpellCast);
    return ref;
}

inline CastInfoRef GetSpellCastInfo(uintptr_t owner, std::int32_t slot) {
    CastInfoRef ref{};
    if (!Globals::IsValidPtr(owner) || slot < 0) {
        return ref;
    }

    const uintptr_t fn = CoreRuntime::ResolveRva(
        Offset::ControlRuntime::GetSpellCastInfo);
    if (!Globals::IsExecutablePtrCached(fn, 16)) {   // was VirtualQuery() per call
        return ref;
    }

    using FnGetSpellCastInfo = uintptr_t(__fastcall*)(uintptr_t, std::uint32_t);
    __try {
        ref.address = reinterpret_cast<FnGetSpellCastInfo>(fn)(
            owner,
            static_cast<std::uint32_t>(slot));
    }
    __except (1) {
        ref.address = 0;
    }
    return ref;
}

inline float CastStartTime(CastInfoRef ref) {
    return ref.IsValid()
        ? Globals::Read<float>(ref.address + Offset::SpellCastInfoLayout::StartTime)
        : 0.0f;
}

inline float CastEndTime(CastInfoRef ref) {
    return ref.IsValid()
        ? Globals::Read<float>(ref.address + Offset::SpellCastInfoLayout::EndTime)
        : 0.0f;
}

inline float ChannelStartTime(CastInfoRef ref) {
    return ref.IsValid()
        ? Globals::Read<float>(ref.address + Offset::SpellCastInfoLayout::ChannelStart)
        : 0.0f;
}

inline float ChannelEndTime(CastInfoRef ref) {
    return ref.IsValid()
        ? Globals::Read<float>(ref.address + Offset::SpellCastInfoLayout::ChannelEnd)
        : 0.0f;
}

inline std::int32_t CastSlot(CastInfoRef ref) {
    return ref.IsValid()
        ? Globals::Read<std::int32_t>(ref.address + Offset::SpellCastInfoLayout::SpellSlot)
        : -1;
}

inline std::uint32_t TargetNetworkId(CastInfoRef ref) {
    return ref.IsValid()
        ? Globals::Read<std::uint32_t>(ref.address + Offset::SpellCastInfoLayout::TargetNetId)
        : 0;
}

inline Vec3 StartPosition(CastInfoRef ref) {
    return ref.IsValid()
        ? Globals::Read<Vec3>(ref.address + Offset::SpellCastInfoLayout::StartPosition)
        : Vec3{};
}

inline Vec3 EndPosition(CastInfoRef ref) {
    return ref.IsValid()
        ? Globals::Read<Vec3>(ref.address + Offset::SpellCastInfoLayout::EndPosition)
        : Vec3{};
}

inline bool IsCastingSpell(uintptr_t owner, float gameTime = 0.0f) {
    const CastInfoRef active = ActiveSpell(owner);
    if (!active.IsValid()) {
        return false;
    }

    if (gameTime <= 0.0f) {
        gameTime = CoreRuntime::GetContext().gameTime;
    }

    const float endTime = CastEndTime(active);
    return endTime <= 0.0f || gameTime <= (endTime + 0.25f);
}

inline bool IsChanneling(uintptr_t owner, float gameTime = 0.0f) {
    const CastInfoRef active = ActiveSpell(owner);
    if (!active.IsValid()) {
        return false;
    }

    if (gameTime <= 0.0f) {
        gameTime = CoreRuntime::GetContext().gameTime;
    }

    const float channelStart = ChannelStartTime(active);
    const float channelEnd = ChannelEndTime(active);
    return channelStart > 0.0f && channelEnd > channelStart && gameTime <= channelEnd;
}

inline std::vector<std::string>& GetRegisteredChargedBuffs() {
    static std::vector<std::string> s_registeredChargedBuffs;
    return s_registeredChargedBuffs;
}

inline void RegisterChargedBuffName(const char* buffName) {
    if (!buffName || !buffName[0]) return;
    auto& list = GetRegisteredChargedBuffs();
    for (const auto& name : list) {
        if (name == buffName) return;
    }
    list.push_back(buffName);
}

inline bool IsCharging(uintptr_t owner) {
    if (!Globals::IsValidPtr(owner)) return false;

    // Check dynamically registered charged buff names
    for (const auto& buffName : GetRegisteredChargedBuffs()) {
        if (CoreBuffs::HasBuff(owner, buffName.c_str())) {
            return true;
        }
    }

    const auto active = ActiveSpell(owner);
    if (active.IsValid()) {
        const float channelStart = ChannelStartTime(active);
        const float channelEnd = ChannelEndTime(active);
        const float gameTime = CoreRuntime::GetContext().gameTime;
        if (channelStart > 0.0f && channelEnd > channelStart && gameTime <= channelEnd) {
            return true;
        }
    }

    const std::int32_t slot = ActiveSlot(owner);
    if (IsValidSlot(slot)) {
        const auto ref = GetSpell(owner, slot);
        if (ref.IsValid()) {
            const uintptr_t fn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::SpellTypeClassify);
            if (Globals::IsExecutablePtrCached(fn, 16)) {
                using FnSpellTypeClassify = std::uint8_t(__fastcall*)(uintptr_t);
                __try {
                    if (reinterpret_cast<FnSpellTypeClassify>(fn)(ref.slot) == 18) {
                        return true;
                    }
                }
                __except (1) {}
            }
        }
    }

    return false;
}

namespace detail {
    class ScopedWritePhase {
    public:
        ScopedWritePhase() {
            started_ = !CoreRuntime::IsWritePhase();
            if (started_) {
                CoreRuntime::BeginWritePhase();
            }
        }

        ~ScopedWritePhase() {
            if (started_) {
                CoreRuntime::EndWritePhase();
            }
        }

    private:
        bool started_ = false;
    };

    inline bool IsLocalOwner(uintptr_t owner) {
        if (!CoreRuntime::EnsureInitialized()) {
            return false;
        }
        (void)CoreRuntime::RefreshReadState();
        return Globals::IsValidPtr(owner) &&
               owner == CoreRuntime::GetContext().localPlayer;
    }
} // namespace detail

inline bool CastSpell(uintptr_t owner, std::int32_t slot) {
    if (!detail::IsLocalOwner(owner) || slot < 0 || slot > CoreCastSpell::SlotSummonerF) {
        return false;
    }

    detail::ScopedWritePhase writePhase;
    return CoreCastSpell::CastSelfSpell(static_cast<std::uint8_t>(slot));
}

inline bool CastSpell(uintptr_t owner, std::int32_t slot, const Vec3& position) {
    if (!detail::IsLocalOwner(owner) || slot < 0 || slot > CoreCastSpell::SlotTrinket) {
        return false;
    }

    return CoreNewCastSpell::CastPositionSpellMethod2(slot, position);
}

inline bool CastSpell(uintptr_t owner,
                      std::int32_t slot,
                      const Vec3& startPosition,
                      const Vec3& endPosition) {
    if (!detail::IsLocalOwner(owner) || slot < 0 || slot > CoreCastSpell::SlotSummonerF) {
        return false;
    }

    detail::ScopedWritePhase writePhase;
    return CoreCastSpell::CastVectorSpell(
        static_cast<std::uint8_t>(slot),
        startPosition,
        endPosition);
}

inline bool CastSpellOnTarget(uintptr_t owner,
                              std::int32_t slot,
                              uintptr_t target) {
    if (!detail::IsLocalOwner(owner) ||
        !Globals::IsValidPtr(target) ||
        slot < 0 ||
        slot > CoreCastSpell::SlotSummonerF) {
        return false;
    }

    return CoreNewCastSpell::CastTargetSpellMethod2(slot, target);
}

inline bool UpdateChargedSpell(uintptr_t owner,
                               std::int32_t slot,
                               const Vec3& position,
                               bool releaseCast) {
    if (!detail::IsLocalOwner(owner) || slot < 0 || slot > CoreCastSpell::SlotSummonerF) {
        return false;
    }

    return CoreNewCastSpell::UpdateChargedSpellMethod1(slot, position, releaseCast);
}

} // namespace CoreSpellBook
