#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ActivatorEngine.h"
#include "../../SDK/UI/IMenu/Menu.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdio>

namespace NightSharp::Companion {

class AwarenessActivatorMenu final {
public:
    using ExportCallback = void (*)(void* context);

    AwarenessActivatorMenu() = default;
    AwarenessActivatorMenu(const AwarenessActivatorMenu&) = delete;
    AwarenessActivatorMenu& operator=(const AwarenessActivatorMenu&) = delete;

    ~AwarenessActivatorMenu() {
        Detach();
    }

    void Attach(ActivatorSettings& settings,
                const char* patchVersion,
                ExportCallback exportCallback = nullptr,
                void* exportContext = nullptr,
                ExportCallback championPresetCallback = nullptr,
                void* championPresetContext = nullptr) {
        Detach();

        settings_ = &settings;
        exportCallback_ = exportCallback;
        exportContext_ = exportContext;
        championPresetCallback_ = championPresetCallback;
        championPresetContext_ = championPresetContext;
        root_ = new SDK::Menu("core.awareness_activator",
                              "Awareness + Activator", true);

        char patchLabel[96] = {};
        std::snprintf(patchLabel, sizeof(patchLabel), "Patch registry: %s",
                      patchVersion && patchVersion[0] ? patchVersion : "embedded defaults");
        root_->Add(new SDK::MenuSeparator("PatchRegistry", patchLabel));

        // Language is intentionally placed before every functional submenu so
        // users can localize the complete tree before configuring anything.
        language_ = root_->Add(new SDK::MenuList(
            "Language", "Language", { "English", "Vietnamese" },
            settings.vietnamese ? 1 : 0));

        SDK::Menu* general = root_->AddSubMenu(
            new SDK::Menu("General", "General and safety"));
        BindBool(general, "Enabled", "Enabled", settings.enabled);
        confirmationKey_ = general->Add(new SDK::MenuKeyBind(
            "ConfirmationKey", "Confirmation key (hold)",
            settings.confirmationVirtualKey, SDK::KeyBindType::Press, false));
        BindBool(general, "DoNotInterruptRecall", "Do not interrupt recall",
                 settings.doNotInterruptRecall);
        BindBool(general, "DoNotUseWhileTyping",
                 "Do not activate while typing",
                 settings.doNotUseWhileTyping);
        BindBool(general, "AllowPracticeAutomation", "Allow automation in Practice only",
                 settings.allowPracticeAutomation);
        general->Add(new SDK::MenuSeparator(
            "LiveSafety", "Live modes remain Suggest or Confirm; Auto is Practice-only"));

        SDK::Menu* awareness = root_->AddSubMenu(
            new SDK::Menu("Awareness", "Awareness overlays"));
        BindBool(awareness, "DrawOverlay", "Draw awareness overlay",
                 settings.drawOverlay);

        SDK::Menu* worldDrawing = awareness->AddSubMenu(
            new SDK::Menu("WorldDrawing", "World drawing"));
        BindBool(worldDrawing, "DrawWorldLayer", "Enable world layer",
                 settings.drawWorldLayer);
        BindBool(worldDrawing, "DrawWorldChampions", "Draw enemy champions",
                 settings.drawWorldChampions);
        BindBool(worldDrawing, "DrawReachableAreas", "Draw enemy reachable areas",
                 settings.drawReachableAreas);
        BindBool(worldDrawing, "DrawThreats", "Draw threat geometry",
                 settings.drawThreats);
        BindBool(worldDrawing, "DrawWards", "Draw wards and vision",
                 settings.drawWards);
        BindBool(worldDrawing, "DrawJungle", "Draw jungle camp timers and confidence",
                 settings.drawJungle);
        BindBool(worldDrawing, "DrawObjectives", "Draw objective state and timers",
                 settings.drawObjectives);
        BindBool(worldDrawing, "DrawCombatState", "Draw combat awareness state",
                 settings.drawCombatState);
        BindBool(worldDrawing, "DrawInsights", "Draw advanced insights",
                 settings.drawInsights);
        BindBool(worldDrawing, "DrawWave", "Draw wave state",
                 settings.drawWave);
        BindBool(worldDrawing, "DrawActivityHeatmap", "Draw replay activity heatmap",
                 settings.drawActivityHeatmap);
        BindBool(worldDrawing, "DrawVisionHeatmap", "Draw vision coverage heatmap",
                 settings.drawVisionHeatmap);
        BindFloat(worldDrawing, "WorldDrawDistance",
                  "World draw distance",
                  settings.worldDrawDistance, 1800.0f, 9000.0f);
        BindFloat(worldDrawing, "ReachableAreaMaxRadius",
                  "Reachable area radius cap",
                  settings.reachableAreaMaxRadius, 800.0f, 6000.0f);

        SDK::Menu* minimapDrawing = awareness->AddSubMenu(
            new SDK::Menu("MinimapDrawing", "Minimap drawing"));
        BindBool(minimapDrawing, "DrawMinimapLayer", "Enable minimap layer",
                 settings.drawMinimapLayer);
        BindBool(minimapDrawing, "DrawMinimapChampions", "Draw enemy champions",
                 settings.drawMinimapChampions);
        BindBool(minimapDrawing, "DrawPathTargets",
                 "Draw observed movement targets",
                 settings.drawPathTargets);
        BindBool(minimapDrawing, "DrawMinimapWards", "Draw wards",
                 settings.drawMinimapWards);
        BindBool(minimapDrawing, "DrawMinimapJungle", "Draw jungle camps",
                 settings.drawMinimapJungle);
        BindBool(minimapDrawing, "DrawMinimapObjectives", "Draw objectives",
                 settings.drawMinimapObjectives);
        BindBool(minimapDrawing, "DrawMinimapLabels", "Draw minimap text labels",
                 settings.drawMinimapLabels);

        SDK::Menu* hudDrawing = awareness->AddSubMenu(
            new SDK::Menu("HudDrawing", "Professional HUD"));
        BindBool(hudDrawing, "DrawIcons", "Draw icons",
                 settings.drawIcons);
        BindBool(hudDrawing, "DrawAlertCenter", "Draw prioritized alert center",
                 settings.drawAlertCenter);
        BindBool(hudDrawing, "DrawEnemyHud",
                 "Draw enemy overhead cooldown HUD",
                 settings.drawEnemyHud);
        hudLayout_ = hudDrawing->Add(new SDK::MenuList(
            "HudLayout", "HUD arrangement",
            { "Vertical", "Horizontal" },
            std::clamp(settings.hudLayoutIndex, 0, 1)));
        BindFloat(hudDrawing, "IconScale", "Icon scale",
                  settings.iconScale, 0.50f, 2.00f);
        BindFloat(hudDrawing, "EnemyHudOffsetX",
                  "Enemy HUD horizontal offset",
                  settings.enemyHudOffsetX, -240.0f, 240.0f);
        BindFloat(hudDrawing, "EnemyHudOffsetY",
                  "Enemy HUD vertical offset",
                  settings.enemyHudOffsetY, -320.0f, 320.0f);
        hudDrawing->Add(new SDK::MenuButton(
            "ResetHudPositions", "Reset smart HUD positions", "Reset",
            &AwarenessActivatorMenu::OnResetHudPositions, this));
        hudDrawing->Add(new SDK::MenuSeparator(
            "HudDragHelp", "Every screen HUD can always be dragged directly"));

        SDK::Menu* advanced = awareness->AddSubMenu(
            new SDK::Menu("Advanced", "Performance, accessibility and diagnostics"));
        BindBool(advanced, "PerformanceMode",
                 "Performance mode (bounded drawing and cached minimap state)",
                 settings.performanceMode);
        BindBool(advanced, "AudioOnly", "Audio-only accessibility mode",
                 settings.audioOnly);
        BindBool(advanced, "StreamerMode", "Privacy-preserving streamer mode",
                 settings.streamerMode);
        BindBool(advanced, "DiagnosticsEnabled",
                 "Profile Awareness FPS by stage",
                 settings.diagnosticsEnabled);
        BindBool(advanced, "DiagnosticsConsoleLog",
                 "Log Awareness FPS to debug console",
                 settings.diagnosticsConsoleLog);
        BindBool(advanced, "DiagnosticsVerbose",
                 "Include object counts and complexity",
                 settings.diagnosticsVerbose);
        BindFloat(advanced, "DiagnosticsReportInterval",
                  "Diagnostics report interval (frames)",
                  settings.diagnosticsReportInterval,
                  15.0f, 600.0f);
        BindFloat(advanced, "DiagnosticsSlowFrameMs",
                  "Log slow render frame threshold (ms)",
                  settings.diagnosticsSlowFrameMs,
                  1.0f, 50.0f);

        SDK::Menu* presets = awareness->AddSubMenu(
            new SDK::Menu("Presets", "Role and champion presets"));
        rolePreset_ = presets->Add(new SDK::MenuList(
            "RolePreset", "Role preset",
            { "Balanced", "Top", "Jungle", "Mid", "Bot", "Support" },
            std::clamp(settings.rolePresetIndex, 0, 5)));
        presets->Add(new SDK::MenuButton(
            "ApplyRolePreset", "Selected role preset", "Apply",
            &AwarenessActivatorMenu::OnApplyRolePreset, this));
        presets->Add(new SDK::MenuButton(
            "ApplyChampionPreset", "Current champion preset", "Apply",
            &AwarenessActivatorMenu::OnApplyChampionPreset, this));

        SDK::Menu* activator = root_->AddSubMenu(
            new SDK::Menu("Activator", "Activator"));
        SDK::Menu* categories = activator->AddSubMenu(
            new SDK::Menu("Categories", "Capability groups"));
        BindBool(categories, "Summoners", "Summoner spells",
                 settings.summonersEnabled);
        BindBool(categories, "DefensiveItems", "Defensive items",
                 settings.defensiveItemsEnabled);
        BindBool(categories, "SupportItems", "Support items",
                 settings.supportItemsEnabled);
        BindBool(categories, "MovementItems", "Movement items",
                 settings.movementItemsEnabled);
        BindBool(categories, "OffensiveItems", "Offensive items",
                 settings.offensiveItemsEnabled);
        BindBool(categories, "VisionItems", "Vision items",
                 settings.visionItemsEnabled);
        BindBool(categories, "Potion", "Potion and consumables",
                 settings.potionEnabled);
        BindBool(categories, "IncludeDamageOverTime", "Include damage over time",
                 settings.includeDamageOverTime);
        BindBool(categories, "ReserveSmiteCharge",
                 "Reserve one Smite charge from Scuttle",
                 settings.reserveSmiteCharge);
        BindBool(categories, "BarrierLethalOnly",
                 "Barrier only for lethal damage",
                 settings.barrierLethalOnly);

        SDK::Menu* thresholds = activator->AddSubMenu(
            new SDK::Menu("Thresholds", "Reaction thresholds"));
        BindFloat(thresholds, "DefensiveHorizon", "Defensive horizon (seconds)",
                  settings.defensiveHorizon, 0.15f, 3.0f);
        BindFloat(thresholds, "ProtectionThreshold", "Protection threshold",
                  settings.protectionThreshold, 0.15f, 1.25f);
        BindFloat(thresholds, "AllySaveThreshold", "Ally save threshold",
                  settings.allySaveThreshold, 0.10f, 0.90f);
        BindFloat(thresholds, "OffensiveSafetyMargin", "Ignite safety margin",
                  settings.offensiveSafetyMargin, 0.0f, 100.0f);
        BindFloat(thresholds, "CleanseReactionDelay", "Cleanse reaction delay (seconds)",
                  settings.cleanseReactionDelay, 0.0f, 0.20f);
        BindFloat(thresholds, "ReactionDebounceSeconds",
                  "Duplicate reaction debounce (seconds)",
                  settings.reactionDebounceSeconds, 0.0f, 1.0f);
        BindFloat(thresholds, "MinimumShieldEfficiency",
                  "Minimum Barrier shield efficiency",
                  settings.minimumShieldEfficiency, 0.10f, 1.0f);
        BindFloat(thresholds, "HealMissingHealthThreshold",
                  "Heal missing-health threshold",
                  settings.healMissingHealthThreshold, 0.05f, 0.90f);
        BindFloat(thresholds, "ExhaustDamageThreshold",
                  "Exhaust incoming-damage threshold",
                  settings.exhaustDamageThreshold, 0.05f, 1.0f);
        BindFloat(thresholds, "GhostMinimumTimeGain",
                  "Minimum Ghost time gain",
                  settings.ghostMinimumTimeGain, 0.10f, 3.0f);

        SDK::Menu* actionModes = root_->AddSubMenu(
            new SDK::Menu("ActionModes", "Per-capability action modes"));
        SDK::Menu* summoners = actionModes->AddSubMenu(
            new SDK::Menu("Summoners", "Summoner spells"));
        BindMode(summoners, Capability::Barrier, "Barrier");
        BindMode(summoners, Capability::Cleanse, "Cleanse");
        BindMode(summoners, Capability::Exhaust, "Exhaust");
        BindMode(summoners, Capability::Flash, "Flash");
        BindMode(summoners, Capability::Ghost, "Ghost");
        BindMode(summoners, Capability::Heal, "Heal");
        BindMode(summoners, Capability::Ignite, "Ignite");
        BindMode(summoners, Capability::Smite, "Smite");
        BindMode(summoners, Capability::Teleport, "Teleport");

        SDK::Menu* protection = actionModes->AddSubMenu(
            new SDK::Menu("Protection", "Cleanse and protection items"));
        BindMode(protection, Capability::Qss, "QSS");
        BindMode(protection, Capability::Mercurial, "Mercurial");
        BindMode(protection, Capability::Mikael, "Mikael");
        BindMode(protection, Capability::Zhonya, "Zhonya");
        BindMode(protection, Capability::Seeker, "Seeker");
        BindMode(protection, Capability::Seraph, "Seraph");
        BindMode(protection, Capability::Locket, "Locket");
        BindMode(protection, Capability::Redemption, "Redemption");

        SDK::Menu* combat = actionModes->AddSubMenu(
            new SDK::Menu("CombatItems", "Combat items"));
        BindMode(combat, Capability::Shurelya, "Shurelya");
        BindMode(combat, Capability::Youmuu, "Youmuu");
        BindMode(combat, Capability::Rocketbelt, "Rocketbelt");
        BindMode(combat, Capability::Stridebreaker, "Stridebreaker");
        BindMode(combat, Capability::Gunblade, "Gunblade");
        BindMode(combat, Capability::Tiamat, "Tiamat");
        BindMode(combat, Capability::RavenousHydra, "Ravenous Hydra");
        BindMode(combat, Capability::TitanicHydra, "Titanic Hydra");
        BindMode(combat, Capability::ProfaneHydra, "Profane Hydra");
        BindMode(combat, Capability::Randuin, "Randuin");
        BindMode(combat, Capability::Actualizer, "Actualizer");

        SDK::Menu* utility = actionModes->AddSubMenu(
            new SDK::Menu("UtilityItems", "Vision and utility items"));
        BindMode(utility, Capability::Oracle, "Oracle Lens");
        BindMode(utility, Capability::Farsight, "Farsight");
        BindMode(utility, Capability::Ward, "Ward");
        BindMode(utility, Capability::Potion, "Potion");
        BindMode(utility, Capability::KnightsVow, "Knight's Vow");

        SDK::Menu* diagnostics = root_->AddSubMenu(
            new SDK::Menu("Diagnostics", "Diagnostics"));
        diagnostics->Add(new SDK::MenuButton(
            "ExportDecisionLog", "Decision log and local telemetry", "Export",
            &AwarenessActivatorMenu::OnExport, this));

        root_->MenuValueChanged = &AwarenessActivatorMenu::OnMenuValueChanged;
        root_->MenuValueChangedUd = this;
        root_->Attach();
        SyncSettingsFromMenu();
    }

