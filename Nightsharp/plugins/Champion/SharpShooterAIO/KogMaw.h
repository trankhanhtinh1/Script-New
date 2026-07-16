#pragma once

// ============================================================================
// SharpShooter AIO — Kog'Maw
// Port từ SharpShooterCSHarp/Plugins/KogMaw.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Caustic Spittle    — skillshot line 950, delay 0.25, width 70, speed 1650, collision.
//   W Bio-Arcane Barrage — self-buff (tăng tầm đánh), cast trước khi đánh thường.
//                          Range động = 565 + 60 + level*30 + 65.
//   E Void Ooze          — skillshot line 1260, delay 0.5, width 120, speed 1400.
//   R Living Artillery    — skillshot circle, range động = 900 + level*300, delay 1.5,
//                          radius 225. Có cost-stack buff "kogmawlivingartillerycost".
//
// Ghi chú port:
//   * "Keep Mana For W": giữ đủ mana để còn cast W (mana - spellCost >= W.ManaCost).
//   * R Stacks Limit: chỉ tự bắn R khi số stack cost hiện tại < ngưỡng; nếu vượt
//     ngưỡng thì chỉ bắn R để kết liễu (killable).
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::KogMaw {

using SDK::Core::Utils::AutoAttack;

inline const char* const kRCostBuff = "kogmawlivingartillerycost";

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 950.0f };
inline Spell W{ SpellSlot::W, FLT_MAX };
inline Spell E{ SpellSlot::E, 1260.0f };
inline Spell R{ SpellSlot::R, 1000.0f };

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

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static float HealthRegenRate(const AIBaseClient& unit) {
    return unit.IsValid() ? ::CoreAIHeroClient::HealthRegenRate(unit.Address()) : 0.0f;
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static AIHeroClient GetTargetNoCollision(Spell& spell) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTargetNoCollision(&spell) : AIHeroClient();
}

// Rút gọn IsKillableAndValidTarget: loại buff bất tử phổ biến + so máu/hồi/shield.
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
    const float shield = damageType == DamageType::Physical
        ? target.PhysicalShield()
        : target.MagicalShield();
    return target.Health() + HealthRegenRate(target) + shield < calculatedDamage - 2.0;
}

// ── Damage tính tay theo wiki (leagueoflegends.com) — KHÔNG dùng DamageData ──
// Q Caustic Spittle (magic) : 80/125/170/215/260 + 90% AP
// E Void Ooze       (magic) : 70/110/150/190/230 + 65% AP
// R Living Artillery (magic, 3 rank):
//     Tối thiểu : 100/140/180 + 75% bonus AD + 35/40/45% AP
//     Tối đa    : 200/280/360 + 150% bonus AD + 70/80/90% AP
//     Low-HP rule (wiki): sát thương tăng 0%–50% theo máu đã mất, và "100% if
//     the target is below 40% maximum health" → khi mục tiêu dưới 40% máu tối đa
//     thì nhân đôi (tối thiểu → tối đa). Ta dùng ngưỡng 40% để nhân đôi raw.
// Trả về damage đã trừ giáp/kháng phép qua Damage::CalculateDamage.
inline constexpr float kRLowHpThresholdPct = 40.0f;
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float bonusAd = player.BonusAttackDamage();
    const float ap = player.AP();

    switch (slot) {
    case SpellSlot::Q: {
        const int rank = Q.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 80.0f, 125.0f, 170.0f, 215.0f, 260.0f };
        const float raw = base[rank - 1] + 0.90f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    case SpellSlot::E: {
        const int rank = E.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 70.0f, 110.0f, 150.0f, 190.0f, 230.0f };
        const float raw = base[rank - 1] + 0.65f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    case SpellSlot::R: {
        const int rank = R.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[3] = { 100.0f, 140.0f, 180.0f };
        static const float apRatio[3] = { 0.35f, 0.40f, 0.45f };
        const int idx = (rank - 1 < 3) ? rank - 1 : 2;
        float raw = base[idx] + 0.75f * bonusAd + apRatio[idx] * ap;
        // Dưới 40% máu tối đa → nhân đôi (tối thiểu → tối đa) theo wiki.
        if (target.HealthPercent() < kRLowHpThresholdPct) {
            raw *= 2.0f;
        }
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    default:
        return 0.0f;
    }
}

// Còn đủ mana để cast W sau khi tiêu spellCost? (Keep Mana For W)
static bool KeepManaForW(Menu* menu, Spell& spell) {
    if (!Bool(menu, "keepManaForW")) {
        return true;
    }
    if (W.Level() <= 0) {
        return true;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return true;
    }
    const float spellCost = spell.Instance().ManaCost();
    const float wCost = W.Instance().ManaCost();
    return player.Mana() - spellCost >= wCost;
}

static int RCostStacks() {
    const auto player = Player();
    return player.IsValid() ? player.GetBuffCount(kRCostBuff) : 0;
}

// Ngưỡng máu enemy để cho phép R trong tầm AA.
inline constexpr float kRInAaRangeHealthPct = 40.0f;

