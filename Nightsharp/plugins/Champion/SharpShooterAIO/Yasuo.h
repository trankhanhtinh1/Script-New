#pragma once

// ============================================================================
// SharpShooter AIO — Yasuo
// Port từ CSharpFiles/Yasuo/Yasuo.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Viktor.h.
//
// Kỹ năng:
//   Q Steel Tempest — Q1/Q2 line 475 (skillshot), Q3 (2-stack) line 1070 AoE tornado.
//   W Wind Wall     — no-target 400 (block missile).
//   E Sweeping Blade — dash tới unit (targeted 475), stack Q khi dash.
//   R Last Breath   — targeted, nhảy tới địch đang bị hất tung (knockup/knockback).
//
// Ghi chú port (giữ 1-1 với C#):
//   * HaveQ3: Q spell name == "YasuoQ3Wrapper". HaveQ2: buff "YasuoQ1".
//   * QDelay/ESpeed tính động theo AttackSpeedMod/MoveSpeed mỗi frame.
//   * Combo: E-gapclose (mouse/target mode) → Q/Q3, EQ tornado bug, R落地双凤.
//   * Game_OnTick (R knockup logic) — MISSING API GameEvent.OnGameTick → gọi từ
//     OnGameUpdate đầu frame (giữ RDelay gating). Xem missapi.md.
//   * Game_OnDoCast E: disable orbwalker AA trong lúc E bay — MISSING DelayAction →
//     dùng tick re-enable (EAttackReEnableTick) check mỗi frame. Xem missapi.md.
//   * OnRenderMouseOvers Glow (highlight target) — MISSING API → bỏ. Xem missapi.md.
//   * EvadeTarget: missile-tracking nội bộ (block bằng W / né qua tường bằng E).
//     missile.Target check (đạn có nhắm mình?) — MISSING API → dựa DB-match + menu.
//   * Damage E cập nhật theo wiki (patch V26.x): base 70/85/100/115/130 = 55+15*lvl,
//     +60% AP +20% bonus AD, stack Ride the Wind +25%/stack. Q/R dùng SDK GetSpellDamage.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Yasuo {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* LastHitMenu = nullptr;
inline Menu* FleeMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* OtherMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 475.0f };
inline Spell Q3{ SpellSlot::Q, 1070.0f };
inline Spell W{ SpellSlot::W, 400.0f };
inline Spell E{ SpellSlot::E, 475.0f };
inline Spell R{ SpellSlot::R, 1200.0f };

inline bool Loaded = false;

inline int EDelay = 0;
inline int RDelay = 0;
inline int LaneClearDelay = 0;
inline int QQDelay = 0;
inline int EAttackReEnableTick = 0;   // MISSING DelayAction — re-enable AA sau khi E bay.

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

static AIHeroClient TSGetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static AIHeroClient TSGetTargetFrom(float range, const Vector3& from) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, DamageType::Physical, true, from) : AIHeroClient();
}

// ── Yasuo spell state ──
static std::string QSpellName() {
    const auto player = Player();
    return player.IsValid() ? player.Spellbook().GetSpell(SpellSlot::Q).Name() : std::string();
}

static bool HaveQ3() {
    return QSpellName() == "YasuoQ3Wrapper";
}

static bool HaveQ2() {
    const auto player = Player();
    return player.IsValid() && player.HasBuff("YasuoQ1");
}

static bool HaveQ1() {
    return !HaveQ2() && !HaveQ3();
}

static float ESpeedValue() {
    const auto player = Player();
    return 750.0f + player.MoveSpeed() * 0.6f;
}

static float QDelayValue() {
    const auto player = Player();
    return 0.4f * (1.0f - std::min((player.AttackSpeedMod() - 1.0f) * 100.0f / 1.67f * 0.01f, 0.67f));
}

// C#: PosAfterE — vị trí Yasuo sau khi E qua target.
static Vector3 PosAfterE(const AIBaseClient& target) {
    const auto player = Player();
    return player.Position().Extend(target.PreviousPosition(), E.Range);
}

// MISSING API: vị trí .CountEnemyHerosInRangeFix() / .IsUnderEnemyTurret() —
// helper cục bộ (Vec3 không có player-context). Xem missapi.md.
static int CountEnemyHeroesAround(const Vector3& pos, float range) {
    int count = 0;
    for (const auto& e : GameObjects::EnemyHeroes()) {
        if (e.IsValid() && !e.IsDead() && e.Position().Distance(pos) <= range) {
            ++count;
        }
    }
    return count;
}

static bool IsUnderEnemyTurretPos(const Vector3& pos) {
    for (const auto& t : GameObjects::EnemyTurrets()) {
        if (t.IsValid() && !t.IsDead() && t.Position().Distance(pos) <= 900.0f) {
            return true;
        }
    }
    return false;
}

// C#: CanCastE — target hợp lệ trong E range, chưa dính buff đã E.
static bool CanCastE(const AIBaseClient& target) {
    if (!target.IsValid() || target.IsDead()) {
        return false;
    }
    return Extensions::IsValidTarget(target, E.Range, true) &&
        !target.HasBuff("YasuoDashWrapper") && !target.HasBuff("YasuoE");
}

// C#: GetRTarget — địch trong R range đang bị hất tung.
static bool CanCastR(const AIHeroClient& target) {
    return SDK::HasBuffOfType(AIBaseClient(target.Handle()), SDK::BuffType::Knockback) ||
        SDK::HasBuffOfType(AIBaseClient(target.Handle()), SDK::BuffType::Knockup);
}

static std::vector<AIHeroClient> GetRTarget() {
    std::vector<AIHeroClient> out;
    for (const auto& i : GameObjects::EnemyHeroes()) {
        if (i.IsEnemy() && R.IsInRange(i) && CanCastR(i)) {
            out.push_back(i);
        }
    }
    return out;
}

// C#: GetDashObj — enemies + jungle + minions mà E tới được.
static std::vector<AIBaseClient> GetDashObj() {
    std::vector<AIBaseClient> out;
    for (const auto& x : GameObjects::EnemyHeroes()) {
        if (x.IsEnemy() && CanCastE(AIBaseClient(x.Handle()))) {
            out.push_back(AIBaseClient(x.Handle()));
        }
    }
    for (const auto& j : GameObjects::Jungle()) {
        if (CanCastE(AIBaseClient(j.Handle()))) {
            out.push_back(AIBaseClient(j.Handle()));
        }
    }
    for (const auto& m : GameObjects::EnemyMinions()) {
        if (CanCastE(AIBaseClient(m.Handle()))) {
            out.push_back(AIBaseClient(m.Handle()));
        }
    }
    return out;
}