    void Detach() noexcept {
        if (root_) {
            root_->MenuValueChanged = nullptr;
            root_->MenuValueChangedUd = nullptr;
            SDK::MenuManager::Instance().Remove(root_);
            delete root_;
        }
        root_ = nullptr;
        settings_ = nullptr;
        language_ = nullptr;
        hudLayout_ = nullptr;
        confirmationKey_ = nullptr;
        boolBindingCount_ = 0;
        floatBindingCount_ = 0;
        modeBindingCount_ = 0;
        rolePreset_ = nullptr;
        exportCallback_ = nullptr;
        exportContext_ = nullptr;
        championPresetCallback_ = nullptr;
        championPresetContext_ = nullptr;
        localeApplied_ = -1;
        syncing_ = false;
    }

    void SyncSettingsFromMenu() noexcept {
        if (!root_ || !settings_ || syncing_) return;
        syncing_ = true;
        for (std::size_t i = 0; i < boolBindingCount_; ++i) {
            *boolBindings_[i].target = boolBindings_[i].item->Value;
        }
        for (std::size_t i = 0; i < floatBindingCount_; ++i) {
            *floatBindings_[i].target = floatBindings_[i].item->Value;
        }
        if (language_) {
            settings_->vietnamese =
                std::clamp(language_->Index, 0, 1) == 1;
        }
        if (hudLayout_) {
            settings_->hudLayoutIndex =
                std::clamp(hudLayout_->Index, 0, 1);
        }
        if (confirmationKey_ && confirmationKey_->Key > 0) {
            settings_->confirmationVirtualKey = confirmationKey_->Key;
        }
        if (rolePreset_) {
            settings_->rolePresetIndex =
                std::clamp(rolePreset_->Index, 0, 5);
        }
        ApplyLocale();
        for (std::size_t i = 0; i < modeBindingCount_; ++i) {
            const int index = std::clamp(modeBindings_[i].item->Index, 0, 3);
            settings_->SetMode(modeBindings_[i].capability,
                               static_cast<ActionMode>(index));
        }
        syncing_ = false;
    }

