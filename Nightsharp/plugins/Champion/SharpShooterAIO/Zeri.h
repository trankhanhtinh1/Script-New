#pragma once

// ============================================================================
// SharpShooter AIO — Zeri
// Port 1-1 từ CSharpFiles/Zeri/Zeri.cs (EnsoulSharp) sang NightSharp C++.
// Chuẩn khung: 7UPAIO/Ezreal.h. Charged/skill đặc thù: XerathSemiPlugin.h.
//
// Ánh xạ Spell (C# → C++):
//   NormalQ  = Spell(Q, 825) skillshot(0, 40, 2600, collision, Line)   ← Q có va chạm
//   FastQ    = Spell(Q, 825) skillshot(0, 40, 3400, no-collision, Line) ← dùng khi có R
//   FastW    = Spell(W, 1200) skillshot(0.6, 40, 2200, collision, Line)
//   WallW    = Spell(W, 1500) skillshot(0.75, 100, MAX, no-collision, Line) ← nổ tường
//   E        = Spell(E, 300)  dash
//   R        = Spell(R, 825)  buff AoE quanh mình
//
// Mấu chốt Q vs orbwalker (giữ nguyên C#):
//   OnBeforeAttack quyết định cho/chặn đòn AA. OnAfterAttack cast Q sau AA khi
//   combo. Trong C# NormalQ speed 2600 collision, FastQ speed 3400 no-collision;
//   khi có buff R (ZeriR) dùng FastQ. Ta mô phỏng bằng CÙNG một spell Q, đổi
//   Speed + Collision động theo IsZeriRBuff() trong CastSmartQ (khớp SetSkillshot
//   của cả 2 bản C#).
//
// MISSING API (xem cuối file): Dash.CastDash / IsGoodPosition / InMelleAttackRange
//   (ImpulseAIO.Common) và GetFirstWallPoint không có trong SDK → port hành vi
//   tương đương bằng NavMesh + hình học (extend + IsWall), comment tại chỗ.
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreControl.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Zeri {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* BasicAttackMenu = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* JumpWallMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* KillStealMenu = nullptr;

// NormalQ + FastQ dùng chung 1 đối tượng Q (đổi Speed/Collision động). WallW là
// biến thể W tầm 1500 để nổ tường.
inline Spell Q{ SpellSlot::Q, 825.0f };
inline Spell W{ SpellSlot::W, 1200.0f };
inline Spell WallW{ SpellSlot::W, 1500.0f };
inline Spell E{ SpellSlot::E, 300.0f };
inline Spell R{ SpellSlot::R, 825.0f };

inline bool Loaded = false;

// ── JumpWall: 18 cặp điểm nhảy tường trên Summoner's Rift (copy y nguyên C#) ──
struct WallJump {
    Vector3 Start;
    Vector3 End;
};

inline const WallJump kWallJumps[] = {
    { Vector3(1164.776f, 455.3192f, 148.8625f),   Vector3(4344.15f, 537.4541f, 95.74805f) },
    { Vector3(420.0431f, 816.72f, 183.5748f),     Vector3(518.7524f, 4716.67f, 93.41431f) },
    { Vector3(630.0f, 4508.0f, 95.74805f),        Vector3(765.5391f, 10341.43f, 52.8374f) },
    { Vector3(4776.0f, 694.0f, 110.8725f),        Vector3(10689.7f, 767.7703f, 49.63037f) },
    { Vector3(14468.89f, 13948.27f, 166.4569f),   Vector3(14256.93f, 10190.89f, 93.31934f) },
    { Vector3(14014.0f, 14512.0f, 171.9777f),     Vector3(9547.33f, 14319.19f, 55.56006f) },
    { Vector3(10572.0f, 14356.0f, 91.42981f),     Vector3(6663.356f, 14085.54f, 52.83838f) },
    { Vector3(14166.0f, 10326.0f, 91.42981f),     Vector3(14138.41f, 5897.795f, 52.70801f) },
    { Vector3(3974.0f, 558.0f, 95.74805f),        Vector3(883.776f, 212.4987f, 174.2166f) },
    { Vector3(525.4004f, 3856.47f, 95.74802f),    Vector3(188.0797f, 416.1705f, 183.5747f) },
    { Vector3(10973.24f, 14356.0f, 91.42984f),    Vector3(13641.23f, 14704.25f, 165.5154f) },
    { Vector3(14271.69f, 11206.16f, 91.42981f),   Vector3(14661.37f, 14413.14f, 171.9775f) },
    { Vector3(7588.0f, 2988.0f, 52.55599f),       Vector3(10283.4f, 2843.151f, 49.19702f) },
    { Vector3(11959.84f, 7753.984f, 52.33273f),   Vector3(11596.24f, 9116.619f, 51.27246f) },
    { Vector3(11308.0f, 5328.0f, -57.65408f),     Vector3(12777.13f, 3343.183f, 51.36719f) },
    { Vector3(3504.0f, 9616.0f, -33.35656f),      Vector3(2314.208f, 11500.38f, 19.47461f) },
    { Vector3(7228.0f, 11924.0f, 56.4768f),       Vector3(4703.729f, 12038.75f, 56.43262f) },
    { Vector3(2930.0f, 7094.0f, 50.69962f),       Vector3(3298.77f, 5223.558f, 54.00513f) },
};

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

static int List(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuList>(key);
    return item ? item->Index : fallback;
}

static bool Key(Menu* menu, const char* key, bool fallback = false) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item ? item->Active : fallback;
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

static std::vector<AIHeroClient> GetTargets(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTargets(range, damageType) : std::vector<AIHeroClient>();
}

// ── Menu getters (map các property C#) ──
static int OrbAAMode()      { return List(BasicAttackMenu, "AAMode", 0); }      // 0=AQ, 1=Disable
static bool DisableAAHP()   { return Bool(BasicAttackMenu, "DisableAAHP", false); }
static bool DisableAACN()   { return Bool(BasicAttackMenu, "DisableAACN", true); }
static int ROrbAAMode()     { return List(BasicAttackMenu, "RAAMode", 0); }
static bool RDisableAAHP()  { return Bool(BasicAttackMenu, "RDisableAAHP", false); }
static bool RDisableAACN()  { return Bool(BasicAttackMenu, "RDisableAACN", true); }
static bool QAA()           { return Bool(BasicAttackMenu, "QAA", true); }
static bool SAFEQA()        { return Bool(BasicAttackMenu, "SAFEQA", true); }
static bool ComboUseW()     { return Bool(ComboMenu, "UseW", true); }
static int ComboUseE()      { return List(ComboMenu, "EMode", 1); }             // 0=Always,1=OnlySafe,2=Disable
static bool ComboUseWWall() { return Bool(ComboMenu, "UseWWall", true); }
static bool ComboUseR()     { return Bool(ComboMenu, "UseR", false); }
static int ComboUseRCount() { return Slider(ComboMenu, "UseRCount", 2); }
static bool HarassUseW()    { return Bool(ComboMenu, "UseW", true); }
static bool HarassUseWWall(){ return Bool(ComboMenu, "UseWWall", true); }
static bool JumpWall()      { return Key(JumpWallMenu, "Key", false); }
static bool DrawQ()         { return Bool(DrawMenu, "Q", false); }
static bool DrawW()         { return Bool(DrawMenu, "W", false); }
static bool DrawR()         { return Bool(DrawMenu, "R", false); }
static bool AntiGapE()      { return Bool(MiscMenu, "antiGapE", true); }

// C# IsJieZouBuff: có LethalTempo với đúng 6 stack (nhịp điệu chết chóc).
static bool IsJieZouBuff() {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    return player.GetBuffCount("LethalTempo") == 6 ||
           player.GetBuffCount("lethaltempo") == 6;
}

// ── Passive Ready: buff "zeriqpassiveready" (đòn A empowered đã sạc). ──
static bool PassiveReady() {
    const auto player = Player();
    return player.IsValid() && player.HasBuff("zeriqpassiveready");
}

// R (Lightning Crash) đang bật? Buff "ZeriR". Khi có R → dùng FastQ.
static bool IsZeriRBuff() {
    const auto player = Player();
    return player.IsValid() && player.HasBuff("ZeriR");
}

// E "Lightning Rounds" đang bật (buff "zeriespecialrounds") → Q AoE + xuyên,
// chỉ chặn tường (YasuoWall), không chặn lính.
static bool AoeMode() {
    const auto player = Player();
    return player.IsValid() && player.HasBuff("zeriespecialrounds");
}

// C# ResetQRange: tầm Q = 825 + (75 nếu có LethalTempo 6 stack).
static void ResetQRange() {
    Q.Range = 825.0f + (IsJieZouBuff() ? 75.0f : 0.0f);
}

// ── Damage wiki (leagueoflegends.com/en-us/Zeri) — verified 2026-07-08 ──
// Q Burst Fire (PHYSICAL, tổng 7 viên): 22/26/30/34/38 + [102/104/106/108/110]% TOTAL AD.
//   + E empower (AoeMode): mỗi loạt Q cộng bonus MAGIC 22/24/26/28/30 + 20% AP.
// W Ultrashock (PHYSICAL): 30/70/110/150/190 + 120% TOTAL AD + 50% AP.
//   Wall crit: 45/105/165/225/285 + 180% TOTAL AD + 75% AP (xem WWallDamage).
// R Lightning Crash (MAGIC nova): 150/250/350 + 60% BONUS AD + 110% AP.
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float totalAd = player.TotalAttackDamage();
    const float bonusAd = player.BonusAttackDamage();
    const float ap = player.AP();

    switch (slot) {
    case SpellSlot::Q: {
        const int rank = Q.Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5]  = { 22.0f, 26.0f, 30.0f, 34.0f, 38.0f };
        static const float adPct[5] = { 1.02f, 1.04f, 1.06f, 1.08f, 1.10f };
        const float physRaw = base[rank - 1] + adPct[rank - 1] * totalAd;
        float dmg = Damage::CalculateDamage(player, target, DamageType::Physical, physRaw);
        if (AoeMode()) {
            const int eRank = std::clamp(E.Level(), 1, 5);
            static const float eBonus[5] = { 22.0f, 24.0f, 26.0f, 28.0f, 30.0f };
            const float magicRaw = eBonus[eRank - 1] + 0.20f * ap;
            dmg += Damage::CalculateDamage(player, target, DamageType::Magical, magicRaw);
        }
        return dmg;
    }
    case SpellSlot::W: {
        const int rank = W.Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 30.0f, 70.0f, 110.0f, 150.0f, 190.0f };
        const float raw = base[rank - 1] + 1.20f * totalAd + 0.50f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    case SpellSlot::R: {
        const int rank = R.Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[3] = { 150.0f, 250.0f, 350.0f };
        const int idx = std::clamp(rank, 1, 3) - 1;
        const float raw = base[idx] + 0.60f * bonusAd + 1.10f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    default:
        return 0.0f;
    }
}

