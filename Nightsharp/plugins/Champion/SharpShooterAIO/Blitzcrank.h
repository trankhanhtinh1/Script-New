#pragma once

// ============================================================================
// SharpShooter AIO — Blitzcrank
// Port từ CSharpFiles/Blitzcrank/Blitzcrank.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Corki.h.
//
// Kỹ năng:
//   Q Rocket Grab  — skillshot line 1050, delay 0.25, width 70, speed 1800, collision.
//   W Overdrive    — self buff (tăng tốc/tốc đánh), cast khi target xa để đuổi.
//   E Power Fist   — on-hit knockup: cast trước đòn đánh (OnBeforeAttack).
//   R Static Field — AoE nuke quanh 600: killable / low-HP / phá shield / interrupt.
//
// Ghi chú port (giữ 1-1 với C#):
//   * AntiGapCloser: Q bắt địch đang dash (hitchance Dash), gate bởi "AntiQGap".
//   * AutoCQDash: tự Q địch đang dash mọi lúc.
//   * Combo: W đuổi khi target xa + Q; không Q khi đang có PowerFist buff và có
//     orbwalker target (giữ đòn E knockup).
//   * RLogic: 4 nhánh — killable, low-HP%, phá shield theo count, interrupt.
//   * MISSING API: Interrupter.OnInterrupterSpell event → thay bằng per-frame
//     Extensions::IsCastingInterruptableSpell (như Caitlyn AutoWCC). Xem missapi.md.
//   * Damage tính tay theo wiki (patch V26.x) — KHÔNG dùng Spell::GetDamage.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Blitzcrank {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1050.0f };
inline Spell W{ SpellSlot::W, FLT_MAX };
inline Spell E{ SpellSlot::E, FLT_MAX };
inline Spell R{ SpellSlot::R, 600.0f };

inline bool Loaded = false;

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

static int ListIndex(Menu* menu, const char* key, int fallback = 0) {
    return menu ? menu->GetListIndex(key, fallback) : fallback;
}

static int SliderButtonValue(Menu* menu, const char* key, int fallback) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuSliderButton>(key);
    return item ? item->Value : fallback;
}

static bool SliderButtonEnabled(Menu* menu, const char* key, bool fallback) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuSliderButton>(key);
    return item ? item->Enabled : fallback;
}

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
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

// ── Damage tính tay theo wiki (leagueoflegends.com/Blitzcrank, patch V26.x) ──
// KHÔNG dùng Spell::GetDamage. Chốt số ngày 2026-07-08.
//
// Q Rocket Grab (MAGIC): 110/160/210/260/310 + 120% AP.
// R Static Field (MAGIC): 275/400/525 + 100% AP.
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float ap = player.AP();

    switch (slot) {
    case SpellSlot::Q: {
        const int rank = Q.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 110.0f, 160.0f, 210.0f, 260.0f, 310.0f };
        const float raw = base[rank - 1] + 1.20f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    case SpellSlot::R: {
        const int rank = R.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        const int idx = (rank - 1 < 3) ? rank - 1 : 2;
        static const float base[3] = { 275.0f, 400.0f, 525.0f };
        const float raw = base[idx] + 1.00f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    default:
        return 0.0f;
    }
}

// C#: IsInterruptUnit — địch nằm trong interrupt list (menu per-enemy toggle).
static bool IsInterruptUnit(const AIHeroClient& unit) {
    if (!RMenu || !unit.IsValid()) {
        return false;
    }
    const std::string key = "rupt." + unit.CharacterName();
    const auto* item = RMenu->Get<MenuBool>(key.c_str());
    return item ? item->Value : false;
}

// Forward declarations — đúng thứ tự file C#.
static void AntiGapCloser(const GapCloserEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void RLogic();
static void Combo();
static void Harass();
static void OnDraw();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Blitzcrank", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuBool("CQDash", "Auto Use Q if Target In Dash"));
    ComboMenu->Add(new MenuBool("CW", "Use W"));
    ComboMenu->Add(new MenuBool("CE", "Use E"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("HMana", "Don't Harass if Mana <= X%", 0, 0, 100));

    RMenu = MenuRoot->AddSubMenu(new Menu("R Settings", "R set"));
    RMenu->Add(new MenuList("RMode", "Use R Mode",
        std::vector<std::string>{ "Only Combo", "Always", "Disable" }, 1));
    RMenu->Add(new MenuBool("UseRKill", "Use R Killable", false));
    RMenu->Add(new MenuSliderButton("UseRHealth", "Only When Target HP% <= X%", 20, 0, 100));
    RMenu->Add(new MenuBool("UseRSheild", "Use R Destroy Shield"));
    RMenu->Add(new MenuSlider("UseRSheildCount", "-> When Shield enemy Count >= X", 2, 1, 5));
    RMenu->Add(new MenuSlider("UseRSheildHeath", "Shield HP >= X", 100, 100, 400));
    RMenu->Add(new MenuBool("UseRInterrupt", "Use R Interrupt"));
    for (const auto& obj : GameObjects::EnemyHeroes()) {
        const std::string name = obj.CharacterName();
        if (name.empty()) {
            continue;
        }
        const std::string key = "rupt." + name;
        RMenu->Add(new MenuBool(key.c_str(), name.c_str()));
    }

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("DrawR", "Draw R"));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "AntiGap"));
    MiscMenu->Add(new MenuBool("AntiQGap", "Use Q AntiGapCloser"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1050.0f);
    Q.SetSkillshot(0.25f, 70.0f, 1800.0f, true, SpellType::Line);
    Q.DamageType = DamageType::Magical;

    W = Spell(SpellSlot::W, FLT_MAX);

    E = Spell(SpellSlot::E, FLT_MAX);
    E.DamageType = DamageType::Magical;

    R = Spell(SpellSlot::R, 600.0f);
    R.DamageType = DamageType::Magical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnGapCloser += &AntiGapCloser;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Blitzcrank loaded</font>");
}

