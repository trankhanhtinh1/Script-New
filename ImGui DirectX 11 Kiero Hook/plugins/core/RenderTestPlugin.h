#pragma once
#include "../IPlugin.h"
#include "sdk/GameObjects/GameObjects.h"
#include "sdk/GameObjects/Missile.h"
#include "sdk/UI/Drawing.h"
#include "core/Globals.h"
#include "core/Offsets.h"
#include <Psapi.h>
#include <cmath>
#include <string>

// ============================================================================
// RenderTestPlugin — Vẽ skillshot THỰC TẾ đang bay trong game
//
// Chỉ vẽ khi có missile object tồn tại trong game memory.
// Tra cứu radius theo tên missile để vẽ đúng kích thước.
// ============================================================================

namespace Plugins {

// --- Bảng tra cứu radius theo missile name ---
struct MissileDrawInfo {
    const char* missileName;
    float       radius;         // Half-width của line hoặc radius của circle
    ImU32       color;
};

static const MissileDrawInfo g_MissileTable[] = {
    // Ezreal
    { "EzrealQ",           60.f,  IM_COL32( 80, 180, 255, 220) },
    { "EzrealW",           80.f,  IM_COL32( 80, 255, 180, 220) },
    { "EzrealR",          160.f,  IM_COL32(255, 120,  40, 220) },
    // Ahri
    { "AhriOrbMissile",   100.f,  IM_COL32(255,  80, 200, 220) },
    { "AhriOrbReturn",    100.f,  IM_COL32(255, 150, 230, 220) },
    { "AhriSeduceMissile", 60.f,  IM_COL32(255,  20, 100, 220) },
    // Ashe
    { "EnchantedCrystalArrow", 130.f, IM_COL32(100, 200, 255, 220) },
    // Braum
    { "BraumQMissile",    100.f,  IM_COL32( 50, 150, 255, 220) },
    // Caitlyn
    { "CaitlynEntrapmentMissile", 80.f, IM_COL32(200, 255, 80, 220) },
    // Draven
    { "DravenR",          160.f,  IM_COL32(255,  60,  60, 220) },
    // Jinx
    { "JinxWMissile",      60.f,  IM_COL32(220,  80, 255, 220) },
    { "JinxR",            140.f,  IM_COL32(255,  40,  40, 220) },
    // Lux
    { "LuxLightBindingMis", 70.f, IM_COL32(255, 220,  50, 220) },
    { "LuxR",             200.f,  IM_COL32(255, 255, 100, 220) },
    // Morgana
    { "MorganaQ",          70.f,  IM_COL32(100,  40, 255, 220) },
    // Thresh
    { "ThreshQMissile",    70.f,  IM_COL32(100, 255, 100, 220) },
    // Xerath
    { "XerathMageSpearMissile", 60.f, IM_COL32(100, 200, 255, 220) },
    // Zed
    { "ZedQMissile",       50.f,  IM_COL32(200, 200,  50, 220) },
    // Auto attacks (màu nhạt)
    { "BasicAttack",       40.f,  IM_COL32(255, 255, 255, 100) },
    { "CritAttack",        40.f,  IM_COL32(255, 220, 100, 100) },
};

static const MissileDrawInfo* LookupMissile(const std::string& name) {
    for (auto& e : g_MissileTable) {
        if (name.find(e.missileName) != std::string::npos)
            return &e;
    }
    return nullptr;
}

// --- Vẽ Line Skillshot: hình chữ nhật width=radius*2 từ start → end ---
static void DrawLineSkillshot(const SDK::Missile& m, float radius, ImU32 color) {
    Vec3 startPos = m.GetStartPos();
    Vec3 endPos   = m.GetEndPos();
    Vec3 curPos   = m.GetPosition();

    // Hình chữ nhật đường bay (start → end)
    ImU32 rectColor = (color & 0x00FFFFFF) | 0x60000000; // mờ hơn
    SDK::Drawing::DrawRectWorld(startPos, endPos, radius * 2.0f, rectColor, 1.5f);

    // Đường trung tâm
    SDK::Drawing::DrawLine3D(startPos, endPos,
        (color & 0x00FFFFFF) | 0x40000000, 1.0f);

    // Vòng tròn tại endpoint
    SDK::Drawing::DrawCircle(endPos, radius,
        (color & 0x00FFFFFF) | 0x80000000, 1.5f);

    // Vòng tròn đầu đạn hiện tại — rõ nhất
    if (!curPos.IsZero())
        SDK::Drawing::DrawCircle(curPos, radius * 0.7f, color, 2.5f);

    // Dot tại vị trí bắt đầu
    SDK::Drawing::DrawCircle(startPos, 15.0f,
        (color & 0x00FFFFFF) | 0x80000000, 1.0f);
}

class RenderTestPlugin : public IPlugin {
private:
    std::shared_ptr<SDK::MenuUI::Menu> m_menu;

public:
    const char* GetName()     const override { return "RenderTest"; }
    const char* GetAuthor()   const override { return "Antigravity"; }
    PluginCategory GetCategory() const override { return PluginCategory::Other; }

