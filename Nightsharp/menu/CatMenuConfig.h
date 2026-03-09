#pragma once
#include "menu/CatStyleMenu.h"

// ============================================================================
// CatMenuConfig - Menu Configuration using CatStyleMenu
// Ported from cat-master menu design
// ============================================================================

namespace CatMenuConfig {

    // Global menu instance
    inline CatMenu::SubMenu* mainMenu = nullptr;

    // ============================================================================
    // Initialize Menu
    // ============================================================================
    inline void Initialize() {
        // Create main menu
        mainMenu = CatMenu::CreateMenu("main", "Cat Menu", 100, 100);
        
        // === Settings Submenu ===
        auto* settingsMenu = mainMenu->addSubmenu("settings", "Menu Settings");
        
        settingsMenu->addHeader("debug_header", "Debug Settings");
        settingsMenu->addBoolean("debug", "Debug Mode", false);
        
        settingsMenu->addHeader("theme_header", "Theme Settings");
        settingsMenu->addSlider("font_size", "Font Size", 13, 8, 20, 1, "%.0f");
        
        settingsMenu->addHeader("color_header", "Color Theme");
        settingsMenu->addColor("bg_main", "Background Main", 16, 18, 22, 224);
        settingsMenu->addColor("bg_title", "Background Title", 30, 35, 40, 224);
        settingsMenu->addColor("text_primary", "Text Primary", 232, 234, 237, 255);
        settingsMenu->addColor("text_header", "Text Header", 94, 158, 214, 255);
        settingsMenu->addColor("checkbox_on", "Checkbox On", 76, 175, 80, 255);
        settingsMenu->addColor("checkbox_off", "Checkbox Off", 102, 102, 102, 255);

        // === Orbwalker Submenu ===
        auto* orbwalkerMenu = mainMenu->addSubmenu("orbwalker", "Orbwalker Settings");
        
        orbwalkerMenu->addHeader("main_header", "Main Settings");
        orbwalkerMenu->addBoolean("enable_orbwalker", "Enable Orbwalker", true);
        orbwalkerMenu->addSlider("extra_windup", "Extra Windup", 0, -50, 50, 5, "%.0f");
        orbwalkerMenu->addColor("target_color", "Target Color", 255, 0, 0, 255);
        
        orbwalkerMenu->addHeader("hotkeys_header", "Hotkeys");
        orbwalkerMenu->addSlider("combo_key", "Combo Key", 32, 0, 255, 1, "%d"); // VK_SPACE
        orbwalkerMenu->addSlider("harass_key", "Harass Key", 67, 0, 255, 1, "%d"); // C
        orbwalkerMenu->addSlider("laneclear_key", "Lane Clear Key", 86, 0, 255, 1, "%d"); // V
        orbwalkerMenu->addSlider("lasthit_key", "Last Hit Key", 88, 0, 255, 1, "%d"); // X
        
        orbwalkerMenu->addHeader("drawings_header", "Drawings");
        orbwalkerMenu->addBoolean("draw_attack_range", "Draw Attack Range", true);
        orbwalkerMenu->addBoolean("draw_target_circle", "Draw Target Circle", true);
        orbwalkerMenu->addBoolean("draw_killable", "Draw Killable Indicator", true);

        // === Combo Submenu ===
        auto* comboMenu = mainMenu->addSubmenu("combo", "Combo Settings");
        
        comboMenu->addHeader("spells_header", "Spell Settings");
        comboMenu->addBoolean("use_q", "Use Q in Combo", true);
        comboMenu->addBoolean("use_w", "Use W in Combo", true);
        comboMenu->addBoolean("use_e", "Use E in Combo", true);
        comboMenu->addBoolean("use_r", "Use R in Combo", true);
        comboMenu->addSlider("combo_delay", "Combo Delay", 0, 0, 500, 25, "%.0f");

        // === Drawings Submenu ===
        auto* drawMenu = mainMenu->addSubmenu("drawings", "Drawing Settings");
        
        drawMenu->addHeader("ranges_header", "Range Drawings");
        drawMenu->addBoolean("draw_q_range", "Draw Q Range", true);
        drawMenu->addBoolean("draw_w_range", "Draw W Range", false);
        drawMenu->addBoolean("draw_e_range", "Draw E Range", false);
        drawMenu->addBoolean("draw_r_range", "Draw R Range", false);
        
        drawMenu->addHeader("colors_header", "Range Colors");
        drawMenu->addColor("q_color", "Q Range Color", 0, 255, 0, 150);
        drawMenu->addColor("w_color", "W Range Color", 0, 0, 255, 150);
        drawMenu->addColor("e_color", "E Range Color", 255, 255, 0, 150);
        drawMenu->addColor("r_color", "R Range Color", 255, 0, 255, 150);

        // === Target Selector Submenu ===
        auto* tsMenu = mainMenu->addSubmenu("target_selector", "Target Selector");
        
        tsMenu->addHeader("mode_header", "Target Mode");
        std::vector<std::string> targetModes = {"Low HP", "Closest", "Most AD", "Most AP", "Priority"};
        tsMenu->addDropdown("target_mode", "Target Mode", targetModes, 0);
        tsMenu->addSlider("target_range", "Target Range", 900, 200, 2000, 50, "%.0f");
        tsMenu->addBoolean("focus_selected", "Focus Selected Target", true);

        // === Evade Submenu ===
        auto* evadeMenu = mainMenu->addSubmenu("evade", "Evade Settings");
        
        evadeMenu->addHeader("main_header", "Main Settings");
        evadeMenu->addBoolean("enable_evade", "Enable Evade", false);
        evadeMenu->addBoolean("dodge_skillshots", "Dodge Skillshots", true);
        evadeMenu->addBoolean("dodge_targeted", "Dodge Targeted", false);
        
        evadeMenu->addHeader("danger_header", "Danger Level");
        std::vector<std::string> dangerLevels = {"Low", "Medium", "High", "Extreme"};
        evadeMenu->addDropdown("danger_level", "Min Danger Level", dangerLevels, 1);
        
        evadeMenu->addHeader("drawings_header", "Drawings");
        evadeMenu->addBoolean("draw_skillshots", "Draw Skillshots", true);
        evadeMenu->addBoolean("draw_safe_pos", "Draw Safe Position", true);

        // === Awareness / ESP Submenu ===
        auto* awarenessMenu = mainMenu->addSubmenu("awareness", "Awareness / ESP");
        
        awarenessMenu->addHeader("main_header", "Main Settings");
        awarenessMenu->addBoolean("enable_awareness", "Enable Awareness", true);
        
        awarenessMenu->addHeader("enemy_header", "Enemy Info");
        awarenessMenu->addBoolean("draw_enemy_hp", "HP Bars", true);
        awarenessMenu->addBoolean("draw_enemy_mana", "Mana Bars", false);
        awarenessMenu->addBoolean("draw_enemy_spells", "Spell Cooldowns", true);
        awarenessMenu->addBoolean("draw_enemy_range", "Attack Range", false);
        awarenessMenu->addBoolean("draw_enemy_path", "Movement Path", false);
        
        awarenessMenu->addHeader("ally_header", "Ally Info");
        awarenessMenu->addBoolean("draw_ally_hp", "HP Bars", false);
        
        awarenessMenu->addHeader("self_header", "Self");
        awarenessMenu->addBoolean("draw_self_range", "Attack Range", true);
        
        awarenessMenu->addHeader("jungle_header", "Jungle");
        awarenessMenu->addBoolean("draw_jungle_hp", "Jungle HP", true);
        awarenessMenu->addBoolean("jungle_timer", "Jungle Timer", true);
        
        awarenessMenu->addHeader("utility_header", "Utility");
        awarenessMenu->addBoolean("ward_timer", "Ward Timers", true);
        awarenessMenu->addBoolean("track_recall", "Track Recalls", true);

        // === Auto Smite Submenu ===
        auto* smiteMenu = mainMenu->addSubmenu("smite", "Auto Smite");
        
        smiteMenu->addHeader("main_header", "Main Settings");
        smiteMenu->addBoolean("enable_smite", "Enable Auto Smite", false);
        
        smiteMenu->addHeader("targets_header", "Targets");
        smiteMenu->addBoolean("smite_dragon", "Dragon", true);
        smiteMenu->addBoolean("smite_baron", "Baron Nashor", true);
        smiteMenu->addBoolean("smite_herald", "Rift Herald", true);
        smiteMenu->addBoolean("smite_horde", "Voidgrub (Horde)", true);
        
        smiteMenu->addHeader("drawings_header", "Drawings");
        smiteMenu->addBoolean("draw_indicator", "Draw Smite Indicator", true);

        // === Zoom Hack Submenu ===
        auto* zoomMenu = mainMenu->addSubmenu("zoom", "Zoom Hack");
        
        zoomMenu->addHeader("main_header", "Main Settings");
        zoomMenu->addBoolean("enable_zoom", "Enable Zoom Hack", false);
        zoomMenu->addSlider("max_zoom", "Max Zoom", 5000, 1000, 10000, 100, "%.0f");
        zoomMenu->addSlider("current_zoom", "Current Zoom", 2250, 1000, 10000, 100, "%.0f");

        // === Misc Submenu ===
        auto* miscMenu = mainMenu->addSubmenu("misc", "Misc Settings");
        
        miscMenu->addHeader("main_header", "Main Settings");
        miscMenu->addBoolean("anti_afk", "Anti-AFK", false);
        miscMenu->addBoolean("auto_accept", "Auto Accept Queue", false);
        
        miscMenu->addHeader("overlay_header", "Info Overlay");
        miscMenu->addBoolean("show_fps", "Show FPS", true);
        miscMenu->addBoolean("show_ping", "Show Ping", true);
        miscMenu->addBoolean("show_game_time", "Show Game Time", true);

        // === Plugin Submenu (placeholder for dynamic plugins) ===
        auto* pluginMenu = mainMenu->addSubmenu("plugins", "Plugins");
        
        pluginMenu->addHeader("info_header", "Plugin Management");
        pluginMenu->addBoolean("auto_load_plugins", "Auto Load Plugins", true);
        pluginMenu->addHeader("loaded_header", "Loaded Plugins");
        // Dynamic plugin items would be added here
    }

