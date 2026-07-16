#pragma once

// ============================================================================
// SharpShooter AIO — Twitch
// Port từ SharpShooterCSHarp/Plugins/Twitch.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Ambush        — stealth self-buff (dùng cho stealth recall).
//   W Venom Cask     — skillshot circle 950, delay 0.25, radius 100, speed 1400.
//   E Contaminate    — no-target, nổ stack "twitchdeadlyvenom" trên địch trong
//                      tầm 1200 (cast khi có địch >=6 stack hoặc chết bởi E).
//   Recall           — dùng chung với Q cho stealth recall keybind.
//
// Ghi chú port:
//   * IsKillableAndValidTarget port thành helper cục bộ (như Tristana.h).
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <vector>

namespace Plugins::SharpAIO::Twitch {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, FLT_MAX };
inline Spell W{ SpellSlot::W, 950.0f };
inline Spell E{ SpellSlot::E, 1200.0f };
inline Spell R{ SpellSlot::R, 850.0f };
inline Spell Recall{ SpellSlot::Recall, FLT_MAX };

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

static bool KeyActive(Menu* menu, const char* key) {
    if (!menu) {
        return false;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item && item->Active;
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

// Port rút gọn của ExtraExtensions.IsKillableAndValidTarget.
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

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

// ── Damage tính tay theo wiki (leagueoflegends.com/en-us/Twitch/LoL) ──
// KHÔNG dùng Spell::GetDamage. Chốt số ngày 2026-07-08.
//
// Passive Deadly Venom (TRUE, không tính trong spell damage):
//   1/2/3/4/5 (theo cấp) (+3% AP) true/s mỗi stack, tối đa 6 stack, 6s.
//
// Q Ambush        : stealth self-buff, KHÔNG gây sát thương.
// W Venom Cask    : KHÔNG gây sát thương trực tiếp. Chỉ apply Deadly Venom +
//                   slow 30/35/40/45/50% (+6% mỗi 100 AP). => loại khỏi killsteal.
// E Contaminate   : MIXED = physical + magic, dựa trên số stack Deadly Venom
//                   trên target lúc bắt đầu cast:
//     base physical/rank : 20 / 30 / 40 / 50 / 60
//     per-stack physical : 15 / 20 / 25 / 30 / 35   (+35% bonus AD mỗi stack)
//     per-stack magic    : 35% AP mỗi stack
//     -> min (1 stack)  : 35/50/65/80/95    (+35% bonus AD)  (+35% AP)
//     -> max (6 stack)  : 110/150/190/230/270 (+210% bonus AD) (+210% AP)
//   Stack đọc từ buff "twitchdeadlyvenom" trên target (GetBuffCount), clamp 0..6.
// R Spray and Pray: chuyển auto thành bolt xuyên (+30/45/60 bonus AD, +range),
//                   KHÔNG có nuke cast trực tiếp. => SpellDamage = 0.
//
// Physical trừ giáp, magic trừ kháng phép riêng qua CalculateMixedDamage.
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float bonusAd = player.BonusAttackDamage();
    const float ap = player.AP();

    switch (slot) {
    case SpellSlot::E: {
        const int rank = E.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float basePhys[5] = { 20.0f, 30.0f, 40.0f, 50.0f, 60.0f };
        static const float perStackPhys[5] = { 15.0f, 20.0f, 25.0f, 30.0f, 35.0f };
        int stacks = target.GetBuffCount("twitchdeadlyvenom");
        if (stacks < 0) {
            stacks = 0;
        }
        if (stacks > 6) {
            stacks = 6;
        }
        const float s = static_cast<float>(stacks);
        const float physRaw = basePhys[rank - 1] + s * (perStackPhys[rank - 1] + 0.35f * bonusAd);
        const float magicRaw = s * (0.35f * ap);
        return Damage::CalculateMixedDamage(player, target, physRaw, magicRaw);
    }
    default:
        // Q/W/R không gây sát thương trực tiếp theo wiki hiện tại.
        return 0.0f;
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void AutoKillsteal();
static void Combo();
static void Clear();
static void StealthRecall();
static void OnUnload();

// E nên nổ khi có ít nhất 1 địch chết bởi E hoặc đã đủ 6 stack venom.
static bool ShouldContaminate(float range) {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, range)) {
            continue;
        }
        if (enemy.GetBuffCount("twitchdeadlyvenom") >= 6 ||
            IsKillable(enemy, SpellDamage(SpellSlot::E, enemy), DamageType::Physical)) {
            return true;
        }
    }
    return false;
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Twitch", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuBool("useE", "Use E (finisher)"));
    ComboMenu->Add(new MenuBool("useEOutRange", "Use E if target escaping E range"));
    ComboMenu->Add(new MenuSlider("eOutRangeStack", "-> if venom stacks >=", 3, 1, 6));
    ComboMenu->Add(new MenuBool("useR", "Use R (Spray and Pray)", false));
    ComboMenu->Add(new MenuSlider("rCount", "-> if enemies in range >=", 2, 1, 5));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useW", "Use W", false));
    JungleClearMenu->Add(new MenuBool("useE", "Use E"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("killsteal", "Killsteal (E)"));
    MiscMenu->Add(new MenuKeyBind("stealthRecall", "Stealth Recall (Q + B)", 'T', KeyBindType::Press));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, FLT_MAX);

    W = Spell(SpellSlot::W, 950.0f);
    W.SetSkillshot(0.25f, 100.0f, 1400.0f, false, SpellType::Circle);
    W.DamageType = DamageType::True;

    E = Spell(SpellSlot::E, 1200.0f);
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 850.0f);
    R.DamageType = DamageType::Physical;

    Recall = Spell(SpellSlot::Recall, FLT_MAX);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Twitch loaded</font>");
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
        return;
    }

    // Auto killsteal: chạy mọi mode, không phụ thuộc combo (giống Corki).
    AutoKillsteal();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::LaneClear:
        Clear();
        break;
    default:
        break;
    }

    StealthRecall();
}

