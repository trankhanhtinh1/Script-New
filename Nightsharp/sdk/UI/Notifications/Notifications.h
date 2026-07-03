#pragma once

#include "../UI.h"
#include "ANotification.h"
#include "Notification.h"

namespace SDK::UI::Notifications {

    class Notifications {
    public:
        static void Initialize(::SDK::Menu* menu = nullptr) {
            (void)menu;
            ::SDK::Notifications::SetEnabled(true);
        }

        static void Add(const ANotification& notification) {
            ::SDK::Notifications::Add(
                notification.Id,
                notification.Header,
                notification.Body,
                notification.Type,
                notification.Duration);
        }

        static void Add(const char* id,
                        const char* header,
                        NotificationType type = NotificationType::Info,
                        float duration = 3.0f) {
            ::SDK::Notifications::Add(id ? id : "", header ? header : "", type, duration);
        }

        static void Add(const char* id,
                        const char* header,
                        const char* body,
                        NotificationType type = NotificationType::Info,
                        float duration = 3.0f) {
            ::SDK::Notifications::Add(id ? id : "", header ? header : "", body ? body : "", type, duration);
        }

        static void AddCustom(const char* id,
                              const char* header,
                              const char* body,
                              ImU32 color,
                              float duration = 3.0f) {
            ::SDK::MenuUI::Notifications::AddCustom(id ? id : "", header ? header : "", body ? body : "", color, duration);
        }

        static void Remove(const char* id) {
            ::SDK::Notifications::Remove(id ? id : "");
        }

        static void Clear() {
            ::SDK::Notifications::Clear();
        }

        static void SetPosition(float x, float y) {
            ::SDK::Notifications::SetPosition(x, y);
        }

        static void SetMaxVisible(int maxVisible) {
            ::SDK::Notifications::SetMaxVisible(maxVisible);
        }

        static void SetEnabled(bool enabled) {
            ::SDK::Notifications::SetEnabled(enabled);
        }

        static int Count() {
            return ::SDK::Notifications::Count();
        }

        static void Render() {
            ::SDK::Notifications::Render();
        }
    };

}
