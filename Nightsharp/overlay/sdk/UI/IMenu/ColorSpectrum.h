#pragma once

#include "../../../imgui/imgui.h"

namespace SDK::UI::IMenu {

    class ColorBox {
    public:
        enum class EDrawStyle {
            Filled,
            Outline
        };

        enum class Orientation {
            Horizontal,
            Vertical
        };
    };

    class VerticalColorSlider {
    public:
        enum class EDrawStyle {
            Filled,
            Outline
        };
    };

    class VerticalAlphaSlider {
    public:
        enum class EDrawStyle {
            Filled,
            Outline
        };
    };

    class AdobeColors {};
    class Cmyk {};
    class Hsl {};

}
