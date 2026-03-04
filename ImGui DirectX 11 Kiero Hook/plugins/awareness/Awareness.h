#pragma once
#include "../IPlugin.h"
#include "../../sdk/SDK.h"
#include "../../menu/MenuConfig.h"
#include <unordered_map>

// ============================================================================
// Awareness Plugin — ESP, Spell Tracker, Range Circles, Jungle Timers
// ============================================================================

namespace Plugins {

    class AwarenessPlugin : public IPlugin {
    public:
        const char* GetName() const override { return "Awareness"; }
        const char* GetAuthor() const override { return "NS#"; }
        PluginCategory GetCategory() const override { return PluginCategory::Utility; }

        void OnLoad() override {}
        void OnUnload() override {}

        // ====================================================================
        // OnUpdate — track FOW and jungle camp timers
        // ====================================================================
        void OnUpdate() override {
            if (!SDK::GameObjects::Player.IsValid()) return;
            float now = SDK::Game::GetTime();

            // --- FOW Tracking ---
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;
                int idx = hero.GetNetId() & 0xF; // 0-15 unique per hero
                auto& info = lastSeen[idx];

                if (hero.IsVisible()) {
                    info.position = hero.GetPosition();
                    info.time = now;
                    info.wasVisible = true;
                } else if (!info.wasVisible) {
                    // First time seeing enemy but in fog — record position anyway
                    info.position = hero.GetPosition();
                }
            }

            // --- Jungle Camp Timers (track by NetId) ---
            if (cfg.drawJungleTimer) {
                // Mark all tracked camps as "not seen this frame"
                for (auto& [id, camp] : campTimers)
                    camp.seenThisFrame = false;

                // Scan current jungle monsters
                for (auto& mob : SDK::GameObjects::JungleMinions) {
                    if (!mob.IsValid()) continue;
                    float maxhp = mob.GetMaxHealth();
                    if (maxhp < 500.0f) continue; // skip small raptors etc

                    int netId = mob.GetNetId();
                    if (netId == 0) continue;

                    auto& camp = campTimers[netId];
                    camp.position = mob.GetPosition();
                    camp.maxHP = maxhp;
                    camp.seenThisFrame = true;

                    if (mob.IsAlive() && mob.GetHealth() > 0) {
                        camp.alive = true;
                        // Guess name for respawn time
                        if (camp.name.empty()) {
                            camp.name = mob.GetChampionName();
                            if (camp.name.empty()) camp.name = mob.GetName();
                        }
                    }
                }

                // Detect deaths: was alive, not seen this frame or HP=0
                for (auto& [id, camp] : campTimers) {
                    if (camp.alive && !camp.seenThisFrame) {
                        camp.alive = false;
                        camp.deathTime = now;
                    }
                }
            }
        }

        // ====================================================================
        // OnRender — Draw everything
        // ====================================================================
        void OnRender() override {
            if (!SDK::GameObjects::Player.IsValid()) return;

            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            if (!dl) return;

            auto& me = SDK::GameObjects::Player;

            // === DEBUG overlay ===
            if (cfg.showDebug) {
                float dy = 100;
                char buf[256];
                snprintf(buf, sizeof(buf), "[AW] Enemies:%d Allies:%d Turrets:%d Jungle:%d Minions:%d",
                    (int)SDK::GameObjects::EnemyHeroes.size(),
                    (int)SDK::GameObjects::AllyHeroes.size(),
                    (int)SDK::GameObjects::Turrets.size(),
                    (int)SDK::GameObjects::JungleMinions.size(),
                    (int)SDK::GameObjects::AllMinions.size());
                dl->AddText(ImVec2(10, dy), IM_COL32(255, 255, 100, 255), buf);
                dy += 15;

                for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                    Vec3 pos = hero.GetPosition();
                    Vec2 scr;
                    bool w2s = SDK::Drawing::WorldToScreen(pos, scr);
                    snprintf(buf, sizeof(buf),
                        "  E: alive=%d vis=%d HP=%.0f pos=(%.0f,%.0f,%.0f) w2s=%d",
                        hero.IsAlive(), hero.IsVisible(),
                        hero.GetHealth(), pos.x, pos.y, pos.z, w2s);
                    dl->AddText(ImVec2(10, dy), IM_COL32(255, 200, 200, 255), buf);
                    dy += 15;
                }
            }

