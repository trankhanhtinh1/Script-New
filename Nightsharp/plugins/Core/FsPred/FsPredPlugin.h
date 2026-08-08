#pragma once

#include "../../IPlugin.h"
#include "../../PluginRegistry.h"
#include "../../../core/CoreRuntime.h"
#include "../../../sdk/SDK.h"
#include "../../../sdk/UI/IMenu/Menu.h"
#include "FsPredEngine.h"

#include <string>

namespace Plugins {

class FsPredPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "FsPred"; }
    const char* GetInternalId() const override { return "core.fspred"; }
    const char* GetAuthor() const override { return "FsPred"; }
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

        const bool added = SDK::Prediction::AddPrediction(kImplementationName, &engine_);
        const bool registered = SDK::Prediction::GetPrediction(kImplementationName) == &engine_;
        if (!registered || !SDK::Prediction::SetPrediction(kImplementationName)) {
            if (added) SDK::Prediction::RemovePrediction(kImplementationName);
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
            return;
        }

        const bool ownsActiveProvider =
            SDK::Prediction::GetPrediction(kImplementationName) == &engine_ &&
            SDK::Prediction::CurrentPredictionName() == kImplementationName;
        active_ = false;

        if (ownsActiveProvider) {
            if (previousWasSdk_) {
                SetSdkPredictionLoaded(true);
            } else if (!previousPrediction_.empty() && SDK::Prediction::GetPrediction(previousPrediction_) != nullptr) {
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

    bool LoadSucceeded() const override {
        return active_ &&
               SDK::Prediction::GetPrediction(kImplementationName) == &engine_ &&
               SDK::Prediction::CurrentPredictionName() == kImplementationName;
    }

private:
    static constexpr const char* kImplementationName = "FsPred";
    static constexpr const char* kSdkPredictionName = "SDK Prediction";

    FsPred::FsPredEngine engine_;
    std::string previousPrediction_;
    bool previousWasSdk_ = false;
    bool providerRegistered_ = false;
    bool active_ = false;

    SDK::Menu* menu_ = nullptr;
    SDK::MenuSlider* item_ = nullptr;
    SDK::MenuSlider* extraDelay_ = nullptr;
    SDK::MenuBool* reCheckHitchance_ = nullptr;
    SDK::MenuBool* sdk_ = nullptr;

    FsPred::FsPredConfig BuildConfig() const {
        FsPred::FsPredConfig config;
        config.maxRangePercent = item_ ? item_->Value : 100;
        config.extraDelayMs = extraDelay_ ? extraDelay_->Value : 10;
        config.recheckHitchance = !reCheckHitchance_ || reCheckHitchance_->Value;
        config.useDefaultSdk = sdk_ && sdk_->Value;
        return config;
    }

    void CreateMenu() {
        DestroyMenu();
        menu_ = new SDK::Menu(GetInternalId(), "FsPred", true);

        item_ = menu_->Add(new SDK::MenuSlider("PredMaxRange", "Max Range %", 100, 0, 100));
        extraDelay_ = menu_->Add(new SDK::MenuSlider("ExtraDelayMs", "Extra Delay (ms)", 10, 0, 100));
        reCheckHitchance_ = menu_->Add(new SDK::MenuBool("ReCheckHitchance", "Recheck Hitchance", true));
        sdk_ = menu_->Add(new SDK::MenuBool("Default", "Default Prediction", false));

        menu_->Attach();
    }

    void DestroyMenu() {
        if (menu_) {
            SDK::UI::MenuManager::Instance().Remove(menu_);
            delete menu_;
            menu_ = nullptr;
        }
        item_ = nullptr;
        extraDelay_ = nullptr;
        reCheckHitchance_ = nullptr;
        sdk_ = nullptr;
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

} // namespace Plugins
