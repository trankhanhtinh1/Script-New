#pragma once

// ============================================================================
// SharpShooter AIO — Syndra
// Port từ CSharpFiles/Syndra/Syndra.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Orianna.h.
//
// Kỹ năng:
//   Q Dark Sphere    — circle 800, delay 0.6, radius 70. Tạo cầu (Seed).
//   W Force of Will  — grab-throw 925: nhặt cầu/minion (buff syndrawtooltip) rồi ném.
//   E Scatter        — cone 700 knockback; QE = E đẩy cầu Q thành line 1100.
//   R Unleashed Power— targeted 675 (lv3 = 750), damage theo số cầu (ammo).
//
// Ghi chú port (giữ 1-1 với C#):
//   * SeedInfo tracking: Seed (MaxHealth==1, IsAlly) qua OnCreate/OnDelete.
//   * QE: Q ném cầu, sau 100ms E đẩy cầu về EndPos (OnProcessSpell "SyndraQSpell").
//   * W 2-pha: chưa có buff syndrawtooltip → W nhặt Objects() (cầu/minion/jungle);
//     có buff → W ném vào target prediction.
//   * QEEvent: hotkey QE, 3 mode (target/mouse/smart).
//   * lastw/lastwe/lastqe/delayyyy: timing gate của C# (Variables.TickCount).
//   * MISSING API: DelayAction.Add(100, E.Cast) → dùng gate lastw + cast E trong
//     OnProcessSpell ngay (không delay 100ms chính xác). Xem missapi.md.
//   * R damage: wiki per-sphere 80/120/160 + 20% AP/sphere (C# dùng 90/135/180 —
//     cập nhật theo wiki). Giữ cấu trúc ammo-based của C#.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Syndra {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* EngageMenu = nullptr;
inline Menu* BlackListMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* AAMenu = nullptr;
inline Menu* LaneMenu = nullptr;
inline Menu* JungleMenu = nullptr;
inline Menu* KSMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 800.0f };
inline Spell W{ SpellSlot::W, 925.0f };
inline Spell E{ SpellSlot::E, 700.0f };
inline Spell R{ SpellSlot::R, 675.0f };
inline Spell QE{ SpellSlot::E, 1100.0f };

inline bool Loaded = false;

inline int delayyyy = 0;
inline int lastwe = 0;
inline int lastw = 0;
inline int lastqe = 0;

// SeedInfo tracking (C#).
struct SeedInfo {
    AIBaseClient Pointer;
    int VaildTime = 0;
};
inline std::vector<SeedInfo> SeedsInfo;

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

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::string RuntimeObjectName(const GameObject& object) {
    char name[128] = {};
    if (object.IsValid() &&
        ::Core::Objects::ReadName(object.Address(), name, static_cast<int>(sizeof(name))) && name[0]) {
        return name;
    }
    return object.CharacterName();
}

// C#: R blacklist theo tên (lowercase).
static bool IsBlackListR(const AIHeroClient& unit) {
    if (!BlackListMenu || !unit.IsValid()) {
        return false;
    }
    const std::string key = ToLower(unit.CharacterName());
    const auto* item = BlackListMenu->Get<MenuBool>(key.c_str());
    return item ? item->Value : false;
}

// C#: Objects() — cầu/minion/jungle gần nhất trong tầm W để W nhặt.
static AIBaseClient Objects() {
    const auto player = Player();
    if (!player.IsValid()) {
        return AIBaseClient();
    }
    for (const auto& s : SeedsInfo) {
        if (s.Pointer.IsValid() && s.Pointer.Distance(player) < W.Range) {
            return s.Pointer;
        }
    }
    for (const auto& m : GameObjects::EnemyMinions()) {
        if (ValidTarget(m, W.Range)) {
            return AIBaseClient(m.Handle());
        }
    }
    for (const auto& j : GameObjects::Jungle()) {
        if (ValidTarget(j, W.Range)) {
            return AIBaseClient(j.Handle());
        }
    }
    return AIBaseClient();
}