    void SyncMenuFromSettings() noexcept {
        if (!root_ || !settings_ || syncing_) return;
        syncing_ = true;
        for (std::size_t i = 0; i < boolBindingCount_; ++i) {
            boolBindings_[i].item->Set(*boolBindings_[i].target);
        }
        for (std::size_t i = 0; i < floatBindingCount_; ++i) {
            floatBindings_[i].item->Set(*floatBindings_[i].target);
        }
        if (language_) {
            language_->Set(settings_->vietnamese ? 1 : 0);
        }
        if (hudLayout_) {
            hudLayout_->Set(std::clamp(settings_->hudLayoutIndex, 0, 1));
        }
        if (confirmationKey_ && settings_->confirmationVirtualKey > 0 &&
            confirmationKey_->Key != settings_->confirmationVirtualKey) {
            confirmationKey_->SetKey(settings_->confirmationVirtualKey);
        }
        if (rolePreset_) {
            rolePreset_->Set(std::clamp(settings_->rolePresetIndex, 0, 5));
        }
        for (std::size_t i = 0; i < modeBindingCount_; ++i) {
            modeBindings_[i].item->Set(static_cast<int>(
                settings_->ModeFor(modeBindings_[i].capability)));
        }
        ApplyLocale();
        syncing_ = false;
    }

