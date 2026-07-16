#pragma once

// ============================================================================
// SharpShooter AIO — Ezreal
// Port từ CSharpFiles/Ezreal/Ezreal.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Corki.h.
//
// Kỹ năng:
//   Q Mystic Shot   — skillshot line 1200, delay 0.25, width 60, speed 2000, collision.
//   W Essence Flux  — skillshot line 1200, delay 0.25, width 80, speed 1700, no-collision.
//   E Arcane Shift  — dash blink 475, delay 0.65 (anti-gap + hook escape).
//   R Trueshot      — skillshot line (RRange slider), delay 1.0, width 160, speed 2000.
//
// Ghi chú port (giữ 1-1 với C#):
//   * ComboUseQ/W, SemiR, Killsteal Q/R, Harass Q/W, LaneClear Q + AutoQHarass,
//     Jungle Q/W, LastHit Q, W-Turret, Draw Q/E, AntiGap E, RRange. Đủ nhánh.
//   * OnNonKillableMinion (LaneClear Q last-hit) + OnBeforeAttack (W lên trụ).
//   * Damage tính tay theo wiki (patch V26.09) — KHÔNG dùng Spell::GetDamage.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::SharpAIO::Ezreal {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* KillStealMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* LastHitMenu = nullptr;
inline Menu* TurretMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1200.0f };
inline Spell W{ SpellSlot::W, 1200.0f };
inline Spell E{ SpellSlot::E, 475.0f };
inline Spell R{ SpellSlot::R, 20000.0f };

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

static bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) {
        return false;
    }
    lastTick = now;
    return true;
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
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

static float HealthPrediction(const AIBaseClient& unit, int ms) {
    return unit.IsValid() ? Prediction::Health::GetPrediction(unit, ms) : 0.0f;
}

static float RealAutoAttackRange(const AIBaseClient& unit) {
    return AutoAttack::GetRealAutoAttackRange(unit);
}

// C#: target.HasBuff("ezrealwattach") — target đang dính W chờ kích nổ.
static bool HaveWBuff(const AIBaseClient& target) {
    return target.IsValid() && target.HasBuff("ezrealwattach");
}

// ── Damage tính tay theo wiki (leagueoflegends.com/Ezreal, patch V26.09) ──
// KHÔNG dùng Spell::GetDamage. Chốt số ngày 2026-07-08.
//
// Q Mystic Shot   (PHYSICAL): 20/45/70/95/120 + 130% total AD + 40% AP
// W Essence Flux  (MAGIC)   : 80/135/190/245/300 + 100% bonus AD + 90% AP
//                             (chỉ gây damage khi được kích nổ; killsteal bỏ qua W)
// R Trueshot      (MAGIC)   : 350/550/750 + 100% bonus AD + 110% AP
//                             lính & quái thường: 150/225/300 (cùng ratio)
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float totalAd = player.AD();
    const float bonusAd = player.BonusAttackDamage();
    const float ap = player.AP();

    switch (slot) {
    case SpellSlot::Q: {
        const int rank = Q.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 20.0f, 45.0f, 70.0f, 95.0f, 120.0f };
        const float raw = base[rank - 1] + 1.30f * totalAd + 0.40f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    case SpellSlot::W: {
        const int rank = W.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 80.0f, 135.0f, 190.0f, 245.0f, 300.0f };
        const float raw = base[rank - 1] + 1.00f * bonusAd + 0.90f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    case SpellSlot::R: {
        const int rank = R.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        const int idx = (rank - 1 < 3) ? rank - 1 : 2;
        const bool minion = target.IsMinion() && !target.IsHero();
        static const float heroBase[3] = { 350.0f, 550.0f, 750.0f };
        static const float minionBase[3] = { 150.0f, 225.0f, 300.0f };
        const float base = minion ? minionBase[idx] : heroBase[idx];
        const float raw = base + 1.00f * bonusAd + 1.10f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    default:
        return 0.0f;
    }
}

// Killsteal: loại buff bất tử phổ biến rồi so máu + shield với damage tính được.
static bool IsKillable(const AIBaseClient& target, double damage, DamageType type) {
    if (!ValidUnit(target)) {
        return false;
    }
    if (target.HasBuff("kindredrnodeathbuff") || target.HasBuff("Undying Rage") ||
        target.HasBuff("JudicatorIntervention") || target.HasBuff("BansheesVeil") ||
        target.HasBuff("SivirShield") || target.HasBuff("ShroudofDarkness")) {
        return false;
    }
    const float shield = (type == DamageType::Physical)
        ? target.PhysicalShield()
        : target.MagicalShield();
    return target.Health() + shield < damage - 2.0;
}

