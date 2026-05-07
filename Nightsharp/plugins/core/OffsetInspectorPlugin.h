#pragma once

// ============================================================================
// OffsetInspectorPlugin
// ----------------------------------------------------------------------------
// Diagnostic tool for verifying SDK offsets. Draws live values from
// GameObject / AIBaseClient / AIHeroClient accessors in two places:
//
//   1) An ImGui floating window ("Offset Debug") with raw values for the
//      local player + every enemy hero — fast way to eyeball if a field
//      looks wrong (e.g. IsDead returning false while the hero is dead,
//      Velocity not matching movement direction, PathEnd miles away, …).
//
//   2) A 3D overlay painted on every hero in the world:
//         - Circle at Position() (BoundingRadius)               — position offset sanity
//         - Larger circle at Position() + AttackRange()         — AttackRange sanity
//         - Arrow Position() → Position() + Velocity * scale   — Velocity sanity
//         - Cross at PreviousPosition()                         — prediction input sanity
//         - Cross at PathEnd() + line Position → PathEnd        — path offset sanity
//         - Red X + "DEAD" label when IsDead() fires            — Dead offset sanity
//         - State tags ("WU"/"MOV"/"DSH"/"INV"/"HID")          — flag offsets sanity
//
// Every drawing category is toggled via the menu so the screen doesn't turn
// into noise. Point-in-time values update per frame from ObjectManager.
// ============================================================================

#include "../IPlugin.h"
#include "../../core/CoreAi.h"
#include "../../core/Globals.h"
#include "../../core/offset.h"
#include "../../imgui/imgui.h"
#include "../../sdk/GameObjects/ObjectManager.h"
#include "../../sdk/UI/Drawing.h"
#include "../../sdk/UI/UI.h"

#include <algorithm>
#include <cstdio>

namespace Plugins {

class OffsetInspectorPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Offset Inspector [Debug]"; }
    const char* GetInternalId() const override { return "plugin_offsetinspector"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }

    void OnLoad() override {
        if (m_menu) {
            return;
        }

        m_menu = SDK::Menu::Create("plugin_offsetinspector", "Offset Inspector");

        m_menu->Add<SDK::MenuSeparator>("sepWindow", "Debug Window");
        m_menu->Add<SDK::MenuBool>("showWindow", "Show Debug Window", true);
        m_menu->Add<SDK::MenuBool>("windowLocalOnly", "  Local Player Only", false);

        m_menu->Add<SDK::MenuSeparator>("sepOverlay", "World Overlay");
        m_menu->Add<SDK::MenuBool>("overlayEnabled", "Enable World Overlay", true);
        m_menu->Add<SDK::MenuBool>("overlaySelfOnly", "  Local Player Only", false);
        m_menu->Add<SDK::MenuBool>("overlayEnemiesOnly", "  Enemy Heroes Only", false);
        m_menu->Add<SDK::MenuBool>("overlayIncludeMinions", "  Include Minions", false);

        m_menu->Add<SDK::MenuSeparator>("sepCats", "Categories");
        m_menu->Add<SDK::MenuBool>("drawPosition", "Draw Position + BoundingRadius", true);
        m_menu->Add<SDK::MenuBool>("drawAttackRange", "Draw AttackRange Circle", true);
        m_menu->Add<SDK::MenuBool>("drawVelocity", "Draw Velocity Arrow", true);
        m_menu->Add<SDK::MenuBool>("drawPreviousPos", "Draw PreviousPosition Cross", true);
        m_menu->Add<SDK::MenuBool>("drawWaypoints", "Draw Nav Waypoints (real path)", true);
        m_menu->Add<SDK::MenuBool>("drawPathEndOnly", "Draw PathEnd Cross (raw target)", false);
        m_menu->Add<SDK::MenuBool>("drawServerPos", "Draw ServerPosition Cross", false);
        m_menu->Add<SDK::MenuBool>("drawDeadMark", "Draw DEAD mark when IsDead()", true);
        m_menu->Add<SDK::MenuBool>("drawStateTags", "Draw State Tags (WU/MOV/DSH)", true);
        m_menu->Add<SDK::MenuBool>("drawNetIdLabel", "Draw NetId Label", false);

        m_menu->Add<SDK::MenuSeparator>("sepTune", "Tuning");
        m_menu->Add<SDK::MenuSlider>("velocityScale", "Velocity Arrow Length", 100, 10, 500);
    }

