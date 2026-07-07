#pragma once

// ============================================================================
// SharpShooter AIO — Graves
// Port từ SharpShooterCSHarp/Plugins/Graves.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q End of the Line — skillshot line 900, delay 0.25, width 45, speed 2000.
//   W Smoke Screen    — skillshot circle 850, delay 0.25, radius 250, speed 1650.
//                       Ưu tiên hero trong tầm nhưng ngoài tầm đánh thường.
//   E Quickdraw       — dash 425, reposition tới con trỏ sau đòn đánh (combo).
//   R Collateral Damage — skillshot line 1100, delay 0.25, width 100, speed 2100.
//                       Finish killable / bắn khi xuyên nhiều mục tiêu.
//
// Ghi chú port:
//   * "Keep Mana for R": chỉ dùng Q/W/E khi mana còn đủ cho R (mana cost theo cấp).
//   * E reposition dùng OnAfterAttack (tương đương Orbwalking.AfterAttack C#).
//   * R multi-hit dùng CastIfWillHit(target, count) khi main target đủ thấp HP.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Graves {

using SDK::Core::Utils::AutoAttack;

inline constexpr int kEManaCost = 40;
inline constexpr int kRManaCost = 100;
inline constexpr int kQManaCost[] = { 0, 50, 55, 60, 65, 70 };
inline constexpr int kWManaCost[] = { 0, 70, 75, 80, 85, 90 };

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 900.0f };
inline Spell W{ SpellSlot::W, 850.0f };
inline Spell E{ SpellSlot::E, 425.0f };
inline Spell R{ SpellSlot::R, 1100.0f };

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

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static AIHeroClient HeroFromInfo(const Core::Events::ObjectInfo& info) {
    ::Core::Objects::ObjectHandle handle{};
    handle.address = info.Ptr;
    handle.index = info.Index;
    handle.networkId = info.NetworkId;
    handle.type = info.Type;
    return AIHeroClient(handle);
}

static bool IsKillable(const AIBaseClient& target, double damage) {
    if (!ValidUnit(target)) {
        return false;
    }
    if (target.HasBuff("kindredrnodeathbuff") || target.HasBuff("Undying Rage") ||
        target.HasBuff("JudicatorIntervention") || target.HasBuff("BansheesVeil") ||
        target.HasBuff("SivirShield") || target.HasBuff("ShroudofDarkness")) {
        return false;
    }
    return target.Health() + target.PhysicalShield() < damage - 2.0;
}

static int ClampLevel(int level) {
    return std::clamp(level, 0, 5);
}

// Có đủ mana cho R sau khi trừ cost của kỹ năng sắp dùng?
static bool HasManaForR(int spellCost) {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    if (!Bool(ComboMenu, "keepManaForR")) {
        return true;
    }
    return player.Mana() - static_cast<float>(spellCost) >= static_cast<float>(kRManaCost);
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnProcessSpell(const ProcessSpellEventArgs& args);
static void OnAfterAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void Combo();
static void Mixed();
static void Clear();
static void Killsteal();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Graves", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuBool("useE", "Use E (reposition)"));
    ComboMenu->Add(new MenuBool("useRKill", "Use R on Killable Target"));
    ComboMenu->Add(new MenuSlider("rHitCount", "Use R if Will Hit >=", 3, 2, 6));
    ComboMenu->Add(new MenuSlider("rHpPercent", "^ And Main Target HP% <=", 80, 10, 100));
    ComboMenu->Add(new MenuBool("keepManaForR", "Keep Mana for R"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q", false));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("killsteal", "Use Killsteal"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (E)"));
    MiscMenu->Add(new MenuBool("antiMelee", "Use Anti-Melee (E)"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 900.0f);
    Q.SetSkillshot(0.25f, 45.0f, 2000.0f, false, SpellType::Line);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 850.0f);
    W.SetSkillshot(0.25f, 250.0f, 1650.0f, false, SpellType::Circle);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 425.0f);

    R = Spell(SpellSlot::R, 1100.0f);
    R.SetSkillshot(0.25f, 100.0f, 2100.0f, false, SpellType::Line);
    R.DamageType = DamageType::Physical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Graves loaded</font>");
}

// Anti-melee E: địch cận chiến auto-attack mình → E lùi ra sau.
static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded || !Bool(MiscMenu, "antiMelee") || !E.IsReady() || !args.IsAutoAttack) {
        return;
    }
    if (args.Target.Ptr == 0) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || player.NetworkId() != static_cast<int>(args.Target.NetworkId)) {
        return;
    }
    const AIHeroClient sender = HeroFromInfo(args.Sender);
    if (sender.IsValid() && sender.IsHero() && sender.IsEnemy() && sender.IsMelee()) {
        E.Cast(player.Position().Extend(sender.Position(), -E.Range));
    }
}

// Sau đòn đánh (combo): W lên target, E reposition tới con trỏ khi ít địch quanh.
static void OnAfterAttack(OrbwalkingActionArgs& args) {
    if (!Loaded || Orbwalker::ActiveMode() != OrbwalkingMode::Combo) {
        return;
    }
    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(targetBase) || !targetBase.IsHero()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(ComboMenu, "useW") && W.IsReady() && HasManaForR(kWManaCost[ClampLevel(W.Level())])) {
        W.Cast(targetBase.Position());
    }

    if (Bool(ComboMenu, "useE") && E.IsReady() && HasManaForR(kEManaCost) &&
        player.CountEnemyHeroesInRange(700.0f) <= 1) {
        E.Cast(player.Position().Extend(Game::CursorPos(), 450.0f));
    }
}

