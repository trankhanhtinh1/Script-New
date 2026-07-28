#pragma once
#include "IDeveloperTab.h"
#include "../DeveloperToolsPlugin.h"

namespace Plugins::DevTools {

class ObjectDetectorTab final : public IDeveloperTab {
private:
    mutable std::vector<SDK::GameObject> scanCache_;
    mutable std::vector<SDK::GameObject> activeObjectsCache_;

public:
    using IDeveloperTab::IDeveloperTab;

    const char* GetTabName() const override { return "Detect Object"; }

    void OnRender() override {
        if (!SDK::Drawing::IsEnabled()) {
            return;
        }

        const Vec3 cursorPos = SDK::Game::CursorPos();
        const float rangeSqr = static_cast<float>(plugin_->maxRange_ * plugin_->maxRange_);
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

            const std::string& name = plugin_->GetObjectName(obj);
            const std::string& charName = plugin_->GetObjectCharacterName(obj);

            if (plugin_->IsClutter(obj, name, charName)) {
                continue;
            }

            // Track age
            const std::uint32_t netId = static_cast<std::uint32_t>(obj.NetworkId());
            float age = 0.0f;
            auto it = plugin_->trackedObjectTicks_.find(netId);
            if (it == plugin_->trackedObjectTicks_.end()) {
                plugin_->trackedObjectTicks_[netId] = now;
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
            const char* typeStr = plugin_->ObjectTypeToString(obj.Type());
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
                plugin_->GetMissileSpeedAndRange(obj, speed, mRange);

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

    void OnDeleteObject(const SDK::Events::ObjectEventArgs& args) override {
        if (args.Sender.NetworkId != 0) {
            plugin_->trackedObjectTicks_.erase(static_cast<std::uint32_t>(args.Sender.NetworkId));
        }
    }

    void OnDrawTab() override {
        const Vec3 cursorPos = SDK::Game::CursorPos();

        ImGui::Text("Scan Source Provider:");
        if (ImGui::RadioButton("SDK::ObjectManager (Raw RAM)", &plugin_->scanProviderIndex_, 0)) {
            if (plugin_->menuProvider_) plugin_->menuProvider_->SetValue(plugin_->scanProviderIndex_);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("SDK::GameObjects Facade", &plugin_->scanProviderIndex_, 1)) {
            if (plugin_->menuProvider_) plugin_->menuProvider_->SetValue(plugin_->scanProviderIndex_);
        }

        bool anySpecificListSelected = false;
        if (plugin_->scanProviderIndex_ == 1) {
            for (const auto& opt : plugin_->listOptions_) {
                if (opt.Enabled) {
                    anySpecificListSelected = true;
                    break;
                }
            }
        }

        if (plugin_->scanProviderIndex_ == 1) {
            ImGui::Separator();
            if (ImGui::CollapsingHeader("GameObjects Specific Lists")) {
                ImGui::Columns(2, "SpecificListsColumns", true);
                for (auto& opt : plugin_->listOptions_) {
                    if (ImGui::Checkbox(opt.DisplayName, &opt.Enabled)) {
                        if (opt.MenuControl) opt.MenuControl->SetValue(opt.Enabled);
                    }
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }
        }

        if (plugin_->scanProviderIndex_ == 0 || !anySpecificListSelected) {
            ImGui::Separator();
            ImGui::Text("Category Filters:");
            if (ImGui::Checkbox("Scan All Raw GameObjects (Scan Everything)", &plugin_->scanRawGameObjects_)) {
                if (plugin_->menuScanAll_) plugin_->menuScanAll_->SetValue(plugin_->scanRawGameObjects_);
            }
            if (!plugin_->scanRawGameObjects_) {
                if (ImGui::Checkbox("Heroes (AIHeroClient)", &plugin_->scanHeroes_)) {
                    if (plugin_->menuScanHeroes_) plugin_->menuScanHeroes_->SetValue(plugin_->scanHeroes_);
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Minions & Pets (AIMinionClient)", &plugin_->scanMinions_)) {
                    if (plugin_->menuScanMinions_) plugin_->menuScanMinions_->SetValue(plugin_->scanMinions_);
                }
                if (ImGui::Checkbox("Turrets (AITurretClient)", &plugin_->scanTurrets_)) {
                    if (plugin_->menuScanTurrets_) plugin_->menuScanTurrets_->SetValue(plugin_->scanTurrets_);
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Missiles (MissileClient)", &plugin_->scanMissiles_)) {
                    if (plugin_->menuScanMissiles_) plugin_->menuScanMissiles_->SetValue(plugin_->scanMissiles_);
                }
            }
        }
        if (ImGui::Checkbox("Filter Clutter (FX, Grass, Emitters, MoveTo)", &plugin_->filterClutter_)) {
            if (plugin_->menuFilterClutter_) plugin_->menuFilterClutter_->SetValue(plugin_->filterClutter_);
        }

        ImGui::Separator();
        ImGui::Text("Active Objects Near Cursor (On Screen):");

        PopulateScanCache();

        activeObjectsCache_.clear();
        const float rangeSqr = static_cast<float>(plugin_->maxRange_ * plugin_->maxRange_);

        for (const auto& obj : scanCache_) {
            if (!obj.IsValid()) continue;

            const Vec3 pos = obj.Position();
            if (pos.DistanceSqr(cursorPos) >= rangeSqr) continue;

            const std::string& name = plugin_->GetObjectName(obj);
            const std::string& charName = plugin_->GetObjectCharacterName(obj);

            if (plugin_->IsClutter(obj, name, charName)) continue;

            activeObjectsCache_.push_back(obj);
        }

        if (activeObjectsCache_.empty()) {
            ImGui::Text("No objects near cursor.");
            return;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Hotkey Hint: Press key 'P' to copy ALL objects in table below to Clipboard.");

        if (ImGui::Button("Copy Entire Table to Clipboard (Key 'P')")) {
            OnCopyHotkey();
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
                const std::string& name = plugin_->GetObjectName(obj);
                const std::string& charName = plugin_->GetObjectCharacterName(obj);
                const char* typeStr = plugin_->ObjectTypeToString(obj.Type());
                const char* teamStr = plugin_->TeamToString(obj);
                char statusBuf[256];
                plugin_->GetStatusString(obj, statusBuf, sizeof(statusBuf));
                float dist = obj.Position().Distance(cursorPos);

                std::uint32_t netId = static_cast<std::uint32_t>(obj.NetworkId());
                float age = 0.0f;
                auto it = plugin_->trackedObjectTicks_.find(netId);
                if (it != plugin_->trackedObjectTicks_.end()) {
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
                ImGui::SameLine();
                char eventBtnId[64];
                std::snprintf(eventBtnId, sizeof(eventBtnId), "Track Events##%u", netId);
                bool isTracking = plugin_->openEventLogWindows_.find(netId) != plugin_->openEventLogWindows_.end();
                if (ImGui::Checkbox(eventBtnId, &isTracking)) {
                    if (isTracking) {
                        plugin_->openEventLogWindows_.insert(netId);
                        plugin_->objectEventLogs_[netId] = std::vector<DevTools::EventLogEntry>();
                    } else {
                        plugin_->openEventLogWindows_.erase(netId);
                        plugin_->objectEventLogs_.erase(netId);
                    }
                }
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "Object Snapshots Memory Manager (Key 'M' to snapshot hovered object)");

        if (ImGui::Button("Prune Invalid (Keep Live Only)")) {
            auto& snaps = plugin_->snapshots_;
            snaps.erase(std::remove_if(snaps.begin(), snaps.end(), [](const DevTools::ObjectSnapshot& s) {
                return !s.isUnderlyingValid;
            }), snaps.end());
            NightSharpDebug::Logf("[Dev] Pruned invalid snapshots.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All Snapshots")) {
            plugin_->snapshots_.clear();
            plugin_->openInspectWindows_.clear();
            NightSharpDebug::Logf("[Dev] Cleared all snapshots.");
        }

        static char snapSearch[64] = {};
        ImGui::InputText("Search Name (Case-insensitive)", snapSearch, sizeof(snapSearch));

        static int snapTypeFilter = 0; // 0=All, 1=Heroes, 2=Minions, 3=Turrets, 4=Missiles, 5=Others
        const char* typeFilters[] = { "All Types", "Heroes Only", "Minions Only", "Turrets Only", "Missiles Only", "Others Only" };
        ImGui::Combo("Filter Type", &snapTypeFilter, typeFilters, IM_ARRAYSIZE(typeFilters));

        if (plugin_->snapshots_.empty()) {
            ImGui::Text("No object snapshots in memory. Hover an object and press key 'M' to take a snapshot!");
        } else {
            if (ImGui::BeginTable("SnapshotsManagerTable", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("CharName");
                ImGui::TableSetupColumn("NetID");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Health/Mana");
                ImGui::TableSetupColumn("Status");
                ImGui::TableSetupColumn("Tag/Note");
                ImGui::TableSetupColumn("Window");
                ImGui::TableSetupColumn("Actions");
                ImGui::TableHeadersRow();

                auto& snaps = plugin_->snapshots_;
                for (std::size_t i = 0; i < snaps.size(); ) {
                    auto& snap = snaps[i];
                    
                    if (snapSearch[0]) {
                        std::string searchStr = snapSearch;
                        std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                        
                        std::string nameLower = snap.name;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        std::string charLower = snap.characterName;
                        std::transform(charLower.begin(), charLower.end(), charLower.begin(), ::tolower);
                        
                        if (nameLower.find(searchStr) == std::string::npos && charLower.find(searchStr) == std::string::npos) {
                            i++;
                            continue;
                        }
                    }

                    if (snapTypeFilter > 0) {
                        bool match = false;
                        if (snapTypeFilter == 1 && snap.type == ::Core::Objects::ObjectType::AIHeroClient) match = true;
                        else if (snapTypeFilter == 2 && snap.type == ::Core::Objects::ObjectType::AIMinionClient) match = true;
                        else if (snapTypeFilter == 3 && snap.type == ::Core::Objects::ObjectType::AITurretClient) match = true;
                        else if (snapTypeFilter == 4 && snap.type == ::Core::Objects::ObjectType::MissileClient) match = true;
                        else if (snapTypeFilter == 5 && 
                                 snap.type != ::Core::Objects::ObjectType::AIHeroClient && 
                                 snap.type != ::Core::Objects::ObjectType::AIMinionClient && 
                                 snap.type != ::Core::Objects::ObjectType::AITurretClient && 
                                 snap.type != ::Core::Objects::ObjectType::MissileClient) match = true;
                        
                        if (!match) {
                            i++;
                            continue;
                        }
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(snap.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(snap.characterName.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", snap.networkId);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(plugin_->ObjectTypeToString(snap.type));
                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f / %.1f", snap.health, snap.mana);
                    ImGui::TableNextColumn();
                    if (snap.isUnderlyingValid) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Live");
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Gone");
                    }
                    
                    ImGui::TableNextColumn();
                    char noteBuf[128];
                    strncpy_s(noteBuf, snap.note.c_str(), _TRUNCATE);
                    char inputId[64];
                    std::snprintf(inputId, sizeof(inputId), "##Note%u", snap.networkId);
                    ImGui::SetNextItemWidth(120.0f);
                    if (ImGui::InputText(inputId, noteBuf, sizeof(noteBuf))) {
                        snap.note = noteBuf;
                    }

                    ImGui::TableNextColumn();
                    bool isOpen = plugin_->openInspectWindows_.find(snap.networkId) != plugin_->openInspectWindows_.end();
                    char checkId[64];
                    std::snprintf(checkId, sizeof(checkId), "Inspect##Win%u", snap.networkId);
                    if (ImGui::Checkbox(checkId, &isOpen)) {
                        if (isOpen) {
                            plugin_->openInspectWindows_.insert(snap.networkId);
                        } else {
                            plugin_->openInspectWindows_.erase(snap.networkId);
                        }
                    }
                    if (snap.isUnderlyingValid) {
                        ImGui::SameLine();
                        bool isTracking = plugin_->openEventLogWindows_.find(snap.networkId) != plugin_->openEventLogWindows_.end();
                        char eventCheckId[64];
                        std::snprintf(eventCheckId, sizeof(eventCheckId), "Events##Win%u", snap.networkId);
                        if (ImGui::Checkbox(eventCheckId, &isTracking)) {
                            if (isTracking) {
                                plugin_->openEventLogWindows_.insert(snap.networkId);
                                plugin_->objectEventLogs_[snap.networkId] = std::vector<DevTools::EventLogEntry>();
                            } else {
                                plugin_->openEventLogWindows_.erase(snap.networkId);
                                plugin_->objectEventLogs_.erase(snap.networkId);
                            }
                        }
                    }

                    ImGui::TableNextColumn();
                    char delBtnId[64];
                    std::snprintf(delBtnId, sizeof(delBtnId), "Delete##%u", snap.networkId);
                    if (ImGui::Button(delBtnId)) {
                        plugin_->openInspectWindows_.erase(snap.networkId);
                        snaps.erase(snaps.begin() + i);
                        continue;
                    }

                    i++;
                }
                ImGui::EndTable();
            }
        }
    }

    void OnCopyHotkey() override {
        PopulateScanCache();

        std::string copyText = "=== DEVELOPER TOOLS OBJECT TABLE ===\n";
        int count = 0;
        const int now = SDK::Variables::TickCount();
        const Vec3 cursorPos = SDK::Game::CursorPos();
        const float rangeSqr = static_cast<float>(plugin_->maxRange_ * plugin_->maxRange_);

        for (const auto& obj : scanCache_) {
            if (!obj.IsValid()) continue;

            const Vec3 pos = obj.Position();
            if (pos.DistanceSqr(cursorPos) >= rangeSqr) continue;

            const std::string& name = plugin_->GetObjectName(obj);
            const std::string& charName = plugin_->GetObjectCharacterName(obj);

            if (plugin_->IsClutter(obj, name, charName)) continue;

            const char* typeStr = plugin_->ObjectTypeToString(obj.Type());
            std::uint32_t netId = static_cast<std::uint32_t>(obj.NetworkId());
            const char* teamStr = plugin_->TeamToString(obj);
            char statusBuf[256];
            plugin_->GetStatusString(obj, statusBuf, sizeof(statusBuf));

            float age = 0.0f;
            auto it = plugin_->trackedObjectTicks_.find(netId);
            if (it != plugin_->trackedObjectTicks_.end()) {
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
            NightSharpDebug::Logf("[Dev] Hotkey copied ALL %d objects in table to Clipboard!", count);
        }
    }

private:
    void PopulateScanCache() const {
        scanCache_.clear();
        if (plugin_->scanProviderIndex_ == 0) {
            // Using SDK::ObjectManager::Get
            if (plugin_->scanRawGameObjects_) {
                for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
                    if (obj.IsValid()) scanCache_.push_back(obj);
                }
            } else {
                if (plugin_->scanHeroes_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
                        if (obj.IsValid()) scanCache_.push_back(obj);
                    }
                }
                if (plugin_->scanMinions_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::AIMinionClient>()) {
                        if (obj.IsValid()) scanCache_.push_back(obj);
                    }
                }
                if (plugin_->scanTurrets_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::AITurretClient>()) {
                        if (obj.IsValid()) scanCache_.push_back(obj);
                    }
                }
                if (plugin_->scanMissiles_) {
                    for (const auto& obj : SDK::ObjectManager::Get<SDK::MissileClient>()) {
                        if (obj.IsValid()) scanCache_.push_back(obj);
                    }
                }
            }
        } else {
            // Using SDK::GameObjects Facade
            std::lock_guard<std::recursive_mutex> lk(SDK::GameObjects::detail::g_mutex);

            bool anySpecificListSelected = false;
            for (const auto& opt : plugin_->listOptions_) {
                if (opt.Enabled) {
                    anySpecificListSelected = true;
                    break;
                }
            }

            if (anySpecificListSelected) {
                for (const auto& opt : plugin_->listOptions_) {
                    if (opt.Enabled) {
                        switch (opt.Type) {
                        case GameObjectListType::AllGameObjects:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::GameObjectsList);
                            break;
                        case GameObjectListType::AttackableUnits:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AttackableUnitsList);
                            break;
                        case GameObjectListType::Ally:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyList);
                            break;
                        case GameObjectListType::Enemy:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyList);
                            break;
                        case GameObjectListType::Heroes:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::HeroesList);
                            break;
                        case GameObjectListType::AllyHeroes:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyHeroesList);
                            break;
                        case GameObjectListType::EnemyHeroes:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyHeroesList);
                            break;
                        case GameObjectListType::Minions:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::MinionsList);
                            break;
                        case GameObjectListType::AllyMinions:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyMinionsList);
                            break;
                        case GameObjectListType::EnemyMinions:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyMinionsList);
                            break;
                        case GameObjectListType::AllyLaneMinions:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyLaneMinionsList);
                            break;
                        case GameObjectListType::EnemyLaneMinions:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyLaneMinionsList);
                            break;
                        case GameObjectListType::AllySpecialMinions:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllySpecialMinionsList);
                            break;
                        case GameObjectListType::EnemySpecialMinions:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemySpecialMinionsList);
                            break;
                        case GameObjectListType::AllyIgnoredMinions:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyIgnoredMinionsList);
                            break;
                        case GameObjectListType::EnemyIgnoredMinions:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyIgnoredMinionsList);
                            break;
                        case GameObjectListType::Wards:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::WardsList);
                            break;
                        case GameObjectListType::AllyWards:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyWardsList);
                            break;
                        case GameObjectListType::EnemyWards:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyWardsList);
                            break;
                        case GameObjectListType::Jungle:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::JungleList);
                            break;
                        case GameObjectListType::JungleSmall:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::JungleSmallList);
                            break;
                        case GameObjectListType::JungleLarge:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::JungleLargeList);
                            break;
                        case GameObjectListType::JungleLegendary:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::JungleLegendaryList);
                            break;
                        case GameObjectListType::Plants:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::PlantsList);
                            break;
                        case GameObjectListType::Clones:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::ClonesList);
                            break;
                        case GameObjectListType::AllyClones:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyClonesList);
                            break;
                        case GameObjectListType::EnemyClones:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyClonesList);
                            break;
                        case GameObjectListType::Pets:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::PetsList);
                            break;
                        case GameObjectListType::AllyPets:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyPetsList);
                            break;
                        case GameObjectListType::EnemyPets:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyPetsList);
                            break;
                        case GameObjectListType::Turrets:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::TurretsList);
                            break;
                        case GameObjectListType::AllyTurrets:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyTurretsList);
                            break;
                        case GameObjectListType::EnemyTurrets:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyTurretsList);
                            break;
                        case GameObjectListType::Inhibitors:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::InhibitorsList);
                            break;
                        case GameObjectListType::AllyInhibitors:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyInhibitorsList);
                            break;
                        case GameObjectListType::EnemyInhibitors:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyInhibitorsList);
                            break;
                        case GameObjectListType::Nexuses:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::NexusList);
                            break;
                        case GameObjectListType::AllyNexus:
                            plugin_->AddUniqueObject(scanCache_, SDK::GameObjects::detail::AllyNexusObject);
                            break;
                        case GameObjectListType::EnemyNexus:
                            plugin_->AddUniqueObject(scanCache_, SDK::GameObjects::detail::EnemyNexusObject);
                            break;
                        case GameObjectListType::Shops:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::ShopsList);
                            break;
                        case GameObjectListType::AllyShops:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllyShopsList);
                            break;
                        case GameObjectListType::EnemyShops:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemyShopsList);
                            break;
                        case GameObjectListType::SpawnPoints:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::SpawnPointsList);
                            break;
                        case GameObjectListType::AllySpawnPoints:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::AllySpawnPointsList);
                            break;
                        case GameObjectListType::EnemySpawnPoints:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::EnemySpawnPointsList);
                            break;
                        case GameObjectListType::ParticleEmitters:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::ParticleEmittersList);
                            break;
                        case GameObjectListType::Missiles:
                            plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::MissilesList);
                            break;
                        case GameObjectListType::Player:
                            plugin_->AddUniqueObject(scanCache_, SDK::GameObjects::detail::PlayerObject);
                            break;
                        default:
                            break;
                        }
                    }
                }
            } else {
                if (plugin_->scanRawGameObjects_) {
                    plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::GameObjectsList);
                } else {
                    if (plugin_->scanHeroes_) {
                        plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::HeroesList);
                    }
                    if (plugin_->scanMinions_) {
                        plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::MinionsList);
                    }
                    if (plugin_->scanTurrets_) {
                        plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::TurretsList);
                    }
                    if (plugin_->scanMissiles_) {
                        plugin_->AddUniqueObjectsFromSource(scanCache_, SDK::GameObjects::detail::MissilesList);
                    }
                }
            }
        }
    }
};

} // namespace Plugins::DevTools
