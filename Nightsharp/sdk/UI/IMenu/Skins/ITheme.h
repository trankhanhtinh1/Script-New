#pragma once

#include <string>

namespace SDK::UI::IMenu::Skins {

    class ITheme {
    public:
        virtual ~ITheme() = default;
        virtual std::string Name() const = 0;
    };

}
