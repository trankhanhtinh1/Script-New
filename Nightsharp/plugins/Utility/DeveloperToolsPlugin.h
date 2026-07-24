#pragma once

#include "../IPlugin.h"
#include "../../SDK/SDK.h"
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
        SDK::Events::AddOnProcessSpell(&DeveloperToolsPlugin::OnProcessSpellCast);
        SDK::Events::AddOnDeleteObject(&DeveloperToolsPlugin::OnObjectDelete);

        DestroyNativeMenu();
        menu_ = new SDK::UI::Menu(GetInternalId(), GetName(), true);
        menuEnabled_ = menu_->Add(new SDK::UI::MenuBool("Enabled", "Enable Developer Tools", enabled_));
        menuMaxRange_ = menu_->Add(new SDK::UI::MenuSlider("MaxRange", "Max Scan Range", maxRange_, 100, 1500));
        menuProvider_ = menu_->Add(new SDK::UI::MenuList("Provider", "Scan Provider", { "SDK::ObjectManager (Raw RAM)", "SDK::GameObjects Facade" }, scanProviderIndex_));

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

    void OnUpdate() override {
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

            for (const auto& obj : GetObjectsToScan()) {
                if (!obj.IsValid()) continue;

                const Vec3 pos = obj.Position();
                if (pos.DistanceSqr(cursorPos) >= rangeSqr) continue;

                std::string name = GetObjectName(obj);
                std::string charName = GetObjectCharacterName(obj);
                if (IsClutter(obj, name, charName)) continue;

                std::string typeStr = ObjectTypeToString(obj.Type());
                std::uint32_t netId = static_cast<std::uint32_t>(obj.NetworkId());
                std::string teamStr = TeamToString(obj);
                std::string statusStr = StatusToString(obj);

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
                              typeStr.c_str(), teamStr.c_str(), statusStr.c_str(), age,
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
        if (!enabled_ || !SDK::Drawing::IsEnabled()) {
            return;
        }

        const Vec3 cursorPos = SDK::Game::CursorPos();
        const float rangeSqr = static_cast<float>(maxRange_ * maxRange_);
        const int now = SDK::Variables::TickCount();

        for (const auto& obj : GetObjectsToScan()) {
            if (!obj.IsValid()) {
                continue;
            }

            const Vec3 pos = obj.Position();
            if (pos.DistanceSqr(cursorPos) >= rangeSqr) {
                continue;
            }

            std::string name = GetObjectName(obj);
            std::string charName = GetObjectCharacterName(obj);

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
            std::string displayName = obj.IsHero() || obj.IsMinion() || obj.IsTurret() ? charName : name;
            if (displayName.empty()) {
                displayName = name.empty() ? charName : name;
            }

            float currentY = screen.y;
            const float stepY = 15.0f;
            const std::uint32_t textColor = 0xFF00CED1u; // DarkTurquoise

            // 1. Name / CharName
            SDK::Drawing::DrawText(Vec2(screen.x, currentY), displayName.c_str(), textColor, false, true);
            currentY += stepY;

            // 2. Object Type
            std::string typeStr = ObjectTypeToString(obj.Type());
            SDK::Drawing::DrawText(Vec2(screen.x, currentY), typeStr.c_str(), textColor, false, true);
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
        ImGui::Checkbox("Enable Developer Tools", &enabled_);
        if (!enabled_) {
            return;
        }
        ImGui::SliderInt("Max object dist from cursor", &maxRange_, 100, 1500);

        ImGui::Separator();
        ImGui::Text("Scan Source Provider:");
        ImGui::RadioButton("SDK::ObjectManager (Raw RAM)", &scanProviderIndex_, 0); ImGui::SameLine();
        ImGui::RadioButton("SDK::GameObjects Facade", &scanProviderIndex_, 1);

        ImGui::Separator();
        ImGui::Text("Category Filters:");
        ImGui::Checkbox("Scan All Raw GameObjects (Scan Everything)", &scanRawGameObjects_);
        if (!scanRawGameObjects_) {
            ImGui::Checkbox("Heroes (AIHeroClient)", &scanHeroes_); ImGui::SameLine();
            ImGui::Checkbox("Minions & Pets (AIMinionClient)", &scanMinions_);
            ImGui::Checkbox("Turrets (AITurretClient)", &scanTurrets_); ImGui::SameLine();
            ImGui::Checkbox("Missiles (MissileClient)", &scanMissiles_);
        }
        ImGui::Checkbox("Filter Clutter (FX, Grass, Emitters, MoveTo)", &filterClutter_);

        ImGui::Separator();
        ImGui::Text("Active Objects Near Cursor (On Screen):");

        std::vector<SDK::GameObject> activeObjects;
        const Vec3 cursorPos = SDK::Game::CursorPos();
        const float rangeSqr = static_cast<float>(maxRange_ * maxRange_);

        for (const auto& obj : GetObjectsToScan()) {
            if (!obj.IsValid()) continue;

            const Vec3 pos = obj.Position();
            if (pos.DistanceSqr(cursorPos) >= rangeSqr) continue;

            std::string name = GetObjectName(obj);
            std::string charName = GetObjectCharacterName(obj);

            if (IsClutter(obj, name, charName)) continue;

            activeObjects.push_back(obj);
        }

        if (activeObjects.empty()) {
            ImGui::Text("No objects near cursor.");
            return;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Hotkey Hint: Press key 'P' to copy ALL objects in table below to Clipboard.");

        if (ImGui::Button("Copy Entire Table to Clipboard (Key 'P')")) {
            std::string copyText = "=== DEVELOPER TOOLS OBJECT TABLE ===\n";
            int count = 0;
            const int now = SDK::Variables::TickCount();
            for (const auto& obj : activeObjects) {
                std::string name = GetObjectName(obj);
                std::string charName = GetObjectCharacterName(obj);
                std::string typeStr = ObjectTypeToString(obj.Type());
                std::uint32_t netId = static_cast<std::uint32_t>(obj.NetworkId());
                std::string teamStr = TeamToString(obj);
                std::string statusStr = StatusToString(obj);

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
                              typeStr.c_str(), teamStr.c_str(), statusStr.c_str(), age,
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
            for (const auto& obj : activeObjects) {
                std::string name = GetObjectName(obj);
                std::string charName = GetObjectCharacterName(obj);
                std::string typeStr = ObjectTypeToString(obj.Type());
                std::string teamStr = TeamToString(obj);
                std::string statusStr = StatusToString(obj);
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
                ImGui::TextUnformatted(typeStr.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(teamStr.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(statusStr.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", dist);
                ImGui::TableNextColumn();
                ImGui::Text("%.1fs", age);
                ImGui::TableNextColumn();

                char btnId[64];
                std::snprintf(btnId, sizeof(btnId), "Log##%u", netId);
                if (ImGui::Button(btnId)) {
                    NightSharpDebug::Logf("[Dev] Name: %s | CharName: %s | NetId: %u | Team: %s | Status: %s | Age: %.1fs",
                                               name.c_str(), charName.c_str(), netId, teamStr.c_str(), statusStr.c_str(), age);
                }
            }
            ImGui::EndTable();
        }
    }

private:
    bool enabled_ = true;
    int maxRange_ = 400;
    int scanProviderIndex_ = 0; // 0 = ObjectManager, 1 = GameObjects Facade
    bool scanRawGameObjects_ = true;
    bool scanHeroes_ = true;
    bool scanMinions_ = true;
    bool scanTurrets_ = true;
    bool scanMissiles_ = true;
    bool filterClutter_ = true;
    std::unordered_map<std::uint32_t, int> trackedObjectTicks_;
    bool pKeyPressedLast_ = false;
    static inline DeveloperToolsPlugin* s_instance = nullptr;

    std::vector<SDK::GameObject> GetObjectsToScan() const {
        std::vector<SDK::GameObject> result;
        if (scanProviderIndex_ == 0) {
            // Using SDK::ObjectManager::Get
            if (scanRawGameObjects_) {
                for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
                    if (obj.IsValid()) result.push_back(obj);
                }
            } else {
                if (scanHeroes_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
                        if (obj.IsValid()) result.push_back(obj);
                    }
                }
                if (scanMinions_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::AIMinionClient>()) {
                        if (obj.IsValid()) result.push_back(obj);
                    }
                }
                if (scanTurrets_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::AITurretClient>()) {
                        if (obj.IsValid()) result.push_back(obj);
                    }
                }
                if (scanMissiles_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::MissileClient>()) {
                        if (obj.IsValid()) result.push_back(obj);
                    }
                }
            }
        } else {
            // Using SDK::GameObjects Facade
            if (scanRawGameObjects_) {
                for (const auto& obj : SDK::GameObjects::AllGameObjects()) {
                    if (obj.IsValid()) result.push_back(obj);
                }
            } else {
                if (scanHeroes_) {
                    for (const auto& obj : SDK::GameObjects::Heroes()) {
                        if (obj.IsValid()) result.push_back(obj);
                    }
                }
                if (scanMinions_) {
                    for (const auto& obj : SDK::GameObjects::Minions()) {
                        if (obj.IsValid()) result.push_back(obj);
                    }
                }
                if (scanTurrets_) {
                    for (const auto& obj : SDK::GameObjects::Turrets()) {
                        if (obj.IsValid()) result.push_back(obj);
                    }
                }
                if (scanMissiles_) {
                    for (const auto& obj : SDK::GameObjects::Missiles()) {
                        if (obj.IsValid()) result.push_back(obj);
                    }
                }
            }
        }
        return result;
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

    static std::string GetObjectName(const SDK::GameObject& object) {
        if (!object.IsValid()) {
            return {};
        }
        std::string name = object.Name();
        if (!name.empty()) {
            return name;
        }
        char nameBuf[96] = {};
        if (::Core::Objects::ReadName(object.Address(), nameBuf, sizeof(nameBuf)) && nameBuf[0]) {
            return nameBuf;
        }
        return {};
    }

    static std::string GetObjectCharacterName(const SDK::GameObject& object) {
        if (!object.IsValid()) {
            return {};
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

    static std::string TeamToString(const SDK::GameObject& obj) {
        if (!obj.IsValid()) return "Unknown";
        if (obj.IsAlly()) return "Ally";
        if (obj.IsEnemy()) return "Enemy";
        const auto team = obj.Team();
        if (team == SDK::GameObjectTeam::Neutral) return "Neutral";
        if (team == SDK::GameObjectTeam::Order) return "Blue (100)";
        if (team == SDK::GameObjectTeam::Chaos) return "Red (200)";
        return "Unknown";
    }

    static std::string StatusToString(const SDK::GameObject& obj) {
        std::string status;
        if (obj.IsDead()) status += "Dead";
        else status += "Alive";

        if (!obj.IsVisible()) status += ", Fog";
        if (!obj.IsTargetable()) status += ", Untargetable";
        if (obj.IsInvulnerable()) status += ", Invulnerable";

        if (obj.IsHero() || obj.IsMinion() || obj.IsTurret()) {
            SDK::AIBaseClient ai(obj.Handle());
            if (ai.IsValid()) {
                char hpBuf[64];
                std::snprintf(hpBuf, sizeof(hpBuf), ", HP:%.0f/%.0f(%.0f%%)",
                              ai.Health(), ai.MaxHealth(), ai.HealthPercent());
                status += hpBuf;
            }
        }
        return status;
    }

    static std::string ObjectTypeToString(::Core::Objects::ObjectType type) {
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