// ── R damage theo C# GetR (cấu trúc ammo-based), base per-sphere theo wiki ──
// wiki: 80/120/160 + 20% AP mỗi cầu. main = (ap+base)*4; extra khi ammo>3.
static float GetR(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const int level = R.Instance().Level();
    float meow = 0.0f;
    if (level == 1) {
        meow = 80.0f;
    } else if (level == 2) {
        meow = 120.0f;
    } else if (level >= 3) {
        meow = 160.0f;
    }
    const float ap = player.AP() * 0.20f;
    const float main = (ap + meow) * 4.0f;
    float extra = 0.0f;
    const int ammo = R.Instance().Ammo();
    if (ammo > 3) {
        extra = (ap + meow) * static_cast<float>(ammo - 3);
    }
    const float together = main + extra;
    return player.CalculateMagicDamage(target, together);
}

// C#: AllDmg = Q+W+E+R (dùng cho draw damage).
static float AllDmg(const AIBaseClient& unit) {
    const auto player = Player();
    if (!unit.IsValid() || !player.IsValid()) {
        return 0.0f;
    }
    float damage = 0.0f;
    if (Q.IsReady()) {
        damage += player.GetSpellDamage(unit, SpellSlot::Q);
    }
    if (W.IsReady()) {
        damage += player.GetSpellDamage(unit, SpellSlot::W);
    }
    if (E.IsReady()) {
        damage += player.GetSpellDamage(unit, SpellSlot::E);
    }
    if (R.IsReady()) {
        damage += GetR(unit);
    }
    return damage;
}

