#pragma once

// ============================================================================
// 7UPAIO — Port từ 7UPAIO/Program.cs (C# EnsoulSharp) sang NightSharp C++
//
// Một plugin duy nhất "7UPAIO" dispatch theo champion name giống Program.cs.
// Mỗi champion viết trong file .h riêng, dùng Facade.h SDK.
// Comment = chưa port. Uncomment khi port xong champion đó.
// ============================================================================

#include "../../IPlugin.h"
#include "../../../SDK/SDK.h"

// === Champion includes (uncomment khi port xong) ===
// #include "Ezreal.h"
// #include "Vayne.h"
// #include "Caitlyn.h"
// #include "Cassiopeia.h"
// #include "Darius.h"
// #include "Ekko.h"
// #include "Hecarim.h"
// #include "Jax.h"
// #include "Jayce.h"
// #include "Jinx.h"
// #include "KSante.h"
// #include "Kalista.h"
// #include "Fizz.h"
// #include "Karthus.h"
// #include "KogMaw.h"
// #include "Leblanc.h"
// #include "Nami.h"
// #include "Nautilus.h"
// #include "Olaf.h"
// #include "Orianna.h"
// #include "Rumble.h"
// #include "Ryze.h"
// #include "Talon.h"
// #include "Taliyah.h"
// #include "Taric.h"
// #include "Thresh.h"
// #include "Sejuani.h"
// #include "Sett.h"
// #include "Shyvana.h"
// #include "Sylas.h"
// #include "Viktor.h"
// #include "Xerath.h"
// #include "Zed.h"

namespace Plugins {

class AIO7UPPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "7UPAIO"; }
    const char* GetInternalId() const override { return "champion.7upaio"; }
    const char* GetAuthor() const override { return "7UP"; }
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    bool AutoLoadByDefault() const override { return true; }
    bool CanLoad() const override { return true; }

    void OnLoad() override {
        const std::string champ = ObjectManager::Player().CharacterName();

        // === Champion dispatch (port từ Program.cs switch) ===
        // Comment = chưa port. Uncomment khi port xong champion đó.
        if (false) {
        }
        // else if (champ == "Vayne")      { Vayne::OnGameLoad(); }
        // else if (champ == "Caitlyn")    { Caitlyn::OnGameLoad(); }
        // else if (champ == "Cassiopeia") { Cassiopeia::OnGameLoad(); }
        // else if (champ == "Darius")     { Darius::OnGameLoad(); }
        // else if (champ == "Ekko")       { Ekko::OnGameLoad(); }
        // else if (champ == "Ezreal")     { Ezreal::OnGameLoad(); }
        // else if (champ == "Hecarim")    { Hecarim::OnGameLoad(); }
        // else if (champ == "Jax")        { Jax::OnGameLoad(); }
        // else if (champ == "Jayce")      { Jayce::OnGameLoad(); }
        // else if (champ == "Jinx")       { Jinx::OnGameLoad(); }
        // else if (champ == "KSante")     { KSante::OnGameLoad(); }
        // else if (champ == "Kalista")    { Kalista::OnGameLoad(); }
        // else if (champ == "Fizz")       { Fizz::OnGameLoad(); }
        // else if (champ == "Karthus")    { Karthus::OnGameLoad(); }
        // else if (champ == "KogMaw")     { KogMaw::OnGameLoad(); }
        // else if (champ == "Leblanc")    { Leblanc::OnGameLoad(); }
        // else if (champ == "Nami")       { Nami::OnGameLoad(); }
        // else if (champ == "Nautilus")   { Nautilus::OnGameLoad(); }
        // else if (champ == "Olaf")       { Olaf::OnGameLoad(); }
        // else if (champ == "Orianna")    { Orianna::OnGameLoad(); }
        // else if (champ == "Rumble")     { Rumble::OnGameLoad(); }
        // else if (champ == "Ryze")       { Ryze::OnGameLoad(); }
        // else if (champ == "Talon")      { Talon::OnGameLoad(); }
        // else if (champ == "Taliyah")    { Taliyah::OnGameLoad(); }
        // else if (champ == "Taric")      { Taric::OnGameLoad(); }
        // else if (champ == "Thresh")     { Thresh::OnGameLoad(); }
        // else if (champ == "Sejuani")    { Sejuani::OnGameLoad(); }
        // else if (champ == "Sett")       { Sett::OnGameLoad(); }
        // else if (champ == "Shyvana")    { Shyvana::OnGameLoad(); }
        // else if (champ == "Sylas")      { Sylas::OnGameLoad(); }
        // else if (champ == "Viktor")     { Viktor::OnGameLoad(); }
        // else if (champ == "Xerath")     { Xerath::OnGameLoad(); }
        // else if (champ == "Zed")        { Zed::OnGameLoad(); }
        else {
            // Program.cs default: "7UP AIO Does Not Support: [champion]"
            const std::string msg =
                "<font color='#b756c5' size='25'>7UP AIO Does Not Support: " +
                champ + "</font>";
            Game::Print(msg.c_str());
        }

        // Program.cs line 129: TrollChat.OnGameLoad();
        // TODO: port TrollChat khi cần
    }
};

} // namespace Plugins
