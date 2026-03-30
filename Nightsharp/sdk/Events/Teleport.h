#pragma once

#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"
#include "../Enumerations/TeleportStatus.h"
#include "../Enumerations/TeleportType.h"

#include <new>
#include <string>
#include <unordered_map>

namespace SDK::Events::Teleport {

struct TeleportEventArgs {
    int Duration = 0;
    bool IsTarget = false;
    AIBaseClient Object = {};
    int Start = 0;
    TeleportStatus Status = TeleportStatus::Unknown;
    TeleportType Type = TeleportType::Unknown;

    bool IsValid() const {
        return Object.IsValid() && Type != TeleportType::Unknown;
    }
};

using TeleportHandler = void(*)(const TeleportEventArgs&);

namespace detail {
    struct TeleportState {
        TeleportEventArgs Args = {};
        bool Active = false;
        std::string RecallName = {};
        int LastUpdateTick = 0;
    };

    inline std::unordered_map<int, TeleportState>* g_teleports = nullptr;
    inline MenuUI::FixedList<TeleportHandler, 64> g_handlers = {};

    inline bool EnsureStorage() {
        if (!g_teleports) {
            g_teleports = new(std::nothrow) std::unordered_map<int, TeleportState>();
        }
        return g_teleports != nullptr;
    }

    inline int GetRecallDurationMs(const std::string& recallName) {
        if (recallName.empty()) {
            return 8000;
        }

        if (lstrcmpiA(recallName.c_str(), "recall") == 0) {
            return 8000;
        }
        if (lstrcmpiA(recallName.c_str(), "recallimproved") == 0) {
            return 7000;
        }
        if (lstrcmpiA(recallName.c_str(), "odinrecall") == 0) {
            return 4500;
        }
        if (lstrcmpiA(recallName.c_str(), "odinrecallimproved") == 0 ||
            lstrcmpiA(recallName.c_str(), "superrecall") == 0 ||
            lstrcmpiA(recallName.c_str(), "superrecallimproved") == 0) {
            return 4000;
        }

        return 8000;
    }

    inline bool ReadActiveSpellName(const AIHeroClient& hero, std::string& outName) {
        const auto cast = hero.Ref().GetActiveSpellCast();
        if (!cast.IsValid()) {
            return false;
        }

        char spellNameBuffer[128] = {};
        if (!cast.ReadSpellName(spellNameBuffer, static_cast<int>(sizeof(spellNameBuffer)))) {
            return false;
        }

        outName.assign(spellNameBuffer);
        return !outName.empty();
    }

    inline TeleportType DetectTeleportType(const AIHeroClient& hero, std::string& recallName, int& durationMs) {
        recallName.clear();
        durationMs = 0;

        if (hero.IsRecalling()) {
            const auto recallSpell = hero.GetSpell(SpellSlot::Recall);
            recallName = recallSpell.Name();
            durationMs = GetRecallDurationMs(recallName);
            return TeleportType::Recall;
        }

        if (hero.HasBuff("summonerteleport") ||
            hero.HasBuff("SummonerTeleport") ||
            hero.HasBuff("teleport_target")) {
            durationMs = 4000;
            return TeleportType::Teleport;
        }

        if (hero.HasBuff("gate") ||
            hero.HasBuff("TwistedFateR") ||
            hero.HasBuff("Destiny")) {
            durationMs = 1500;
            return TeleportType::TwistedFate;
        }

        if (hero.HasBuff("ShenRChannel") ||
            hero.HasBuff("shenrchannelbuff") ||
            hero.HasBuff("ShenStandUnited")) {
            durationMs = 3000;
            return TeleportType::Shen;
        }

        std::string activeSpellName = {};
        if (ReadActiveSpellName(hero, activeSpellName)) {
            if (lstrcmpiA(activeSpellName.c_str(), "summonerteleport") == 0) {
                durationMs = 4000;
                return TeleportType::Teleport;
            }
            if (lstrcmpiA(activeSpellName.c_str(), "gate") == 0) {
                durationMs = 1500;
                return TeleportType::TwistedFate;
            }
            if (_strnicmp(activeSpellName.c_str(), "shenr", 5) == 0) {
                durationMs = 3000;
                return TeleportType::Shen;
            }
        }

        return TeleportType::Unknown;
    }

    inline void FireEvent(const TeleportEventArgs& args) {
        for (const auto& handler : g_handlers) {
            if (handler) {
                handler(args);
            }
        }
    }
}

inline void Initialize() {
    detail::EnsureStorage();
}

inline bool AddOnTeleport(TeleportHandler handler) {
    return handler && detail::g_handlers.push_back(handler);
}

inline bool OnTeleport(TeleportHandler handler) {
    return AddOnTeleport(handler);
}

inline TeleportEventArgs GetTeleportData(const AIBaseClient& object) {
    if (!detail::g_teleports || !object.IsValid()) {
        return {};
    }

    const auto it = detail::g_teleports->find(object.NetworkId());
    return (it != detail::g_teleports->end()) ? it->second.Args : TeleportEventArgs{};
}

inline void Update() {
    if (!detail::EnsureStorage()) {
        return;
    }

    const int now = Game::TickCount();
    for (const auto& hero : ObjectManager::Heroes()) {
        const int netId = hero.NetworkId();
        if (!hero.IsValid() || netId == 0 || hero.IsDead()) {
            if (netId != 0) {
                detail::g_teleports->erase(netId);
            }
            continue;
        }

        std::string recallName = {};
        int durationMs = 0;
        const TeleportType detectedType = detail::DetectTeleportType(hero, recallName, durationMs);
        auto& state = (*detail::g_teleports)[netId];

        if (detectedType != TeleportType::Unknown) {
            if (!state.Active || state.Args.Type != detectedType) {
                state.Active = true;
                state.RecallName = recallName;
                state.LastUpdateTick = now;
                state.Args = {};
                state.Args.Object = hero;
                state.Args.Type = detectedType;
                state.Args.Status = TeleportStatus::Start;
                state.Args.Duration = durationMs > 0 ? durationMs : 4000;
                state.Args.Start = now;
                state.Args.IsTarget = false;
                detail::FireEvent(state.Args);
            } else {
                state.LastUpdateTick = now;
            }
            continue;
        }

        if (!state.Active) {
            continue;
        }

        const int elapsed = now - state.Args.Start;
        state.Active = false;
        state.Args.Status = (elapsed + 100 >= state.Args.Duration) ? TeleportStatus::Finish : TeleportStatus::Abort;
        detail::FireEvent(state.Args);
    }
}

inline void Reset() {
    if (detail::g_teleports) {
        detail::g_teleports->clear();
    }
    detail::g_handlers.clear();
}

} // namespace SDK::Events::Teleport