// Forward declarations — đúng thứ tự file C#.
static void AIBaseClient_OnProcessSpellCast(const Events::ProcessSpellEventArgs& args);
static void OnDraw();
static void OnCreateSeedObj(const GameObject& obj);
static void OnRemoveSeedObj(const GameObject& obj);
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void AutoDashQ();
static void Harass();
static void Combo();
static void JungleClear();
static void LaneClear();
static void Killsteal();
static void QEEvent();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Syndra", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("combo", "Combo"));
    ComboMenu->Add(new MenuBool("useq", "Use Q"));
    ComboMenu->Add(new MenuBool("useqe", "Use QE"));
    ComboMenu->Add(new MenuSlider("range", "QE Distance", 1100, 800, 1150));
    ComboMenu->Add(new MenuBool("usew", "Use W"));
    ComboMenu->Add(new MenuBool("usee", "Use E"));

    EngageMenu = ComboMenu->AddSubMenu(new Menu("engage", "R Set"));
    EngageMenu->Add(new MenuList("rmode", "R Mode",
        std::vector<std::string>{ "Damage", "Speed" }, 0));
    EngageMenu->Add(new MenuBool("user", "Use R in Combo"));
    EngageMenu->Add(new MenuSlider("waster", "Don't save R if enemy HP <=", 0, 0, 500));
    EngageMenu->Add(new MenuSlider("orb", "Min Orb count for fast R", 5, 3, 6));
    EngageMenu->Add(new MenuBool("kill", "Only fast R if killable"));

    ComboMenu->Add(new MenuKeyBind("qe", "Manual QE key", 'T', KeyBindType::Press));
    ComboMenu->Add(new MenuList("qemode", "QE Mode",
        std::vector<std::string>{ "Target", "Mouse", "Smart" }, 0));

    BlackListMenu = MenuRoot->AddSubMenu(new Menu("blacklist", "R BlackList"));
    for (const auto& target : GameObjects::EnemyHeroes()) {
        const std::string name = target.CharacterName();
        if (name.empty()) {
            continue;
        }
        const std::string key = ToLower(name);
        BlackListMenu->Add(new MenuBool(key.c_str(), name.c_str(), false));
    }

    HarassMenu = MenuRoot->AddSubMenu(new Menu("harass", "Harass"));
    HarassMenu->Add(new MenuKeyBind("qtoggle", "Auto Harass", 'K', KeyBindType::Toggle));
    HarassMenu->Add(new MenuSlider("mana", "Harass if Mana >= X%", 50, 0, 100));
    HarassMenu->Add(new MenuBool("dashing", "Auto Q on dash"));
    HarassMenu->Add(new MenuBool("useq", "Use Q Harass"));
    HarassMenu->Add(new MenuBool("usew", "Use W Harass"));
    HarassMenu->Add(new MenuBool("usee", "Use E Harass"));

    AAMenu = MenuRoot->AddSubMenu(new Menu("aa", "AA Disable"));
    AAMenu->Add(new MenuBool("disable", "Disable AA in Combo", false));
    AAMenu->Add(new MenuSlider("level", "Disable AA if Level >= X", 6, 1, 18));
    AAMenu->Add(new MenuBool("enaifcd", "Enable AA if all spells CD"));
    AAMenu->Add(new MenuBool("enaifmana", "Enable AA if low mana"));

    LaneMenu = MenuRoot->AddSubMenu(new Menu("lane", "LaneClear"));
    LaneMenu->Add(new MenuSlider("mana", "LaneClear if Mana >= X%", 50, 0, 100));
    LaneMenu->Add(new MenuBool("useq", "Use Q"));
    LaneMenu->Add(new MenuSlider("hitq", "Q if hit >= X minions", 2, 1, 6));
    LaneMenu->Add(new MenuBool("usew", "Use W"));
    LaneMenu->Add(new MenuSlider("hitw", "W if hit >= X minions", 3, 1, 6));

    JungleMenu = MenuRoot->AddSubMenu(new Menu("jungle", "JungleClear"));
    JungleMenu->Add(new MenuSlider("mana", "JungleClear if Mana >= X%", 50, 0, 100));
    JungleMenu->Add(new MenuBool("useq", "Use Q"));
    JungleMenu->Add(new MenuBool("usew", "Use W"));

    KSMenu = MenuRoot->AddSubMenu(new Menu("killsteal", "Killsteal"));
    KSMenu->Add(new MenuBool("ksq", "Killsteal Q"));
    KSMenu->Add(new MenuBool("ksw", "Killsteal W"));
    KSMenu->Add(new MenuBool("ksr", "Killsteal R"));
    KSMenu->Add(new MenuSlider("waster", "Don't save R if enemy HP <=", 0, 0, 500));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("drawings", "Draw"));
    DrawMenu->Add(new MenuBool("drawq", "Draw Q Range"));
    DrawMenu->Add(new MenuBool("drawqe", "Draw QE Range"));
    DrawMenu->Add(new MenuBool("draww", "Draw W Range", false));
    DrawMenu->Add(new MenuBool("drawe", "Draw E Range", false));
    DrawMenu->Add(new MenuBool("drawr", "Draw R Range"));
    DrawMenu->Add(new MenuBool("drawdamage", "Draw R Damage"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 800.0f);
    Q.SetSkillshot(0.6f, 70.0f, FLT_MAX, false, SpellType::Circle);
    Q.DamageType = DamageType::Magical;

    W = Spell(SpellSlot::W, 925.0f);
    W.SetSkillshot(0.25f, 120.0f, 1600.0f, false, SpellType::Circle);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 700.0f);
    E.SetSkillshot(0.25f, 45.0f * 0.5f, 2500.0f, false, SpellType::Cone);
    E.DamageType = DamageType::Magical;

    R = Spell(SpellSlot::R, 675.0f);
    R.SetTargetted(0.25f, 1100.0f);
    R.DamageType = DamageType::Magical;

    QE = Spell(SpellSlot::E, 1100.0f);
    QE.SetSkillshot(0.5f, 55.0f, 2500.0f, false, SpellType::Line);
    QE.DamageType = DamageType::Magical;

    BuildMenu();

    for (const auto& obj : GameObjects::AllGameObjects()) {
        OnCreateSeedObj(obj);
    }

    Drawing::OnDraw += &OnDraw;
    Events::hook.OnProcessSpell += &AIBaseClient_OnProcessSpellCast;
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    GameObjects::AddOnCreate(&OnCreateSeedObj);
    GameObjects::AddOnDelete(&OnRemoveSeedObj);
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Syndra loaded</font>");
}

