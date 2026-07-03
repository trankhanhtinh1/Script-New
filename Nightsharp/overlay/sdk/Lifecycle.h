#pragma once

#include "../Core/CoreEvents.h"
#include "../Core/CoreRuntime.h"
#include "../Core/Corehook.h"
#include "Core/Game.h"
#include "Data/GameData.h"
#include "Events/Events.h"
#include "GameObjects/GameObjects.h"
#include "Signals/SignalManager.h"
#include "UI/Drawing.h"
#include "UI/Icons.h"
#include "UI/PermaShow.h"
#include "UI/UI.h"

#include <Windows.h>

namespace SDK::Lifecycle {

inline volatile LONG g_shuttingDown = 0;

inline bool IsShuttingDown() {
    return InterlockedCompareExchange(&g_shuttingDown, 0, 0) != 0;
}

inline void ResetShutdownState() {
    InterlockedExchange(&g_shuttingDown, 0);
}

inline void Shutdown() {
    if (InterlockedCompareExchange(&g_shuttingDown, 1, 0) != 0) {
        return;
    }

    // Stop SDK-owned callbacks and discard every pointer that can reference a
    // plugin object before the DLL image is released.
    __try { Signals::SignalManager::Reset(); } __except (1) {}
    __try { Events::Reset(); } __except (1) {}
    __try { Game::Reset(); } __except (1) {}
    __try { Drawing::Reset(); } __except (1) {}
    __try { GameObjects::Shutdown(); } __except (1) {}

    // The core dispatcher can still be called by native detours. Put it into
    // shutdown mode and wait briefly for active dispatches before restoring
    // the original bytes.
    __try { ::Core::Events::Shutdown(); } __except (1) {}
    __try { ::CoreHookTest::UninstallAll(); } __except (1) {}

    __try { Data::GameData::Reset(); } __except (1) {}
    __try { UI::Icons::Reset(); } __except (1) {}
    __try { UI::PermaShow::Clear(); } __except (1) {}
    __try { UI::MenuManager::Instance().Clear(); } __except (1) {}
    __try { CoreRuntime::Shutdown(); } __except (1) {}
}

} // namespace SDK::Lifecycle