// Ưu tiên đánh tay (W->E->Q) khi enemy trong tầm AA -> hạn chế R.
// R chỉ được phép bắn khi enemy ngoài tầm AA; nếu enemy đã trong tầm AA thì
// chỉ bắn R khi máu enemy < 40% (để kết liễu), còn lại nhường cho đòn tay.
static bool RAllowedOnTarget(const AIBaseClient& target) {
    if (!AutoAttack::InAutoAttackRange(target)) {
        return true;
    }
    return target.HealthPercent() < kRInAaRangeHealthPct;
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void AutoKillsteal();
static void Combo();
static void Mixed();
static void Clear();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Kog'Maw", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("useR", "Use R"));
    ComboMenu->Add(new MenuSlider("rStacks", "R Stacks Limit", 3, 1, 6));
    ComboMenu->Add(new MenuSlider("rMana", "R If Mana > %", 40, 0, 100));
    ComboMenu->Add(new MenuBool("keepManaForW", "Keep Mana For W"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuBool("useE", "Use E"));
    HarassMenu->Add(new MenuBool("useR", "Use R"));
    HarassMenu->Add(new MenuSlider("rStacks", "R Stacks Limit", 1, 1, 6));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useE", "Use E", false));
    LaneClearMenu->Add(new MenuBool("useR", "Use R", false));
    LaneClearMenu->Add(new MenuSlider("rStacks", "R Stacks Limit", 1, 1, 6));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useW", "Use W"));
    JungleClearMenu->Add(new MenuBool("useE", "Use E"));
    JungleClearMenu->Add(new MenuBool("useR", "Use R"));
    JungleClearMenu->Add(new MenuSlider("rStacks", "R Stacks Limit", 1, 1, 6));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("killsteal", "Auto Killsteal (Q/R)"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 950.0f);
    Q.SetSkillshot(0.25f, 70.0f, 1650.0f, true, SpellType::Line);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, FLT_MAX);

    E = Spell(SpellSlot::E, 1260.0f);
    E.SetSkillshot(0.5f, 120.0f, 1400.0f, false, SpellType::Line);
    E.DamageType = DamageType::Magical;

    R = Spell(SpellSlot::R, 1000.0f);
    R.SetSkillshot(1.5f, 225.0f, FLT_MAX, false, SpellType::Circle);
    R.DamageType = DamageType::Magical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Kog'Maw loaded</font>");
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
        return;
    }

    // Range động theo cấp W/R.
    W.Range = 565.0f + 60.0f + static_cast<float>(W.Level()) * 30.0f + 65.0f;
    R.Range = 900.0f + static_cast<float>(R.Level()) * 300.0f;

    // Auto killsteal: chạy mọi mode, không phụ thuộc combo.
    AutoKillsteal();

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

// Auto killsteal: R Living Artillery (no-collision, tầm xa nhất) → Q Caustic
// Spittle (skillshot line). Damage tính tay theo wiki qua SpellDamage. Chỉ cast
// khi hạ gục được. R nhân đôi sát thương với mục tiêu dưới 40% máu tối đa.
// E Void Ooze bỏ qua killsteal: chủ yếu là slow, burst thấp, ưu tiên combo.
static void AutoKillsteal() {
    if (!Bool(MiscMenu, "killsteal")) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // R Living Artillery: target no-collision, tầm động 900 + level*300.
    if (R.IsReady()) {
        const auto target = GetTargetNoCollision(R);
        if (ValidHeroTarget(target, R.Range) &&
            IsKillable(target, SpellDamage(SpellSlot::R, target), DamageType::Magical)) {
            const auto pred = R.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                R.Cast(pred.GetCastPosition());
                return;
            }
        }
    }

    // Q Caustic Spittle: skillshot line 950, collision.
    if (Q.IsReady()) {
        const auto target = GetTargetNoCollision(Q);
        if (ValidHeroTarget(target, Q.Range) &&
            IsKillable(target, SpellDamage(SpellSlot::Q, target), DamageType::Magical)) {
            const auto pred = Q.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                Q.Cast(pred.GetCastPosition());
            }
        }
    }
}

