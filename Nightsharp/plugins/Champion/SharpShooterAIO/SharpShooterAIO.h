#pragma once

// ============================================================================
// SharpShooterAIO — Port từ SharpShooterCSHarp (LeagueSharp/EloBuddy) sang
// NightSharp C++.
//
// Một plugin duy nhất "SharpShooterAIO" dispatch theo champion name giống
// Program.cs/Initializer.cs của bản C#. Mỗi champion viết trong file .h riêng,
// namespace Plugins::SharpAIO::<Champ>, dùng Facade SDK.
//
// Chuẩn kiến trúc: plugins/Champion/7UPAIO/Ezreal.h.
// Skill charged: plugins/Champion/XerathSemiPlugin.h.
//
// Comment = chưa port. Uncomment khi port xong champion đó.
// Ưu tiên: các tướng AD.
// ============================================================================

#include "../../IPlugin.h"
#include "../../../SDK/SDK.h"

#include <string>

// === Champion includes (uncomment khi port xong) ===
#include "Ashe.h"
#include "Tristana.h"
#include "Caitlyn.h"
#include "Corki.h"
#include "Draven.h"
#include "Graves.h"
#include "Jinx.h"
#include "Kalista.h"
#include "KogMaw.h"
#include "Lucian.h"
#include "MissFortune.h"
#include "Sivir.h"
#include "Twitch.h"
#include "Varus.h"
#include "Vayne.h"
#include "Zeri.h"

namespace Plugins {

class SharpShooterAIOPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "SharpShooterAIO"; }
    const char* GetInternalId() const override { return "champion.sharpshooteraio"; }
    const char* GetAuthor() const override { return "SharpShooter"; }
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    const char* GetChampionName() const override { return CurrentSupportedChampionName(); }
    bool AutoLoadByDefault() const override { return true; }
    bool CanLoad() const override { return IsCurrentChampionSupported(); }

    void OnLoad() override {
        const std::string champ = CurrentChampionName();
        if (!IsSupportedChampionName(champ.c_str())) {
            return;
        }

        // === Champion dispatch (port từ Initializer.cs switch) ===
        if (false) {
        }
        else if (_stricmp(champ.c_str(), "Ashe") == 0)       { SharpAIO::Ashe::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Tristana") == 0)   { SharpAIO::Tristana::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Caitlyn") == 0)    { SharpAIO::Caitlyn::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Corki") == 0)      { SharpAIO::Corki::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Draven") == 0)     { SharpAIO::Draven::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Graves") == 0)     { SharpAIO::Graves::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Jinx") == 0)       { SharpAIO::Jinx::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Kalista") == 0)    { SharpAIO::Kalista::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "KogMaw") == 0)     { SharpAIO::KogMaw::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Lucian") == 0)     { SharpAIO::Lucian::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "MissFortune") == 0){ SharpAIO::MissFortune::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Sivir") == 0)      { SharpAIO::Sivir::OnGameLoad(); }
        // else if (_stricmp(champ.c_str(), "Tristana") == 0)   { SharpAIO::Tristana::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Twitch") == 0)     { SharpAIO::Twitch::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Varus") == 0)      { SharpAIO::Varus::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Vayne") == 0)      { SharpAIO::Vayne::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Zeri") == 0)       { SharpAIO::Zeri::OnGameLoad(); }
        else {
            return;
        }
    }

    void OnUnload() override {
        SharpAIO::Ashe::OnUnload();
        SharpAIO::Tristana::OnUnload();
        SharpAIO::Caitlyn::OnUnload();
        SharpAIO::Corki::OnUnload();
        SharpAIO::Draven::OnUnload();
        SharpAIO::Graves::OnUnload();
        SharpAIO::Jinx::OnUnload();
        SharpAIO::Kalista::OnUnload();
        SharpAIO::KogMaw::OnUnload();
        SharpAIO::Lucian::OnUnload();
        SharpAIO::MissFortune::OnUnload();
        SharpAIO::Sivir::OnUnload();
        SharpAIO::Twitch::OnUnload();
        SharpAIO::Varus::OnUnload();
        SharpAIO::Vayne::OnUnload();
        SharpAIO::Zeri::OnUnload();
    }

private:
    static bool IsSupportedChampionName(const char* championName) {
        return championName && championName[0] &&
            (_stricmp(championName, "Ashe") == 0 ||
             _stricmp(championName, "Tristana") == 0 ||
             _stricmp(championName, "MissFortune") == 0 ||
             _stricmp(championName, "Corki") == 0 ||
             _stricmp(championName, "KogMaw") == 0 ||
             _stricmp(championName, "Twitch") == 0 ||
             _stricmp(championName, "Lucian") == 0 ||
             _stricmp(championName, "Jinx") == 0 ||
             _stricmp(championName, "Sivir") == 0 ||
             _stricmp(championName, "Varus") == 0 ||
             _stricmp(championName, "Kalista") == 0 ||
             _stricmp(championName, "Draven") == 0 ||
             _stricmp(championName, "Caitlyn") == 0 ||
             _stricmp(championName, "Vayne") == 0 ||
             _stricmp(championName, "Graves") == 0 ||
             _stricmp(championName, "Zeri") == 0);
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
        static std::string cached;
        const std::string champ = CurrentChampionName();
        if (!IsSupportedChampionName(champ.c_str())) {
            return nullptr;
        }
        cached = champ;
        return cached.c_str();
    }
};

} // namespace Plugins
