#pragma once

// ============================================================================
// SharpShooter AIO — Kai'Sa
// Port từ CSharpFiles/Kaisa/Kaisa.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Corki.h.
//
// Kỹ năng:
//   Q Icathian Rain — no-target, bắn 6 (evolved 12) tên lửa 600 quanh Kai'Sa.
//   W Void Seeker    — skillshot line 3000, delay 0.4, width 100, speed 1750, collision.
//   E Supercharge    — no-target self buff (tàng hình + tăng tốc/tốc đánh).
//   R Killer Instinct— dash blink 1500 (+750/rank) tới gần địch dính plasma.
//
// Ghi chú port (giữ 1-1 với C#):
//   * Passive plasma: buff "kaisapassivemarker" (GetBuffCount) → điều kiện W.
//     Địch bị R-mark: buff "kaisapassivemarkerr".
//   * R combo: quanh mỗi target dính plasma sinh ring 7 điểm bán kính 470, chọn
//     điểm dash hợp lệ: (1) RW killsteal, (2) R-dash-kill, (3) force-R khi <10% HP.
//   * OnAfterAttack: W lên hero dính đủ plasma (ComboUseWCount).
//   * MISSING API: Player.Spellbook.EvolveSpell/EvolvePoints không có trong SDK
//     (LoL hiện đại auto-evolve) → khối Fast-evolve comment lại, xem missapi.md.
//   * Damage tính tay theo wiki (patch V26.13) — KHÔNG dùng Spell::GetDamage.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::SharpAIO::Kaisa {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 600.0f };
inline Spell W{ SpellSlot::W, 3000.0f };
inline Spell E{ SpellSlot::E, 0.0f };
inline Spell R{ SpellSlot::R, 1500.0f };

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

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

// Buff tiến hoá — Q/W/E evolved.
static bool IsUpdateQ() { const auto p = Player(); return p.IsValid() && p.HasBuff("KaisaQEvolved"); }
static bool IsUpdateW() { const auto p = Player(); return p.IsValid() && p.HasBuff("KaisaWEvolved"); }
static bool IsUpdateE() { const auto p = Player(); return p.IsValid() && p.HasBuff("KaisaEEvolved"); }

// C#: t.GetBuffCount("kaisapassivemarker") — số stack plasma trên mục tiêu.
static int GetPassiveBuffCount(const AIBaseClient& t) {
    return t.IsValid() ? t.GetBuffCount("kaisapassivemarker") : 0;
}

// ── Damage tính tay theo wiki (leagueoflegends.com/KaiSa, patch V26.13) ──
// KHÔNG dùng Spell::GetDamage. Chốt số ngày 2026-07-08. Giữ cấu trúc công thức
// C# (multiplier theo số tên lửa Q / trigger plasma W), ratio cập nhật theo wiki.
//
// Q Icathian Rain (PHYSICAL/missile): 40/55/70/85/100 + 55% bonus AD + 20% AP.
//   C# GetQDmg(t,c): base + bonusAD*ratio + AP*ratio, nếu c>1 thì *(1 + 0.25*c).
// W Void Seeker    (MAGIC): 30/55/80/105/130 + 130% total AD + 45% AP.
//   + plasma proc (>=3 stack): (maxHP - hp) * (0.15 + 0.06%/100 * AP) missing HP.
static float GetQDmg(const AIBaseClient& t, int c) {
    const auto player = Player();
    if (!player.IsValid() || !t.IsValid() || !Q.IsReady()) {
        return 0.0f;
    }
    const int level = Q.Instance().Level();
    if (level < 1) {
        return 0.0f;
    }
    const int baseDamage = 40 + (level - 1) * 15;
    const float damageAD = player.BonusAttackDamage() * 0.55f;
    const float damageAP = player.AP() * 0.20f;
    float enddmg = static_cast<float>(baseDamage) + damageAD + damageAP;
    if (c > 1) {
        enddmg = enddmg + enddmg * 0.25f * static_cast<float>(c);
    }
    return Damage::CalculateDamage(player, t, DamageType::Physical, enddmg);
}

