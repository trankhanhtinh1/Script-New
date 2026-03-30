#pragma once

#include "ITheme.h"

namespace SDK::UI::IMenu::Skins {

    class BlueTheme2 : public ITheme {
    public:
        std::string Name() const override { return "Blue2"; }
    };

}
