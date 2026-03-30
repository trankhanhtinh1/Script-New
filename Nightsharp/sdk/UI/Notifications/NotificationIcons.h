#pragma once

#include "../../Enumerations/NotificationIconType.h"
#include "../../../imgui/imgui.h"

namespace SDK::UI::Notifications {

    class NotificationIcons {
    public:
        static const char* GetIcon(NotificationIconType type) {
            switch (type) {
            case NotificationIconType::Error: return "[X]";
            case NotificationIconType::Warning: return "[!]";
            case NotificationIconType::Check: return "[+]";
            case NotificationIconType::Select: return "[>]";
            default: return "[-]";
            }
        }

        static ImU32 GetColor(NotificationIconType type) {
            switch (type) {
            case NotificationIconType::Error: return IM_COL32(220, 50, 50, 255);
            case NotificationIconType::Warning: return IM_COL32(230, 180, 40, 255);
            case NotificationIconType::Check: return IM_COL32(40, 200, 80, 255);
            case NotificationIconType::Select: return IM_COL32(60, 140, 230, 255);
            default: return IM_COL32(200, 200, 200, 255);
            }
        }
    };

}
