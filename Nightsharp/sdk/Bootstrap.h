#pragma once

#include "Constants.h"
#include "Variables.h"
#include "GameObjects/GameObjects.h"
#include "UI/PermaShow.h"
#include "UI/UI.h"
#include "Utils/Logging.h"
#include "Utils/ResourceLoader.h"
#include "Wrappers/Damages/Damage.h"
#include "Wrappers/Orbwalking/Orbwalker.h"
#include "Wrappers/TargetSelector/TargetSelector.h"

#include <clocale>
#include <locale>
#include <string>
#include <vector>

namespace SDK {

class Bootstrap {
public:
    static bool Initialized() {
        return InitializedFlag();
    }

    static void Init(const std::vector<std::string>& args = {}) {
        (void)args;
        if (InitializedFlag()) {
            return;
        }
        InitializedFlag() = true;

        std::setlocale(LC_ALL, "C");
        std::locale::global(std::locale::classic());

        Utils::Logging::Write()(LogLevel::Info, "[-- SDK Bootstrap Loading --]");

        Utils::ResourceLoader::Initialize();
        Utils::Logging::Write()(LogLevel::Info, "[SDK Bootstrap] Resources Initialized.");

        GameObjects::Initialize();
        Utils::Logging::Write()(LogLevel::Info, "[SDK Bootstrap] GameObjects Initialized.");

        Variables::EnsoulSharpMenu = new Menu("EnsoulSharp", "EnsoulSharp", true);
        Variables::EnsoulSharpMenu->Attach();
        Utils::Logging::Write()(LogLevel::Info, "[SDK Bootstrap] EnsoulSharp Menu Created.");

        Variables::TargetSelector = new ::SDK::TargetSelector(Variables::EnsoulSharpMenu);
        Utils::Logging::Write()(LogLevel::Info, "[SDK Bootstrap] TargetSelector Initialized.");

        Variables::Orbwalker = new ::SDK::Orbwalker(Variables::EnsoulSharpMenu);
        Utils::Logging::Write()(LogLevel::Info, "[SDK Bootstrap] Orbwalker Initialized.");

        InitializeNotifications(Variables::EnsoulSharpMenu);
        Utils::Logging::Write()(LogLevel::Info, "[SDK Bootstrap] Notifications Initialized.");

        UI::PermaShow::Initialize(Variables::EnsoulSharpMenu);
        Utils::Logging::Write()(LogLevel::Info, "[SDK Bootstrap] Permashow Initialized.");

        Damage::Initialize(Variables::GameVersion);
        Utils::Logging::Write()(LogLevel::Info, "[SDK Bootstrap] Damage Library Initialized.");

        Utils::Logging::Write()(LogLevel::Info, "[-- SDK Bootstrap Loading --]");
    }

    static void Shutdown() {
        if (!InitializedFlag()) {
            return;
        }

        delete Variables::Orbwalker;
        Variables::Orbwalker = nullptr;

        delete Variables::TargetSelector;
        Variables::TargetSelector = nullptr;

        if (Variables::EnsoulSharpMenu) {
            MenuManager::Instance().Remove(Variables::EnsoulSharpMenu);
            delete Variables::EnsoulSharpMenu;
            Variables::EnsoulSharpMenu = nullptr;
        }

        ClearNotifications();
        UI::PermaShow::Clear();
        InitializedFlag() = false;
    }

private:
    static bool& InitializedFlag() {
        static bool initialized = false;
        return initialized;
    }

    static void InitializeNotifications(Menu* menu) {
        (void)menu;
    }

    static void ClearNotifications() {}
};

} // namespace SDK
