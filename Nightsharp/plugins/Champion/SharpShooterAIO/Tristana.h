#pragma once

// ============================================================================
// SharpShooter AIO — Tristana
// Port từ SharpShooterCSHarp/Plugins/Tristana.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Rapid Fire      — self-buff, cast trên before-attack khi trong tầm AA.
//   W Rocket Jump     — skillshot circle 1170, delay 0.5, radius 270 (chỉ vẽ range).
//   E Explosive Charge— targeted, tầm = AA range + 65, gây nổ stack.
//   R Buster Shot     — targeted knockback, tầm = AA range + 65.
//
// Ghi chú port:
//   * Menu E-targets per-enemy của bản C# được rút gọn thành 1 toggle "useE"
//     (tối giản kiểu 7UP), vẫn giữ logic chọn mục tiêu ưu tiên trong tầm E.
//   * IsWillDieByTristanaE / IsKillableAndValidTarget port thành helper cục bộ.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <string>

namespace Plugins::SharpAIO::Tristana {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, FLT_MAX };
inline Spell W{ SpellSlot::W, 1170.0f };
inline Spell E{ SpellSlot::E, 550.0f };
inline Spell R{ SpellSlot::R, 550.0f };

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

static float HealthRegenRate(const AIBaseClient& unit) {
    return unit.IsValid() ? ::CoreAIHeroClient::HealthRegenRate(unit.Address()) : 0.0f;
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

// Tầm auto-attack thực + 65 (E/R của Tristana bằng tầm đánh thường + 65).
static float AutoAttackRangePlus65() {
    const auto player = Player();
    if (!player.IsValid()) {
        return 550.0f;
    }
    return AutoAttack::GetRealAutoAttackRange(player, AttackableUnit()) + 65.0f;
}

// Port rút gọn của ExtraExtensions.IsKillableAndValidTarget: loại các buff bất
// tử phổ biến rồi so máu + hồi máu + shield với sát thương.
static bool IsKillable(const AIBaseClient& target, double calculatedDamage, DamageType damageType) {
    if (!ValidUnit(target)) {
        return false;
    }

    if (target.HasBuff("kindredrnodeathbuff") ||
        target.HasBuff("Undying Rage") ||
        target.HasBuff("JudicatorIntervention") ||
        target.HasBuff("BansheesVeil") ||
        target.HasBuff("SivirShield") ||
        target.HasBuff("ShroudofDarkness")) {
        return false;
    }

    const auto player = Player();
    if (player.IsValid() && player.HasBuff("summonerexhaust")) {
        calculatedDamage *= 0.6;
    }

    if (target.HasBuff("FerociousHowl")) {
        calculatedDamage *= 0.3;
    }

    const float shield = damageType == DamageType::Physical
        ? target.PhysicalShield()
        : target.MagicalShield();
    return target.Health() + HealthRegenRate(target) + shield < calculatedDamage - 2.0;
}

static double TristanaEDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }
    const double eDmg = player.GetSpellDamage(target, SpellSlot::E);
    const int charges = target.GetBuffCount("tristanaecharge");
    return eDmg * (charges * 0.30) + eDmg;
}

static bool WillDieByTristanaE(const AIBaseClient& target) {
    if (!target.HasBuff("tristanaecharge")) {
        return false;
    }
    return IsKillable(target, TristanaEDamage(target), DamageType::Physical);
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void OnAfterAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void Combo();
static void Mixed();
static void Clear();
static void KillSteal();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Tristana", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("useR", "Use R (finisher)"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useE", "Use E"));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 0, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useE", "Use E"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("killsteal", "Killsteal (R)"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (R)"));
    MiscMenu->Add(new MenuBool("interrupter", "Interrupter (R)"));
    MiscMenu->Add(new MenuBool("autoETurret", "Auto E on Turret"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, FLT_MAX);

    W = Spell(SpellSlot::W, 1170.0f);
    W.SetSkillshot(0.5f, 270.0f, 1500.0f, false, SpellType::Circle);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 550.0f);
    E.DamageType = DamageType::Physical;
    R = Spell(SpellSlot::R, 550.0f);
    R.DamageType = DamageType::Physical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Tristana loaded</font>");
}

