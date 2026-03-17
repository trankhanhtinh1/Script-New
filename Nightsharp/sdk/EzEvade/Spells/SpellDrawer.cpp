#include "SpellDrawer.h"
#include "SpellDetector.h"
#include "../Core/Evade.h"
#include "../../UI/Drawing.h"

// ImGui
#include "../../../imgui/imgui.h"

namespace EzEvade {

    // =========================================================================
    // Helper: convert DrawColor to ImU32
    // =========================================================================
    static ImU32 ToImU32(const SpellDrawer::DrawColor& c) {
        return IM_COL32(c.r, c.g, c.b, c.a);
    }

    // =========================================================================
    // Initialize (C# line 23-29)
    // =========================================================================
    void SpellDrawer::Initialize()
    {
        // No menu setup needed — configs stored in static members
    }

    // =========================================================================
    // GetDangerConfig (replaces C# menu lookups line 203-204)
    // =========================================================================
    SpellDrawer::DangerDrawConfig SpellDrawer::GetDangerConfig(const std::string& dangerStr)
    {
        if (dangerStr == "Low")      return lowDanger;                              // C# line 42-44
        if (dangerStr == "Normal")   return normalDanger;                           // C# line 46-48
        if (dangerStr == "High")     return highDanger;                             // C# line 50-52
        if (dangerStr == "Extreme")  return extremeDanger;                          // C# line 54-56
        return normalDanger;
    }

    // =========================================================================
    // WorldToScreen wrapper (C# Drawing.WorldToScreen)
    // =========================================================================
    bool SpellDrawer::WorldToScreen(const Vec3& world, ImVec2& screen)
    {
        Vec2 s;
        if (SDK::Drawing::WorldToScreen(world, s)) {
            screen = ImVec2(s.x, s.y);
            return true;
        }
        return false;
    }

    // =========================================================================
    // DrawLineRectangle (C# line 73-92)
    // Draw a rectangle from start to end with given radius (half-width)
    // =========================================================================
    void SpellDrawer::DrawLineRectangle(const Vec2& start, const Vec2& end,
        int radius, int width, const DrawColor& color)
    {
        Vec2 dir = (end - start).Normalized();                                      // C# line 75
        Vec2 pDir = dir.Perpendicular();                                            // C# line 76

        Vec2 rightStartPos = start + pDir * (float)radius;                          // C# line 78
        Vec2 leftStartPos = start - pDir * (float)radius;                           // C# line 79
        Vec2 rightEndPos = end + pDir * (float)radius;                              // C# line 80
        Vec2 leftEndPos = end - pDir * (float)radius;                               // C# line 81

        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return;
        float h = myHero.GetPosition().y;

        // WorldToScreen conversions (C# line 83-86)
        ImVec2 rStart, lStart, rEnd, lEnd;
        bool ok = true;
        ok &= WorldToScreen(Vec3(rightStartPos.x, h, rightStartPos.y), rStart);    // C# line 83
        ok &= WorldToScreen(Vec3(leftStartPos.x, h, leftStartPos.y), lStart);      // C# line 84
        ok &= WorldToScreen(Vec3(rightEndPos.x, h, rightEndPos.y), rEnd);           // C# line 85
        ok &= WorldToScreen(Vec3(leftEndPos.x, h, leftEndPos.y), lEnd);             // C# line 86

        if (!ok) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        ImU32 col = ToImU32(color);
        float th = (float)width;

        draw->AddLine(rStart, rEnd, col, th);                                       // C# line 88
        draw->AddLine(lStart, lEnd, col, th);                                       // C# line 89
        draw->AddLine(rStart, lStart, col, th);                                     // C# line 90
        draw->AddLine(lEnd, rEnd, col, th);                                         // C# line 91
    }

