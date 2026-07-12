#pragma once

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::ziblldev9898::Irelia {

using SDK::Core::Utils::AutoAttack;

// ============================================================================
// Menu pointers
// ============================================================================
inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* KillStealMenu = nullptr;
inline Menu* DebugMenu = nullptr;

// ============================================================================
// Spell instances (CDragon irelia.bin.json verified 2026-07-09)
// ============================================================================
// Q: CastRange=600, CD=10/10/9/8/7/6/5, Mana=15, Dash (target unit/position)
// W: CastRange=775 (display 825), CD=20/20/18/16/14/12/10, Mana=70-90, Channel+Recast
// E: CastRange=850 (display), CD=16/16/14.5/13/11.5/10/10, Mana=50, Location 2-cast
// R: CastRange=950 (display), CD=125/125/105/85, Mana=100, Line width=160, speed=2000
inline Spell Q{ SpellSlot::Q, 600.0f };
inline Spell W{ SpellSlot::W, 825.0f };
inline Spell E{ SpellSlot::E, 850.0f };
inline Spell R{ SpellSlot::R, 950.0f };

// ============================================================================
// State / tick tracking
// ============================================================================
inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;
inline DWORD LastHarassEvalTick = 0;
inline DWORD LastLaneClearEvalTick = 0;
inline DWORD LastJungleClearEvalTick = 0;

// --- E two-cast tracking ---
// E1 cast position (first blade), E2 cast position (second blade)
// When both are set, the line between them stuns enemies
inline Vec3 E1Position = {};
inline bool E1Active = false;
inline DWORD E1CastTick = 0;
inline Vec3 E2Position = {};
inline bool E2Active = false;
inline DWORD E2CastTick = 0;
// E spell name to detect: "IreliaE" (first cast), "IreliaE2" (second cast)

// --- W channel tracking ---
inline bool WChanneling = false;
inline DWORD WChannelStartTick = 0;

// --- Passive tracking ---
inline int PassiveStacks = 0;
inline float PassiveRemainingTime = 0.0f;
inline bool PassiveMaxed = false;

// ============================================================================
// Helpers
// ============================================================================
static AIHeroClient Player() {
    return ObjectManager::Player();
}

static bool Bool(Menu* menu, const char* key, bool fallback = true) {
    if (!menu) return fallback;
    const auto* item = menu->Get<MenuBool>(key);
    return item ? item->Value : fallback;
}

static int Slider(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) return fallback;
    const auto* item = menu->Get<MenuSlider>(key);
    return item ? item->Value : fallback;
}

static bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) return false;
    lastTick = now;
    return true;
}

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
}

static bool ValidTarget(const AIBaseClient& unit, float range = FLT_MAX) {
    return ValidUnit(unit) && Extensions::IsValidTarget(unit, range, true);
}

static bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

// ============================================================================
// Buff reading (same pattern as Locke.h)
// ============================================================================
static int GetActiveBuffStacksDirect(uintptr_t obj, const char* name) {
    uintptr_t buffs[256] = {};
    const int count = CoreBuffs::Enumerate(obj, buffs, 256);
    const float gameTime = CoreBuffs::ResolveGameTime();
    char buf[96] = {};
    int bestStacks = 0;
    for (int i = 0; i < count; ++i) {
        CoreBuffs::BuffRef buff{ buffs[i] };
        if (!buff.IsActive(gameTime)) continue;
        if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
        if (CoreBuffs::NameMatchesQuery(buf, name)) {
            const int s = buff.GetStacks();
            if (s > bestStacks) bestStacks = s;
        }
    }
    return bestStacks;
}

static float GetBuffRemainingTime(uintptr_t obj, const char* name) {
    uintptr_t buffs[256] = {};
    const int count = CoreBuffs::Enumerate(obj, buffs, 256);
    const float gameTime = CoreBuffs::ResolveGameTime();
    char buf[96] = {};
    float bestRemaining = 0.0f;
    for (int i = 0; i < count; ++i) {
        CoreBuffs::BuffRef buff{ buffs[i] };
        if (!buff.IsActive(gameTime)) continue;
        if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) continue;
        if (CoreBuffs::NameMatchesQuery(buf, name)) {
            const float remaining = buff.GetRemainingTime(gameTime);
            if (remaining > bestRemaining) bestRemaining = remaining;
        }
    }
    return bestRemaining;
}

