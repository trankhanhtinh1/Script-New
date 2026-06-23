#pragma once

#include "LastCastedSpellEntry.h"
#include "LastCastPacketSentEntry.h"

#include "../../Events/Events.h"

#include <cstdint>
#include <unordered_map>

namespace SDK {

class LastCast {
public:
    static void Initialize() {
        if (Initialized()) {
            return;
        }

        Initialized() = true;
        Events::AddOnDoCast(&AIHeroClient_OnDoCast);
        Events::AddOnProcessCastSpell(&OnCastSpell);
    }

    static LastCastedSpellEntry GetLastCastedSpell(const AIHeroClient& target) {
        Initialize();
        const auto it = CastedSpells().find(static_cast<std::uint32_t>(target.NetworkId()));
        return it != CastedSpells().end() ? it->second : LastCastedSpellEntry();
    }

    static const LastCastPacketSentEntry& LastCastPacketSent() {
        Initialize();
        return LastCastPacketSentStorage();
    }

private:
    static bool& Initialized() {
        static bool initialized = false;
        return initialized;
    }

    static std::unordered_map<std::uint32_t, LastCastedSpellEntry>& CastedSpells() {
        static std::unordered_map<std::uint32_t, LastCastedSpellEntry> castedSpells;
        return castedSpells;
    }

    static LastCastPacketSentEntry& LastCastPacketSentStorage() {
        static LastCastPacketSentEntry entry;
        return entry;
    }

    static void AIHeroClient_OnDoCast(const Events::ProcessSpellEventArgs& args) {
        if (!args.Sender.IsValid()) {
            return;
        }

        const AIHeroClient hero = ObjectManager::GetUnitByNetworkId<AIHeroClient>(
            static_cast<int>(args.Sender.NetworkId));
        if (!hero.IsValid()) {
            return;
        }

        CastedSpells()[args.Sender.NetworkId] = LastCastedSpellEntry(args);
    }

    static void OnCastSpell(const Events::CastSpellEventArgs& args) {
        if (!Events::IsLocalPlayer(args.Sender)) {
            return;
        }

        // TODO(SDK parity): Decode SpellbookCastSpellEventArgs.Slot in
        // Core::Events::DecodeProcessCastSpell, then store the real slot.
        LastCastPacketSentStorage() = LastCastPacketSentEntry(
            SpellSlot::Unknown,
            Variables::TickCount(),
            args.TargetNetworkId);
    }
};

} // namespace SDK
