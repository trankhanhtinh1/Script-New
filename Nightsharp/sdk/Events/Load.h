#pragma once

#include "../../menu/MenuUI.h"

namespace SDK::Events {

    using LoadHandler = void(*)();

    inline MenuUI::FixedList<LoadHandler, 32>& GetLoadHandlers() {
        static MenuUI::FixedList<LoadHandler, 32> s_handlers = {};
        return s_handlers;
    }

    inline MenuUI::FixedList<LoadHandler, 32>& GetInvokedLoadHandlers() {
        static MenuUI::FixedList<LoadHandler, 32> s_handlers = {};
        return s_handlers;
    }

    inline bool AddOnLoad(LoadHandler handler) {
        return handler && GetLoadHandlers().push_back(handler);
    }

    inline bool OnLoad(LoadHandler handler) {
        return AddOnLoad(handler);
    }

    inline void DispatchLoad() {
        for (const auto& handler : GetLoadHandlers()) {
            bool alreadyInvoked = false;
            for (const auto& invoked : GetInvokedLoadHandlers()) {
                if (invoked == handler) {
                    alreadyInvoked = true;
                    break;
                }
            }

            if (handler && !alreadyInvoked) {
                handler();
                GetInvokedLoadHandlers().push_back(handler);
            }
        }
    }

    inline void ResetLoadState() {
        GetInvokedLoadHandlers().clear();
        GetLoadHandlers().clear();
    }

    inline void ResetLoadHandlers() {
        ResetLoadState();
    }

} // namespace SDK::Events