// C#: sau khi cast Q ("SyndraQSpell") → sau 100ms E đẩy cầu về EndPos.
// MISSING API DelayAction: cast E ngay (gate bằng lastw).
static void AIBaseClient_OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (std::string(args.SpellName) == "SyndraQSpell") {
        const bool shouldQE = Orbwalker::ActiveMode() == OrbwalkingMode::Combo ||
                              KeyActive(ComboMenu, "qe");
        if (shouldQE && lastw < SDK::Variables::TickCount()) {
            const Vector3 endPos = args.EndPosition;
            SDK::Utils::DelayAction::Add(100 + Game::Ping() / 2, [endPos]() {
                const auto player = Player();
                if (!Loaded || !player.IsValid() || !E.IsReady()) {
                    return;
                }
                const Vector3 castPos = player.Position().Distance(endPos) <= E.Range
                    ? endPos
                    : player.Position().Extend(endPos, E.Range - 5.0f);
                E.Cast(castPos);
            });
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "drawq", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFFFFFFu);
    }
    if (Bool(DrawMenu, "drawqe", false) && E.IsReady() && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), QE.Range, 0xFFFFA500u);
    }
    if (Bool(DrawMenu, "draww", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFF98FB98u);
    }
    if (Bool(DrawMenu, "drawe", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFFFF0000u);
    }
    if (Bool(DrawMenu, "drawr", false) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFF9370DBu);
    }
}

static void OnCreateSeedObj(const GameObject& obj) {
    const auto base = AIBaseClient(obj.Handle());
    if (!base.IsValid()) {
        return;
    }
    const std::string name = ToLower(RuntimeObjectName(obj));
    if (name.find("seed") != std::string::npos && base.MaxHealth() <= 1.0f && base.IsAlly()) {
        const int id = base.NetworkId();
        if (std::any_of(SeedsInfo.begin(), SeedsInfo.end(),
            [id](const SeedInfo& seed) {
                return seed.Pointer.IsValid() && seed.Pointer.NetworkId() == id;
            })) {
            return;
        }
        SeedInfo info;
        info.Pointer = base;
        info.VaildTime = SDK::Variables::TickCount() + 6000;
        SeedsInfo.push_back(info);
    }
}

static void OnRemoveSeedObj(const GameObject& obj) {
    const int netId = obj.NetworkId();
    SeedsInfo.erase(
        std::remove_if(SeedsInfo.begin(), SeedsInfo.end(),
            [netId](const SeedInfo& s) {
                return s.Pointer.IsValid() && s.Pointer.NetworkId() == netId;
            }),
        SeedsInfo.end());
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    if (R.Instance().Level() == 3) {
        R.Range = 750.0f;
    }
    const auto ball = Objects();
    if (ball.IsValid()) {
        W.From = ball.Position();
    }

    Killsteal();
    QEEvent();
    AutoDashQ();

    if (KeyActive(HarassMenu, "qtoggle") && Orbwalker::ActiveMode() != OrbwalkingMode::Combo) {
        Harass();
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        if (!KeyActive(HarassMenu, "qtoggle")) {
            Harass();
        }
        break;
    case OrbwalkingMode::LaneClear:
        JungleClear();
        LaneClear();
        break;
    default:
        break;
    }
}

// C#: OnBeforeAttack — tắt AA khi combo (theo điều kiện level/cd/mana).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (Bool(AAMenu, "disable", false)) {
        if (player.Level() >= Slider(AAMenu, "level", 6)) {
            if (Bool(AAMenu, "enaifcd")) {
                if (!Q.IsReady() && !W.IsReady() && !E.IsReady()) {
                    args.Process = true;
                    return;
                }
            }
            if (Bool(AAMenu, "enaifmana")) {
                if (player.ManaPercent() <= 20.0f) {
                    args.Process = true;
                    return;
                }
            }
            args.Process = false;
            return;
        }
    }
}