// ============================================================================
// Debug log — writes to C:\Users\Public\IreliaDebug.txt
// ============================================================================
static void IreliaLog(const char* fmt, ...) {
    FILE* f = nullptr;
    fopen_s(&f, "C:\\Users\\Public\\IreliaDebug.txt", "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fprintf(f, "\n");
    fclose(f);
}

// ============================================================================
// Passive: Ionian Fervor
// Buff: IreliaPassiveStacks (max 4, duration 6s)
// Each stack is a separate buff entry, so we count entries instead of GetStacks.
// Logs all buffs when stack count changes to help identify correct buff name.
// ============================================================================
static int LastLoggedPassiveStacks = -1;

static void UpdatePassiveState() {
    const auto player = Player();
    if (!player.IsValid()) {
        PassiveStacks = 0;
        PassiveRemainingTime = 0.0f;
        PassiveMaxed = false;
        return;
    }

    // Use SDK's GetBuffCount — uses event cache + live resolution
    PassiveStacks = player.GetBuffCount("ireliapassivestacks");

    // Get remaining time via CoreBuffs::FindActiveByName
    const float gameTime = CoreBuffs::ResolveGameTime();
    const auto buff = CoreBuffs::FindActiveByName(player.Address(), "ireliapassivestacks", gameTime);
    PassiveRemainingTime = buff.IsValid() ? buff.GetRemainingTime(gameTime) : 0.0f;
    PassiveMaxed = PassiveStacks >= 4;

    // Log all buffs when passive stack count changes
    if (PassiveStacks != LastLoggedPassiveStacks) {
        IreliaLog("[Irelia] Passive stacks changed: %d -> %d (SDK GetBuffCount=%d remaining=%.3f)",
                  LastLoggedPassiveStacks, PassiveStacks,
                  player.GetBuffCount("ireliapassivestacks"), PassiveRemainingTime);
        LastLoggedPassiveStacks = PassiveStacks;
    }
}

// ============================================================================
// E: Flawless Duet - two-cast detection
// E1 = IreliaE (first blade), E2 = IreliaE2 (second blade, free recast)
// We hook OnProcessSpell to catch both casts and record their EndPosition
// ============================================================================
static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid()) return;
    if (args.Sender.NetworkId != player.NetworkId()) return;

    // Detect E1 cast
    if (_stricmp(args.SpellName, "IreliaE") == 0) {
        E1Position = args.EndPosition;
        E1Active = true;
        E1CastTick = GetTickCount();
        E2Active = false; // reset E2 when E1 is cast again
    }

    // Detect E2 cast
    if (_stricmp(args.SpellName, "IreliaE2") == 0) {
        E2Position = args.EndPosition;
        E2Active = true;
        E2CastTick = GetTickCount();
    }

    // Detect W channel start
    if (_stricmp(args.SpellName, "IreliaW") == 0) {
        WChanneling = true;
        WChannelStartTick = GetTickCount();
    }

    // Detect W release (W2)
    if (_stricmp(args.SpellName, "IreliaW2") == 0) {
        WChanneling = false;
    }
}

// ============================================================================
// Damage calculations
// ============================================================================
static double QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0;
    const float sdkDamage = Q.GetDamage(target);
    if (sdkDamage > 0.0f) return sdkDamage;
    return player.GetSpellDamage(target, SpellSlot::Q);
}

static double EDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float sdkDamage = E.GetDamage(target);
    if (sdkDamage > 0.0f) return sdkDamage;
    return player.GetSpellDamage(target, SpellSlot::E);
}

static double RDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float sdkDamage = R.GetDamage(target);
    if (sdkDamage > 0.0f) return sdkDamage;
    return player.GetSpellDamage(target, SpellSlot::R);
}

// ============================================================================
// Combo damage
// ============================================================================
static double GetComboDamage(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0;

    double damage = Damage::GetAutoAttackDamage(player, target);
    if (Q.IsReady()) damage += QDamage(target);
    if (E.IsReady()) {
        damage += EDamage(target);
        damage += Damage::GetAutoAttackDamage(player, target); // Q reset after E mark
    }
    if (R.IsReady()) damage += RDamage(target);
    return damage;
}

// ============================================================================
// Mark detection (IreliaMark on enemy = Q reset)
// ============================================================================
static bool HasMark(const AIBaseClient& target) {
    if (!target.IsValid()) return false;
    return target.HasBuff("IreliaMark");
}

static bool HasMaxPassive() {
    return PassiveMaxed;
}

// ============================================================================
// Forward declarations
// ============================================================================
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnDraw();
static void OnUnload();