static float GetWDmg(const AIBaseClient& t) {
    const auto player = Player();
    if (!player.IsValid() || !t.IsValid() || !W.IsReady()) {
        return 0.0f;
    }
    const int level = W.Instance().Level();
    if (level < 1) {
        return 0.0f;
    }
    const int baseDamage = 30 + (level - 1) * 25;
    const float damageAD = player.AD() * 1.30f;
    const float damageAP = player.AP() * 0.45f;
    float enddmg = static_cast<float>(baseDamage) + damageAD + damageAP;
    if (GetPassiveBuffCount(t) >= 3) {
        const float ratio = 0.15f + (0.06f / 100.0f * player.AP());
        const float missing = (t.MaxHealth() - t.Health()) * ratio;
        enddmg += missing;
    }
    return Damage::CalculateMixedDamage(player, t, 0.0f, enddmg);
}

// Ring N điểm bán kính r quanh center (thay Geometry.Circle.Points của C#).
static std::vector<Vector3> RingPoints(const Vector3& center, float radius, int count) {
    std::vector<Vector3> points;
    if (count <= 0) {
        return points;
    }
    points.reserve(count);
    const float step = 2.0f * 3.14159265f / static_cast<float>(count);
    for (int i = 0; i < count; ++i) {
        const float angle = step * static_cast<float>(i);
        Vector3 p(center.x + radius * std::cos(angle), center.y, center.z + radius * std::sin(angle));
        p.y = NavMesh::GetHeightForPosition(p);
        points.push_back(p);
    }
    return points;
}

// C#: Dash.IsGoodPosition — dash tới được, không dính tường.
static bool IsGoodDashPos(const Vector3& pos) {
    const auto player = Player();
    if (!player.IsValid() || (std::fabs(pos.x) < 0.01f && std::fabs(pos.z) < 0.01f)) {
        return false;
    }
    if (NavMesh::IsWall(pos) || NavMesh::IsWallBetween(player.PreviousPosition(), pos, R.Range / 5.0f)) {
        return false;
    }
    return true;
}

// C#: Dash.InMelleAttackRange(pos) — có địch melee mà pos nằm trong tầm AA của nó.
static bool InMeleeAttackRange(const Vector3& pos) {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) && enemy.IsMelee() &&
            enemy.Position().Distance(pos) <= AutoAttack::GetRealAutoAttackRange(enemy) + enemy.BoundingRadius()) {
            return true;
        }
    }
    return false;
}

// Đếm số lính trong tầm r quanh pos.
static int CountMinionsInRange(const Vector3& pos, float range) {
    int count = 0;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion) && minion.Position().Distance(pos) <= range) {
            ++count;
        }
    }
    return count;
}

// Đếm số hero địch trong tầm r quanh pos.
static int CountEnemyHeroesAround(const Vector3& pos, float range) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) && enemy.Position().Distance(pos) <= range) {
            ++count;
        }
    }
    return count;
}

// Đếm số hero đồng minh trong tầm r quanh pos.
static int CountAllyHeroesAround(const Vector3& pos, float range) {
    int count = 0;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ValidHeroTarget(ally, FLT_MAX) && !ally.IsMe() && ally.Position().Distance(pos) <= range) {
            ++count;
        }
    }
    return count;
}

// Số collision (hero/minion/wall) của W từ from tới to.
static int GetWCollision(const Vector3& from, const Vector3& to) {
    const auto hits = W.GetCollision(from.To2D(), std::vector<Vector2>{ to.To2D() });
    return static_cast<int>(hits.size());
}

