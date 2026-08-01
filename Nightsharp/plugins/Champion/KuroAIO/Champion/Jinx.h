#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::Jinx {

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* WSettingsMenu = nullptr;
inline Menu* ESettingsMenu = nullptr;
inline Menu* RSettingsMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 525.0f };
inline Spell W{ SpellSlot::W, 1500.0f };
inline Spell E{ SpellSlot::E, 900.0f };
inline Spell R{ SpellSlot::R, 25000.0f };

inline bool Loaded = false;
inline AIHeroClient CurrentTarget = AIHeroClient();
inline float DynamicRange = 1500.0f;

constexpr float BonusQRange[6] = { 0.0f, 80.0f, 110.0f, 140.0f, 170.0f, 200.0f };

static bool IsQActive() {
    const auto player = Player();
    return player.IsValid() && (player.HasBuff("JinxQ") || player.HasBuff("jinxq"));
}

static float GetMinigunRange(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    float range = player.AttackRange() + player.BoundingRadius() + target.BoundingRadius();
    if (IsQActive()) {
        const int level = std::clamp(Q.Level(), 1, 5);
        range -= BonusQRange[level];
    }
    return range;
}

static float GetRocketRange(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    float range = player.AttackRange() + player.BoundingRadius() + target.BoundingRadius();
    if (!IsQActive()) {
        const int level = std::clamp(Q.Level(), 1, 5);
        range += BonusQRange[level];
    }
    return range;
}

static bool InRangeMinigun(const AIBaseClient& target) {
    if (!ValidTarget(target)) return false;
    const auto player = Player();
    const float dist = player.Position().Distance2D(target.Position());
    return dist <= GetMinigunRange(target);
}

static bool InRangeRocket(const AIBaseClient& target, float bonus = 0.0f) {
    if (!ValidTarget(target)) return false;
    const auto player = Player();
    const float dist = player.Position().Distance2D(target.Position());
    return dist <= GetRocketRange(target) + bonus + 25.0f;
}

static bool InCurrentAutoAttackRange(const AIBaseClient& target, float extra = 0.0f) {
    if (!ValidTarget(target)) return false;
    const auto player = Player();
    const float range = player.AttackRange() + player.BoundingRadius() + target.BoundingRadius() + extra;
    return player.Position().Distance2D(target.Position()) <= range;
}

static void UseLogicQ(const AIHeroClient& target) {
    if (Q.Level() < 1 || !CanAttack(250)) return;

    if (InRangeMinigun(target)) {
        if (IsQActive()) {
            Q.Cast();
        }
    } else if (InRangeRocket(target)) {
        if (!IsQActive()) {
            Q.Cast();
        }
    }
}

static float CalculateWDamage(const AIHeroClient& target) {
    if (!target.IsValid() || Q.Level() < 1) return 0.0f;
    const auto player = Player();
    const int wLevel = std::clamp(W.Level(), 1, 5);
    if (wLevel <= 0) return 0.0f;

    // Zap! Base damage: 10/60/110/160/210 + 1.6 * total AD
    const float baseDmg = 10.0f + 50.0f * (wLevel - 1);
    const float rawDmg = baseDmg + 1.6f * player.TotalAttackDamage();
    return player.CalculatePhysicalDamage(target, rawDmg);
}

static float CalculateRDamage(const AIHeroClient& target) {
    if (!ValidHeroTarget(target)) return 0.0f;
    const auto player = Player();
    const int rLevel = std::clamp(R.Level(), 1, 3);
    if (rLevel <= 0) return 0.0f;

    // Super Mega Death Rocket! Base: 250/400/550 + 1.5 * bonus AD + 25/30/35% missing HP
    const float baseDmg = 250.0f + 150.0f * (rLevel - 1);
    const float bonusAD = player.BonusAttackDamage();
    const float missingHpPercent = 0.25f + 0.05f * (rLevel - 1);
    const float missingHp = target.MaxHealth() - target.Health();

    float rawDmg = baseDmg + 1.5f * bonusAD + missingHpPercent * missingHp;
    const float dist = player.Position().Distance2D(target.Position());
    if (dist < 1500.0f) {
        rawDmg *= (0.1f + 0.9f * (dist / 1500.0f));
    }

    return player.CalculatePhysicalDamage(target, rawDmg);
}

