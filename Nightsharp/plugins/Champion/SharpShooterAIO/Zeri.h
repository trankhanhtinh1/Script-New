#pragma once

// ============================================================================
// SharpShooter AIO — Zeri
// Port sang NightSharp C++ theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Sivir.h.
// Spec: docs/superpowers/specs/2026-07-06-zeri-aio-design.md
//
// Kỹ năng:
//   Q Burst Fire     — skillshot line 825, r40, speed 2600, delay 0,
//                      collision {hero,minion}. NHƯNG hoạt động như đòn đánh
//                      thường (attack timer, A-reset, on-hit). Cast bằng
//                      prediction NHƯNG phải tôn trọng nhịp orbwalker.
//   W Ultrashock     — skillshot line 1200, r40, speed 2500, delay 250ms,
//                      collision {hero,minion}. Xuyên/nổ.
//   E Spark Surge    — dash 600, nạp lại Q. Không auto-spam.
//   R Lightning Crash— buff AoE quanh mình (KHÔNG phải skillshot).
//
// Q vs Orbwalker (mấu chốt):
//   Trong OnBeforeAttack, khi Q sẵn sàng + có mục tiêu hợp lệ → set
//   args.Process=false (huỷ AA mặc định), tự cast Q bằng prediction, rồi
//   Orbwalker::ResetAutoAttackTimer() để giữ nhịp attack-speed. Q không
//   ready/hết mana/không target → KHÔNG chặn → orbwalker đánh thường (passive).
//   => Mỗi lượt đánh chỉ xảy ra một trong hai: Q-predict HOẶC A thường.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Zeri {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* KillStealMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 825.0f };
inline Spell W{ SpellSlot::W, 1200.0f };
inline Spell E{ SpellSlot::E, 600.0f };
inline Spell R{ SpellSlot::R, 750.0f };

inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;
inline DWORD LastClearEvalTick = 0;

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

static bool Key(Menu* menu, const char* key, bool fallback = false) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item ? item->Active : fallback;
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

// Q tính là spell → check readiness bằng spellbook (CanUseSpell == Ready),
// không dùng nhịp attack-timer của orbwalker.
static bool QReady() {
    const auto player = Player();
    return player.IsValid() &&
           player.Spellbook().CanUseSpell(SpellSlot::Q) == CoreSpellBook::State_Ready;
}

// Các buff bất tử/miễn nhiễm phổ biến — không tính killsteal (port từ Ezreal).
static bool HasImmortalBuff(const AIHeroClient& hero) {
    return hero.HasBuff("JudicatorIntervention") ||
           hero.HasBuff("kindredrnodeathbuff") ||
           hero.HasBuff("Undying Rage") ||
           hero.HasBuff("FioraW") ||
           hero.HasBuff("ShroudofDarkness") ||
           hero.HasBuff("SivirShield") ||
           hero.HasBuff("BansheesVeil");
}

// Vị trí dash E hợp lệ (không đâm vào tường).
static bool IsGoodDashPosition(const Vector3& position) {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    if (SDK::NavMesh::IsWall(position)) {
        return false;
    }
    return true;
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void CastComboW();
static void AutoW();
static void HandleR();
static void SemiE();
static void Farm();
static void KillSteal();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Zeri", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuBool("useE", "Use E (gapclose)", false));
    ComboMenu->Add(new MenuBool("useR", "Use R"));
    ComboMenu->Add(new MenuBool("qHighOnly", "Q only on High hitchance", false));
    ComboMenu->Add(new MenuBool("qSkipCollision", "Skip Q collision (enemy)", false));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuBool("useW", "Use W", false));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 40, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("minMinions", "Q if hits >= x", 3, 1, 6));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 30, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("autoW", "Auto W on immobile"));
    MiscMenu->Add(new MenuKeyBind("semiE", "Semi E to cursor", SDK::Keys::A, KeyBindType::Press));
    MiscMenu->Add(new MenuBool("autoR", "Auto R", false));
    MiscMenu->Add(new MenuSlider("rMinEnemies", "R min enemies in range", 1, 1, 5));

    KillStealMenu = MenuRoot->AddSubMenu(new Menu("KillSteal Settings", "KillSteal"));
    KillStealMenu->Add(new MenuBool("ksMaster", "Enable KillSteal"));
    KillStealMenu->Add(new MenuBool("ksPassive", "Use Passive (AA)"));
    KillStealMenu->Add(new MenuBool("ksQ", "Use Q"));
    KillStealMenu->Add(new MenuBool("ksW", "Use W"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 825.0f);
    Q.SetSkillshot(0.0f, 40.0f, 2600.0f, true, SpellType::Line);
    Q.SetCollisionObjects(
        SDK::CollisionableObjects::Minions |
        SDK::CollisionableObjects::Heroes |
        SDK::CollisionableObjects::YasuoWall);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 1200.0f);
    W.SetSkillshot(0.25f, 40.0f, 2500.0f, true, SpellType::Line);
    W.SetCollisionObjects(
        SDK::CollisionableObjects::Minions |
        SDK::CollisionableObjects::Heroes |
        SDK::CollisionableObjects::YasuoWall);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 600.0f);

    R = Spell(SpellSlot::R, 750.0f);

    BuildMenu();

    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Events::hook.OnGameUpdate += &Game_OnUpdate;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Zeri loaded</font>");
}