// Forward declarations — đúng thứ tự C#.
static void Game_OnDoCast(const Events::ProcessSpellEventArgs& args);
static void Game_HideAnimation(const Events::ProcessSpellEventArgs& args);
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void Game_OnTick();
static void Combo();
static void Harass();
static void LaneClear();
static void LastHit();
static void Flee();
static AIBaseClient GetBestObjToMouse(bool underTower = true);
static float GetEDmg(const AIBaseClient& target);
static float GetQHpPred(const AIBaseClient& minion);
static float GetQDmg(const AIBaseClient& target);
static bool CastQ3();
static bool StackQ();
static bool CanCastDelayR(const AIHeroClient& target);
static AIBaseClient GetNearObj(const AIBaseClient& target, bool inQCir, bool underTower, bool checkFace);
static void OnDraw();
static void OnInterrupterSpell(const Events::InterruptableSpell::InterruptableTargetEventArgs& args);
static void OnGapcloser(const GapCloserEventArgs& args);
static void OnUnload();

// EvadeTarget subsystem (nested class C#).
namespace EvadeTarget {
    struct SpellDataEntry {
        std::string ChampionName;
        std::vector<std::string> SpellNames;
        SpellSlot Slot = SpellSlot::Unknown;
        std::string MissileName() const { return SpellNames.empty() ? std::string() : SpellNames.front(); }
    };
    struct TargetEntry {
        MissileClient Obj;
        Vector3 Start;
    };
    inline std::vector<TargetEntry> DetectedTargets;
    inline std::vector<SpellDataEntry> Spells;
    inline Vector2 WallCastedPos{};
    inline Menu* EvadeMenu = nullptr;
    inline Menu* EvadeTargetListMenu = nullptr;

    void Init();
    void OnUnload();
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Yasuo", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CE", "Use E"));
    ComboMenu->Add(new MenuKeyBind("CEMode", "Allow Mouse", 'A', KeyBindType::Toggle));
    ComboMenu->Add(new MenuSlider("CEDIS", "Use E When EnemyDisMe >= X", 300, 0, static_cast<int>(E.Range)));
    ComboMenu->Add(new MenuBool("CeAllc", "If E follows the mouse, closest to mouse becomes kill target"));
    ComboMenu->Add(new MenuBool("CEStackQ", "Stack Q", false));
    ComboMenu->Add(new MenuBool("CEOnlyQ23", "Use E Only Have Q1 or Q2", false));
    ComboMenu->Add(new MenuBool("CEBUG", "Bug eQ", false));
    ComboMenu->Add(new MenuBool("CR", "Use R"));
    ComboMenu->Add(new MenuBool("CRDelay", "Delay Cast"));
    ComboMenu->Add(new MenuSlider("RHpU", "Only Enemy Health <= X%", 30, 0, 100));
    ComboMenu->Add(new MenuSlider("RCountA", "Or KnockUp Count >= X", 2, 1, 5));
    ComboMenu->Add(new MenuBool("RSF", "Try EQ -> R"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HQ3", "Use Q3"));
    HarassMenu->Add(new MenuBool("HQLastHit", "UseQ1 / Q2 LastHit if Target Don't in Q Range"));

    ClearMenu = MenuRoot->AddSubMenu(new Menu("Clear Settings", "Lane / JungleClear"));
    LaneClearMenu = ClearMenu->AddSubMenu(new Menu("LaneClear", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("LQ3", "Use Q3"));
    LaneClearMenu->Add(new MenuBool("LE", "Use E"));
    LaneClearMenu->Add(new MenuBool("LELastHit", "Only Lasthit E"));
    JungleClearMenu = ClearMenu->AddSubMenu(new Menu("JungleClear", "JungleClear"));
    JungleClearMenu->Add(new MenuBool("JQ3", "Use Q3"));
    JungleClearMenu->Add(new MenuBool("JE", "Use E"));

    LastHitMenu = MenuRoot->AddSubMenu(new Menu("LastHit Settings", "LastHit"));
    LastHitMenu->Add(new MenuBool("KQ12", "Use Q"));
    LastHitMenu->Add(new MenuBool("KQ3", "Use Q3"));
    LastHitMenu->Add(new MenuBool("KE", "Use E"));

    FleeMenu = MenuRoot->AddSubMenu(new Menu("Flee Settings", "Flee"));
    FleeMenu->Add(new MenuBool("FQ", "Use Q3"));
    FleeMenu->Add(new MenuBool("FE", "Use E"));
    FleeMenu->Add(new MenuBool("FEStackQ", "Stack Q3"));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DQ", "Draw Q Range"));
    DrawMenu->Add(new MenuBool("DE", "Draw E Range"));
    DrawMenu->Add(new MenuBool("DR", "Draw R Range"));
    DrawMenu->Add(new MenuBool("DrawTarget", "Highlight the target to kill"));

    OtherMenu = MenuRoot->AddSubMenu(new Menu("Other Settings", "Misc"));
    OtherMenu->Add(new MenuKeyBind("OQUnderTower", "UnderTower", 'A', KeyBindType::Toggle));
    OtherMenu->Add(new MenuBool("OEInttput", "Use Q3 Interrupt"));
    OtherMenu->Add(new MenuBool("OEATG", "Use Q3 Anti GapCloser"));
    OtherMenu->Add(new MenuBool("HideQ", "Hide Q CastAnimation"));
    OtherMenu->Add(new MenuBool("HideR", "Hide R CastAnimation"));
    OtherMenu->Add(new MenuBool("AutoRC", "Play Ctrl+6 When R Cast"));

    EvadeTarget::Init();

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 475.0f);
    Q.SetSkillshot(0.25f, 55.0f, 5000.0f, false, SpellType::Line);
    Q3 = Spell(SpellSlot::Q, 1070.0f);
    Q3.SetSkillshot(0.25f, 90.0f, 1200.0f, false, SpellType::Line);
    W = Spell(SpellSlot::W, 400.0f);
    E = Spell(SpellSlot::E, 475.0f);
    E.SetTargetted(0.0f, 1400.0f);
    R = Spell(SpellSlot::R, 1200.0f);
    R.SetTargetted(0.0f, FLT_MAX);
    Q.DamageType = Q3.DamageType = R.DamageType = DamageType::Physical;

    BuildMenu();

    Events::hook.OnDoCast += &Game_OnDoCast;
    Events::hook.OnDoCast += &Game_HideAnimation;
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Drawing::OnDraw += &OnDraw;
    Events::hook.OnInterruptableSpell += &OnInterrupterSpell;
    Events::hook.OnGapCloser += &OnGapcloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Yasuo loaded</font>");
}

// C# constructor lambda: Interrupter.OnInterrupterSpell → Q3 chặn.
static void OnInterrupterSpell(const Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    if (!Bool(OtherMenu, "OEInttput") || !Q.IsReady() || !HaveQ3() || Player().IsDashing()) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid()) {
        return;
    }
    const auto pred = Q3.GetPrediction(sender);
    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
        Q3.Cast(pred.GetCastPosition());
    }
}

// C# constructor lambda: AntiGapcloser.OnGapcloser → Q3 chặn.
static void OnGapcloser(const GapCloserEventArgs& args) {
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid() || !sender.IsDashing() || !Bool(OtherMenu, "OEATG") ||
        !Q.IsReady() || !HaveQ3() || Player().IsDashing()) {
        return;
    }
    const auto pred = Q3.GetPrediction(sender);
    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
        Q3.Cast(pred.GetCastPosition());
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), HaveQ3() ? Q3.Range : Q.Range, 0xFF00FF00u);
    }
    if (Bool(DrawMenu, "DE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFFFFA500u);
    }
    if (Bool(DrawMenu, "DR", false) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFF0000u);
    }
    // MISSING API: NearMouseTarget.Glow (highlight) — xem missapi.md.
}

