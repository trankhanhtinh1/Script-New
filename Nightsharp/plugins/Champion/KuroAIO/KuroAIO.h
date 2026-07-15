#pragma once

// ============================================================================
// KuroAIO — champion dispatcher for Kuro-owned ports.
// Mirrors the small 7UPAIO loader pattern, but keeps non-7UP champions separate.
// ============================================================================

#include "../../IPlugin.h"
#include "../../../SDK/SDK.h"

#include <string>

#include "Champion/Katarina.h"
#include "Champion/Lucian.h"
#include "Champion/Samira.h"
#include "Champion/Senna.h"
#include "Champion/Syndra.h"
#include "Champion/TwistedFate.h"
#include "Champion/Viktor.h"
#include "Champion/Yasuo/Yasuo.h"
#include "Champion/Fiora/Fiora.h"

namespace Plugins {

class KuroAIOPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "KuroAIO"; }
    const char* GetInternalId() const override {
        const std::string champ = CurrentChampionName();
        if (_stricmp(champ.c_str(), "Katarina") == 0) {
            return "champion.kuroaio.katarina";
        }
        if (_stricmp(champ.c_str(), "Lucian") == 0) {
            return "champion.kuroaio.lucian";
        }
        if (_stricmp(champ.c_str(), "Samira") == 0) {
            return "champion.kuroaio.samira";
        }
        if (_stricmp(champ.c_str(), "Senna") == 0) {
            return "champion.kuroaio.senna";
        }
        if (_stricmp(champ.c_str(), "Syndra") == 0) {
            return "champion.kuroaio.syndra";
        }
        if (_stricmp(champ.c_str(), "Yasuo") == 0) {
            return "champion.kuroaio.yasuo";
        }
        if (_stricmp(champ.c_str(), "Fiora") == 0) {
            return "champion.kuroaio.fiora";
        }
        if (_stricmp(champ.c_str(), "TwistedFate") == 0) {
            return "champion.kuroaio.twistedfate";
        }
        if (_stricmp(champ.c_str(), "Viktor") == 0) {
            return "champion.kuroaio.viktor";
        }
        return "champion.kuroaio";
    }
    const char* GetAuthor() const override { return "Kuro"; }
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    const char* GetChampionName() const override { return CurrentSupportedChampionName(); }
    bool AutoLoadByDefault() const override { return true; }
    bool CanLoad() const override { return IsCurrentChampionSupported(); }

    void OnLoad() override {
        const std::string champ = CurrentChampionName();
        if (!IsSupportedChampionName(champ.c_str())) {
            return;
        }

        if (_stricmp(champ.c_str(), "Katarina") == 0) {
            KuroAIO::Katarina::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Lucian") == 0) {
            KuroAIO::Lucian::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Samira") == 0) {
            KuroAIO::Samira::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Senna") == 0) {
            KuroAIO::Senna::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Syndra") == 0) {
            KuroAIO::Syndra::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Yasuo") == 0) {
            KuroAIO::Yasuo::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Fiora") == 0) {
            KuroAIO::Fiora::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "TwistedFate") == 0) {
            KuroAIO::TwistedFate::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Viktor") == 0) {
            KuroAIO::Viktor::OnGameLoad();
        }
    }

    void OnUnload() override {
        KuroAIO::Katarina::OnUnload();
        KuroAIO::Lucian::OnUnload();
        KuroAIO::Samira::OnUnload();
        KuroAIO::Senna::OnUnload();
        KuroAIO::Syndra::OnUnload();
        KuroAIO::Yasuo::OnUnload();
        KuroAIO::Fiora::OnUnload();
        KuroAIO::TwistedFate::OnUnload();
        KuroAIO::Viktor::OnUnload();
    }

private:
    static bool IsSupportedChampionName(const char* championName) {
        return championName && championName[0] &&
            (_stricmp(championName, "Katarina") == 0 ||
             _stricmp(championName, "Lucian") == 0 ||
             _stricmp(championName, "Samira") == 0 ||
             _stricmp(championName, "Senna") == 0 ||
             _stricmp(championName, "Syndra") == 0 ||
             _stricmp(championName, "Yasuo") == 0 ||
             _stricmp(championName, "Fiora") == 0 ||
             _stricmp(championName, "TwistedFate") == 0 ||
             _stricmp(championName, "Viktor") == 0);
    }

    static std::string CurrentChampionName() {
        const std::string& cached = SDK::GameObject::GetCachedChampionName();
        if (!cached.empty()) {
            return cached;
        }

        const auto player = ObjectManager::Player();
        return player.IsValid() ? player.CharacterName() : std::string();
    }

    static bool IsCurrentChampionSupported() {
        const std::string championName = CurrentChampionName();
        return IsSupportedChampionName(championName.c_str());
    }

    static const char* CurrentSupportedChampionName() {
        const std::string championName = CurrentChampionName();
        if (_stricmp(championName.c_str(), "Katarina") == 0) {
            return "Katarina";
        }
        if (_stricmp(championName.c_str(), "Lucian") == 0) {
            return "Lucian";
        }
        if (_stricmp(championName.c_str(), "Samira") == 0) {
            return "Samira";
        }
        if (_stricmp(championName.c_str(), "Senna") == 0) {
            return "Senna";
        }
        if (_stricmp(championName.c_str(), "Syndra") == 0) {
            return "Syndra";
        }
        if (_stricmp(championName.c_str(), "Yasuo") == 0) {
            return "Yasuo";
        }
        if (_stricmp(championName.c_str(), "Fiora") == 0) {
            return "Fiora";
        }
        if (_stricmp(championName.c_str(), "TwistedFate") == 0) {
            return "TwistedFate";
        }
        if (_stricmp(championName.c_str(), "Viktor") == 0) {
            return "Viktor";
        }
        return nullptr;
    }
};

} // namespace Plugins