// ============================================================================
// Menu
// ============================================================================
static void BuildMenu() {
    MenuRoot = new Menu("champion.ziblldev9898.irelia", "ziblldev9898 - Irelia", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("useR", "Use R"));
    ComboMenu->Add(new MenuBool("rExecute", "Use R Execute", true));
    ComboMenu->Add(new MenuBool("rAoe", "Use R AoE", true));
    ComboMenu->Add(new MenuSlider("rMinEnemies", "Min Enemies for R AoE", 3, 1, 5));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("ManaHarass", "Mana Harass", 30, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q (lasthit)"));
    LaneClearMenu->Add(new MenuSlider("ManaLC", "Mana Clear", 30, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuSlider("ManaJC", "Mana Clear", 30, 0, 100));

    KillStealMenu = MenuRoot->AddSubMenu(new Menu("KillSteal Settings", "KillSteal"));
    KillStealMenu->Add(new MenuBool("killstealQ", "Use Q"));
    KillStealMenu->Add(new MenuBool("killstealR", "Use R"));

    DebugMenu = MenuRoot->AddSubMenu(new Menu("Debug Settings", "Debug"));
    DebugMenu->Add(new MenuBool("debugPassive", "Debug Passive Stacks", true));
    DebugMenu->Add(new MenuBool("debugE", "Debug E Positions", true));
    DebugMenu->Add(new MenuBool("debugW", "Debug W Channel", true));
    DebugMenu->Add(new MenuBool("debugMarks", "Debug Marks on Enemies", true));

    MenuRoot->Attach();
}

// ============================================================================
// OnGameLoad
// ============================================================================
static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) return;

    // CDragon: Q=600 dash, W=825 channel+recast, E=850 location 2-cast, R=950 line
    Q = Spell(SpellSlot::Q, 600.0f);
    W = Spell(SpellSlot::W, 825.0f);
    E = Spell(SpellSlot::E, 850.0f);
    R = Spell(SpellSlot::R, 950.0f);
    R.SetSkillshot(0.25f, 160.0f, 2000.0f, false, SpellType::Line);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#8ec5ff' size='20'>ziblldev9898 - Irelia loaded</font>");
}

// ============================================================================
// Game_OnUpdate
// ============================================================================
static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) return;
    if (Game::IsChatOpen()) return;
    if (player.Spellbook().IsWindingUp()) return;

    // Update passive state every frame
    UpdatePassiveState();

    // Clear E positions after timeout (E has 0.25s CD between casts, 5s mark duration)
    const DWORD now = GetTickCount();
    if (E1Active && now - E1CastTick > 5000) {
        E1Active = false;
        E1Position = {};
    }
    if (E2Active && now - E2CastTick > 5000) {
        E2Active = false;
        E2Position = {};
    }

    // TODO: Combo, Harass, LaneClear, JungleClear, KillSteal
}

