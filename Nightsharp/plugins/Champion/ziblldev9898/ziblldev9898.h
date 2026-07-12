#pragma once
// ============================================================================
// ziblldev9898.h - AIO dispatcher (ten folder = ten nguoi dev)
//
// Theo skill port-champion: file dispatcher dat theo ten dev/folder, con logic
// tung tuong nam o file rieng ten theo tuong (Locke.h). Dispatcher detect champion
// dang choi va forward lifecycle (giong plugins/Champion/7UPAIO/7UPAIO.h).
// ============================================================================

#include "../../IPlugin.h"
#include "../../../SDK/SDK.h"

#include "Locke.h"
#include "Ezreal.h"
#include "Leesin.h"
#include "Irelia.h"

#include <string>

namespace Plugins {

class Ziblldev9898Plugin final : public IPlugin {
public:
    const char* GetName() const override { return "Ziblldev9898"; }
    const char* GetInternalId() const override { return "champion.ziblldev9898"; }
    const char* GetAuthor() const override { return "ziblldev9898"; }
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    const char* GetChampionName() const override { return CurrentSupportedChampionName(); }
    bool AutoLoadByDefault() const override { return true; }
    bool CanLoad() const override { return IsCurrentChampionSupported(); }

    void OnLoad() override {
        const std::string champ = CurrentChampionName();
        if (_stricmp(champ.c_str(), "Locke") == 0) {
            ziblldev9898::Locke::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Ezreal") == 0) {
            ziblldev9898::Ezreal::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "LeeSin") == 0) {
            ziblldev9898::LeeSin::OnGameLoad();
        } else if (_stricmp(champ.c_str(), "Irelia") == 0) {
            ziblldev9898::Irelia::OnGameLoad();
        }
    }

    void OnUnload() override {
        ziblldev9898::Locke::OnUnload();
        ziblldev9898::Ezreal::OnUnload();
        ziblldev9898::LeeSin::OnUnload();
        ziblldev9898::Irelia::OnUnload();
    }

private:
    static bool IsSupportedChampionName(const char* championName) {
        return championName && championName[0] &&
               (_stricmp(championName, "Locke") == 0 ||
                _stricmp(championName, "Ezreal") == 0 ||
                _stricmp(championName, "LeeSin") == 0 ||
                _stricmp(championName, "Irelia") == 0);
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
        return IsSupportedChampionName(CurrentChampionName().c_str());
    }

    static const char* CurrentSupportedChampionName() {
        const std::string champion = CurrentChampionName();
        if (_stricmp(champion.c_str(), "Locke") == 0) return "Locke";
        if (_stricmp(champion.c_str(), "Ezreal") == 0) return "Ezreal";
        if (_stricmp(champion.c_str(), "LeeSin") == 0) return "LeeSin";
        if (_stricmp(champion.c_str(), "Irelia") == 0) return "Irelia";
        return nullptr;
    }
};

} // namespace Plugins