    bool IsAttached() const noexcept { return root_ != nullptr; }
    SDK::Menu* Root() noexcept { return root_; }
    const SDK::Menu* Root() const noexcept { return root_; }

    SDK::MenuList* ModeItem(Capability capability) noexcept {
        for (std::size_t i = 0; i < modeBindingCount_; ++i) {
            if (modeBindings_[i].capability == capability) {
                return modeBindings_[i].item;
            }
        }
        return nullptr;
    }

private:
    struct BoolBinding {
        bool* target = nullptr;
        SDK::MenuBool* item = nullptr;
    };

    struct FloatBinding {
        float* target = nullptr;
        SDK::MenuSliderF* item = nullptr;
    };

    struct ModeBinding {
        Capability capability = Capability::None;
        SDK::MenuList* item = nullptr;
    };

    SDK::MenuBool* BindBool(SDK::Menu* parent,
                            const char* name,
                            const char* label,
                            bool& target) {
        if (!parent || boolBindingCount_ >= boolBindings_.size()) return nullptr;
        SDK::MenuBool* item = parent->Add(new SDK::MenuBool(name, label, target));
        boolBindings_[boolBindingCount_++] = { &target, item };
        return item;
    }

    SDK::MenuSliderF* BindFloat(SDK::Menu* parent,
                                const char* name,
                                const char* label,
                                float& target,
                                float minimum,
                                float maximum) {
        if (!parent || floatBindingCount_ >= floatBindings_.size()) return nullptr;
        SDK::MenuSliderF* item = parent->Add(
            new SDK::MenuSliderF(name, label, target, minimum, maximum));
        floatBindings_[floatBindingCount_++] = { &target, item };
        return item;
    }

