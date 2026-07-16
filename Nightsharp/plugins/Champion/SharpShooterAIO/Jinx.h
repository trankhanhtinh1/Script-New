#pragma once

// ============================================================================
// SharpShooter AIO — Jinx
// Port từ SharpShooterCSHarp/Plugins/Jinx.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Switcheroo!   — đổi dạng vũ khí (minigun ↔ rocket). "JinxQ" buff = đang
//                     dùng rocket (tầm xa hơn, AoE nhỏ). QSwitch() bật/tắt dạng.
//   W Zap!          — skillshot line 1450, delay 0.6, width 60, speed 3300,
//                     collision (minion) true.
//   E Flame Chompers— skillshot circle 900, delay 1.1 (bẫy trói). Auto lên
//                     mục tiêu bất động / đang di chuyển predict được.
//   R Super Mega Death Rocket! — skillshot line toàn cầu, sát thương theo %HP
//                     đã mất + scale theo khoảng cách. Auto finish killable.
//
// Ghi chú port:
//   * QSwitch: chỉ cast Q khi không đang cast spell, để đổi đúng dạng mong muốn.
//   * Damage tính tay theo wiki (leagueoflegends.com/en-us/Jinx/LoL) — KHÔNG
//     dùng GetDamage. R = base ramp theo khoảng cách 10%→100% + %HP đã mất:
//       max 200/350/500 (+120% bonus AD), min = 10% max = 20/35/50 (+12% bonus AD),
//       + 25/30/35% máu đã mất (không scale theo distance), quy đổi qua giáp.
//   * R có collision check (hero chắn đường) trước khi bắn.
//   * HitchanceSelector cho W của bản C# rút gọn thành hardcode High (tối giản 7UP).
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Jinx {

using SDK::Core::Utils::AutoAttack;

inline constexpr int kDefaultRange = 525;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, FLT_MAX };
inline Spell W{ SpellSlot::W, 1450.0f };
inline Spell E{ SpellSlot::E, 900.0f };
inline Spell R{ SpellSlot::R, 2500.0f };

inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;
inline DWORD WCastTick = 0;

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

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static AIHeroClient GetTargetNoCollision(Spell& spell) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTargetNoCollision(&spell) : AIHeroClient();
}

static bool IsQActive() {
    const auto player = Player();
    return player.IsValid() && player.HasBuff("JinxQ");
}

// Tầm AA khi dùng rocket (Q rocket): 525 + 25*qLevel.
static float QRange() {
    return static_cast<float>(kDefaultRange + 25 * Q.Level());
}

// Killable rút gọn (loại buff bất tử + so máu/shield vật lý).
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

// ── Damage tính tay theo wiki (leagueoflegends.com/en-us/Jinx/LoL) — KHÔNG dùng GetDamage ──
// Q Switcheroo! (Fishbones): chỉ MODIFIER auto-attack (rocket = 110% total AD,
//                            splash vật lý AoE nhỏ). KHÔNG phải nuke cast → không tính ở đây.
// W Zap!  (physical)       : 10 / 60 / 110 / 160 / 210 (+ 140% total AD).
//                            Skillshot line 1450 tầm xa → killsteal tốt.
// E Flame Chompers! (magic): 90 / 140 / 190 / 240 / 290 (+ 100% AP). Root 1.5s (dmg phụ).
// R Super Mega Death Rocket! (physical, 3 rank, toàn cầu):
//     Ramp theo khoảng cách đã bay (10% → 100%):
//       max 200 / 350 / 500 (+ 120% bonus AD)   ← full range
//       min  20 /  35 /  50 (+  12% bonus AD)   ← cận (đúng 10% của max)
//     + %HP đã mất (KHÔNG scale theo distance): 25 / 30 / 35% * (maxHP - HP).
//     → killsteal chủ lực. Quy đổi qua CalculateDamage (giáp).
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float totalAd = player.TotalAttackDamage();
    const float bonusAd = player.BonusAttackDamage();
    const float ap = player.AP();

    switch (slot) {
    case SpellSlot::W: {
        const int rank = W.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 10.0f, 60.0f, 110.0f, 160.0f, 210.0f };
        const float raw = base[rank - 1] + 1.40f * totalAd;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    case SpellSlot::E: {
        const int rank = E.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 90.0f, 140.0f, 190.0f, 240.0f, 290.0f };
        const float raw = base[rank - 1] + 1.00f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    case SpellSlot::R: {
        const int rank = R.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        const int idx = (rank - 1 < 3) ? rank - 1 : 2; // R chỉ có 3 rank.
        static const float maxBase[3] = { 200.0f, 350.0f, 500.0f };
        static const float missPct[3] = { 0.25f, 0.30f, 0.35f };
        // Ramp 10% → 100% theo khoảng cách đã bay (xấp xỉ: đạt max quanh ~1500u).
        const float dist = player.Distance(target.ServerPosition());
        const float factor = std::clamp(0.10f + 0.90f * (dist / 1500.0f), 0.10f, 1.00f);
        float raw = factor * (maxBase[idx] + 1.20f * bonusAd);
        // Bonus %HP đã mất (không scale theo distance).
        const float missing = target.MaxHealth() - target.Health();
        raw += missPct[idx] * missing;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    default:
        return 0.0f;
    }
}