static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }

    if (Bool(ComboMenu, "useW") && W.IsReady()) {
        const auto target = GetTarget(W.Range, DamageType::True);
        if (ValidHeroTarget(target, W.Range)) {
            const auto pred = W.GetPrediction(target, true);
            if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::High)) {
                W.Cast(pred.GetCastPosition());
            }
        }
    }

    if (Bool(ComboMenu, "useE") && E.IsReady() && ShouldContaminate(E.Range)) {
        E.Cast();
        return;
    }

    // Hoist snapshot EnemyHeroes 1 lần cho cả nhánh useEOutRange và useR (frame đóng băng = y hệt).
    const auto comboHeroes = GameObjects::EnemyHeroes();

    // E out-range escape (C# ComboUseEOutRange): con mồi dính đủ stack đang ở rìa
    // tầm E và sẽ thoát khỏi tầm sau ~0.6s → nổ E ngay trước khi mất mục tiêu.
    if (Bool(ComboMenu, "useEOutRange") && E.IsReady()) {
        const int minStack = Slider(ComboMenu, "eOutRangeStack", 3);
        for (const auto& enemy : comboHeroes) {
            if (!ValidHeroTarget(enemy, E.Range) ||
                enemy.GetBuffCount("twitchdeadlyvenom") < minStack) {
                continue;
            }
            const float dist = enemy.DistanceToPlayer();
            if (dist <= E.Range - 100.0f) {
                continue; // còn sâu trong tầm, chưa cần nổ vội.
            }
            const auto pred = Prediction::GetPrediction(AIBaseClient(enemy.Handle()), 0.6f);
            if (pred.GetCastPosition().Distance(Player().Position()) >= E.Range) {
                E.Cast();
                return;
            }
        }
    }

    // R Spray and Pray (C# ComboUseR): bật R khi có >= N địch trong tầm (teamfight).
    if (Bool(ComboMenu, "useR", false) && R.IsReady()) {
        const int need = Slider(ComboMenu, "rCount", 2);
        int count = 0;
        for (const auto& enemy : comboHeroes) {
            if (ValidHeroTarget(enemy, R.Range)) {
                ++count;
            }
        }
        if (count >= need) {
            R.Cast();
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid() || !ManaOkay(Slider(JungleClearMenu, "Mana", 20))) {
        return;
    }

    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !ValidTarget(mob, E.Range) || mob.IsPlant() || mob.IsPet();
            }),
        mobs.end());

    if (mobs.empty()) {
        return;
    }

    if (Bool(JungleClearMenu, "useW", false) && W.IsReady()) {
        std::vector<AIBaseClient> targets;
        targets.reserve(mobs.size());
        for (const auto& mob : mobs) {
            if (ValidTarget(mob, W.Range)) {
                targets.push_back(AIBaseClient(mob.Handle()));
            }
        }
        if (!targets.empty()) {
            const auto farm = W.GetCircularFarmLocation(targets);
            if (farm.MinionsHit >= 1) {
                W.Cast(Vector3::From2D(farm.Position));
            }
        }
    }

    if (Bool(JungleClearMenu, "useE") && E.IsReady()) {
        // Nổ E nếu có mob trong tầm (jungle clear đơn giản).
        for (const auto& mob : mobs) {
            if (ValidTarget(mob, E.Range)) {
                E.Cast();
                break;
            }
        }
    }
}

// Auto killsteal E Contaminate (no-target, tầm 1200). Damage tính tay theo wiki
// qua SpellDamage (KHÔNG dùng GetDamage) — đã tính stack Deadly Venom trên từng
// địch. W/R không có nuke trực tiếp nên không tham gia killsteal.
static void AutoKillsteal() {
    if (!Bool(MiscMenu, "killsteal") || !E.IsReady()) {
        return;
    }

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy, E.Range) &&
            IsKillable(enemy, SpellDamage(SpellSlot::E, enemy), DamageType::Physical)) {
            E.Cast();
            return;
        }
    }
}

static void StealthRecall() {
    if (!KeyActive(MiscMenu, "stealthRecall")) {
        return;
    }
    if (Q.IsReady() && Extensions::IsReady(SpellSlot::Recall)) {
        Q.Cast();
        Recall.Cast();
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Twitch
