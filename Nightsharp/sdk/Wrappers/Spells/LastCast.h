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
        ::SDK::Events::AddOnDoCast(&AIHeroClient_OnDoCast);
        ::SDK::Events::AddOnProcessCastSpell(&OnCastSpell);
    }

    static void Shutdown() {
        if (!Initialized()) {
            return;
        }

        ::SDK::Events::RemoveOnProcessCastSpell(&OnCastSpell);
        ::SDK::Events::RemoveOnDoCast(&AIHeroClient_OnDoCast);
        if (auto* spells = CastedSpells()) {
            spells->clear();
        }
        Initialized() = false;
    }

    static const LastCastedSpellEntry* Get(const AIBaseClient& target) {
        if (!target.IsValid()) {
            return nullptr;
        }

        auto* spells = CastedSpells();
        if (!spells) {
            return nullptr;
        }

        const auto it = spells->find(target.NetworkId());
        if (it == spells->end() || !it->second.IsValid) {
            return nullptr;
        }

        return &it->second;
    }

    static LastCastedSpellEntry GetLastCastedSpell(const AIBaseClient& target) {
        if (const auto* ptr = Get(target)) {
            return *ptr;
        }
        return {};
    }

    static const LastCastPacketSentEntry& LastCastPacketSent() {
        return LastCastPacketSentStorage();
    }

private:
    static bool& Initialized() {
        static bool initialized = false;
        return initialized;
    }

    static std::unordered_map<std::uint32_t, LastCastedSpellEntry>* CastedSpellsStorage() {
        static std::unordered_map<std::uint32_t, LastCastedSpellEntry> storage;
        return &storage;
    }

    static std::unordered_map<std::uint32_t, LastCastedSpellEntry>* CastedSpells() {
        return CastedSpellsStorage();
    }

    static LastCastPacketSentEntry& LastCastPacketSentStorage() {
        static LastCastPacketSentEntry entry;
        return entry;
    }

    static void AIHeroClient_OnDoCast(const ::SDK::Events::ProcessSpellEventArgs& args) {
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

        if (spells->size() > 64) {
            const float now = static_cast<float>(::SDK::Variables::TickCount());
            for (auto it = spells->begin(); it != spells->end(); ) {
                if (now - it->second.StartTime > 15000.0f) {
                    it = spells->erase(it);
                } else {
                    ++it;
                }
            }
        }

        auto [it, inserted] = spells->try_emplace(args.Sender.NetworkId, LastCastedSpellEntry(args));
        if (!inserted) {
            it->second = LastCastedSpellEntry(args);
        }
    }

    static void OnCastSpell(const ::SDK::Events::CastSpellEventArgs& args) {
        if (!::SDK::Events::IsLocalPlayer(args.Sender)) {
            return;
        }

        LastCastPacketSentStorage() = LastCastPacketSentEntry(
            static_cast<SpellSlot>(args.Slot),
            Variables::TickCount(),
            args.TargetNetworkId);
    }
};

} // namespace SDK