static float RDmgget(const AIHeroClient& target) {
    if (!ValidHeroTarget(target)) return 0.0f;
    const auto player = Player();
    const int type = List(RSettingsMenu, "RCalDmg", 2);
    const float baseR = CalculateRDamage(target);

    switch (type) {
    case 0:
        return 1.5f * baseR;
    case 1:
        return baseR;
    default: {
        float comboDmg = baseR;
        if (player.Position().Distance2D(target.Position()) <= W.Range - 200.0f && W.IsReady()) {
            comboDmg += CalculateWDamage(target);
        }
        if (InCurrentAutoAttackRange(target)) {
            comboDmg += player.GetAutoAttackDamage(target, true);
        }
        return comboDmg;
    }
    }
}

static bool IsImmovableOrCC(const AIHeroClient& target) {
    if (!target.IsValid()) return false;
    return SDK::HasBuffOfType(target, SDK::BuffType::Stun) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Snare) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Taunt) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Suppression) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Charm) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Fear) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Knockup) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Knockback) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Slow);
}

static void OnGapcloser(const GapCloserEventArgs& args) {
    if (!Loaded || !E.IsReady()) return;
    const AIHeroClient sender(args.Sender);
    if (!ValidHeroTarget(sender, E.Range)) return;

    if (Bool(ESettingsMenu, "UseE", true) || Bool(ESettingsMenu, "AutoE", true)) {
        auto pred = E.GetPrediction(sender);
        if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::High)) {
            E.Cast(pred.GetCastPosition());
        }
    }
}

static void UpdateTarget() {
    const auto player = Player();
    if (!player.IsValid()) {
        CurrentTarget = AIHeroClient();
        return;
    }

    const int qLevel = std::clamp(Q.Level(), 1, 5);
    DynamicRange = std::max(1500.0f, player.AttackRange() + player.BoundingRadius() + (IsQActive() ? 0.0f : BonusQRange[qLevel]));

    AIHeroClient rocketTarget = AIHeroClient();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (InRangeRocket(enemy)) {
            rocketTarget = enemy;
            break;
        }
    }

    if (ValidHeroTarget(rocketTarget)) {
        CurrentTarget = rocketTarget;
    } else {
        CurrentTarget = GetPhysicalTarget(DynamicRange);
    }
}

static bool CastWAt(const AIHeroClient& target, HitChance required) {
    if (!W.IsReady() || !ValidHeroTarget(target, W.Range)) {
        return false;
    }
    const auto prediction = W.GetPrediction(target);
    const auto player = Player();
    const Vector3 castPosition = prediction.GetCastPosition();
    if (!player.IsValid() || castPosition.IsZero() ||
        static_cast<int>(prediction.Hitchance) < static_cast<int>(required) ||
        SDK::Collision::HasProjectileWallCollision(
            player.Position(), castPosition, W.Width * 0.5f)) {
        return false;
    }
    return W.Cast(castPosition);
}

static bool CastRAt(const AIHeroClient& target, HitChance required) {
    if (!R.IsReady() || !ValidHeroTarget(target, R.Range)) {
        return false;
    }
    const auto prediction = R.GetPrediction(target);
    const auto player = Player();
    const Vector3 castPosition = prediction.GetCastPosition();
    if (!player.IsValid() || castPosition.IsZero() ||
        static_cast<int>(prediction.Hitchance) < static_cast<int>(required) ||
        SDK::Collision::HasProjectileWallCollision(
            player.Position(), castPosition, R.Width * 0.5f)) {
        return false;
    }
    return R.Cast(castPosition);
}

static void CastLogicW(const AIHeroClient& target) {
    if (!W.IsReady() || !ValidHeroTarget(target)) return;
    const int wMode = List(WSettingsMenu, "WComboType", 0);

    if (wMode == 0) { // Logic
        if (!CanAttack(500) && CastWAt(target, HitChance::High)) {
            return;
        }
    } else if (wMode == 1) { // Out AA Range
        if (!InCurrentAutoAttackRange(target) &&
            CastWAt(target, HitChance::High)) {
            return;
        }
    } else if (wMode == 2) { // Always
        (void)CastWAt(target, HitChance::High);
    }
}