// E blink: tìm vị trí dash hợp lệ (tránh tường), ưu tiên dash ra xa nguồn.
static Vector3 GetDashPosition(const AIBaseClient& awayFrom) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    if (awayFrom.IsValid()) {
        Vector3 away = player.Position().Extend(awayFrom.Position(), -E.Range);
        away.y = NavMesh::GetHeightForPosition(away);
        if (!NavMesh::IsWall(away)) {
            return away;
        }
    }

    Vector3 cursor = player.Position().Extend(Game::CursorPos(), E.Range);
    cursor.y = NavMesh::GetHeightForPosition(cursor);
    if (!NavMesh::IsWall(cursor)) {
        return cursor;
    }
    return {};
}

// Forward declarations — đúng thứ tự file C#.
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnAntiGapCloser(const GapCloserEventArgs& args);
static void OnBuffAdd(const Events::BuffEventArgs& args);
static void OnDraw();
static void OnNonKillableMinion(OrbwalkingActionArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void LastHit();
static void JungleClear();
static void LaneClear();
static void Harass();
static void SemiRCast();
static void CastW();
static void CastQ();
static void Killsteal();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Ezreal", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuBool("CW", "Use W"));
    ComboMenu->Add(new MenuKeyBind("SemiR", "Semi - R Cast", 'T', KeyBindType::Press));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HQ", "Use Q"));
    HarassMenu->Add(new MenuBool("HW", "Use W", false));
    HarassMenu->Add(new MenuSlider("HMana", "Don't Harass if Mana <= X%", 30, 0, 100));

    KillStealMenu = MenuRoot->AddSubMenu(new Menu("Killsteal Settings", "Killsteal"));
    KillStealMenu->Add(new MenuBool("KQ", "Use Q"));
    KillStealMenu->Add(new MenuBool("KR", "Use R"));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("LQ", "Use Q"));
    LaneClearMenu->Add(new MenuKeyBind("AQH", "LaneClear Auto Q Harass", 'G', KeyBindType::Toggle));
    LaneClearMenu->Add(new MenuSlider("LQMana", "Don't Laneclear/JungleClear if Mana <= X%", 30, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("JungleClear Settings", "JungleClear"));
    JungleClearMenu->Add(new MenuBool("JQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("JW", "Use W"));

    LastHitMenu = MenuRoot->AddSubMenu(new Menu("LastHit Settings", "LastHit"));
    LastHitMenu->Add(new MenuBool("LQ", "Use Q"));
    LastHitMenu->Add(new MenuSlider("LQMana", "Don't LastHit if Mana <= X%", 70, 0, 100));

    TurretMenu = MenuRoot->AddSubMenu(new Menu("Turret Settings", "W Turret"));
    TurretMenu->Add(new MenuBool("TW", "Use W"));
    TurretMenu->Add(new MenuSliderButton("safe", "^ Only if no enemies in range", 1400, 1200, 2000));
    TurretMenu->Add(new MenuSliderButton("allies", "^ Despite allies count nearby >? x", 1, 1, 4));
    TurretMenu->Add(new MenuSlider("TWMana", "Don't W Turret if Mana <= X%", 70, 0, 100));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("DE", "Draw E"));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("EAntiGap", "Use E |Anti Gapcloser"));
    MiscMenu->Add(new MenuBool("hookE", "Use E on Hook/Pull Buff"));
    MiscMenu->Add(new MenuSlider("RRange", "R Range", 1400, 0, 3000));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1200.0f);
    Q.SetSkillshot(0.25f, 60.0f, 2000.0f, true, SpellType::Line);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 1200.0f);
    W.SetSkillshot(0.25f, 80.0f, 1700.0f, false, SpellType::Line);
    W.DamageType = DamageType::Physical;

    E = Spell(SpellSlot::E, 475.0f);
    E.Delay = 0.65f;
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 20000.0f);
    R.SetSkillshot(1.0f, 160.0f, 2000.0f, false, SpellType::Line);
    R.DamageType = DamageType::Physical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnGapCloser += &OnAntiGapCloser;
    Events::hook.OnBuffAdd += &OnBuffAdd;
    Drawing::OnDraw += &OnDraw;
    Orbwalker::OnNonKillableMinion += &OnNonKillableMinion;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Ezreal loaded</font>");
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    Killsteal();

    // C#: Orbwalker.ForceTarget = enemy có W-buff. Ưu tiên đánh target dính W.
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) && HaveWBuff(enemy)) {
            Orbwalker::ForceTarget(enemy);
            break;
        }
    }

    if (R.IsReady() && KeyActive(ComboMenu, "SemiR")) {
        SemiRCast();
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        if (!player.Spellbook().IsWindingUp()) {
            CastW();
            CastQ();
        }
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        JungleClear();
        break;
    case OrbwalkingMode::LastHit:
        LastHit();
        break;
    default:
        break;
    }
}