    // =========================================================================
    // DrawLineTriangle (C# line 94-110)
    // Draw a triangle from start to end with given radius
    // =========================================================================
    void SpellDrawer::DrawLineTriangle(const Vec2& start, const Vec2& end,
        int radius, int width, const DrawColor& color)
    {
        Vec2 dir = (end - start).Normalized();                                      // C# line 96
        Vec2 pDir = dir.Perpendicular();                                            // C# line 97

        Vec2 initStartPos = start + dir;                                            // C# line 99
        Vec2 rightEndPos = end + pDir * (float)radius;                              // C# line 100
        Vec2 leftEndPos = end - pDir * (float)radius;                               // C# line 101

        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return;
        float h = myHero.GetPosition().y;

        ImVec2 iStart, rEnd, lEnd;
        bool ok = true;
        ok &= WorldToScreen(Vec3(initStartPos.x, h, initStartPos.y), iStart);      // C# line 103
        ok &= WorldToScreen(Vec3(rightEndPos.x, h, rightEndPos.y), rEnd);           // C# line 104
        ok &= WorldToScreen(Vec3(leftEndPos.x, h, leftEndPos.y), lEnd);             // C# line 105

        if (!ok) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        ImU32 col = ToImU32(color);
        float th = (float)width;

        draw->AddLine(iStart, rEnd, col, th);                                       // C# line 107
        draw->AddLine(iStart, lEnd, col, th);                                       // C# line 108
        draw->AddLine(rEnd, lEnd, col, th);                                         // C# line 109
    }

    // =========================================================================
    // DrawCircle3D — draw a circle at world position
    // Uses SDK::Drawing::DrawCircle
    // =========================================================================
    void SpellDrawer::DrawCircle3D(const Vec3& center, float radius,
        const DrawColor& color, int width, int segments)
    {
        SDK::Drawing::DrawCircle(center, radius, ToImU32(color), (float)width, segments);
    }

    // =========================================================================
    // DrawEvadeStatus (C# line 112-170)
    // Draw "Evade: ON/OFF" text near hero
    // =========================================================================
    void SpellDrawer::DrawEvadeStatus()
    {
        if (!ObjectCache::GetBool("ShowStatus"))                                    // C# line 114
            return;

        auto myHero = SDK::ObjectManager::GetLocalPlayer();                                       // C# line 116
        if (!myHero.IsValid()) return;

        ImVec2 heroPos;
        if (!WorldToScreen(myHero.GetPosition(), heroPos)) return;                 // C# line 116

        const char* text = "Evade: ON";
        ImVec2 textSize = ImGui::CalcTextSize(text);

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        float drawX = heroPos.x - textSize.x / 2;                                  // C# line 117: centered
        float drawY = heroPos.y;

        if (ObjectCache::GetBool("DodgeSkillShots"))                                // C# line 119
        {
            if (Evade::isDodging)                                                   // C# line 121
            {
                draw->AddText(ImVec2(drawX, drawY),
                    IM_COL32(255, 0, 0, 255), "Evade: ON");                         // C# line 123: Red
            }
            else
            {
                if (ObjectCache::GetBool("DodgeOnlyOnComboKeyEnabled") &&
                    !ObjectCache::GetBool("DodgeComboKey"))                          // C# line 127-128
                {
                    draw->AddText(ImVec2(drawX, drawY),
                        IM_COL32(128, 128, 128, 255), "Evade: OFF");                // C# line 130: Gray
                }
                else
                {
                    if (ObjectCache::GetBool("DontDodgeKeyEnabled") &&
                        ObjectCache::GetBool("DontDodgeKey"))                        // C# line 134-135
                    {
                        draw->AddText(ImVec2(drawX, drawY),
                            IM_COL32(128, 128, 128, 255), "Evade: OFF");            // C# line 136: Gray
                    }
                    else if (Evade::IsDodgeDangerousEnabled())                       // C# line 137
                    {
                        draw->AddText(ImVec2(drawX, drawY),
                            IM_COL32(255, 255, 0, 255), "Evade: ON");               // C# line 138: Yellow
                    }
                    else
                    {
                        draw->AddText(ImVec2(drawX, drawY),
                            IM_COL32(0, 255, 0, 255), "Evade: ON");                 // C# line 140: Lime
                    }
                }
            }
        }
        else                                                                        // C# line 144
        {
            if (ObjectCache::GetBool("ActivateEvadeSpells"))                         // C# line 146
            {
                if (ObjectCache::GetBool("DodgeOnlyOnComboKeyEnabled") &&
                    !ObjectCache::GetBool("DodgeComboKey"))                          // C# line 148-149
                {
                    draw->AddText(ImVec2(drawX, drawY),
                        IM_COL32(128, 128, 128, 255), "Evade: OFF");                // C# line 151: Gray
                }
                else
                {
                    if (Evade::IsDodgeDangerousEnabled())                             // C# line 155
                    {
                        draw->AddText(ImVec2(drawX, drawY),
                            IM_COL32(255, 255, 0, 255), "Evade: Spell");            // C# line 156: Yellow
                    }
                    else
                    {
                        draw->AddText(ImVec2(drawX, drawY),
                            IM_COL32(0, 191, 255, 255), "Evade: Spell");            // C# line 158: DeepSkyBlue
                    }
                }
            }
            else                                                                    // C# line 161
            {
                draw->AddText(ImVec2(drawX, drawY),
                    IM_COL32(128, 128, 128, 255), "Evade: OFF");                    // C# line 163: Gray
            }
        }
    }

