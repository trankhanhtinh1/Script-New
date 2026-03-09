#pragma once
#include "../IPlugin.h"
#include "sdk/UI/MenuUI.h"
#include "sdk/GameObjects/GameObject.h"
#include "sdk/GameObjects/GameObjects.h"
#include "sdk/GameObjects/Missile.h"
#include "sdk/UI/Drawing.h"
#include "sdk/Game.h"
#include "sdk/Wrappers/Spells/SpellDatabase.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// RenderTestPlugin - Draw active missiles/skillshots for evade debugging
// ============================================================================

namespace Plugins {

    struct MissileDrawInfo {
        const char* missileName;
        float radius;
        ImU32 color;
    };

    static const MissileDrawInfo g_MissileTable[] = {
        // Ezreal
        { "ezrealq", 60.f,  IM_COL32(80, 180, 255, 220) },
        { "ezrealqmissile", 60.f,  IM_COL32(80, 180, 255, 220) },
        { "ezrealmysticshot", 60.f,  IM_COL32(80, 180, 255, 220) },
        { "ezrealmysticshotmissile", 60.f,  IM_COL32(80, 180, 255, 220) },
        { "ezrealw", 80.f,  IM_COL32(80, 255, 180, 220) },
        { "ezrealessencefluxmissile", 80.f,  IM_COL32(80, 255, 180, 220) },
        { "ezrealr", 160.f, IM_COL32(255, 120, 40, 220) },
        { "ezrealtrueshotbarrage", 160.f, IM_COL32(255, 120, 40, 220) },

        // Common skillshots
        { "ahriorbmissile", 100.f, IM_COL32(255, 80, 200, 220) },
        { "ahriseducemissile", 60.f, IM_COL32(255, 20, 100, 220) },
        { "enchantedcrystalarrow", 130.f, IM_COL32(100, 200, 255, 220) },
        { "braumqmissile", 100.f, IM_COL32(50, 150, 255, 220) },
        { "dravenr", 160.f, IM_COL32(255, 60, 60, 220) },
        { "jinxwmissile", 60.f, IM_COL32(220, 80, 255, 220) },
        { "jinxr", 140.f, IM_COL32(255, 40, 40, 220) },
        { "luxlightbindingmis", 70.f, IM_COL32(255, 220, 50, 220) },
        { "morganaq", 70.f, IM_COL32(100, 40, 255, 220) },
        { "threshqmissile", 70.f, IM_COL32(100, 255, 100, 220) },
        { "zedqmissile", 50.f, IM_COL32(200, 200, 50, 220) },

        // Auto attacks
        { "basicattack", 40.f, IM_COL32(255, 255, 255, 100) },
        { "critattack", 40.f, IM_COL32(255, 220, 100, 100) },
    };

    static std::string ToLowerCopy(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    }

    static std::string BuildLookupKey(const std::string& missileName,
                                      const std::string& spellName,
                                      const std::string& objectName) {
        std::string key;
        if (!missileName.empty()) {
            key += missileName;
            key.push_back(' ');
        }
        if (!spellName.empty()) {
            key += spellName;
            key.push_back(' ');
        }
        if (!objectName.empty()) {
            key += objectName;
        }
        return ToLowerCopy(key);
    }

    static std::string BuildDisplayName(const std::string& missileName,
                                        const std::string& spellName,
                                        const std::string& objectName,
                                        int netId) {
        if (!missileName.empty()) return missileName;
        if (!spellName.empty()) return spellName;
        if (!objectName.empty()) return objectName;

        char unk[64];
        snprintf(unk, sizeof(unk), "missile_%d", netId);
        return unk;
    }

    static const MissileDrawInfo* LookupMissile(const std::string& lookupKey) {
        for (const auto& e : g_MissileTable) {
            if (lookupKey.find(e.missileName) != std::string::npos) {
                return &e;
            }
        }
        return nullptr;
    }

    static const SDK::SpellDatabaseEntry* LookupSpellEntry(const std::string& missileName,
                                                           const std::string& spellName,
                                                           const std::string& objectName) {
        if (!missileName.empty()) {
            if (const auto* entry = SDK::SpellDatabase::GetByMissileName(missileName)) {
                return entry;
            }
            if (const auto* entry = SDK::SpellDatabase::GetByName(missileName)) {
                return entry;
            }
        }

        if (!spellName.empty()) {
            if (const auto* entry = SDK::SpellDatabase::GetByMissileName(spellName)) {
                return entry;
            }
            if (const auto* entry = SDK::SpellDatabase::GetByName(spellName)) {
                return entry;
            }
        }

        if (!objectName.empty()) {
            if (const auto* entry = SDK::SpellDatabase::GetByMissileName(objectName)) {
                return entry;
            }
            if (const auto* entry = SDK::SpellDatabase::GetBySourceObjectName(objectName)) {
                return entry;
            }
        }

        return nullptr;
    }

