#pragma once

#include "LastCastedSpellEntry.h"
#include "LastCastPacketSentEntry.h"

#include "../../Events/Events.h"

#include <cstdint>
#include <new>
#include <unordered_map>

namespace SDK {

class LastCast {
public:
    static void Initialize() {
        if (Initialized()) {
            return;
        }

        auto* spells = CastedSpells();
        if (!spells) {
            return;
        }

        Initialized() = true;
        Events::AddOnDoCast(&AIHeroClient_OnDoCast);
        Events::AddOnProcessCastSpell(&OnCastSpell);
    }

    static void Shutdown() {
        if (!Initialized()) {
            return;
        }

        Events::RemoveOnProcessCastSpell(&OnCastSpell);
        Events::RemoveOnDoCast(&AIHeroClient_OnDoCast);
        if (auto* spells = CastedSpells()) {
            spells->clear();
        }
        LastCastPacketSentStorage() = LastCastPacketSentEntry();
        Initialized() = false;
    }

    static LastCastedSpellEntry GetLastCastedSpell(const AIHeroClient& target) {
        Initialize();
        auto* spells = CastedSpells();
        if (!spells) {
            return LastCastedSpellEntry();
        }
        const auto it = spells->find(static_cast<std::uint32_t>(target.NetworkId()));
        return it != spells->end() ? it->second : LastCastedSpellEntry();
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

    static std::unordered_map<std::uint32_t, LastCastedSpellEntry>*& CastedSpellsStorage() {
        static auto* castedSpells = new(std::nothrow) std::unordered_map<std::uint32_t, LastCastedSpellEntry>();
        return castedSpells;
    }

    static std::unordered_map<std::uint32_t, LastCastedSpellEntry>* CastedSpells() {
        return CastedSpellsStorage();
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

        auto* spells = CastedSpells();
        if (!spells) {
            return;
        }

        auto [it, inserted] = spells->try_emplace(args.Sender.NetworkId, LastCastedSpellEntry(args));
        if (!inserted) {
            it->second = LastCastedSpellEntry(args);
        }
    }

    static void OnCastSpell(const Events::CastSpellEventArgs& args) {
        if (!Events::IsLocalPlayer(args.Sender)) {
            return;
        }

        LastCastPacketSentStorage() = LastCastPacketSentEntry(
            static_cast<SpellSlot>(args.Slot),
            Variables::TickCount(),
            args.TargetNetworkId);
    }
};

} // namespace SDK
