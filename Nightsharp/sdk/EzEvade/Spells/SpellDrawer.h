#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Helpers/EvadeContext.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/EzEvade/Spells/SpellDetector.h"

namespace EzEvade {

class SpellDrawer {
public:
    static inline std::shared_ptr<SDK::MenuUI::Menu> MenuRoot = nullptr;
    static inline std::shared_ptr<SDK::MenuUI::Menu> DrawMenu = nullptr;

    explicit SpellDrawer(const std::shared_ptr<SDK::MenuUI::Menu>& mainMenu) {
        MenuRoot = mainMenu;
        BuildMenu();
    }

    static void Draw() {
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return;

        DrawEvadeStatus();

        if (!ObjectCache::Menu.GetBool("DrawSkillShots", true)) {
            return;
        }

        const int avoidRadius = ObjectCache::Menu.GetSlider("ExtraAvoidDistance", 50);
        (void)avoidRadius;

        for (auto& [id, spell] : SpellDetector::DrawSpells) {
            (void)id;
            if (!spell.Info) continue;

            const std::string drawKey = spell.Info->spellName + "DrawSpell";
            if (!ObjectCache::Menu.GetBool(drawKey, true)) {
                continue;
            }

            const std::string danger = SpellExtensions::GetSpellDangerString(spell);
            const auto width = ObjectCache::Menu.GetSlider(danger + "Width", danger == "Extreme" ? 4 : 3);

            auto* colorItem = dynamic_cast<SDK::MenuUI::MenuColor*>(ObjectCache::Menu.Get(danger + "Color"));
            const ImU32 color = colorItem ? colorItem->GetImU32() : IM_COL32(255, 255, 255, 180);

            bool canEvade = true;
            if (EvadeContext::HasLastPosInfo && std::find(EvadeContext::LastPosInfo.UndodgeableSpells.begin(),
                EvadeContext::LastPosInfo.UndodgeableSpells.end(), spell.SpellID) != EvadeContext::LastPosInfo.UndodgeableSpells.end()) {
                canEvade = false;
            }

            const ImU32 drawColor = canEvade ? color : IM_COL32(255, 255, 0, 255);

            if (spell.Type == SpellType::Line) {
                DrawLineRectangle(spell.CurrentSpellPosition, SpellExtensions::GetSpellEndPosition(spell), (int)spell.Radius, width, drawColor);
                if (ObjectCache::Menu.GetBool("DrawSpellPos", false)) {
                    SDK::Drawing::DrawCircle(Vec3::From2D(spell.CurrentSpellPosition, spell.Height), spell.Radius, drawColor, (float)width);
                }
            } else if (spell.Type == SpellType::Circular) {
                SDK::Drawing::DrawCircle(Vec3::From2D(spell.EndPos, spell.Height), spell.Radius, drawColor, (float)width);
                if (spell.Info->spellName == "VeigarEventHorizon") {
                    SDK::Drawing::DrawCircle(Vec3::From2D(spell.EndPos, spell.Height), spell.Radius - 125.0f, drawColor, (float)width);
                } else if (spell.Info->spellName == "DariusCleave") {
                    SDK::Drawing::DrawCircle(Vec3::From2D(spell.EndPos, spell.Height), spell.Radius - 220.0f, drawColor, (float)width);
                }
            } else if (spell.Type == SpellType::Cone) {
                DrawLineTriangle(spell.StartPos, spell.EndPos, (int)spell.Radius, width, drawColor);
            }
        }

        if (ObjectCache::Menu.GetBool("DrawEvadePosition", false) && EvadeContext::HasLastPosInfo) {
            SDK::Drawing::DrawCircle(
                Vec3::From2D(EvadeContext::LastPosInfo.Position, me.GetPosition().y),
                65.0f,
                IM_COL32(255, 0, 0, 255),
                2.5f);
        }
    }

private:
    static void BuildMenu() {
        if (!MenuRoot) return;

        DrawMenu = MenuRoot->AddSubMenu("Draw", "Draw");
        DrawMenu->Add<SDK::MenuUI::MenuBool>("DrawSkillShots", "Draw SkillShots", true);
        DrawMenu->Add<SDK::MenuUI::MenuBool>("ShowStatus", "Show Evade Status", true);
        DrawMenu->Add<SDK::MenuUI::MenuBool>("DrawSpellPos", "Draw Spell Position", false);
        DrawMenu->Add<SDK::MenuUI::MenuBool>("DrawEvadePosition", "Draw Evade Position", false);

        auto dangerMenu = DrawMenu->AddSubMenu("DangerLevelDrawings", "DangerLevel Drawings");
        AddDangerMenu(dangerMenu, "Low", 3, 255, 255, 255, 60);
        AddDangerMenu(dangerMenu, "Normal", 3, 255, 255, 255, 140);
        AddDangerMenu(dangerMenu, "High", 3, 255, 255, 255, 255);
        AddDangerMenu(dangerMenu, "Extreme", 4, 255, 255, 255, 255);

        ObjectCache::Menu.AddMenuToCache(DrawMenu);
    }

