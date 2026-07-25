#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::Rengar {

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* QSettingsMenu = nullptr;
inline Menu* WSettingsMenu = nullptr;
inline Menu* ESettingsMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 150.0f };
inline Spell W{ SpellSlot::W, 450.0f };
inline Spell E{ SpellSlot::E, 1000.0f };
inline Spell R{ SpellSlot::R, 725.0f };

inline bool Loaded = false;
inline AIHeroClient CurrentTarget = AIHeroClient();

static bool IsEmp() {
    const auto instance = Q.Instance();
    if (instance.IsValid()) {
        std::string name = instance.Name();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (name.find("emp") != std::string::npos) {
            return true;
        }
    }
    const auto player = Player();
    return player.IsValid() && player.Mana() >= 4.0f;
}

static bool IsLeaping() {
    const auto player = Player();
    return player.IsValid() && (SDK::Extensions::IsDashing(player) || player.HasBuff("RengarR") || player.HasBuff("rengarpassivebuff"));
}

static bool QCanAttack(const AIBaseClient& target, float bonusRange = 75.0f) {
    if (!ValidTarget(target)) return false;
    const auto player = Player();
    const float range = player.AttackRange() + player.BoundingRadius() + target.BoundingRadius() + bonusRange;
    return player.Position().Distance2D(target.Position()) <= range;
}

static bool IsImmovableOrCC(const AIHeroClient& target) {
    if (!target.IsValid()) return false;
    return SDK::HasBuffOfType(target, SDK::BuffType::Stun) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Snare) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Taunt) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Suppression) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Charm) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Fear) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Silence) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Asleep);
}

static void CastComboQ(const AIHeroClient& target) {
    if (!Q.IsReady() || !ValidHeroTarget(target)) return;

    const bool use3Q = Key(QSettingsMenu, "Use3Q", false);
    const auto player = Player();
    const bool dashing = SDK::Extensions::IsDashing(player);

    // Fast 3Q Mid-Air Leap logic: If leaping/dashing with 4 Ferocity (EmpQ), cast Q mid-air immediately!
    if (dashing && use3Q && IsEmp()) {
        if (player.Position().Distance2D(target.Position()) <= 750.0f) {
            Q.Cast();
            return;
        }
    }

    if (!dashing || use3Q) {
        if (QCanAttack(target, 50.0f)) {
            Q.Cast();
        }
    }
}

static void CastComboW(const AIHeroClient& target) {
    if (!W.IsReady() || !ValidHeroTarget(target)) return;

    const auto player = Player();
    const bool use3Q = Key(QSettingsMenu, "Use3Q", false);
    const bool dashing = SDK::Extensions::IsDashing(player);

    if ((!dashing || use3Q) && Bool(WSettingsMenu, "UseW", true) && !IsEmp()) {
        if (player.Position().Distance2D(target.Position()) <= W.Range) {
            W.Cast(player.Position());
        }
    }
}

static void CastComboE(const AIHeroClient& target) {
    if (!E.IsReady() || !ValidHeroTarget(target)) return;

    const auto player = Player();
    const bool dashing = SDK::Extensions::IsDashing(player);
    const bool use3Q = Key(QSettingsMenu, "Use3Q", false);

    // Allow E in mid-air during dash/leap when not holding Emp, or during Fast 3Q
    if (QCanAttack(target, 100.0f) && IsEmp() && !dashing && !use3Q) {
        return; // Save empowered for EmpQ or EmpW when grounded
    }

    if (!player.HasBuff("RengarR") || dashing || use3Q) {
        if (Bool(ESettingsMenu, "UseE", true)) {
            auto pred = E.GetPrediction(target);
            if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::High)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }
}

