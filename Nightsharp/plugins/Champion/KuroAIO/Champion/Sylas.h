#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::Sylas {

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 775.0f };
inline Spell W{ SpellSlot::W, 400.0f };
inline Spell E{ SpellSlot::E, 800.0f };
inline Spell R{ SpellSlot::R, 950.0f };

inline bool Loaded = false;
inline int LastCasted = 0;
inline int LastE = 0;

static void Combo() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    // 1. Pyke R steal logic
    const auto rInstance = R.Instance();
    if (R.IsReady() && rInstance.IsValid()) {
        std::string rName = rInstance.Name();
        std::transform(rName.begin(), rName.end(), rName.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (rName.find("pyke") != std::string::npos) {
            R.SetSkillshot(0.40f, 100.0f, FLT_MAX, false, SkillshotType::SkillshotCircle);
            for (const auto& enemy : EnemyHeroesByHealth(R.Range)) {
                if (enemy.HealthPercent() <= 30.0f) {
                    const auto prediction = R.GetPrediction(enemy);
                    if (prediction.Hitchance >= HitChance::High) {
                        if (R.Cast(prediction.GetCastPosition())) {
                            return;
                        }
                    }
                }
            }
        }
    }

    // 2. E logic
    const auto eInstance = E.Instance();
    bool isE2 = false;
    if (eInstance.IsValid()) {
        std::string eName = eInstance.Name();
        if (eName.find("2") != std::string::npos) {
            isE2 = true;
        }
    }

    const int now = SDK::Variables::TickCount();

    if (E.IsReady() && Bool(EMenu, "UseECombo", true)) {
        if (isE2) {
            if (Bool(EMenu, "E2", true)) {
                for (const auto& enemy : EnemyHeroesByHealth(E.Range)) {
                    const auto prediction = E.GetPrediction(enemy);
                    if (prediction.Hitchance >= HitChance::High) {
                        if (LastCasted + 1000 >= now && KuroAIO::CanAttack(250) && LastE + 2750 >= now) {
                            return;
                        }
                        if (!SDK::Collision::HasProjectileWallCollision(
                                player.Position(), prediction.GetCastPosition(),
                                E.Width * 0.5f) &&
                            E.Cast(prediction.GetCastPosition())) {
                            return;
                        }
                    }
                }
            }
        } else {
            if (Bool(EMenu, "E1", true)) {
                const auto target = GetPhysicalTarget(E.Range);
                if (target.IsValid()) {
                    if (E.Cast(target.Position())) {
                        return;
                    }
                }
            }
        }
    }

    // 3. Q logic
    if (Q.IsReady() && Bool(QMenu, "UseQCombo", true)) {
        for (const auto& enemy : EnemyHeroesByHealth(Q.Range)) {
            const auto prediction = Q.GetPrediction(enemy);
            if (prediction.Hitchance >= HitChance::High) {
                if (LastCasted + 1000 >= now && KuroAIO::CanAttack(250)) {
                    return;
                }
                if (Q.Cast(prediction.GetCastPosition())) {
                    return;
                }
            }
        }
    }

    // 4. W logic
    if (W.IsReady() && Bool(WMenu, "UseWCombo", true)) {
        const float playerHpPercent = player.HealthPercent();
        const int minPlayerHp = Slider(WMenu, "OnlyWWhenp", 50);

        const auto target = GetPhysicalTarget(W.Range);
        if (target.IsValid()) {
            const float targetHpPercent = target.HealthPercent();
            const int minTargetHp = Slider(WMenu, "OnlyWWhent", 70);

            if (playerHpPercent <= static_cast<float>(minPlayerHp) || targetHpPercent <= static_cast<float>(minTargetHp)) {
                if (KuroAIO::CanAttack(250)) {
                    return;
                }
                if (W.Cast(target) == CastStates::SuccessfullyCasted) {
                    return;
                }
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    const auto eInstance = E.Instance();
    bool isE2 = false;
    if (eInstance.IsValid()) {
        std::string eName = eInstance.Name();
        if (eName.find("2") != std::string::npos) {
            isE2 = true;
        }
    }

    const int now = SDK::Variables::TickCount();

    if (E.IsReady() && Bool(EMenu, "UseEHarass", true)) {
        if (isE2) {
            if (Bool(EMenu, "E2", true)) {
                for (const auto& enemy : EnemyHeroesByHealth(E.Range)) {
                    const auto prediction = E.GetPrediction(enemy);
                    if (prediction.Hitchance >= HitChance::High) {
                        if (LastCasted + 1000 >= now && KuroAIO::CanAttack(250) && LastE + 2750 >= now) {
                            return;
                        }
                        if (!SDK::Collision::HasProjectileWallCollision(
                                player.Position(), prediction.GetCastPosition(),
                                E.Width * 0.5f) &&
                            E.Cast(prediction.GetCastPosition())) {
                            return;
                        }
                    }
                }
            }
        } else {
            if (Bool(EMenu, "E1", true)) {
                const auto target = GetPhysicalTarget(E.Range);
                if (target.IsValid()) {
                    if (E.Cast(target.Position())) {
                        return;
                    }
                }
            }
        }
    }

    if (Q.IsReady() && Bool(QMenu, "UseQHarass", true)) {
        for (const auto& enemy : EnemyHeroesByHealth(Q.Range)) {
            const auto prediction = Q.GetPrediction(enemy);
            if (prediction.Hitchance >= HitChance::High) {
                if (LastCasted + 1000 >= now && KuroAIO::CanAttack(250)) {
                    return;
                }
                if (Q.Cast(prediction.GetCastPosition())) {
                    return;
                }
            }
        }
    }

    if (W.IsReady() && Bool(WMenu, "UseWHarass", true)) {
        const auto target = GetPhysicalTarget(W.Range);
        if (target.IsValid()) {
            if (KuroAIO::CanAttack(250)) {
                return;
            }
            if (W.Cast(target) == CastStates::SuccessfullyCasted) {
                return;
            }
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() || Game::IsChatOpen()) {
        return;
    }

    if (Orbwalker::IsWindingUp()) {
        return;
    }

    const auto mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo) {
        Combo();
    } else if (mode == OrbwalkingMode::Harass) {
        Harass();
    }
}

static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    if (!Loaded || !Events::IsLocalPlayer(args.Sender)) {
        return;
    }

    if (Orbwalker::IsAutoAttack(args.SpellName)) {
        const AIBaseClient target(args.Target.Ptr);
        if (target.IsValid() && target.IsHero()) {
            LastCasted = 0;
        }
    }

    if (args.Slot == static_cast<int>(SpellSlot::Q) ||
        args.Slot == static_cast<int>(SpellSlot::W) ||
        args.Slot == static_cast<int>(SpellSlot::E)) {
        LastCasted = SDK::Variables::TickCount();
        if (args.Slot == static_cast<int>(SpellSlot::E)) {
            LastE = SDK::Variables::TickCount();
        }
    }
}

static void OnAfterAttack(OrbwalkingActionArgs& args) {
    if (!Loaded || Orbwalker::ActiveMode() != OrbwalkingMode::Combo) {
        return;
    }

    LastCasted = 0;

    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    // 1. Pyke R steal logic
    const auto rInstance = R.Instance();
    if (R.IsReady() && rInstance.IsValid()) {
        std::string rName = rInstance.Name();
        std::transform(rName.begin(), rName.end(), rName.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (rName.find("pyke") != std::string::npos) {
            R.SetSkillshot(0.40f, 100.0f, FLT_MAX, false, SkillshotType::SkillshotCircle);
            for (const auto& enemy : EnemyHeroesByHealth(R.Range)) {
                if (enemy.HealthPercent() <= 30.0f) {
                    const auto prediction = R.GetPrediction(enemy);
                    if (prediction.Hitchance >= HitChance::High) {
                        if (R.Cast(prediction.GetCastPosition())) {
                            return;
                        }
                    }
                }
            }
        }
    }

    // 2. E logic
    const auto eInstance = E.Instance();
    bool isE2 = false;
    if (eInstance.IsValid()) {
        std::string eName = eInstance.Name();
        if (eName.find("2") != std::string::npos) {
            isE2 = true;
        }
    }

    if (E.IsReady() && Bool(EMenu, "UseECombo", true)) {
        if (isE2) {
            if (Bool(EMenu, "E2", true)) {
                for (const auto& enemy : EnemyHeroesByHealth(E.Range)) {
                    const auto prediction = E.GetPrediction(enemy);
                    if (prediction.Hitchance >= HitChance::High) {
                        if (!SDK::Collision::HasProjectileWallCollision(
                                player.Position(), prediction.GetCastPosition(),
                                E.Width * 0.5f) &&
                            E.Cast(prediction.GetCastPosition())) {
                            return;
                        }
                    }
                }
            }
        } else {
            if (Bool(EMenu, "E1", true)) {
                const auto target = GetPhysicalTarget(E.Range);
                if (target.IsValid()) {
                    if (E.Cast(target.Position())) {
                        return;
                    }
                }
            }
        }
    }

    // 3. Q logic
    if (Q.IsReady() && Bool(QMenu, "UseQCombo", true)) {
        for (const auto& enemy : EnemyHeroesByHealth(Q.Range)) {
            const auto prediction = Q.GetPrediction(enemy);
            if (prediction.Hitchance >= HitChance::High) {
                if (Q.Cast(prediction.GetCastPosition())) {
                    return;
                }
            }
        }
    }

    // 4. W logic
    if (W.IsReady() && Bool(WMenu, "UseWCombo", true)) {
        const float playerHpPercent = player.HealthPercent();
        const int minPlayerHp = Slider(WMenu, "OnlyWWhenp", 50);

        const auto target = GetPhysicalTarget(W.Range);
        if (target.IsValid()) {
            const float targetHpPercent = target.HealthPercent();
            const int minTargetHp = Slider(WMenu, "OnlyWWhent", 70);

            if (playerHpPercent <= static_cast<float>(minPlayerHp) || targetHpPercent <= static_cast<float>(minTargetHp)) {
                if (W.Cast(target) == CastStates::SuccessfullyCasted) {
                    return;
                }
            }
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    const Vector3 pos = player.Position();
    if (Q.IsReady() && Bool(DrawMenu, "DrawQ", false)) {
        Drawing::DrawCircle(pos, Q.Range, 0xFFFFFFFF, 1.5f, 64);
    }
    if (W.IsReady() && Bool(DrawMenu, "DrawW", false)) {
        Drawing::DrawCircle(pos, W.Range, 0xFF00FF00, 1.5f, 64);
    }
    if (E.IsReady() && Bool(DrawMenu, "DrawE", false)) {
        Drawing::DrawCircle(pos, E.Range, 0xFFFF0000, 1.5f, 64);
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("KuroAIO.Sylas", "KuroAIO - Sylas", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    QMenu = MenuRoot->AddSubMenu(new Menu("QMenu", "Q Settings"));
    QMenu->Add(new MenuBool("UseQCombo", "Use Q in Combo", true));
    QMenu->Add(new MenuBool("UseQHarass", "Use Q in Harass", true));

    WMenu = MenuRoot->AddSubMenu(new Menu("WMenu", "W Settings"));
    WMenu->Add(new MenuBool("UseWCombo", "Use W in Combo", true));
    WMenu->Add(new MenuSlider("OnlyWWhenp", "Only When Player HP < x%", 50, 0, 101));
    WMenu->Add(new MenuSlider("OnlyWWhent", "Only When Target HP < x%", 70, 0, 101));
    WMenu->Add(new MenuBool("UseWHarass", "Use W in Harass", true));

    EMenu = MenuRoot->AddSubMenu(new Menu("EMenu", "E Settings"));
    EMenu->Add(new MenuBool("UseECombo", "Use E in Combo", true));
    EMenu->Add(new MenuBool("E1", "Use E1 (Dash)", true));
    EMenu->Add(new MenuBool("E2", "Use E2 (Chain)", true));
    EMenu->Add(new MenuBool("UseEHarass", "Use E in Harass", true));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw Settings"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q Range", false));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W Range", false));
    DrawMenu->Add(new MenuBool("DrawE", "Draw E Range", false));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) return;
    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    QMenu = nullptr;
    WMenu = nullptr;
    EMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) return;

    Q = Spell(SpellSlot::Q, 775.0f);
    Q.SetSkillshot(0.40f, 50.0f, FLT_MAX, false, SkillshotType::SkillshotCircle);
    W = Spell(SpellSlot::W, 400.0f);
    W.SetTargetted(0.40f, 2000.0f);
    E = Spell(SpellSlot::E, 800.0f);
    E.SetSkillshot(0.25f, 50.0f, 1500.0f, true, SkillshotType::SkillshotLine);
    R = Spell(SpellSlot::R, 950.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Drawing::OnDraw += &OnDraw;
    Orbwalker::OnAfterAttack += &OnAfterAttack;

    Loaded = true;
    Game::Print("<font color='#00f0ff' size='20'>Kuro - Sylas loaded</font>");
}

static void OnUnload() {
    if (!Loaded) return;

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Drawing::OnDraw -= &OnDraw;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;

    RemoveMenu();
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Sylas