static void AntiGapCloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "AntiQGap") || !Q.IsReady()) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid() || !sender.IsEnemy()) {
        return;
    }
    if (sender.IsDashing()) {
        const auto pred = Q.GetPrediction(sender);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::Dash)) {
            Q.Cast(pred.GetCastPosition());
        }
    }
}

// E Power Fist: cast trước đòn đánh vào hero (knockup on-hit).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }
    const auto heroClient = AIHeroClient(args.Target.Handle());
    if (ValidHeroTarget(heroClient)) {
        if (Bool(ComboMenu, "CE") && E.IsReady()) {
            E.Cast();
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

    // AutoCQDash: tự Q mọi địch đang dash.
    if (Bool(ComboMenu, "CQDash")) {
        if (Q.IsReady()) {
            for (const auto& obj : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(obj, Q.Range)) {
                    Q.CastIfHitchanceEquals(AIBaseClient(obj.Handle()), HitChance::Dash);
                }
            }
        }
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    default:
        break;
    }

    RLogic();
}

static void RLogic() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const int rMode = ListIndex(RMenu, "RMode", 1);
    if (!R.IsReady() || rMode == 2) {
        return;
    }
    if ((rMode == 0 && Orbwalker::ActiveMode() == OrbwalkingMode::Combo) || rMode == 1) {
        // Hoist snapshot EnemyHeroes 1 lần: 4 nhánh R bên dưới dùng chung trong
        // cùng frame (dữ liệu đã đóng băng nên = kết quả y hệt), tránh copy 4 lần.
        const auto rEnemies = GameObjects::EnemyHeroes();
        if (Bool(RMenu, "UseRKill", false)) {
            for (const auto& x : rEnemies) {
                if (ValidHeroTarget(x, R.Range) && x.Health() < SpellDamage(SpellSlot::R, AIBaseClient(x.Handle()))) {
                    R.Cast();
                    break;
                }
            }
        }
        if (SliderButtonEnabled(RMenu, "UseRHealth", false)) {
            const float hpPct = static_cast<float>(SliderButtonValue(RMenu, "UseRHealth", 20));
            for (const auto& x : rEnemies) {
                if (ValidHeroTarget(x, R.Range) && x.HealthPercent() <= hpPct) {
                    R.Cast();
                    break;
                }
            }
        }
        if (Bool(RMenu, "UseRSheild")) {
            const float shieldHp = static_cast<float>(Slider(RMenu, "UseRSheildHeath", 100));
            int count = 0;
            for (const auto& x : rEnemies) {
                if (ValidHeroTarget(x, R.Range) &&
                    (x.AllShield() + x.PhysicalShield() + x.MagicalShield()) >= shieldHp) {
                    ++count;
                }
            }
            if (count >= Slider(RMenu, "UseRSheildCount", 2)) {
                R.Cast();
            }
        }
        // Interrupt: địch đang cast important spell trong interrupt list → R.
        if (Bool(RMenu, "UseRInterrupt")) {
            for (const auto& x : rEnemies) {
                if (ValidHeroTarget(x, R.Range) && IsInterruptUnit(x) &&
                    Extensions::IsCastingInterruptableSpell(x, false)) {
                    R.Cast();
                    break;
                }
            }
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const auto target = GetTarget(Q.Range, DamageType::Magical);
    if (!ValidHeroTarget(target, Q.Range)) {
        return;
    }

    // W đuổi khi target ngoài tầm AA và con trỏ gần target hơn mình.
    if (Bool(ComboMenu, "CW") && W.IsReady() &&
        player.Mana() > W.Instance().ManaCost() + Q.Instance().ManaCost()) {
        if (target.DistanceToPlayer() >= AutoAttack::GetRealAutoAttackRange(target) + 100.0f) {
            if (Game::CursorPos().Distance(target.Position()) < target.DistanceToPlayer()) {
                W.Cast();
            }
        }
    }

    if (Bool(ComboMenu, "CQ") && Q.IsReady()) {
        // Không Q khi đang có PowerFist buff và còn địch trong tầm AA (giữ đòn E
        // knockup: C# check Orbwalker.GetTarget() != null).
        if (player.HasBuff("PowerFist") && ValidHeroTarget(target, AutoAttack::GetRealAutoAttackRange(target))) {
            return;
        }
        const auto pred = Q.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
            Q.Cast(pred.GetCastPosition());
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() <= static_cast<float>(Slider(HarassMenu, "HMana", 0))) {
        return;
    }
    if (Bool(HarassMenu, "HQ")) {
        const auto target = GetTarget(Q.Range, DamageType::Magical);
        if (ValidHeroTarget(target, Q.Range) && Q.IsReady()) {
            const auto pred = Q.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
                Q.Cast(pred.GetCastPosition());
            }
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DrawQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFF0000u);
    }
    if (Bool(DrawMenu, "DrawR", false) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFFA500u);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnGapCloser -= &AntiGapCloser;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Blitzcrank