// Đổi dạng vũ khí: activate=true → muốn rocket; false → muốn minigun.
static void QSwitch(bool activate) {
    if (!Q.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || player.Spellbook().IsCastingSpell()) {
        return;
    }
    const bool hasRocket = player.HasBuff("JinxQ");
    if (activate && !hasRocket) {
        Q.Cast();
    } else if (!activate && hasRocket) {
        Q.Cast();
    }
}

// ── Tầm đánh 2 dạng vũ khí ──────────────────────────────────────────────
// Minigun: tầm gốc 525. Rocket (Fishbones): +bonus theo cấp Q.
static float MinigunRange() {
    return static_cast<float>(kDefaultRange); // 525
}
static float RocketRange() {
    // Rocket cộng thêm tầm đánh theo rank Q (~600 rank1 → ~700 rank5).
    return MinigunRange() + 50.0f + 25.0f * static_cast<float>(Q.Level());
}

// Nội tại minigun (jinxqramp) đạt 3 stack = tốc đánh tối đa → ưu tiên minigun.
static bool RampMaxed() {
    const auto player = Player();
    return player.IsValid() && player.GetBuffCount("jinxqramp") >= 3;
}

// Điều khiển dạng vũ khí Q theo target:
//   * target trong tầm minigun → về minigun (đang trong tầm AA, không cần rocket).
//   * target ngoài minigun nhưng trong tầm rocket → bật rocket (đủ tầm mới bật).
//     Ưu tiên này áp dụng cả khi ramp 3 stack: nếu địch KHÔNG trong tầm minigun
//     (không đánh tay tới được) mà bật rocket thì địch lọt tầm → bật rocket để
//     vẫn đánh được, thay vì giữ minigun rồi đứng không.
//   * ramp 3 stack + target đã trong tầm minigun → giữ minigun (giữ tốc đánh nội tại).
//   * xa hơn cả rocket        → giữ nguyên (không đổi vô ích).
static void UpdateQWeapon(const AIHeroClient& target) {
    if (!Q.IsReady() || Q.Level() <= 0) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || !ValidUnit(target)) {
        return;
    }

    const float br = target.BoundingRadius() + player.BoundingRadius();
    const float dist = player.Distance(target);
    const float miniR = MinigunRange() + br;
    const float rockR = RocketRange() + br;

    if (dist <= miniR) {
        // Địch đã trong tầm minigun (AA range) → về minigun (giữ ramp nếu có).
        QSwitch(false);
    } else if (dist <= rockR) {
        // Địch ngoài tầm minigun nhưng trong tầm rocket → bật rocket để với tới.
        // Áp dụng cả khi ramp 3 stack: đánh được quan trọng hơn giữ nội tại.
        QSwitch(true);
    } else if (RampMaxed()) {
        // Xa hơn cả rocket + ramp maxed → về minigun giữ tốc đánh nội tại.
        QSwitch(false);
    }
}

