#pragma once

#include "ITheme.h"

namespace SDK::UI::IMenu::Skins {

    class TechTheme : public ITheme {
    public:
        std::string Name() const override { return "Tech"; }
    };

}
