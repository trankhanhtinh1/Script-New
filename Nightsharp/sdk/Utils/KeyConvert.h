#pragma once

#include <cstdint>
#include <string>

namespace SDK::Core::Utils {

class KeyConvert {
public:
    static std::string KeyToText(std::uint32_t vKey) {
        if (vKey >= 65 && vKey <= 90) {
            return std::string(1, static_cast<char>(vKey));
        }

        if (vKey >= 112 && vKey <= 123) {
            return "F" + std::to_string(vKey - 111);
        }

        switch (vKey) {
        case 9: return "Tab";
        case 16: return "Shift";
        case 17: return "Ctrl";
        case 20: return "CAPS";
        case 27: return "ESC";
        case 32: return "Space";
        case 45: return "Insert";
        default: return std::to_string(vKey);
        }
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using KeyConvert = ::SDK::Core::Utils::KeyConvert;
} // namespace SDK::Utils