// C# Game_OnDoCast: E → disable orbwalker AA trong lúc E bay, re-enable sau FlyTime+300.
static void Game_OnDoCast(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (args.Slot == static_cast<int>(SpellSlot::E)) {
        const auto to = AIBaseClient(args.Target.Ptr);
        if (to.IsValid()) {
            Orbwalker::AttackEnabled(false);
            const int flyTime = static_cast<int>((Player().PreviousPosition().Distance(PosAfterE(to)) / E.Speed) * 1000.0f);
            // MISSING DelayAction — re-enable qua tick check trong Game_OnUpdate.
            EAttackReEnableTick = SDK::Variables::TickCount() + flyTime + 300;
        }
    }
}

// C# Game_HideAnimation: HideQ/HideR emote dance + AutoRC mastery badge.
static void Game_HideAnimation(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (Bool(OtherMenu, "HideQ") && args.Slot == static_cast<int>(SpellSlot::Q)) {
        Game::SendEmote(EmoteId::Dance);
    }
    if (args.Slot == static_cast<int>(SpellSlot::R)) {
        if (Bool(OtherMenu, "AutoRC")) {
            Game::SendMasteryBadge();
        }
        if (Bool(OtherMenu, "HideR")) {
            Game::SendEmote(EmoteId::Dance);
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

    // MISSING DelayAction — re-enable AA khi E bay xong.
    if (EAttackReEnableTick != 0 && SDK::Variables::TickCount() >= EAttackReEnableTick) {
        Orbwalker::AttackEnabled(true);
        EAttackReEnableTick = 0;
    }

    Q.Delay = Q3.Delay = QDelayValue();
    E.Speed = ESpeedValue();

    // MISSING API GameEvent.OnGameTick — gọi từ đây (RDelay tự gating).
    Game_OnTick();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        break;
    case OrbwalkingMode::LastHit:
        LastHit();
        break;
    case OrbwalkingMode::Flee:
        Flee();
        break;
    default:
        break;
    }
}

// C# Game_OnTick: combo R khi đủ điều kiện knockup/hp/count + EQ落地双凤.
static void Game_OnTick() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    auto rTargets = GetRTarget();
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo && R.IsReady() &&
        Bool(ComboMenu, "CR") && !rTargets.empty()) {
        // hero = enemies thỏa (nearEnemy > 1 & killable R) hoặc (avg HP% <= RHpU) hoặc (count >= RCountA),
        // sắp theo nearEnemy.Count giảm dần.
        struct Scored { AIHeroClient hero; int nearCount; };
        std::vector<Scored> hero;
        // Hoist snapshot EnemyHeroes 1 lần ngoài loop rTargets: frame đã đóng băng
        // nên tái dùng cho vòng nearEnemy = kết quả y hệt, tránh copy O(rTargets×n).
        const auto tickEnemies = GameObjects::EnemyHeroes();
        for (const auto& enemy : rTargets) {
            std::vector<AIHeroClient> nearEnemy;
            for (const auto& i : tickEnemies) {
                if (i.IsEnemy() && i.Distance(enemy) < 400.0f && CanCastR(i)) {
                    nearEnemy.push_back(i);
                }
            }
            float sumHpPct = 0.0f;
            for (const auto& i : nearEnemy) {
                sumHpPct += i.HealthPercent();
            }
            const int nc = static_cast<int>(nearEnemy.size());
            const bool cond =
                (nc > 1 && enemy.Health() <= player.GetSpellDamage(AIBaseClient(enemy.Handle()), SpellSlot::R)) ||
                (nc > 0 && (sumHpPct / static_cast<float>(nc)) <= static_cast<float>(Slider(ComboMenu, "RHpU", 30))) ||
                nc >= Slider(ComboMenu, "RCountA", 2);
            if (cond) {
                hero.push_back({ enemy, nc });
            }
        }
        std::sort(hero.begin(), hero.end(),
            [](const Scored& a, const Scored& b) { return a.nearCount > b.nearCount; });
        if (!hero.empty()) {
            if (Bool(ComboMenu, "RSF")) {
                if ((E.IsReady() || player.IsDashing()) && Q.IsReady(500)) {
                    AIBaseClient canE;
                    for (const auto& x : GameObjects::AllGameObjects()) {
                        const auto xb = AIBaseClient(x.Handle());
                        if (xb.IsValid() && Extensions::IsValidTarget(xb, E.Range, true) && CanCastE(xb)) {
                            canE = xb;
                            break;
                        }
                    }
                    if (canE.IsValid()) {
                        E.CastOnUnit(canE);
                        EDelay = SDK::Variables::TickCount() + 120;
                        RDelay = EDelay + 10;
                    }
                }
            }
            if (SDK::Variables::TickCount() > RDelay) {
                AIHeroClient target;
                if (!Bool(ComboMenu, "CRDelay")) {
                    target = hero.front().hero;
                } else {
                    for (const auto& h : hero) {
                        if (CanCastDelayR(h.hero)) {
                            target = h.hero;
                            break;
                        }
                    }
                }
                if (target.IsValid()) {
                    R.Cast(target.Position());
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

    if (Bool(ComboMenu, "CE") && E.IsReady() && !player.Spellbook().IsWindingUp()) {
        if (!KeyActive(ComboMenu, "CEMode")) {
            auto target = Q.GetTarget(75.0f);
            if (target.IsValid() && HaveQ3() && Q.IsReady(500)) {
                const auto nearObj = GetNearObj(AIBaseClient(target.Handle()), true, KeyActive(OtherMenu, "OQUnderTower"), false);
                if (nearObj.IsValid() &&
                    (CountEnemyHeroesAround(PosAfterE(nearObj), 300.0f) > 1 ||
                     player.CountEnemyHeroesInRange(Q.Range + E.Range / 2.0f) == 1) &&
                    E.CastOnUnit(nearObj)) {
                    EDelay = SDK::Variables::TickCount() + 100;
                    return;
                }
            }
            target = Q.GetTarget(Q.Width);
            if (!target.IsValid()) {
                target = Q3.GetTarget();
            }
            if (target.IsValid()) {
                const auto nearObj = GetNearObj(AIBaseClient(target.Handle()), false, KeyActive(OtherMenu, "OQUnderTower"), false);
                if (nearObj.IsValid() &&
                    (nearObj.NetworkId() == target.NetworkId()
                         ? !AutoAttack::InAutoAttackRange(target)
                         : player.Distance(target) > static_cast<float>(Slider(ComboMenu, "CEDIS", 300)) + nearObj.BoundingRadius()) &&
                    E.CastOnUnit(nearObj)) {
                    EDelay = SDK::Variables::TickCount() + 100;
                    return;
                }
            }
        } else {
            const auto nearMouse = TSGetTargetFrom(250.0f, Game::CursorPos());
            if (Bool(ComboMenu, "CeAllc") && nearMouse.IsValid()) {
                const auto nearObj = GetNearObj(AIBaseClient(nearMouse.Handle()), false, KeyActive(OtherMenu, "OQUnderTower"), false);
                if (nearObj.IsValid() &&
                    (nearObj.NetworkId() == nearMouse.NetworkId()
                         ? !AutoAttack::InAutoAttackRange(nearMouse)
                         : player.Distance(nearMouse) > static_cast<float>(Slider(ComboMenu, "CEDIS", 300))) &&
                    E.CastOnUnit(nearObj)) {
                    EDelay = SDK::Variables::TickCount() + 100;
                    return;
                }
            } else {
                const auto nearObj = GetNearObj(AIBaseClient(), false, KeyActive(OtherMenu, "OQUnderTower"), false);
                if (nearObj.IsValid() && player.Distance(Game::CursorPos()) > static_cast<float>(Slider(ComboMenu, "CEDIS", 300)) &&
                    E.CastOnUnit(nearObj)) {
                    EDelay = SDK::Variables::TickCount() + 100;
                    return;
                }
            }
        }
    }

    if (Q.IsReady() && (player.IsDashing() || EDelay < SDK::Variables::TickCount()) &&
        QQDelay < SDK::Variables::TickCount()) {
        if (RDelay > EDelay && SDK::Variables::TickCount() - RDelay < 120) {
            if (Q.Cast(Bool(ComboMenu, "CEBUG") ? Vector3(50000.0f, 50000.0f, 50000.0f) : player.Position())) {
                QQDelay = SDK::Variables::TickCount() + 50;
            }
            auto rt = GetRTarget();
            if (!rt.empty()) {
                R.Cast(rt.front().Position());
            }
            return;
        }
        if (player.IsDashing()) {
            const Vector3 dashEnd = Extensions::GetDashInfo(player).EndPos;
            const auto qCirTarget = TSGetTargetFrom(300.0f, dashEnd);
            if (qCirTarget.IsValid()) {
                const Vector3 castPos = Bool(ComboMenu, "CEBUG")
                    ? Vector3(50000.0f, 50000.0f, 50000.0f)
                    : player.Position();
                if (Q.Cast(castPos)) {
                    QQDelay = SDK::Variables::TickCount() + 50;
                }
            }
            if (!HaveQ3() && Bool(ComboMenu, "CE") && Bool(ComboMenu, "CEStackQ", false) &&
                player.CountEnemyHeroesInRange(450.0f) == 0 && StackQ()) {
                QQDelay = SDK::Variables::TickCount() + 50;
                return;
            }
        } else {
            if (!player.Spellbook().IsWindingUp()) {
                if (!HaveQ3()) {
                    const auto target = Q.GetTarget(Q.Width / 2.0f);
                    if (ValidHeroTarget(target)) {
                        const auto pred = Q.GetPrediction(target);
                        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                            if (Q.Cast(pred.GetCastPosition())) {
                                QQDelay = SDK::Variables::TickCount() + 50;
                            }
                            return;
                        }
                    }
                } else if (CastQ3()) {
                    return;
                }
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!Q.IsReady() || player.IsDashing()) {
        return;
    }

    if (!HaveQ3()) {
        const auto state = Q.CastOnBestTarget();
        if (state == CastStates::SuccessfullyCasted) {
            return;
        }
        if ((state == CastStates::InvalidTarget || state == CastStates::NotCasted) &&
            Bool(HarassMenu, "HQLastHit") && !Q.GetTarget().IsValid()) {
            AIMinionClient best;
            float bestHp = -1.0f;
            for (const auto& i : GameObjects::EnemyMinions()) {
                if (!i.IsEnemy() || !(i.IsMinion() || i.IsPet()) || !Extensions::IsValidTarget(i, FLT_MAX, true)) {
                    continue;
                }
                const auto ib = AIBaseClient(i.Handle());
                if (GetQHpPred(ib) > 0.0f && GetQHpPred(ib) <= GetQDmg(ib) &&
                    (i.IsUnderAllyTurret() || (i.IsUnderEnemyTurret() && !player.IsUnderEnemyTurret()) ||
                     i.DistanceToPlayer() > AutoAttack::GetRealAutoAttackRange(i) + 50.0f ||
                     i.Health() > Damage::GetAutoAttackDamage(player, ib))) {
                    if (i.MaxHealth() > bestHp) {
                        bestHp = i.MaxHealth();
                        best = i;
                    }
                }
            }
            if (best.IsValid()) {
                Q.Cast(best.PreviousPosition());
            }
        }
    } else if (Bool(HarassMenu, "HQ3")) {
        CastQ3();
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if ((Bool(LaneClearMenu, "LE") || Bool(JungleClearMenu, "JE")) && E.IsReady() && !player.IsDashing()) {
        // Hoist snapshot minion/jungle 1 lần: dùng lại cho vòng build ngoài và
        // vòng nearMinion bên trong (frame đã đóng băng nên = kết quả y hệt),
        // tránh copy lại 2 vector mỗi mob trong loop O(n²).
        const auto laneMinionsSnap = GameObjects::EnemyMinions();
        const auto laneJungleSnap = GameObjects::Jungle();
        std::vector<AIBaseClient> minion;
        for (const auto& m : laneMinionsSnap) {
            if (ValidTarget(m, E.Range)) {
                minion.push_back(AIBaseClient(m.Handle()));
            }
        }
        for (const auto& j : laneJungleSnap) {
            if (ValidTarget(j, E.Range)) {
                minion.push_back(AIBaseClient(j.Handle()));
            }
        }
        std::vector<AIBaseClient> filtered;
        for (const auto& i : minion) {
            if (CanCastE(i) && (!IsUnderEnemyTurretPos(PosAfterE(i)) || KeyActive(OtherMenu, "OQUnderTower"))) {
                filtered.push_back(i);
            }
        }
        std::sort(filtered.begin(), filtered.end(),
            [](const AIBaseClient& a, const AIBaseClient& b) { return a.MaxHealth() > b.MaxHealth(); });
        if (!filtered.empty()) {
            AIBaseClient obj;
            for (const auto& i : filtered) {
                if (E.GetHealthPrediction(i) > 0.0f && E.GetHealthPrediction(i) <= GetEDmg(i)) {
                    obj = i;
                    break;
                }
            }
            if (!obj.IsValid() && Q.IsReady(500) &&
                (!HaveQ3() || (Bool(LaneClearMenu, "LQ3") || Bool(JungleClearMenu, "JQ3")))) {
                std::vector<AIBaseClient> sub;
                for (const auto& mob : filtered) {
                    if (((E.GetHealthPrediction(mob) > 0.0f && E.GetHealthPrediction(mob) - GetEDmg(mob) <= GetQDmg(mob)) ||
                         mob.Team() == GameObjectTeam::Neutral) &&
                        mob.Distance(PosAfterE(mob)) < 225.0f) {
                        sub.push_back(mob);
                    }
                    if (Bool(LaneClearMenu, "LELastHit")) {
                        continue;
                    }
                    std::vector<AIBaseClient> nearMinion;
                    for (const auto& m : laneMinionsSnap) {
                        if (Extensions::IsValidTarget(AIBaseClient(m.Handle()), 225.0f, true, PosAfterE(mob))) {
                            nearMinion.push_back(AIBaseClient(m.Handle()));
                        }
                    }
                    for (const auto& j : laneJungleSnap) {
                        if (Extensions::IsValidTarget(AIBaseClient(j.Handle()), 225.0f, true, PosAfterE(mob))) {
                            nearMinion.push_back(AIBaseClient(j.Handle()));
                        }
                    }
                    int killQ = 0;
                    for (const auto& i : nearMinion) {
                        (void)i;
                        if (E.GetHealthPrediction(mob) > 0.0f && E.GetHealthPrediction(mob) <= GetQDmg(mob)) {
                            ++killQ;
                        }
                    }
                    if (static_cast<int>(nearMinion.size()) > 2 || killQ > 1) {
                        sub.push_back(mob);
                    }
                }
                if (!sub.empty()) {
                    obj = sub.front();
                }
            }
            if (obj.IsValid() && Extensions::IsValidTarget(obj, FLT_MAX, true) &&
                (!obj.IsMinion() || (!Bool(LaneClearMenu, "LELastHit") || obj.Health() < GetEDmg(obj)))) {
                E.CastOnUnit(obj);
                LaneClearDelay = SDK::Variables::TickCount() + 250;
                return;
            }
        }
    }

    if (Q.IsReady() && (LaneClearDelay < SDK::Variables::TickCount() || player.IsDashing())) {
        if (player.IsDashing()) {
            if (!HaveQ1()) {
                Q3.Cast(Bool(ComboMenu, "CEBUG") ? Vector3(50000.0f, 50000.0f, 50000.0f) : player.Position());
            }
            return;
        } else {
            std::vector<AIBaseClient> minion;
            const float qRange = (!HaveQ3() ? Q.Range : Q3.Range) - Q.Width;
            for (const auto& m : GameObjects::EnemyMinions()) {
                if (ValidTarget(m, qRange)) {
                    minion.push_back(AIBaseClient(m.Handle()));
                }
            }
            for (const auto& j : GameObjects::Jungle()) {
                if (ValidTarget(j, qRange)) {
                    minion.push_back(AIBaseClient(j.Handle()));
                }
            }
            std::sort(minion.begin(), minion.end(),
                [](const AIBaseClient& a, const AIBaseClient& b) { return a.MaxHealth() > b.MaxHealth(); });
            if (minion.empty()) {
                return;
            }
            if (!HaveQ3()) {
                AIBaseClient obj;
                for (const auto& i : minion) {
                    if (Q.GetHealthPrediction(i) > 0.0f && Q.GetHealthPrediction(i) <= GetQDmg(i)) {
                        obj = i;
                        break;
                    }
                }
                if (obj.IsValid()) {
                    Q.Cast(obj.Position());
                    return;
                }
            }
            if (Bool(LaneClearMenu, "LQ3")) {
                std::vector<AIBaseClient> minionsForFarm = minion;
                const auto pos = Q3.GetLineFarmLocation(minionsForFarm);
                if (pos.MinionsHit > 0) {
                    Q3.Cast(Vector3::From2D(pos.Position));
                }
            }
        }
    }
}

static void LastHit() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Q.IsReady() && !player.IsDashing()) {
        if (!HaveQ3() && Bool(LastHitMenu, "KQ12")) {
            AIMinionClient best;
            float bestHp = -1.0f;
            for (const auto& i : GameObjects::EnemyMinions()) {
                if (!i.IsEnemy() || !(i.IsMinion() || i.IsPet()) || !ValidTarget(AIBaseClient(i.Handle()), Q.Range)) {
                    continue;
                }
                const auto ib = AIBaseClient(i.Handle());
                if (GetQHpPred(ib) > 0.0f && GetQHpPred(ib) <= GetQDmg(ib) &&
                    (i.IsUnderAllyTurret() || (i.IsUnderEnemyTurret() && !player.IsUnderEnemyTurret()) ||
                     i.DistanceToPlayer() > AutoAttack::GetRealAutoAttackRange(i) + 50.0f ||
                     i.Health() > Damage::GetAutoAttackDamage(player, ib))) {
                    if (i.MaxHealth() > bestHp) {
                        bestHp = i.MaxHealth();
                        best = i;
                    }
                }
            }
            if (best.IsValid() && Q.Cast(best.PreviousPosition())) {
                return;
            }
        } else if (HaveQ3() && Bool(LastHitMenu, "KQ3")) {
            AIMinionClient best;
            float bestHp = -1.0f;
            for (const auto& i : GameObjects::EnemyMinions()) {
                if (!i.IsEnemy() || !(i.IsMinion() || i.IsPet())) {
                    continue;
                }
                const auto ib = AIBaseClient(i.Handle());
                if (Extensions::IsValidTarget(ib, Q3.Range - i.BoundingRadius() / 2.0f, true) &&
                    Q3.GetHealthPrediction(ib) > 0.0f && Q3.GetHealthPrediction(ib) <= GetQDmg(ib) &&
                    (i.IsUnderAllyTurret() || (i.IsUnderEnemyTurret() && !player.IsUnderEnemyTurret()) ||
                     i.DistanceToPlayer() > AutoAttack::GetRealAutoAttackRange(i) + 50.0f ||
                     i.Health() > Damage::GetAutoAttackDamage(player, ib))) {
                    if (i.MaxHealth() > bestHp) {
                        bestHp = i.MaxHealth();
                        best = i;
                    }
                }
            }
            if (best.IsValid()) {
                Q3.Cast(best.PreviousPosition());
            }
        }
    }
    if (Bool(LastHitMenu, "KE") && E.IsReady() && !player.Spellbook().IsWindingUp()) {
        AIMinionClient best;
        float bestHp = -1.0f;
        for (const auto& i : GameObjects::EnemyMinions()) {
            if (!i.IsEnemy() || !(i.IsMinion() || i.IsPet())) {
                continue;
            }
            const auto ib = AIBaseClient(i.Handle());
            if (CanCastE(ib) && E.GetHealthPrediction(ib) > 0.0f && E.GetHealthPrediction(ib) <= GetEDmg(ib) &&
                !IsUnderEnemyTurretPos(PosAfterE(ib)) &&
                (i.IsUnderAllyTurret() || (i.IsUnderEnemyTurret() && !player.IsUnderEnemyTurret()) ||
                 i.DistanceToPlayer() > AutoAttack::GetRealAutoAttackRange(i) + 50.0f ||
                 i.Health() > Damage::GetAutoAttackDamage(player, ib))) {
                if (i.MaxHealth() > bestHp) {
                    bestHp = i.MaxHealth();
                    best = i;
                }
            }
        }
        if (best.IsValid()) {
            E.CastOnUnit(AIBaseClient(best.Handle()));
        }
    }
}

static void Flee() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(FleeMenu, "FQ") && Q.IsReady() && HaveQ3() && !player.IsDashing()) {
        AIHeroClient target;
        float bestDist = FLT_MAX;
        for (const auto& x : GameObjects::EnemyHeroes()) {
            if (x.IsEnemy() && Q3.IsInRange(x) && !x.IsDead() && Extensions::IsValidTarget(x, FLT_MAX, true)) {
                const float d = x.DistanceToPlayer();
                if (d < bestDist) {
                    bestDist = d;
                    target = x;
                }
            }
        }
        if (ValidHeroTarget(target)) {
            const auto pred = Q3.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                Q3.Cast(pred.GetCastPosition());
            }
        }
    }
    if (!Bool(FleeMenu, "FE")) {
        return;
    }
    const auto obj = GetBestObjToMouse();
    if (obj.IsValid() && E.IsReady()) {
        E.CastOnUnit(obj);
    }
    if (player.IsDashing() && !HaveQ3() && Bool(FleeMenu, "FEStackQ") && Q.IsReady() &&
        QQDelay < SDK::Variables::TickCount()) {
        if (Q.Cast(Bool(ComboMenu, "CEBUG") ? Vector3(50000.0f, 50000.0f, 50000.0f) : player.Position())) {
            QQDelay = SDK::Variables::TickCount() + 50;
        }
    }
}

