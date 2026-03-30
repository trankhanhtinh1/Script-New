#pragma once

#include "ITheme.h"

namespace SDK::UI::IMenu::Skins {

    class ColoredTheme : public ITheme {
    public:
        std::string Name() const override { return "Colored"; }
    };

}
