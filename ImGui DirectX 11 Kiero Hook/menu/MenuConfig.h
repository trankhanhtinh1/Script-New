#pragma once

// ============================================================================
// Menu Configuration - All toggle states and settings
// ============================================================================

namespace Config {

    // ============ General ============
    // Menu toggle: CapsLock (toggle), Shift (hold) — managed by BGXMenu::Render()
    inline bool showMenu        = true;

    // ============ Orbwalker ============
    namespace Orbwalker {
        inline bool enabled     = false;
        inline int  comboKey    = VK_SPACE;
        inline int  harassKey   = 'C';
        inline int  laneClearKey = 'V';
        inline int  lastHitKey  = 'X';

        inline bool drawAttackRange = true;
        inline bool drawTargetCircle = true;
        inline bool drawKillable    = true;

        inline float holdRadius = 75.0f;    // Min move distance
    }

    // ============ Target Selector ============
    namespace TargetSelector {
        // 0=LowHP, 1=Closest, 2=MostAD, 3=MostAP, 4=Priority
        inline int  mode        = 0;
        inline float range      = 900.0f;
        inline bool focusSelected = true;
    }

    // ============ Spells ============
    namespace Spells {
        inline bool autoQ       = false;
        inline bool autoW       = false;
        inline bool autoE       = false;
        inline bool autoR       = false;

        inline bool drawQRange  = true;
        inline bool drawWRange  = false;
        inline bool drawERange  = false;
        inline bool drawRRange  = false;

        // Semi-Cast (press key = cast at mouse)
        inline bool semiQEnabled = true;
        inline int  semiQKey     = 'S';       // S key
        inline bool semiREnabled = true;
        inline int  semiRKey     = 'T';       // T key

        // Cast method: 0=CastSpellSafe(FnCall), 1=KeySimulation
        inline int  castMethod   = 1;  // Default to KeySim (most reliable)

        // Debug overlay
        inline bool showCastDebug = true;
    }

    // ============ Evade ============
    namespace Evade {
        inline bool enabled     = false;
        inline bool drawSpells  = true;
        inline bool drawSafePos = true;
        inline bool dodgeSkillshots = true;
        inline bool dodgeTargeted   = false;
        inline int  dangerLevel     = 1;    // 1=Low, 2=Medium, 3=High, 4=Extreme
    }

    // ============ Awareness / ESP ============
    namespace Awareness {
        inline bool enabled         = true;

        // Enemy
        inline bool drawEnemyHP     = true;
        inline bool drawEnemyMana   = false;
        inline bool drawEnemySpells = true;
        inline bool drawEnemyRange  = false;
        inline bool drawEnemyPath   = false;

        // Ally
        inline bool drawAllyHP      = false;

        // Self
        inline bool drawSelfRange   = true;

        // Jungle
        inline bool drawJungleHP    = true;
        inline bool jungleTimer     = true;

        // Ward
        inline bool wardTimer       = true;

        // Recall
        inline bool trackRecall     = true;

        // Minimap
        inline bool clickableNames  = false;
    }

    // ============ Auto Smite ============
    namespace AutoSmite {
        inline bool enabled     = false;
        inline bool smiteDragon = true;
        inline bool smiteBaron  = true;
        inline bool smiteHerald = true;
        inline bool smiteHorde  = true;
        inline bool drawIndicator = true;
    }

    // ============ Zoom Hack ============
    namespace ZoomHack {
        inline bool enabled     = false;
        inline float zoomValue  = 2250.0f;  // Default max is ~2250
        inline float maxZoom    = 5000.0f;  // Extended max zoom
    }

    // ============ Misc ============
    namespace Misc {
        inline bool antiAFK     = false;
        inline bool autoAccept  = false;
        inline bool showFPS     = true;
        inline bool showPing    = true;
        inline bool showGameTime = true;
    }
}
