#pragma once

#include "ITheme.h"

namespace SDK::UI::IMenu::Skins {

    class LightTheme2 : public ITheme {
    public:
        std::string Name() const override { return "Light2"; }
    };

}
