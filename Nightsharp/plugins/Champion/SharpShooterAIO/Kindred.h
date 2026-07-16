#pragma once

// ============================================================================
// SharpShooter AIO — Kindred
// Port từ CSharpFiles/Kindred/Kindred.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Corki.h.
//
// Kỹ năng:
//   Q Dance of Arrows — dash ngắn (skillshot 340, width 30, speed 1400) tới con trỏ.
//   W Wolf's Frenzy   — no-target 900, vùng sói cắn quanh.
//   E Mounting Dread  — targeted 575, đánh dấu; 3 stack → sói vồ.
//   R Lamb's Respite  — no-target 500, save-ult (bất tử vùng, hồi máu). Không damage.
//
// Ghi chú port (giữ 1-1 với C#):
//   * GetTarget: ưu tiên hero dính "KindredHitTracker"/"kindredecharge", else TS.
//   * Combo: E (bỏ qua black-list + comboAdvancedE), Q dash khi target trong AA,
//     W khi địch trong comboDistanceW.
//   * AttackE: force-attack mục tiêu 3-stack (kindredecharge) để dồn sói vồ; nếu có
//     hero khác chết được bằng 2 AA thì đánh nó trước.
//   * OnProcessSpellCast + autoR: địch AA vào mình/ally trong R range mà sát thương
//     >= máu*1.2 → R cứu.
//   * ClassicUltimate: ally < 10% HP bị vây, hoặc health-pred <= 100 → R.
//   * Dash wrapper C# (dash.CastDash) → Q.Cast(player.Extend(cursor, Q.Range)).
//   * MISSING API: InFountain() không có trong SDK → bỏ filter đó (an toàn).
//   * Kindred KHÔNG có killsteal spell-damage (combo dùng ước lượng AA) — khớp C#.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Kindred {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* EsetMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 340.0f };
inline Spell W{ SpellSlot::W, 900.0f };
inline Spell E{ SpellSlot::E, 575.0f };
inline Spell R{ SpellSlot::R, 500.0f };

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

