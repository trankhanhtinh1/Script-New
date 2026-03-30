#pragma once

#include "../../generated/InterruptableSpellData.generated.h"
#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace SDK {

class Interrupter {
public:
    using DangerLevel = SDK::DangerLevel;

    struct InterruptSpellArgs {
        SpellSlot Slot = SpellSlot::Unknown;
        SDK::DangerLevel DangerLevel = SDK::DangerLevel::Low;
        bool MovementInterrupts = true;
        std::string SpellName = {};
        float EndTime = 0.0f;

        bool IsValid() const {
            return Slot != SpellSlot::Unknown || !SpellName.empty();
        }
    };

    struct ActiveSpellEntry {
        AIHeroClient Sender = {};
        InterruptSpellArgs Args = {};

        bool IsValid() const {
            return Sender.IsValid() && Args.IsValid();
        }
    };

    using Handler = void(*)(const AIHeroClient&, const InterruptSpellArgs&);

    static void Initialize() {
        if (s_initialized) {
            return;
        }
        s_initialized = true;
    }

    static bool AddOnInterrupterSpell(Handler handler) {
        return handler && s_handlers.push_back(handler);
    }

    static bool OnInterrupterSpell(Handler handler) {
        return AddOnInterrupterSpell(handler);
    }

    static bool AddOnInterruptableTarget(Handler handler) {
        return AddOnInterrupterSpell(handler);
    }

    static bool OnInterruptableTarget(Handler handler) {
        return AddOnInterruptableTarget(handler);
    }

    static InterruptSpellArgs GetInterruptableTargetData(const AIHeroClient& target) {
        if (!s_active) {
            return InterruptSpellArgs{};
        }
        const auto it = s_active->find(target.NetworkId());
        return it != s_active->end() ? it->second.Args : InterruptSpellArgs{};
    }

    static std::vector<ActiveSpellEntry> CastingInterruptableSpell() {
        std::vector<ActiveSpellEntry> out = {};
        if (!s_active) {
            return out;
        }

        out.reserve(s_active->size());
        for (const auto& [_, state] : *s_active) {
            ActiveSpellEntry entry = {};
            entry.Sender = state.Sender;
            entry.Args = state.Args;
            if (entry.IsValid()) {
                out.push_back(entry);
            }
        }
        return out;
    }

    static bool IsCastingInterruptableSpell(const AIHeroClient& target, bool checkMovementInterruption = false) {
        const auto args = GetInterruptableTargetData(target);
        return args.IsValid() && (!checkMovementInterruption || args.MovementInterrupts);
    }

    static void Update() {
        Initialize();
        if (!EnsureStorage()) {
            return;
        }

        std::unordered_map<int, ActiveState> nextActive = {};
        for (const auto& hero : ObjectManager::EnemyHeroes()) {
            ActiveState state = {};
            if (TryBuildState(hero, state)) {
                nextActive[hero.NetworkId()] = state;
            }
        }

        s_active->swap(nextActive);
        if (s_handlers.empty()) {
            return;
        }

        for (const auto& [_, state] : *s_active) {
            for (const auto& handler : s_handlers) {
                if (handler) {
                    handler(state.Sender, state.Args);
                }
            }
        }
    }

    static void Reset() {
        s_handlers.clear();
        if (s_active) {
            s_active->clear();
        }
        s_initialized = false;
    }

private:
    struct ActiveState {
        AIHeroClient Sender = {};
        InterruptSpellArgs Args = {};
        uintptr_t CastAddress = 0;
    };

    static std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(::tolower(c));
        });
        return value;
    }

    static bool EqualsInsensitive(const std::string& lhs, const char* rhs) {
        return rhs && lstrcmpiA(lhs.c_str(), rhs) == 0;
    }

    static const Generated::InterruptableSpellData::InterruptableEntry* FindChampionEntry(
        const AIHeroClient& hero,
        SpellSlot slot,
        const std::string& spellNameLower) {
        const std::string champion = hero.CharacterName();
        if (champion.empty()) {
            return nullptr;
        }

        for (const auto& entry : Generated::InterruptableSpellData::kChampionInterruptables) {
            if (!EqualsInsensitive(champion, entry.ChampionName) || entry.Slot != slot) {
                continue;
            }
            if (entry.Name && entry.Name[0] != 0 && spellNameLower != entry.Name) {
                continue;
            }
            return &entry;
        }

        return nullptr;
    }

    static const Generated::InterruptableSpellData::InterruptableEntry* FindGlobalEntry(const std::string& spellNameLower) {
        for (const auto& entry : Generated::InterruptableSpellData::kGlobalInterruptables) {
            if (entry.Name && spellNameLower == entry.Name) {
                return &entry;
            }
        }
        return nullptr;
    }

    static bool TryBuildState(const AIHeroClient& hero, ActiveState& out) {
        if (!hero.IsValid() || hero.IsDead()) {
            return false;
        }

        const auto cast = hero.Ref().GetActiveSpellCast();
        if (!cast.IsValid()) {
            return false;
        }

        char spellNameBuffer[128] = {};
        const bool hasSpellName = cast.ReadSpellName(spellNameBuffer, static_cast<int>(sizeof(spellNameBuffer)));
        const std::string spellNameLower = hasSpellName ? ToLower(std::string(spellNameBuffer)) : std::string();
        const SpellSlot slot = (cast.GetSlot() >= 0 && cast.GetSlot() <= static_cast<int>(SpellSlot::R))
            ? static_cast<SpellSlot>(cast.GetSlot())
            : SpellSlot::Unknown;

        const auto* globalEntry = spellNameLower.empty() ? nullptr : FindGlobalEntry(spellNameLower);
        const auto* championEntry = FindChampionEntry(hero, slot, spellNameLower);
        if (!globalEntry && !championEntry) {
            return false;
        }

        const auto* entry = championEntry ? championEntry : globalEntry;

        out.Sender = hero;
        out.CastAddress = cast.address;
        out.Args.Slot = entry->Slot;
        out.Args.DangerLevel = entry->DangerLevel;
        out.Args.MovementInterrupts = entry->MovementInterrupts;
        out.Args.SpellName = spellNameLower;
        out.Args.EndTime = Game::Time() + std::max(cast.GetCastDelay(), 0.25f);
        return true;
    }

    static bool EnsureStorage() {
        if (!s_active) {
            s_active = new(std::nothrow) std::unordered_map<int, ActiveState>();
        }
        return s_active != nullptr;
    }

    static inline bool s_initialized = false;
    static inline MenuUI::FixedList<Handler, 64> s_handlers = {};
    static inline std::unordered_map<int, ActiveState>* s_active = nullptr;
};

} // namespace SDK
