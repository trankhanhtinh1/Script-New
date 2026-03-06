#pragma once
// ============================================================================
// KeyConvert.h — Convert virtual key codes to human-readable display names
// Ported from EnsoulSharp.SDK/Core/Utils/KeyConvert.cs
// ============================================================================

#include <string>
#include <cstdint>
#include <Windows.h>

namespace SDK {

    // ========================================================================
    // KeyConvert — maps VK_ codes to readable names for menu display
    // ========================================================================
    class KeyConvert {
    public:
        // Convert a virtual key code to a display string
        static std::string KeyToText(uint32_t vKey) {
            // A-Z
            if (vKey >= 0x41 && vKey <= 0x5A) {
                return std::string(1, static_cast<char>(vKey));
            }

            // 0-9
            if (vKey >= 0x30 && vKey <= 0x39) {
                return std::string(1, static_cast<char>(vKey));
            }

            // F1-F12
            if (vKey >= VK_F1 && vKey <= VK_F12) {
                return "F" + std::to_string(vKey - VK_F1 + 1);
            }

            // Numpad 0-9
            if (vKey >= VK_NUMPAD0 && vKey <= VK_NUMPAD9) {
                return "Num" + std::to_string(vKey - VK_NUMPAD0);
            }

            // Special keys
            switch (vKey) {
                case 0:              return "None";
                case VK_LBUTTON:     return "LMB";
                case VK_RBUTTON:     return "RMB";
                case VK_MBUTTON:     return "MMB";
                case VK_XBUTTON1:    return "Mouse4";
                case VK_XBUTTON2:    return "Mouse5";
                case VK_BACK:        return "Backspace";
                case VK_TAB:         return "Tab";
                case VK_RETURN:      return "Enter";
                case VK_SHIFT:       return "Shift";
                case VK_CONTROL:     return "Ctrl";
                case VK_MENU:        return "Alt";
                case VK_PAUSE:       return "Pause";
                case VK_CAPITAL:     return "CapsLock";
                case VK_ESCAPE:      return "Esc";
                case VK_SPACE:       return "Space";
                case VK_PRIOR:       return "PageUp";
                case VK_NEXT:        return "PageDown";
                case VK_END:         return "End";
                case VK_HOME:        return "Home";
                case VK_LEFT:        return "Left";
                case VK_UP:          return "Up";
                case VK_RIGHT:       return "Right";
                case VK_DOWN:        return "Down";
                case VK_SNAPSHOT:    return "PrintScreen";
                case VK_INSERT:      return "Insert";
                case VK_DELETE:      return "Delete";
                case VK_LWIN:        return "LWin";
                case VK_RWIN:        return "RWin";
                case VK_APPS:        return "Apps";
                case VK_SLEEP:       return "Sleep";
                case VK_MULTIPLY:    return "Num*";
                case VK_ADD:         return "Num+";
                case VK_SEPARATOR:   return "NumSep";
                case VK_SUBTRACT:    return "Num-";
                case VK_DECIMAL:     return "Num.";
                case VK_DIVIDE:      return "Num/";
                case VK_NUMLOCK:     return "NumLock";
                case VK_SCROLL:      return "ScrollLock";
                case VK_LSHIFT:      return "LShift";
                case VK_RSHIFT:      return "RShift";
                case VK_LCONTROL:    return "LCtrl";
                case VK_RCONTROL:    return "RCtrl";
                case VK_LMENU:       return "LAlt";
                case VK_RMENU:       return "RAlt";
                case VK_OEM_1:       return ";";
                case VK_OEM_PLUS:    return "=";
                case VK_OEM_COMMA:   return ",";
                case VK_OEM_MINUS:   return "-";
                case VK_OEM_PERIOD:  return ".";
                case VK_OEM_2:       return "/";
                case VK_OEM_3:       return "`";
                case VK_OEM_4:       return "[";
                case VK_OEM_5:       return "\\";
                case VK_OEM_6:       return "]";
                case VK_OEM_7:       return "'";
                default:             return "Key(" + std::to_string(vKey) + ")";
            }
        }

        // Convert a display name back to a VK code (reverse lookup)
        static uint32_t TextToKey(const std::string& text) {
            if (text.empty() || text == "None") return 0;
            if (text.length() == 1) {
                char c = text[0];
                if (c >= 'A' && c <= 'Z') return static_cast<uint32_t>(c);
                if (c >= 'a' && c <= 'z') return static_cast<uint32_t>(c - 32);
                if (c >= '0' && c <= '9') return static_cast<uint32_t>(c);
            }

            // F-keys
            if (text[0] == 'F' && text.length() <= 3) {
                int num = std::atoi(text.c_str() + 1);
                if (num >= 1 && num <= 24) return VK_F1 + num - 1;
            }

            // Common names
            if (text == "LMB")        return VK_LBUTTON;
            if (text == "RMB")        return VK_RBUTTON;
            if (text == "MMB")        return VK_MBUTTON;
            if (text == "Space")      return VK_SPACE;
            if (text == "Tab")        return VK_TAB;
            if (text == "Enter")      return VK_RETURN;
            if (text == "Shift")      return VK_SHIFT;
            if (text == "Ctrl")       return VK_CONTROL;
            if (text == "Alt")        return VK_MENU;
            if (text == "Esc")        return VK_ESCAPE;
            if (text == "CapsLock")   return VK_CAPITAL;
            if (text == "Backspace")  return VK_BACK;
            if (text == "Insert")     return VK_INSERT;
            if (text == "Delete")     return VK_DELETE;
            if (text == "Home")       return VK_HOME;
            if (text == "End")        return VK_END;
            if (text == "PageUp")     return VK_PRIOR;
            if (text == "PageDown")   return VK_NEXT;
            if (text == "Mouse4" || text == "XButton1")   return VK_XBUTTON1;
            if (text == "Mouse5" || text == "XButton2")   return VK_XBUTTON2;

            return 0; // Unknown
        }

        // Check if a key is a modifier
        static bool IsModifier(uint32_t vKey) {
            return vKey == VK_SHIFT || vKey == VK_CONTROL || vKey == VK_MENU
                || vKey == VK_LSHIFT || vKey == VK_RSHIFT
                || vKey == VK_LCONTROL || vKey == VK_RCONTROL
                || vKey == VK_LMENU || vKey == VK_RMENU;
        }

        // Check if a key is a mouse button
        static bool IsMouseButton(uint32_t vKey) {
            return vKey == VK_LBUTTON || vKey == VK_RBUTTON
                || vKey == VK_MBUTTON || vKey == VK_XBUTTON1
                || vKey == VK_XBUTTON2;
        }
    };

} // namespace SDK