// ── Q-as-attack: huỷ AA mặc định, cast Q bằng prediction, giữ nhịp orbwalker ──
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded || !QReady()) {
        return; // Q không sẵn → để orbwalker đánh thường (passive AA).
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(targetBase) || !targetBase.IsHero()) {
        return; // farm-minion do Farm() xử lý; ở đây chỉ Q lên hero.
    }

    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    const bool combo = mode == OrbwalkingMode::Combo && Bool(ComboMenu, "useQ");
    const bool harass = mode == OrbwalkingMode::Harass && Bool(HarassMenu, "useQ") &&
                        ManaOkay(Slider(HarassMenu, "Mana", 40));
    if (!combo && !harass) {
        return;
    }

    const auto target = AIHeroClient(targetBase.Handle());
    if (!ValidHeroTarget(target, Q.Range)) {
        return;
    }

    const bool skipCollision = combo && Bool(ComboMenu, "qSkipCollision", false);
    const bool highOnly = Bool(ComboMenu, "qHighOnly", false);

    const auto pred = Q.GetPrediction(target);

    // Combo skip-collision: bỏ qua vật cản, chỉ cần đủ hitchance.
    if (!skipCollision && !pred.CollisionObjects.empty()) {
        return; // bị chắn → để đánh thường.
    }

    const HitChance needed = highOnly ? HitChance::High : HitChance::Medium;
    if (!HitchanceAtLeast(pred.Hitchance, needed)) {
        return; // hitchance thấp → rơi về đánh thường.
    }

    args.Process = false;                 // huỷ AA mặc định
    Q.Cast(pred.GetCastPosition());       // Q thay cho đòn đánh
    Orbwalker::ResetAutoAttackTimer();    // giữ nhịp attack-speed
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
        return;
    }

    SemiE();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        CastComboW();
        HandleR();
        break;
    case OrbwalkingMode::Harass:
        if (Bool(HarassMenu, "useW", false) && ManaOkay(Slider(HarassMenu, "Mana", 40))) {
            CastComboW();
        }
        break;
    case OrbwalkingMode::LaneClear:
        Farm();
        break;
    default:
        break;
    }

    AutoW();
    KillSteal();
}

// W trong combo: ưu tiên bắn khi target ngoài tầm Q hoặc đứng yên.
static void CastComboW() {
    if (!Bool(ComboMenu, "useW") || !W.IsReady()) {
        return;
    }
    if (!ShouldRunNow(LastComboEvalTick, 100)) {
        return;
    }
    const auto target = GetTarget(W.Range, DamageType::Magical);
    if (!ValidHeroTarget(target, W.Range)) {
        return;
    }
    const auto pred = W.GetPrediction(target);
    if (!pred.CollisionObjects.empty()) {
        return;
    }
    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
        W.Cast(pred.GetCastPosition());
    }
}