static bool KeyActive(Menu* menu, const char* key) {
    if (!menu) {
        return false;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item && item->Active;
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

static AIHeroClient TSGetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

// C#: notcast.<name> trong E black-list.
static bool IsBlackListE(const AIHeroClient& unit) {
    if (!EsetMenu || !unit.IsValid()) {
        return false;
    }
    const std::string key = "notcast." + unit.CharacterName();
    const auto* item = EsetMenu->Get<MenuBool>(key.c_str());
    return item ? item->Value : false;
}

// Forward declarations — đúng thứ tự file C#.
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void ClassicUltimate();
static void Combo();
static void LaneClear();
static void OnDraw();
static void AntiGapCloser(const GapCloserEventArgs& args);
static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args);
static void AutoR();
static void FastBlackE();
static AIHeroClient GetTarget(float range);
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Kindred", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuBool("CW", "Use W"));
    ComboMenu->Add(new MenuBool("CE", "Use E"));
    EsetMenu = ComboMenu->AddSubMenu(new Menu("E BlackList", "E BlackList"));
    for (const auto& h : GameObjects::EnemyHeroes()) {
        const std::string name = h.CharacterName();
        if (name.empty()) {
            continue;
        }
        const std::string key = "notcast." + name;
        EsetMenu->Add(new MenuBool(key.c_str(), name.c_str(), false));
    }
    ComboMenu->Add(new MenuBool("comboAdvancedE", "Don't E If Target Health <= 3x Attack Damage"));
    ComboMenu->Add(new MenuSlider("comboDistanceW", "Use W Min Distance",
        static_cast<int>(W.Range) / 2, static_cast<int>(W.Range) / 5, static_cast<int>(W.Range)));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("LQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("LQC", "Use Q Min Minion Count", 3, 1, 3));
    LaneClearMenu->Add(new MenuBool("LW", "Use W", false));
    LaneClearMenu->Add(new MenuSlider("Lmana", "Don't LaneClear if Mana <= X%", 40, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("JungleClear Settings", "JungleClear"));
    JungleClearMenu->Add(new MenuBool("JQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("JW", "Use W"));
    JungleClearMenu->Add(new MenuBool("JE", "Use E"));
    JungleClearMenu->Add(new MenuSlider("Jmana", "Don't JungleClear if Mana <= X%", 40, 0, 100));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("DW", "Draw W"));
    DrawMenu->Add(new MenuBool("DE", "Draw E"));
    DrawMenu->Add(new MenuBool("DR", "Draw R"));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("autoR", "Auto R"));
    MiscMenu->Add(new MenuBool("AttackE", "Force Attack E Target"));
    MiscMenu->Add(new MenuBool("AntiGapQ", "AntiGap Q"));
    MiscMenu->Add(new MenuBool("AntiGapE", "AntiGap E"));
    MiscMenu->Add(new MenuKeyBind("FastE", "Fast E To E Black Target", 'E', KeyBindType::Press));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 340.0f);
    Q.SetSkillshot(0.25f, 30.0f, 1400.0f, false, SpellType::Line);

    W = Spell(SpellSlot::W, 900.0f);

    E = Spell(SpellSlot::E, 575.0f);
    E.SetTargetted(0.1f, FLT_MAX);

    R = Spell(SpellSlot::R, 500.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpellCast;
    Events::hook.OnGapCloser += &AntiGapCloser;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Kindred loaded</font>");
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    if (KeyActive(MiscMenu, "FastE")) {
        FastBlackE();
    }
    if (Bool(MiscMenu, "AttackE")) {
        // AA gom mục tiêu 3-stack (kindredecharge); nếu có hero khác chết được bằng
        // 2 AA mà mục tiêu-stack không chết bằng 3 AA thì đánh hero đó trước.
        const float aaRange = AutoAttack::GetRealAutoAttackRange(player, AttackableUnit());
        // Hoist snapshot EnemyHeroes 1 lần cho cả 2 vòng (frame đóng băng = y hệt).
        const auto attackEHeroes = GameObjects::EnemyHeroes();
        AIHeroClient attackE;
        for (const auto& x : attackEHeroes) {
            if (ValidHeroTarget(x, aaRange) && x.HasBuff("kindredecharge")) {
                attackE = x;
                break;
            }
        }
        if (attackE.IsValid()) {
            AIHeroClient other;
            for (const auto& y : attackEHeroes) {
                if (!ValidHeroTarget(y, aaRange) || y.NetworkId() == attackE.NetworkId()) {
                    continue;
                }
                const auto yBase = AIBaseClient(y.Handle());
                const auto eBase = AIBaseClient(attackE.Handle());
                if (y.Health() <= Damage::GetAutoAttackDamage(player, yBase) * 2.0f &&
                    attackE.Health() > Damage::GetAutoAttackDamage(player, eBase) * 3.0f) {
                    other = y;
                    break;
                }
            }
            Orbwalker::ForceTarget(other.IsValid() ? other : attackE);
        }
    }

    AutoR();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        break;
    default:
        break;
    }
}

// C#: R cứu ally < 10% HP bị vây, hoặc health-pred <= 100.
static void ClassicUltimate() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!ValidHeroTarget(ally, R.Range) || ally.IsRecalling() || ally.IsZombie() ||
            ally.DistanceToPlayer() >= R.Range) {
            continue;
        }
        if (ally.HealthPercent() < 10.0f &&
            player.CountEnemyHeroesInRange(R.Range + 400.0f) >= 1 &&
            ally.CountEnemyHeroesInRange(675.0f) >= 1) {
            R.Cast();
        }
        if (Prediction::Health::GetPrediction(AIBaseClient(ally.Handle()), 300) <= 100.0f) {
            R.Cast();
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const auto target = GetTarget(Q.Range + 500.0f);
    // Hoist snapshot EnemyHeroes 1 lần cho cả nhánh CE và CW (frame đóng băng = y hệt).
    const auto comboHeroes = GameObjects::EnemyHeroes();

    if (Bool(ComboMenu, "CE") && E.IsReady()) {
        for (const auto& enemy : comboHeroes) {
            if (!ValidHeroTarget(enemy, E.Range)) {
                continue;
            }
            if (!IsBlackListE(enemy)) {
                if (Bool(ComboMenu, "comboAdvancedE")) {
                    if (enemy.Health() <= Damage::GetAutoAttackDamage(player, AIBaseClient(enemy.Handle())) * 2.0f) {
                        return;
                    }
                }
                E.Cast(AIBaseClient(enemy.Handle()));
            }
        }
    }
    if (Bool(ComboMenu, "CQ") && Q.IsReady() && !player.Spellbook().IsWindingUp()) {
        if (target.IsValid() && AutoAttack::InAutoAttackRange(target)) {
            Q.Cast(player.Position().Extend(Game::CursorPos(), Q.Range));
        }
    }
    if (Bool(ComboMenu, "CW") && W.IsReady()) {
        for (const auto& enemy : comboHeroes) {
            if (ValidHeroTarget(enemy, W.Range) &&
                enemy.DistanceToPlayer() <= static_cast<float>(Slider(ComboMenu, "comboDistanceW", static_cast<int>(W.Range) / 2))) {
                W.Cast(enemy.PreviousPosition());
            }
        }
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (player.ManaPercent() >= static_cast<float>(Slider(LaneClearMenu, "Lmana", 40))) {
        const float aaRange = AutoAttack::GetRealAutoAttackRange(player, AttackableUnit());
        int minionCount = 0;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, aaRange)) {
                ++minionCount;
            }
        }
        if (minionCount >= Slider(LaneClearMenu, "LQC", 3)) {
            if (Bool(LaneClearMenu, "LQ") && Q.IsReady()) {
                Q.Cast(player.Position().Extend(Game::CursorPos(), Q.Range));
            }
            if (Bool(LaneClearMenu, "LW", false) && W.IsReady()) {
                W.Cast();
            }
        }
    }

    if (player.ManaPercent() >= static_cast<float>(Slider(JungleClearMenu, "Jmana", 40))) {
        const float aaRange = AutoAttack::GetRealAutoAttackRange(player, AttackableUnit());
        std::vector<AIMinionClient> mob;
        for (const auto& j : GameObjects::Jungle()) {
            if (ValidTarget(j, aaRange)) {
                mob.push_back(j);
            }
        }
        if (mob.empty()) {
            return;
        }
        bool hasBigMob = false;
        for (const auto& x : mob) {
            if (x.DistanceToPlayer() <= W.Range &&
                static_cast<int>(x.GetJungleType()) >= static_cast<int>(JungleType::Large)) {
                hasBigMob = true;
                break;
            }
        }
        if (Bool(JungleClearMenu, "JE") && E.IsReady()) {
            for (const auto& x : mob) {
                if (ValidTarget(x, E.Range) &&
                    static_cast<int>(x.GetJungleType()) >= static_cast<int>(JungleType::Large) &&
                    x.Health() > Damage::GetAutoAttackDamage(player, AIBaseClient(x.Handle())) * 2.0f) {
                    E.CastOnUnit(AIBaseClient(x.Handle()));
                    break;
                }
            }
        }
        if (Bool(JungleClearMenu, "JQ") && Q.IsReady()) {
            Q.Cast(player.Position().Extend(Game::CursorPos(), Q.Range));
        }
        if (Bool(JungleClearMenu, "JW") && W.IsReady() && (hasBigMob || static_cast<int>(mob.size()) >= 3)) {
            W.Cast();
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFFFFFFu);
    }
    if (Bool(DrawMenu, "DW", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(),
            static_cast<float>(Slider(ComboMenu, "comboDistanceW", static_cast<int>(W.Range) / 2)), 0xFFFFD700u);
    }
    if (Bool(DrawMenu, "DE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFF1E90FFu);
    }
    if (Bool(DrawMenu, "DR", false) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFADFF2Fu);
    }
}