    // =========================================================================
    // OnDraw (C# line 172-257)
    // Main drawing callback — draws all active spell indicators
    // =========================================================================
    void SpellDrawer::OnDraw()
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return;

        // Draw evade position (C# line 175-189)
        if (ObjectCache::GetBool("DrawEvadePosition"))                              // C# line 175
        {
            if (Evade::lastPosInfo != nullptr)                                      // C# line 184
            {
                Vec2 pos = Evade::lastPosInfo->position;                            // C# line 186
                Vec3 center(pos.x, myHero.GetPosition().y, pos.y);
                DrawCircle3D(center, 65,
                    DrawColor(255, 0, 0, 255), 3);                                  // C# line 187: Red circle
            }
        }

        DrawEvadeStatus();                                                          // C# line 191

        if (!ObjectCache::GetBool("DrawSkillShots"))                                // C# line 193
            return;                                                                 // C# line 195

        for (auto& entry : SpellDetector::drawSpells)                               // C# line 198
        {
            Spell& spell = entry.second;                                            // C# line 200

            std::string dangerStr = spell.GetSpellDangerString();                   // C# line 202
            DangerDrawConfig config = GetDangerConfig(dangerStr);                   // C# line 203
            int spellDrawingWidth = config.width;                                   // C# line 204
            // int avoidRadius = ObjectCache::GetSlider("ExtraAvoidDistance");      // C# line 205 (unused in draw)

            if (ObjectCache::GetBool(spell.info.spellName + "DrawSpell") &&
                config.active)                                                      // C# line 207-208
            {
                // canEvade check (C# line 211)
                bool canEvade = !(Evade::lastPosInfo != nullptr &&
                    std::find(Evade::lastPosInfo->undodgeableSpells.begin(),
                              Evade::lastPosInfo->undodgeableSpells.end(),
                              spell.spellID) != Evade::lastPosInfo->undodgeableSpells.end())
                    || !Evade::devModeOn;                                            // C# line 211

                DrawColor drawColor = canEvade ? config.color : undodgeableColor;    // yellow if undodgeable

                if (spell.spellType == SpellType::Line)                             // C# line 213
                {
                    Vec2 spellPos = spell.currentSpellPosition;                     // C# line 215
                    Vec2 spellEndPos = spell.GetSpellEndPosition();                 // C# line 216

                    DrawLineRectangle(spellPos, spellEndPos,
                        (int)spell.radius, spellDrawingWidth, drawColor);           // C# line 218-219

                    if (ObjectCache::GetBool("DrawSpellPos"))                       // C# line 221
                    {
                        Vec3 center(spellPos.x, spell.height, spellPos.y);
                        DrawCircle3D(center, spell.radius, drawColor,
                            spellDrawingWidth);                                     // C# line 223
                    }
                }
                else if (spell.spellType == SpellType::Circular)                    // C# line 227
                {
                    Vec3 center(spell.endPos.x, spell.height, spell.endPos.y);
                    DrawCircle3D(center, spell.radius, drawColor,
                        spellDrawingWidth);                                         // C# line 229

                    // VeigarEventHorizon inner ring (C# line 231-234)
                    if (spell.info.spellName == "VeigarEventHorizon")               // C# line 231
                    {
                        DrawCircle3D(center, spell.radius - 125, drawColor,
                            spellDrawingWidth);                                     // C# line 233
                    }
                    // DariusCleave inner ring (C# line 235-238)
                    else if (spell.info.spellName == "DariusCleave")                // C# line 235
                    {
                        DrawCircle3D(center, spell.radius - 220, drawColor,
                            spellDrawingWidth);                                     // C# line 237
                    }
                }
                else if (spell.spellType == SpellType::Arc)                         // C# line 240
                {
                    // Arc drawing — commented out in original C# (line 242-249)
                    // Left as no-op matching original behavior
                }
                else if (spell.spellType == SpellType::Cone)                        // C# line 251
                {
                    DrawLineTriangle(spell.startPos, spell.endPos,
                        (int)spell.radius, spellDrawingWidth, drawColor);           // C# line 253
                }
            }
        }
    }

} // namespace EzEvade
