#pragma once

#include "ITheme.h"

namespace SDK::UI::IMenu::Skins {

    class BlueTheme : public ITheme {
    public:
        std::string Name() const override { return "Blue"; }
    };

}