// Kiểm tra hero chắn đường R tại vị trí predict.
static bool RCollides(const AIBaseClient& target, const Vector3& castPos) {
    SDK::PredictionInput input;
    input.Unit = Player();
    input.Delay = R.Delay;
    input.Speed = R.Speed;
    input.Radius = R.Width > 1.0f ? R.Width : 140.0f;
    input.SetCollisionObjects(SDK::CollisionableObjects::Heroes);

    const auto collisions = Collision::GetCollision({ castPos }, input);
    for (const auto& unit : collisions) {
        if (unit.NetworkId() != target.NetworkId()) {
            return true;
        }
    }
    return false;
}

static void TryCastR(const AIHeroClient& target) {
    if (!ValidHeroTarget(target, R.Range)) {
        return;
    }
    const auto pred = R.GetPrediction(target);
    if (!HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
        return;
    }
    if (!RCollides(target, pred.GetUnitPosition())) {
        R.Cast(pred.GetCastPosition());
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnProcessSpell(const ProcessSpellEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void AutoKillsteal();
static void Combo();
static void Mixed();
static void Clear();
static void AutoR();
static void AutoE();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Jinx", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q (weapon switch)"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("useR", "Use R (finisher)"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuBool("useW", "Use W"));
    HarassMenu->Add(new MenuBool("autoHarass", "Auto Harass (W)"));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useW", "Use W"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("killsteal", "Auto Killsteal (R/W)"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (E)"));
    MiscMenu->Add(new MenuBool("interrupter", "Interrupter (E)"));
    MiscMenu->Add(new MenuBool("autoE", "Auto E on Immobile Target"));
    MiscMenu->Add(new MenuBool("autoR", "Auto R on Killable Target"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, FLT_MAX);

    W = Spell(SpellSlot::W, 1450.0f);
    W.SetSkillshot(0.6f, 60.0f, 3300.0f, true, SpellType::Line);
    W.DamageType = DamageType::Physical;

    E = Spell(SpellSlot::E, 900.0f);
    E.SetSkillshot(1.1f, 1.0f, 1750.0f, false, SpellType::Circle);
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 2500.0f);
    R.SetSkillshot(0.6f, 140.0f, 1700.0f, false, SpellType::Line);
    R.DamageType = DamageType::Physical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Jinx loaded</font>");
}

static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded || !Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (args.Slot == static_cast<int>(SpellSlot::W)) {
        WCastTick = GetTickCount();
    }
}

// Combo: bật rocket khi target ngoài tầm minigun hoặc đông địch; W khi không có
// địch sát mình; E lên mục tiêu di chuyển; R finish.
static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Q weapon-switch: chỉ bật rocket khi target ngoài tầm minigun mà vẫn trong
    // tầm rocket; tự về minigun khi target đã vào tầm AA hoặc ramp đủ 3 stack.
    if (Bool(ComboMenu, "useQ") && Q.IsReady()) {
        const auto target = GetTarget(RocketRange() + player.BoundingRadius() + 200.0f,
                                      DamageType::Physical);
        if (ValidHeroTarget(target)) {
            UpdateQWeapon(target);
        }
    }

    if (Bool(ComboMenu, "useW") && W.IsReady() && player.CountEnemyHeroesInRange(400.0f) == 0) {
        const auto target = GetTargetNoCollision(W);
        if (ValidHeroTarget(target, W.Range)) {
            W.Cast(target);
        }
    }

    // Hoist snapshot EnemyHeroes 1 lần cho cả nhánh useE và useR (frame đóng băng = y hệt).
    const auto comboHeroes = GameObjects::EnemyHeroes();

    if (Bool(ComboMenu, "useE") && E.IsReady()) {
        AIHeroClient best;
        float bestDist = FLT_MAX;
        for (const auto& enemy : comboHeroes) {
            if (!ValidHeroTarget(enemy, 600.0f) || !enemy.IsMoving()) {
                continue;
            }
            const auto pred = E.GetPrediction(enemy);
            if (!HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                continue;
            }
            const float d = player.Distance(enemy);
            if (d < bestDist) {
                bestDist = d;
                best = enemy;
            }
        }
        if (best.IsValid()) {
            E.Cast(best);
        }
    }

    if (Bool(ComboMenu, "useR") && R.IsReady() && WCastTick + 1060 <= GetTickCount()) {
        for (const auto& enemy : comboHeroes) {
            if (enemy.IsZombie() || enemy.CountAllyHeroesInRange(500.0f) >= 2) {
                continue;
            }
            if (player.Distance(enemy) < QRange()) {
                continue;
            }
            if (IsKillable(enemy, SpellDamage(SpellSlot::R, enemy))) {
                TryCastR(enemy);
                break;
            }
        }
    }
}