// W đập tường (crit) — PHYSICAL: 45/105/165/225/285 + 180% TOTAL AD + 75% AP.
static float WWallDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const int rank = W.Level();
    if (rank < 1) {
        return 0.0f;
    }
    static const float base[5] = { 45.0f, 105.0f, 165.0f, 225.0f, 285.0f };
    const float raw = base[rank - 1] + 1.80f * player.TotalAttackDamage() + 0.75f * player.AP();
    return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
}

// ── C# GetAADamage: sát thương đòn đánh tay (passive Living Battery), magic ──
// PassiveReady (charged): LevelDmg = min(90 + (110/17)*(lvl-1)*(0.7025+0.0175*(lvl-1)), 200)
//                         + 0.8*AP + %maxHP (min 15%).
// Chưa charged        : LevelDmg = min(10 + (15/17)*(lvl-1)*(0.7025+0.0175*(lvl-1)), 25)
//                         + 0.03*AP; nếu HP% < 35 thì *6 (execute passive).
// GIỮ NGUYÊN công thức C# gốc (số C# tự chọn, khác wiki một chút ở phần execute).
static float GetAADamage(const AIBaseClient& t) {
    const auto player = Player();
    if (!player.IsValid() || !t.IsValid()) {
        return 0.0f;
    }
    const double lvl = static_cast<double>(player.Level());
    const double ap = static_cast<double>(player.AP());

    if (PassiveReady()) {
        double levelDamage = 90.0 + (110.0 / 17.0) * (lvl - 1.0) * (0.7025 + 0.0175 * (lvl - 1.0));
        levelDamage = std::min(levelDamage, 200.0);
        const double extraDamage = 0.8 * ap;
        double healthDamage = (3.0 + (17.0 / 17.0) * (lvl - 1.0) * (0.0725 + 0.0002 * (lvl - 1.0))) / 100.0;
        healthDamage = std::min(healthDamage, 0.15);
        healthDamage = static_cast<double>(t.MaxHealth()) * healthDamage;
        return player.CalculateMagicDamage(t, static_cast<float>(levelDamage + extraDamage + healthDamage));
    }

    double levelDamage = 10.0 + (15.0 / 17.0) * (lvl - 1.0) * (0.7025 + 0.0175 * (lvl - 1.0));
    levelDamage = std::min(levelDamage, 25.0);
    const double extraDamage = 0.03 * ap;
    const double totalDamage = levelDamage + extraDamage;
    const double endDamage = t.HealthPercent() >= 35.0f ? totalDamage : totalDamage + (totalDamage * 5.0);
    return player.CalculateMagicDamage(t, static_cast<float>(endDamage));
}

