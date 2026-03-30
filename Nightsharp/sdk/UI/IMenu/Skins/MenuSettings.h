#pragma once

#include "../Customizer/MenuCustomizer.h"

namespace SDK::UI::IMenu::Skins {

    class MenuSettings {
    public:
        static float& FontScale() { return ::SDK::MenuCustomizer::FontScale; }
        static float& MenuAlpha() { return ::SDK::MenuCustomizer::MenuAlpha; }
        static float& ItemSpacing() { return ::SDK::MenuCustomizer::ItemSpacing; }
        static float& Rounding() { return ::SDK::MenuCustomizer::Rounding; }
        static bool& LockPosition() { return ::SDK::MenuCustomizer::LockPosition; }
    };

}
