#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include <algorithm>
#include <vector>

namespace EzEvade {

class AutoSetPing {
public:
    explicit AutoSetPing(const std::shared_ptr<SDK::MenuUI::Menu>& mainMenu) {
        if (!mainMenu) {
            return;
        }

        auto autoSetPingMenu = mainMenu->AddSubMenu("AutoSetPingMenu", "AutoSetPing");
        autoSetPingMenu->Add<SDK::MenuUI::MenuBool>("AutoSetPingOn", "Auto Set Ping", true);
        autoSetPingMenu->Add<SDK::MenuUI::MenuSlider>("AutoSetPercentile", "Auto Set Percentile", 75, 0, 100);
        autoSetPingMenu->Add<SDK::MenuUI::MenuBool>("AutoSetPingDebug", "Debug Sample", false);

        ObjectCache::Menu.AddMenuToCache(autoSetPingMenu);

        SDK::EventSystem::OnGameUpdate([](float) {
            OnGameUpdate();
        });
    }

private:
    static inline std::vector<float> s_pingSamples = {};
    static inline float s_lastSampleTime = 0.0f;

    static void OnGameUpdate() {
        if (!ObjectCache::Menu.GetBool("AutoSetPingOn", true)) {
            return;
        }

        const float now = SDK::Game::GetTime();
        if (now - s_lastSampleTime < 1.0f) {
            return;
        }
        s_lastSampleTime = now;

        const float ping = SDK::Game::GetPing();
        if (ping <= 0.0f || ping > 1000.0f) {
            return;
        }
        s_pingSamples.push_back(ping);

        if (s_pingSamples.size() < 100) {
            return;
        }

        std::vector<float> sorted = s_pingSamples;
        std::sort(sorted.begin(), sorted.end());

        const int percentile = ObjectCache::Menu.GetSlider("AutoSetPercentile", 75);
        const float ratio = std::clamp((float)percentile / 100.0f, 0.0f, 1.0f);
        const size_t idx = (size_t)std::clamp((int)(ratio * (float)(sorted.size() - 1)), 0, (int)sorted.size() - 1);
        const int extraPing = std::max(0, (int)(sorted[idx] - ping));

        auto* extraPingItem = dynamic_cast<SDK::MenuUI::MenuSlider*>(ObjectCache::Menu.Get("ExtraPingBuffer"));
        if (extraPingItem) {
            extraPingItem->Value = std::clamp(extraPing, extraPingItem->MinValue, extraPingItem->MaxValue);
        }

        s_pingSamples.clear();
    }
};

} // namespace EzEvade