    void OnLoad() override {
        m_menu = SDK::MenuUI::Menu::Create("RenderTest", "Skillshot Draw Test");
        m_menu->Add<SDK::MenuUI::MenuBool>("DrawMissiles", "Draw Active Missiles",     true);
        m_menu->Add<SDK::MenuUI::MenuBool>("DrawAiDebug",  "Draw AiManager Debug",     false);
        m_menu->Add<SDK::MenuUI::MenuBool>("DrawTurret",   "Draw Turret Shots",        false);
        m_menu->Add<SDK::MenuUI::MenuBool>("DrawAA",       "Draw Auto Attacks",        false);
    }

    void OnUnload() override {
        SDK::MenuUI::Menu::Remove("RenderTest");
        m_menu.reset();
    }

    void OnUpdate() override {}

    void OnRender() override {
        if (!m_menu) return;
        if (!SDK::GameObjects::Player.IsValid()) return;

        // --- Vẽ Missiles đang bay ---
        if (m_menu->Get<SDK::MenuUI::MenuBool>("DrawMissiles")->Enabled)
            DrawActiveMissiles();

        // --- AiManager Debug ---
        if (m_menu->Get<SDK::MenuUI::MenuBool>("DrawAiDebug")->Enabled)
            DrawAiDebug();
    }

private:

    void DrawActiveMissiles() {
        bool showTurret = m_menu->Get<SDK::MenuUI::MenuBool>("DrawTurret")->Enabled;
        bool showAA     = m_menu->Get<SDK::MenuUI::MenuBool>("DrawAA")->Enabled;

        auto missiles = SDK::MissileManager::GetMissiles();

        // Panel bên trái
        float px = 10.0f, py = 350.0f, lh = 15.0f;
        int   ln = 0;
        char  buf[128];

        SDK::Drawing::DrawScreenText(Vec2(px, py + lh * ln++),
            "=== Missiles In Flight ===", IM_COL32(255, 200, 0, 255));

        int drawn = 0;
        for (auto& m : missiles) {
            if (!m.IsValid()) continue;

            std::string mname = m.GetMissileName();
            if (mname.empty()) mname = m.GetSpellName();
            if (mname.empty()) continue;

            // Lọc turret / AA nếu không bật
            bool isTurret = m.IsTurretShot();
            bool isAA     = m.IsAutoAttack();
            if (isTurret && !showTurret) continue;
            if (isAA     && !showAA)     continue;

            // Tìm radius trong bảng
            const MissileDrawInfo* info = LookupMissile(mname);
            float   radius = info ? info->radius : 50.0f;
            ImU32   color  = info ? info->color
                                  : IM_COL32(200, 200, 200, 180);

            // Vẽ skillshot
            DrawLineSkillshot(m, radius, color);

            // Label tên tại vị trí hiện tại
            Vec3 curPos = m.GetPosition();
            if (!curPos.IsZero())
                SDK::Drawing::DrawTextCentered(curPos, mname.c_str(), color);

            // Panel text
            if (drawn < 12) {
                snprintf(buf, sizeof(buf), "[%d] %s  r=%.0f", drawn + 1,
                    mname.c_str(), radius);
                SDK::Drawing::DrawScreenText(Vec2(px, py + lh * ln++),
                    buf, color);
            }
            drawn++;
        }

        if (drawn == 0) {
            SDK::Drawing::DrawScreenText(Vec2(px, py + lh * ln),
                "(no spells in flight)", IM_COL32(120, 120, 120, 200));
        }
    }

