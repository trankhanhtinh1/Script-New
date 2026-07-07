#pragma once

// ============================================================================
// SharpShooter AIO — Corki
// Port từ SharpShooterCSHarp/Plugins/Corki.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Phosphorus Bomb — skillshot circle 825, delay 0.35, radius 250, speed 1000.
//   W Valkyrie        — dash (chỉ vẽ range, không auto-cast trong bản C#).
//   E Gatling Gun     — cone tự thân, cast không cần vị trí (bật khi có target
//                       trong 600).
//   R Missile Barrage — skillshot line 1250, delay 0.20, width 40, speed 2000,
//                       collision. Dùng Ammo (stacks); giữ tối thiểu theo slider
//                       ở harass/jungleclear.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <vector>

namespace Plugins::SharpAIO::Corki {

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 825.0f };
inline Spell W{ SpellSlot::W, 600.0f };
inline Spell E{ SpellSlot::E, 600.0f };
inline Spell R{ SpellSlot::R, 1250.0f };

inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;

static AIHeroClient Player() {
    return ObjectManager::Player();
}

static bool Bool(Menu* menu, const char* key, bool fallback = true) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuBool>(key);
    return item ? item->Value : fallback;
}

static int Slider(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuSlider>(key);
    return item ? item->Value : fallback;
}

static bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) {
        return false;
    }
    lastTick = now;
    return true;
}

static bool ManaOkay(int percent) {
    const auto player = Player();
    return player.IsValid() && player.ManaPercent() >= static_cast<float>(percent);
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

static AIHeroClient GetTargetNoCollision(Spell& spell) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTargetNoCollision(&spell) : AIHeroClient();
}

// Ammo còn lại của R (số tên lửa Missile Barrage).
static int RAmmo() {
    return R.Instance().Ammo();
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void Combo();
static void Mixed();
static void Clear();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Corki", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("useR", "Use R"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuBool("useR", "Use R"));
    HarassMenu->Add(new MenuSlider("keepR", "Keep R Stacks", 3, 0, 7));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q", false));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useR", "Use R"));
    JungleClearMenu->Add(new MenuSlider("keepR", "Keep R Stacks", 5, 0, 7));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 825.0f);
    Q.SetSkillshot(0.35f, 250.0f, 1000.0f, false, SpellType::Circle);
    Q.DamageType = DamageType::Magical;
    Q.MinHitChance = HitChance::High;

    W = Spell(SpellSlot::W, 600.0f);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 600.0f);
    E.SetSkillshot(0.0f, 45.0f, FLT_MAX, false, SpellType::Cone);
    E.DamageType = DamageType::Physical;
    E.MinHitChance = HitChance::Low;

    R = Spell(SpellSlot::R, 1250.0f);
    R.SetSkillshot(0.20f, 40.0f, 2000.0f, true, SpellType::Line);
    R.DamageType = DamageType::Magical;
    R.MinHitChance = HitChance::High;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Corki loaded</font>");
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
        return;
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Mixed();
        break;
    case OrbwalkingMode::LaneClear:
        Clear();
        break;
    default:
        break;
    }
}

static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }

    if (Bool(ComboMenu, "useQ") && Q.IsReady()) {
        const auto target = GetTarget(Q.Range, DamageType::Magical);
        if (ValidHeroTarget(target, Q.Range)) {
            Q.Cast(target, false, true);
        }
    }

    if (Bool(ComboMenu, "useE") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(target, E.Range)) {
            E.Cast();
        }
    }

    if (Bool(ComboMenu, "useR") && R.IsReady()) {
        const auto target = GetTargetNoCollision(R);
        if (ValidHeroTarget(target, R.Range)) {
            R.Cast(target);
        }
    }
}

static void Mixed() {
    const auto player = Player();
    if (!player.IsValid() || !ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        return;
    }

    if (Bool(HarassMenu, "useQ") && Q.IsReady()) {
        const auto target = GetTarget(Q.Range, DamageType::Magical);
        if (ValidHeroTarget(target, Q.Range)) {
            Q.Cast(target, false, true);
        }
    }

    if (Bool(HarassMenu, "useR") && R.IsReady() && RAmmo() > Slider(HarassMenu, "keepR", 3)) {
        const auto target = GetTargetNoCollision(R);
        if (ValidHeroTarget(target, R.Range)) {
            R.Cast(target);
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Lane clear Q: bắn cụm lính (>=4).
    if (Bool(LaneClearMenu, "useQ", false) && Q.IsReady() && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
        auto minions = GameObjects::EnemyLaneMinions();
        if (minions.empty()) {
            minions = GameObjects::EnemyMinions();
        }
        std::vector<AIBaseClient> targets;
        targets.reserve(minions.size());
        for (const auto& minion : minions) {
            if (ValidTarget(minion, Q.Range)) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        if (!targets.empty()) {
            const auto farm = Q.GetCircularFarmLocation(targets);
            if (farm.MinionsHit >= 4) {
                Q.Cast(Vector3::From2D(farm.Position));
            }
        }
    }

    // Jungle clear: mob máu cao nhất trong tầm.
    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !ValidTarget(mob) || mob.IsPlant() || mob.IsPet();
            }),
        mobs.end());
    std::sort(
        mobs.begin(),
        mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) {
            return a.MaxHealth() > b.MaxHealth();
        });

    if (mobs.empty()) {
        return;
    }
    const auto& mob = mobs.front();

    if (Bool(JungleClearMenu, "useQ") && Q.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
        ValidTarget(mob, Q.Range)) {
        Q.Cast(mob);
    }

    if (Bool(JungleClearMenu, "useR") && R.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
        RAmmo() > Slider(JungleClearMenu, "keepR", 5) && ValidTarget(mob, R.Range)) {
        R.Cast(mob);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Corki
