#pragma once

#include "../IPlugin.h"
#include "../../SDK/SDK.h"

namespace SDK::GameObjects::detail {
    inline std::vector<AITurretClient> TurretsList;
    inline std::vector<AITurretClient> AllyTurretsList;
    inline std::vector<AITurretClient> EnemyTurretsList;
    inline std::vector<BarracksDampenerClient> InhibitorsList;
    inline std::vector<BarracksDampenerClient> AllyInhibitorsList;
    inline std::vector<BarracksDampenerClient> EnemyInhibitorsList;
    inline std::vector<HQClient> NexusList;
    inline HQClient AllyNexusObject;
    inline HQClient EnemyNexusObject;
}

#include "../../DebugLog.h"
#include "../../imgui/imgui.h"

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

namespace Plugins {

enum class GameObjectListType : int {
    UseCategoryFilters = 0,
    AllGameObjects,
    AttackableUnits,
    Ally,
    Enemy,
    Heroes,
    AllyHeroes,
    EnemyHeroes,
    Minions,
    AllyMinions,
    EnemyMinions,
    AllyLaneMinions,
    EnemyLaneMinions,
    AllySpecialMinions,
    EnemySpecialMinions,
    AllyIgnoredMinions,
    EnemyIgnoredMinions,
    Wards,
    AllyWards,
    EnemyWards,
    Jungle,
    JungleSmall,
    JungleLarge,
    JungleLegendary,
    Plants,
    Clones,
    AllyClones,
    EnemyClones,
    Pets,
    AllyPets,
    EnemyPets,
    Turrets,
    AllyTurrets,
    EnemyTurrets,
    Inhibitors,
    AllyInhibitors,
    EnemyInhibitors,
    Nexuses,
    AllyNexus,
    EnemyNexus,
    Shops,
    AllyShops,
    EnemyShops,
    SpawnPoints,
    AllySpawnPoints,
    EnemySpawnPoints,
    ParticleEmitters,
    Missiles,
    Player
};

class DeveloperToolsPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Developer Tools"; }
    const char* GetInternalId() const override { return "utility.developer_tools"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        s_instance = this;
        enabled_ = true;
        maxRange_ = 400;
        trackedObjectTicks_.clear();
        scanCache_.reserve(256);
        activeObjectsCache_.reserve(128);
        SDK::Events::AddOnProcessSpell(&DeveloperToolsPlugin::OnProcessSpellCast);
        SDK::Events::AddOnDeleteObject(&DeveloperToolsPlugin::OnObjectDelete);

        DestroyNativeMenu();
        menu_ = new SDK::UI::Menu(GetInternalId(), GetName(), true);
        menuEnabled_ = menu_->Add(new SDK::UI::MenuBool("Enabled", "Enable Developer Tools", enabled_));
        menuMaxRange_ = menu_->Add(new SDK::UI::MenuSlider("MaxRange", "Max Scan Range", maxRange_, 100, 1500));
        menuProvider_ = menu_->Add(new SDK::UI::MenuList("Provider", "Scan Provider", { "SDK::ObjectManager (Raw RAM)", "SDK::GameObjects Facade" }, scanProviderIndex_));

        auto* specificSub = menu_->AddSubMenu(new SDK::UI::Menu("SpecificLists", "GameObjects Specific Lists"));
        for (auto& opt : listOptions_) {
            opt.MenuControl = specificSub->Add(new SDK::UI::MenuBool(opt.Name, opt.DisplayName, opt.Enabled));
        }

        auto* filters = menu_->AddSubMenu(new SDK::UI::Menu("Filters", "Category Filters"));
        menuScanAll_ = filters->Add(new SDK::UI::MenuBool("ScanAll", "Scan All GameObjects", scanRawGameObjects_));
        menuScanHeroes_ = filters->Add(new SDK::UI::MenuBool("ScanHeroes", "Heroes (AIHeroClient)", scanHeroes_));
        menuScanMinions_ = filters->Add(new SDK::UI::MenuBool("ScanMinions", "Minions & Pets", scanMinions_));
        menuScanTurrets_ = filters->Add(new SDK::UI::MenuBool("ScanTurrets", "Turrets", scanTurrets_));
        menuScanMissiles_ = filters->Add(new SDK::UI::MenuBool("ScanMissiles", "Missiles", scanMissiles_));
        menuFilterClutter_ = filters->Add(new SDK::UI::MenuBool("FilterClutter", "Filter Clutter (FX, MoveTo)", filterClutter_));

        menuInspector_ = menu_->Add(new SDK::UI::MenuRuntime("LiveInspector", "Open Live Object Inspector", &OnMenuBridge, this, 620.0f));

        menu_->Attach();
    }

    void OnUnload() override {
        SDK::Events::RemoveOnProcessSpell(&DeveloperToolsPlugin::OnProcessSpellCast);
        SDK::Events::RemoveOnDeleteObject(&DeveloperToolsPlugin::OnObjectDelete);
        DestroyNativeMenu();
        s_instance = nullptr;
    }

    void SyncMenuSettings() {
        if (menuEnabled_) enabled_ = menuEnabled_->Value;
        if (menuMaxRange_) maxRange_ = menuMaxRange_->Value;
        if (menuProvider_) scanProviderIndex_ = menuProvider_->Index;
        for (auto& opt : listOptions_) {
            if (opt.MenuControl) opt.Enabled = opt.MenuControl->Value;
        }
        if (menuScanAll_) scanRawGameObjects_ = menuScanAll_->Value;
        if (menuScanHeroes_) scanHeroes_ = menuScanHeroes_->Value;
        if (menuScanMinions_) scanMinions_ = menuScanMinions_->Value;
        if (menuScanTurrets_) scanTurrets_ = menuScanTurrets_->Value;
        if (menuScanMissiles_) scanMissiles_ = menuScanMissiles_->Value;
        if (menuFilterClutter_) filterClutter_ = menuFilterClutter_->Value;
    }