static void OnAntiGapCloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "EAntiGap") || !E.IsReady()) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid() || !sender.IsEnemy()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (args.SkillType == SDK::GapcloserType::Targeted) {
        // e.Target.IsMe → dash escape.
        const Vector3 dashPos = GetDashPosition(sender);
        if (dashPos.x != 0.0f || dashPos.z != 0.0f) {
            E.Cast(dashPos);
        }
    } else if (args.SkillType == SDK::GapcloserType::Skillshot &&
               args.End.Distance(player.Position()) < args.Start.Distance(player.Position()) &&
               args.End.Distance(player.Position()) <= 500.0f) {
        const Vector3 dashPos = GetDashPosition(sender);
        if (dashPos.x != 0.0f || dashPos.z != 0.0f) {
            E.Cast(dashPos);
        }
    }
}

static void OnBuffAdd(const Events::BuffEventArgs& args) {
    if (!Loaded || !Bool(MiscMenu, "hookE") || !E.IsReady()) {
        return;
    }
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    // C#: ThreshQ / rocketgrab2 / PykeQ → dash escape.
    if (_stricmp(args.BuffName, "ThreshQ") == 0 ||
        _stricmp(args.BuffName, "rocketgrab2") == 0 ||
        _stricmp(args.BuffName, "PykeQ") == 0) {
        const Vector3 dashPos = GetDashPosition(AIBaseClient());
        if (dashPos.x != 0.0f || dashPos.z != 0.0f) {
            E.Cast(dashPos);
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFF0000u);
    }
    if (Bool(DrawMenu, "DE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFFFF0000u);
    }
}

// C#: Orbwalker.OnNonKillableMinion — LaneClear Q last-hit lính không thể AA.
static void OnNonKillableMinion(OrbwalkingActionArgs& args) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const auto minion = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(minion)) {
        return;
    }
    if (Orbwalker::ActiveMode() != OrbwalkingMode::LaneClear) {
        return;
    }
    if (player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "LQMana", 30)) ||
        !Bool(LaneClearMenu, "LQ")) {
        return;
    }
    if (!Q.IsReady()) {
        return;
    }

    const float hp = HealthPrediction(minion, 500);
    if (hp > 0.0f && hp < SpellDamage(SpellSlot::Q, minion)) {
        const auto pred = Q.GetPrediction(minion);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::Medium) && pred.CollisionObjects.empty()) {
            Q.Cast(pred.GetCastPosition());
        }
    }
}

// C#: Orbwalker.OnBeforeAttack — W lên trụ khi LaneClear (safe check).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (Orbwalker::ActiveMode() != OrbwalkingMode::LaneClear) {
        return;
    }
    const auto turret = AITurretClient(args.Target.Handle());
    if (!turret.IsValid() || !ValidTarget(AIBaseClient(turret.Handle()), W.Range)) {
        return;
    }
    if (!Bool(TurretMenu, "TW") || !W.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (static_cast<float>(Slider(TurretMenu, "TWMana", 70)) >= player.ManaPercent()) {
        return;
    }

    int alliesCount = 0;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ValidHeroTarget(ally, 900.0f) && !ally.IsMe()) {
            ++alliesCount;
        }
    }

    const bool safeEnabled = SliderButtonEnabled(TurretMenu, "safe", true);
    const int safeValue = SliderButtonValue(TurretMenu, "safe", 1400);
    const bool overrideEnabled = SliderButtonEnabled(TurretMenu, "allies", false);
    const int overrideValue = SliderButtonValue(TurretMenu, "allies", 1);

    if (safeEnabled && player.CountEnemyHeroesInRange(static_cast<float>(safeValue)) != 0 &&
        (!overrideEnabled || alliesCount < overrideValue)) {
        return;
    }
    if (safeEnabled && player.CountEnemyHeroesInRange(static_cast<float>(safeValue)) != 0 &&
        overrideEnabled && alliesCount < overrideValue) {
        return;
    }

    W.Cast(turret.Position());
}