// C# GetRawQDmg: raw Q (không trừ giáp) = (10 + (Qlvl-1)*5) + 1.1*TotalAD.
static float GetRawQDmg() {
    const auto player = Player();
    if (!player.IsValid()) {
        return 0.0f;
    }
    const float baseDamage = 10.0f + static_cast<float>(std::max(0, Q.Level() - 1)) * 5.0f;
    return baseDamage + player.TotalAttackDamage() * 1.1f;
}

// C# GetRealHeath(DamageType): HP thật + shield tương ứng loại damage. Zeri Q/W
// physical → cộng PhysicalShield; passive magic → cộng MagicalShield.
static float GetRealHealth(const AIBaseClient& unit, DamageType type) {
    if (!ValidUnit(unit)) {
        return 0.0f;
    }
    float shield = unit.AllShield();
    if (type == DamageType::Physical) {
        shield = unit.PhysicalShield();
    } else if (type == DamageType::Magical) {
        shield = unit.MagicalShield();
    }
    return unit.Health() + shield;
}

// Killsteal: loại buff bất tử/miễn nhiễm phổ biến.
static bool HasImmortalBuff(const AIHeroClient& hero) {
    return hero.HasBuff("JudicatorIntervention") ||
           hero.HasBuff("kindredrnodeathbuff") ||
           hero.HasBuff("Undying Rage") ||
           hero.HasBuff("FioraW") ||
           hero.HasBuff("ShroudofDarkness") ||
           hero.HasBuff("SivirShield") ||
           hero.HasBuff("BansheesVeil");
}