static AIBaseClient GetBestObjToMouse(bool /*underTower*/) {
    const Vector3 pos = Game::CursorPos();
    const auto player = Player();
    AIBaseClient best;
    float bestDist = FLT_MAX;
    for (const auto& x : GetDashObj()) {
        if (PosAfterE(x).Distance(pos) < player.Distance(pos)) {
            const float d = PosAfterE(x).Distance(pos);
            if (d < bestDist) {
                bestDist = d;
                best = x;
            }
        }
    }
    return best;
}

// C#: GetEDmg — wiki (patch V26.x): base 70/85/100/115/130 = 55+15*lvl,
// +60% AP (FlatMagicDamageMod*0.6) +20% bonus AD (FlatPhysicalDamageMod*0.2),
// stack Ride the Wind +25%/stack (YasuoDashScalar). CalculateMagicDamage - 15.
static float GetEDmg(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const int stackCount = player.GetBuffCount("YasuoDashScalar");
    const float stacks = 1.0f + 0.25f * static_cast<float>(stackCount > 0 ? stackCount : 0);
    const float damage = (55.0f + 15.0f * static_cast<float>(E.Instance().Level())) * stacks +
        player.AP() * 0.6f + player.BonusAttackDamage() * 0.2f;
    return player.CalculateMagicDamage(target, damage) - 15.0f;
}