// ============================================================================
// Debug Draw
// ============================================================================
static void OnDraw() {
    if (!Loaded) return;
    const auto player = Player();
    if (!player.IsValid()) return;
    if (!Drawing::IsEnabled()) return;

    const bool debugPassive = Bool(DebugMenu, "debugPassive", true);
    const bool debugE = Bool(DebugMenu, "debugE", true);
    const bool debugW = Bool(DebugMenu, "debugW", true);
    const bool debugMarks = Bool(DebugMenu, "debugMarks", true);

    // --- Debug: Passive stacks + countdown ---
    if (debugPassive) {
        char text[128] = {};
        if (PassiveStacks > 0) {
            if (PassiveMaxed) {
                _snprintf_s(text, sizeof(text), _TRUNCATE,
                    "PASSIVE: MAX (4/4) | %.1fs", PassiveRemainingTime);
            } else {
                _snprintf_s(text, sizeof(text), _TRUNCATE,
                    "PASSIVE: %d/4 | %.1fs", PassiveStacks, PassiveRemainingTime);
            }
        } else {
            _snprintf_s(text, sizeof(text), _TRUNCATE, "PASSIVE: 0/4");
        }

        const uint32_t color = PassiveMaxed ? 0xFF00FF00 :
            (PassiveStacks >= 2 ? 0xFFFFFF00 : 0xFFFF8800);

        Vec2 screenPos = {};
        if (Drawing::WorldToScreen(player.Position(), screenPos) && screenPos.IsValid()) {
            Drawing::DrawText(screenPos.x - 40.0f, screenPos.y - 80.0f, color, text);
        }
    }

    // --- Debug: E1/E2 positions ---
    if (debugE) {
        // Draw E range circle
        Drawing::DrawCircle(player.Position(), E.Range, 0xFF00AAAA);

        const DWORD now = GetTickCount();

        // Draw E1 position
        if (E1Active) {
            Drawing::DrawCircle(E1Position, 80.0f, 0xFF00FFFF);

            Vec2 screenPos = {};
            if (Drawing::WorldToScreen(E1Position, screenPos) && screenPos.IsValid()) {
                const float elapsed = static_cast<float>(now - E1CastTick) / 1000.0f;
                char text[128] = {};
                _snprintf_s(text, sizeof(text), _TRUNCATE,
                    "E1 (%.0f,%.0f) %.1fs", E1Position.x, E1Position.z, elapsed);
                Drawing::DrawText(screenPos.x - 50.0f, screenPos.y - 30.0f,
                    0xFF00FFFF, text);
            }
        }

        // Draw E2 position
        if (E2Active) {
            Drawing::DrawCircle(E2Position, 80.0f, 0xFFFF00FF);

            Vec2 screenPos = {};
            if (Drawing::WorldToScreen(E2Position, screenPos) && screenPos.IsValid()) {
                const float elapsed = static_cast<float>(now - E2CastTick) / 1000.0f;
                char text[128] = {};
                _snprintf_s(text, sizeof(text), _TRUNCATE,
                    "E2 (%.0f,%.0f) %.1fs", E2Position.x, E2Position.z, elapsed);
                Drawing::DrawText(screenPos.x - 50.0f, screenPos.y - 30.0f,
                    0xFFFF00FF, text);
            }
        }

        // Draw line between E1 and E2 (stun zone)
        if (E1Active && E2Active) {
            Drawing::DrawLine(E1Position, E2Position, 4, 0xFFFF0000);

            // Draw distance between E1 and E2
            const float dist = E1Position.Distance2D(E2Position);
            Vec2 midScreen = {};
            const Vec3 midPos(
                (E1Position.x + E2Position.x) * 0.5f,
                (E1Position.y + E2Position.y) * 0.5f,
                (E1Position.z + E2Position.z) * 0.5f
            );
            if (Drawing::WorldToScreen(midPos, midScreen) && midScreen.IsValid()) {
                char text[128] = {};
                _snprintf_s(text, sizeof(text), _TRUNCATE,
                    "STUN LINE dist=%.0f", dist);
                Drawing::DrawText(midScreen.x - 50.0f, midScreen.y - 15.0f,
                    0xFFFF0000, text);
            }
        }
    }

    // --- Debug: W channel ---
    if (debugW) {
        if (WChanneling) {
            const DWORD channelMs = GetTickCount() - WChannelStartTick;
            const float channelSec = static_cast<float>(channelMs) / 1000.0f;
            char text[64] = {};
            _snprintf_s(text, sizeof(text), _TRUNCATE, "W CHARGING: %.2fs", channelSec);

            Vec2 screenPos = {};
            if (Drawing::WorldToScreen(player.Position(), screenPos) && screenPos.IsValid()) {
                // Green if < 0.75s (max charge), yellow if approaching
                const uint32_t color = channelSec >= 0.75f ? 0xFF00FF00 : 0xFFFFFF00;
                Drawing::DrawText(screenPos.x - 40.0f, screenPos.y - 100.0f, color, text);
            }
        }

        // Draw W range
        if (W.IsReady()) {
            Drawing::DrawCircle(player.Position(), W.Range, 0xFF00FFAA);
        }
    }

    // --- Debug: Marks on enemies ---
    if (debugMarks) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) continue;

            const bool marked = HasMark(enemy);
            if (!marked) continue;

            // Draw mark indicator above enemy
            Vec2 screenPos = {};
            if (Drawing::WorldToScreen(enemy.Position(), screenPos) && screenPos.IsValid()) {
                Drawing::DrawText(screenPos.x - 25.0f, screenPos.y - 60.0f,
                    0xFFFF0000, "MARKED");

                // Also draw circle around marked enemy
                Drawing::DrawCircle(enemy.Position(), 100.0f, 0xFFFF0000);
            }
        }
    }

    // --- Debug: R range ---
    if (R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFF00FF);
    }
}

// ============================================================================
// OnUnload
// ============================================================================
static void OnUnload() {
    if (!Loaded) return;

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
    E1Active = false;
    E2Active = false;
    WChanneling = false;
    PassiveStacks = 0;
}

} // namespace Plugins::ziblldev9898::Irelia