static void OnAfterAttack(OrbwalkingActionArgs& args) {
    if (!Loaded || !IsComboMode()) return;
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    if (args.Target.IsValid() && Q.IsReady() && Bool(ComboMenu, "UseQ", true)) {
        Q.Cast();
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    CurrentTarget = GetPhysicalTarget(E.Range);

    // Auto EmpW Cleanse / CC removal
    if (W.IsReady() && Bool(WSettingsMenu, "AutoCleanse", true) && IsEmp()) {
        if (IsImmovableOrCC(player)) {
            W.Cast(player.Position());
            return;
        }
    }

    // Auto W Heal when low HP
    if (W.IsReady() && Bool(WSettingsMenu, "AutoHeal", true) && IsEmp()) {
        if (player.HealthPercent() <= static_cast<float>(Slider(WSettingsMenu, "HealHp", 30))) {
            W.Cast(player.Position());
            return;
        }
    }

    // Combo Mode
    if (IsComboMode() && ValidHeroTarget(CurrentTarget)) {
        if (Bool(ComboMenu, "UseQ", true) && CanAttack(250)) {
            CastComboQ(CurrentTarget);
        }

        if (Bool(ComboMenu, "UseW", true)) {
            CastComboW(CurrentTarget);
        }

        if (Bool(ComboMenu, "UseE", true)) {
            CastComboE(CurrentTarget);
        }
    }

    // Harass Mode
    if (IsHarassMode() && ValidHeroTarget(CurrentTarget)) {
        if (Bool(ComboMenu, "UseQ", true) && CanAttack(250)) {
            CastComboQ(CurrentTarget);
        }
        if (Bool(ComboMenu, "UseW", true)) {
            CastComboW(CurrentTarget);
        }
        if (Bool(ComboMenu, "UseE", true)) {
            CastComboE(CurrentTarget);
        }
    }

    // Lane / Jungle Clear Mode
    if (IsClearMode()) {
        const float clearRange = player.AttackRange() + player.BoundingRadius() + 150.0f;

        // Jungle clear
        for (const auto& minion : GameObjects::Jungle()) {
            if (ValidTarget(minion, clearRange)) {
                if (Q.IsReady() && Bool(ClearMenu, "JungleQ", true)) {
                    Q.Cast();
                    return;
                }
                if (W.IsReady() && Bool(ClearMenu, "JungleW", true)) {
                    W.Cast(player.Position());
                    return;
                }
                if (E.IsReady() && Bool(ClearMenu, "JungleE", true) && !IsEmp()) {
                    E.Cast(minion.Position());
                    return;
                }
            }
        }

        // Minion clear
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (ValidTarget(minion, clearRange) && !IsEmp()) {
                if (Q.IsReady() && Bool(ClearMenu, "LaneQ", true)) {
                    Q.Cast();
                    return;
                }
            }
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    const Vector3 pos = player.Position();

    if (Bool(DrawMenu, "DrawW", true) && W.IsReady()) {
        Drawing::DrawCircle(pos, W.Range, 0xFFFFAA00u, 1.5f, 64);
    }

    if (Bool(DrawMenu, "DrawE", true) && E.IsReady()) {
        Drawing::DrawCircle(pos, E.Range, 0xFFD90429u, 1.5f, 64);
    }

    if (Bool(DrawMenu, "DrawLeap", true)) {
        const float leapRange = IsLeaping() ? 725.0f : 0.0f;
        if (leapRange > 0.0f) {
            Drawing::DrawCircle(pos, leapRange, 0xFFFF6B00u, 2.0f, 64);
        }
    }

    if (Bool(DrawMenu, "DrawTarget", true) && ValidHeroTarget(CurrentTarget)) {
        Vec2 playerScreen{}, targetScreen{};
        if (Drawing::WorldToScreen(pos, playerScreen) && Drawing::WorldToScreen(CurrentTarget.Position(), targetScreen)) {
            Drawing::DrawLine(playerScreen, targetScreen, 0xFFD90429u, 2.0f);
            Drawing::DrawText(targetScreen.x - 40.0f, targetScreen.y + 20.0f, 0xFFD90429u, "Combo Target");
        }
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("KuroAIO.Rengar", "KuroAIO - Rengar", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("UseQ", "Use Q Savagery", true));
    ComboMenu->Add(new MenuBool("UseW", "Use W Battle Roar", true));
    ComboMenu->Add(new MenuBool("UseE", "Use E Bola Strike", true));

    QSettingsMenu = MenuRoot->AddSubMenu(new Menu("QSettings", "Q Savagery Settings"));
    QSettingsMenu->Add(new MenuKeyBind("Use3Q", "Fast 3Q Combo", SDK::Keys::A, KeyBindType::Toggle))->Permashow();

    WSettingsMenu = MenuRoot->AddSubMenu(new Menu("WSettings", "W Battle Roar Settings"));
    WSettingsMenu->Add(new MenuBool("UseW", "Use W in Combo", true));
    WSettingsMenu->Add(new MenuBool("AutoCleanse", "Auto EmpW Cleanse CC", true));
    WSettingsMenu->Add(new MenuBool("AutoHeal", "Auto EmpW Heal Low HP", true));
    WSettingsMenu->Add(new MenuSlider("HealHp", "Heal at HP %", 30, 10, 80));

    ESettingsMenu = MenuRoot->AddSubMenu(new Menu("ESettings", "E Bola Strike Settings"));
    ESettingsMenu->Add(new MenuBool("UseE", "Use E in Combo", true));

    ClearMenu = MenuRoot->AddSubMenu(new Menu("Clear", "Clear Settings"));
    ClearMenu->Add(new MenuBool("JungleQ", "Jungle Q", true));
    ClearMenu->Add(new MenuBool("JungleW", "Jungle W", true));
    ClearMenu->Add(new MenuBool("JungleE", "Jungle E", true));
    ClearMenu->Add(new MenuBool("LaneQ", "Lane Q", true));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Drawings"));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W Range", true));
    DrawMenu->Add(new MenuBool("DrawE", "Draw E Range", true));
    DrawMenu->Add(new MenuBool("DrawLeap", "Draw Leap Range", true));
    DrawMenu->Add(new MenuBool("DrawTarget", "Draw Current Target Line", true));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) return;
    if (auto* item = QSettingsMenu ? QSettingsMenu->Get<MenuKeyBind>("Use3Q") : nullptr) {
        item->RemovePermashow();
    }
    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    QSettingsMenu = nullptr;
    WSettingsMenu = nullptr;
    ESettingsMenu = nullptr;
    ClearMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) return;

    Q = Spell(SpellSlot::Q, 150.0f);
    W = Spell(SpellSlot::W, 450.0f);
    E = Spell(SpellSlot::E, 1000.0f);
    E.SetSkillshot(0.25f, 70.0f, 1500.0f, true, SkillshotType::SkillshotLine);

    R = Spell(SpellSlot::R, 725.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Drawing::OnDraw += &OnDraw;
    Orbwalker::OnAfterAttack += &OnAfterAttack;

    Loaded = true;
    Game::Print("<font color='#d90429' size='20'>Kuro - Rengar loaded</font>");
}

static void OnUnload() {
    if (!Loaded) return;

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Drawing::OnDraw -= &OnDraw;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;

    RemoveMenu();
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Rengar