static float GetQHpPred(const AIBaseClient& minion) {
    return Prediction::Health::GetPrediction(minion, static_cast<int>(Q.Delay * 1000.0f - 100.0f));
}

// C#: GetQDmg — Q.GetDamage + Sheen/Trinity proc.
static float GetQDmg(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    float dmgItem = 0.0f;
    if (player.HasItem(3057) && (SDK::Items::CanUseItem(player, 3057) || player.HasBuff("Sheen"))) {
        dmgItem = player.BaseAttackDamage();
    }
    if (player.HasItem(3078) && (SDK::Items::CanUseItem(player, 3078) || player.HasBuff("Sheen"))) {
        dmgItem = player.BaseAttackDamage() * 2.0f;
    }
    if (dmgItem > 0.0f) {
        dmgItem = player.CalculatePhysicalDamage(target, dmgItem);
    }
    return Q.GetDamage(target) + dmgItem;
}

static bool CastQ3() {
    const auto target = TSGetTarget(Q3.Range + (Q3.Width / 2.0f), DamageType::Physical);
    if (target.IsValid()) {
        const auto pred = Q3.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            if (Q3.Cast(pred.GetCastPosition())) {
                QQDelay = SDK::Variables::TickCount() + 50;
            }
        }
    }
    return true;
}