    static void AddDangerMenu(const std::shared_ptr<SDK::MenuUI::Menu>& dangerMenu,
                              const std::string& label, int width,
                              int r, int g, int b, int a) {
        auto sub = dangerMenu->AddSubMenu(label + "Drawing", label);
        sub->Add<SDK::MenuUI::MenuSlider>(label + "Width", "Line Width", width, 1, 15);
        sub->Add<SDK::MenuUI::MenuColor>(label + "Color", "Color",
            r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    static void DrawLineRectangle(const Vec2& start, const Vec2& end, int radius, int width, ImU32 color) {
        Vec2 dir = (end - start).Normalized();
        Vec2 pDir = dir.Perpendicular();

        Vec2 rightStartPos = start + pDir * (float)radius;
        Vec2 leftStartPos = start - pDir * (float)radius;
        Vec2 rightEndPos = end + pDir * (float)radius;
        Vec2 leftEndPos = end - pDir * (float)radius;

        SDK::Drawing::DrawLine(Vec3::From2D(rightStartPos, SDK::GameObjects::Player.GetPosition().y), Vec3::From2D(rightEndPos, SDK::GameObjects::Player.GetPosition().y), color, (float)width);
        SDK::Drawing::DrawLine(Vec3::From2D(leftStartPos, SDK::GameObjects::Player.GetPosition().y), Vec3::From2D(leftEndPos, SDK::GameObjects::Player.GetPosition().y), color, (float)width);
        SDK::Drawing::DrawLine(Vec3::From2D(rightStartPos, SDK::GameObjects::Player.GetPosition().y), Vec3::From2D(leftStartPos, SDK::GameObjects::Player.GetPosition().y), color, (float)width);
        SDK::Drawing::DrawLine(Vec3::From2D(leftEndPos, SDK::GameObjects::Player.GetPosition().y), Vec3::From2D(rightEndPos, SDK::GameObjects::Player.GetPosition().y), color, (float)width);
    }

    static void DrawLineTriangle(const Vec2& start, const Vec2& end, int radius, int width, ImU32 color) {
        Vec2 dir = (end - start).Normalized();
        Vec2 pDir = dir.Perpendicular();

        Vec2 initStartPos = start + dir;
        Vec2 rightEndPos = end + pDir * (float)radius;
        Vec2 leftEndPos = end - pDir * (float)radius;

        const float y = SDK::GameObjects::Player.GetPosition().y;
        SDK::Drawing::DrawLine(Vec3::From2D(initStartPos, y), Vec3::From2D(rightEndPos, y), color, (float)width);
        SDK::Drawing::DrawLine(Vec3::From2D(initStartPos, y), Vec3::From2D(leftEndPos, y), color, (float)width);
        SDK::Drawing::DrawLine(Vec3::From2D(rightEndPos, y), Vec3::From2D(leftEndPos, y), color, (float)width);
    }

    static void DrawEvadeStatus() {
        if (!ObjectCache::Menu.GetBool("ShowStatus", true)) {
            return;
        }

        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return;

        const bool dodgeSkillShots = ObjectCache::Menu.GetKey("DodgeSkillShots", true);
        const bool evadeSpellOnly = ObjectCache::Menu.GetKey("ActivateEvadeSpells", true);
        const bool comboOnlyEnabled = ObjectCache::Menu.GetBool("DodgeOnlyOnComboKeyEnabled", false);
        const bool comboKeyActive = ObjectCache::Menu.GetKey("DodgeComboKey", false);
        const bool dontDodgeEnabled = ObjectCache::Menu.GetBool("DontDodgeKeyEnabled", false);
        const bool dontDodgeActive = ObjectCache::Menu.GetKey("DontDodgeKey", false);

        std::string text = "Evade: OFF";
        ImU32 color = IM_COL32(150, 150, 150, 255);

        if (dodgeSkillShots) {
            if (EvadeContext::IsDodging) {
                text = "Evade: ON";
                color = IM_COL32(255, 64, 64, 255);
            } else if (comboOnlyEnabled && !comboKeyActive) {
                text = "Evade: OFF";
                color = IM_COL32(150, 150, 150, 255);
            } else if (dontDodgeEnabled && dontDodgeActive) {
                text = "Evade: OFF";
                color = IM_COL32(150, 150, 150, 255);
            } else if (Situation::IsDodgeDangerousEnabled()) {
                text = "Evade: ON";
                color = IM_COL32(255, 220, 64, 255);
            } else {
                text = "Evade: ON";
                color = IM_COL32(64, 255, 64, 255);
            }
        } else if (evadeSpellOnly) {
            if (comboOnlyEnabled && !comboKeyActive) {
                text = "Evade: OFF";
                color = IM_COL32(150, 150, 150, 255);
            } else if (Situation::IsDodgeDangerousEnabled()) {
                text = "Evade: Spell";
                color = IM_COL32(255, 220, 64, 255);
            } else {
                text = "Evade: Spell";
                color = IM_COL32(40, 180, 255, 255);
            }
        }

        text += " [A:" + std::to_string((int)SpellDetector::Spells.size()) +
                " D:" + std::to_string((int)SpellDetector::DrawSpells.size()) + "]";

        const Vec3 pos = me.GetPosition();
        SDK::Drawing::DrawTextCentered(Vec3(pos.x, pos.y + me.GetBoundingRadius() + 80.0f, pos.z), text.c_str(), color);
    }
};

} // namespace EzEvade