static bool IsKillable(const AIBaseClient& target, double damage) {
    if (!ValidUnit(target)) {
        return false;
    }
    if (target.HasBuff("kindredrnodeathbuff") || target.HasBuff("Undying Rage") ||
        target.HasBuff("JudicatorIntervention") || target.HasBuff("BansheesVeil") ||
        target.HasBuff("SivirShield") || target.HasBuff("ShroudofDarkness") ||
        target.HasBuff("FioraW")) {
        return false;
    }
    return target.Health() + target.AllShield() < damage - 2.0;
}

// MISSING API (Dash.CastDash/IsGoodPosition): port hành vi — điểm dash E hợp lệ
// = không đâm tường. C# dash E tầm 300 (Spark Surge in-combat dash).
static bool IsGoodDashPosition(const Vector3& position) {
    if (SDK::NavMesh::IsWall(position)) {
        return false;
    }
    return true;
}

// MISSING API: dash.CastDash(true) trả vị trí dash tối ưu. Port: dash tới con trỏ
// (clamp tầm E), set Y theo địa hình, kiểm tra hợp lệ.
static Vector3 GetDashPosition() {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }
    const Vector3 cursor = Game::CursorPos();
    const float dist = player.Position().Distance2D(cursor);
    const float clamp = std::min(dist, E.Range);
    Vector3 dashPos = player.Position().Extend(cursor, clamp);
    dashPos.y = SDK::NavMesh::GetHeightForPosition(dashPos);
    return dashPos;
}

// MISSING API (dash.InMelleAttackRange): có hero cận chiến địch trong tầm đánh?
static bool InMeleeAttackRange() {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy) || !enemy.IsMelee()) {
            continue;
        }
        const float reach = enemy.AttackRange() + enemy.BoundingRadius() + player.BoundingRadius();
        if (player.Distance(enemy) <= reach) {
            return true;
        }
    }
    return false;
}