static bool StackQ() {
    const auto player = Player();
    const Vector3 pos = Extensions::GetDashInfo(player).EndPos;
    std::vector<AIBaseClient> objList;
    for (const auto& x : GameObjects::EnemyHeroes()) {
        if (x.IsEnemy()) {
            objList.push_back(AIBaseClient(x.Handle()));
        }
    }
    for (const auto& m : GameObjects::EnemyMinions()) {
        objList.push_back(AIBaseClient(m.Handle()));
    }
    std::vector<AIBaseClient> filtered;
    for (const auto& y : objList) {
        if (Extensions::IsValidTarget(y, 300.0f, true, pos)) {
            filtered.push_back(y);
        }
    }
    if (filtered.empty()) {
        return false;
    }
    const auto target = filtered.front();
    return target.IsValid() &&
        Q.Cast(player.Position().Extend(target.Position(), Bool(ComboMenu, "CEBUG") ? 50000.0f : Q.Range));
}

// C#: CanCastDelayR — buff knockup/knockback còn ít thời gian (sắp hết) → R chốt.
static bool CanCastDelayR(const AIHeroClient& target) {
    if (!target.IsValid()) {
        return false;
    }
    uintptr_t buffs[256] = {};
    const int count = ::CoreBuffs::Enumerate(target.Address(), buffs, 256);
    const float now = Game::Time();
    for (int i = 0; i < count; ++i) {
        const ::CoreBuffs::BuffRef b{ buffs[i] };
        if (!b.IsActive(now)) {
            continue;
        }
        const int type = b.GetType();
        if (type == static_cast<int>(SDK::BuffType::Knockback) || type == static_cast<int>(SDK::BuffType::Knockup)) {
            const float endTime = b.GetEndTime();
            const float startTime = b.GetStartTime();
            const float total = endTime - startTime;
            const float divisor = (total <= 0.5f) ? 1.5f : 4.0f;
            if ((endTime - now) <= (total / divisor)) {
                return true;
            }
        }
    }
    return false;
}

