#pragma once

#include "AIChampionEngine.h"
#include "Profiles/AIAatrox.h"
#include "Profiles/AIAhri.h"
#include "Profiles/AIAkali.h"
#include "Profiles/AIAkshan.h"
#include "Profiles/AIAlistar.h"
#include "Profiles/AIAmbessa.h"
#include "Profiles/AIAmumu.h"
#include "Profiles/AIAnivia.h"
#include "Profiles/AIAnnie.h"
#include "Profiles/AIAphelios.h"
#include "Profiles/AIAshe.h"
#include "Profiles/AIAurelionSol.h"
#include "Profiles/AIAurora.h"
#include "Profiles/AIAzir.h"
#include "Profiles/AIBard.h"
#include "Profiles/AIBelveth.h"
#include "Profiles/AIBlitzcrank.h"
#include "Profiles/AICaitlyn.h"
#include "Profiles/AIEzreal.h"
#include "Profiles/AIJhin.h"
#include "Profiles/AIKogMaw.h"
#include "Profiles/AIMissFortune.h"
#include "Profiles/AIRyze.h"
#include "Profiles/AITaliyah.h"
#include "Profiles/AITristana.h"
#include "Profiles/AITwitch.h"
#include "Profiles/AIVarus.h"
#include "Controllers/AIAatroxController.h"
#include "Controllers/AIAhriController.h"
#include "Controllers/AIAkaliController.h"
#include "Controllers/AIAkshanController.h"
#include "Controllers/AIAlistarController.h"
#include "Controllers/AIAmbessaController.h"
#include "Controllers/AIAmumuController.h"
#include "Controllers/AIAniviaController.h"
#include "Controllers/AIAnnieController.h"
#include "Controllers/AIApheliosController.h"
#include "Controllers/AIAsheController.h"
#include "Controllers/AIAurelionSolController.h"
#include "Controllers/AIAuroraController.h"
#include "Controllers/AIAzirController.h"
#include "Controllers/AIBardController.h"
#include "Controllers/AIBelvethController.h"
#include "Controllers/AIBlitzcrankController.h"
#include "Controllers/AICaitlynController.h"
#include "Controllers/AIEzrealController.h"
#include "Controllers/AIJhinController.h"
#include "Controllers/AIKogMawController.h"
#include "Controllers/AIMissFortuneController.h"
#include "Controllers/AIRyzeController.h"
#include "Controllers/AITaliyahController.h"
#include "Controllers/AITristanaController.h"
#include "Controllers/AITwitchController.h"
#include "Controllers/AIVarusController.h"

#include <array>
#include <cstring>

namespace Plugins::KuroAIO::AI::Catalog {

struct ChampionEntry {
    const ChampionProfile* Profile = nullptr;
    const ChampionController* Controller = nullptr;
};

inline constexpr std::array<ChampionEntry, 27> AllChampions = {
    ChampionEntry{ &Profiles::Aatrox, &Controllers::Aatrox::Controller },
    ChampionEntry{ &Profiles::Ahri, &Controllers::Ahri::Controller },
    ChampionEntry{ &Profiles::Akali, &Controllers::Akali::Controller },
    ChampionEntry{ &Profiles::Akshan, &Controllers::Akshan::Controller },
    ChampionEntry{ &Profiles::Alistar, &Controllers::Alistar::Controller },
    ChampionEntry{ &Profiles::Ambessa, &Controllers::Ambessa::Controller },
    ChampionEntry{ &Profiles::Amumu, &Controllers::Amumu::Controller },
    ChampionEntry{ &Profiles::Anivia, &Controllers::Anivia::Controller },
    ChampionEntry{ &Profiles::Annie, &Controllers::Annie::Controller },
    ChampionEntry{ &Profiles::Aphelios, &Controllers::Aphelios::Controller },
    ChampionEntry{ &Profiles::Ashe, &Controllers::Ashe::Controller },
    ChampionEntry{ &Profiles::AurelionSol,
                   &Controllers::AurelionSol::Controller },
    ChampionEntry{ &Profiles::Aurora, &Controllers::Aurora::Controller },
    ChampionEntry{ &Profiles::Azir, &Controllers::Azir::Controller },
    ChampionEntry{ &Profiles::Bard, &Controllers::Bard::Controller },
    ChampionEntry{ &Profiles::Belveth, &Controllers::Belveth::Controller },
    ChampionEntry{ &Profiles::Blitzcrank,
                   &Controllers::Blitzcrank::Controller },
    ChampionEntry{ &Profiles::Caitlyn, &Controllers::Caitlyn::Controller },
    ChampionEntry{ &Profiles::Ezreal, &Controllers::Ezreal::Controller },
    ChampionEntry{ &Profiles::Jhin, &Controllers::Jhin::Controller },
    ChampionEntry{ &Profiles::KogMaw, &Controllers::KogMaw::Controller },
    ChampionEntry{ &Profiles::MissFortune,
                   &Controllers::MissFortune::Controller },
    ChampionEntry{ &Profiles::Ryze, &Controllers::Ryze::Controller },
    ChampionEntry{ &Profiles::Taliyah, &Controllers::Taliyah::Controller },
    ChampionEntry{ &Profiles::Tristana, &Controllers::Tristana::Controller },
    ChampionEntry{ &Profiles::Twitch, &Controllers::Twitch::Controller },
    ChampionEntry{ &Profiles::Varus, &Controllers::Varus::Controller },
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