    SDK::MenuList* BindMode(SDK::Menu* parent,
                            Capability capability,
                            const char* label) {
        if (!parent || !settings_ || modeBindingCount_ >= modeBindings_.size()) {
            return nullptr;
        }
        SDK::MenuList* item = parent->Add(new SDK::MenuList(
            CapabilityName(capability), label,
            { "Off", "Suggest", "Confirm", "Auto" },
            static_cast<int>(settings_->ModeFor(capability))));
        modeBindings_[modeBindingCount_++] = { capability, item };
        return item;
    }

    static void SetLabel(SDK::Menu* menu,
                         const char* name,
                         const char* label) noexcept {
        if (!menu) return;
        SDK::UI::MenuItem* item = menu->Item(name);
        if (item) item->DisplayName = label;
    }

    void ApplyLocale() noexcept {
        if (!root_ || !settings_) return;
        const int locale = settings_->vietnamese ? 1 : 0;
        if (localeApplied_ == locale) return;
        localeApplied_ = locale;
        const bool vi = locale != 0;

        root_->DisplayName = vi
            ? "Nhận thức + Kích hoạt" : "Awareness + Activator";
        SetLabel(root_, "Language", vi ? "Ngôn ngữ" : "Language");

        SDK::Menu* general = root_->GetSubMenu("General");
        SDK::Menu* awareness = root_->GetSubMenu("Awareness");
        SDK::Menu* activator = root_->GetSubMenu("Activator");
        SDK::Menu* actionModes = root_->GetSubMenu("ActionModes");
        SDK::Menu* diagnostics = root_->GetSubMenu("Diagnostics");

        if (general) {
            general->DisplayName = vi
                ? "Chung và an toàn" : "General and safety";
            SetLabel(general, "Enabled", vi ? "Bật" : "Enabled");
            SetLabel(general, "ConfirmationKey",
                     vi ? "Phím xác nhận (giữ)"
                        : "Confirmation key (hold)");
            SetLabel(general, "DoNotInterruptRecall",
                     vi ? "Không ngắt Biến Về"
                        : "Do not interrupt recall");
            SetLabel(general, "DoNotUseWhileTyping",
                     vi ? "Không kích hoạt khi đang gõ"
                        : "Do not activate while typing");
            SetLabel(general, "AllowPracticeAutomation",
                     vi ? "Chỉ cho phép tự động trong Phòng Tập"
                        : "Allow automation in Practice only");
        }

        if (awareness) {
            awareness->DisplayName = vi
                ? "Lớp hiển thị nhận thức" : "Awareness overlays";
            SetLabel(awareness, "DrawOverlay",
                     vi ? "Bật toàn bộ lớp hiển thị"
                        : "Draw awareness overlay");

            if (SDK::Menu* world =
                    awareness->GetSubMenu("WorldDrawing")) {
                world->DisplayName = vi
                    ? "Vẽ trong thế giới" : "World drawing";
                SetLabel(world, "DrawWorldLayer",
                         vi ? "Bật lớp vẽ thế giới"
                            : "Enable world layer");
                SetLabel(world, "DrawWorldChampions",
                         vi ? "Hiện tướng địch"
                            : "Draw enemy champions");
                SetLabel(world, "DrawReachableAreas",
                         vi ? "Hiện vùng đối thủ có thể tới"
                            : "Draw enemy reachable areas");
                SetLabel(world, "DrawThreats",
                         vi ? "Hiện vùng nguy hiểm"
                            : "Draw threat geometry");
                SetLabel(world, "DrawWards",
                         vi ? "Hiện mắt và tầm nhìn"
                            : "Draw wards and vision");
                SetLabel(world, "DrawJungle",
                         vi ? "Hiện bộ đếm và độ tin cậy bãi rừng"
                            : "Draw jungle camp timers and confidence");
                SetLabel(world, "DrawObjectives",
                         vi ? "Hiện mục tiêu và bộ đếm"
                            : "Draw objective state and timers");
                SetLabel(world, "DrawCombatState",
                         vi ? "Hiện trạng thái giao tranh"
                            : "Draw combat awareness state");
                SetLabel(world, "DrawInsights",
                         vi ? "Hiện bảng phân tích chiến thuật"
                            : "Draw advanced insights");
                SetLabel(world, "DrawWave",
                         vi ? "Hiện trạng thái đợt lính"
                            : "Draw wave state");
                SetLabel(world, "DrawActivityHeatmap",
                         vi ? "Bản đồ nhiệt hoạt động phát lại"
                            : "Draw replay activity heatmap");
                SetLabel(world, "DrawVisionHeatmap",
                         vi ? "Bản đồ nhiệt tầm nhìn"
                            : "Draw vision coverage heatmap");
                SetLabel(world, "WorldDrawDistance",
                         vi ? "Khoảng cách vẽ trong thế giới"
                            : "World draw distance");
                SetLabel(world, "ReachableAreaMaxRadius",
                         vi ? "Giới hạn bán kính vùng có thể tới"
                            : "Reachable area radius cap");
            }

            if (SDK::Menu* minimap =
                    awareness->GetSubMenu("MinimapDrawing")) {
                minimap->DisplayName = vi
                    ? "Vẽ trên bản đồ nhỏ" : "Minimap drawing";
                SetLabel(minimap, "DrawMinimapLayer",
                         vi ? "Bật lớp bản đồ nhỏ"
                            : "Enable minimap layer");
                SetLabel(minimap, "DrawMinimapChampions",
                         vi ? "Hiện tướng địch"
                            : "Draw enemy champions");
                SetLabel(minimap, "DrawPathTargets",
                         vi ? "Hiện đích di chuyển đã quan sát"
                            : "Draw observed movement targets");
                SetLabel(minimap, "DrawMinimapWards",
                         vi ? "Hiện mắt" : "Draw wards");
                SetLabel(minimap, "DrawMinimapJungle",
                         vi ? "Hiện bãi rừng" : "Draw jungle camps");
                SetLabel(minimap, "DrawMinimapObjectives",
                         vi ? "Hiện mục tiêu lớn" : "Draw objectives");
                SetLabel(minimap, "DrawMinimapLabels",
                         vi ? "Hiện nhãn chữ trên bản đồ nhỏ"
                            : "Draw minimap text labels");
            }

            if (SDK::Menu* hud = awareness->GetSubMenu("HudDrawing")) {
                hud->DisplayName = vi ? "HUD chuyên nghiệp" : "Professional HUD";
                SetLabel(hud, "DrawIcons",
                         vi ? "Hiện biểu tượng" : "Draw icons");
                SetLabel(hud, "DrawAlertCenter",
                         vi ? "Hiện trung tâm cảnh báo ưu tiên"
                            : "Draw prioritized alert center");
                SetLabel(hud, "DrawEnemyHud",
                         vi ? "Hiện HUD hồi chiêu trên đầu đối thủ"
                            : "Draw enemy overhead cooldown HUD");
                SetLabel(hud, "HudLayout",
                         vi ? "Kiểu sắp xếp HUD"
                            : "HUD arrangement");
                SetLabel(hud, "IconScale",
                         vi ? "Tỷ lệ biểu tượng" : "Icon scale");
                SetLabel(hud, "EnemyHudOffsetX",
                         vi ? "Độ lệch HUD đối thủ theo chiều ngang"
                            : "Enemy HUD horizontal offset");
                SetLabel(hud, "EnemyHudOffsetY",
                         vi ? "Độ lệch HUD đối thủ theo chiều dọc"
                            : "Enemy HUD vertical offset");
                SetLabel(hud, "ResetHudPositions",
                         vi ? "Đặt lại vị trí HUD thông minh"
                            : "Reset smart HUD positions");
            }

            if (SDK::Menu* advanced =
                    awareness->GetSubMenu("Advanced")) {
                advanced->DisplayName = vi
                    ? "Hiệu năng, trợ năng và chẩn đoán"
                    : "Performance, accessibility and diagnostics";
                SetLabel(advanced, "PerformanceMode",
                         vi ? "Chế độ hiệu năng"
                            : "Performance mode (bounded drawing and cached minimap state)");
                SetLabel(advanced, "AudioOnly",
                         vi ? "Chế độ trợ năng chỉ âm thanh"
                            : "Audio-only accessibility mode");
                SetLabel(advanced, "StreamerMode",
                         vi ? "Chế độ riêng tư khi phát sóng"
                            : "Privacy-preserving streamer mode");
                SetLabel(advanced, "DiagnosticsEnabled",
                         vi ? "Đo hiệu năng Awareness theo từng giai đoạn"
                            : "Profile Awareness FPS by stage");
                SetLabel(advanced, "DiagnosticsConsoleLog",
                         vi ? "Ghi log hiệu năng ra bảng gỡ lỗi"
                            : "Log Awareness FPS to debug console");
                SetLabel(advanced, "DiagnosticsVerbose",
                         vi ? "Ghi số đối tượng và độ phức tạp"
                            : "Include object counts and complexity");
                SetLabel(advanced, "DiagnosticsReportInterval",
                         vi ? "Chu kỳ báo cáo chẩn đoán (khung hình)"
                            : "Diagnostics report interval (frames)");
                SetLabel(advanced, "DiagnosticsSlowFrameMs",
                         vi ? "Ngưỡng khung hình vẽ chậm (ms)"
                            : "Log slow render frame threshold (ms)");
            }

            if (SDK::Menu* presets = awareness->GetSubMenu("Presets")) {
                presets->DisplayName = vi
                    ? "Cấu hình theo vai trò và tướng"
                    : "Role and champion presets";
                SetLabel(presets, "RolePreset",
                         vi ? "Cấu hình vai trò" : "Role preset");
                SetLabel(presets, "ApplyRolePreset",
                         vi ? "Áp dụng cấu hình vai trò đã chọn"
                            : "Selected role preset");
                SetLabel(presets, "ApplyChampionPreset",
                         vi ? "Áp dụng cấu hình tướng hiện tại"
                            : "Current champion preset");
            }
        }

        if (activator) {
            activator->DisplayName = vi ? "Kích hoạt" : "Activator";
            if (SDK::Menu* categories =
                    activator->GetSubMenu("Categories")) {
                categories->DisplayName =
                    vi ? "Nhóm kích hoạt" : "Capability groups";
                SetLabel(categories, "Summoners",
                         vi ? "Phép bổ trợ D/F" : "Summoner spells");
                SetLabel(categories, "DefensiveItems",
                         vi ? "Trang bị phòng thủ" : "Defensive items");
                SetLabel(categories, "SupportItems",
                         vi ? "Trang bị hỗ trợ" : "Support items");
                SetLabel(categories, "MovementItems",
                         vi ? "Trang bị di chuyển" : "Movement items");
                SetLabel(categories, "OffensiveItems",
                         vi ? "Trang bị tấn công" : "Offensive items");
                SetLabel(categories, "VisionItems",
                         vi ? "Trang bị tầm nhìn" : "Vision items");
                SetLabel(categories, "Potion",
                         vi ? "Bình máu và vật phẩm tiêu hao"
                            : "Potion and consumables");
                SetLabel(categories, "IncludeDamageOverTime",
                         vi ? "Tính sát thương theo thời gian"
                            : "Include damage over time");
                SetLabel(categories, "ReserveSmiteCharge",
                         vi ? "Giữ một lần Trừng Phạt, không dùng cho Cua"
                            : "Reserve one Smite charge from Scuttle");
                SetLabel(categories, "BarrierLethalOnly",
                         vi ? "Chỉ dùng Lá Chắn khi sát thương có thể kết liễu"
                            : "Barrier only for lethal damage");
            }
            if (SDK::Menu* thresholds =
                    activator->GetSubMenu("Thresholds")) {
                thresholds->DisplayName =
                    vi ? "Ngưỡng phản ứng" : "Reaction thresholds";
                SetLabel(thresholds, "DefensiveHorizon",
                         vi ? "Khoảng dự báo phòng thủ (giây)"
                            : "Defensive horizon (seconds)");
                SetLabel(thresholds, "ProtectionThreshold",
                         vi ? "Ngưỡng bảo vệ" : "Protection threshold");
                SetLabel(thresholds, "AllySaveThreshold",
                         vi ? "Ngưỡng cứu đồng minh" : "Ally save threshold");
                SetLabel(thresholds, "OffensiveSafetyMargin",
                         vi ? "Biên an toàn Thiêu Đốt"
                            : "Ignite safety margin");
                SetLabel(thresholds, "CleanseReactionDelay",
                         vi ? "Độ trễ Thanh Tẩy (giây)"
                            : "Cleanse reaction delay (seconds)");
                SetLabel(thresholds, "ReactionDebounceSeconds",
                         vi ? "Chống lặp phản ứng (giây)"
                            : "Duplicate reaction debounce (seconds)");
                SetLabel(thresholds, "MinimumShieldEfficiency",
                         vi ? "Hiệu suất Lá Chắn tối thiểu"
                            : "Minimum Barrier shield efficiency");
                SetLabel(thresholds, "HealMissingHealthThreshold",
                         vi ? "Ngưỡng máu thiếu để Hồi Máu"
                            : "Heal missing-health threshold");
                SetLabel(thresholds, "ExhaustDamageThreshold",
                         vi ? "Ngưỡng sát thương để Kiệt Sức"
                            : "Exhaust incoming-damage threshold");
                SetLabel(thresholds, "GhostMinimumTimeGain",
                         vi ? "Lợi thời gian Tăng Tốc tối thiểu"
                            : "Minimum Ghost time gain");
            }
        }

        if (actionModes) {
            actionModes->DisplayName = vi
                ? "Chế độ theo từng khả năng"
                : "Per-capability action modes";
            if (SDK::Menu* summoners = actionModes->GetSubMenu("Summoners")) {
                summoners->DisplayName = vi
                    ? "Phép bổ trợ" : "Summoner spells";
            }
            if (SDK::Menu* protection = actionModes->GetSubMenu("Protection")) {
                protection->DisplayName = vi
                    ? "Thanh tẩy và bảo vệ"
                    : "Cleanse and protection items";
            }
            if (SDK::Menu* combat = actionModes->GetSubMenu("CombatItems")) {
                combat->DisplayName = vi
                    ? "Trang bị giao tranh" : "Combat items";
            }
            if (SDK::Menu* utility = actionModes->GetSubMenu("UtilityItems")) {
                utility->DisplayName = vi
                    ? "Tầm nhìn và tiện ích"
                    : "Vision and utility items";
            }
        }

        if (diagnostics) {
            diagnostics->DisplayName = vi
                ? "Xuất dữ liệu" : "Diagnostics";
            SetLabel(diagnostics, "ExportDecisionLog",
                     vi ? "Nhật ký quyết định và dữ liệu cục bộ"
                        : "Decision log and local telemetry");
        }
    }