static void AntiGapCloser(const GapCloserEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (Bool(MiscMenu, "AntiGapE") && E.IsReady() && ValidHeroTarget(sender, E.Range) &&
        ObjectManager::Player().Position().Distance(args.Start) > ObjectManager::Player().Position().Distance(args.End)) {
        E.CastOnUnit(AIBaseClient(sender.Handle()));
    }
    if (Bool(MiscMenu, "AntiGapQ") && Q.IsReady() && ValidHeroTarget(sender, 400.0f) &&
        ObjectManager::Player().Position().Distance(args.Start) > ObjectManager::Player().Position().Distance(args.End)) {
        Q.Cast(player.Position().Extend(Game::CursorPos(), Q.Range));
    }
}

static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!R.IsReady() || player.IsDead() || player.IsZombie()) {
        return;
    }
    if (Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender.Ptr);
    if (!sender.IsValid() || sender.IsAlly()) {
        return;
    }

    if (Bool(MiscMenu, "autoR")) {
        if (R.IsReady() && sender.IsEnemy()) {
            // Chỉ quan tâm đòn auto-attack.
            if (args.IsAutoAttack) {
                const auto attackTarget = AIHeroClient(args.Target.Ptr);
                if (ValidHeroTarget(attackTarget, R.Range) && attackTarget.IsAlly()) {
                    const auto senderBase = AIBaseClient(sender.Handle());
                    if (attackTarget.IsMe()) {
                        if (sender.GetAutoAttackDamage(AIBaseClient(player.Handle()), true) * 1.2f > player.Health()) {
                            R.Cast();
                        }
                    } else if (attackTarget.IsAlly() && attackTarget.DistanceToPlayer() <= R.Range) {
                        if (sender.GetAutoAttackDamage(AIBaseClient(attackTarget.Handle()), true) * 1.2f > attackTarget.Health()) {
                            R.Cast();
                        }
                    }
                }
            }
        }
    }
}

static void AutoR() {
    if (Bool(MiscMenu, "autoR")) {
        ClassicUltimate();
    }
}

// C#: E nhanh tới hero trong black-list (máu thấp nhất sau 3 AA).
static void FastBlackE() {
    if (!E.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    AIHeroClient best;
    float bestScore = FLT_MAX;
    for (const auto& x : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(x, E.Range) || !IsBlackListE(x)) {
            continue;
        }
        const float score = x.Health() - Damage::GetAutoAttackDamage(player, AIBaseClient(x.Handle())) * 3.0f;
        if (score < bestScore) {
            bestScore = score;
            best = x;
        }
    }
    if (best.IsValid()) {
        E.CastOnUnit(AIBaseClient(best.Handle()));
    }
}

// C#: ưu tiên hero dính KindredHitTracker/kindredecharge, else TargetSelector.
static AIHeroClient GetTarget(float range) {
    const auto player = Player();
    if (!player.IsValid()) {
        return AIHeroClient();
    }
    AIHeroClient preferred;
    for (const auto& o : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(o, range + player.BoundingRadius() + o.BoundingRadius())) {
            continue;
        }
        if (o.HasBuff("KindredHitTracker") || o.HasBuff("kindredecharge")) {
            preferred = o;
            break;
        }
    }
    if (preferred.IsValid()) {
        return preferred;
    }
    return TSGetTarget(range, DamageType::Physical);
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpellCast;
    Events::hook.OnGapCloser -= &AntiGapCloser;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Kindred