            // === Self Attack Range ===
            if (cfg.drawSelfRange)
                SDK::Drawing::DrawCircle(me.GetPosition(), me.GetRealAttackRange(),
                    IM_COL32(0, 255, 0, 80), 1.5f);

            // === Enemies ===
            DrawEnemies(dl, me);

            // === Allies ===
            DrawAllies(dl, me);

            // === Turret Range ===
            if (cfg.drawTurretRange) {
                for (auto& turret : SDK::GameObjects::Turrets) {
                    if (!turret.IsValid() || !turret.IsAlive()) continue;
                    if (turret.IsEnemy(me)) {
                        SDK::Drawing::DrawCircle(turret.GetPosition(), 875.0f,
                            IM_COL32(255, 50, 50, 80), 2.0f, 48);
                    } else {
                        // Ally turret — subtle blue
                        if (cfg.drawAllyTurretRange) {
                            SDK::Drawing::DrawCircle(turret.GetPosition(), 875.0f,
                                IM_COL32(50, 150, 255, 40), 1.0f, 48);
                        }
                    }
                }
            }

            // === Jungle Camp HP ===
            DrawJungle(dl);

            // === Jungle Respawn Timers (on Minimap) ===
            if (cfg.drawJungleTimer) {
                DrawJungleTimersOnMinimap(dl);
            }
        }

        // ====================================================================
        // Menu
        // ====================================================================
        void OnMenu() override {
            if (ImGui::TreeNode("Self")) {
                ImGui::Checkbox("Attack Range", &cfg.drawSelfRange);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Enemies")) {
                ImGui::Checkbox("HP/Mana Bars", &cfg.drawEnemyHP);
                ImGui::Checkbox("Spell Cooldowns", &cfg.drawEnemySpells);
                ImGui::Checkbox("Attack Range", &cfg.drawEnemyRange);
                ImGui::Checkbox("Movement Path", &cfg.drawEnemyPath);
                ImGui::Checkbox("Last Position (FOW)", &cfg.drawLastPosition);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Allies")) {
                ImGui::Checkbox("HP Bars##ally", &cfg.drawAllyHP);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("World")) {
                ImGui::Checkbox("Enemy Turret Range", &cfg.drawTurretRange);
                ImGui::Checkbox("Ally Turret Range", &cfg.drawAllyTurretRange);
                ImGui::Checkbox("Jungle Camp HP", &cfg.drawJungleHP);
                ImGui::Checkbox("Jungle Respawn Timer (Minimap)", &cfg.drawJungleTimer);
                if (cfg.drawJungleTimer) {
                    ImGui::SliderFloat("Minimap Scale", &cfg.minimapScale, 0.5f, 2.0f, "%.2f");
                    ImGui::SliderFloat("Minimap X Offset", &cfg.minimapOffsetX, -200.0f, 200.0f, "%.0f");
                    ImGui::SliderFloat("Minimap Y Offset", &cfg.minimapOffsetY, -200.0f, 200.0f, "%.0f");
                }
                ImGui::TreePop();
            }
            ImGui::Separator();
            ImGui::Checkbox("Show Debug Info", &cfg.showDebug);
        }

    private:
        // ====================================================================
        // Config
        // ====================================================================
        struct {
            bool drawSelfRange      = true;
            bool drawEnemyHP        = true;
            bool drawEnemySpells    = true;
            bool drawEnemyRange     = true;
            bool drawEnemyPath      = true;
            bool drawLastPosition   = true;
            bool drawAllyHP         = false;
            bool drawTurretRange    = true;
            bool drawAllyTurretRange = false;
            bool drawJungleHP       = true;
            bool drawJungleTimer    = true;
            bool showDebug          = true;

            // Minimap config (adjustable via menu)
            float minimapScale      = 1.0f;     // HUD scale multiplier
            float minimapOffsetX    = 0.0f;     // manual X adjust
            float minimapOffsetY    = 0.0f;     // manual Y adjust
        } cfg;

        // ====================================================================
        // FOW Tracking
        // ====================================================================
        struct LastSeenInfo {
            Vec3 position;
            float time = 0;
            bool wasVisible = false;
        };
        LastSeenInfo lastSeen[16] = {};

        // ====================================================================
        // Jungle Camp Timer — tracked by NetId
        // ====================================================================
        struct CampInfo {
            Vec3 position;
            std::string name;
            float maxHP = 0;
            float deathTime = 0;
            bool alive = true;
            bool seenThisFrame = false;
        };
        std::unordered_map<int, CampInfo> campTimers;

        // Respawn time based on monster name
        static float GetRespawnByName(const std::string& name) {
            if (name.find("Baron") != std::string::npos)   return 360.0f;
            if (name.find("Dragon") != std::string::npos)  return 300.0f;
            if (name.find("Herald") != std::string::npos)  return 360.0f;
            if (name.find("Horde") != std::string::npos)   return 120.0f;
            if (name.find("Blue") != std::string::npos)    return 300.0f;  // SRU_Blue
            if (name.find("Red") != std::string::npos)     return 300.0f;  // SRU_Red
            if (name.find("Crab") != std::string::npos)    return 120.0f;
            if (name.find("Gromp") != std::string::npos)   return 120.0f;
            if (name.find("Murkwolf") != std::string::npos) return 120.0f;
            if (name.find("Razorbeak") != std::string::npos) return 120.0f;
            if (name.find("Krug") != std::string::npos)    return 120.0f;
            return 120.0f; // default
        }

        // ====================================================================
        // Draw Enemies
        // ====================================================================
        void DrawEnemies(ImDrawList* dl, SDK::GameObject& me) {
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;

                // Update FOW tracking
                int fowIdx = hero.GetNetId() & 0xF;
                auto& fow = lastSeen[fowIdx];

                if (hero.IsVisible()) {
                    fow.position = hero.GetPosition();
                    fow.time = SDK::Game::GetTime();
                    fow.wasVisible = true;

                    Vec3 pos = hero.GetPosition();
                    Vec2 scr;
                    if (!SDK::Drawing::WorldToScreen(pos, scr)) continue;

                    // Name + Level
                    {
                        char buf[64];
                        std::string name = hero.GetChampionName();
                        if (name.empty()) name = hero.GetName();
                        snprintf(buf, sizeof(buf), "%s [%d]", name.c_str(), hero.GetLevel());
                        ImVec2 ts = ImGui::CalcTextSize(buf);
                        dl->AddText(ImVec2(scr.x - ts.x / 2, scr.y - 55),
                            IM_COL32(255, 60, 60, 255), buf);
                    }

                    // HP Bar
                    if (cfg.drawEnemyHP) {
                        float hp = hero.GetHealth();
                        float maxhp = hero.GetMaxHealth();
                        if (maxhp > 0) {
                            float pct = hp / maxhp;
                            float barW = 80.0f, barH = 6.0f;
                            float bx = scr.x - barW / 2, by = scr.y - 42;
                            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW, by + barH),
                                IM_COL32(0, 0, 0, 180));
                            ImU32 hpCol = pct > 0.5f ? IM_COL32(0, 200, 0, 255) :
                                          pct > 0.25f ? IM_COL32(255, 200, 0, 255) :
                                                        IM_COL32(255, 50, 50, 255);
                            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW * pct, by + barH), hpCol);
                            dl->AddRect(ImVec2(bx, by), ImVec2(bx + barW, by + barH),
                                IM_COL32(255, 255, 255, 80));

                            char hpBuf[32];
                            snprintf(hpBuf, sizeof(hpBuf), "%.0f", hp);
                            ImVec2 hts = ImGui::CalcTextSize(hpBuf);
                            dl->AddText(ImVec2(scr.x - hts.x / 2, by - 13),
                                IM_COL32(255, 255, 255, 200), hpBuf);
                        }

                        // Mana Bar
                        float mp = hero.GetMana();
                        float maxmp = hero.GetMaxMana();
                        if (maxmp > 0) {
                            float pct = mp / maxmp;
                            float barW = 80.0f, barH = 3.0f;
                            float bx = scr.x - barW / 2, by = scr.y - 35;
                            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW, by + barH),
                                IM_COL32(0, 0, 0, 150));
                            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW * pct, by + barH),
                                IM_COL32(50, 100, 255, 200));
                        }
                    }

                    // Spell Cooldowns — visual box style
                    if (cfg.drawEnemySpells) {
                        SDK::SpellBook sb(hero.address);
                        if (sb.IsValid()) {
                            const char* slotNames[] = { "Q", "W", "E", "R" };
                            const float boxW = 22.0f, boxH = 22.0f, gap = 2.0f;
                            float totalW = 4 * boxW + 3 * gap;
                            float startX = scr.x - totalW / 2;
                            float spY = scr.y - 30;

                            for (int i = 0; i < 4; i++) {
                                SDK::SpellSlot spell = sb.GetSpell((SDK::SpellSlotId)i);
                                float bx = startX + i * (boxW + gap);
                                ImVec2 tl(bx, spY);
                                ImVec2 br(bx + boxW, spY + boxH);

                                int level = spell.IsValid() ? spell.GetLevel() : 0;

                                if (level <= 0) {
                                    // Not learned — dark gray box
                                    dl->AddRectFilled(tl, br, IM_COL32(30, 30, 30, 180));
                                    ImVec2 ts = ImGui::CalcTextSize(slotNames[i]);
                                    dl->AddText(ImVec2(bx + (boxW - ts.x) / 2, spY + (boxH - ts.y) / 2),
                                        IM_COL32(80, 80, 80, 150), slotNames[i]);
                                } else {
                                    float cd = spell.GetRemainingCooldown();
                                    float totalCd = spell.GetTotalCooldown();

                                    if (cd <= 0.0f) {
                                        // Ready — green box
                                        dl->AddRectFilled(tl, br, IM_COL32(20, 80, 20, 200));
                                        ImVec2 ts = ImGui::CalcTextSize(slotNames[i]);
                                        dl->AddText(ImVec2(bx + (boxW - ts.x) / 2, spY + (boxH - ts.y) / 2),
                                            IM_COL32(50, 255, 50, 255), slotNames[i]);
                                    } else {
                                        // On cooldown — dark bg + progress overlay
                                        dl->AddRectFilled(tl, br, IM_COL32(40, 10, 10, 220));

                                        // CD progress fill (bottom-up)
                                        float pct = (totalCd > 0.01f) ? (cd / totalCd) : 1.0f;
                                        if (pct > 1.0f) pct = 1.0f;
                                        float fillH = boxH * pct;
                                        dl->AddRectFilled(
                                            ImVec2(bx, spY + boxH - fillH),
                                            br,
                                            IM_COL32(120, 20, 20, 180));

                                        // CD timer text (centered in box)
                                        char cdBuf[8];
                                        if (cd >= 100.0f)
                                            snprintf(cdBuf, sizeof(cdBuf), "%d", (int)cd);
                                        else if (cd >= 10.0f)
                                            snprintf(cdBuf, sizeof(cdBuf), "%.0f", cd);
                                        else
                                            snprintf(cdBuf, sizeof(cdBuf), "%.1f", cd);

                                        ImU32 cdCol = cd <= 3.0f ? IM_COL32(255, 255, 100, 255) :
                                                                    IM_COL32(255, 200, 200, 255);
                                        ImVec2 ts = ImGui::CalcTextSize(cdBuf);
                                        dl->AddText(ImVec2(bx + (boxW - ts.x) / 2, spY + (boxH - ts.y) / 2),
                                            cdCol, cdBuf);
                                    }
                                }
                                // Border
                                dl->AddRect(tl, br, IM_COL32(255, 255, 255, 60));
                            }
                        }
                    }

                    // Attack Range circle
                    if (cfg.drawEnemyRange) {
                        float range = hero.GetAttackRange() + hero.GetBoundingRadius();
                        SDK::Drawing::DrawCircle(hero.GetPosition(), range,
                            IM_COL32(255, 50, 50, 120), 1.5f);
                    }

                    // Movement Path
                    if (cfg.drawEnemyPath) {
                        SDK::AiManager ai(hero.address);
                        if (ai.IsValid() && ai.IsMoving()) {
                            Vec3 target = ai.GetPathEnd();
                            if (!target.IsZero()) {
                                SDK::Drawing::DrawLine(pos, target,
                                    IM_COL32(255, 255, 100, 120), 1.5f);
                            }
                        }
                    }

                } else {
                    // --- FOW: Draw last known position ---
                    if (cfg.drawLastPosition && fow.wasVisible && !fow.position.IsZero()) {
                        float elapsed = SDK::Game::GetTime() - fow.time;
                        if (elapsed < 90.0f && elapsed > 0.5f) {
                            int alpha = (int)(200.0f * (1.0f - elapsed / 90.0f));
                            if (alpha > 10) {
                                Vec2 scr;
                                if (SDK::Drawing::WorldToScreen(fow.position, scr)) {
                                    char buf[64];
                                    std::string name = hero.GetChampionName();
                                    snprintf(buf, sizeof(buf), "? %s %.0fs", name.c_str(), elapsed);
                                    ImVec2 ts = ImGui::CalcTextSize(buf);
                                    dl->AddText(ImVec2(scr.x - ts.x / 2, scr.y - 10),
                                        IM_COL32(255, 100, 100, alpha), buf);
                                }
                            }
                        }
                    }
                }
            }
        }

        // ====================================================================
        // Draw Allies
        // ====================================================================
        void DrawAllies(ImDrawList* dl, SDK::GameObject& me) {
            for (auto& hero : SDK::GameObjects::AllyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
                if (hero.address == me.address) continue;

                Vec2 scr;
                if (!SDK::Drawing::WorldToScreen(hero.GetPosition(), scr)) continue;

                if (cfg.drawAllyHP) {
                    float hp = hero.GetHealth(), maxhp = hero.GetMaxHealth();
                    if (maxhp > 0) {
                        float pct = hp / maxhp;
                        float barW = 60.0f, barH = 4.0f;
                        float bx = scr.x - barW / 2, by = scr.y - 35;
                        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW, by + barH),
                            IM_COL32(0, 0, 0, 150));
                        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW * pct, by + barH),
                            IM_COL32(0, 180, 0, 200));
                    }
                }
            }
        }

        // ====================================================================
        // Draw Jungle Camp HP
        // ====================================================================
        void DrawJungle(ImDrawList* dl) {
            if (!cfg.drawJungleHP) return;

            for (auto& mob : SDK::GameObjects::JungleMinions) {
                if (!mob.IsValid() || !mob.IsAlive() || !mob.IsVisible()) continue;
                if (mob.GetMaxHealth() <= 500.0f) continue;

                Vec2 scr;
                if (!SDK::Drawing::WorldToScreen(mob.GetPosition(), scr)) continue;

                float hp = mob.GetHealth(), maxhp = mob.GetMaxHealth();
                float pct = hp / maxhp;
                float barW = 60, barH = 5;
                float bx = scr.x - barW / 2, by = scr.y - 20;
                dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW, by + barH),
                    IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW * pct, by + barH),
                    IM_COL32(255, 200, 0, 200));
                char hpBuf[16];
                snprintf(hpBuf, sizeof(hpBuf), "%.0f", hp);
                dl->AddText(ImVec2(scr.x - 15, by - 13), IM_COL32(255, 255, 200, 200), hpBuf);
            }
        }

        // ====================================================================
        // Minimap Coordinate Conversion
        // ====================================================================

        // Get minimap screen rect (calculated from screen resolution)
        struct MinimapRect {
            float x, y, w, h;
        };

        MinimapRect GetMinimapRect() const {
            ImVec2 display = ImGui::GetIO().DisplaySize;
            float screenW = display.x;
            float screenH = display.y;

            // LoL minimap is always bottom-right
            // Default proportions (from HUD scale 1.0):
            //   Size ≈ 13.9% of screen width
            //   Right margin ≈ 0px
            //   Bottom margin ≈ 0px (sits above taskbar strip)
            float baseSize = screenW * 0.139f * cfg.minimapScale;

            // Position: bottom-right corner
            float mmX = screenW - baseSize + cfg.minimapOffsetX;
            float mmY = screenH - baseSize + cfg.minimapOffsetY;

            return { mmX, mmY, baseSize, baseSize };
        }

        // Convert world position (X, Z) to minimap screen coordinates
        bool WorldToMinimap(const Vec3& world, ImVec2& out) const {
            MinimapRect mm = GetMinimapRect();

            // Summoner's Rift world bounds
            const float mapMinX = -120.0f;
            const float mapMinZ = -120.0f;
            const float mapMaxX = 14870.0f;
            const float mapMaxZ = 14980.0f;
            const float mapRangeX = mapMaxX - mapMinX; // ~14990
            const float mapRangeZ = mapMaxZ - mapMinZ; // ~15100

            float normX = (world.x - mapMinX) / mapRangeX;
            float normZ = (world.z - mapMinZ) / mapRangeZ;

            // Clamp to [0,1]
            if (normX < 0.0f) normX = 0.0f; if (normX > 1.0f) normX = 1.0f;
            if (normZ < 0.0f) normZ = 0.0f; if (normZ > 1.0f) normZ = 1.0f;

            // Minimap: X goes left-to-right, Z is inverted (top=max, bottom=min)
            out.x = mm.x + normX * mm.w;
            out.y = mm.y + (1.0f - normZ) * mm.h;
            return true;
        }

        // ====================================================================
        // Draw Jungle Respawn Timers ON MINIMAP
        // ====================================================================
        void DrawJungleTimersOnMinimap(ImDrawList* dl) {
            float now = SDK::Game::GetTime();

            for (auto& [id, camp] : campTimers) {
                if (camp.alive || camp.position.IsZero() || camp.deathTime <= 0) continue;
                float elapsed = now - camp.deathTime;
                float respawn = GetRespawnByName(camp.name);
                float remaining = respawn - elapsed;
                if (remaining <= 0.0f || remaining > 600.0f) continue;

                // Convert camp world position to minimap screen coords
                ImVec2 mmPos;
                if (!WorldToMinimap(camp.position, mmPos)) continue;

                // Format timer text
                char buf[16];
                int mins = (int)remaining / 60;
                int secs = (int)remaining % 60;
                if (mins > 0)
                    snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
                else
                    snprintf(buf, sizeof(buf), "%ds", secs);

                // Timer color: red when <15s, yellow otherwise
                ImU32 textCol = remaining < 15.0f
                    ? IM_COL32(255, 80, 30, 255)
                    : IM_COL32(255, 255, 80, 255);

                // Draw background circle + text
                ImVec2 ts = ImGui::CalcTextSize(buf);
                float radius = (ts.x > ts.y ? ts.x : ts.y) / 2.0f + 3.0f;

                dl->AddCircleFilled(mmPos, radius, IM_COL32(0, 0, 0, 180));
                dl->AddText(ImVec2(mmPos.x - ts.x / 2, mmPos.y - ts.y / 2),
                    textCol, buf);
            }
        }
    };

} // namespace Plugins
