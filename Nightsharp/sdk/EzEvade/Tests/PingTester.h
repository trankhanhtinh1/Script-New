#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/Utils/DebugConsole.h"
#include <algorithm>

namespace EzEvade {

class PingTester {
public:
    PingTester() {
        Menu = SDK::MenuUI::Menu::Create("PingTest", "Ping Tester");
        Menu->Add<SDK::MenuUI::MenuBool>("AutoSetPing", "Auto Set Ping", false);
        Menu->Add<SDK::MenuUI::MenuBool>("TestMoveTime", "Test Ping", false);
        Menu->Add<SDK::MenuUI::MenuBool>("SetMaxPing", "Set Max Ping", false);
        Menu->Add<SDK::MenuUI::MenuBool>("SetAvgPing", "Set Avg Ping", false);
        Menu->Add<SDK::MenuUI::MenuBool>("Test20MoveTime", "Test Ping x20", false);
        Menu->Add<SDK::MenuUI::MenuBool>("PrintResults", "Print Results", false);

        SDK::EventSystem::OnGameUpdate([](float) {
            OnGameUpdate();
        });
    }

private:
    static inline std::shared_ptr<SDK::MenuUI::Menu> Menu = nullptr;
    static inline float SumPingTime = 0.0f;
    static inline float AveragePingTime = 0.0f;
    static inline int TestCount = 0;
    static inline float MaxPingTime = 0.0f;
    static inline float LastSampleTime = 0.0f;

    static void OnGameUpdate() {
        if (!Menu) {
            return;
        }

        const float now = SDK::Game::GetTime();
        if (now - LastSampleTime < 0.25f) {
            return;
        }
        LastSampleTime = now;

        if (!(Menu->GetBoolValue("AutoSetPing", false) || Menu->GetBoolValue("TestMoveTime", false)
            || Menu->GetBoolValue("Test20MoveTime", false))) {
            return;
        }

        const float ping = SDK::Game::GetPing();
        if (ping <= 0.0f || ping > 1000.0f) {
            return;
        }

        SumPingTime += ping;
        TestCount += 1;
        AveragePingTime = (TestCount > 0) ? (SumPingTime / (float)TestCount) : ping;
        if (ping > MaxPingTime) {
            MaxPingTime = ping;
        }

        if (Menu->GetBoolValue("PrintResults", false)) {
            if (auto* item = Menu->Get<SDK::MenuUI::MenuBool>("PrintResults")) {
                item->Enabled = false;
            }
            DebugConsole::Log("[PingTester] Avg=%.1f Max=%.1f Samples=%d", AveragePingTime, MaxPingTime, TestCount);
        }

        if (Menu->GetBoolValue("SetAvgPing", false)) {
            if (auto* toggle = Menu->Get<SDK::MenuUI::MenuBool>("SetAvgPing")) {
                toggle->Enabled = false;
            }
            if (auto* item = dynamic_cast<SDK::MenuUI::MenuSlider*>(ObjectCache::Menu.Get("ExtraPingBuffer"))) {
                item->Value = std::clamp((int)AveragePingTime, item->MinValue, item->MaxValue);
            }
        }

        if (Menu->GetBoolValue("SetMaxPing", false)) {
            if (auto* toggle = Menu->Get<SDK::MenuUI::MenuBool>("SetMaxPing")) {
                toggle->Enabled = false;
            }
            if (auto* item = dynamic_cast<SDK::MenuUI::MenuSlider*>(ObjectCache::Menu.Get("ExtraPingBuffer"))) {
                item->Value = std::clamp((int)MaxPingTime, item->MinValue, item->MaxValue);
            }
        }
    }
};

} // namespace EzEvade
