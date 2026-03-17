#pragma once
// =========================================================================
// SpellDrawer.h — C++ port of EzEvade/Spells/SpellDrawer.cs (260 lines)
// Line-by-line, preserving original logic
// Drawing via ImGui (replaces LeagueSharp Drawing API)
// =========================================================================
#include <string>
#include <vector>
#include <cmath>

#include "Spell.h"
#include "SpellData.h"
#include "../Helpers/ObjectCache.h"
#include "../Utils/EvadeUtils.h"
#include "../../GameObjects/GameObjects.h"
#include "../../Game.h"

// Forward declare ImGui types
struct ImDrawList;
struct ImVec2;

namespace EzEvade {

    // Forward declaration
    namespace SpellDetector {
        extern std::map<int, Spell> drawSpells;
    }

    // =========================================================================
    // SpellDrawer — renders spell indicators on screen
    //   C# original: ezEvade.SpellDrawer (SpellDrawer.cs, 260 lines)
    //   Drawing API: ImGui overlay (replaces LeagueSharp Drawing)
    // =========================================================================
    class SpellDrawer {
    public:
        // =====================================================================
        // Color struct for drawing (replaces System.Drawing.Color)
        // =====================================================================
        struct DrawColor {
            int r, g, b, a;
            DrawColor() : r(255), g(255), b(255), a(255) {}
            DrawColor(int _r, int _g, int _b, int _a = 255) : r(_r), g(_g), b(_b), a(_a) {}
        };

        // Danger level colors (C# line 42-61)
        struct DangerDrawConfig {
            DrawColor color;
            int width;
            bool active;
        };

        // =====================================================================
        // Static drawing settings
        // =====================================================================
        static inline DangerDrawConfig lowDanger =
            {{255, 255, 255, 60}, 3, true};                                        // C# line 43-44

        static inline DangerDrawConfig normalDanger =
            {{255, 255, 255, 140}, 3, true};                                       // C# line 47-48

        static inline DangerDrawConfig highDanger =
            {{255, 255, 255, 255}, 3, true};                                       // C# line 51-52

        static inline DangerDrawConfig extremeDanger =
            {{255, 255, 255, 255}, 4, true};                                       // C# line 55-56

        // Undodgeable color (yellow)
        static inline DrawColor undodgeableColor = {255, 255, 0, 255};

        // =====================================================================
        // Methods
        // =====================================================================

        // Initialize (C# line 23-29) — setup
        static void Initialize();

        // Drawing_OnDraw (C# line 172-257) — main draw callback
        static void OnDraw();

        // DrawEvadeStatus (C# line 112-170) — draw ON/OFF text
        static void DrawEvadeStatus();

        // DrawLineRectangle (C# line 73-92) — draw rectangle for line spells
        static void DrawLineRectangle(const Vec2& start, const Vec2& end,
            int radius, int width, const DrawColor& color);

        // DrawLineTriangle (C# line 94-110) — draw triangle for cone spells
        static void DrawLineTriangle(const Vec2& start, const Vec2& end,
            int radius, int width, const DrawColor& color);

        // DrawCircle3D (helper) — draw circle in world space
        static void DrawCircle3D(const Vec3& center, float radius,
            const DrawColor& color, int width = 3, int segments = 32);

        // GetDangerConfig — get color/width for a danger level string
        static DangerDrawConfig GetDangerConfig(const std::string& dangerStr);

        // Helper: WorldToScreen
        static bool WorldToScreen(const Vec3& world, ImVec2& screen);
    };

} // namespace EzEvade