// MISSING API (GetFirstWallPoint): C# tìm điểm tường đầu tiên trên đường W để xác
// nhận W đập tường. Port: quét dần phía sau target xem có tường không.
static bool WHitsWall(const Vector3& targetPos) {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    for (int d = 40; d <= 300; d += 40) {
        const Vector3 behind = targetPos.Extend(player.Position(), -static_cast<float>(d));
        if (SDK::NavMesh::IsWall(behind)) {
            return true;
        }
    }
    return false;
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnDraw();
static bool LastHitQ(const AIBaseClient& unit);
static bool CastSmartQ(const AIBaseClient& unit);
static void OnOrbwalkerAfter(OrbwalkingActionArgs& args);
static void OnOrbwalkerBefore(OrbwalkingActionArgs& args);
static void CastWToPos(const Vector3& pos);
static void Combo();
static void Harass();
static void OnNonKillableMinion(OrbwalkingActionArgs& args);
static void LogicE();
static void AutoKill();
static void JumpWallLogic();
static void LastHit();
static void KillSteal();
static void OnGapCloser(const GapCloserEventArgs& args);
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Zeri", true);

    BasicAttackMenu = MenuRoot->AddSubMenu(new Menu("BasicAttack", "Orbwalker"));
    BasicAttackMenu->Add(new MenuList("AAMode", "Attack Mode", { "AQ", "Disable" }, 0));
    BasicAttackMenu->Add(new MenuBool("DisableAAHP", "-> Disable: attack only if enemy HP <= 35%", false));
    BasicAttackMenu->Add(new MenuBool("DisableAACN", "-> Disable: attack only if fully charged", true));
    BasicAttackMenu->Add(new MenuList("RAAMode", "R Active Attack Mode", { "AQ", "Disable" }, 0));
    BasicAttackMenu->Add(new MenuBool("SAFEQA", "Disable QA if in melee hero range", true));
    BasicAttackMenu->Add(new MenuBool("RDisableAAHP", "-> R Disable: attack only if enemy HP <= 35%", false));
    BasicAttackMenu->Add(new MenuBool("RDisableAACN", "-> R Disable: attack only if fully charged", true));
    BasicAttackMenu->Add(new MenuBool("QAA", "Use Q instead of AA (harass/clear)", true));

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo"));
    ComboMenu->Add(new MenuBool("UseW", "Use W"));
    ComboMenu->Add(new MenuList("EMode", "Use E", { "Always", "Only Safe", "Disable" }, 1));
    ComboMenu->Add(new MenuBool("UseWWall", "-> Extra W Wall Mode"));
    ComboMenu->Add(new MenuBool("UseR", "Use R", false));
    ComboMenu->Add(new MenuSlider("UseRCount", "-> When Enemy >= X", 2, 1, 5));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass"));
    HarassMenu->Add(new MenuBool("UseW", "Use W", false));
    HarassMenu->Add(new MenuBool("UseWWall", "-> Extra W Wall Mode", false));

    JumpWallMenu = MenuRoot->AddSubMenu(new Menu("JumpWall", "JumpWall"));
    JumpWallMenu->Add(new MenuBool("UseJ", "Use JumpWall"));
    JumpWallMenu->Add(new MenuKeyBind("Key", "JumpWall Key", SDK::Keys::H, KeyBindType::Press));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw"));
    DrawMenu->Add(new MenuBool("Q", "Draw Q", false));
    DrawMenu->Add(new MenuBool("W", "Draw W", false));
    DrawMenu->Add(new MenuBool("R", "Draw R", false));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc", "Misc"));
    MiscMenu->Add(new MenuBool("antiGapE", "Anti-Gapcloser E", true));

    KillStealMenu = MenuRoot->AddSubMenu(new Menu("KillSteal", "KillSteal"));
    KillStealMenu->Add(new MenuBool("ksMaster", "Enable KillSteal"));
    KillStealMenu->Add(new MenuBool("ksQ", "Use Q"));
    KillStealMenu->Add(new MenuBool("ksW", "Use W"));
    KillStealMenu->Add(new MenuBool("ksR", "Use R (nova)", false));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    // NormalQ: speed 2600, collision. FastQ: speed 3400, no collision. Ta giữ 1
    // spell Q, chỉnh Speed/Collision động trong CastSmartQ theo IsZeriRBuff().
    Q = Spell(SpellSlot::Q, 825.0f);
    Q.SetSkillshot(0.0f, 40.0f, 2600.0f, true, SpellType::Line);
    Q.SetCollisionObjects(
        SDK::CollisionableObjects::Minions |
        SDK::CollisionableObjects::YasuoWall);
    Q.DamageType = DamageType::Physical;

    // FastW: delay 0.6 (cập nhật động theo attack speed như C#), width 40, speed 2200.
    W = Spell(SpellSlot::W, 1200.0f);
    W.SetSkillshot(0.6f, 40.0f, 2200.0f, true, SpellType::Line);
    W.SetCollisionObjects(
        SDK::CollisionableObjects::Heroes |
        SDK::CollisionableObjects::Minions |
        SDK::CollisionableObjects::Walls |
        SDK::CollisionableObjects::YasuoWall);
    W.DamageType = DamageType::Physical;

    // WallW: tầm 1500, delay 0.75+FastW.Delay, width 100, no-collision.
    WallW = Spell(SpellSlot::W, 1500.0f);
    WallW.SetSkillshot(0.75f, 100.0f, FLT_MAX, false, SpellType::Line);
    WallW.DamageType = DamageType::Physical;

    E = Spell(SpellSlot::E, 300.0f);
    R = Spell(SpellSlot::R, 825.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnBeforeAttack += &OnOrbwalkerBefore;
    Orbwalker::OnAfterAttack += &OnOrbwalkerAfter;
    Orbwalker::OnNonKillableMinion += &OnNonKillableMinion;
    Events::hook.OnGapCloser += &OnGapCloser;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Zeri loaded</font>");
}

// C# LastHitQ: Q lasthit lính. AoeMode → chỉ chặn YasuoWall; thường → chặn
// YasuoWall+Minions. Hitchance >= Medium thì bắn vào UnitPosition.
static bool LastHitQ(const AIBaseClient& unit) {
    if (!ValidTarget(unit, Q.Range)) {
        return false;
    }
    const bool aoe = AoeMode();
    SDK::CollisionObjectsBridge col = aoe
        ? SDK::CollisionObjectsBridge({ SDK::CollisionableObjects::YasuoWall })
        : SDK::CollisionObjectsBridge({ SDK::CollisionableObjects::YasuoWall,
                                        SDK::CollisionableObjects::Minions });
    Q.Collision = true;
    const auto pred = Q.GetPrediction(unit, aoe, -1.0f, col);
    if (HitchanceAtLeast(pred.Hitchance, HitChance::Medium)) {
        return Q.Cast(pred.GetUnitPosition());
    }
    return false;
}

// C# CastSmartQ: bắn Q thông minh có xử lý collision-fallback (nếu target chính
// bị chắn thì chọn target khác không chắn). IsZeriRBuff → dùng FastQ (speed 3400,
// no-collision-fallback). Với lính/quái bỏ collision hoàn toàn (Col = null).
static bool CastSmartQ(const AIBaseClient& unit) {
    if (!ValidTarget(unit, Q.Range)) {
        return false;
    }

    const bool aoe = AoeMode();
    // Target là lính/quái → bỏ collision (Col = null trong C#).
    const bool nullCol = unit.IsMinion();

    SDK::CollisionObjectsBridge eBuffColi({ SDK::CollisionableObjects::YasuoWall });
    SDK::CollisionObjectsBridge normalColi({ SDK::CollisionableObjects::YasuoWall,
                                             SDK::CollisionableObjects::Minions });
    SDK::CollisionObjectsBridge emptyCol({});

    SDK::CollisionObjectsBridge col = nullCol ? emptyCol : (aoe ? eBuffColi : normalColi);
    Q.Collision = !nullCol;

    if (IsZeriRBuff()) {
        // FastQ: speed 3400, không collision mặc định (collision-fallback thủ công).
        Q.Speed = 3400.0f;
        const auto preds = Q.GetPrediction(unit, aoe, -1.0f, eBuffColi);
        if (HitchanceAtLeast(preds.Hitchance, HitChance::Medium)) {
            return Q.Cast(preds.GetUnitPosition());
        }
        if (preds.Hitchance == HitChance::Collision) {
            const auto targets = GetTargets(Q.Range, DamageType::Physical);
            const AIHeroClient* best = nullptr;
            HitChance bestHit = HitChance::None;
            for (const auto& i : targets) {
                const auto p = Q.GetPrediction(i, aoe, -1.0f, eBuffColi);
                if (HitchanceAtLeast(p.Hitchance, HitChance::Medium) &&
                    p.GetCastPosition().Distance(Player().Position()) <= Q.Range &&
                    static_cast<int>(p.Hitchance) > static_cast<int>(bestHit)) {
                    best = &i;
                    bestHit = p.Hitchance;
                }
            }
            if (best) {
                const auto p = Q.GetPrediction(*best, aoe, -1.0f, eBuffColi);
                return Q.Cast(p.GetCastPosition());
            }
        }
        return false;
    }

    // NormalQ: speed 2600.
    Q.Speed = 2600.0f;
    const auto predNormal = Q.GetPrediction(unit, aoe, -1.0f, col);
    if (HitchanceAtLeast(predNormal.Hitchance, HitChance::Medium)) {
        return Q.Cast(predNormal.GetUnitPosition());
    }
    if (predNormal.Hitchance == HitChance::Collision) {
        const auto targets = GetTargets(Q.Range, DamageType::Physical);
        const AIHeroClient* best = nullptr;
        HitChance bestHit = HitChance::None;
        for (const auto& i : targets) {
            const auto p = Q.GetPrediction(i, aoe, -1.0f, col);
            if (HitchanceAtLeast(p.Hitchance, HitChance::Medium) &&
                p.GetCastPosition().Distance(Player().Position()) <= Q.Range &&
                static_cast<int>(p.Hitchance) > static_cast<int>(bestHit)) {
                best = &i;
                bestHit = p.Hitchance;
            }
        }
        if (best) {
            const auto p = Q.GetPrediction(*best, aoe, -1.0f, col);
            return Q.Cast(p.GetCastPosition());
        }
    }
    return false;
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    if (JumpWall()) {
        JumpWallLogic();
    }
    ResetQRange();
    // C#: FastW.Delay = 0.6 - clamp(0.02*((AttackSpeedMod-1)/0.25), 0, 0.2).
    const float asMod = player.AttackSpeedMod();
    W.Delay = 0.6f - std::max(0.0f, std::min(0.2f, 0.02f * ((asMod - 1.0f) / 0.25f)));

    AutoKill();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
    case OrbwalkingMode::LastHit:
        LastHit();
        break;
    default:
        break;
    }

    KillSteal();
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (DrawQ() && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFF0000u);
    }
    if (DrawW() && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFF0000FFu);
    }
    if (DrawR() && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFFA500u);
    }
}