    // ============================================================================
    // Render Menu
    // ============================================================================
    inline void Render() {
        CatMenu::Render();
    }

    // ============================================================================
    // Get/Set Helper Functions
    // ============================================================================
    
    // Orbwalker
    inline bool GetOrbwalkerEnabled() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("orbwalker");
        if (auto* item = menu->findBool("enable_orbwalker")) return item->get();
        return false;
    }

    inline float GetExtraWindup() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("orbwalker");
        if (auto* item = menu->findSlider("extra_windup")) return item->get();
        return 0;
    }

    // Spells (Combo)
    inline bool GetComboUseQ() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("combo");
        if (auto* item = menu->findBool("use_q")) return item->get();
        return true;
    }

    inline bool GetComboUseW() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("combo");
        if (auto* item = menu->findBool("use_w")) return item->get();
        return true;
    }

    inline bool GetComboUseE() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("combo");
        if (auto* item = menu->findBool("use_e")) return item->get();
        return true;
    }

    inline bool GetComboUseR() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("combo");
        if (auto* item = menu->findBool("use_r")) return item->get();
        return true;
    }

    // Drawings
    inline bool GetDrawQRange() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("drawings");
        if (auto* item = menu->findBool("draw_q_range")) return item->get();
        return true;
    }

    // Evade
    inline bool GetEvadeEnabled() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("evade");
        if (auto* item = menu->findBool("enable_evade")) return item->get();
        return false;
    }

    // Awareness
    inline bool GetAwarenessEnabled() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("awareness");
        if (auto* item = menu->findBool("enable_awareness")) return item->get();
        return true;
    }

    // Smite
    inline bool GetSmiteEnabled() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("smite");
        if (auto* item = menu->findBool("enable_smite")) return item->get();
        return false;
    }

    // Zoom
    inline bool GetZoomEnabled() {
        auto* menu = CatMenu::g_menu.getRootMenu()->findSubmenu("zoom");
        if (auto* item = menu->findBool("enable_zoom")) return item->get();
        return false;
    }

} // namespace CatMenuConfig