    void OnUpdate() override {
        SyncMenuSettings();
        if (!enabled_) {
            pKeyPressedLast_ = false;
            return;
        }

        if (trackedObjectTicks_.size() > 128) {
            const int now = SDK::Variables::TickCount();
            for (auto it = trackedObjectTicks_.begin(); it != trackedObjectTicks_.end(); ) {
                if (now - it->second > 15000) {
                    it = trackedObjectTicks_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        bool isDown = (GetAsyncKeyState('P') & 0x8000) != 0;
        if (isDown && !pKeyPressedLast_) {
            const Vec3 cursorPos = SDK::Game::CursorPos();
            const float rangeSqr = static_cast<float>(maxRange_ * maxRange_);
            const int now = SDK::Variables::TickCount();

            std::string copyText = "=== DEVELOPER TOOLS OBJECT TABLE ===\n";
            int count = 0;

            PopulateScanCache();

            for (const auto& obj : scanCache_) {
                if (!obj.IsValid()) continue;

                const Vec3 pos = obj.Position();
                if (pos.DistanceSqr(cursorPos) >= rangeSqr) continue;

                const std::string& name = GetObjectName(obj);
                const std::string& charName = GetObjectCharacterName(obj);
                if (IsClutter(obj, name, charName)) continue;

                const char* typeStr = ObjectTypeToString(obj.Type());
                std::uint32_t netId = static_cast<std::uint32_t>(obj.NetworkId());
                const char* teamStr = TeamToString(obj);
                char statusBuf[256];
                GetStatusString(obj, statusBuf, sizeof(statusBuf));

                float age = 0.0f;
                auto it = trackedObjectTicks_.find(netId);
                if (it != trackedObjectTicks_.end()) {
                    age = static_cast<float>(now - it->second) / 1000.0f;
                }

                char lineBuf[512];
                std::snprintf(lineBuf, sizeof(lineBuf),
                              "[%d] Name: %s | CharName: %s | NetId: %u | Addr: 0x%llX | Type: %s | Team: %s | Status: %s | Age: %.1fs | Pos: (%.1f, %.1f, %.1f)\n",
                              ++count, name.c_str(), charName.c_str(), netId,
                              static_cast<unsigned long long>(obj.Address()),
                              typeStr, teamStr, statusBuf, age,
                              pos.x, pos.y, pos.z);
                copyText += lineBuf;
            }

            if (count > 0) {
                ImGui::SetClipboardText(copyText.c_str());
                NightSharpDebug::Logf("[Dev] Copied ALL %d objects in table to Clipboard!", count);
            } else {
                NightSharpDebug::Logf("[Dev] No objects in range to copy!");
            }
        }
        pKeyPressedLast_ = isDown;
    }

    void OnRender() override {
        SyncMenuSettings();
        if (!enabled_ || !SDK::Drawing::IsEnabled()) {
            return;
        }

        const Vec3 cursorPos = SDK::Game::CursorPos();
        const float rangeSqr = static_cast<float>(maxRange_ * maxRange_);
        const int now = SDK::Variables::TickCount();

        PopulateScanCache();

        for (const auto& obj : scanCache_) {
            if (!obj.IsValid()) {
                continue;
            }

            const Vec3 pos = obj.Position();
            if (pos.DistanceSqr(cursorPos) >= rangeSqr) {
                continue;
            }

            const std::string& name = GetObjectName(obj);
            const std::string& charName = GetObjectCharacterName(obj);

            if (IsClutter(obj, name, charName)) {
                continue;
            }

            // Track age
            const std::uint32_t netId = static_cast<std::uint32_t>(obj.NetworkId());
            float age = 0.0f;
            auto it = trackedObjectTicks_.find(netId);
            if (it == trackedObjectTicks_.end()) {
                trackedObjectTicks_[netId] = now;
            } else {
                age = static_cast<float>(now - it->second) / 1000.0f;
            }

            Vec2 screen = {};
            if (!SDK::Drawing::WorldToScreen(pos, screen) || !screen.IsValid()) {
                continue;
            }

            // Draw text directly on screen
            const std::string& displayName = (obj.IsHero() || obj.IsMinion() || obj.IsTurret()) ? charName : name;
            const std::string& fallbackName = displayName.empty() ? (name.empty() ? charName : name) : displayName;

            float currentY = screen.y;
            const float stepY = 15.0f;
            const std::uint32_t textColor = 0xFF00CED1u; // DarkTurquoise

            // 1. Name / CharName
            SDK::Drawing::DrawText(Vec2(screen.x, currentY), fallbackName.c_str(), textColor, false, true);
            currentY += stepY;

            // 2. Object Type
            const char* typeStr = ObjectTypeToString(obj.Type());
            SDK::Drawing::DrawText(Vec2(screen.x, currentY), typeStr, textColor, false, true);
            currentY += stepY;

            // 3. NetworkID
            char netIdTxt[64];
            std::snprintf(netIdTxt, sizeof(netIdTxt), "NetworkID: %u", netId);
            SDK::Drawing::DrawText(Vec2(screen.x, currentY), netIdTxt, textColor, false, true);
            currentY += stepY;

            // 4. Position
            char posTxt[128];
            std::snprintf(posTxt, sizeof(posTxt), "Position: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
            SDK::Drawing::DrawText(Vec2(screen.x, currentY), posTxt, textColor, false, true);
            currentY += stepY;

            // 5. Age
            char ageTxt[64];
            std::snprintf(ageTxt, sizeof(ageTxt), "Age: %.1fs", age);
            SDK::Drawing::DrawText(Vec2(screen.x, currentY), ageTxt, textColor, false, true);
            currentY += stepY;

            // 6. AIBaseClient Info (Health)
            if (obj.IsHero() || obj.IsMinion() || obj.IsTurret()) {
                SDK::AIBaseClient aiObj(obj.Handle());
                if (aiObj.IsValid()) {
                    char hpTxt[128];
                    std::snprintf(hpTxt, sizeof(hpTxt), "Health: %.1f/%.1f (%.1f%%)",
                                  aiObj.Health(), aiObj.MaxHealth(), aiObj.HealthPercent());
                    SDK::Drawing::DrawText(Vec2(screen.x, currentY), hpTxt, textColor, false, true);
                    currentY += stepY;
                }
            }

            // 7. AIHeroClient Info (Spells & Buffs)
            if (obj.IsHero()) {
                SDK::AIHeroClient hero(obj.Handle());
                if (hero.IsValid()) {
                    SDK::Drawing::DrawText(Vec2(screen.x, currentY), "Spells:", textColor, false, true);
                    currentY += stepY;

                    char qTxt[128];
                    std::snprintf(qTxt, sizeof(qTxt), "(Q): %s", hero.GetSpell(SDK::SpellSlot::Q).Name().c_str());
                    SDK::Drawing::DrawText(Vec2(screen.x, currentY), qTxt, textColor, false, true);
                    currentY += stepY;

                    char wTxt[128];
                    std::snprintf(wTxt, sizeof(wTxt), "(W): %s", hero.GetSpell(SDK::SpellSlot::W).Name().c_str());
                    SDK::Drawing::DrawText(Vec2(screen.x, currentY), wTxt, textColor, false, true);
                    currentY += stepY;

                    char eTxt[128];
                    std::snprintf(eTxt, sizeof(eTxt), "(E): %s", hero.GetSpell(SDK::SpellSlot::E).Name().c_str());
                    SDK::Drawing::DrawText(Vec2(screen.x, currentY), eTxt, textColor, false, true);
                    currentY += stepY;

                    char rTxt[128];
                    std::snprintf(rTxt, sizeof(rTxt), "(R): %s", hero.GetSpell(SDK::SpellSlot::R).Name().c_str());
                    SDK::Drawing::DrawText(Vec2(screen.x, currentY), rTxt, textColor, false, true);
                    currentY += stepY;

                    char dTxt[128];
                    std::snprintf(dTxt, sizeof(dTxt), "(D): %s", hero.GetSpell(SDK::SpellSlot::Summoner1).Name().c_str());
                    SDK::Drawing::DrawText(Vec2(screen.x, currentY), dTxt, textColor, false, true);
                    currentY += stepY;

                    char fTxt[128];
                    std::snprintf(fTxt, sizeof(fTxt), "(F): %s", hero.GetSpell(SDK::SpellSlot::Summoner2).Name().c_str());
                    SDK::Drawing::DrawText(Vec2(screen.x, currentY), fTxt, textColor, false, true);
                    currentY += stepY;

                    // Enumerate Buffs
                    uintptr_t buffs[256] = {};
                    const int count = ::CoreBuffs::Enumerate(obj.Address(), buffs, 256);
                    const float gameTime = ::CoreBuffs::ResolveGameTime();

                    bool printedBuffHeader = false;
                    for (int i = 0; i < count; ++i) {
                        const ::CoreBuffs::BuffRef buff{ buffs[i] };
                        if (!buff.IsActive(gameTime)) {
                            continue;
                        }
                        char buffName[96] = {};
                        if (buff.ReadName(buffName, sizeof(buffName)) && buffName[0]) {
                            if (!printedBuffHeader) {
                                SDK::Drawing::DrawText(Vec2(screen.x, currentY), "Buffs:", textColor, false, true);
                                currentY += stepY;
                                printedBuffHeader = true;
                            }
                            char buffTxt[128] = {};
                            std::snprintf(buffTxt, sizeof(buffTxt), "%dx %s", buff.GetStacks(), buffName);
                            SDK::Drawing::DrawText(Vec2(screen.x, currentY), buffTxt, textColor, false, true);
                            currentY += stepY;
                        }
                    }
                }
            }

            // 8. Missile Info
            if (obj.IsMissile()) {
                float speed = 0.0f;
                float mRange = 0.0f;
                GetMissileSpeedAndRange(obj, speed, mRange);

                char speedTxt[128];
                std::snprintf(speedTxt, sizeof(speedTxt), "Missile Speed: %.1f", speed);
                SDK::Drawing::DrawText(Vec2(screen.x, currentY), speedTxt, textColor, false, true);
                currentY += stepY;

                char rangeTxt[128];
                std::snprintf(rangeTxt, sizeof(rangeTxt), "Cast Range: %.1f", mRange);
                SDK::Drawing::DrawText(Vec2(screen.x, currentY), rangeTxt, textColor, false, true);
                currentY += stepY;
            }
        }
    }

    void OnMenu() override {
        if (ImGui::Checkbox("Enable Developer Tools", &enabled_)) {
            if (menuEnabled_) menuEnabled_->SetValue(enabled_);
        }
        if (!enabled_) {
            return;
        }
        if (ImGui::SliderInt("Max object dist from cursor", &maxRange_, 100, 1500)) {
            if (menuMaxRange_) menuMaxRange_->SetValue(maxRange_);
        }

        ImGui::Separator();
        ImGui::Text("Scan Source Provider:");
        if (ImGui::RadioButton("SDK::ObjectManager (Raw RAM)", &scanProviderIndex_, 0)) {
            if (menuProvider_) menuProvider_->SetValue(scanProviderIndex_);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("SDK::GameObjects Facade", &scanProviderIndex_, 1)) {
            if (menuProvider_) menuProvider_->SetValue(scanProviderIndex_);
        }

        bool anySpecificListSelected = false;
        if (scanProviderIndex_ == 1) {
            for (const auto& opt : listOptions_) {
                if (opt.Enabled) {
                    anySpecificListSelected = true;
                    break;
                }
            }
        }

        if (scanProviderIndex_ == 1) {
            ImGui::Separator();
            if (ImGui::CollapsingHeader("GameObjects Specific Lists")) {
                ImGui::Columns(2, "SpecificListsColumns", true);
                for (auto& opt : listOptions_) {
                    if (ImGui::Checkbox(opt.DisplayName, &opt.Enabled)) {
                        if (opt.MenuControl) opt.MenuControl->SetValue(opt.Enabled);
                    }
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }
        }

        if (scanProviderIndex_ == 0 || !anySpecificListSelected) {
            ImGui::Separator();
            ImGui::Text("Category Filters:");
            if (ImGui::Checkbox("Scan All Raw GameObjects (Scan Everything)", &scanRawGameObjects_)) {
                if (menuScanAll_) menuScanAll_->SetValue(scanRawGameObjects_);
            }
            if (!scanRawGameObjects_) {
                if (ImGui::Checkbox("Heroes (AIHeroClient)", &scanHeroes_)) {
                    if (menuScanHeroes_) menuScanHeroes_->SetValue(scanHeroes_);
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Minions & Pets (AIMinionClient)", &scanMinions_)) {
                    if (menuScanMinions_) menuScanMinions_->SetValue(scanMinions_);
                }
                if (ImGui::Checkbox("Turrets (AITurretClient)", &scanTurrets_)) {
                    if (menuScanTurrets_) menuScanTurrets_->SetValue(scanTurrets_);
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Missiles (MissileClient)", &scanMissiles_)) {
                    if (menuScanMissiles_) menuScanMissiles_->SetValue(scanMissiles_);
                }
            }
        }
        if (ImGui::Checkbox("Filter Clutter (FX, Grass, Emitters, MoveTo)", &filterClutter_)) {
            if (menuFilterClutter_) menuFilterClutter_->SetValue(filterClutter_);
        }

        ImGui::Separator();
        ImGui::Text("Active Objects Near Cursor (On Screen):");

        PopulateScanCache();

        activeObjectsCache_.clear();
        const Vec3 cursorPos = SDK::Game::CursorPos();
        const float rangeSqr = static_cast<float>(maxRange_ * maxRange_);

        for (const auto& obj : scanCache_) {
            if (!obj.IsValid()) continue;

            const Vec3 pos = obj.Position();
            if (pos.DistanceSqr(cursorPos) >= rangeSqr) continue;

            const std::string& name = GetObjectName(obj);
            const std::string& charName = GetObjectCharacterName(obj);

            if (IsClutter(obj, name, charName)) continue;

            activeObjectsCache_.push_back(obj);
        }

        if (activeObjectsCache_.empty()) {
            ImGui::Text("No objects near cursor.");
            return;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Hotkey Hint: Press key 'P' to copy ALL objects in table below to Clipboard.");

        if (ImGui::Button("Copy Entire Table to Clipboard (Key 'P')")) {
            std::string copyText = "=== DEVELOPER TOOLS OBJECT TABLE ===\n";
            int count = 0;
            const int now = SDK::Variables::TickCount();
            for (const auto& obj : activeObjectsCache_) {
                const std::string& name = GetObjectName(obj);
                const std::string& charName = GetObjectCharacterName(obj);
                const char* typeStr = ObjectTypeToString(obj.Type());
                std::uint32_t netId = static_cast<std::uint32_t>(obj.NetworkId());
                const char* teamStr = TeamToString(obj);
                char statusBuf[256];
                GetStatusString(obj, statusBuf, sizeof(statusBuf));

                float age = 0.0f;
                auto it = trackedObjectTicks_.find(netId);
                if (it != trackedObjectTicks_.end()) {
                    age = static_cast<float>(now - it->second) / 1000.0f;
                }

                const Vec3 pos = obj.Position();
                char lineBuf[512];
                std::snprintf(lineBuf, sizeof(lineBuf),
                              "[%d] Name: %s | CharName: %s | NetId: %u | Addr: 0x%llX | Type: %s | Team: %s | Status: %s | Age: %.1fs | Pos: (%.1f, %.1f, %.1f)\n",
                              ++count, name.c_str(), charName.c_str(), netId,
                              static_cast<unsigned long long>(obj.Address()),
                              typeStr, teamStr, statusBuf, age,
                              pos.x, pos.y, pos.z);
                copyText += lineBuf;
            }
            if (count > 0) {
                ImGui::SetClipboardText(copyText.c_str());
                NightSharpDebug::Logf("[Dev] Copied ALL %d objects in table to Clipboard!", count);
            }
        }

        if (ImGui::BeginTable("OnScreenObjectsTable", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("CharName");
            ImGui::TableSetupColumn("NetId");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Team");
            ImGui::TableSetupColumn("Status");
            ImGui::TableSetupColumn("Dist to Mouse");
            ImGui::TableSetupColumn("Age (s)");
            ImGui::TableSetupColumn("Action");
            ImGui::TableHeadersRow();

            const int now = SDK::Variables::TickCount();
            for (const auto& obj : activeObjectsCache_) {
                const std::string& name = GetObjectName(obj);
                const std::string& charName = GetObjectCharacterName(obj);
                const char* typeStr = ObjectTypeToString(obj.Type());
                const char* teamStr = TeamToString(obj);
                char statusBuf[256];
                GetStatusString(obj, statusBuf, sizeof(statusBuf));
                float dist = obj.Position().Distance(cursorPos);

                std::uint32_t netId = static_cast<std::uint32_t>(obj.NetworkId());
                float age = 0.0f;
                auto it = trackedObjectTicks_.find(netId);
                if (it != trackedObjectTicks_.end()) {
                    age = static_cast<float>(now - it->second) / 1000.0f;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(charName.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%u", netId);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(typeStr);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(teamStr);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(statusBuf);
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", dist);
                ImGui::TableNextColumn();
                ImGui::Text("%.1fs", age);
                ImGui::TableNextColumn();

                char btnId[64];
                std::snprintf(btnId, sizeof(btnId), "Log##%u", netId);
                if (ImGui::Button(btnId)) {
                    NightSharpDebug::Logf("[Dev] Name: %s | CharName: %s | NetId: %u | Team: %s | Status: %s | Age: %.1fs",
                                               name.c_str(), charName.c_str(), netId, teamStr, statusBuf, age);
                }
            }
            ImGui::EndTable();
        }
    }

private:
    bool enabled_ = true;
    int maxRange_ = 400;
    int scanProviderIndex_ = 0; // 0 = ObjectManager, 1 = GameObjects Facade
    struct ListOption {
        const char* Name;
        const char* DisplayName;
        GameObjectListType Type;
        bool Enabled;
        SDK::UI::MenuBool* MenuControl;
    };

    mutable ListOption listOptions_[48] = {
        { "AllGameObjects", "All Game Objects (AllGameObjects)", GameObjectListType::AllGameObjects, false, nullptr },
        { "AttackableUnits", "Attackable Units (AttackableUnits)", GameObjectListType::AttackableUnits, false, nullptr },
        { "Ally", "Allies (Ally)", GameObjectListType::Ally, false, nullptr },
        { "Enemy", "Enemies (Enemy)", GameObjectListType::Enemy, false, nullptr },
        { "Heroes", "Heroes (Heroes)", GameObjectListType::Heroes, false, nullptr },
        { "AllyHeroes", "Ally Heroes (AllyHeroes)", GameObjectListType::AllyHeroes, false, nullptr },
        { "EnemyHeroes", "Enemy Heroes (EnemyHeroes)", GameObjectListType::EnemyHeroes, false, nullptr },
        { "Minions", "Minions (Minions)", GameObjectListType::Minions, false, nullptr },
        { "AllyMinions", "Ally Minions (AllyMinions)", GameObjectListType::AllyMinions, false, nullptr },
        { "EnemyMinions", "Enemy Minions (EnemyMinions)", GameObjectListType::EnemyMinions, false, nullptr },
        { "AllyLaneMinions", "Ally Lane Minions (AllyLaneMinions)", GameObjectListType::AllyLaneMinions, false, nullptr },
        { "EnemyLaneMinions", "Enemy Lane Minions (EnemyLaneMinions)", GameObjectListType::EnemyLaneMinions, false, nullptr },
        { "AllySpecialMinions", "Ally Special Minions (AllySpecialMinions)", GameObjectListType::AllySpecialMinions, false, nullptr },
        { "EnemySpecialMinions", "Enemy Special Minions (EnemySpecialMinions)", GameObjectListType::EnemySpecialMinions, false, nullptr },
        { "AllyIgnoredMinions", "Ally Ignored Minions (AllyIgnoredMinions)", GameObjectListType::AllyIgnoredMinions, false, nullptr },
        { "EnemyIgnoredMinions", "Enemy Ignored Minions (EnemyIgnoredMinions)", GameObjectListType::EnemyIgnoredMinions, false, nullptr },
        { "Wards", "Wards (Wards)", GameObjectListType::Wards, false, nullptr },
        { "AllyWards", "Ally Wards (AllyWards)", GameObjectListType::AllyWards, false, nullptr },
        { "EnemyWards", "Enemy Wards (EnemyWards)", GameObjectListType::EnemyWards, false, nullptr },
        { "Jungle", "Jungle Minions (Jungle)", GameObjectListType::Jungle, false, nullptr },
        { "JungleSmall", "Jungle Small (JungleSmall)", GameObjectListType::JungleSmall, false, nullptr },
        { "JungleLarge", "Jungle Large (JungleLarge)", GameObjectListType::JungleLarge, false, nullptr },
        { "JungleLegendary", "Jungle Legendary (JungleLegendary)", GameObjectListType::JungleLegendary, false, nullptr },
        { "Plants", "Plants (Plants)", GameObjectListType::Plants, false, nullptr },
        { "Clones", "Clones (Clones)", GameObjectListType::Clones, false, nullptr },
        { "AllyClones", "Ally Clones (AllyClones)", GameObjectListType::AllyClones, false, nullptr },
        { "EnemyClones", "Enemy Clones (EnemyClones)", GameObjectListType::EnemyClones, false, nullptr },
        { "Pets", "Pets (Pets)", GameObjectListType::Pets, false, nullptr },
        { "AllyPets", "Ally Pets (AllyPets)", GameObjectListType::AllyPets, false, nullptr },
        { "EnemyPets", "Enemy Pets (EnemyPets)", GameObjectListType::EnemyPets, false, nullptr },
        { "Turrets", "Turrets (Turrets)", GameObjectListType::Turrets, false, nullptr },
        { "AllyTurrets", "Ally Turrets (AllyTurrets)", GameObjectListType::AllyTurrets, false, nullptr },
        { "EnemyTurrets", "Enemy Turrets (EnemyTurrets)", GameObjectListType::EnemyTurrets, false, nullptr },
        { "Inhibitors", "Inhibitors (Inhibitors)", GameObjectListType::Inhibitors, false, nullptr },
        { "AllyInhibitors", "Ally Inhibitors (AllyInhibitors)", GameObjectListType::AllyInhibitors, false, nullptr },
        { "EnemyInhibitors", "Enemy Inhibitors (EnemyInhibitors)", GameObjectListType::EnemyInhibitors, false, nullptr },
        { "Nexuses", "Nexuses (Nexuses)", GameObjectListType::Nexuses, false, nullptr },
        { "AllyNexus", "Ally Nexus (AllyNexus)", GameObjectListType::AllyNexus, false, nullptr },
        { "EnemyNexus", "Enemy Nexus (EnemyNexus)", GameObjectListType::EnemyNexus, false, nullptr },
        { "Shops", "Shops (Shops)", GameObjectListType::Shops, false, nullptr },
        { "AllyShops", "Ally Shops (AllyShops)", GameObjectListType::AllyShops, false, nullptr },
        { "EnemyShops", "Enemy Shops (EnemyShops)", GameObjectListType::EnemyShops, false, nullptr },
        { "SpawnPoints", "Spawn Points (SpawnPoints)", GameObjectListType::SpawnPoints, false, nullptr },
        { "AllySpawnPoints", "Ally Spawn Points (AllySpawnPoints)", GameObjectListType::AllySpawnPoints, false, nullptr },
        { "EnemySpawnPoints", "Enemy Spawn Points (EnemySpawnPoints)", GameObjectListType::EnemySpawnPoints, false, nullptr },
        { "ParticleEmitters", "Particle Emitters (ParticleEmitters)", GameObjectListType::ParticleEmitters, false, nullptr },
        { "Missiles", "Missiles (Missiles)", GameObjectListType::Missiles, false, nullptr },
        { "Player", "Player Object (Player)", GameObjectListType::Player, false, nullptr }
    };
    bool scanRawGameObjects_ = true;
    bool scanHeroes_ = true;
    bool scanMinions_ = true;
    bool scanTurrets_ = true;
    bool scanMissiles_ = true;
    bool filterClutter_ = true;
    std::unordered_map<std::uint32_t, int> trackedObjectTicks_;
    bool pKeyPressedLast_ = false;
    mutable std::vector<SDK::GameObject> scanCache_;
    mutable std::vector<SDK::GameObject> activeObjectsCache_;
    static inline DeveloperToolsPlugin* s_instance = nullptr;

    template <typename T>
    void AddUniqueObjectsFromSource(std::vector<SDK::GameObject>& dest, const std::vector<T>& source) const {
        for (const auto& obj : source) {
            if (!obj.IsValid()) continue;
            auto it = std::find_if(dest.begin(), dest.end(), [&obj](const SDK::GameObject& item) {
                return item.IsValid() && item.Address() == obj.Address();
            });
            if (it == dest.end()) {
                dest.push_back(SDK::GameObject(obj.Handle()));
            }
        }
    }

    void AddUniqueObject(std::vector<SDK::GameObject>& dest, const SDK::GameObject& obj) const {
        if (!obj.IsValid()) return;
        auto it = std::find_if(dest.begin(), dest.end(), [&obj](const SDK::GameObject& item) {
            return item.IsValid() && item.Address() == obj.Address();
        });
        if (it == dest.end()) {
            dest.push_back(obj);
        }
    }

    void PopulateScanCache() const {
        scanCache_.clear();
        if (scanProviderIndex_ == 0) {
            // Using SDK::ObjectManager::Get
            if (scanRawGameObjects_) {
                for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
                    if (obj.IsValid()) scanCache_.push_back(obj);
                }
            } else {
                if (scanHeroes_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
                        if (obj.IsValid()) scanCache_.push_back(obj);
                    }
                }
                if (scanMinions_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::AIMinionClient>()) {
                        if (obj.IsValid()) scanCache_.push_back(obj);
                    }
                }
                if (scanTurrets_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::AITurretClient>()) {
                        if (obj.IsValid()) scanCache_.push_back(obj);
                    }
                }
                if (scanMissiles_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::MissileClient>()) {
                        if (obj.IsValid()) scanCache_.push_back(obj);
                    }
                }
            }
        } else {
            // Using SDK::GameObjects Facade
            std::lock_guard<std::recursive_mutex> lk(SDK::GameObjects::detail::g_mutex);

            bool anySpecificListSelected = false;
            for (const auto& opt : listOptions_) {
                if (opt.Enabled) {
                    anySpecificListSelected = true;
                    break;
                }
            }

            if (anySpecificListSelected) {
                for (const auto& opt : listOptions_) {
                    if (opt.Enabled) {
                        switch (opt.Type) {
                        case GameObjectListType::AllGameObjects:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::GameObjectsList);
                            break;
                        case GameObjectListType::AttackableUnits:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AttackableUnitsList);
                            break;
                        case GameObjectListType::Ally:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyList);
                            break;
                        case GameObjectListType::Enemy:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyList);
                            break;
                        case GameObjectListType::Heroes:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::HeroesList);
                            break;
                        case GameObjectListType::AllyHeroes:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyHeroesList);
                            break;
                        case GameObjectListType::EnemyHeroes:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyHeroesList);
                            break;
                        case GameObjectListType::Minions:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::MinionsList);
                            break;
                        case GameObjectListType::AllyMinions:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyMinionsList);
                            break;
                        case GameObjectListType::EnemyMinions:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyMinionsList);
                            break;
                        case GameObjectListType::AllyLaneMinions:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyLaneMinionsList);
                            break;
                        case GameObjectListType::EnemyLaneMinions:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyLaneMinionsList);
                            break;
                        case GameObjectListType::AllySpecialMinions:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllySpecialMinionsList);
                            break;
                        case GameObjectListType::EnemySpecialMinions:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemySpecialMinionsList);
                            break;
                        case GameObjectListType::AllyIgnoredMinions:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyIgnoredMinionsList);
                            break;
                        case GameObjectListType::EnemyIgnoredMinions:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyIgnoredMinionsList);
                            break;
                        case GameObjectListType::Wards:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::WardsList);
                            break;
                        case GameObjectListType::AllyWards:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyWardsList);
                            break;
                        case GameObjectListType::EnemyWards:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyWardsList);
                            break;
                        case GameObjectListType::Jungle:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::JungleList);
                            break;
                        case GameObjectListType::JungleSmall:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::JungleSmallList);
                            break;
                        case GameObjectListType::JungleLarge:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::JungleLargeList);
                            break;
                        case GameObjectListType::JungleLegendary:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::JungleLegendaryList);
                            break;
                        case GameObjectListType::Plants:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::PlantsList);
                            break;
                        case GameObjectListType::Clones:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::ClonesList);
                            break;
                        case GameObjectListType::AllyClones:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyClonesList);
                            break;
                        case GameObjectListType::EnemyClones:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyClonesList);
                            break;
                        case GameObjectListType::Pets:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::PetsList);
                            break;
                        case GameObjectListType::AllyPets:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyPetsList);
                            break;
                        case GameObjectListType::EnemyPets:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyPetsList);
                            break;
                        case GameObjectListType::Turrets:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::TurretsList);
                            break;
                        case GameObjectListType::AllyTurrets:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyTurretsList);
                            break;
                        case GameObjectListType::EnemyTurrets:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyTurretsList);
                            break;
                        case GameObjectListType::Inhibitors:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::InhibitorsList);
                            break;
                        case GameObjectListType::AllyInhibitors:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyInhibitorsList);
                            break;
                        case GameObjectListType::EnemyInhibitors:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyInhibitorsList);
                            break;
                        case GameObjectListType::Nexuses:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::NexusList);
                            break;
                        case GameObjectListType::AllyNexus:
                            AddUniqueObject(scanCache_, SDK::GameObjects::detail::AllyNexusObject);
                            break;
                        case GameObjectListType::EnemyNexus:
                            AddUniqueObject(scanCache_, SDK::GameObjects::detail::EnemyNexusObject);
                            break;
                        case GameObjectListType::Shops:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::ShopsList);
                            break;
                        case GameObjectListType::AllyShops:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyShopsList);
                            break;
                        case GameObjectListType::EnemyShops:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyShopsList);
                            break;
                        case GameObjectListType::SpawnPoints:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::SpawnPointsList);
                            break;
                        case GameObjectListType::AllySpawnPoints:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllySpawnPointsList);
                            break;
                        case GameObjectListType::EnemySpawnPoints:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemySpawnPointsList);
                            break;
                        case GameObjectListType::ParticleEmitters:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::ParticleEmittersList);
                            break;
                        case GameObjectListType::Missiles:
                            AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::MissilesList);
                            break;
                        case GameObjectListType::Player:
                            AddUniqueObject(scanCache_, SDK::GameObjects::detail::PlayerObject);
                            break;
                        default:
                            break;
                        }
                    }
                }
            } else {
                if (scanRawGameObjects_) {
                    AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::GameObjectsList);
                } else {
                    if (scanHeroes_) {
                        AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::HeroesList);
                    }
                    if (scanMinions_) {
                        AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::MinionsList);
                    }
                    if (scanTurrets_) {
                        AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::TurretsList);
                    }
                    if (scanMissiles_) {
                        AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::MissilesList);
                    }
                }
            }
        }
    }

    bool IsClutter(const SDK::GameObject& obj, const std::string& name, const std::string& charName) const {
        if (!filterClutter_) {
            return false;
        }
        if (name.empty() && charName.empty()) {
            return true;
        }
        if (name == "missile" || name.find("MoveTo") != std::string::npos ||
            charName.find("Grass") != std::string::npos || charName.find("FX") != std::string::npos ||
            charName.find("LevelProp") != std::string::npos || charName.find("emitter") != std::string::npos) {
            return true;
        }
        return false;
    }

    static void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance && args.Sender.NetworkId != 0) {
            s_instance->trackedObjectTicks_.erase(static_cast<std::uint32_t>(args.Sender.NetworkId));
        }
    }

    static void OnProcessSpellCast(const SDK::Events::ProcessSpellEventArgs& args) {
        const auto player = SDK::ObjectManager::Player();
        if (player.IsValid() && args.CasterNetworkId == player.NetworkId()) {
            char debugMsg[256];
            std::snprintf(debugMsg, sizeof(debugMsg), "Detected Spell Name: %s Issued By: %s",
                          args.SpellName[0] ? args.SpellName : args.ScriptName,
                          player.CharacterName().c_str());
            NightSharpDebug::Logf("%s", debugMsg);
        }
    }

    static const std::string& GetObjectName(const SDK::GameObject& object) {
        if (!object.IsValid()) {
            static const std::string empty;
            return empty;
        }
        return object.Name();
    }

    static const std::string& GetObjectCharacterName(const SDK::GameObject& object) {
        if (!object.IsValid()) {
            static const std::string empty;
            return empty;
        }
        return object.CharacterName();
    }

    static void GetMissileSpeedAndRange(const SDK::GameObject& obj, float& speed, float& range) {
        speed = 0.0f;
        range = 0.0f;

        const uintptr_t a = obj.Address();
        if (!a) return;

        const uintptr_t spellData = Globals::Read<uintptr_t>(a + Offset::MissileClient::SpellDataPtr);
        if (!Globals::IsValidPtr(spellData)) return;

        const uintptr_t spellDataObj = Globals::Read<uintptr_t>(spellData + 0x00);
        if (!Globals::IsValidPtr(spellDataObj)) return;

        const uintptr_t resource = Globals::Read<uintptr_t>(spellDataObj + 0x60); // Offset::SpellDataLayout::DataResource
        if (!Globals::IsValidPtr(resource)) return;

        range = Globals::Read<float>(resource + 0x478); // Offset::SpellDataResourceLayout::ResCastRange
        speed = Globals::Read<float>(resource + 0x518); // Offset::SpellDataResourceLayout::ResMissileSpeed
    }

    static const char* TeamToString(const SDK::GameObject& obj) {
        if (!obj.IsValid()) return "Unknown";
        if (obj.IsAlly()) return "Ally";
        if (obj.IsEnemy()) return "Enemy";
        const auto team = obj.Team();
        if (team == SDK::GameObjectTeam::Neutral) return "Neutral";
        if (team == SDK::GameObjectTeam::Order) return "Blue (100)";
        if (team == SDK::GameObjectTeam::Chaos) return "Red (200)";
        return "Unknown";
    }

    static void GetStatusString(const SDK::GameObject& obj, char* buf, size_t maxLen) {
        if (!obj.IsValid()) {
            std::snprintf(buf, maxLen, "Unknown");
            return;
        }
        int len = 0;
        if (obj.IsDead()) len += std::snprintf(buf + len, maxLen - len, "Dead");
        else len += std::snprintf(buf + len, maxLen - len, "Alive");

        if (!obj.IsVisible()) len += std::snprintf(buf + len, maxLen - len, ", Fog");
        if (!obj.IsTargetable()) len += std::snprintf(buf + len, maxLen - len, ", Untargetable");
        if (obj.IsInvulnerable()) len += std::snprintf(buf + len, maxLen - len, ", Invulnerable");

        if (obj.IsHero() || obj.IsMinion() || obj.IsTurret()) {
            SDK::AIBaseClient ai(obj.Handle());
            if (ai.IsValid()) {
                len += std::snprintf(buf + len, maxLen - len, ", HP:%.0f/%.0f(%.0f%%)",
                                     ai.Health(), ai.MaxHealth(), ai.HealthPercent());
            }
        }
    }

    static const char* ObjectTypeToString(::Core::Objects::ObjectType type) {
        switch (type) {
        case ::Core::Objects::ObjectType::GameObject: return "GameObject";
        case ::Core::Objects::ObjectType::AIHeroClient: return "AIHeroClient";
        case ::Core::Objects::ObjectType::AIMinionClient: return "AIMinionClient";
        case ::Core::Objects::ObjectType::AITurretClient: return "AITurretClient";
        case ::Core::Objects::ObjectType::MissileClient: return "MissileClient";
        case ::Core::Objects::ObjectType::BarracksDampenerClient: return "BarracksDampenerClient";
        case ::Core::Objects::ObjectType::HQClient: return "HQClient";
        case ::Core::Objects::ObjectType::ShopClient: return "ShopClient";
        case ::Core::Objects::ObjectType::Obj_SpawnPoint: return "Obj_SpawnPoint";
        case ::Core::Objects::ObjectType::EffectEmitter: return "EffectEmitter";
        default: return "Unknown";
        }
    }

    static void OnMenuBridge(void* userData) {
        if (auto* self = static_cast<DeveloperToolsPlugin*>(userData)) {
            self->OnMenu();
        }
    }

    void DestroyNativeMenu() {
        if (menu_) {
            SDK::UI::MenuManager::Instance().Remove(menu_);
            delete menu_;
            menu_ = nullptr;
            menuEnabled_ = nullptr;
            menuMaxRange_ = nullptr;
            menuProvider_ = nullptr;
            for (auto& opt : listOptions_) {
                opt.MenuControl = nullptr;
            }
            menuScanAll_ = nullptr;
            menuScanHeroes_ = nullptr;
            menuScanMinions_ = nullptr;
            menuScanTurrets_ = nullptr;
            menuScanMissiles_ = nullptr;
            menuFilterClutter_ = nullptr;
            menuInspector_ = nullptr;
        }
    }

private:
    SDK::UI::Menu* menu_ = nullptr;
    SDK::UI::MenuBool* menuEnabled_ = nullptr;
    SDK::UI::MenuSlider* menuMaxRange_ = nullptr;
    SDK::UI::MenuList* menuProvider_ = nullptr;
    SDK::UI::MenuBool* menuScanAll_ = nullptr;
    SDK::UI::MenuBool* menuScanHeroes_ = nullptr;
    SDK::UI::MenuBool* menuScanMinions_ = nullptr;
    SDK::UI::MenuBool* menuScanTurrets_ = nullptr;
    SDK::UI::MenuBool* menuScanMissiles_ = nullptr;
    SDK::UI::MenuBool* menuFilterClutter_ = nullptr;
    SDK::UI::MenuRuntime* menuInspector_ = nullptr;
};

inline DeveloperToolsPlugin* s_instance = nullptr;

} // namespace Plugins
