#pragma once

#include "AIChampionEngine.h"
#include "Profiles/AIAatrox.h"
#include "Profiles/AIAhri.h"
#include "Profiles/AIAkali.h"
#include "Profiles/AIAkshan.h"
#include "Profiles/AIAlistar.h"
#include "Profiles/AIAmbessa.h"
#include "Controllers/AIAatroxController.h"
#include "Controllers/AIAhriController.h"
#include "Controllers/AIAkaliController.h"
#include "Controllers/AIAkshanController.h"
#include "Controllers/AIAlistarController.h"
#include "Controllers/AIAmbessaController.h"

#include <array>
#include <cstring>

namespace Plugins::KuroAIO::AI::Catalog {

struct ChampionEntry {
    const ChampionProfile* Profile = nullptr;
    const ChampionController* Controller = nullptr;
};

inline constexpr std::array<ChampionEntry, 6> AllChampions = {
    ChampionEntry{ &Profiles::Aatrox, &Controllers::Aatrox::Controller },
    ChampionEntry{ &Profiles::Ahri, &Controllers::Ahri::Controller },
    ChampionEntry{ &Profiles::Akali, &Controllers::Akali::Controller },
    ChampionEntry{ &Profiles::Akshan, &Controllers::Akshan::Controller },
    ChampionEntry{ &Profiles::Alistar, &Controllers::Alistar::Controller },
    ChampionEntry{ &Profiles::Ambessa, &Controllers::Ambessa::Controller },
};

inline const ChampionEntry* FindEntry(const char* championName) {
    if (!championName || !championName[0]) {
        return nullptr;
    }
    for (const auto& entry : AllChampions) {
        if (entry.Profile &&
            _stricmp(entry.Profile->ChampionName, championName) == 0) {
            return &entry;
        }
    }
    return nullptr;
}

inline const ChampionProfile* Find(const char* championName) {
    const auto* entry = FindEntry(championName);
    return entry ? entry->Profile : nullptr;
}

inline bool Supports(const char* championName) {
    return Find(championName) != nullptr;
}

inline void Load(const char* championName) {
    if (const auto* entry = FindEntry(championName)) {
        Engine::OnGameLoad(*entry->Profile, entry->Controller);
    }
}

inline void Unload() {
    Engine::OnUnload();
}

} // namespace Plugins::KuroAIO::AI::Catalog
