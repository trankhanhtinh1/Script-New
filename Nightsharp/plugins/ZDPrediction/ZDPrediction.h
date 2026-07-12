#pragma once

#include "../IPlugin.h"
#include "../PluginRegistry.h"
#include "../../Core/CoreRuntime.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"
#include "Core/PredictionEngine.h"

#include <algorithm>
#include <string>

namespace Plugins {

class ZDPredictionPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "ZDPrediction"; }
    const char* GetInternalId() const override { return "core.zdprediction"; }
    const char* GetAuthor() const override { return "ZD"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        if (active_) return;
        previousPrediction_ = SDK::Prediction::CurrentPredictionName();
        previousWasSdk_ = previousPrediction_.empty() || previousPrediction_ == kSdkPredictionName;
        if (previousPrediction_.empty()) previousPrediction_ = kSdkPredictionName;

        CreateMenu();
        engine_.SetConfig(BuildConfig());
        ZDPrediction::MovementTracker::Initialize();

        const bool added = SDK::Prediction::AddPrediction(kImplementationName, &engine_);
        const bool registered = SDK::Prediction::GetPrediction(kImplementationName) == &engine_;
        if (!registered || !SDK::Prediction::SetPrediction(kImplementationName)) {
            if (added) SDK::Prediction::RemovePrediction(kImplementationName);
            ZDPrediction::MovementTracker::Shutdown();
            DestroyMenu();
            previousPrediction_.clear();
            previousWasSdk_ = false;
            return;
        }

        providerRegistered_ = true;
        active_ = true;
        if (previousWasSdk_) SetSdkPredictionLoaded(false);
    }

    void OnUnload() override {
        if (!active_ && !providerRegistered_) {
            DestroyMenu();
            ZDPrediction::MovementTracker::Shutdown();
            return;
        }

        const bool ownsActiveProvider =
            SDK::Prediction::GetPrediction(kImplementationName) == &engine_ &&
            SDK::Prediction::CurrentPredictionName() == kImplementationName;
        active_ = false;

        if (ownsActiveProvider) {
            if (previousWasSdk_) {
                SetSdkPredictionLoaded(true);
            } else if (!previousPrediction_.empty() &&
                       SDK::Prediction::GetPrediction(previousPrediction_) != nullptr) {
                SDK::Prediction::SetPrediction(previousPrediction_);
            } else {
                SetSdkPredictionLoaded(true);
            }
        }

        if (providerRegistered_) {
            SDK::Prediction::RemovePrediction(kImplementationName);
            providerRegistered_ = false;
        }
        previousPrediction_.clear();
        previousWasSdk_ = false;
        ZDPrediction::MovementTracker::Shutdown();
        DestroyMenu();
    }

    void OnUpdate() override {
        if (!active_) return;
        engine_.SetConfig(BuildConfig());
        if (SDK::Prediction::GetPrediction(kImplementationName) == &engine_ &&
            SDK::Prediction::CurrentPredictionName() != kImplementationName) {
            SDK::Prediction::SetPrediction(kImplementationName);
        }
    }

    void OnMenu() override {
        if (!menu_) return;
        menu_->DrawImGui();
        const ZDPrediction::PredictionStatistics stats = engine_.Statistics();
        ImGui::Separator();
        ImGui::Text("Provider: %s", SDK::Prediction::CurrentPredictionName().c_str());
        ImGui::Text("Predictions: %llu", static_cast<unsigned long long>(stats.total));
        ImGui::Text("Dash: %llu  Immobile: %llu", static_cast<unsigned long long>(stats.dash),
                    static_cast<unsigned long long>(stats.immobile));
        ImGui::Text("AoE: %llu  Collision: %llu", static_cast<unsigned long long>(stats.aoe),
                    static_cast<unsigned long long>(stats.collision));
        ImGui::Text("Rejected: %llu  No solution: %llu",
                    static_cast<unsigned long long>(stats.rejected),
                    static_cast<unsigned long long>(stats.noSolution));
    }