    void OnUnload() override {
        // Menu is retained (so the toggles persist across reload); nothing to do.
    }

    // No per-frame work needed — everything is pulled fresh from
    // ObjectManager inside OnRender() below.
    void OnUpdate() override {}

    void OnRender() override {
        if (!m_menu) {
            return;
        }

        const bool showWindow = m_menu->GetBoolValue("showWindow", true);
        if (showWindow) {
            RenderDebugWindow();
        }

        if (m_menu->GetBoolValue("overlayEnabled", true)) {
            RenderWorldOverlay();
        }
    }

    SDK::MenuUI::Menu* GetMenuRoot() override { return m_menu; }

private:
    SDK::MenuUI::Menu* m_menu = nullptr;

    // ── Floating ImGui window ────────────────────────────────────────────
    //
    // Shows one collapsible section per hero with raw accessor values. The
    // columns are aligned so a broken offset sticks out (e.g. Health that
    // always reads zero, Position that never updates, Dead flag flipped).
    void RenderDebugWindow() {
        ImGui::SetNextWindowSize(ImVec2(640.0f, 520.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 80.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.82f);
        if (!ImGui::Begin("Offset Debug", nullptr, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            return;
        }

        const auto player = Player();
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.0f, 1.0f), "Local Player");
        if (player.IsValid()) {
            DrawHeroRow(player, true);
        } else {
            ImGui::TextDisabled("(player not valid)");
        }

        ImGui::Separator();

        if (m_menu->GetBoolValue("windowLocalOnly", false)) {
            ImGui::End();
            return;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f), "Heroes (all teams)");
        int idx = 0;
        for (const auto& hero : SDK::ObjectManager::Heroes()) {
            if (!hero.IsValid()) continue;
            // Skip the local player — already rendered above.
            if (player.IsValid() && hero.NetworkId() == player.NetworkId()) continue;
            ImGui::PushID(++idx);
            DrawHeroRow(hero, false);
            ImGui::PopID();
        }

