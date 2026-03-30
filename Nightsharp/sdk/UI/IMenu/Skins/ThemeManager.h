#pragma once

#include "ITheme.h"

#include <string>

namespace SDK::UI::IMenu::Skins {

    class ThemeManager {
    public:
        static std::string GetCurrentThemeName() {
            return CurrentThemeName();
        }

        static void SetCurrentThemeName(const std::string& name) {
            CurrentThemeName() = name;
        }

    private:
        static std::string& CurrentThemeName() {
            static std::string theme = "NightSharp";
            return theme;
        }
    };

}
