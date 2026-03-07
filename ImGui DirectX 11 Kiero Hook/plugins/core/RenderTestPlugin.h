#pragma once
#include "../IPlugin.h"
#include "sdk/GameObjects/GameObjects.h"
#include "sdk/UI/Drawing.h"
#include "core/Globals.h"
#include "core/Offsets.h"
#include <Psapi.h>

namespace Plugins {
    class RenderTestPlugin : public IPlugin {
    private:
        std::shared_ptr<SDK::MenuUI::Menu> m_menu;

    public:
        const char* GetName() const override { return "RenderTest"; }
        const char* GetAuthor() const override { return "Antigravity"; }
        PluginCategory GetCategory() const override { return PluginCategory::Other; }

        void OnLoad() override {
            m_menu = SDK::MenuUI::Menu::Create("RenderTest", "Spoof Render Test");
            m_menu->Add<SDK::MenuUI::MenuBool>("DrawText", "Draw Spoofed W2S On Champion", true);
        }

        void OnUnload() override {
            SDK::MenuUI::Menu::Remove("RenderTest");
            m_menu.reset();
        }

        void OnUpdate() override {}

        void OnRender() override {
            if (!m_menu || !m_menu->Get<SDK::MenuUI::MenuBool>("DrawText")->Enabled) return;
            if (!SDK::GameObjects::Player.IsValid()) return;

            Vec3 myPos = SDK::GameObjects::Player.GetPosition();
            Vec2 screenPos;
            
            if (!SDK::Drawing::WorldToScreen(myPos, screenPos)) return;

            auto aimgr = SDK::GameObjects::Player.GetAiManager();
            if (!aimgr.IsValid()) {
                SDK::Drawing::DrawScreenText(Vec2(screenPos.x - 80.0f, screenPos.y - 50.0f), "AiManager INVALID!", IM_COL32(255, 0, 0, 255));
                return;
            }

            // Read ALL data
            bool isMoving = aimgr.IsMoving();
            bool isDashing = aimgr.IsDashing();
            bool hasPath = aimgr.HasPath();
            float moveSpeed = aimgr.GetMoveSpeed();
            float dashSpeed = aimgr.GetDashSpeed();
            int curSeg = aimgr.GetCurrentSegment();
            int segCount = aimgr.GetSegmentCount();
            Vec3 serverPos = aimgr.GetServerPosition();
            Vec3 pathStart = aimgr.GetPathStart();
            Vec3 pathEnd = aimgr.GetPathEnd();
            Vec3 targetPos = aimgr.GetTargetPosition();
            Vec3 moveVec3 = aimgr.GetMoveVec3();
            uintptr_t navArr = aimgr.GetNavArrayPtr();
            auto path = aimgr.GetRemainingPath();

            // State text
            const char* stateText = "IDLE";
            ImU32 stateCol = IM_COL32(150, 150, 150, 255);
            if (isDashing) { stateText = "DASHING"; stateCol = IM_COL32(255, 50, 50, 255); }
            else if (isMoving) { stateText = "MOVING"; stateCol = IM_COL32(0, 255, 100, 255); }

            // === DEBUG TEXT PANEL (top-left area) ===
            float panelX = 10.0f;
            float panelY = 300.0f;
            float lineH = 16.0f;
            int line = 0;

            auto drawLine = [&](const char* txt, ImU32 col = IM_COL32(255, 255, 255, 255)) {
                SDK::Drawing::DrawScreenText(Vec2(panelX, panelY + lineH * line), txt, col);
                line++;
            };

            char buf[256];

            drawLine("=== AiManager Debug ===", IM_COL32(0, 200, 255, 255));

            snprintf(buf, sizeof(buf), "State: %s", stateText);
            drawLine(buf, stateCol);

            snprintf(buf, sizeof(buf), "MoveSpeed(0x318): %.1f", moveSpeed);
            drawLine(buf);

            snprintf(buf, sizeof(buf), "IsMoving(0x31C): %d", isMoving ? 1 : 0);
            drawLine(buf, isMoving ? IM_COL32(0, 255, 100, 255) : IM_COL32(150, 150, 150, 255));

            snprintf(buf, sizeof(buf), "IsDashing(0x384): %d", isDashing ? 1 : 0);
            drawLine(buf, isDashing ? IM_COL32(255, 50, 50, 255) : IM_COL32(150, 150, 150, 255));

            snprintf(buf, sizeof(buf), "HasPath(0x354): %d", hasPath ? 1 : 0);
            drawLine(buf);

            snprintf(buf, sizeof(buf), "DashSpeed(0x360): %.1f", dashSpeed);
            drawLine(buf);

            snprintf(buf, sizeof(buf), "CurSeg/Total(0x320/0x350): %d / %d", curSeg, segCount);
            drawLine(buf);

            snprintf(buf, sizeof(buf), "ServerPos(0x474): %.0f, %.0f, %.0f", serverPos.x, serverPos.y, serverPos.z);
            drawLine(buf, IM_COL32(0, 150, 255, 255));

            snprintf(buf, sizeof(buf), "TargetPos(0x034): %.0f, %.0f, %.0f", targetPos.x, targetPos.y, targetPos.z);
            drawLine(buf, IM_COL32(255, 200, 0, 255));

            snprintf(buf, sizeof(buf), "PathStart(0x330): %.0f, %.0f, %.0f", pathStart.x, pathStart.y, pathStart.z);
            drawLine(buf, IM_COL32(100, 255, 100, 255));

            snprintf(buf, sizeof(buf), "PathEnd(0x33C): %.0f, %.0f, %.0f", pathEnd.x, pathEnd.y, pathEnd.z);
            drawLine(buf, IM_COL32(255, 0, 150, 255));

            snprintf(buf, sizeof(buf), "MoveVec3(0x480): %.0f, %.0f, %.0f", moveVec3.x, moveVec3.y, moveVec3.z);
            drawLine(buf);

            snprintf(buf, sizeof(buf), "NavArray(0x348): 0x%llX", (unsigned long long)navArr);
            drawLine(buf);

            snprintf(buf, sizeof(buf), "Waypoints: %zu remaining", path.size());
            drawLine(buf, IM_COL32(255, 100, 0, 255));

            // Show each waypoint
            for (size_t i = 0; i < path.size() && i < 10; i++) {
                snprintf(buf, sizeof(buf), "  WP[%zu]: %.0f, %.0f, %.0f", i, path[i].x, path[i].y, path[i].z);
                drawLine(buf, IM_COL32(255, 180, 50, 255));
            }

            // === WORLD RENDERING ===
            // ServerPos circle (blue)
            SDK::Drawing::DrawCircle(serverPos, 60.0f, IM_COL32(0, 150, 255, 255), 2.0f);

            // TargetPos circle (yellow)
            if (!targetPos.IsZero()) {
                SDK::Drawing::DrawCircle(targetPos, 30.0f, IM_COL32(255, 200, 0, 255), 1.5f);
            }
            
            // Draw Path with waypoints
            if (isMoving || isDashing) {
                // PathEnd circle (magenta)
                SDK::Drawing::DrawCircle(pathEnd, 40.0f, IM_COL32(255, 0, 150, 255), 2.0f);
                
                if (!path.empty()) {
                    Vec3 current = serverPos;
                    for (size_t i = 0; i < path.size(); i++) {
                        const Vec3& waypoint = path[i];
                        SDK::Drawing::DrawLine(current, waypoint, IM_COL32(255, 255, 0, 255), 2.0f);
                        SDK::Drawing::DrawCircle(waypoint, 20.0f, IM_COL32(255, 100, 0, 255), 1.5f);
                        current = waypoint;
                    }
                } else {
                    SDK::Drawing::DrawLine(serverPos, pathEnd, IM_COL32(255, 255, 0, 200), 2.0f);
                }
            }

            // State label above champion
            SDK::Drawing::DrawScreenText(Vec2(screenPos.x - 30.0f, screenPos.y - 50.0f), stateText, stateCol);
        }
    };
}
