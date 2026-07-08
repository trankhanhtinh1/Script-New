#pragma once

// ============================================================================
// KuroAIO — champion dispatcher for Kuro-owned ports.
// Mirrors the small 7UPAIO loader pattern, but keeps non-7UP champions separate.
// ============================================================================

#include "../../IPlugin.h"
#include "../../../SDK/SDK.h"

#include <string>

#include "Champion/Katarina.h"
#include "Champion/Samira.h"
#include "Champion/Yasuo/Yasuo.h"

namespace Plugins {

class KuroAIOPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "KuroAIO"; }
    const char* GetInternalId() const override {
        const std::string champ = CurrentChampionName();
        if (_stricmp(champ.c_str(), "Katarina") == 0) {
            return "champion.kuroaio.katarina";
        }
        if (_stricmp(champ.c_str(), "Samira") == 0) {
            return "champion.kuroaio.samira";
        }
        if (_stricmp(champ.c_str(), "Yasuo") == 0) {
            return "champion.kuroaio.yasuo";
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
        } else if (_stricmp(champ.c_str(), "Samira") == 0) {
            KuroAIO::Samira::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Yasuo") == 0) {
            KuroAIO::Yasuo::OnGameLoad();
        }
    }

    void OnUnload() override {
        KuroAIO::Katarina::OnUnload();
        KuroAIO::Samira::OnUnload();
        KuroAIO::Yasuo::OnUnload();
    }

private:
    static bool IsSupportedChampionName(const char* championName) {
        return championName && championName[0] &&
            (_stricmp(championName, "Katarina") == 0 ||
             _stricmp(championName, "Samira") == 0 ||
             _stricmp(championName, "Yasuo") == 0);
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
        if (_stricmp(championName.c_str(), "Samira") == 0) {
            return "Samira";
        }
        if (_stricmp(championName.c_str(), "Yasuo") == 0) {
            return "Yasuo";
        }
        return nullptr;
    }
};

} // namespace Plugins