    static void ApplyProfile(ActivatorSettings& settings,
                             const AwarenessPresetProfile& profile) noexcept {
        settings.drawEnemyHud = profile.enemyHud;
        settings.drawCombatState = profile.combat;
        settings.drawWards = profile.wards;
        settings.drawMinimapWards = profile.wards;
        settings.drawJungle = profile.jungle;
        settings.drawMinimapJungle = profile.jungle;
        settings.drawObjectives = profile.objectives;
        settings.drawMinimapObjectives = profile.objectives;
        settings.drawActivityHeatmap = profile.heatmaps;
        settings.defensiveHorizon = profile.defensiveHorizon;
    }

    static void OnMenuValueChanged(SDK::MenuValueChangedEventArgs, void* context) {
        auto* self = static_cast<AwarenessActivatorMenu*>(context);
        if (self) self->SyncSettingsFromMenu();
    }

    static void OnResetHudPositions(SDK::MenuButton*, void* context) {
        auto* self = static_cast<AwarenessActivatorMenu*>(context);
        if (!self || !self->settings_) return;
        self->settings_->alertPanelX = -1.0f;
        self->settings_->alertPanelY = -1.0f;
        self->settings_->objectivePanelX = -1.0f;
        self->settings_->objectivePanelY = -1.0f;
        self->settings_->insightPanelX = -1.0f;
        self->settings_->insightPanelY = -1.0f;
        self->settings_->enemyHudOffsetX = 0.0f;
        self->settings_->enemyHudOffsetY = -62.0f;
        self->SyncMenuFromSettings();
    }

