#pragma once

#include "../../IPlugin.h"
#include "../../PluginRegistry.h"
#include "../../../core/CoreRuntime.h"
#include "../../../sdk/SDK.h"
#include "../../../sdk/UI/IMenu/Menu.h"
#include "FsPredEngine.h"
#include "FsPredDrawing.h"

#include <array>
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
        FsPred::UnitTracker::Initialize();

        previousPrediction_ = SDK::Prediction::CurrentPredictionName();
        previousWasSdk_ = previousPrediction_.empty() || previousPrediction_ == kSdkPredictionName;
        if (previousPrediction_.empty()) previousPrediction_ = kSdkPredictionName;

        CreateMenu();
        engine_.SetConfig(BuildConfig());

        const bool added = SDK::Prediction::AddPrediction(kImplementationName, &engine_);
        const bool registered = SDK::Prediction::GetPrediction(kImplementationName) == &engine_;
        if (!registered || !SDK::Prediction::SetPrediction(kImplementationName)) {
            if (added) SDK::Prediction::RemovePrediction(kImplementationName);
            drawing_.Shutdown();
            FsPred::UnitTracker::Shutdown();
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
            drawing_.Shutdown();
            FsPred::UnitTracker::Shutdown();
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
        drawing_.Shutdown();
        engine_.ClearDebugPredictions();
        FsPred::UnitTracker::Shutdown();
        previousPrediction_.clear();
        previousWasSdk_ = false;
        DestroyMenu();
    }

    void OnUpdate() override {
        if (!active_) return;
        const FsPred::FsPredConfig config = BuildConfig();
        engine_.SetConfig(config);
        if (SDK::Prediction::GetPrediction(kImplementationName) == &engine_ &&
            SDK::Prediction::CurrentPredictionName() != kImplementationName) {
            SDK::Prediction::SetPrediction(kImplementationName);
        }
        if (drawHitchanceCrests_ && drawHitchanceCrests_->Value) {
            drawing_.Update(
                config.AntiBait,
                config.AntiBait.Enabled);
        }
    }

    void OnRender() override {
        if (!active_) {
            return;
        }
        if (drawHitchanceCrests_ && drawHitchanceCrests_->Value) {
            drawing_.Render();
        }
        if (drawPredictedPositions_ && drawPredictedPositions_->Value) {
            drawing_.RenderPredictedPositions(engine_);
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

    struct SlotMenuItems {
        SDK::MenuSlider* DelayOffset = nullptr;
        SDK::MenuSlider* SpeedScale = nullptr;
        SDK::MenuSlider* SpeedOverride = nullptr;
        SDK::MenuSlider* RadiusOffset = nullptr;
        SDK::MenuSlider* RadiusOverride = nullptr;
        SDK::MenuList* Policy = nullptr;
    };

    FsPred::FsPredEngine engine_;
    FsPred::CrestDrawing drawing_;
    std::string previousPrediction_;
    bool previousWasSdk_ = false;
    bool providerRegistered_ = false;
    bool active_ = false;
    bool applyingPreset_ = false;
    FsPred::FsPredPreset lastNonCustomPreset_ =
        FsPred::FsPredPreset::Balanced;

    SDK::Menu* menu_ = nullptr;
    SDK::Menu* presetMenu_ = nullptr;
    SDK::Menu* antiBaitMenu_ = nullptr;
    SDK::Menu* spellOverridesMenu_ = nullptr;
    SDK::Menu* visualMenu_ = nullptr;
    SDK::Menu* advancedMenu_ = nullptr;
    SDK::MenuList* presetMode_ = nullptr;
    SDK::MenuButton* reapplyPreset_ = nullptr;
    SDK::MenuBool* enableAntiBait_ = nullptr;
    SDK::MenuSlider* maxRangeSafety_ = nullptr;
    SDK::MenuSlider* reactionFloor_ = nullptr;
    SDK::MenuSlider* evasiveTurnAngle_ = nullptr;
    SDK::MenuSlider* longImpactHorizon_ = nullptr;
    SDK::MenuSlider* longProjectileFlight_ = nullptr;
    SDK::MenuSlider* longCastDistance_ = nullptr;
    SDK::MenuBool* terrainCorridorBoost_ = nullptr;
    SDK::MenuList* aoeSecondaryMinimum_ = nullptr;
    SDK::MenuSlider* hardRange_ = nullptr;
    SDK::MenuSlider* globalExtraDelay_ = nullptr;
    SDK::MenuBool* reCheckHitchance_ = nullptr;
    SDK::MenuBool* sdk_ = nullptr;
    SDK::MenuBool* drawHitchanceCrests_ = nullptr;
    SDK::MenuBool* drawPredictedPositions_ = nullptr;
    std::array<SlotMenuItems, 4> slotMenus_{};

    static FsPred::AntiBaitPolicy PolicyFromIndex(int index) {
        switch (index) {
        case 1: return FsPred::AntiBaitPolicy::Full;
        case 2: return FsPred::AntiBaitPolicy::IgnoreMaxRange;
        case 3: return FsPred::AntiBaitPolicy::Disabled;
        case 0:
        default:
            return FsPred::AntiBaitPolicy::Inherit;
        }
    }

    FsPred::FsPredConfig BuildConfig() const {
        FsPred::FsPredConfig config;
        config.Preset = FsPred::PresetFromIndex(
            presetMode_ ? presetMode_->Index : 1);
        config.HardRangePercent = hardRange_ ? hardRange_->Value : 100;
        config.GlobalExtraDelayMs =
            globalExtraDelay_ ? globalExtraDelay_->Value : 10;
        config.RecheckHitchance =
            !reCheckHitchance_ || reCheckHitchance_->Value;
        config.UseDefaultSdk = sdk_ && sdk_->Value;
        config.RecordDebugPredictions =
            drawPredictedPositions_ && drawPredictedPositions_->Value;

        config.AntiBait.Enabled =
            !enableAntiBait_ || enableAntiBait_->Value;
        config.AntiBait.MaxRangeThresholdPercent =
            maxRangeSafety_ ? maxRangeSafety_->Value : 90;
        config.AntiBait.ReactionFloorMs =
            reactionFloor_ ? reactionFloor_->Value : 180;
        config.AntiBait.EvasiveTurnAngleDegrees =
            evasiveTurnAngle_ ? evasiveTurnAngle_->Value : 60;
        config.AntiBait.LongImpactHorizonMs =
            longImpactHorizon_ ? longImpactHorizon_->Value : 750;
        config.AntiBait.LongProjectileFlightMs =
            longProjectileFlight_ ? longProjectileFlight_->Value : 650;
        config.AntiBait.LongCastDistance =
            longCastDistance_ ? longCastDistance_->Value : 900;
        config.AntiBait.TerrainCorridorBoost =
            terrainCorridorBoost_ && terrainCorridorBoost_->Value;
        config.AntiBait.AoeSecondaryMinimum =
            aoeSecondaryMinimum_ && aoeSecondaryMinimum_->Index == 1
            ? SDK::HitChance::Medium
            : SDK::HitChance::High;

        for (std::size_t index = 0; index < slotMenus_.size(); ++index) {
            const SlotMenuItems& items = slotMenus_[index];
            FsPred::SlotCalibration& slot = config.Slots[index];
            slot.DelayOffsetMs =
                items.DelayOffset ? items.DelayOffset->Value : 0;
            slot.SpeedScalePercent =
                items.SpeedScale ? items.SpeedScale->Value : 100;
            slot.SpeedOverride =
                items.SpeedOverride ? items.SpeedOverride->Value : 0;
            slot.RadiusOffset =
                items.RadiusOffset ? items.RadiusOffset->Value : 0;
            slot.RadiusOverride =
                items.RadiusOverride ? items.RadiusOverride->Value : 0;
            slot.Policy = PolicyFromIndex(
                items.Policy ? items.Policy->Index : 0);
        }
        return config;
    }

    void WatchAsCustom(SDK::MenuItem* item) {
        if (!item) {
            return;
        }
        item->ValueChanged = &FsPredPlugin::OnWeightValueChanged;
        item->ValueChangedUd = this;
    }

    static void OnPresetValueChanged(
        SDK::MenuItem*,
        void* userData) {
        auto* self = static_cast<FsPredPlugin*>(userData);
        if (self) {
            self->HandlePresetChanged();
        }
    }

    static void OnWeightValueChanged(
        SDK::MenuItem*,
        void* userData) {
        auto* self = static_cast<FsPredPlugin*>(userData);
        if (self) {
            self->MarkPresetCustom();
        }
    }

    static void OnReapplyPreset(
        SDK::MenuButton*,
        void* userData) {
        auto* self = static_cast<FsPredPlugin*>(userData);
        if (self) {
            self->ReapplySelectedPreset();
        }
    }

    void HandlePresetChanged() {
        if (applyingPreset_ || !presetMode_) {
            return;
        }
        const FsPred::FsPredPreset preset =
            FsPred::PresetFromIndex(presetMode_->Index);
        if (preset != FsPred::FsPredPreset::Custom) {
            ApplyPreset(preset, false);
        }
    }

    void MarkPresetCustom() {
        if (applyingPreset_ || !presetMode_) {
            return;
        }
        const FsPred::FsPredPreset current =
            FsPred::PresetFromIndex(presetMode_->Index);
        if (current == FsPred::FsPredPreset::Custom) {
            return;
        }
        lastNonCustomPreset_ = current;
        applyingPreset_ = true;
        presetMode_->Set(static_cast<int>(FsPred::FsPredPreset::Custom));
        applyingPreset_ = false;
    }

    void ReapplySelectedPreset() {
        if (!presetMode_) {
            return;
        }
        FsPred::FsPredPreset preset =
            FsPred::PresetFromIndex(presetMode_->Index);
        if (preset == FsPred::FsPredPreset::Custom) {
            preset = lastNonCustomPreset_;
        }
        ApplyPreset(preset, true);
    }

    void ApplyPreset(
        FsPred::FsPredPreset preset,
        bool updateSelector) {
        if (preset == FsPred::FsPredPreset::Custom) {
            return;
        }

        const FsPred::AntiBaitWeights weights =
            FsPred::PresetAntiBaitWeights(preset);
        applyingPreset_ = true;
        if (updateSelector && presetMode_) {
            presetMode_->Set(static_cast<int>(preset));
        }
        if (enableAntiBait_) {
            enableAntiBait_->Set(weights.Enabled);
        }
        if (maxRangeSafety_) {
            maxRangeSafety_->Set(weights.MaxRangeThresholdPercent);
        }
        if (reactionFloor_) {
            reactionFloor_->Set(weights.ReactionFloorMs);
        }
        if (evasiveTurnAngle_) {
            evasiveTurnAngle_->Set(weights.EvasiveTurnAngleDegrees);
        }
        if (longImpactHorizon_) {
            longImpactHorizon_->Set(weights.LongImpactHorizonMs);
        }
        if (longProjectileFlight_) {
            longProjectileFlight_->Set(
                weights.LongProjectileFlightMs);
        }
        if (longCastDistance_) {
            longCastDistance_->Set(weights.LongCastDistance);
        }
        if (terrainCorridorBoost_) {
            terrainCorridorBoost_->Set(weights.TerrainCorridorBoost);
        }
        if (aoeSecondaryMinimum_) {
            aoeSecondaryMinimum_->Set(
                weights.AoeSecondaryMinimum == SDK::HitChance::Medium
                ? 1
                : 0);
        }
        applyingPreset_ = false;
        lastNonCustomPreset_ = preset;
    }

    void CreateSlotMenus() {
        static constexpr std::array<const char*, 4> kMenuNames{
            "SlotQ", "SlotW", "SlotE", "SlotR"
        };
        static constexpr std::array<const char*, 4> kDisplayNames{
            "Slot Q Overrides",
            "Slot W Overrides",
            "Slot E Overrides",
            "Slot R Overrides"
        };

        for (std::size_t index = 0; index < slotMenus_.size(); ++index) {
            SDK::Menu* slotMenu = spellOverridesMenu_->AddSubMenu(
                new SDK::Menu(
                    kMenuNames[index],
                    kDisplayNames[index]));
            SlotMenuItems& items = slotMenus_[index];
            items.DelayOffset = slotMenu->Add(new SDK::MenuSlider(
                "DelayOffsetMs",
                "Extra Delay Offset (ms)",
                0,
                -100,
                100));
            items.SpeedScale = slotMenu->Add(new SDK::MenuSlider(
                "SpeedScalePercent",
                "Speed Multiplier (%)",
                100,
                80,
                120));
            items.SpeedOverride = slotMenu->Add(new SDK::MenuSlider(
                "SpeedOverride",
                "Speed Override (0 = inherit)",
                0,
                0,
                5000));
            items.RadiusOffset = slotMenu->Add(new SDK::MenuSlider(
                "RadiusOffset",
                "Radius Offset (units)",
                0,
                -50,
                100));
            items.RadiusOverride = slotMenu->Add(new SDK::MenuSlider(
                "RadiusOverride",
                "Radius Override (0 = inherit)",
                0,
                0,
                500));
            items.Policy = slotMenu->Add(new SDK::MenuList(
                "AntiBaitPolicy",
                "Anti-Bait Policy",
                {
                    "Inherit",
                    "Full",
                    "Ignore Max-Range Penalty",
                    "Disabled"
                },
                0));
        }
    }

    void CreateMenu() {
        DestroyMenu();
        menu_ = new SDK::Menu(
            GetInternalId(),
            "FsPred Core Engine Settings",
            true);

        presetMenu_ = menu_->AddSubMenu(new SDK::Menu(
            "PresetProfile",
            "Preset & Profile Mode"));
        presetMode_ = presetMenu_->Add(new SDK::MenuList(
            "PresetMode",
            "Preset",
            {
                "Strict Anti-Bait",
                "Balanced",
                "Aggressive / Spam",
                "Custom"
            },
            1));
        presetMode_->ValueChanged =
            &FsPredPlugin::OnPresetValueChanged;
        presetMode_->ValueChangedUd = this;
        reapplyPreset_ = presetMenu_->Add(new SDK::MenuButton(
            "ReapplyPresetDefaults",
            "Reapply Selected Preset",
            "Apply",
            &FsPredPlugin::OnReapplyPreset,
            this));

        antiBaitMenu_ = menu_->AddSubMenu(new SDK::Menu(
            "AntiBaitWeights",
            "Global Entropy & Anti-Bait Weights"));
        enableAntiBait_ = antiBaitMenu_->Add(new SDK::MenuBool(
            "EnableAntiBait",
            "Enable Anti-Bait System",
            true));
        globalExtraDelay_ = antiBaitMenu_->Add(new SDK::MenuSlider(
            "ExtraDelayMs",
            "Global Latency Buffer (ms)",
            10,
            0,
            100));
        maxRangeSafety_ = antiBaitMenu_->Add(new SDK::MenuSlider(
            "MaxRangeSafetyPercent",
            "Max-Range Hitchance Threshold (%)",
            90,
            70,
            100));
        reactionFloor_ = antiBaitMenu_->Add(new SDK::MenuSlider(
            "ReactionFloorMs",
            "Human Reaction Floor (ms)",
            180,
            100,
            250));
        evasiveTurnAngle_ = antiBaitMenu_->Add(new SDK::MenuSlider(
            "EvasiveTurnAngle",
            "Evasive Turn Angle (degrees)",
            60,
            45,
            90));
        longImpactHorizon_ = antiBaitMenu_->Add(new SDK::MenuSlider(
            "LongImpactHorizonMs",
            "Long Impact Horizon (ms)",
            750,
            300,
            1000));
        longProjectileFlight_ = antiBaitMenu_->Add(new SDK::MenuSlider(
            "LongProjectileFlightMs",
            "Long Projectile Flight (ms)",
            650,
            300,
            1000));
        longCastDistance_ = antiBaitMenu_->Add(new SDK::MenuSlider(
            "LongCastDistance",
            "Long Cast Distance (units)",
            900,
            400,
            2000));
        terrainCorridorBoost_ = antiBaitMenu_->Add(new SDK::MenuBool(
            "TerrainCorridorBoost",
            "Terrain Corridor Boost",
            false));
        aoeSecondaryMinimum_ = antiBaitMenu_->Add(new SDK::MenuList(
            "AoeSecondaryMinimum",
            "AoE Secondary Target Minimum",
            { "High", "Medium" },
            0));

        WatchAsCustom(enableAntiBait_);
        WatchAsCustom(maxRangeSafety_);
        WatchAsCustom(reactionFloor_);
        WatchAsCustom(evasiveTurnAngle_);
        WatchAsCustom(longImpactHorizon_);
        WatchAsCustom(longProjectileFlight_);
        WatchAsCustom(longCastDistance_);
        WatchAsCustom(terrainCorridorBoost_);
        WatchAsCustom(aoeSecondaryMinimum_);

        spellOverridesMenu_ = menu_->AddSubMenu(new SDK::Menu(
            "SpellSlotOverrides",
            "Spell Slot Overrides & Fixes"));
        CreateSlotMenus();

        visualMenu_ = menu_->AddSubMenu(new SDK::Menu(
            "VisualDebug",
            "Visual & Debug Drawings"));
        drawHitchanceCrests_ = visualMenu_->Add(new SDK::MenuBool(
            "DrawHitchanceCrests",
            "Draw Hitchance Crests",
            true));
        drawPredictedPositions_ = visualMenu_->Add(new SDK::MenuBool(
            "DrawPredictedPositions",
            "Draw Latest Predicted Positions",
            false));

        advancedMenu_ = menu_->AddSubMenu(new SDK::Menu(
            "Advanced",
            "Advanced"));
        hardRange_ = advancedMenu_->Add(new SDK::MenuSlider(
            "PredMaxRange",
            "Hard Cast Range Limit (%)",
            100,
            0,
            100));
        reCheckHitchance_ = advancedMenu_->Add(new SDK::MenuBool(
            "ReCheckHitchance",
            "Recheck Hitchance",
            true));
        sdk_ = advancedMenu_->Add(new SDK::MenuBool(
            "Default",
            "Use Default SDK Prediction",
            false));

        ApplyPreset(FsPred::FsPredPreset::Balanced, false);
        // Attaching restores persisted values and fires their callbacks. Keep
        // those restore notifications from relabeling the saved preset as
        // Custom before the full menu tree has been loaded.
        applyingPreset_ = true;
        menu_->Attach();
        applyingPreset_ = false;
        const FsPred::FsPredPreset restoredPreset =
            FsPred::PresetFromIndex(presetMode_->Index);
        if (restoredPreset != FsPred::FsPredPreset::Custom) {
            lastNonCustomPreset_ = restoredPreset;
        }
    }

    void DestroyMenu() {
        if (menu_) {
            SDK::UI::MenuManager::Instance().Remove(menu_);
            delete menu_;
        }
        menu_ = nullptr;
        presetMenu_ = nullptr;
        antiBaitMenu_ = nullptr;
        spellOverridesMenu_ = nullptr;
        visualMenu_ = nullptr;
        advancedMenu_ = nullptr;
        presetMode_ = nullptr;
        reapplyPreset_ = nullptr;
        enableAntiBait_ = nullptr;
        maxRangeSafety_ = nullptr;
        reactionFloor_ = nullptr;
        evasiveTurnAngle_ = nullptr;
        longImpactHorizon_ = nullptr;
        longProjectileFlight_ = nullptr;
        longCastDistance_ = nullptr;
        terrainCorridorBoost_ = nullptr;
        aoeSecondaryMinimum_ = nullptr;
        hardRange_ = nullptr;
        globalExtraDelay_ = nullptr;
        reCheckHitchance_ = nullptr;
        sdk_ = nullptr;
        drawHitchanceCrests_ = nullptr;
        drawPredictedPositions_ = nullptr;
        slotMenus_ = {};
        applyingPreset_ = false;
        lastNonCustomPreset_ = FsPred::FsPredPreset::Balanced;
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