static void LastHit() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.ManaPercent() < static_cast<float>(Slider(LastHitMenu, "LQMana", 70))) {
        return;
    }

    if (Bool(LastHitMenu, "LQ") && Q.IsReady()) {
        AIMinionClient best;
        float bestMaxHp = -1.0f;
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!ValidTarget(minion, Q.Range)) {
                continue;
            }
            const float hp = HealthPrediction(minion, 500);
            if (!(hp > 0.0f && hp <= SpellDamage(SpellSlot::Q, minion))) {
                continue;
            }
            const bool underAlly = minion.IsUnderAllyTurret();
            const bool underEnemy = minion.IsUnderEnemyTurret();
            const bool farFromAa = minion.DistanceToPlayer() > RealAutoAttackRange(minion) + 50.0f;
            const bool tanky = minion.Health() > Damage::GetAutoAttackDamage(player, minion);
            if (!(underAlly || (underEnemy && !player.IsUnderEnemyTurret()) || farFromAa || tanky)) {
                continue;
            }
            if (minion.MaxHealth() > bestMaxHp) {
                bestMaxHp = minion.MaxHealth();
                best = minion;
            }
        }
        if (best.IsValid()) {
            const auto pred = Q.GetPrediction(best);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::Medium) && pred.CollisionObjects.empty()) {
                Q.Cast(pred.GetCastPosition());
                return;
            }
        }
    }
}

static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.ManaPercent() < static_cast<float>(Slider(LaneClearMenu, "LQMana", 30))) {
        return;
    }

    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !ValidTarget(mob, Q.Range) || mob.IsPlant() || mob.IsPet();
            }),
        mobs.end());

    for (const auto& obj : mobs) {
        if (Bool(JungleClearMenu, "JW") && W.IsReady() && ValidTarget(obj, W.Range)) {
            if (obj.GetJungleType() >= JungleType::Large) {
                const auto pred = W.GetPrediction(obj);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    W.Cast(pred.GetCastPosition());
                }
            }
        }
        if (Bool(JungleClearMenu, "JQ") && Q.IsReady() && ValidTarget(obj, Q.Range)) {
            const auto pred = Q.GetPrediction(obj);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                Q.Cast(pred.GetCastPosition());
            }
        }
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.ManaPercent() < static_cast<float>(Slider(LaneClearMenu, "LQMana", 30))) {
        return;
    }

    if (Q.IsReady()) {
        if (KeyActive(LaneClearMenu, "AQH")) {
            Harass();
        }
        if (Bool(LaneClearMenu, "LQ")) {
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (!ValidTarget(minion, Q.Range)) {
                    continue;
                }
                const float hp = HealthPrediction(minion, 500);
                if (!(hp > 0.0f && hp <= SpellDamage(SpellSlot::Q, minion))) {
                    continue;
                }
                const bool underAlly = minion.IsUnderAllyTurret();
                const bool underEnemy = minion.IsUnderEnemyTurret();
                const bool farFromAa = minion.DistanceToPlayer() > RealAutoAttackRange(minion) + 50.0f;
                const bool tanky = minion.Health() > Damage::GetAutoAttackDamage(player, minion);
                if (!(underAlly || (underEnemy && !player.IsUnderEnemyTurret()) || farFromAa || tanky)) {
                    continue;
                }
                const auto pred = Q.GetPrediction(minion);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High) &&
                    pred.CollisionObjects.empty() &&
                    pred.GetCastPosition().Distance(player.Position()) <= Q.Range) {
                    Q.Cast(pred.GetCastPosition());
                    return;
                }
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.ManaPercent() < static_cast<float>(Slider(HarassMenu, "HMana", 30))) {
        return;
    }

    if (Bool(HarassMenu, "HW", false) && W.IsReady()) {
        if (!(!Q.IsReady() && player.CountEnemyHeroesInRange(RealAutoAttackRange(player)) == 0)) {
            const auto wtarget = GetTarget(W.Range, DamageType::Physical);
            if (!ValidHeroTarget(wtarget, W.Range)) {
                return;
            }
            const auto winput = W.GetPrediction(wtarget);
            if (HitchanceAtLeast(winput.Hitchance, HitChance::High)) {
                W.Cast(winput.GetCastPosition());
                return;
            }
        }
    }
    if (Bool(HarassMenu, "HQ") && Q.IsReady()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, Q.Range) && HaveWBuff(enemy)) {
                const auto qinput = Q.GetPrediction(enemy);
                if (HitchanceAtLeast(qinput.Hitchance, HitChance::High)) {
                    Q.Cast(qinput.GetCastPosition());
                    return;
                }
                break;
            }
        }
        const auto target = GetTarget(Q.Range, DamageType::Physical);
        if (!ValidHeroTarget(target, Q.Range)) {
            return;
        }
        const auto pred = Q.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
            Q.Cast(pred.GetCastPosition());
        }
    }
}

