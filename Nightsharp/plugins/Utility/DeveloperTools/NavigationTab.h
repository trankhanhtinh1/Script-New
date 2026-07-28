#pragma once
#include "IDeveloperTab.h"
#include "../DeveloperToolsPlugin.h"
#include <mutex>
#include <vector>
#include <string>

namespace Plugins::DevTools {

class NavigationTab final : public IDeveloperTab {
private:
    struct NavigationObjectInfo {
        uintptr_t address = 0;
        std::uint32_t color = 0xFFFFFFFF;
        Vec3 position;
        float attackRange = 0.0f;
        std::vector<Vec3> path;
        bool hasPath = false;
        float moveSpeed = 0.0f;
    };

    struct FocusedObjectNavInfo {
        bool isValid = false;
        std::string characterName;
        bool hasPath = false;
        float moveSpeed = 0.0f;
        std::vector<Vec3> path;
        Vec3 position;
    };

    bool drawPaths_ = true;
    bool drawRange_ = false;
    bool drawPrediction_ = false;
    int predictionMs_ = 500;
    bool onlyFocused_ = false;

    mutable std::mutex navMutex_;
    std::vector<NavigationObjectInfo> cachedNavObjects_;
    FocusedObjectNavInfo cachedFocusedInfo_;
    int lastUpdateTick_ = 0;

public:
    using IDeveloperTab::IDeveloperTab;

    const char* GetTabName() const override { return "Navigation"; }

    void OnDrawTab() override {
        ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "Navigation & Prediction Visualizer");
        ImGui::Separator();

        ImGui::Checkbox("Draw Paths in 3D Overlay", &drawPaths_);
        ImGui::Checkbox("Draw Attack Range Circles", &drawRange_);
        ImGui::Checkbox("Draw Predicted Positions", &drawPrediction_);
        
        if (drawPrediction_) {
            ImGui::SliderInt("Prediction Delay (ms)", &predictionMs_, 50, 2000);
        }

        ImGui::Checkbox("Only Draw for Hovered/Focused Object", &onlyFocused_);

        ImGui::Separator();
        
        FocusedObjectNavInfo focusedInfo;
        {
            std::lock_guard<std::mutex> lk(navMutex_);
            focusedInfo = cachedFocusedInfo_;
        }