// C#: GetNearObj — chọn obj E tới gần `pos` nhất (pos = target hoặc cursor).
static AIBaseClient GetNearObj(const AIBaseClient& target, bool inQCir, bool underTower, bool checkFace) {
    const auto player = Player();
    const Vector3 pos = target.IsValid() ? target.PreviousPosition() : Game::CursorPos();
    const bool rReady = R.IsReady() && !GetRTarget().empty();
    const bool onlyQ23 = Bool(ComboMenu, "CEOnlyQ23", false);

    std::vector<AIBaseClient> obj;
    for (const auto& i : GameObjects::EnemyHeroes()) {
        if (i.IsEnemy() && !i.IsRecalling() &&
            (rReady || !onlyQ23 || ((HaveQ2() || HaveQ3()) && Q.IsReady(500)))) {
            obj.push_back(AIBaseClient(i.Handle()));
        }
    }
    for (const auto& m : GameObjects::EnemyMinions()) {
        obj.push_back(AIBaseClient(m.Handle()));
    }
    for (const auto& j : GameObjects::Jungle()) {
        obj.push_back(AIBaseClient(j.Handle()));
    }

    AIBaseClient best;
    float bestDist = FLT_MAX;
    for (const auto& i : obj) {
        if (CanCastE(i) && (!checkFace || Extensions::IsFacing(player, i)) &&
            (underTower || !IsUnderEnemyTurretPos(PosAfterE(i))) &&
            PosAfterE(i).Distance(pos) < (inQCir ? 300.0f : player.Distance(pos))) {
            const float d = PosAfterE(i).Distance(pos);
            if (d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
    }
    return best;
}

// ─────────────────────────────────────────────────────────────────────────
// EvadeTarget — né/chặn đạn nhắm Yasuo (nested class C#).
// ─────────────────────────────────────────────────────────────────────────
namespace EvadeTarget {

static MissileClient FindWall() {
    const auto player = Player();
    for (const auto& m : SDK::ObjectManager::Get<MissileClient>()) {
        if (m.IsValid() && m.SpellName() == "YasuoW_VisualMis" && m.Team() == player.Team()) {
            return m;
        }
    }
    return MissileClient();
}

// C#: GoThroughWall — đoạn (pos1,pos2) có cắt tường W không.
static bool GoThroughWall(const Vector2& pos1, const Vector2& pos2) {
    const auto wall = FindWall();
    if (!wall.IsValid()) {
        return false;
    }
    const float wallWidth = 375.0f;
    const Vector2 wallDir = SDK::Prediction::Vec2Ext::Perpendicular(
        (wall.Position().To2D() - WallCastedPos).Normalized());
    const Vector2 wallStart = wall.Position().To2D() + wallDir * (wallWidth / 2.0f);
    const Vector2 wallEnd = wallStart - wallDir * wallWidth;
    // Rectangle(wallStart, wallEnd, 75) → polygon points; kiểm tra giao cắt cạnh.
    SDK::RectanglePoly rect(wallStart, wallEnd, 75.0f);
    const auto& pts = rect.Points;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const Vector2& a = pts[i];
        const Vector2& b = pts[i != pts.size() - 1 ? i + 1 : 0];
        const auto inter = SDK::Prediction::Vec2Ext::Intersection(a, b, pos1, pos2);
        if (inter.Valid) {
            return true;
        }
    }
    return false;
}

static void LoadSpellData() {
    Spells.clear();
    Spells.push_back({ "Ahri", { "ahriwdamagemissileback1", "ahriwdamagemissilefront1", "ahriwdamagemissileright1" }, SpellSlot::W });
    Spells.push_back({ "Ahri", { "ahritumblemissile" }, SpellSlot::R });
    Spells.push_back({ "Akshan", { "akshanrmissile" }, SpellSlot::R });
    Spells.push_back({ "Anivia", { "frostbite" }, SpellSlot::E });
    Spells.push_back({ "Annie", { "annieq" }, SpellSlot::Q });
    Spells.push_back({ "Brand", { "brandr", "brandrmissile" }, SpellSlot::R });
    Spells.push_back({ "Caitlyn", { "caitlynrmissile" }, SpellSlot::R });
    Spells.push_back({ "Elise", { "elisehumanq" }, SpellSlot::Q });
    Spells.push_back({ "Ezreal", { "ezrealemissile" }, SpellSlot::E });
    Spells.push_back({ "FiddleSticks", { "fiddlesticksqmissilefear" }, SpellSlot::Q });
    Spells.push_back({ "Gangplank", { "gangplankqproceed" }, SpellSlot::Q });
    Spells.push_back({ "Janna", { "sowthewind" }, SpellSlot::W });
    Spells.push_back({ "Kassadin", { "nulllance" }, SpellSlot::Q });
    Spells.push_back({ "Katarina", { "katarinaq", "katarinaqdaggerarc" }, SpellSlot::Q });
    Spells.push_back({ "Kindred", { "kindrede" }, SpellSlot::E });
    Spells.push_back({ "Leblanc", { "leblancq", "leblancrq" }, SpellSlot::Q });
    Spells.push_back({ "Lillia", { "lilliarexpungemissile" }, SpellSlot::R });
    Spells.push_back({ "Lulu", { "luluw" }, SpellSlot::W });
    Spells.push_back({ "Malphite", { "seismicshard" }, SpellSlot::Q });
    Spells.push_back({ "MissFortune", { "missfortunericochetshot", "missfortunershotextra" }, SpellSlot::Q });
    Spells.push_back({ "Nami", { "namiwenemy", "namiwmissileenemy" }, SpellSlot::W });
    Spells.push_back({ "Ryze", { "ryzee" }, SpellSlot::E });
    Spells.push_back({ "Shaco", { "twoshivpoison" }, SpellSlot::E });
    Spells.push_back({ "Sona", { "sonaqmissile" }, SpellSlot::Q });
    Spells.push_back({ "Syndra", { "syndrarspell" }, SpellSlot::R });
    Spells.push_back({ "Teemo", { "blindingdart" }, SpellSlot::Q });
    Spells.push_back({ "Tristana", { "tristanae" }, SpellSlot::E });
    Spells.push_back({ "Tristana", { "tristanar" }, SpellSlot::R });
    Spells.push_back({ "TwistedFate", { "bluecardattack" }, SpellSlot::W });
    Spells.push_back({ "TwistedFate", { "goldcardattack" }, SpellSlot::W });
    Spells.push_back({ "TwistedFate", { "redcardattack" }, SpellSlot::W });
    Spells.push_back({ "Urgot", { "urgotrrecastmissile" }, SpellSlot::R });
    Spells.push_back({ "Vayne", { "vaynecondemnmissile" }, SpellSlot::E });
    Spells.push_back({ "Veigar", { "veigarr" }, SpellSlot::R });
    Spells.push_back({ "Viktor", { "viktorpowertransfer" }, SpellSlot::Q });
}

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static bool IsSpellMenuEnabled(const SpellDataEntry& sp) {
    if (!EvadeTargetListMenu) {
        return false;
    }
    Menu* sub = EvadeTargetListMenu->Get<Menu>(("scc" + sp.ChampionName).c_str());
    if (!sub) {
        return false;
    }
    const auto* item = sub->Get<MenuBool>(sp.MissileName().c_str());
    return item && item->Value;
}

static void ObjSpellMissileOnCreate(const GameObject& sender) {
    const auto player = Player();
    const auto missile = MissileClient(sender.Handle());
    if (!missile.IsValid()) {
        return;
    }
    const std::string mName = ToLower(missile.SpellName());
    // caster hero (địch).
    AIHeroClient caster;
    const int casterId = missile.CasterNetworkId();
    for (const auto& h : GameObjects::EnemyHeroes()) {
        if (static_cast<int>(h.NetworkId()) == casterId) {
            caster = h;
            break;
        }
    }
    if (!caster.IsValid() || caster.Team() == player.Team()) {
        return;
    }

    const SpellDataEntry* spellData = nullptr;
    for (const auto& sp : Spells) {
        bool nameMatch = false;
        for (const auto& n : sp.SpellNames) {
            if (n == mName) {
                nameMatch = true;
                break;
            }
        }
        if (nameMatch && IsSpellMenuEnabled(sp)) {
            spellData = &sp;
            break;
        }
    }

    SpellDataEntry autoData;
    if (spellData == nullptr) {
        const bool isCrit = mName.find("crit") != std::string::npos;
        const bool useBAttack = !isCrit && Bool(EvadeMenu, "BAttack") &&
            player.HealthPercent() < static_cast<float>(Slider(EvadeMenu, "BAttackHpU", 35));
        const bool useCAttack = isCrit && Bool(EvadeMenu, "CAttack") &&
            player.HealthPercent() < static_cast<float>(Slider(EvadeMenu, "CAttackHpU", 40));
        if (useBAttack || useCAttack) {
            autoData.ChampionName = caster.CharacterName();
            autoData.SpellNames = { missile.SpellName() };
            spellData = &autoData;
        }
    }
    if (spellData == nullptr) {
        return;
    }
    // MISSING API: missile.Target (đạn có nhắm mình?) — dựa DB-match + menu. Xem missapi.md.
    TargetEntry entry;
    entry.Obj = missile;
    entry.Start = caster.Position();
    DetectedTargets.push_back(entry);
}

static void ObjSpellMissileOnDelete(const GameObject& sender) {
    const auto missile = MissileClient(sender.Handle());
    if (!missile.IsValid()) {
        return;
    }
    const int netId = missile.NetworkId();
    DetectedTargets.erase(
        std::remove_if(DetectedTargets.begin(), DetectedTargets.end(),
            [netId](const TargetEntry& t) { return t.Obj.IsValid() && t.Obj.NetworkId() == netId; }),
        DetectedTargets.end());
}

static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    const auto sender = AIBaseClient(args.Sender.Ptr);
    if (!sender.IsValid() || sender.Team() != player.Team() ||
        std::string(args.SpellName) != "YasuoWMovingWall") {
        return;
    }
    WallCastedPos = sender.Position().To2D();
}

static void OnUpdateTarget(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (SDK::HasBuffOfType(AIBaseClient(player.Handle()), SDK::BuffType::SpellImmunity) ||
        SDK::HasBuffOfType(AIBaseClient(player.Handle()), SDK::BuffType::SpellShield)) {
        return;
    }
    const auto wall = FindWall();
    if (!W.IsReady(300) && (!wall.IsValid() || !E.IsReady(200))) {
        return;
    }
    for (const auto& target : DetectedTargets) {
        if (!target.Obj.IsValid() || player.Distance(target.Obj.Position()) >= 700.0f) {
            continue;
        }
        if (E.IsReady() && Bool(EvadeMenu, "E") && wall.IsValid() &&
            !GoThroughWall(player.Position().To2D(), target.Obj.Position().To2D()) &&
            W.IsInRange(target.Obj.Position())) {
            std::vector<AIBaseClient> obj;
            for (const auto& m : GameObjects::EnemyMinions()) {
                if (Extensions::IsValidTarget(AIBaseClient(m.Handle()), E.Range, true)) {
                    obj.push_back(AIBaseClient(m.Handle()));
                }
            }
            for (const auto& h : GameObjects::EnemyHeroes()) {
                if (Extensions::IsValidTarget(h, E.Range, true)) {
                    obj.push_back(AIBaseClient(h.Handle()));
                }
            }
            std::vector<AIBaseClient> filtered;
            for (const auto& i : obj) {
                if (CanCastE(i) &&
                    (!IsUnderEnemyTurretPos(PosAfterE(i)) || Bool(EvadeMenu, "ETower", false)) &&
                    GoThroughWall(player.Position().To2D(), PosAfterE(i).To2D())) {
                    filtered.push_back(i);
                }
            }
            std::sort(filtered.begin(), filtered.end(),
                [](const AIBaseClient& a, const AIBaseClient& b) {
                    return PosAfterE(a).Distance(Game::CursorPos()) < PosAfterE(b).Distance(Game::CursorPos());
                });
            bool casted = false;
            for (const auto& i : filtered) {
                if (E.CastOnUnit(i)) {
                    casted = true;
                    break;
                }
            }
            if (casted) {
                return;
            }
        }
        if (W.IsReady() && Bool(EvadeMenu, "W") && W.IsInRange(target.Obj.Position())) {
            W.Cast(player.Position().Extend(target.Start, 100.0f));
        }
    }
}

inline void Init() {
    LoadSpellData();

    EvadeMenu = MenuRoot->AddSubMenu(new Menu("EvadeTarget", "Wind Wall Evade"));
    EvadeMenu->Add(new MenuBool("W", "Use W"));
    EvadeMenu->Add(new MenuBool("E", "Use E (behind W)"));
    EvadeMenu->Add(new MenuBool("ETower", "-> Under Tower", false));
    EvadeMenu->Add(new MenuBool("BAttack", "Auto Attack"));
    EvadeMenu->Add(new MenuSlider("BAttackHpU", "-> If HP <", 35, 0, 100));
    EvadeMenu->Add(new MenuBool("CAttack", "Crit"));
    EvadeMenu->Add(new MenuSlider("CAttackHpU", "-> If HP <", 40, 0, 100));
    EvadeTargetListMenu = EvadeMenu->AddSubMenu(new Menu("EvadeTargetList", "Evade Targets"));
    for (const auto& sp : Spells) {
        bool present = false;
        for (const auto& h : GameObjects::EnemyHeroes()) {
            if (h.CharacterName() == sp.ChampionName) {
                present = true;
                break;
            }
        }
        if (!present) {
            continue;
        }
        Menu* existing = EvadeTargetListMenu->Get<Menu>(("scc" + sp.ChampionName).c_str());
        Menu* sub = existing ? existing : EvadeTargetListMenu->AddSubMenu(new Menu(("scc" + sp.ChampionName).c_str(), sp.ChampionName.c_str()));
        sub->Add(new MenuBool(sp.MissileName().c_str(), (sp.MissileName() + " (" + std::to_string(static_cast<int>(sp.Slot)) + ")").c_str(), true));
    }

    Events::hook.OnGameUpdate += &OnUpdateTarget;
    GameObjects::AddOnCreate(&ObjSpellMissileOnCreate);
    GameObjects::AddOnDelete(&ObjSpellMissileOnDelete);
    Events::hook.OnProcessSpell += &OnProcessSpellCast;
}

inline void OnUnload() {
    Events::hook.OnGameUpdate -= &OnUpdateTarget;
    GameObjects::RemoveOnCreate(&ObjSpellMissileOnCreate);
    GameObjects::RemoveOnDelete(&ObjSpellMissileOnDelete);
    Events::hook.OnProcessSpell -= &OnProcessSpellCast;
    DetectedTargets.clear();
}

} // namespace EvadeTarget

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnDoCast -= &Game_OnDoCast;
    Events::hook.OnDoCast -= &Game_HideAnimation;
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Drawing::OnDraw -= &OnDraw;
    Events::hook.OnInterruptableSpell -= &OnInterrupterSpell;
    Events::hook.OnGapCloser -= &OnGapcloser;
    EvadeTarget::OnUnload();

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Yasuo