    static void OnApplyRolePreset(SDK::MenuButton*, void* context) {
        auto* self = static_cast<AwarenessActivatorMenu*>(context);
        if (!self || !self->settings_) return;
        self->SyncSettingsFromMenu();
        const int selected = std::clamp(
            self->settings_->rolePresetIndex, 0, 5);
        ApplyProfile(
            *self->settings_,
            AwarenessPresetService::ForRole(
                static_cast<AwarenessPreset>(selected)));
        self->SyncMenuFromSettings();
    }

    static void OnApplyChampionPreset(SDK::MenuButton*, void* context) {
        auto* self = static_cast<AwarenessActivatorMenu*>(context);
        if (!self || !self->settings_) return;
        if (self->championPresetCallback_) {
            self->championPresetCallback_(
                self->championPresetContext_);
        }
        self->SyncMenuFromSettings();
    }

    static void OnExport(SDK::MenuButton*, void* context) {
        auto* self = static_cast<AwarenessActivatorMenu*>(context);
        if (self && self->exportCallback_) {
            self->exportCallback_(self->exportContext_);
        }
    }
    SDK::Menu* root_ = nullptr;
    ActivatorSettings* settings_ = nullptr;
    SDK::MenuList* language_ = nullptr;
    SDK::MenuList* hudLayout_ = nullptr;
    SDK::MenuKeyBind* confirmationKey_ = nullptr;
    SDK::MenuList* rolePreset_ = nullptr;
    std::array<BoolBinding, 64> boolBindings_{};
    std::array<FloatBinding, 20> floatBindings_{};
    std::array<ModeBinding, 40> modeBindings_{};
    std::size_t boolBindingCount_ = 0;
    std::size_t floatBindingCount_ = 0;
    std::size_t modeBindingCount_ = 0;
    ExportCallback exportCallback_ = nullptr;
    void* exportContext_ = nullptr;
    ExportCallback championPresetCallback_ = nullptr;
    void* championPresetContext_ = nullptr;
    bool syncing_ = false;
    int localeApplied_ = -1;
};

} // namespace NightSharp::Companion