    static float ResolveRadius(const MissileDrawInfo* info,
                               const SDK::SpellDatabaseEntry* dbEntry,
                               bool isAA) {
        if (dbEntry) {
            int radius = dbEntry->Radius > 0 ? dbEntry->Radius : dbEntry->Width;
            if (radius > 0) {
                return (float)radius;
            }
        }

        if (info) {
            return info->radius;
        }

        return isAA ? 40.0f : 65.0f;
    }

    static ImU32 ResolveColor(const MissileDrawInfo* info,
                              const SDK::SpellDatabaseEntry* dbEntry) {
        if (info) {
            return info->color;
        }

        if (dbEntry) {
            if (dbEntry->DangerValue >= 4) return IM_COL32(255, 90, 90, 225);
            if (dbEntry->DangerValue == 3) return IM_COL32(255, 180, 70, 225);
            if (dbEntry->DangerValue == 2) return IM_COL32(120, 210, 255, 225);
            return IM_COL32(180, 230, 120, 225);
        }

        return IM_COL32(255, 220, 120, 210);
    }

    static bool DrawLineSkillshot(const SDK::Missile& m, float radius, ImU32 color) {
        Vec3 startPos = m.GetStartPos();
        Vec3 endPos = m.GetEndPos();
        Vec3 castEndPos = m.GetCastEndPos();
        Vec3 curPos = m.GetPosition();

        if (startPos.IsZero()) {
            startPos = !curPos.IsZero() ? curPos : castEndPos;
        }
        if (endPos.IsZero()) {
            endPos = !castEndPos.IsZero() ? castEndPos : curPos;
        }

        bool drewAnything = false;
        if (!startPos.IsZero() && !endPos.IsZero() && startPos.Distance2D(endPos) > 2.0f) {
            ImU32 rectColor = (color & 0x00FFFFFF) | 0x60000000;
            SDK::Drawing::DrawRectWorld(startPos, endPos, radius * 2.0f, rectColor, 1.5f);
            SDK::Drawing::DrawLine3D(startPos, endPos,
                (color & 0x00FFFFFF) | 0x50000000, 1.5f);
            SDK::Drawing::DrawCircle(endPos, radius,
                (color & 0x00FFFFFF) | 0x90000000, 1.5f);
            drewAnything = true;
        }

        if (!curPos.IsZero()) {
            SDK::Drawing::DrawCircle(curPos, std::max(20.0f, radius * 0.75f), color, 2.5f);
            drewAnything = true;
        }

        return drewAnything;
    }

    class RenderTestPlugin : public IPlugin {
    private:
        std::shared_ptr<SDK::MenuUI::Menu> m_menu;

    public:
        const char* GetName() const override { return "RenderTest"; }
        const char* GetAuthor() const override { return "NightSharp"; }
        PluginCategory GetCategory() const override { return PluginCategory::Other; }

        void OnLoad() override {
            m_menu = SDK::MenuUI::Menu::Create("RenderTest", "Skillshot Draw Test");
            m_menu->Add<SDK::MenuUI::MenuBool>("DrawMissiles", "Draw Active Missiles", true);
            m_menu->Add<SDK::MenuUI::MenuBool>("DrawAiDebug", "Draw AiManager Debug", false);
            m_menu->Add<SDK::MenuUI::MenuBool>("DrawTurret", "Draw Turret Shots", false);
            m_menu->Add<SDK::MenuUI::MenuBool>("DrawAA", "Draw Auto Attacks", false);
            m_menu->Add<SDK::MenuUI::MenuBool>("DrawUnknown", "Draw Unknown Missiles", true);
            m_menu->Add<SDK::MenuUI::MenuBool>("DebugPanel", "Show Left Debug Panel", true);
        }

        void OnUnload() override {
            SDK::MenuUI::Menu::Remove("RenderTest");
            m_menu.reset();
        }

        void OnUpdate() override {}