static void Combo() {
    if (LastComboEvalTick != 0 && GetTickCount() - LastComboEvalTick < 60) {
        return;
    }
    LastComboEvalTick = GetTickCount();

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(ComboMenu, "useQ") && Q.IsReady() && HasManaForR(kQManaCost[ClampLevel(Q.Level())])) {
        const auto target = GetTarget(Q.Range, DamageType::Physical);
        if (ValidHeroTarget(target, Q.Range)) {
            const auto pred = Q.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                Q.Cast(pred.GetCastPosition());
            }
        }
    }

    // W: ưu tiên hero trong tầm nhưng ngoài tầm đánh thường.
    if (Bool(ComboMenu, "useW") && W.IsReady() && HasManaForR(kWManaCost[ClampLevel(W.Level())])) {
        AIHeroClient best;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, W.Range) && !AutoAttack::InAutoAttackRange(enemy)) {
                best = enemy;
                break;
            }
        }
        if (best.IsValid()) {
            const auto pred = W.GetPrediction(best);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                W.Cast(pred.GetCastPosition());
            }
        }
    }

    if (R.IsReady()) {
        if (Bool(ComboMenu, "useRKill")) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(enemy, R.Range) && IsKillable(enemy, R.GetDamage(enemy))) {
                    const auto pred = R.GetPrediction(enemy);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        R.Cast(pred.GetCastPosition());
                        return;
                    }
                }
            }
        }

        // R multi-hit: main target đủ thấp HP + xuyên >= N.
        const auto target = GetTarget(R.Range, DamageType::Physical);
        if (ValidHeroTarget(target, R.Range) &&
            target.HealthPercent() <= static_cast<float>(Slider(ComboMenu, "rHpPercent", 80))) {
            R.CastIfWillHit(AIBaseClient(target.Handle()), Slider(ComboMenu, "rHitCount", 3));
        }
    }
}

static void Mixed() {
    if (!Bool(HarassMenu, "useQ") || !Q.IsReady() || !ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        return;
    }
    const auto target = GetTarget(Q.Range, DamageType::Physical);
    if (ValidHeroTarget(target, Q.Range)) {
        const auto pred = Q.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            Q.Cast(pred.GetCastPosition());
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid() || !Q.IsReady()) {
        return;
    }

    // Lane clear Q: bắn hàng lính (>=3).
    if (Bool(LaneClearMenu, "useQ", false) && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
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
            const auto farm = Q.GetLineFarmLocation(targets);
            if (farm.MinionsHit >= 3) {
                Q.Cast(Vector3::From2D(farm.Position));
                return;
            }
        }
    }

    // Jungle clear Q: mob máu cao nhất trong tầm.
    if (Bool(JungleClearMenu, "useQ") && ManaOkay(Slider(JungleClearMenu, "Mana", 20))) {
        auto mobs = GameObjects::Jungle();
        mobs.erase(
            std::remove_if(
                mobs.begin(),
                mobs.end(),
                [](const AIMinionClient& mob) {
                    return !ValidTarget(mob, Q.Range) || mob.IsPlant() || mob.IsPet();
                }),
            mobs.end());
        std::sort(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& a, const AIMinionClient& b) {
                return a.MaxHealth() > b.MaxHealth();
            });
        if (!mobs.empty()) {
            Q.Cast(mobs.front());
        }
    }
}

// Killsteal: quét địch (máu cao nhất trước) và finish bằng Q/W/R nếu chết.
static void Killsteal() {
    if (!Bool(MiscMenu, "killsteal")) {
        return;
    }
    std::vector<AIHeroClient> enemies = GameObjects::EnemyHeroes();
    std::sort(
        enemies.begin(),
        enemies.end(),
        [](const AIHeroClient& a, const AIHeroClient& b) {
            return a.Health() > b.Health();
        });

    for (const auto& enemy : enemies) {
        if (!ValidUnit(enemy)) {
            continue;
        }
        if (Q.IsReady() && ValidHeroTarget(enemy, Q.Range) && IsKillable(enemy, Q.GetDamage(enemy))) {
            const auto pred = Q.GetPrediction(enemy);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                Q.Cast(pred.GetCastPosition());
                return;
            }
        }
        if (W.IsReady() && ValidHeroTarget(enemy, W.Range) && IsKillable(enemy, W.GetDamage(enemy))) {
            const auto pred = W.GetPrediction(enemy);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                W.Cast(pred.GetCastPosition());
                return;
            }
        }
        if (R.IsReady() && ValidHeroTarget(enemy, R.Range) && IsKillable(enemy, R.GetDamage(enemy))) {
            const auto pred = R.GetPrediction(enemy);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                R.Cast(pred.GetCastPosition());
                return;
            }
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
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

    Killsteal();
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "gapcloser") || !E.IsReady()) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (!ValidHeroTarget(sender)) {
        return;
    }
    const std::string name = sender.CharacterName();
    if (_stricmp(name.c_str(), "MasterYi") == 0) {
        return;
    }
    const auto player = Player();
    if (player.IsValid() && args.End.Distance2D(player.Position()) <= 200.0f) {
        E.Cast(player.Position().Extend(sender.Position(), -E.Range));
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Graves