    void DrawAiDebug() {
        if (!SDK::GameObjects::Player.IsValid()) return;
        Vec3 myPos = SDK::GameObjects::Player.GetPosition();
        Vec2 myScreen;
        if (!SDK::Drawing::WorldToScreen(myPos, myScreen)) return;

        auto aimgr = SDK::GameObjects::Player.GetAiManager();
        if (!aimgr.IsValid()) {
            SDK::Drawing::DrawScreenText(
                Vec2(myScreen.x - 80.f, myScreen.y - 50.f),
                "AiManager INVALID", IM_COL32(255, 0, 0, 255));
            return;
        }

        bool  isMoving  = aimgr.IsMoving();
        bool  isDashing = aimgr.IsDashing();
        float speed     = aimgr.GetMoveSpeed();
        Vec3  serverPos = aimgr.GetServerPosition();
        Vec3  pathEnd   = aimgr.GetPathEnd();
        auto  path      = aimgr.GetRemainingPath();

        const char* stateText = isDashing ? "DASHING" : isMoving ? "MOVING" : "IDLE";
        ImU32 stateCol = isDashing ? IM_COL32(255,50,50,255)
                       : isMoving  ? IM_COL32(0,255,100,255)
                                   : IM_COL32(150,150,150,255);

        float px = 10.f, py = 300.f, lh = 16.f;
        int   ln = 0;
        char  buf[256];

        auto dLine = [&](const char* t, ImU32 c = IM_COL32(255,255,255,255)) {
            SDK::Drawing::DrawScreenText(Vec2(px, py + lh * ln++), t, c);
        };

        dLine("=== AiManager ===", IM_COL32(0, 200, 255, 255));
        snprintf(buf, sizeof(buf), "State: %s", stateText);      dLine(buf, stateCol);
        snprintf(buf, sizeof(buf), "Speed: %.0f", speed);         dLine(buf);
        snprintf(buf, sizeof(buf), "ServerPos: %.0f,%.0f",
            serverPos.x, serverPos.z);                            dLine(buf, IM_COL32(0,150,255,255));
        snprintf(buf, sizeof(buf), "PathEnd:   %.0f,%.0f",
            pathEnd.x, pathEnd.z);                                dLine(buf, IM_COL32(255,0,150,255));
        snprintf(buf, sizeof(buf), "Waypoints: %zu", path.size()); dLine(buf);

        SDK::Drawing::DrawCircle(serverPos, 60.f, IM_COL32(0,150,255,200), 2.f);
        if (!pathEnd.IsZero())
            SDK::Drawing::DrawCircle(pathEnd, 40.f, IM_COL32(255,0,150,200), 2.f);
        if (isMoving || isDashing) {
            SDK::Drawing::DrawLine3D(serverPos, pathEnd,
                IM_COL32(255,255,0,180), 2.f);
            Vec3 cur = serverPos;
            for (auto& wp : path) {
                SDK::Drawing::DrawLine3D(cur, wp, IM_COL32(255,200,0,150), 1.5f);
                SDK::Drawing::DrawCircle(wp, 20.f, IM_COL32(255,100,0,180), 1.f);
                cur = wp;
            }
        }

        SDK::Drawing::DrawScreenText(
            Vec2(myScreen.x - 30.f, myScreen.y - 50.f),
            stateText, stateCol);
    }
};

} // namespace Plugins