// Q Rapid Fire: bật trước khi đánh thường (combo, hoặc jungle mob khi laneclear).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded || !Q.IsReady()) {
        return;
    }

    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(targetBase) || !AutoAttack::InAutoAttackRange(targetBase)) {
        return;
    }

    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo) {
        if (Bool(ComboMenu, "useQ")) {
            Q.Cast();
        }
    } else if (mode == OrbwalkingMode::LaneClear) {
        if (Bool(JungleClearMenu, "useQ") && targetBase.IsMinion() &&
            targetBase.Team() == GameObjectTeam::Neutral) {
            Q.Cast();
        }
    }
}

// Auto E lên trụ địch sau khi đánh thường.
static void OnAfterAttack(OrbwalkingActionArgs& args) {
    if (!Loaded || !Bool(MiscMenu, "autoETurret") || !E.IsReady()) {
        return;
    }

    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (ValidUnit(targetBase) && targetBase.IsTurret()) {
        E.CastOnUnit(targetBase);
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
        return;
    }

    // E/R có tầm phụ thuộc tầm đánh thường hiện tại.
    const float castRange = AutoAttackRangePlus65();
    E.Range = castRange;
    R.Range = castRange;

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

    KillSteal();
}

static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }

    if (Bool(ComboMenu, "useE") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(target, E.Range)) {
            E.CastOnUnit(target);
        }
    }

    if (Bool(ComboMenu, "useR") && R.IsReady()) {
        // R chỉ dùng để finish: mục tiêu chết bởi R và không chết sẵn bởi E.
        AIHeroClient best;
        float bestHealth = -1.0f;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, R.Range)) {
                continue;
            }
            if (WillDieByTristanaE(enemy)) {
                continue;
            }
            if (IsKillable(enemy, R.GetDamage(enemy), DamageType::Physical) &&
                enemy.Health() > bestHealth) {
                best = enemy;
                bestHealth = enemy.Health();
            }
        }
        if (best.IsValid()) {
            R.CastOnUnit(best);
        }
    }
}

static void Mixed() {
    if (!Bool(HarassMenu, "useE") || !E.IsReady() || !ManaOkay(Slider(HarassMenu, "Mana", 0))) {
        return;
    }

    const auto target = GetTarget(E.Range, DamageType::Physical);
    if (ValidHeroTarget(target, E.Range)) {
        E.CastOnUnit(target);
    }
}

static void Clear() {
    if (!Bool(JungleClearMenu, "useE") || !E.IsReady() || !ManaOkay(Slider(JungleClearMenu, "Mana", 20))) {
        return;
    }

    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !ValidTarget(mob, 600.0f) || mob.IsPlant() || mob.IsPet();
            }),
        mobs.end());
    std::sort(
        mobs.begin(),
        mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) {
            return a.MaxHealth() > b.MaxHealth();
        });

    for (const auto& mob : mobs) {
        if (ValidTarget(mob, E.Range)) {
            E.CastOnUnit(mob);
            return;
        }
    }
}

static void KillSteal() {
    if (!Bool(MiscMenu, "killsteal") || !R.IsReady()) {
        return;
    }

    AIHeroClient best;
    float bestHealth = -1.0f;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, R.Range) || WillDieByTristanaE(enemy)) {
            continue;
        }
        if (IsKillable(enemy, R.GetDamage(enemy), DamageType::Physical) &&
            enemy.Health() > bestHealth) {
            best = enemy;
            bestHealth = enemy.Health();
        }
    }
    if (best.IsValid()) {
        R.CastOnUnit(best);
    }
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "gapcloser") || !R.IsReady()) {
        return;
    }

    const auto sender = AIHeroClient(args.Sender);
    if (!ValidHeroTarget(sender, R.Range)) {
        return;
    }

    const auto player = Player();
    if (player.IsValid() && args.End.Distance2D(player.Position()) <= 200.0f) {
        R.CastOnUnit(sender);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Tristana
