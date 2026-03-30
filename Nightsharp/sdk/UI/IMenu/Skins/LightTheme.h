#pragma once

#include "ITheme.h"

namespace SDK::UI::IMenu::Skins {

    class LightTheme : public ITheme {
    public:
        std::string Name() const override { return "Light"; }
    };

}