static void AutoDashQ() {
    const auto player = Player();
    if (!player.IsValid() || player.IsRecalling()) {
        return;
    }
    if (player.ManaPercent() <= 15.0f) {
        return;
    }
    if (Q.IsReady() && Bool(HarassMenu, "dashing")) {
        for (const auto& obj : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(obj, Q.Range)) {
                Q.CastIfHitchanceEquals(AIBaseClient(obj.Handle()), HitChance::Dash);
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() < static_cast<float>(Slider(HarassMenu, "mana", 50))) {
        return;
    }

    if (Bool(HarassMenu, "usew") && W.IsReady()) {
        const auto target = TSGetTarget(W.Range, DamageType::Magical);
        if (ValidHeroTarget(target, W.Range) && target.Distance(player) < W.Range) {
            if (!player.HasBuff("syndrawtooltip")) {
                if (delayyyy <= SDK::Variables::TickCount()) {
                    const auto ball = Objects();
                    if (ball.IsValid()) {
                        if (W.Cast(ball.Position())) {
                            lastw = SDK::Variables::TickCount() + Game::Ping() + 20;
                            lastwe = SDK::Variables::TickCount() + Game::Ping() + 200;
                            delayyyy = 1000 + SDK::Variables::TickCount();
                            return;
                        }
                    }
                }
            }
            if (player.HasBuff("syndrawtooltip")) {
                if (!target.HasBuff("SyndraEDebuff")) {
                    if (lastqe < SDK::Variables::TickCount()) {
                        const auto predN = W.GetPrediction(target);
                        if (HitchanceAtLeast(predN.Hitchance, HitChance::High)) {
                            W.Cast(predN.GetCastPosition());
                        }
                    }
                }
            }
        }
    }

    if (Bool(HarassMenu, "useq") && Q.IsReady()) {
        const auto target = TSGetTarget(Q.Range, DamageType::Magical);
        if (ValidHeroTarget(target, Q.Range) && target.Distance(player) < Q.Range) {
            const auto predN = Q.GetPrediction(target);
            if (HitchanceAtLeast(predN.Hitchance, HitChance::High)) {
                Q.Cast(predN.GetCastPosition());
            }
        }
    }

    if (Bool(HarassMenu, "usee") && E.IsReady()) {
        if (E.IsReady() && lastwe <= SDK::Variables::TickCount()) {
            const auto target = TSGetTarget(QE.Range, DamageType::Magical);
            if (ValidHeroTarget(target, QE.Range) && target.Distance(player) < Q.Range && target.Distance(player) <= 1100.0f) {
                // target không đổi trong vòng lặp orb → tính prediction 1 lần (GetPrediction đắt).
                const auto enemyPred = QE.GetPrediction(target);
                for (const auto& orb : SeedsInfo) {
                    if (!orb.Pointer.IsValid() || orb.Pointer.Distance(player) >= 1000.0f) {
                        continue;
                    }
                    if (orb.Pointer.Distance(player) <= E.Range && player.Distance(orb.Pointer.Position()) >= 100.0f) {
                        const float test = player.Distance(enemyPred.GetCastPosition());
                        const Vector3 miau = player.Position().Extend(orb.Pointer.Position(), test);
                        if (miau.Distance(enemyPred.GetCastPosition()) < QE.Width + target.BoundingRadius() - 60.0f) {
                            E.Cast(orb.Pointer.Position());
                        }
                    }
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

    if (Bool(ComboMenu, "useq") && Q.IsReady()) {
        const auto target = TSGetTarget(Q.Range, DamageType::Magical);
        if (ValidHeroTarget(target)) {
            const auto predN = Q.GetPrediction(target);
            if (HitchanceAtLeast(predN.Hitchance, HitChance::High)) {
                Q.Cast(predN.GetCastPosition());
            }
        }
    }

    if (Bool(ComboMenu, "usew") && W.IsReady()) {
        const auto target = TSGetTarget(W.Range, DamageType::Magical);
        if (ValidHeroTarget(target, W.Range) && target.Distance(player) <= W.Range) {
            if (!player.HasBuff("syndrawtooltip")) {
                if (delayyyy <= SDK::Variables::TickCount()) {
                    const auto ball = Objects();
                    if (ball.IsValid()) {
                        if (W.Cast(ball.Position())) {
                            lastw = SDK::Variables::TickCount() + Game::Ping() + 20;
                            lastwe = SDK::Variables::TickCount() + Game::Ping() + 200;
                            delayyyy = 1000 + SDK::Variables::TickCount();
                            return;
                        }
                    }
                }
            }
            if (player.HasBuff("syndrawtooltip")) {
                if (!target.HasBuff("SyndraEDebuff")) {
                    const auto ball = Objects();
                    if (ball.IsValid()) {
                        if (lastqe < SDK::Variables::TickCount()) {
                            const auto predN = W.GetPrediction(target);
                            if (HitchanceAtLeast(predN.Hitchance, HitChance::High)) {
                                W.Cast(predN.GetCastPosition());
                            }
                        }
                    }
                }
            }
        }
    }

    if (E.IsReady() && Bool(ComboMenu, "usee")) {
        const auto target = TSGetTarget(QE.Range, DamageType::Magical);
        if (ValidHeroTarget(target)) {
            // target không đổi trong vòng lặp orb → tính prediction 1 lần (GetPrediction đắt).
            const auto enemyPred = QE.GetPrediction(target);
            for (const auto& orb : SeedsInfo) {
                if (!orb.Pointer.IsValid() || orb.Pointer.Distance(player) >= 1000.0f) {
                    continue;
                }
                if (orb.Pointer.Distance(player) <= E.Range && player.Distance(orb.Pointer.Position()) >= 100.0f &&
                    target.Distance(player) <= 1100.0f) {
                    const float test = player.Distance(enemyPred.GetCastPosition());
                    const Vector3 miau = player.Position().Extend(orb.Pointer.Position(), test);
                    if (miau.Distance(enemyPred.GetCastPosition()) < QE.Width + target.BoundingRadius() - 60.0f) {
                        E.Cast(orb.Pointer.Position());
                        lastqe = SDK::Variables::TickCount() + Game::Ping() + 100;
                    }
                }
            }
        }
    }

    if (E.IsReady() && Bool(ComboMenu, "useqe")) {
        const auto target = TSGetTarget(QE.Range, DamageType::Magical);
        if (ValidHeroTarget(target) &&
            target.Distance(player) < static_cast<float>(Slider(ComboMenu, "range", 1100))) {
            if (target.Distance(player) > E.Range) {
                QE.Delay = E.Delay + Q.Range / E.Speed;
                QE.From = player.Position().Extend(target.Position(), Q.Range);
                const auto pred = QE.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    Q.Cast(player.Position().Extend(pred.GetCastPosition(), Q.Range - 100.0f));
                }
            }
        }
    }

    if (R.IsReady() && Bool(EngageMenu, "user")) {
        const auto target = TSGetTarget(R.Range, DamageType::Magical);
        if (ValidHeroTarget(target)) {
            const auto tBase = AIBaseClient(target.Handle());
            if (target.Health() >= static_cast<float>(Slider(EngageMenu, "waster", 0))) {
                switch (ListIndex(EngageMenu, "rmode", 0)) {
                case 0:
                    if (target.Health() < GetR(tBase)) {
                        if (!IsBlackListR(target)) {
                            R.CastOnUnit(tBase);
                        }
                    }
                    break;
                case 1:
                    if (!IsBlackListR(target)) {
                        if (R.Instance().Ammo() >= Slider(EngageMenu, "orb", 5)) {
                            if (!Bool(EngageMenu, "kill")) {
                                R.CastOnUnit(tBase);
                            }
                            if (Bool(EngageMenu, "kill")) {
                                const float qDmg = player.GetSpellDamage(tBase, SpellSlot::Q);
                                const float wDmg = player.GetSpellDamage(tBase, SpellSlot::W);
                                const float eDmg = player.GetSpellDamage(tBase, SpellSlot::E);
                                const float rDmg = GetR(tBase);
                                if (target.Health() <= qDmg + wDmg + eDmg + rDmg) {
                                    R.CastOnUnit(tBase);
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
}

static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const bool useQ = Bool(JungleMenu, "useq");
    const bool useW = Bool(JungleMenu, "usew");

    std::vector<Vector2> junglePreds;
    for (const auto& m : GameObjects::Jungle()) {
        if (ValidTarget(m, W.Range)) {
            junglePreds.push_back(Q.GetPrediction(m).GetCastPosition().To2D());
        }
    }
    const auto ball = Objects();

    if (useW && ball.IsValid() && W.IsReady()) {
        const auto predW = SDK::Utils::Minion::GetBestCircularFarmLocation(junglePreds, 180.0f, W.Range);
        if (predW.MinionsHit >= 1) {
            if (!player.HasBuff("syndrawtooltip") && delayyyy < SDK::Variables::TickCount()) {
                W.Cast(ball.Position());
                delayyyy = SDK::Variables::TickCount() + 1000;
            }
            if (player.HasBuff("syndrawtooltip")) {
                W.Cast(Vector3::From2D(predW.Position));
            }
        }
    }
    if (useQ && Q.IsReady()) {
        std::vector<Vector2> junglesQ;
        for (const auto& m : GameObjects::Jungle()) {
            if (m.IsJungle() && ValidTarget(m, Q.Range)) {
                junglesQ.push_back(Q.GetPrediction(m).GetCastPosition().To2D());
            }
        }
        const auto predq = SDK::Utils::Minion::GetBestCircularFarmLocation(junglesQ, 120.0f, Q.Range);
        if (predq.MinionsHit >= 1) {
            Q.Cast(Vector3::From2D(predq.Position));
        }
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const bool useQ = Bool(LaneMenu, "useq");
    const bool useW = Bool(LaneMenu, "usew");
    const float manapercent = static_cast<float>(Slider(LaneMenu, "mana", 50));
    if (manapercent < player.ManaPercent()) {
        // Snapshot minion list once per frame: GameObjects::EnemyMinions() copies
        // the whole vector under a mutex on every call, so the original nested
        // loops paid one full copy per inner iteration (O(n^2) copies). The list
        // is a frozen cache snapshot, so reusing it is behavior-identical.
        const auto laneMinions = GameObjects::EnemyMinions();
        if (useQ && Q.IsReady()) {
            for (const auto& minion : laneMinions) {
                if (!ValidTarget(minion, Q.Range)) {
                    continue;
                }
                int count = 0;
                for (const auto& h : laneMinions) {
                    if (ValidTarget(h) && h.Distance(minion.Position()) <= 200.0f) {
                        ++count;
                    }
                }
                if (count >= Slider(LaneMenu, "hitq", 2)) {
                    Q.Cast(minion.Position());
                    break;
                }
            }
        }
        if (useW && W.IsReady()) {
            for (const auto& minion : laneMinions) {
                if (!ValidTarget(minion, W.Range)) {
                    continue;
                }
                int count = 0;
                for (const auto& h : laneMinions) {
                    if (ValidTarget(h) && h.Distance(minion.Position()) <= 225.0f) {
                        ++count;
                    }
                }
                if (count >= Slider(LaneMenu, "hitw", 3)) {
                    if (!player.HasBuff("syndrawtooltip") && delayyyy < SDK::Variables::TickCount()) {
                        const auto ball = Objects();
                        if (ball.IsValid()) {
                            W.CastOnUnit(ball);
                            delayyyy = SDK::Variables::TickCount() + 1000;
                        }
                    }
                    if (player.HasBuff("syndrawtooltip")) {
                        W.Cast(minion.Position());
                    }
                    break;
                }
            }
        }
    }
}

static void Killsteal() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Q.IsReady() && Bool(KSMenu, "ksq")) {
        const auto bestTarget = TSGetTarget(Q.Range, DamageType::Magical);
        if (bestTarget.IsValid() &&
            player.GetSpellDamage(AIBaseClient(bestTarget.Handle()), SpellSlot::Q) >= bestTarget.Health() &&
            ValidHeroTarget(bestTarget, Q.Range)) {
            const auto predN = Q.GetPrediction(bestTarget);
            if (HitchanceAtLeast(predN.Hitchance, HitChance::High)) {
                Q.Cast(predN.GetCastPosition());
            }
        }
    }
    if (W.IsReady() && Bool(KSMenu, "ksw")) {
        const auto bestTarget = TSGetTarget(W.Range, DamageType::Magical);
        if (bestTarget.IsValid() &&
            player.GetSpellDamage(AIBaseClient(bestTarget.Handle()), SpellSlot::W) >= bestTarget.Health() &&
            ValidHeroTarget(bestTarget, W.Range)) {
            if (!player.HasBuff("syndrawtooltip")) {
                if (delayyyy <= SDK::Variables::TickCount()) {
                    const auto ball = Objects();
                    if (ball.IsValid()) {
                        if (W.Cast(ball.Position())) {
                            lastw = SDK::Variables::TickCount() + Game::Ping() + 20;
                            lastwe = SDK::Variables::TickCount() + Game::Ping() + 200;
                            delayyyy = 1000 + SDK::Variables::TickCount();
                            return;
                        }
                    }
                }
            }
            if (player.HasBuff("syndrawtooltip")) {
                if (!bestTarget.HasBuff("SyndraEDebuff")) {
                    if (lastqe < SDK::Variables::TickCount()) {
                        const auto predN = W.GetPrediction(bestTarget);
                        if (HitchanceAtLeast(predN.Hitchance, HitChance::High)) {
                            W.Cast(predN.GetCastPosition());
                        }
                    }
                }
            }
        }
    }
    if (R.IsReady() && Bool(KSMenu, "ksr")) {
        const auto bestTarget = TSGetTarget(R.Range, DamageType::Magical);
        if (bestTarget.IsValid() &&
            GetR(AIBaseClient(bestTarget.Handle())) > bestTarget.Health() &&
            ValidHeroTarget(bestTarget, R.Range) &&
            bestTarget.Health() >= static_cast<float>(Slider(KSMenu, "waster", 0))) {
            if (!IsBlackListR(bestTarget)) {
                R.CastOnUnit(AIBaseClient(bestTarget.Handle()));
            }
        }
    }
}

static void QEEvent() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (KeyActive(ComboMenu, "qe") && E.IsReady() && Q.IsReady()) {
        const Vector3 pos = (Game::CursorPos() - player.Position()).Normalized();
        const auto target = TSGetTarget(QE.Range, DamageType::Magical);
        switch (ListIndex(ComboMenu, "qemode", 0)) {
        case 0: {
            if (!ValidHeroTarget(target)) {
                return;
            }
            if (target.Distance(player) > E.Range) {
                QE.Delay = E.Delay + Q.Range / E.Speed;
                QE.From = player.Position().Extend(target.Position(), Q.Range);
                const auto pred = QE.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    Q.Cast(player.Position().Extend(pred.GetCastPosition(), Q.Range - 100.0f));
                }
            }
            break;
        }
        case 1:
            if (player.Distance(Game::CursorPos()) < 800.0f) {
                Q.Cast(Game::CursorPos());
            }
            if (player.Distance(Game::CursorPos()) > 800.0f) {
                Q.Cast(player.Position() + pos * 800.0f);
            }
            break;
        case 2:
            if (player.CountEnemyHeroesInRange(static_cast<float>(Slider(ComboMenu, "range", 1100))) == 0) {
                if (player.Distance(Game::CursorPos()) < 800.0f) {
                    Q.Cast(Game::CursorPos());
                }
                if (player.Distance(Game::CursorPos()) > 800.0f) {
                    Q.Cast(player.Position() + pos * 800.0f);
                }
            }
            if (player.CountEnemyHeroesInRange(static_cast<float>(Slider(ComboMenu, "range", 1100))) > 0) {
                if (!ValidHeroTarget(target)) {
                    return;
                }
                if (target.Distance(player) > E.Range) {
                    QE.Delay = E.Delay + Q.Range / E.Speed;
                    QE.From = player.Position().Extend(target.Position(), Q.Range);
                    const auto pred = QE.GetPrediction(target);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        Q.Cast(player.Position().Extend(pred.GetCastPosition(), Q.Range - 100.0f));
                    }
                }
            }
            break;
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Drawing::OnDraw -= &OnDraw;
    Events::hook.OnProcessSpell -= &AIBaseClient_OnProcessSpellCast;
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    GameObjects::RemoveOnCreate(&OnCreateSeedObj);
    GameObjects::RemoveOnDelete(&OnRemoveSeedObj);
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;

    SeedsInfo.clear();
    Loaded = false;
}

} // namespace Plugins::SharpAIO::Syndra