        void OnRender() override {
            if (!m_menu || !SDK::GameObjects::Player.IsValid()) {
                return;
            }

            if (m_menu->Get<SDK::MenuUI::MenuBool>("DrawMissiles")->Enabled) {
                DrawActiveMissiles();
            }

            if (m_menu->Get<SDK::MenuUI::MenuBool>("DrawAiDebug")->Enabled) {
                DrawAiDebug();
            }
        }

        void OnMenu() override {
            if (m_menu) {
                m_menu->Draw();
            }
        }

        SDK::MenuUI::Menu* GetMenuRoot() override {
            return m_menu.get();
        }

    private:
        void DrawActiveMissiles() {
            bool showTurret = m_menu->Get<SDK::MenuUI::MenuBool>("DrawTurret")->Enabled;
            bool showAA = m_menu->Get<SDK::MenuUI::MenuBool>("DrawAA")->Enabled;
            bool drawUnknown = m_menu->Get<SDK::MenuUI::MenuBool>("DrawUnknown")->Enabled;
            bool showPanel = m_menu->Get<SDK::MenuUI::MenuBool>("DebugPanel")->Enabled;

            auto missiles = SDK::MissileManager::GetMissiles();

            float px = 10.0f, py = 350.0f, lh = 15.0f;
            int ln = 0;
            char buf[220];

            if (showPanel) {
                snprintf(buf, sizeof(buf), "=== Missiles In Flight: %d ===", (int)missiles.size());
                SDK::Drawing::DrawScreenText(Vec2(px, py + lh * ln++), buf, IM_COL32(255, 200, 0, 255));
            }

            int drawn = 0;
            int listed = 0;
            int filteredTurret = 0;
            int filteredAA = 0;
            int filteredUnknown = 0;
            int noGeometry = 0;

            for (auto& m : missiles) {
                if (!m.IsValid()) {
                    continue;
                }

                std::string spellName = m.GetSpellName();
                std::string missileName = m.GetMissileName();
                std::string objectName = SDK::GameObject(m.address).GetName();
                std::string lookupKey = BuildLookupKey(missileName, spellName, objectName);
                const SDK::SpellDatabaseEntry* dbEntry = LookupSpellEntry(missileName, spellName, objectName);
                std::string displayName = BuildDisplayName(
                    missileName, spellName, objectName, m.GetNetworkId());
                if (displayName.find("missile_") == 0 && dbEntry) {
                    displayName = !dbEntry->MissileSpellName.empty()
                        ? dbEntry->MissileSpellName
                        : dbEntry->SpellName;
                }

                bool isTurret = m.IsTurretShot();
                bool isAA = m.IsAutoAttack();
                if (isTurret && !showTurret) {
                    filteredTurret++;
                    continue;
                }
                if (isAA && !showAA) {
                    filteredAA++;
                    continue;
                }

                const MissileDrawInfo* info = LookupMissile(lookupKey);
                bool known = (info != nullptr) || (dbEntry != nullptr);

                float radius = ResolveRadius(info, dbEntry, isAA);
                ImU32 color = ResolveColor(info, dbEntry);

                // Heuristic fallback: force-highlight Ezreal missiles even if name differs.
                if (!known && lookupKey.find("ezreal") != std::string::npos) {
                    if (lookupKey.find("q") != std::string::npos ||
                        lookupKey.find("mysticshot") != std::string::npos) {
                        known = true;
                        radius = 60.0f;
                        color = IM_COL32(80, 180, 255, 230);
                    }
                    else if (lookupKey.find("w") != std::string::npos ||
                             lookupKey.find("essenceflux") != std::string::npos) {
                        known = true;
                        radius = 80.0f;
                        color = IM_COL32(80, 255, 180, 230);
                    }
                    else if (lookupKey.find("r") != std::string::npos ||
                             lookupKey.find("trueshot") != std::string::npos) {
                        known = true;
                        radius = 160.0f;
                        color = IM_COL32(255, 120, 40, 230);
                    }
                }

                if (!known && !drawUnknown) {
                    filteredUnknown++;
                    continue;
                }

                bool rendered = DrawLineSkillshot(m, radius, color);
                if (!rendered) {
                    noGeometry++;
                    continue;
                }

                Vec3 labelPos = m.GetPosition();
                if (labelPos.IsZero()) labelPos = m.GetEndPos();
                if (labelPos.IsZero()) labelPos = m.GetStartPos();
                if (!labelPos.IsZero()) {
                    SDK::Drawing::DrawTextCentered(labelPos, displayName.c_str(), color);
                }

                if (showPanel && listed < 12) {
                    snprintf(buf, sizeof(buf), "[%d] %s | known=%s | spell=%s | db=%s",
                        listed + 1,
                        displayName.c_str(),
                        known ? "yes" : "no",
                        spellName.empty() ? "-" : spellName.c_str(),
                        dbEntry ? "yes" : "no");
                    SDK::Drawing::DrawScreenText(Vec2(px, py + lh * ln++), buf, color);
                    snprintf(buf, sizeof(buf), "    pos=%.0f,%.0f  start=%.0f,%.0f  end=%.0f,%.0f",
                        m.GetPosition().x, m.GetPosition().z,
                        m.GetStartPos().x, m.GetStartPos().z,
                        m.GetEndPos().x, m.GetEndPos().z);
                    SDK::Drawing::DrawScreenText(Vec2(px, py + lh * ln++), buf, IM_COL32(180, 180, 220, 210));
                    listed++;
                }

                drawn++;
            }

            if (showPanel) {
                snprintf(buf, sizeof(buf), "drawn=%d  turretFiltered=%d  aaFiltered=%d  unknownFiltered=%d  noGeometry=%d",
                    drawn, filteredTurret, filteredAA, filteredUnknown, noGeometry);
                SDK::Drawing::DrawScreenText(Vec2(px, py + lh * ln++), buf, IM_COL32(140, 200, 255, 220));
            }

            if (showPanel && drawn == 0) {
                SDK::Drawing::DrawScreenText(
                    Vec2(px, py + lh * ln),
                    "(no spells rendered: check DrawAA/DrawTurret/DrawUnknown toggles)",
                    IM_COL32(120, 120, 120, 210));
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

            bool isMoving = aimgr.IsMoving();
            bool isDashing = aimgr.IsDashing();
            float speed = aimgr.GetMoveSpeed();
            Vec3 serverPos = aimgr.GetServerPosition();
            Vec3 pathEnd = aimgr.GetPathEnd();
            auto path = aimgr.GetRemainingPath();

            const char* stateText = isDashing ? "DASHING" : isMoving ? "MOVING" : "IDLE";
            ImU32 stateCol = isDashing ? IM_COL32(255, 50, 50, 255)
                : isMoving ? IM_COL32(0, 255, 100, 255)
                : IM_COL32(150, 150, 150, 255);

            float px = 10.f, py = 300.f, lh = 16.f;
            int ln = 0;
            char buf[128];

            auto dLine = [&](const char* t, ImU32 c = IM_COL32(255, 255, 255, 255)) {
                SDK::Drawing::DrawScreenText(Vec2(px, py + lh * ln++), t, c);
            };

            dLine("=== AiManager ===", IM_COL32(0, 200, 255, 255));
            snprintf(buf, sizeof(buf), "State: %s", stateText); dLine(buf, stateCol);
            snprintf(buf, sizeof(buf), "Speed: %.0f", speed); dLine(buf);
            snprintf(buf, sizeof(buf), "ServerPos: %.0f,%.0f", serverPos.x, serverPos.z); dLine(buf, IM_COL32(0, 150, 255, 255));
            snprintf(buf, sizeof(buf), "PathEnd: %.0f,%.0f", pathEnd.x, pathEnd.z); dLine(buf, IM_COL32(255, 0, 150, 255));
            snprintf(buf, sizeof(buf), "Waypoints: %zu", path.size()); dLine(buf);

            SDK::Drawing::DrawCircle(serverPos, 60.f, IM_COL32(0, 150, 255, 200), 2.f);
            if (!pathEnd.IsZero()) {
                SDK::Drawing::DrawCircle(pathEnd, 40.f, IM_COL32(255, 0, 150, 200), 2.f);
            }
            if (isMoving || isDashing) {
                SDK::Drawing::DrawLine3D(serverPos, pathEnd, IM_COL32(255, 255, 0, 180), 2.f);
                Vec3 cur = serverPos;
                for (auto& wp : path) {
                    SDK::Drawing::DrawLine3D(cur, wp, IM_COL32(255, 200, 0, 150), 1.5f);
                    SDK::Drawing::DrawCircle(wp, 20.f, IM_COL32(255, 100, 0, 180), 1.f);
                    cur = wp;
                }
            }

            SDK::Drawing::DrawScreenText(Vec2(myScreen.x - 30.f, myScreen.y - 50.f), stateText, stateCol);
        }
    };

} // namespace Plugins