        ImGui::End();
    }

    // Render a single hero row in the debug window. `isLocal` highlights it.
    void DrawHeroRow(const SDK::AIHeroClient& hero, bool isLocal) {
        const auto name = hero.CharacterName();
        char header[160];
        std::snprintf(header, sizeof(header), "%s%s  [NetId=%d Addr=0x%llX]",
                      isLocal ? "[self] " : "",
                      name.empty() ? "?" : name.c_str(),
                      hero.NetworkId(),
                      static_cast<unsigned long long>(hero.Address()));

        const ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_SpanAvailWidth |
            (isLocal ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        if (!ImGui::TreeNodeEx(header, flags)) {
            return;
        }

        // Pull every value once per row (cheap — they're already read once
        // per frame by the SDK).
        const auto pos  = hero.Position();
        const auto prev = hero.PreviousPosition();
        const auto vel  = hero.Velocity();
        const auto path = hero.PathEnd();
        const auto dir  = hero.Direction();

        ImGui::Text("Position        : (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
        ImGui::Text("PreviousPosition: (%.1f, %.1f, %.1f)", prev.x, prev.y, prev.z);
        ImGui::Text("Velocity        : (%.1f, %.1f, %.1f)  |v|=%.1f",
                    vel.x, vel.y, vel.z, vel.Length());
        ImGui::Text("Direction       : (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);
        ImGui::Text("PathEnd         : (%.1f, %.1f, %.1f)  segs=%d",
                    path.x, path.y, path.z, hero.GetPathLength());
        const auto ai = CoreAi::Get(hero.Address());
        if (ai.IsValid()) {
            const int inner320 = Globals::Read<int>(
                ai.inner + Offset::AiManagerInnerCompatLayout::CurrentSegment);
            const int inner384 = Globals::Read<int>(
                ai.inner + Offset::AiManagerInnerCompatLayout::IsDashing);
            const int inner384Byte = static_cast<int>(Globals::Read<uint8_t>(
                ai.inner + Offset::AiManagerInnerCompatLayout::IsDashing));
            const int nav35c = Globals::IsValidPtr(ai.navBase)
                ? Globals::Read<int>(ai.navBase + Offset::AiManagerNavDataLayout::IsDashingInner)
                : 0;
            const int nav35cByte = Globals::IsValidPtr(ai.navBase)
                ? static_cast<int>(Globals::Read<uint8_t>(
                    ai.navBase + Offset::AiManagerNavDataLayout::IsDashingInner))
                : 0;
            const float dashSpeed = Globals::Read<float>(
                ai.inner + Offset::AiManagerInnerCompatLayout::DashSpeed);
            const float dashRemain = Globals::Read<float>(
                ai.inner + Offset::AiManagerInnerCompatLayout::DashDistRemaining);
            const float dashDuration = Globals::Read<float>(
                ai.inner + Offset::AiManagerInnerCompatLayout::DashDuration);

            ImGui::Text("AiManager       : raw=0x%llX inner=0x%llX nav=0x%llX",
                        static_cast<unsigned long long>(ai.raw),
                        static_cast<unsigned long long>(ai.inner),
                        static_cast<unsigned long long>(ai.navBase));
            ImGui::Text("Dash raw        : inner+320=%d  inner+384.i=%d b=%d  nav+35C.i=%d b=%d",
                        inner320, inner384, inner384Byte, nav35c, nav35cByte);
            ImGui::Text("Dash data       : speed=%.1f  remain=%.1f  duration=%.3f",
                        dashSpeed, dashRemain, dashDuration);
        }

        ImGui::Separator();
        ImGui::Text("Health   : %8.1f / %8.1f   (%.1f%%)",
                    hero.Health(), hero.MaxHealth(), hero.HealthPercent() * 100.0f);
        ImGui::Text("Mana     : %8.1f / %8.1f",
                    hero.Mana(), hero.MaxMana());
        ImGui::Text("AD/AP/Arm: %6.1f / %6.1f / %6.1f",
                    hero.TotalAttackDamage(), hero.AbilityPower(), hero.Armor());
        ImGui::Text("Range    : AA=%.0f  BoundingR=%.0f  MoveSpd=%.0f",
                    hero.AttackRange(), hero.BoundingRadius(), hero.MoveSpeed());
        ImGui::Text("Level    : %d   Team=%d   Index=%d",
                    hero.Level(), hero.Team(), hero.Index());

        ImGui::Separator();
        // Colour-code the boolean flags so a wrong one jumps out.
        DrawBool("IsValid",        hero.IsValid());
        ImGui::SameLine(160.0f); DrawBool("IsAlive",   hero.IsAlive());
        ImGui::SameLine(300.0f); DrawBool("IsDead",    hero.IsDead());
        ImGui::SameLine(440.0f); DrawBool("IsVisible", hero.IsVisible());

        DrawBool("IsTargetable", hero.IsTargetable());
        ImGui::SameLine(160.0f); DrawBool("IsInvulnerable", hero.IsInvulnerable());
        ImGui::SameLine(300.0f); DrawBool("IsRecalling",    hero.IsRecalling());
        ImGui::SameLine(440.0f); DrawBool("IsWindingUp",    hero.IsWindingUp());

        DrawBool("IsMoving",   hero.IsMoving());
        ImGui::SameLine(160.0f); DrawBool("IsDashing",  hero.IsDashing());
        ImGui::SameLine(300.0f); DrawBool("IsImmobile", hero.IsImmobile());
        ImGui::SameLine(440.0f); DrawBool("IsEnemy",    hero.IsEnemy());

        ImGui::TreePop();
    }

    // Small coloured label — green=true, red=false — so a broken flag
    // offset reads as an obviously-wrong colour.
    static void DrawBool(const char* label, bool value) {
        const ImVec4 col = value ? ImVec4(0.35f, 1.00f, 0.35f, 1.0f)
                                 : ImVec4(1.00f, 0.35f, 0.35f, 1.0f);
        ImGui::TextColored(col, "%s=%s", label, value ? "true" : "false");
    }

    // ── 3D world overlay on every hero (and optionally minions) ──────────
    //
    // We pull the toggle state once so per-hero cost stays low even when
    // all categories are on.
    void RenderWorldOverlay() {
        const bool drawPos         = m_menu->GetBoolValue("drawPosition", true);
        const bool drawRange       = m_menu->GetBoolValue("drawAttackRange", true);
        const bool drawVel         = m_menu->GetBoolValue("drawVelocity", true);
        const bool drawPrev        = m_menu->GetBoolValue("drawPreviousPos", true);
        const bool drawWaypoints   = m_menu->GetBoolValue("drawWaypoints", true);
        const bool drawPathEndOnly = m_menu->GetBoolValue("drawPathEndOnly", false);
        const bool drawServerPos   = m_menu->GetBoolValue("drawServerPos", false);
        const bool drawDeadMark    = m_menu->GetBoolValue("drawDeadMark", true);
        const bool drawStateTags   = m_menu->GetBoolValue("drawStateTags", true);
        const bool drawNetId       = m_menu->GetBoolValue("drawNetIdLabel", false);
        const bool selfOnly        = m_menu->GetBoolValue("overlaySelfOnly", false);
        const bool enemiesOnly     = m_menu->GetBoolValue("overlayEnemiesOnly", false);
        const bool includeMinions  = m_menu->GetBoolValue("overlayIncludeMinions", false);
        const float velScale =
            static_cast<float>(m_menu->GetSliderValue("velocityScale", 100)) / 100.0f;

        const auto player = Player();

        if (selfOnly) {
            if (player.IsValid()) {
                DrawHeroOverlay(player, true, drawPos, drawRange, drawVel,
                                drawPrev, drawWaypoints, drawPathEndOnly, drawServerPos,
                                drawDeadMark, drawStateTags, drawNetId, velScale);
            }
            return;
        }

        for (const auto& hero : SDK::ObjectManager::Heroes()) {
            if (!hero.IsValid()) continue;
            const bool isSelf = player.IsValid() && hero.NetworkId() == player.NetworkId();
            // Enemies-only filter: skip self + allies. The local player is
            // always drawn at home above so a self-row stays available
            // when the user wants self+enemies.
            if (enemiesOnly && !hero.IsEnemy()) continue;
            DrawHeroOverlay(hero, isSelf, drawPos, drawRange, drawVel,
                            drawPrev, drawWaypoints, drawPathEndOnly, drawServerPos,
                            drawDeadMark, drawStateTags, drawNetId, velScale);
        }

        if (includeMinions) {
            for (const auto& minion : SDK::ObjectManager::EnemyMinions()) {
                if (!minion.IsValid()) continue;
                DrawMinionOverlay(minion, drawPos, drawDeadMark, drawNetId);
            }
            for (const auto& minion : SDK::ObjectManager::AllyMinions()) {
                if (!minion.IsValid()) continue;
                DrawMinionOverlay(minion, drawPos, drawDeadMark, drawNetId);
            }
        }
    }

    // Colour key:
    //   green  – friendly to local player (ally / self)
    //   red    – enemy
    //   yellow – neutral / unknown team
    //   grey   – invalid / dead state
    static ImU32 TeamColour(const SDK::AIHeroClient& hero, bool isSelf) {
        if (!hero.IsAlive()) {
            return IM_COL32(120, 120, 120, 200);
        }
        if (isSelf) {
            return IM_COL32(120, 200, 255, 220);
        }
        if (hero.IsEnemy()) {
            return IM_COL32(255, 90, 90, 220);
        }
        if (hero.IsAlly()) {
            return IM_COL32(120, 255, 120, 200);
        }
        return IM_COL32(230, 220, 80, 200);
    }

    void DrawHeroOverlay(const SDK::AIHeroClient& hero,
                         bool isSelf,
                         bool drawPos,
                         bool drawRange,
                         bool drawVel,
                         bool drawPrev,
                         bool drawWaypoints,
                         bool drawPathEndOnly,
                         bool drawServerPos,
                         bool drawDeadMark,
                         bool drawStateTags,
                         bool drawNetId,
                         float velScale) const {
        const ImU32 col = TeamColour(hero, isSelf);
        const auto pos = hero.Position();
        if (pos.IsZero()) {
            return;
        }

        if (drawPos) {
            SDK::Drawing::DrawCircle(pos, std::max(hero.BoundingRadius(), 30.0f),
                                     col, 2.0f, 32, true);
        }

        if (drawRange && hero.AttackRange() > 10.0f) {
            SDK::Drawing::DrawCircle(pos, hero.AttackRange() + hero.BoundingRadius(),
                                     (col & 0x00FFFFFF) | (110u << 24),
                                     1.4f, 36, true);
        }

        if (drawVel) {
            auto vel = hero.Velocity();
            vel.y = 0.0f;
            if (vel.IsValid() && vel.Length2D() > 1.0f && vel.Length2D() < 5000.0f) {
                const SDK::Vector3 tip{
                    pos.x + vel.x * velScale,
                    pos.y,
                    pos.z + vel.z * velScale,
                };
                SDK::Drawing::DrawLine(pos, tip, IM_COL32(255, 200, 0, 230), 2.0f);
            }
        }

        if (drawPrev) {
            const auto prev = hero.PreviousPosition();
            if (!prev.IsZero()) {
                SDK::Drawing::DrawCircle(prev, 35.0f, IM_COL32(180, 120, 255, 200),
                                         1.2f, 20, true);
            }
        }

        if (drawServerPos) {
            const auto sp = hero.ServerPosition();
            if (!sp.IsZero() && sp.Distance2D(pos) > 0.5f) {
                SDK::Drawing::DrawCircle(sp, 30.0f, IM_COL32(255, 255, 0, 180),
                                         1.0f, 18, true);
            }
        }

        // Nav waypoints — this is the REAL path the engine uses for
        // movement / prediction. Drawing this lets you confirm whether
        // `Path[]` array offsets and `SegmentsCount` are sane: a healthy
        // path follows the terrain (does NOT cut through walls), starts
        // at or near `Position`, and ends at the order point. If the
        // polyline goes through unwalkable terrain or jumps to (0,0,0),
        // the AiManager nav offsets need re-verification.
        if (drawWaypoints) {
            const auto waypoints = hero.GetWaypoints();
            if (waypoints.size() >= 2) {
                for (size_t i = 0; i + 1 < waypoints.size(); ++i) {
                    SDK::Drawing::DrawLine(waypoints[i], waypoints[i + 1],
                                           IM_COL32(80, 200, 255, 220), 1.8f);
                }
                // Mark each waypoint vertex.
                for (const auto& wp : waypoints) {
                    SDK::Drawing::DrawCircle(wp, 18.0f,
                                             IM_COL32(80, 200, 255, 200),
                                             1.2f, 10, true);
                }
                // Highlight final destination.
                SDK::Drawing::DrawCircle(waypoints.back(), 38.0f,
                                         IM_COL32(80, 200, 255, 255),
                                         1.6f, 18, true);
            }
        }

        // Raw `PathEnd` cross — debug-only. NOTE: this is the raw order
        // target (cursor click), so a STRAIGHT line from `pos` to
        // `pathEnd` cutting through walls is **expected** — that's why
        // engine prediction uses the waypoint array above, not pathEnd.
        if (drawPathEndOnly) {
            const auto pathEnd = hero.PathEnd();
            if (!pathEnd.IsZero() && pathEnd.Distance2D(pos) > 50.0f) {
                SDK::Drawing::DrawCircle(pathEnd, 45.0f, IM_COL32(255, 80, 180, 220),
                                         1.6f, 18, true);
                SDK::Drawing::DrawText(pathEnd, "[raw target]",
                                       IM_COL32(255, 80, 180, 240), true, true);
            }
        }

        if (drawDeadMark && hero.IsDead()) {
            SDK::Drawing::DrawText(pos, "[DEAD]",
                                   IM_COL32(255, 90, 90, 255), true, true);
        }

        if (drawStateTags) {
            char tag[64] = {};
            int  n = 0;
            if (hero.IsWindingUp())     n += std::snprintf(tag + n, sizeof(tag) - n, "WU ");
            if (hero.IsMoving())        n += std::snprintf(tag + n, sizeof(tag) - n, "MOV ");
            if (hero.IsDashing())       n += std::snprintf(tag + n, sizeof(tag) - n, "DSH ");
            if (hero.IsInvulnerable())  n += std::snprintf(tag + n, sizeof(tag) - n, "INV ");
            if (!hero.IsVisible())      n += std::snprintf(tag + n, sizeof(tag) - n, "HID ");
            if (hero.IsRecalling())     n += std::snprintf(tag + n, sizeof(tag) - n, "RC ");
            if (!hero.IsTargetable())   n += std::snprintf(tag + n, sizeof(tag) - n, "!TGT ");
            if (n > 0) {
                SDK::Vector3 labelPos = pos;
                labelPos.y += 110.0f;  // float above model
                SDK::Drawing::DrawText(labelPos, tag,
                                       IM_COL32(255, 220, 120, 240), true, true);
            }
        }

        if (drawNetId) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "NetId=%d", hero.NetworkId());
            SDK::Vector3 labelPos = pos;
            labelPos.y += 70.0f;
            SDK::Drawing::DrawText(labelPos, buf,
                                   IM_COL32(220, 220, 220, 220), true, true);
        }
    }

    // Lightweight minion overlay — we only show BR + Dead mark; we don't
    // need Velocity / Path here for offset verification (unless a future
    // problem motivates it, in which case the same toggles apply).
    void DrawMinionOverlay(const SDK::GameObject& minion,
                           bool drawPos,
                           bool drawDeadMark,
                           bool drawNetId) const {
        const auto pos = minion.Position();
        if (pos.IsZero()) {
            return;
        }
        const ImU32 col = minion.IsAlive()
            ? (minion.IsEnemy() ? IM_COL32(255, 140, 140, 200)
                                : IM_COL32(140, 255, 140, 200))
            : IM_COL32(120, 120, 120, 180);

        if (drawPos) {
            SDK::Drawing::DrawCircle(pos, std::max(minion.BoundingRadius(), 25.0f),
                                     col, 1.2f, 20, true);
        }

        if (drawDeadMark && minion.IsDead()) {
            SDK::Drawing::DrawText(pos, "[DEAD]",
                                   IM_COL32(255, 90, 90, 255), true, true);
        }

        if (drawNetId) {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "NetId=%d", minion.NetworkId());
            SDK::Vector3 labelPos = pos;
            labelPos.y += 45.0f;
            SDK::Drawing::DrawText(labelPos, buf,
                                   IM_COL32(210, 210, 210, 200), true, true);
        }
    }
};

} // namespace Plugins