// C# OnOrbwalkerAfter: sau đòn AA trong combo, cast Q vào target.
static void OnOrbwalkerAfter(OrbwalkingActionArgs&) {}

// C# OnOrbwalkerBefore: gate đòn AA. Ngoài tầm AA → chặn. Combo → theo AAMode
// (0=AQ luôn cho A; 1=Disable chỉ A khi thỏa điều kiện). Farm/Harass → QAA thay AA.
static void OnOrbwalkerBefore(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(targetBase)) {
        return;
    }

    float normalRange = player.AttackRange() + player.BoundingRadius() + targetBase.BoundingRadius();
    if (IsJieZouBuff()) {
        normalRange = normalRange - targetBase.BoundingRadius() - 20.0f;
    }
    if (targetBase.DistanceToPlayer() > normalRange) {
        args.Process = false;
        return;
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
        if (targetBase.IsValid()) {
            if (GetRealHealth(targetBase, DamageType::Magical) <= GetAADamage(targetBase)) {
                args.Process = true;
                return;
            }
            if (Q.IsReady() && (!IsZeriRBuff() || !SAFEQA() || !InMeleeAttackRange())) {
                args.Process = false;
                CastSmartQ(targetBase);
                return;
            }
            args.Process = false;
            return;
        }
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear ||
        Orbwalker::ActiveMode() == OrbwalkingMode::LastHit ||
        Orbwalker::ActiveMode() == OrbwalkingMode::Harass) {
        if (targetBase.IsValid() && !targetBase.IsHero()) {
            if (targetBase.Health() < GetAADamage(targetBase)) {
                args.Process = true;
                return;
            }
            if (QAA() && Q.IsReady()) {
                args.Process = false;
                CastSmartQ(targetBase);
                return;
            }
            if (!Q.IsReady() && QAA()) {
                args.Process = false;
                return;
            }
        }
    }
}

