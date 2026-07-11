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
        enabled_ = true;
        maxRange_ = 400;
        trackedObjectTicks_.clear();
        SDK::Events::AddOnProcessSpell(&DeveloperToolsPlugin::OnProcessSpellCast);
    }

    void OnUnload() override {
        SDK::Events::RemoveOnProcessSpell(&DeveloperToolsPlugin::OnProcessSpellCast);
    }

    void OnUpdate() override {
        if (!enabled_) {
            pKeyPressedLast_ = false;
            return;
        }

        bool isDown = (GetAsyncKeyState('P') & 0x8000) != 0;
        if (isDown && !pKeyPressedLast_) {
            const Vec3 cursorPos = SDK::Game::CursorPos();
            float closestDist = 999999.0f;
            SDK::GameObject closestObj;

            for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
                if (!obj.IsValid()) continue;

                std::string name = GetObjectName(obj);
                std::string charName = GetObjectCharacterName(obj);
                if (name.empty() && charName.empty()) continue;
                if (name == "missile" || name.find("MoveTo") != std::string::npos ||
                    charName.find("Grass") != std::string::npos || charName.find("FX") != std::string::npos ||
                    charName.find("LevelProp") != std::string::npos || charName.find("emitter") != std::string::npos) {
                    continue;
                }

                float dist = obj.Position().Distance(cursorPos);
                if (dist < closestDist) {
                    closestDist = dist;
                    closestObj = obj;
                }
            }

            if (closestObj.IsValid() && closestDist <= static_cast<float>(maxRange_)) {
                std::string name = GetObjectName(closestObj);
                std::string charName = GetObjectCharacterName(closestObj);
                std::string typeStr = ObjectTypeToString(closestObj.Type());
                std::uint32_t netId = static_cast<std::uint32_t>(closestObj.NetworkId());
                
                float age = 0.0f;
                auto it = trackedObjectTicks_.find(netId);
                if (it != trackedObjectTicks_.end()) {
                    age = static_cast<float>(SDK::Variables::TickCount() - it->second) / 1000.0f;
                }

                char copyBuf[512];
                std::snprintf(copyBuf, sizeof(copyBuf),
                              "Name: %s | CharName: %s | NetId: %u | Addr: 0x%llX | Type: %s | Age: %.1fs | Position: (%.1f, %.1f, %.1f)",
                              name.c_str(), charName.c_str(), netId,
                              static_cast<unsigned long long>(closestObj.Address()),
                              typeStr.c_str(), age,
                              closestObj.Position().x, closestObj.Position().y, closestObj.Position().z);

                ImGui::SetClipboardText(copyBuf);
                SDK::Orbwalker::DebugPrint("[Dev] Copied to Clipboard: %s", name.c_str());
            }
        }
        pKeyPressedLast_ = isDown;
    }

    void OnRender() override {
        if (!enabled_ || !SDK::Drawing::IsEnabled()) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        const bool hasPlayer = player.IsValid();
        const Vec3 playerPosition = hasPlayer ? player.Position() : Vec3{};
        const Vec3 cursorPos = SDK::Game::CursorPos();
        const float rangeSqr = static_cast<float>(maxRange_ * maxRange_);
        const int now = SDK::Variables::TickCount();

        for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
            if (!obj.IsValid()) {
                continue;
            }

            const Vec3 pos = obj.Position();
            if (pos.DistanceSqr(cursorPos) >= rangeSqr) {
                continue;
            }

            std::string name = GetObjectName(obj);
            std::string charName = GetObjectCharacterName(obj);

            // Skip clutter objects
            if (name.empty() && charName.empty()) {
                continue;
            }
            if (name == "missile" || name.find("MoveTo") != std::string::npos ||
                charName.find("Grass") != std::string::npos || charName.find("FX") != std::string::npos ||
                charName.find("LevelProp") != std::string::npos || charName.find("emitter") != std::string::npos) {
                continue;
            }
            if (obj.IsTurret() && (name.find("Turret_") == std::string::npos && charName.find("Turret") == std::string::npos)) {
                // Skip base spawn/nexus turrets if they clutter
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
        ImGui::Text("Active Objects Near Cursor (On Screen):");

        std::vector<SDK::GameObject> activeObjects;
        const Vec3 cursorPos = SDK::Game::CursorPos();
        const float rangeSqr = static_cast<float>(maxRange_ * maxRange_);

        for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
            if (!obj.IsValid()) continue;

            const Vec3 pos = obj.Position();
            if (pos.DistanceSqr(cursorPos) >= rangeSqr) continue;

            std::string name = GetObjectName(obj);
            std::string charName = GetObjectCharacterName(obj);

            if (name.empty() && charName.empty()) continue;
            if (name == "missile" || name.find("MoveTo") != std::string::npos ||
                charName.find("Grass") != std::string::npos || charName.find("FX") != std::string::npos ||
                charName.find("LevelProp") != std::string::npos || charName.find("emitter") != std::string::npos) {
                continue;
            }

            activeObjects.push_back(obj);
        }

        if (activeObjects.empty()) {
            ImGui::Text("No objects near cursor.");
            return;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Hotkey Hint: Hover an object and press key 'P' to copy its details.");

        if (ImGui::BeginTable("OnScreenObjectsTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("CharName");
            ImGui::TableSetupColumn("NetId");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Dist to Mouse");
            ImGui::TableSetupColumn("Age (s)");
            ImGui::TableSetupColumn("Action");
            ImGui::TableHeadersRow();

            const int now = SDK::Variables::TickCount();
            for (const auto& obj : activeObjects) {
                std::string name = GetObjectName(obj);
                std::string charName = GetObjectCharacterName(obj);
                std::string typeStr = ObjectTypeToString(obj.Type());
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
                ImGui::Text("%.1f", dist);
                ImGui::TableNextColumn();
                ImGui::Text("%.1fs", age);
                ImGui::TableNextColumn();

                char btnId[64];
                std::snprintf(btnId, sizeof(btnId), "Log##%u", netId);
                if (ImGui::Button(btnId)) {
                    SDK::Orbwalker::DebugPrint("[Dev] Name: %s | CharName: %s | NetId: %u | Age: %.1fs",
                                               name.c_str(), charName.c_str(), netId, age);
                }
            }
            ImGui::EndTable();
        }
    }

private:
    bool enabled_ = true;
    int maxRange_ = 400;
    std::unordered_map<std::uint32_t, int> trackedObjectTicks_;
    bool pKeyPressedLast_ = false;

    static void OnProcessSpellCast(const SDK::Events::ProcessSpellEventArgs& args) {
        const auto player = SDK::ObjectManager::Player();
        if (player.IsValid() && args.CasterNetworkId == player.NetworkId()) {
            char debugMsg[256];
            std::snprintf(debugMsg, sizeof(debugMsg), "Detected Spell Name: %s Issued By: %s",
                          args.SpellName[0] ? args.SpellName : args.ScriptName,
                          player.CharacterName().c_str());
            SDK::Orbwalker::DebugPrint("%s", debugMsg);
        }
    }

    static std::string GetObjectName(const SDK::GameObject& object) {
        if (!object.IsValid()) {
            return {};
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
        std::string name = object.CharacterName();
        if (!name.empty()) {
            return name;
        }
        char nameBuf[96] = {};
        if (::Core::Objects::ReadCharacterName(object.Address(), nameBuf, sizeof(nameBuf)) && nameBuf[0]) {
            return nameBuf;
        }
        return {};
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
};

inline DeveloperToolsPlugin* s_instance = nullptr;

} // namespace Plugins
