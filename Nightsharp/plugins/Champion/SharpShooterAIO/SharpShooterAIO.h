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
#include "Blitzcrank.h"
#include "Brand.h"
#include "Cassiopeia.h"
#include "Tristana.h"
#include "Caitlyn.h"
#include "Corki.h"
#include "Draven.h"
#include "LeeSin.h"
#include "Yone.h"
#include "Ezreal.h"
#include "Graves.h"
#include "Jhin.h"
#include "Irelia.h"
#include "Jinx.h"
#include "Kaisa.h"
#include "Kalista.h"
#include "Kindred.h"
#include "KogMaw.h"
#include "Lucian.h"
#include "MissFortune.h"
#include "Orianna.h"
#include "Renata.h"
#include "Samira.h"
#include "Senna.h"
#include "Sivir.h"
#include "Syndra.h"
#include "Thresh.h"
#include "TwistedFate.h"
#include "Twitch.h"
#include "Varus.h"
#include "Vayne.h"
#include "Viktor.h"
#include "Xerath.h"
#include "Yasuo.h"
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
        else if (_stricmp(champ.c_str(), "Blitzcrank") == 0) { SharpAIO::Blitzcrank::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Brand") == 0)      { SharpAIO::Brand::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Cassiopeia") == 0) { SharpAIO::Cassiopeia::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "LeeSin") == 0)     { SharpAIO::LeeSin::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Yone") == 0)       { SharpAIO::Yone::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Tristana") == 0)   { SharpAIO::Tristana::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Caitlyn") == 0)    { SharpAIO::Caitlyn::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Corki") == 0)      { SharpAIO::Corki::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Draven") == 0)     { SharpAIO::Draven::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Ezreal") == 0)     { SharpAIO::Ezreal::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Graves") == 0)     { SharpAIO::Graves::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Jhin") == 0)       { SharpAIO::Jhin::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Irelia") == 0)     { SharpAIO::Irelia::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Jinx") == 0)       { SharpAIO::Jinx::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Kaisa") == 0)      { SharpAIO::Kaisa::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Kalista") == 0)    { SharpAIO::Kalista::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Kindred") == 0)    { SharpAIO::Kindred::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "KogMaw") == 0)     { SharpAIO::KogMaw::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Lucian") == 0)     { SharpAIO::Lucian::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "MissFortune") == 0){ SharpAIO::MissFortune::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Orianna") == 0)    { SharpAIO::Orianna::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Renata") == 0)     { SharpAIO::Renata::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Samira") == 0)     { SharpAIO::Samira::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Senna") == 0)      { SharpAIO::Senna::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Sivir") == 0)      { SharpAIO::Sivir::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Syndra") == 0)     { SharpAIO::Syndra::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Thresh") == 0)     { SharpAIO::Thresh::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "TwistedFate") == 0){ SharpAIO::TwistedFate::OnGameLoad(); }
        // else if (_stricmp(champ.c_str(), "Tristana") == 0)   { SharpAIO::Tristana::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Twitch") == 0)     { SharpAIO::Twitch::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Varus") == 0)      { SharpAIO::Varus::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Vayne") == 0)      { SharpAIO::Vayne::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Viktor") == 0)     { SharpAIO::Viktor::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Xerath") == 0)     { SharpAIO::Xerath::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Yasuo") == 0)      { SharpAIO::Yasuo::OnGameLoad(); }
        else if (_stricmp(champ.c_str(), "Zeri") == 0)       { SharpAIO::Zeri::OnGameLoad(); }
        else {
            return;
        }
    }

    void OnUnload() override {
        SharpAIO::Ashe::OnUnload();
        SharpAIO::Blitzcrank::OnUnload();
        SharpAIO::Brand::OnUnload();
        SharpAIO::Cassiopeia::OnUnload();
        SharpAIO::LeeSin::OnUnload();
        SharpAIO::Yone::OnUnload();
        SharpAIO::Tristana::OnUnload();
        SharpAIO::Caitlyn::OnUnload();
        SharpAIO::Corki::OnUnload();
        SharpAIO::Draven::OnUnload();
        SharpAIO::Ezreal::OnUnload();
        SharpAIO::Graves::OnUnload();
        SharpAIO::Jhin::OnUnload();
        SharpAIO::Irelia::OnUnload();
        SharpAIO::Jinx::OnUnload();
        SharpAIO::Kaisa::OnUnload();
        SharpAIO::Kalista::OnUnload();
        SharpAIO::Kindred::OnUnload();
        SharpAIO::KogMaw::OnUnload();
        SharpAIO::Lucian::OnUnload();
        SharpAIO::MissFortune::OnUnload();
        SharpAIO::Orianna::OnUnload();
        SharpAIO::Renata::OnUnload();
        SharpAIO::Samira::OnUnload();
        SharpAIO::Senna::OnUnload();
        SharpAIO::Sivir::OnUnload();
        SharpAIO::Syndra::OnUnload();
        SharpAIO::Thresh::OnUnload();
        SharpAIO::TwistedFate::OnUnload();
        SharpAIO::Twitch::OnUnload();
        SharpAIO::Varus::OnUnload();
        SharpAIO::Vayne::OnUnload();
        SharpAIO::Viktor::OnUnload();
        SharpAIO::Xerath::OnUnload();
        SharpAIO::Yasuo::OnUnload();
        SharpAIO::Zeri::OnUnload();
    }

private:
    static bool IsSupportedChampionName(const char* championName) {
        return championName && championName[0] &&
            (_stricmp(championName, "Ashe") == 0 ||
             _stricmp(championName, "Blitzcrank") == 0 ||
             _stricmp(championName, "Brand") == 0 ||
             _stricmp(championName, "Cassiopeia") == 0 ||
             _stricmp(championName, "LeeSin") == 0 ||
             _stricmp(championName, "Yone") == 0 ||
             _stricmp(championName, "Tristana") == 0 ||
             _stricmp(championName, "MissFortune") == 0 ||
             _stricmp(championName, "Orianna") == 0 ||
             _stricmp(championName, "Renata") == 0 ||
             _stricmp(championName, "Samira") == 0 ||
             _stricmp(championName, "Senna") == 0 ||
             _stricmp(championName, "Corki") == 0 ||
             _stricmp(championName, "KogMaw") == 0 ||
             _stricmp(championName, "Twitch") == 0 ||
             _stricmp(championName, "Lucian") == 0 ||
             _stricmp(championName, "Jhin") == 0 ||
             _stricmp(championName, "Irelia") == 0 ||
             _stricmp(championName, "Jinx") == 0 ||
             _stricmp(championName, "Sivir") == 0 ||
             _stricmp(championName, "Syndra") == 0 ||
             _stricmp(championName, "Thresh") == 0 ||
             _stricmp(championName, "TwistedFate") == 0 ||
             _stricmp(championName, "Varus") == 0 ||
             _stricmp(championName, "Kalista") == 0 ||
             _stricmp(championName, "Kindred") == 0 ||
             _stricmp(championName, "Draven") == 0 ||
             _stricmp(championName, "Ezreal") == 0 ||
             _stricmp(championName, "Caitlyn") == 0 ||
             _stricmp(championName, "Vayne") == 0 ||
             _stricmp(championName, "Viktor") == 0 ||
             _stricmp(championName, "Graves") == 0 ||
             _stricmp(championName, "Xerath") == 0 ||
             _stricmp(championName, "Yasuo") == 0 ||
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