// Forward declarations — đúng thứ tự file C#.
static void OnOrbwalkerAfter(OrbwalkingActionArgs& args);
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnDoCast(const Events::ProcessSpellEventArgs& args);
static void OnDraw();
static void AutoKill();
static void JungleClear();
static void LaneClear();
static void Harass();
static void Combo();
static void CastW(const AIHeroClient& unit);
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Kaisa", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuSlider("CQCount", "When QRange Minion <= X", 2, 1, 12));
    ComboMenu->Add(new MenuBool("CW", "Use W"));
    ComboMenu->Add(new MenuSlider("CWCount", "Only When Passive Count >= X", 3, 0, 5));
    ComboMenu->Add(new MenuBool("CE", "Use E"));
    ComboMenu->Add(new MenuBool("CR", "Use R"));
    ComboMenu->Add(new MenuBool("CRForce", "If Will die, force R"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("HQCount", "When QRange Minion <= X", 2, 1, 12));
    HarassMenu->Add(new MenuBool("HW", "Use W"));
    HarassMenu->Add(new MenuSlider("HWCount", "Only When Passive Count >= X", 3, 0, 5));
    HarassMenu->Add(new MenuSlider("HMana", "Don't Harass if Mana <= X%", 25, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("LQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("LQNum", "When QRange Minion >= X", 3, 1, 6));
    LaneClearMenu->Add(new MenuSlider("LMana", "Don't Laneclear/JungleClear if Mana <= X%", 30, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("JungleClear Settings", "JungleClear"));
    JungleClearMenu->Add(new MenuBool("JQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("JW", "Use W"));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuKeyBind("FastA", "Fast Update Spell", 0x5D, KeyBindType::Press));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 600.0f + player.BoundingRadius());
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 3000.0f);
    W.SetSkillshot(0.4f, 100.0f, 1750.0f, true, SpellType::Line);
    W.SetCollisionObjects(
        SDK::CollisionableObjects::Heroes |
        SDK::CollisionableObjects::Minions |
        SDK::CollisionableObjects::YasuoWall);
    W.DamageType = DamageType::Physical;

    E = Spell(SpellSlot::E, 0.0f);

    R = Spell(SpellSlot::R, 1500.0f);

    BuildMenu();

    Orbwalker::OnAfterAttack += &OnOrbwalkerAfter;
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnDoCast += &OnDoCast;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Kaisa loaded</font>");
}

static void OnOrbwalkerAfter(OrbwalkingActionArgs& args) {
    if (Bool(ComboMenu, "CW") && W.IsReady()) {
        const auto unit = AIHeroClient(args.Target.Handle());
        if (ValidHeroTarget(unit, W.Range) &&
            GetPassiveBuffCount(AIBaseClient(unit.Handle())) >= Slider(ComboMenu, "CWCount", 3) - 1) {
            CastW(unit);
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    if (R.Instance().Level() > 0) {
        R.Range = 1500.0f + static_cast<float>(R.Instance().Level() - 1) * 750.0f;
    }

    // C#: Orbwalker.AttackEnabled = !Player.HasBuff("KaisaE") (đang E thì khoá AA).
    Orbwalker::AttackEnabled(!player.HasBuff("KaisaE"));

    // MISSING API: EvolveSpell/EvolvePoints không có trong SDK — xem missapi.md.
    // Khối Fast-evolve gốc:
    //   if (Fast && Player.EvolvePoints != 0) { CastSpell(Recall); EvolveSpell(E/Q/W); }
    // LoL hiện đại tự động tiến hoá, nên bỏ qua an toàn.

    AutoKill();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        JungleClear();
        break;
    default:
        break;
    }

    // E khi vào tầm đánh của địch melee (evolved E = tàng hình reposition).
    if (IsUpdateE() && E.IsReady() && Bool(ComboMenu, "CE")) {
        if (InMeleeAttackRange(player.PreviousPosition())) {
            E.Cast();
        }
    }
}

static void OnDoCast(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (args.Slot == static_cast<int>(SpellSlot::W)) {
        Game::SendEmote(EmoteId::Joke);
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFF800080u);
    }
}

static void AutoKill() {
    if (!W.IsReady()) {
        return;
    }
    for (const auto& tt : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(tt, W.Range)) {
            continue;
        }
        if (tt.Health() > GetWDmg(AIBaseClient(tt.Handle()))) {
            continue;
        }
        const auto preds = W.GetPrediction(tt);
        if (HitchanceAtLeast(preds.Hitchance, HitChance::High) && preds.CollisionObjects.empty()) {
            W.Cast(preds.GetCastPosition());
        }
    }
}

static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() < static_cast<float>(Slider(LaneClearMenu, "LMana", 30))) {
        return;
    }

    // Hoist snapshot Jungle 1 lần cho cả nhánh JQ và JW (frame đóng băng = y hệt).
    const auto jungleMobs = GameObjects::Jungle();

    if (Bool(JungleClearMenu, "JQ") && Q.IsReady()) {
        int count = 0;
        for (const auto& mob : jungleMobs) {
            if (ValidTarget(mob, Q.Range)) {
                ++count;
            }
        }
        if (count > 0) {
            Q.Cast();
        }
    }
    if (Bool(JungleClearMenu, "JW") && W.IsReady()) {
        AIMinionClient best;
        int bestStacks = -1;
        for (const auto& mob : jungleMobs) {
            if (!ValidTarget(mob, W.Range)) {
                continue;
            }
            const int stacks = GetPassiveBuffCount(AIBaseClient(mob.Handle()));
            if (stacks > bestStacks) {
                bestStacks = stacks;
                best = mob;
            }
        }
        if (best.IsValid()) {
            const auto preds = W.GetPrediction(best);
            if (HitchanceAtLeast(preds.Hitchance, HitChance::High)) {
                W.Cast(preds.GetCastPosition());
            }
        }
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() < static_cast<float>(Slider(LaneClearMenu, "LMana", 30))) {
        return;
    }
    if (Bool(LaneClearMenu, "LQ") && Q.IsReady()) {
        const int minCount = Slider(LaneClearMenu, "LQNum", 3);
        const int miniNum = CountMinionsInRange(player.PreviousPosition(), Q.Range);
        if (miniNum >= minCount) {
            const int dmgCount = miniNum / (IsUpdateQ() ? 12 : 6);
            bool anyKill = false;
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (ValidTarget(minion, Q.Range) &&
                    minion.Health() <= GetQDmg(AIBaseClient(minion.Handle()), dmgCount)) {
                    anyKill = true;
                    break;
                }
            }
            if (anyKill) {
                Q.Cast();
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() <= static_cast<float>(Slider(HarassMenu, "HMana", 25))) {
        return;
    }

    if (Bool(HarassMenu, "HQ") && Q.IsReady()) {
        if (CountMinionsInRange(player.PreviousPosition(), Q.Range) < Slider(HarassMenu, "HQCount", 2) &&
            CountEnemyHeroesAround(player.PreviousPosition(), Q.Range) > 0) {
            Q.Cast();
        }
    }
    if (Bool(HarassMenu, "HW") && W.IsReady()) {
        for (const auto& wwtarget : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(wwtarget, W.Range) ||
                GetPassiveBuffCount(AIBaseClient(wwtarget.Handle())) < Slider(HarassMenu, "HWCount", 3)) {
                continue;
            }
            const auto pred = W.GetPrediction(wwtarget);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
                W.Cast(pred.GetCastPosition());
            }
            break;
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(ComboMenu, "CR") && R.IsReady()) {
        // Snapshot EnemyHeroes() một lần: mỗi lần gọi accessor này copy toàn bộ
        // vector dưới mutex; vòng ngoài + vòng RW-killsteal lồng nhau vốn copy
        // lại theo từng vòng (O(n^2) copies). List là snapshot cache đông cứng
        // nên tái dùng cho cả 2 vòng là hành vi y hệt.
        const auto enemyHeroes = GameObjects::EnemyHeroes();
        // Danh sách hero địch dính plasma R-mark.
        for (const auto& obj : enemyHeroes) {
            if (!ValidHeroTarget(obj, R.Range) || !obj.HasBuff("kaisapassivemarkerr")) {
                continue;
            }
            const Vector3 objPos = obj.PreviousPosition();

            // (1) RW killsteal: quanh objPos tìm điểm dash để W hạ gục 1 địch khác.
            if (W.IsReady() && Bool(ComboMenu, "CW")) {
                for (const auto& killwobj : enemyHeroes) {
                    if (!ValidHeroTarget(killwobj, FLT_MAX) ||
                        killwobj.Health() >= GetWDmg(AIBaseClient(killwobj.Handle()))) {
                        continue;
                    }
                    Vector3 bestPoint;
                    float bestCursorDist = FLT_MAX;
                    for (const auto& p : RingPoints(objPos, 470.0f, 7)) {
                        if (GetWCollision(p, killwobj.PreviousPosition()) == 0 &&
                            p.Distance(killwobj.PreviousPosition()) < W.Range - 50.0f &&
                            IsGoodDashPos(p)) {
                            const float cd = p.Distance(Game::CursorPos());
                            if (cd < bestCursorDist) {
                                bestCursorDist = cd;
                                bestPoint = p;
                            }
                        }
                    }
                    if (bestPoint.IsValid()) {
                        if (R.Cast(bestPoint)) {
                            CastW(killwobj);
                        }
                    }
                }
            }

            // (2) R-dash-kill: target dính plasma sắp chết, dash vào tầm.
            const AIBaseClient objBase = AIBaseClient(obj.Handle());
            if (obj.Health() <= GetQDmg(objBase, 4) + GetWDmg(objBase) +
                    Damage::GetAutoAttackDamage(player, objBase) * 2.0f &&
                !AutoAttack::InAutoAttackRange(obj)) {
                Vector3 bestPoint;
                float bestDist = FLT_MAX;
                for (const auto& p : RingPoints(objPos, 470.0f, 7)) {
                    if (!IsGoodDashPos(p)) {
                        continue;
                    }
                    if (!W.IsReady() || GetWCollision(p, objPos) == 0) {
                        const float d = p.Distance(objPos);
                        if (d < bestDist) {
                            bestDist = d;
                            bestPoint = p;
                        }
                    }
                }
                if (bestPoint.IsValid()) {
                    const bool isCast = R.Cast(bestPoint);
                    if (Bool(ComboMenu, "CE") && E.IsReady() && isCast) {
                        if (CountEnemyHeroesAround(bestPoint, 400.0f) >= 1 &&
                            CountAllyHeroesAround(bestPoint, 400.0f) <= 3) {
                            if (IsUpdateE()) {
                                E.Cast();
                            }
                        }
                    }
                }
            }

            // (3) Force-R escape khi máu <=10% và bị đe doạ.
            if (Bool(ComboMenu, "CRForce") && player.HealthPercent() <= 10.0f &&
                (player.CountEnemyHeroesInRange(450.0f) != 0 ||
                 InMeleeAttackRange(player.PreviousPosition()) ||
                 Prediction::Health::GetPrediction(player, 400) <= 0.0f)) {
                Vector3 bestPoint;
                float bestDist = -1.0f;
                for (const auto& p : RingPoints(objPos, 470.0f, 7)) {
                    if (IsGoodDashPos(p)) {
                        const float d = p.Distance(objPos);
                        if (d > bestDist) {
                            bestDist = d;
                            bestPoint = p;
                        }
                    }
                }
                if (bestPoint.IsValid()) {
                    const bool isCast = R.Cast(bestPoint);
                    if (Bool(ComboMenu, "CE") && E.IsReady() && isCast && IsUpdateE()) {
                        E.Cast();
                    }
                }
            }
        }
    }

    if (Bool(ComboMenu, "CQ") && Q.IsReady()) {
        const int qCount = Slider(ComboMenu, "CQCount", 2);
        // Cache count 1 lần: cùng args frame-invariant (player pos + Q.Range), tránh
        // copy lại EnemyHeroes ở lần gọi thứ 2 (frame đóng băng = giá trị y hệt).
        const int enemiesAroundQ = CountEnemyHeroesAround(player.PreviousPosition(), Q.Range);
        if (CountMinionsInRange(player.PreviousPosition(), Q.Range) <= qCount &&
            enemiesAroundQ > 0) {
            const bool isCast = Q.Cast();
            if (Bool(ComboMenu, "CE") && E.IsReady() && isCast) {
                if (enemiesAroundQ > 1) {
                    E.Cast();
                } else {
                    const auto best = GetTarget(Q.Range, DamageType::Physical);
                    if (ValidHeroTarget(best)) {
                        const AIBaseClient bestBase = AIBaseClient(best.Handle());
                        if (best.Health() > GetQDmg(bestBase, 6 - qCount) +
                                Damage::GetAutoAttackDamage(player, bestBase) * 3.0f) {
                            E.Cast();
                        }
                    }
                }
            }
        }
    }

    if (Bool(ComboMenu, "CW") && W.IsReady() && !player.Spellbook().IsWindingUp()) {
        for (const auto& wwtarget : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(wwtarget, W.Range)) {
                continue;
            }
            if (!(player.IsDashing() ||
                  GetPassiveBuffCount(AIBaseClient(wwtarget.Handle())) >= Slider(ComboMenu, "CWCount", 3))) {
                continue;
            }
            if (GetWCollision(player.PreviousPosition(), wwtarget.PreviousPosition()) != 0) {
                continue;
            }
            CastW(wwtarget);
            break;
        }
    }
}

static void CastW(const AIHeroClient& unit) {
    if (!unit.IsValid()) {
        return;
    }
    const auto predW = W.GetPrediction(unit);
    if (HitchanceAtLeast(predW.Hitchance, HitChance::High) && predW.CollisionObjects.empty()) {
        W.Cast(predW.GetCastPosition());
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Orbwalker::OnAfterAttack -= &OnOrbwalkerAfter;
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnDoCast -= &OnDoCast;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Kaisa