        if (focusedInfo.isValid) {
            ImGui::Text("Focused Object: %s", focusedInfo.characterName.c_str());
            ImGui::Text("Has Path: %s", focusedInfo.hasPath ? "Yes" : "No");
            ImGui::Text("Move Speed: %.1f", focusedInfo.moveSpeed);
            
            if (!focusedInfo.path.empty()) {
                ImGui::Text("Path Waypoints Count: %d", static_cast<int>(focusedInfo.path.size()));
                
                if (ImGui::BeginTable("WaypointsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Idx");
                    ImGui::TableSetupColumn("X");
                    ImGui::TableSetupColumn("Y");
                    ImGui::TableSetupColumn("Z");
                    ImGui::TableSetupColumn("Dist from Unit");
                    ImGui::TableHeadersRow();
                    
                    const Vec3 unitPos = focusedInfo.position;
                    for (int i = 0; i < static_cast<int>(focusedInfo.path.size()); ++i) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", i);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", focusedInfo.path[i].x);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", focusedInfo.path[i].y);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", focusedInfo.path[i].z);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", unitPos.Distance(focusedInfo.path[i]));
                    }
                    ImGui::EndTable();
                }
            } else {
                ImGui::Text("No active movement path.");
            }
        } else {
            ImGui::Text("No focused object selected (hover cursor near a hero or minion).");
        }
    }

    void OnUpdate() override {
        if (!plugin_->enabled_) {
            std::lock_guard<std::mutex> lk(navMutex_);
            cachedNavObjects_.clear();
            cachedFocusedInfo_ = {};
            return;
        }

        const int now = SDK::Variables::TickCount();
        if (now - lastUpdateTick_ < 100) {
            return;
        }
        lastUpdateTick_ = now;

        std::vector<NavigationObjectInfo> list;
        FocusedObjectNavInfo focusedInfo;

        const auto localPlayer = SDK::ObjectManager::Player();
        const auto focusedObj = plugin_->GetFocusedObject();

        for (const auto& obj : SDK::ObjectManager::Get<SDK::GameObject>()) {
            if (!obj.IsValid() || (!obj.IsHero() && !obj.IsMinion())) {
                continue;
            }

            if (onlyFocused_ && focusedObj.IsValid() && obj.Address() != focusedObj.Address()) {
                continue;
            }

            const auto aiObj = SDK::AIBaseClient(obj.Handle());
            if (!aiObj.IsValid()) {
                continue;
            }

            NavigationObjectInfo info;
            info.address = aiObj.Address();
            info.position = aiObj.Position();
            info.attackRange = aiObj.AttackRange();
            info.path = aiObj.Path();
            info.hasPath = aiObj.HasPath();
            info.moveSpeed = aiObj.MoveSpeed();

            info.color = 0xFFFFFFFF; // White
            if (aiObj.IsHero()) {
                if (localPlayer.IsValid() && aiObj.Address() == localPlayer.Address()) {
                    info.color = 0xFF00FFFF; // Cyan (Local Player)
                } else if (aiObj.Team() == SDK::GameObjectTeam::Order) {
                    info.color = 0xFF00FF00; // Green (Allies)
                } else if (aiObj.Team() == SDK::GameObjectTeam::Chaos) {
                    info.color = 0xFFFF0000; // Red (Enemies)
                }
            } else {
                info.color = 0xFF888888; // Grey (Minions)
            }

            list.push_back(std::move(info));
        }

        // Populate focused object nav info for GUI
        if (focusedObj.IsValid() && (focusedObj.IsHero() || focusedObj.IsMinion())) {
            const auto player = SDK::AIBaseClient(focusedObj.Handle());
            if (player.IsValid()) {
                focusedInfo.isValid = true;
                focusedInfo.characterName = player.CharacterName();
                focusedInfo.hasPath = player.HasPath();
                focusedInfo.moveSpeed = player.MoveSpeed();
                focusedInfo.path = player.Path();
                focusedInfo.position = player.Position();
            }
        }

        std::lock_guard<std::mutex> lk(navMutex_);
        cachedNavObjects_ = std::move(list);
        cachedFocusedInfo_ = std::move(focusedInfo);
    }

    void OnRender() override {
        if (!SDK::Drawing::IsEnabled()) {
            return;
        }

        std::vector<NavigationObjectInfo> renderList;
        {
            std::lock_guard<std::mutex> lk(navMutex_);
            renderList = cachedNavObjects_;
        }

        const auto focusedObj = plugin_->GetFocusedObject();

        for (const auto& unit : renderList) {
            if (onlyFocused_ && focusedObj.IsValid() && unit.address != focusedObj.Address()) {
                continue;
            }

            if (drawRange_) {
                SDK::Drawing::DrawCircle(unit.position, unit.attackRange, unit.color, 1.5f, 64, false);
            }

            if (drawPaths_ && !unit.path.empty()) {
                Vec3 current = unit.position;
                for (const auto& point : unit.path) {
                    SDK::Drawing::DrawLine(current, point, unit.color, 1.5f, false);
                    SDK::Drawing::DrawCircle(point, 10.0f, unit.color, 1.0f, 64, false);
                    current = point;
                }
            }

            if (drawPrediction_ && unit.hasPath && unit.moveSpeed > 0.01f) {
                float distToMove = unit.moveSpeed * (static_cast<float>(predictionMs_) / 1000.0f);
                Vec3 predictedPos = unit.position;
                
                if (!unit.path.empty()) {
                    Vec3 current = unit.position;
                    for (const auto& wp : unit.path) {
                        float dist = current.Distance(wp);
                        if (distToMove <= dist) {
                            predictedPos = current.Extend(wp, distToMove);
                            distToMove = 0.0f;
                            break;
                        } else {
                            distToMove -= dist;
                            current = wp;
                        }
                    }
                    if (distToMove > 0.0f) {
                        predictedPos = current;
                    }
                }

                SDK::Drawing::DrawCircle(predictedPos, 20.0f, 0xFFFFD700, 2.0f, 64, false); // Gold
                SDK::Drawing::DrawLine(unit.position, predictedPos, 0xFFFFD700, 2.0f, false);
            }
        }
    }

    void OnCopyHotkey() override {
        const auto obj = plugin_->GetFocusedObject();
        if (!obj.IsValid() || (!obj.IsHero() && !obj.IsMinion())) {
            return;
        }

        const auto player = SDK::AIBaseClient(obj.Handle());
        auto path = player.Path();
        if (path.empty()) return;

        std::string dump = "=== WAYPOINTS FOR " + player.CharacterName() + " ===\n";
        for (int i = 0; i < static_cast<int>(path.size()); ++i) {
            char line[256];
            std::snprintf(line, sizeof(line), "WP[%d] - (%.2f, %.2f, %.2f)\n", i, path[i].x, path[i].y, path[i].z);
            dump += line;
        }

        ImGui::SetClipboardText(dump.c_str());
        NightSharpDebug::Logf("[Dev] Copied waypoints of %s to Clipboard!", player.CharacterName().c_str());
    }
};

} // namespace Plugins::DevTools