private:
    static constexpr const char* kImplementationName = "ZD Prediction";
    static constexpr const char* kSdkPredictionName = "SDK Prediction";

    ZDPrediction::PredictionEngine engine_;
    std::string previousPrediction_;
    bool previousWasSdk_ = false;
    bool providerRegistered_ = false;
    bool active_ = false;

    Menu* menu_ = nullptr;
    MenuBool* pathHistory_ = nullptr;
    MenuBool* velocityBlend_ = nullptr;
    MenuBool* acceleration_ = nullptr;
    MenuBool* wallAnalysis_ = nullptr;
    MenuBool* collision_ = nullptr;
    MenuBool* aoe_ = nullptr;
    MenuSlider* reactionTime_ = nullptr;
    MenuSlider* historyWindow_ = nullptr;
    MenuSlider* maximumPrediction_ = nullptr;
    MenuSlider* maximumSegments_ = nullptr;
    MenuSlider* maximumRange_ = nullptr;
    MenuSlider* highThreshold_ = nullptr;
    MenuSlider* veryHighThreshold_ = nullptr;

    ZDPrediction::PredictionConfig BuildConfig() const {
        ZDPrediction::PredictionConfig config;
        config.usePathHistory = !pathHistory_ || pathHistory_->Value;
        config.useVelocityBlend = !velocityBlend_ || velocityBlend_->Value;
        config.useAcceleration = acceleration_ && acceleration_->Value;
        config.useWallAnalysis = wallAnalysis_ && wallAnalysis_->Value;
        config.useCollision = !collision_ || collision_->Value;
        config.useAoe = !aoe_ || aoe_->Value;
        config.reactionTimeMs = reactionTime_ ? reactionTime_->Value : 280;
        config.historyWindowMs = historyWindow_ ? historyWindow_->Value : 800;
        config.maximumPredictionMs = maximumPrediction_ ? maximumPrediction_->Value : 6000;
        config.maximumPathSegments = maximumSegments_ ? maximumSegments_->Value : 24;
        config.maximumRangePercent = static_cast<float>(maximumRange_ ? maximumRange_->Value : 90);
        config.highThreshold = static_cast<float>(highThreshold_ ? highThreshold_->Value : 60) / 100.0f;
        config.veryHighThreshold = static_cast<float>(veryHighThreshold_ ? veryHighThreshold_->Value : 78) / 100.0f;
        if (config.veryHighThreshold <= config.highThreshold) {
            config.veryHighThreshold = std::min(0.99f, config.highThreshold + 0.05f);
        }
        return config;
    }

    void CreateMenu() {
        DestroyMenu();
        menu_ = new Menu(GetInternalId(), GetName(), true);

        auto* movement = menu_->AddSubMenu(new Menu("movement", "Movement Model"));
        pathHistory_ = movement->Add(new MenuBool("pathHistory", "Path History", true));
        velocityBlend_ = movement->Add(new MenuBool("velocityBlend", "Velocity Blend", true));
        acceleration_ = movement->Add(new MenuBool("acceleration", "Acceleration Solver", false));
        reactionTime_ = movement->Add(new MenuSlider("reactionTime", "Enemy Reaction Time", 280, 0, 700));
        historyWindow_ = movement->Add(new MenuSlider("historyWindow", "History Window", 800, 250, 1200));
        maximumSegments_ = movement->Add(new MenuSlider("maximumSegments", "Maximum Path Segments", 24, 2, 48));

        auto* accuracy = menu_->AddSubMenu(new Menu("accuracy", "Accuracy"));
        wallAnalysis_ = accuracy->Add(new MenuBool("wallAnalysis", "Wall Restriction Analysis", false));
        collision_ = accuracy->Add(new MenuBool("collision", "Collision Validation", true));
        aoe_ = accuracy->Add(new MenuBool("aoe", "AoE Optimization", true));
        maximumPrediction_ = accuracy->Add(new MenuSlider("maximumPrediction", "Maximum Prediction Time", 6000, 500, 12000));
        maximumRange_ = accuracy->Add(new MenuSlider("maximumRange", "Maximum Range Percent", 90, 50, 100));
        highThreshold_ = accuracy->Add(new MenuSlider("highThreshold", "High Threshold", 60, 40, 90));
        veryHighThreshold_ = accuracy->Add(new MenuSlider("veryHighThreshold", "Very High Threshold", 78, 55, 99));

        menu_->Attach();
    }

    void DestroyMenu() {
        if (menu_) {
            MenuManager::Instance().Remove(menu_);
            delete menu_;
            menu_ = nullptr;
        }
        pathHistory_ = nullptr;
        velocityBlend_ = nullptr;
        acceleration_ = nullptr;
        wallAnalysis_ = nullptr;
        collision_ = nullptr;
        aoe_ = nullptr;
        reactionTime_ = nullptr;
        historyWindow_ = nullptr;
        maximumPrediction_ = nullptr;
        maximumSegments_ = nullptr;
        maximumRange_ = nullptr;
        highThreshold_ = nullptr;
        veryHighThreshold_ = nullptr;
    }

    static void SetSdkPredictionLoaded(bool loaded) {
        const int index = PluginRegistry::FindByInternalId("prediction");
        if (index >= 0 && PluginRegistry::HasRuntime(index)) {
            if (loaded) PluginRegistry::LoadPlugin(index);
            else PluginRegistry::UnloadPlugin(index);
            return;
        }
        if (loaded) SDK::Prediction::ResumeSdkPredictionRuntime(nullptr);
        else SDK::Prediction::SuspendSdkPredictionRuntime(nullptr);
        if (index >= 0) PluginRegistry::Plugins[index].Loaded = loaded;
    }
};

}