// C# CastWToPos: cast W tới vị trí (from == to).
static void CastWToPos(const Vector3& pos) {
    W.Cast(pos, pos);
}

static void Combo() {
    if (E.IsReady()) {
        LogicE();
    }
    if (ComboUseW() && W.IsReady()) {
        const auto target = GetTarget(W.Range, DamageType::Physical);
        if (target.IsValid()) {
            SDK::CollisionObjectsBridge wCol({ SDK::CollisionableObjects::Heroes,
                                               SDK::CollisionableObjects::Minions,
                                               SDK::CollisionableObjects::Walls,
                                               SDK::CollisionableObjects::YasuoWall });
            const auto pred = W.GetPrediction(target, false, -1.0f, wCol);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                CastWToPos(pred.GetCastPosition());
                return;
            }
            if (ComboUseWWall()) {
                if (!pred.CollisionObjects.empty()) {
                    WallW.Delay = 0.75f + W.Delay;
                    const auto preds = WallW.GetPrediction(target, true);
                    if (HitchanceAtLeast(preds.Hitchance, HitChance::High)) {
                        if (WHitsWall(preds.GetUnitPosition())) {
                            CastWToPos(preds.GetUnitPosition());
                        }
                    }
                }
            }
        }
    }
    if (ComboUseR() && R.IsReady()) {
        const auto player = Player();
        if (player.IsValid() && player.CountEnemyHeroesInRange(R.Range) >= ComboUseRCount()) {
            R.Cast();
        }
    }
    if (Q.IsReady()) {
        const auto orbTarget = Orbwalker::GetTarget();
        if (orbTarget.IsValid()) {
            const AIBaseClient newObj(orbTarget.Handle());
            if (!IsZeriRBuff() && newObj.IsValid() && OrbAAMode() == 1 &&
                ((DisableAAHP() && newObj.HealthPercent() <= 35.0f) ||
                 (DisableAACN() && PassiveReady()) ||
                 (GetRealHealth(newObj, DamageType::Physical) <
                  (GetAADamage(newObj) + SpellDamage(SpellSlot::Q, newObj))))) {
                return;
            }
            if (IsZeriRBuff() && newObj.IsValid() && ROrbAAMode() == 1 &&
                ((RDisableAAHP() && newObj.HealthPercent() <= 35.0f) ||
                 (RDisableAACN() && PassiveReady()) ||
                 (GetRealHealth(newObj, DamageType::Physical) <
                  (GetAADamage(newObj) + SpellDamage(SpellSlot::Q, newObj))))) {
                return;
            }
        }

        const bool orbNull = !orbTarget.IsValid();
        if (((!IsZeriRBuff() || (!SAFEQA() || !InMeleeAttackRange())) &&
             (IsZeriRBuff() && ROrbAAMode() != 0)) ||
            (!IsZeriRBuff() && OrbAAMode() != 0) ||
            orbNull) {
            const auto target = GetTarget(Q.Range, DamageType::Physical);
            if (target.IsValid()) {
                CastSmartQ(target);
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (Q.IsReady()) {
        if (player.IsValid() && !player.Spellbook().IsWindingUp()) {
            const auto target = GetTarget(Q.Range, DamageType::Physical);
            if (target.IsValid()) {
                CastSmartQ(target);
            }
        }
    }
    if (HarassUseW() && W.IsReady()) {
        const auto target = GetTarget(W.Range, DamageType::Physical);
        if (target.IsValid()) {
            SDK::CollisionObjectsBridge wCol({ SDK::CollisionableObjects::Heroes,
                                               SDK::CollisionableObjects::Minions,
                                               SDK::CollisionableObjects::Walls,
                                               SDK::CollisionableObjects::YasuoWall });
            const auto pred = W.GetPrediction(target, false, -1.0f, wCol);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                CastWToPos(pred.GetCastPosition());
                return;
            }
            if (HarassUseWWall()) {
                if (!pred.CollisionObjects.empty()) {
                    WallW.Delay = 0.75f + W.Delay;
                    const auto preds = WallW.GetPrediction(target, true);
                    if (HitchanceAtLeast(preds.Hitchance, HitChance::High)) {
                        if (WHitsWall(preds.GetUnitPosition())) {
                            CastWToPos(preds.GetUnitPosition());
                        }
                    }
                }
            }
        }
    }
}

// C# OnNonKillalbeMinion: khi orbwalker báo lính không thể lasthit bằng AA →
// dùng Q (farm/harass mode).
static void OnNonKillableMinion(OrbwalkingActionArgs& args) {
    if (!Q.IsReady()) {
        return;
    }
    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::LaneClear ||
        mode == OrbwalkingMode::LastHit ||
        mode == OrbwalkingMode::Harass) {
        const auto targetBase = AIBaseClient(args.Target.Handle());
        if (targetBase.IsValid() && targetBase.IsMinion()) {
            CastSmartQ(targetBase);
        }
    }
}