static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }

    if (Bool(ComboMenu, "useQ") && Q.IsReady() && KeepManaForW(ComboMenu, Q)) {
        const auto target = GetTargetNoCollision(Q);
        if (ValidHeroTarget(target, Q.Range)) {
            Q.Cast(target);
        }
    }

    // Hoist snapshot EnemyHeroes 1 lần cho cả nhánh useW và useR-finish (frame đóng băng = y hệt).
    const auto comboHeroes = GameObjects::EnemyHeroes();

    if (Bool(ComboMenu, "useW") && W.IsReady()) {
        for (const auto& enemy : comboHeroes) {
            if (ValidHeroTarget(enemy, W.Range)) {
                W.Cast();
                break;
            }
        }
    }

    if (Bool(ComboMenu, "useE") && E.IsReady() && KeepManaForW(ComboMenu, E)) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target, E.Range)) {
            const auto pred = E.GetPrediction(target, true);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }

    if (Bool(ComboMenu, "useR") && R.IsReady() && KeepManaForW(ComboMenu, R) &&
        ManaOkay(Slider(ComboMenu, "rMana", 40))) {
        if (RCostStacks() < Slider(ComboMenu, "rStacks", 3)) {
            const auto target = GetTarget(R.Range, DamageType::Magical);
            if (ValidHeroTarget(target, R.Range) && RAllowedOnTarget(target)) {
                const auto pred = R.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    R.Cast(pred.GetCastPosition());
                }
            }
        } else {
            // Vượt ngưỡng stack: chỉ bắn để kết liễu.
            for (const auto& enemy : comboHeroes) {
                if (!ValidHeroTarget(enemy, R.Range)) {
                    continue;
                }
                if (IsKillable(enemy, SpellDamage(SpellSlot::R, enemy), DamageType::Magical)) {
                    const auto pred = R.GetPrediction(enemy);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        R.Cast(pred.GetCastPosition());
                        break;
                    }
                }
            }
        }
    }
}

static void Mixed() {
    const auto player = Player();
    if (!player.IsValid() || !ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        return;
    }

    if (Bool(HarassMenu, "useQ") && Q.IsReady()) {
        const auto target = GetTargetNoCollision(Q);
        if (ValidHeroTarget(target, Q.Range)) {
            Q.Cast(target);
        }
    }

    if (Bool(HarassMenu, "useE") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target, E.Range)) {
            const auto pred = E.GetPrediction(target, true);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }

    if (Bool(HarassMenu, "useR") && R.IsReady() && RCostStacks() < Slider(HarassMenu, "rStacks", 1)) {
        const auto target = GetTarget(R.Range, DamageType::Magical);
        if (ValidHeroTarget(target, R.Range) && RAllowedOnTarget(target)) {
            const auto pred = R.GetPrediction(target, true);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                R.Cast(pred.GetCastPosition());
            }
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Hoist snapshot lane-minion (fallback EnemyMinions) 1 lần cho cả nhánh useE và useR
    // (frame đã đóng băng nên phân giải nguồn lính = kết quả y hệt), tránh copy lại vector.
    auto laneMinionsSnap = GameObjects::EnemyLaneMinions();
    if (laneMinionsSnap.empty()) {
        laneMinionsSnap = GameObjects::EnemyMinions();
    }

    // Lane clear E: bắn dàn lính trên đường (>=4).
    if (Bool(LaneClearMenu, "useE", false) && E.IsReady() && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
        std::vector<AIBaseClient> targets;
        targets.reserve(laneMinionsSnap.size());
        for (const auto& minion : laneMinionsSnap) {
            if (ValidTarget(minion, E.Range)) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        if (!targets.empty()) {
            const auto farm = E.GetLineFarmLocation(targets);
            if (farm.MinionsHit >= 4) {
                E.Cast(Vector3::From2D(farm.Position));
            }
        }
    }

    // Lane clear R: bắn cụm lính (>=4) khi còn dưới ngưỡng stack.
    if (Bool(LaneClearMenu, "useR", false) && R.IsReady() &&
        ManaOkay(Slider(LaneClearMenu, "Mana", 60)) &&
        RCostStacks() < Slider(LaneClearMenu, "rStacks", 1)) {
        std::vector<AIBaseClient> targets;
        targets.reserve(laneMinionsSnap.size());
        for (const auto& minion : laneMinionsSnap) {
            if (ValidTarget(minion, R.Range)) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        if (!targets.empty()) {
            const auto farm = R.GetCircularFarmLocation(targets);
            if (farm.MinionsHit >= 4) {
                R.Cast(Vector3::From2D(farm.Position));
            }
        }
    }

    // Jungle clear E / R: mob máu cao nhất trong tầm.
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

    if (mobs.empty()) {
        return;
    }
    const auto& mob = mobs.front();

    if (Bool(JungleClearMenu, "useE") && E.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
        ValidTarget(mob, E.Range)) {
        E.Cast(mob.Position());
    }

    if (Bool(JungleClearMenu, "useR") && R.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
        RCostStacks() < Slider(JungleClearMenu, "rStacks", 1) && ValidTarget(mob, R.Range)) {
        R.Cast(mob.Position());
    }
}

// W bật trước khi đánh thường (combo hoặc jungle mob khi laneclear).
// Chặn đánh thường khi đang trong trạng thái W (kogmawicathiansurprise).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }

    const auto player = Player();
    if (player.IsValid() && player.HasBuff("kogmawicathiansurprise")) {
        args.Process = false;
    }

    if (!W.IsReady()) {
        return;
    }

    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(targetBase)) {
        return;
    }

    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo) {
        if (Bool(ComboMenu, "useW") && Extensions::IsValidTarget(targetBase, W.Range, true)) {
            W.Cast();
        }
    } else if (mode == OrbwalkingMode::LaneClear) {
        if (Bool(JungleClearMenu, "useW") && targetBase.IsMinion() &&
            targetBase.Team() == GameObjectTeam::Neutral) {
            W.Cast();
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::KogMaw
