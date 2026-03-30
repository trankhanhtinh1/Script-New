#pragma once

#include "ITheme.h"

namespace SDK::UI::IMenu::Skins {

    class DefaultTheme : public ITheme {
    public:
        std::string Name() const override { return "Default"; }
    };

}