// Auto W lên mục tiêu bất động (W tầm xa nhất — tốt để bắt CC).
static void AutoW() {
    if (!Bool(MiscMenu, "autoW") || !W.IsReady()) {
        return;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, W.Range)) {
            continue;
        }
        const auto pred = W.GetPrediction(enemy);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::Immobile)) {
            W.Cast(pred.GetCastPosition());
            break;
        }
    }
}

// R Lightning Crash: buff AoE quanh mình khi đủ số hero địch trong tầm.
static void HandleR() {
    const bool comboR = Bool(ComboMenu, "useR");
    const bool autoR = Bool(MiscMenu, "autoR", false);
    if ((!comboR && !autoR) || !R.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const int need = Slider(MiscMenu, "rMinEnemies", 1);
    if (player.CountEnemyHeroesInRange(R.Range) >= need) {
        R.Cast();
    }
}

// Semi-key E: dash thủ công tới con trỏ (an toàn nhất cho E).
static void SemiE() {
    if (!Key(MiscMenu, "semiE", false) || !E.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const Vector3 cursor = Game::CursorPos();
    const float dist = player.Position().Distance2D(cursor);
    const float clamp = std::min(dist, E.Range);
    Vector3 dashPos = player.Position().Extend(cursor, clamp);
    dashPos.y = SDK::NavMesh::GetHeightForPosition(dashPos);
    if (IsGoodDashPosition(dashPos)) {
        E.Cast(dashPos);
    }
}

// LaneClear + Jungle: Q có collision minion nên dùng line-farm location.
static void Farm() {
    const auto player = Player();
    if (!player.IsValid() || !QReady()) {
        return;
    }
    if (!ShouldRunNow(LastClearEvalTick, 120)) {
        return;
    }

    // Lane clear.
    if (Bool(LaneClearMenu, "useQ") && ManaOkay(Slider(LaneClearMenu, "Mana", 30))) {
        auto minions = GameObjects::EnemyLaneMinions();
        if (minions.empty()) {
            minions = GameObjects::EnemyMinions();
        }
        std::vector<AIBaseClient> targets;
        targets.reserve(minions.size());
        for (const auto& minion : minions) {
            if (ValidTarget(minion, Q.Range) && !minion.IsJungle()) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        if (!targets.empty()) {
            const auto farm = Q.GetLineFarmLocation(targets);
            if (farm.MinionsHit >= Slider(LaneClearMenu, "minMinions", 3)) {
                Q.Cast(Vector3::From2D(farm.Position));
                return;
            }
        }
    }

    // Jungle clear: mob máu cao nhất trong tầm.
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
        for (const auto& mob : mobs) {
            const auto pred = Q.GetPrediction(mob);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                Q.Cast(pred.GetCastPosition());
                return;
            }
        }
    }
}

// KillSteal: A → Q → W (rẻ trước; W để dành khi ngoài tầm Q).
static void KillSteal() {
    if (!Bool(KillStealMenu, "ksMaster")) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, W.Range) || HasImmortalBuff(enemy)) {
            continue;
        }
        const float effHealth = enemy.Health() + enemy.AllShield();

        // Passive (A): trong tầm đánh thường → ép orbwalk đánh.
        if (Bool(KillStealMenu, "ksPassive") && AutoAttack::InAutoAttackRange(enemy)) {
            const double aaDmg = Damage::GetAutoAttackDamage(player, enemy);
            if (aaDmg >= effHealth) {
                Orbwalker::ForceTarget(enemy);
                return;
            }
        }

        // Q: trong tầm 825, hitchance High.
        if (Bool(KillStealMenu, "ksQ") && QReady() && ValidHeroTarget(enemy, Q.Range)) {
            if (Q.GetDamage(enemy) >= effHealth) {
                const auto pred = Q.GetPrediction(enemy);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    Q.Cast(pred.GetCastPosition());
                    return;
                }
            }
        }

        // W: tầm 1200, hitchance High (chốt khi địch ngoài tầm Q).
        if (Bool(KillStealMenu, "ksW") && W.IsReady()) {
            if (W.GetDamage(enemy) >= effHealth) {
                const auto pred = W.GetPrediction(enemy);
                if (pred.CollisionObjects.empty() &&
                    HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    W.Cast(pred.GetCastPosition());
                    return;
                }
            }
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Events::hook.OnGameUpdate -= &Game_OnUpdate;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Zeri