static void Mixed() {
    const auto player = Player();
    if (!player.IsValid() || !ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        QSwitch(false);
        return;
    }

    if (Bool(HarassMenu, "useQ") && Q.IsReady()) {
        const auto target = GetTarget(RocketRange() + player.BoundingRadius() + 200.0f,
                                      DamageType::Physical);
        if (ValidHeroTarget(target)) {
            UpdateQWeapon(target);
        } else {
            QSwitch(false);
        }
    } else {
        QSwitch(false);
    }

    if (Bool(HarassMenu, "useW") && W.IsReady() && player.CountEnemyHeroesInRange(400.0f) == 0) {
        const auto target = GetTargetNoCollision(W);
        if (ValidHeroTarget(target, W.Range)) {
            W.Cast(target);
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Jungle clear W: mob máu cao nhất predict được.
    if (Bool(JungleClearMenu, "useW") && W.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20))) {
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
            const auto pred = W.GetPrediction(mob);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                W.Cast(mob);
                break;
            }
        }
    }
}

// Auto R lên mục tiêu killable (độc lập chế độ orbwalk).
static void AutoR() {
    if (!Bool(MiscMenu, "autoR") || !R.IsReady() || WCastTick + 1060 > GetTickCount()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (enemy.IsZombie() || enemy.CountAllyHeroesInRange(500.0f) >= 2) {
            continue;
        }
        if (player.Distance(enemy) < QRange()) {
            continue;
        }
        if (IsKillable(enemy, SpellDamage(SpellSlot::R, enemy))) {
            TryCastR(enemy);
            break;
        }
    }
}

// Auto E lên mục tiêu bất động > 0.5s.
static void AutoE() {
    if (!Bool(MiscMenu, "autoE") || !E.IsReady()) {
        return;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, E.Range)) {
            continue;
        }
        const auto pred = E.GetPrediction(enemy);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::Immobile)) {
            E.Cast(enemy);
            break;
        }
    }
}

// Auto killsteal: chạy mọi mode, gate bằng toggle "killsteal".
// Ưu tiên R (toàn cầu, chủ lực + bonus %HP đã mất) → W (line 1450 tầm xa).
// Damage tính tay qua SpellDamage (wiki), KHÔNG dùng GetDamage. Chỉ cast khi hạ gục.
static void AutoKillsteal() {
    if (!Bool(MiscMenu, "killsteal")) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // R Super Mega Death Rocket! — chủ lực, tầm 2500, no-collision target.
    // Tránh tự cast lên chính mình sau W (giữ delay như AutoR) và không R khi
    // địch quá cận (trong tầm minigun/QRange) để dành cho AA/W.
    if (R.IsReady() && WCastTick + 1060 <= GetTickCount()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, R.Range) || enemy.IsZombie()) {
                continue;
            }
            if (player.Distance(enemy) < QRange()) {
                continue;
            }
            if (IsKillable(enemy, SpellDamage(SpellSlot::R, enemy))) {
                TryCastR(enemy);
                return;
            }
        }
    }

    // W Zap! — skillshot line 1450, collision (minion). Killsteal tầm xa.
    if (W.IsReady()) {
        const auto target = GetTargetNoCollision(W);
        if (ValidHeroTarget(target, W.Range) && IsKillable(target, SpellDamage(SpellSlot::W, target))) {
            const auto pred = W.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                W.Cast(pred.GetCastPosition());
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

    // Auto killsteal: chạy mọi mode, không phụ thuộc combo (gate bằng toggle).
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

    AutoR();
    AutoE();
}

// Bản C# dùng before-attack để ép minigun khi last-hit; tối giản: bỏ qua.
static void OnBeforeAttack(OrbwalkingActionArgs&) {
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "gapcloser") || !E.IsReady()) {
        return;
    }
    const auto player = Player();
    if (player.IsValid() && args.End.Distance2D(player.Position()) <= 200.0f) {
        E.Cast(args.End);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Jinx
