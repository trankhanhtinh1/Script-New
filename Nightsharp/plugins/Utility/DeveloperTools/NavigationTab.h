#pragma once
#include "IDeveloperTab.h"
#include "../DeveloperToolsPlugin.h"

namespace Plugins::DevTools {

class NavigationTab final : public IDeveloperTab {
private:
    bool drawPaths_ = true;
    bool drawRange_ = false;
    bool drawPrediction_ = false;
    int predictionMs_ = 500;
    bool onlyFocused_ = false;

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
        
        const auto obj = plugin_->GetFocusedObject();
        if (obj.IsValid() && (obj.IsHero() || obj.IsMinion())) {
            const auto player = SDK::AIBaseClient(obj.Handle());
            ImGui::Text("Focused Object: %s", player.CharacterName().c_str());
            ImGui::Text("Has Path: %s", player.HasPath() ? "Yes" : "No");
            ImGui::Text("Move Speed: %.1f", player.MoveSpeed());
            
            auto path = player.Path();
            if (!path.empty()) {
                ImGui::Text("Path Waypoints Count: %d", static_cast<int>(path.size()));
                
                if (ImGui::BeginTable("WaypointsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Idx");
                    ImGui::TableSetupColumn("X");
                    ImGui::TableSetupColumn("Y");
                    ImGui::TableSetupColumn("Z");
                    ImGui::TableSetupColumn("Dist from Unit");
                    ImGui::TableHeadersRow();
                    
                    const Vec3 unitPos = player.Position();
                    for (int i = 0; i < static_cast<int>(path.size()); ++i) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", i);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", path[i].x);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", path[i].y);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", path[i].z);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", unitPos.Distance(path[i]));
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

    void OnRender() override {
        if (!SDK::Drawing::IsEnabled()) {
            return;
        }

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

            std::uint32_t color = 0xFFFFFFFF; // White
            if (aiObj.IsHero()) {
                if (localPlayer.IsValid() && aiObj.Address() == localPlayer.Address()) {
                    color = 0xFF00FFFF; // Cyan (Local Player)
                } else if (aiObj.Team() == SDK::GameObjectTeam::Order) {
                    color = 0xFF00FF00; // Green (Allies)
                } else if (aiObj.Team() == SDK::GameObjectTeam::Chaos) {
                    color = 0xFFFF0000; // Red (Enemies)
                }
            } else {
                color = 0xFF888888; // Grey (Minions)
            }

            if (drawRange_) {
                float range = aiObj.AttackRange();
                SDK::Drawing::DrawCircle(aiObj.Position(), range, color, 1.5f, 64, false);
            }

            auto path = aiObj.Path();
            if (drawPaths_ && !path.empty()) {
                Vec3 current = aiObj.Position();
                for (const auto& point : path) {
                    SDK::Drawing::DrawLine(current, point, color, 1.5f, false);
                    SDK::Drawing::DrawCircle(point, 10.0f, color, 1.0f, 64, false);
                    current = point;
                }
            }

            if (drawPrediction_ && aiObj.HasPath() && aiObj.MoveSpeed() > 0.01f) {
                float distToMove = aiObj.MoveSpeed() * (static_cast<float>(predictionMs_) / 1000.0f);
                Vec3 predictedPos = aiObj.Position();
                
                if (!path.empty()) {
                    Vec3 current = aiObj.Position();
                    for (const auto& wp : path) {
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
                SDK::Drawing::DrawLine(aiObj.Position(), predictedPos, 0xFFFFD700, 2.0f, false);
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
