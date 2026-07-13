#pragma once

#include "../IPlugin.h"
#include "../PluginRegistry.h"
#include "../../Core/CoreRuntime.h"
#include "../../menu/MenuConfig.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"
#include "../../SDK/MenuSDK/Integration/MenuSDKBridge.h"
#include "Core/PredictionEngine.h"

#include <algorithm>
#include <string>

namespace Plugins {

class ZDPredictionPlugin final : public IPlugin {
public:
    ~ZDPredictionPlugin() override {
        DestroyMenuSDKConfig();
        NightSharpMenu::MenuSDKBridge::Instance().UnregisterPlugin(GetInternalId());
    }

    const char* GetName() const override { return "ZDPrediction"; }
    const char* GetInternalId() const override { return "core.zdprediction"; }
    const char* GetAuthor() const override { return "spine8797"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnMenuRegister() override {
        NightSharpMenu::MenuSDKBridge::Instance().RegisterPlugin(
            GetInternalId(),
            GetName(),
            GetAuthor(),
            GetRegistryIndex(),
            "core");
    }

    void OnLoad() override {
        if (active_) return;
        previousPrediction_ = SDK::Prediction::CurrentPredictionName();
        previousWasSdk_ = previousPrediction_.empty() || previousPrediction_ == kSdkPredictionName;
        if (previousPrediction_.empty()) previousPrediction_ = kSdkPredictionName;

        CreateMenu();
        CreateMenuSDK();
        engine_.SetConfig(BuildConfig());
        ZDPrediction::MovementTracker::Initialize();

        const bool added = SDK::Prediction::AddPrediction(kImplementationName, &engine_);
        const bool registered = SDK::Prediction::GetPrediction(kImplementationName) == &engine_;
        if (!registered || !SDK::Prediction::SetPrediction(kImplementationName)) {
            if (added) SDK::Prediction::RemovePrediction(kImplementationName);
            ZDPrediction::MovementTracker::Shutdown();
            DestroyMenuSDKConfig();
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
        DestroyMenuSDKConfig();
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

    NightSharp::Menu::MenuItemHandle sdkPathHistory_;
    NightSharp::Menu::MenuItemHandle sdkVelocityBlend_;
    NightSharp::Menu::MenuItemHandle sdkAcceleration_;
    NightSharp::Menu::MenuItemHandle sdkWallAnalysis_;
    NightSharp::Menu::MenuItemHandle sdkCollision_;
    NightSharp::Menu::MenuItemHandle sdkAoe_;
    NightSharp::Menu::MenuItemHandle sdkReactionTime_;
    NightSharp::Menu::MenuItemHandle sdkHistoryWindow_;
    NightSharp::Menu::MenuItemHandle sdkMaximumPrediction_;
    NightSharp::Menu::MenuItemHandle sdkMaximumSegments_;
    NightSharp::Menu::MenuItemHandle sdkMaximumRange_;
    NightSharp::Menu::MenuItemHandle sdkHighThreshold_;
    NightSharp::Menu::MenuItemHandle sdkVeryHighThreshold_;
    NightSharp::Menu::PermashowEntryHandle sdkStatus_;
    NightSharp::Menu::PermashowEntryHandle sdkProvider_;

    ZDPrediction::PredictionConfig BuildConfig() const {
        ZDPrediction::PredictionConfig config;
        const bool useMenuSdk =
            Config::MenuBackend::mode != Config::MenuBackend::Mode::Legacy;
        const auto boolValue = [](const NightSharp::Menu::MenuItemHandle& item, bool fallback) {
            return item ? item->value : fallback;
        };
        const auto intValue = [](const NightSharp::Menu::MenuItemHandle& item, int fallback) {
            return item ? item->integer : fallback;
        };
        config.usePathHistory = useMenuSdk
            ? boolValue(sdkPathHistory_, true)
            : (!pathHistory_ || pathHistory_->Value);
        config.useVelocityBlend = useMenuSdk
            ? boolValue(sdkVelocityBlend_, true)
            : (!velocityBlend_ || velocityBlend_->Value);
        config.useAcceleration = useMenuSdk
            ? boolValue(sdkAcceleration_, false)
            : (acceleration_ && acceleration_->Value);
        config.useWallAnalysis = useMenuSdk
            ? boolValue(sdkWallAnalysis_, false)
            : (wallAnalysis_ && wallAnalysis_->Value);
        config.useCollision = useMenuSdk
            ? boolValue(sdkCollision_, true)
            : (!collision_ || collision_->Value);
        config.useAoe = useMenuSdk
            ? boolValue(sdkAoe_, true)
            : (!aoe_ || aoe_->Value);
        config.reactionTimeMs = useMenuSdk
            ? intValue(sdkReactionTime_, 280)
            : (reactionTime_ ? reactionTime_->Value : 280);
        config.historyWindowMs = useMenuSdk
            ? intValue(sdkHistoryWindow_, 800)
            : (historyWindow_ ? historyWindow_->Value : 800);
        config.maximumPredictionMs = useMenuSdk
            ? intValue(sdkMaximumPrediction_, 6000)
            : (maximumPrediction_ ? maximumPrediction_->Value : 6000);
        config.maximumPathSegments = useMenuSdk
            ? intValue(sdkMaximumSegments_, 24)
            : (maximumSegments_ ? maximumSegments_->Value : 24);
        config.maximumRangePercent = static_cast<float>(useMenuSdk
            ? intValue(sdkMaximumRange_, 90)
            : (maximumRange_ ? maximumRange_->Value : 90));
        config.highThreshold = static_cast<float>(useMenuSdk
            ? intValue(sdkHighThreshold_, 60)
            : (highThreshold_ ? highThreshold_->Value : 60)) / 100.0f;
        config.veryHighThreshold = static_cast<float>(useMenuSdk
            ? intValue(sdkVeryHighThreshold_, 78)
            : (veryHighThreshold_ ? veryHighThreshold_->Value : 78)) / 100.0f;
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

    void CreateMenuSDK() {
        auto builder = NightSharpMenu::MenuSDKBridge::Instance().RegisterMenu(
            GetInternalId(),
            GetName());
        auto movement = builder.Section("movement", "Movement Model");
        sdkPathHistory_ = movement.Checkbox(
            "pathHistory",
            "Path History",
            !pathHistory_ || pathHistory_->Value);
        sdkVelocityBlend_ = movement.Checkbox(
            "velocityBlend",
            "Velocity Blend",
            !velocityBlend_ || velocityBlend_->Value);
        sdkAcceleration_ = movement.Checkbox(
            "acceleration",
            "Acceleration Solver",
            acceleration_ && acceleration_->Value);
        sdkReactionTime_ = movement.Slider(
            "reactionTime",
            "Enemy Reaction Time",
            reactionTime_ ? reactionTime_->Value : 280,
            0,
            700);
        sdkHistoryWindow_ = movement.Slider(
            "historyWindow",
            "History Window",
            historyWindow_ ? historyWindow_->Value : 800,
            250,
            1200);
        sdkMaximumSegments_ = movement.Slider(
            "maximumSegments",
            "Maximum Path Segments",
            maximumSegments_ ? maximumSegments_->Value : 24,
            2,
            48);

        auto accuracy = builder.Section("accuracy", "Accuracy");
        sdkWallAnalysis_ = accuracy.Checkbox(
            "wallAnalysis",
            "Wall Restriction Analysis",
            wallAnalysis_ && wallAnalysis_->Value);
        sdkCollision_ = accuracy.Checkbox(
            "collision",
            "Collision Validation",
            !collision_ || collision_->Value);
        sdkAoe_ = accuracy.Checkbox(
            "aoe",
            "AoE Optimization",
            !aoe_ || aoe_->Value);
        sdkMaximumPrediction_ = accuracy.Slider(
            "maximumPrediction",
            "Maximum Prediction Time",
            maximumPrediction_ ? maximumPrediction_->Value : 6000,
            500,
            12000);
        sdkMaximumRange_ = accuracy.Slider(
            "maximumRange",
            "Maximum Range Percent",
            maximumRange_ ? maximumRange_->Value : 90,
            50,
            100);
        sdkHighThreshold_ = accuracy.Slider(
            "highThreshold",
            "High Threshold",
            highThreshold_ ? highThreshold_->Value : 60,
            40,
            90);
        sdkVeryHighThreshold_ = accuracy.Slider(
            "veryHighThreshold",
            "Very High Threshold",
            veryHighThreshold_ ? veryHighThreshold_->Value : 78,
            55,
            99);

        auto& permashow = NightSharpMenu::MenuSDKBridge::Instance().Permashow();
        sdkStatus_ = permashow.AddText(
            "core.zdprediction.status",
            "ZDPrediction",
            "[OFF]");
        sdkStatus_->group = "Status";
        sdkStatus_->valueProvider = [this](const NightSharp::Menu::PermashowEntry&) {
            return active_ ? std::string("[ON]") : std::string("[OFF]");
        };
        sdkStatus_->valueColor = ImVec4(0.34f, 0.62f, 0.46f, 1.0f);
        sdkProvider_ = permashow.AddText(
            "core.zdprediction.provider",
            "Prediction",
            kImplementationName);
        sdkProvider_->group = "Runtime";
        sdkProvider_->valueProvider = [](const NightSharp::Menu::PermashowEntry&) {
            return SDK::Prediction::CurrentPredictionName();
        };
        sdkProvider_->valueColor = ImVec4(0.48f, 0.56f, 0.64f, 1.0f);
    }

    void DestroyMenuSDKConfig() {
        auto& bridge = NightSharpMenu::MenuSDKBridge::Instance();
        bridge.Permashow().Remove("core.zdprediction.status");
        bridge.Permashow().Remove("core.zdprediction.provider");
        bridge.UnregisterMenu(GetInternalId());
        sdkPathHistory_.reset();
        sdkVelocityBlend_.reset();
        sdkAcceleration_.reset();
        sdkWallAnalysis_.reset();
        sdkCollision_.reset();
        sdkAoe_.reset();
        sdkReactionTime_.reset();
        sdkHistoryWindow_.reset();
        sdkMaximumPrediction_.reset();
        sdkMaximumSegments_.reset();
        sdkMaximumRange_.reset();
        sdkHighThreshold_.reset();
        sdkVeryHighThreshold_.reset();
        sdkStatus_.reset();
        sdkProvider_.reset();
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
