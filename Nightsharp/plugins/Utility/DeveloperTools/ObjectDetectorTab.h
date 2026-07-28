#pragma once
#include "IDeveloperTab.h"
#include "../DeveloperToolsPlugin.h"
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>

namespace Plugins::DevTools {

class ObjectDetectorTab final : public IDeveloperTab {
private:
    struct DetectedObjectInfo {
        Vec3 position;
        std::uint32_t networkId = 0;
        std::vector<std::string> lines;
    };

    struct DetectorTabActiveObject {
        std::string name;
        std::string characterName;
        std::uint32_t networkId = 0;
        uintptr_t address = 0;
        std::string typeStr;
        std::string teamStr;
        std::string statusStr;
        float distance = 0.0f;
        float age = 0.0f;
        Vec3 position;
    };

    mutable std::vector<SDK::GameObject> scanCache_;
    mutable std::vector<SDK::GameObject> activeObjectsCache_;
    mutable std::mutex detectorMutex_;
    std::vector<DetectedObjectInfo> detectedObjects_;
    std::vector<DetectorTabActiveObject> guiActiveObjects_;
    int lastUpdateTick_ = 0;

public:
    using IDeveloperTab::IDeveloperTab;

    const char* GetTabName() const override { return "Detect Object"; }

    void OnUpdate() override {
        if (!plugin_->enabled_) {
            std::lock_guard<std::mutex> lk(detectorMutex_);
            detectedObjects_.clear();
            guiActiveObjects_.clear();
            return;
        }

        const int now = SDK::Variables::TickCount();
        if (now - lastUpdateTick_ < 100) {
            return;
        }
        lastUpdateTick_ = now;

        PopulateScanCache();

        std::vector<DetectedObjectInfo> detectedList;
        std::vector<DetectorTabActiveObject> guiActiveList;

        const Vec3 cursorPos = SDK::Game::CursorPos();
        const float rangeSqr = static_cast<float>(plugin_->maxRange_ * plugin_->maxRange_);

        for (const auto& obj : scanCache_) {
            if (!obj.IsValid()) {
                continue;
            }

            const Vec3 pos = obj.Position();
            const float distSqr = pos.DistanceSqr(cursorPos);
            if (distSqr >= rangeSqr) {
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

            const std::string& displayName = (obj.IsHero() || obj.IsMinion() || obj.IsTurret()) ? charName : name;
            const std::string& fallbackName = displayName.empty() ? (name.empty() ? charName : name) : displayName;

            // --- 1. Populate DetectedObjectInfo (for 3D text drawing) ---
            {
                DetectedObjectInfo info;
                info.position = pos;
                info.networkId = netId;

                // 1. Name / CharName
                info.lines.push_back(fallbackName);

                // 2. Object Type
                const char* typeStr = plugin_->ObjectTypeToString(obj.Type());
                info.lines.push_back(typeStr);

                // 3. NetworkID
                char netIdTxt[64];
                std::snprintf(netIdTxt, sizeof(netIdTxt), "NetworkID: %u", netId);
                info.lines.push_back(netIdTxt);

                // 4. Position
                char posTxt[128];
                std::snprintf(posTxt, sizeof(posTxt), "Position: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
                info.lines.push_back(posTxt);

                // 5. Age
                char ageTxt[64];
                std::snprintf(ageTxt, sizeof(ageTxt), "Age: %.1fs", age);
                info.lines.push_back(ageTxt);

                // 6. AIBaseClient Info (Health)
                if (obj.IsHero() || obj.IsMinion() || obj.IsTurret()) {
                    SDK::AIBaseClient aiObj(obj.Handle());
                    if (aiObj.IsValid()) {
                        char hpTxt[128];
                        std::snprintf(hpTxt, sizeof(hpTxt), "Health: %.1f/%.1f (%.1f%%)",
                                      aiObj.Health(), aiObj.MaxHealth(), aiObj.HealthPercent());
                        info.lines.push_back(hpTxt);
                    }
                }

                // 7. AIHeroClient Info (Spells & Buffs)
                if (obj.IsHero()) {
                    SDK::AIHeroClient hero(obj.Handle());
                    if (hero.IsValid()) {
                        info.lines.push_back("Spells:");

                        char qTxt[128];
                        std::snprintf(qTxt, sizeof(qTxt), "(Q): %s", hero.GetSpell(SDK::SpellSlot::Q).Name().c_str());
                        info.lines.push_back(qTxt);

                        char wTxt[128];
                        std::snprintf(wTxt, sizeof(wTxt), "(W): %s", hero.GetSpell(SDK::SpellSlot::W).Name().c_str());
                        info.lines.push_back(wTxt);

                        char eTxt[128];
                        std::snprintf(eTxt, sizeof(eTxt), "(E): %s", hero.GetSpell(SDK::SpellSlot::E).Name().c_str());
                        info.lines.push_back(eTxt);

                        char rTxt[128];
                        std::snprintf(rTxt, sizeof(rTxt), "(R): %s", hero.GetSpell(SDK::SpellSlot::R).Name().c_str());
                        info.lines.push_back(rTxt);

                        char dTxt[128];
                        std::snprintf(dTxt, sizeof(dTxt), "(D): %s", hero.GetSpell(SDK::SpellSlot::Summoner1).Name().c_str());
                        info.lines.push_back(dTxt);

                        char fTxt[128];
                        std::snprintf(fTxt, sizeof(fTxt), "(F): %s", hero.GetSpell(SDK::SpellSlot::Summoner2).Name().c_str());
                        info.lines.push_back(fTxt);

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
                                    info.lines.push_back("Buffs:");
                                    printedBuffHeader = true;
                                }
                                char buffTxt[128] = {};
                                std::snprintf(buffTxt, sizeof(buffTxt), "%dx %s", buff.GetStacks(), buffName);
                                info.lines.push_back(buffTxt);
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
                    info.lines.push_back(speedTxt);

                    char rangeTxt[128];
                    std::snprintf(rangeTxt, sizeof(rangeTxt), "Cast Range: %.1f", mRange);
                    info.lines.push_back(rangeTxt);
                }

                detectedList.push_back(std::move(info));
            }

            // --- 2. Populate DetectorTabActiveObject (for GUI Table) ---
            {
                DetectorTabActiveObject guiObj;
                guiObj.name = name;
                guiObj.characterName = charName;
                guiObj.networkId = netId;
                guiObj.address = obj.Address();
                guiObj.typeStr = plugin_->ObjectTypeToString(obj.Type());
                guiObj.teamStr = plugin_->TeamToString(obj);
                
                char statusBuf[256] = {};
                plugin_->GetStatusString(obj, statusBuf, sizeof(statusBuf));
                guiObj.statusStr = statusBuf;
                
                guiObj.distance = std::sqrt(distSqr);
                guiObj.age = age;
                guiObj.position = pos;

                guiActiveList.push_back(std::move(guiObj));
            }
        }

        std::lock_guard<std::mutex> lk(detectorMutex_);
        detectedObjects_ = std::move(detectedList);
        guiActiveObjects_ = std::move(guiActiveList);
    }

    void OnRender() override {
        if (!SDK::Drawing::IsEnabled()) {
            return;
        }

        std::vector<DetectedObjectInfo> renderList;
        {
            std::lock_guard<std::mutex> lk(detectorMutex_);
            renderList = detectedObjects_;
        }

        const Vec2 rendererSize = SDK::Drawing::GetRendererSize();

        for (const auto& obj : renderList) {
            Vec2 screen = {};
            if (!SDK::Drawing::WorldToScreen(obj.position, screen) || !screen.IsValid()) {
                continue;
            }

            if (screen.x < 0.0f || screen.y < 0.0f || screen.x > rendererSize.x || screen.y > rendererSize.y) {
                continue;
            }

            float currentY = screen.y;
            const float stepY = 15.0f;
            const std::uint32_t textColor = 0xFF00CED1u; // DarkTurquoise

            for (const auto& line : obj.lines) {
                SDK::Drawing::DrawText(Vec2(screen.x, currentY), line.c_str(), textColor, false, true);
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

        std::vector<DetectorTabActiveObject> activeObjects;
        {
            std::lock_guard<std::mutex> lk(detectorMutex_);
            activeObjects = guiActiveObjects_;
        }

        if (activeObjects.empty()) {
            ImGui::Text("No objects near cursor.");
        } else {
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

                for (const auto& obj : activeObjects) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(obj.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(obj.characterName.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", obj.networkId);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(obj.typeStr.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(obj.teamStr.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(obj.statusStr.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f", obj.distance);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.1fs", obj.age);
                    ImGui::TableNextColumn();

                    char btnId[64];
                    std::snprintf(btnId, sizeof(btnId), "Log##%u", obj.networkId);
                    if (ImGui::Button(btnId)) {
                        NightSharpDebug::Logf("[Dev] Name: %s | CharName: %s | NetId: %u | Team: %s | Status: %s | Age: %.1fs",
                                                   obj.name.c_str(), obj.characterName.c_str(), obj.networkId, obj.teamStr.c_str(), obj.statusStr.c_str(), obj.age);
                    }
                    ImGui::SameLine();
                    char eventBtnId[64];
                    std::snprintf(eventBtnId, sizeof(eventBtnId), "Track Events##%u", obj.networkId);
                    bool isTracking = plugin_->openEventLogWindows_.find(obj.networkId) != plugin_->openEventLogWindows_.end();
                    if (ImGui::Checkbox(eventBtnId, &isTracking)) {
                        if (isTracking) {
                            plugin_->openEventLogWindows_.insert(obj.networkId);
                            plugin_->objectEventLogs_[obj.networkId] = std::vector<DevTools::EventLogEntry>();
                        } else {
                            plugin_->openEventLogWindows_.erase(obj.networkId);
                            plugin_->objectEventLogs_.erase(obj.networkId);
                        }
                    }
                }
                ImGui::EndTable();
            }
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
        std::vector<DetectorTabActiveObject> copyList;
        {
            std::lock_guard<std::mutex> lk(detectorMutex_);
            copyList = guiActiveObjects_;
        }

        std::string copyText = "=== DEVELOPER TOOLS OBJECT TABLE ===\n";
        int count = 0;
        for (const auto& obj : copyList) {
            char lineBuf[512];
            std::snprintf(lineBuf, sizeof(lineBuf),
                          "[%d] Name: %s | CharName: %s | NetId: %u | Addr: 0x%llX | Type: %s | Team: %s | Status: %s | Age: %.1fs | Pos: (%.1f, %.1f, %.1f)\n",
                          ++count, obj.name.c_str(), obj.characterName.c_str(), obj.networkId,
                          static_cast<unsigned long long>(obj.address),
                          obj.typeStr.c_str(), obj.teamStr.c_str(), obj.statusStr.c_str(), obj.age,
                          obj.position.x, obj.position.y, obj.position.z);
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