// C# LogicE: EMode 0=Always (dash khi không AoeMode), 1=OnlySafe (dash khi vị trí
// hiện tại không tốt), 2=Disable.
static void LogicE() {
    const int mode = ComboUseE();
    if (mode == 2) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const bool aoe = AoeMode();
    if (mode == 0 && !aoe) {
        const Vector3 dashPos = GetDashPosition();
        if (dashPos.IsValid() && IsGoodDashPosition(dashPos)) {
            E.Cast(player.Position(), dashPos);
        }
    }
    if (mode == 1) {
        // "Only Safe": chỉ dash khi vị trí hiện tại KHÔNG tốt (đang kẹt tường).
        if (!IsGoodDashPosition(player.Position())) {
            const Vector3 dashPos = GetDashPosition();
            if (dashPos.IsValid() && IsGoodDashPosition(dashPos)) {
                E.Cast(player.Position(), dashPos);
            }
        }
    }
}

// C# AutoKill: killsteal Q/W trên hero địch trong tầm nếu HP thật < damage.
static void AutoKill() {
    for (const auto& obj : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(obj)) {
            continue;
        }
        const float realHealth = GetRealHealth(obj, DamageType::Physical);
        if (Q.IsReady() && Q.IsInRange(obj)) {
            if (realHealth < SpellDamage(SpellSlot::Q, obj)) {
                CastSmartQ(obj);
            }
        }
        if (W.IsReady() && W.IsInRange(obj)) {
            if (realHealth < SpellDamage(SpellSlot::W, obj)) {
                const auto pred = W.GetPrediction(obj);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    CastWToPos(pred.GetCastPosition());
                }
            }
        }
    }
}

// C# JumpWallLogic: giữ phím → di chuyển tới điểm start gần con trỏ rồi E qua tường.
static void JumpWallLogic() {
    if (!E.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const Vector3 cursor = Game::CursorPos();
    for (const auto& jump : kWallJumps) {
        if (cursor.Distance(jump.Start) <= 50.0f) {
            CoreControl::IssueMove(jump.Start, true);
            if (player.Distance(jump.Start) <= 20.0f) {
                E.Cast(player.Position(), jump.End);
                break;
            }
        }
    }
}

// C# LastHit: Q lasthit lính ngoài tầm AA mà HP thật < Q damage.
static void LastHit() {
    const auto player = Player();
    if (!player.IsValid() || !Q.IsReady()) {
        return;
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidTarget(minion, Q.Range) || !minion.IsMinion()) {
            continue;
        }
        const AIBaseClient m(minion.Handle());
        if (!AutoAttack::InAutoAttackRange(m) &&
            GetRealHealth(m, DamageType::Physical) < SpellDamage(SpellSlot::Q, m)) {
            LastHitQ(m);
        }
    }
}

// KillSteal menu-driven (chạy mọi mode): Q → W → R.
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

        if (Bool(KillStealMenu, "ksQ") && Q.IsReady() && ValidHeroTarget(enemy, Q.Range)) {
            if (IsKillable(enemy, SpellDamage(SpellSlot::Q, enemy))) {
                if (CastSmartQ(enemy)) {
                    return;
                }
            }
        }

        if (Bool(KillStealMenu, "ksW") && W.IsReady()) {
            const auto pred = W.GetPrediction(enemy);
            const bool wall = WHitsWall(pred.GetUnitPosition());
            const float wDmg = wall ? WWallDamage(enemy) : SpellDamage(SpellSlot::W, enemy);
            if (IsKillable(enemy, wDmg) &&
                pred.CollisionObjects.empty() &&
                HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                CastWToPos(pred.GetCastPosition());
                return;
            }
        }

        if (Bool(KillStealMenu, "ksR", false) && R.IsReady() && ValidHeroTarget(enemy, R.Range)) {
            if (IsKillable(enemy, SpellDamage(SpellSlot::R, enemy))) {
                R.Cast();
                return;
            }
        }
    }
}

// C# AntiGapcloser.OnGapcloser: địch lao vào trong 450 → E né ra.
static void OnGapCloser(const GapCloserEventArgs& args) {
    if (!E.IsReady() || !AntiGapE()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid() || !sender.IsEnemy()) {
        return;
    }
    const Vector3 playerPos = player.Position();
    if (args.Start.Distance(playerPos) > args.End.Distance(playerPos) &&
        args.End.Distance(playerPos) <= 450.0f) {
        const Vector3 dashPos = GetDashPosition();
        if (dashPos.IsValid() && IsGoodDashPosition(dashPos)) {
            E.Cast(player.Position(), dashPos);
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnBeforeAttack -= &OnOrbwalkerBefore;
    Orbwalker::OnAfterAttack -= &OnOrbwalkerAfter;
    Orbwalker::OnNonKillableMinion -= &OnNonKillableMinion;
    Events::hook.OnGapCloser -= &OnGapCloser;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Zeri