static void SemiRCast() {
    const auto target = GetTarget(R.Range, DamageType::Physical);
    if (ValidHeroTarget(target, R.Range)) {
        const auto pred = R.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High) &&
            pred.GetCastPosition().Distance(Player().Position()) <= R.Range) {
            R.Cast(pred.GetCastPosition());
        }
    }
}

static void CastW() {
    if (!Bool(ComboMenu, "CW") || !W.IsReady()) {
        return;
    }
    const auto target = GetTarget(W.Range, DamageType::Physical);
    if (!ValidHeroTarget(target, W.Range)) {
        return;
    }
    const auto pred = W.GetPrediction(target);
    if (HitchanceAtLeast(pred.Hitchance, HitChance::High) &&
        pred.GetCastPosition().Distance(Player().Position()) <= W.Range) {
        W.Cast(pred.GetCastPosition());
    }
}

static void CastQ() {
    if (!Bool(ComboMenu, "CQ") || !Q.IsReady()) {
        return;
    }
    const auto target = GetTarget(Q.Range, DamageType::Physical);
    if (!ValidHeroTarget(target, Q.Range)) {
        return;
    }

    // Ưu tiên target dính W-buff (kích nổ W).
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy, Q.Range) && HaveWBuff(enemy)) {
            const auto qinput = Q.GetPrediction(enemy);
            if (HitchanceAtLeast(qinput.Hitchance, HitChance::High) &&
                qinput.GetCastPosition().Distance(Player().Position()) <= Q.Range) {
                Q.Cast(qinput.GetCastPosition());
            }
            return;
        }
    }

    const auto pred = Q.GetPrediction(target);
    if (HitchanceAtLeast(pred.Hitchance, HitChance::High) &&
        pred.CollisionObjects.empty() &&
        pred.GetCastPosition().Distance(Player().Position()) <= Q.Range) {
        Q.Cast(pred.GetCastPosition());
    }
}

static void Killsteal() {
    if (!Bool(KillStealMenu, "KQ") || !Q.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Hoist snapshot EnemyHeroes 1 lần cho cả nhánh KQ và KR (frame đóng băng = y hệt).
    const auto ksHeroes = GameObjects::EnemyHeroes();

    for (const auto& target : ksHeroes) {
        if (!ValidHeroTarget(target, Q.Range)) {
            continue;
        }
        if (IsKillable(target, SpellDamage(SpellSlot::Q, target), DamageType::Physical)) {
            const auto qinput = Q.GetPrediction(target);
            if (HitchanceAtLeast(qinput.Hitchance, HitChance::High)) {
                Q.Cast(qinput.GetCastPosition());
            }
        }
    }

    if (!Bool(KillStealMenu, "KR", false) || !R.IsReady()) {
        return;
    }

    if (player.CountEnemyHeroesInRange(800.0f) > 0) {
        return;
    }

    const float rRange = static_cast<float>(Slider(MiscMenu, "RRange", 1400));
    for (const auto& target : ksHeroes) {
        if (!ValidHeroTarget(target, rRange)) {
            continue;
        }
        if (IsKillable(target, SpellDamage(SpellSlot::R, target), DamageType::Magical)) {
            const auto rinput = R.GetPrediction(target);
            if (HitchanceAtLeast(rinput.Hitchance, HitChance::High)) {
                R.Cast(rinput.GetCastPosition());
            }
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnGapCloser -= &OnAntiGapCloser;
    Events::hook.OnBuffAdd -= &OnBuffAdd;
    Drawing::OnDraw -= &OnDraw;
    Orbwalker::OnNonKillableMinion -= &OnNonKillableMinion;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Ezreal