static void CastLogicE(const AIHeroClient& target) {
    if (!E.IsReady() || !ValidHeroTarget(target)) return;

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy, E.Range) && SDK::Extensions::IsDashing(enemy)) {
            auto pred = E.GetPrediction(enemy);
            if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::High)) {
                E.Cast(pred.GetCastPosition());
                return;
            }
        }
    }

    const bool useLogicE = Bool(ESettingsMenu, "UseLogicE", false);
    const bool condition = SDK::Extensions::IsFacing(target, Player()) ||
                           SDK::Extensions::IsBothFacing(target, Player()) ||
                           IsImmovableOrCC(target) ||
                           SDK::Extensions::IsCastingInterruptableSpell(target);

    if (useLogicE) {
        if (Player().Position().Distance2D(target.Position()) <= E.Range - 200.0f && condition) {
            auto pred = E.GetPrediction(target);
            if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::VeryHigh)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    } else if (condition) {
        auto pred = E.GetPrediction(target);
        if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::High)) {
            E.Cast(pred.GetCastPosition());
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    UpdateTarget();

    // Semi-Manual R
    if (Key(RSettingsMenu, "SemiR", false) && R.IsReady()) {
        auto rTarget = GetPhysicalTarget(R.Range);
        if (ValidHeroTarget(rTarget)) {
            if (CastRAt(rTarget, HitChance::High)) {
                return;
            }
        }
    }

    // Auto Q back to Minigun when no orbwalking mode active
    if (Orbwalker::ActiveMode() == OrbwalkingMode::None) {
        if (IsQActive() && Bool(ComboMenu, "AutoQ", true)) {
            Q.Cast();
            return;
        }

        if (E.IsReady() && Bool(ESettingsMenu, "AutoE", true)) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(enemy, E.Range) && SDK::Extensions::IsDashing(enemy)) {
                    auto pred = E.GetPrediction(enemy);
                    if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::High)) {
                        E.Cast(pred.GetCastPosition());
                        return;
                    }
                }
            }
        }
    }

    // Combo Mode
    if (IsComboMode() && ValidHeroTarget(CurrentTarget)) {
        if (Bool(ComboMenu, "UseQ", true) && CanAttack(250)) {
            AIHeroClient orbTarget = CurrentTarget;
            if (!ValidHeroTarget(orbTarget)) {
                if (InRangeRocket(CurrentTarget, 300.0f)) {
                    if (!IsQActive()) {
                        Q.Cast();
                    }
                    return;
                }
            }
            UseLogicQ(CurrentTarget);
        }

        if (Bool(ESettingsMenu, "UseE", true)) {
            CastLogicE(CurrentTarget);
        }

        if (Bool(WSettingsMenu, "UseW", true)) {
            CastLogicW(CurrentTarget);
        }

        if (Bool(RSettingsMenu, "UseR", true) && R.IsReady() &&
            CurrentTarget.Health() <= RDmgget(CurrentTarget) &&
            CastRAt(CurrentTarget, HitChance::VeryHigh)) {
            return;
        }
    }

    // Harass Mode
    if (IsHarassMode() && ValidHeroTarget(CurrentTarget)) {
        if (player.ManaPercent() >= static_cast<float>(Slider(HarassMenu, "Mana", 40))) {
            if (Bool(HarassMenu, "UseQ", true) && CanAttack(250)) {
                UseLogicQ(CurrentTarget);
            }
            if (Bool(HarassMenu, "UseW", true)) {
                CastLogicW(CurrentTarget);
            }
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    const Vector3 pos = player.Position();

    if (Bool(DrawMenu, "DrawQ", true)) {
        const float qRange = player.AttackRange() + player.BoundingRadius() + (IsQActive() ? 0.0f : BonusQRange[std::clamp(Q.Level(), 1, 5)]);
        Drawing::DrawCircle(pos, qRange, 0xFF00E5FFu, 2.0f, 64);
    }

    if (Bool(DrawMenu, "DrawW", true) && W.IsReady()) {
        Drawing::DrawCircle(pos, W.Range, 0xFFFFAA00u, 1.5f, 64);
    }

    if (Bool(DrawMenu, "DrawE", true) && E.IsReady()) {
        Drawing::DrawCircle(pos, E.Range, 0xFFFF2A70u, 1.5f, 64);
    }

    if (Bool(DrawMenu, "DrawTarget", true) && ValidHeroTarget(CurrentTarget)) {
        Vec2 playerScreen{}, targetScreen{};
        if (Drawing::WorldToScreen(pos, playerScreen) && Drawing::WorldToScreen(CurrentTarget.Position(), targetScreen)) {
            Drawing::DrawLine(playerScreen, targetScreen, 0xFFFF2A70u, 2.0f);
            Drawing::DrawText(targetScreen.x - 40.0f, targetScreen.y + 20.0f, 0xFFFF2A70u, "Combo Target");
        }
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("KuroAIO.Jinx", "KuroAIO - Jinx", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("UseQ", "Use Q Switcheroo!", true));
    ComboMenu->Add(new MenuBool("AutoQ", "Auto Switch Minigun when out of combat", true));

    WSettingsMenu = MenuRoot->AddSubMenu(new Menu("WSettings", "W Zap! Settings"));
    WSettingsMenu->Add(new MenuBool("UseW", "Use W Zap!", true));
    WSettingsMenu->Add(new MenuList("WComboType", "W Combo Mode", { "Logic", "Out AA Range", "Always" }, 0));

    ESettingsMenu = MenuRoot->AddSubMenu(new Menu("ESettings", "E Flame Chompers! Settings"));
    ESettingsMenu->Add(new MenuBool("UseE", "Use E Flame Chompers!", true));
    ESettingsMenu->Add(new MenuBool("AutoE", "Auto E on Dashing / CC Enemies", true));
    ESettingsMenu->Add(new MenuBool("UseLogicE", "Use Logic E (VeryHigh hitchance close)", false));

    RSettingsMenu = MenuRoot->AddSubMenu(new Menu("RSettings", "R Super Mega Death Rocket! Settings"));
    RSettingsMenu->Add(new MenuBool("UseR", "Use R in Combo (Killable)", true));
    RSettingsMenu->Add(new MenuList("RCalDmg", "R Damage Calculation", { "x1.5 Dmg", "Base Dmg", "Combo Dmg" }, 2));
    RSettingsMenu->Add(new MenuKeyBind("SemiR", "Semi-Manual R", SDK::Keys::A, KeyBindType::Press))->Permashow();

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass Settings"));
    HarassMenu->Add(new MenuBool("UseQ", "Use Q Harass", true));
    HarassMenu->Add(new MenuBool("UseW", "Use W Harass", true));
    HarassMenu->Add(new MenuSlider("Mana", "Minimum Mana %", 40, 0, 100));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Drawings"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q Range", true));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W Range", true));
    DrawMenu->Add(new MenuBool("DrawE", "Draw E Range", true));
    DrawMenu->Add(new MenuBool("DrawTarget", "Draw Current Target Line", true));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) return;
    if (auto* item = RSettingsMenu ? RSettingsMenu->Get<MenuKeyBind>("SemiR") : nullptr) {
        item->RemovePermashow();
    }
    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    WSettingsMenu = nullptr;
    ESettingsMenu = nullptr;
    RSettingsMenu = nullptr;
    HarassMenu = nullptr;
    ClearMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) return;

    Q = Spell(SpellSlot::Q, 525.0f);
    W = Spell(SpellSlot::W, 1500.0f);
    W.SetSkillshot(0.4f, 45.0f, 3300.0f, true, SkillshotType::SkillshotLine);

    E = Spell(SpellSlot::E, 900.0f);
    E.SetSkillshot(1.25f, 120.0f, FLT_MAX, false, SkillshotType::SkillshotCircle);

    R = Spell(SpellSlot::R, 25000.0f);
    R.SetSkillshot(0.6f, 140.0f, 1700.0f, false, SkillshotType::SkillshotLine);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Drawing::OnDraw += &OnDraw;
    Events::hook.OnGapCloser += &OnGapcloser;

    Loaded = true;
    Game::Print("<font color='#ff2a70' size='20'>Kuro - Jinx loaded</font>");
}

static void OnUnload() {
    if (!Loaded) return;

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Drawing::OnDraw -= &OnDraw;
    Events::hook.OnGapCloser -= &OnGapcloser;

    RemoveMenu();
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Jinx
