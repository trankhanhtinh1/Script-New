#pragma once

// ============================================================================
// SharpShooter AIO — Orianna
// Port từ CSharpFiles/Orianna/Orianna.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Sivir.h.
//
// Kỹ năng:
//   Q Command: Attack     — line 825 từ vị trí bóng, width 80, speed 1400.
//   W Command: Dissonance — circle 225 quanh bóng, no-target.
//   E Command: Protect    — targeted (bóng bay tới ally/self/enemy), line 1120.
//   R Command: Shockwave   — circle 400 quanh bóng, delay 0.5.
//
// Ghi chú port (giữ 1-1 với C#):
//   * OriannaBallManager: track vị trí bóng qua OnProcessSpell (OrianaIzunaCommand
//     = bóng bay tới điểm; OrianaRedactCommand = thu bóng về; buff orianaghost/
//     orianaghostself giữ bóng theo unit). Spell.From/RangeCheckFrom = ball pos.
//   * GetBestQLocation: dùng ConvexHull::GetMec (minimal enclosing circle) — 1-1.
//   * GetHits/GetEHits: đếm hero bóng/E trúng bằng Spell::WillHit.
//   * RLogic: killable (GetComboDamage) + hit-count/priority.
//   * SheildE: auto E khi bị đạn/AA nhắm (OnProcessSpell + missile check).
//   * MISSING API: MissileManager.MissileWillHitMyHero (đạn sắp trúng) → không có
//     trong SDK; SheildE chỉ dùng OnProcessSpell (địch cast/AA vào mình). Xem missapi.md.
//   * Damage tính tay theo wiki (patch V26.x) — dùng cho GetComboDamage killable R.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Orianna {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* JungleMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* InterruptListMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 825.0f };
inline Spell W{ SpellSlot::W, 225.0f };
inline Spell E{ SpellSlot::E, 1120.0f };
inline Spell R{ SpellSlot::R, 400.0f };

inline bool Loaded = false;

// OriannaBallManager: vị trí bóng.
inline Vector3 BallPosition{};
inline int BallTick = 0;

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

static int HeroPriority(const AIHeroClient& hero) {
    auto* prio = SDK::Modes::Priority::Instance();
    return prio ? prio->GetHeroPriority(hero) : 0;
}

static float HealthPred(const AIBaseClient& u, int ms) {
    return u.IsValid() ? Prediction::Health::GetPrediction(u, ms) : 0.0f;
}

// ── Damage tính tay theo wiki (leagueoflegends.com/Orianna, patch V26.x) ──
// Q: 60/90/120/150/180 + 55% AP. W: 70/110/150/190/230 + 80% AP.
// R: 225/350/475 + 110% AP.
static float QDamage(const AIBaseClient& t) {
    const auto player = Player();
    if (!player.IsValid() || !t.IsValid()) {
        return 0.0f;
    }
    const int r = Q.Instance().Level();
    if (r < 1) {
        return 0.0f;
    }
    static const float base[5] = { 60.0f, 90.0f, 120.0f, 150.0f, 180.0f };
    return player.CalculateMagicDamage(t, base[r - 1] + 0.55f * player.AP());
}

static float WDamage(const AIBaseClient& t) {
    const auto player = Player();
    if (!player.IsValid() || !t.IsValid()) {
        return 0.0f;
    }
    const int r = W.Instance().Level();
    if (r < 1) {
        return 0.0f;
    }
    static const float base[5] = { 70.0f, 110.0f, 150.0f, 190.0f, 230.0f };
    return player.CalculateMagicDamage(t, base[r - 1] + 0.80f * player.AP());
}

static float RDamage(const AIBaseClient& t) {
    const auto player = Player();
    if (!player.IsValid() || !t.IsValid()) {
        return 0.0f;
    }
    const int r = R.Instance().Level();
    if (r < 1) {
        return 0.0f;
    }
    const int idx = (r - 1 < 3) ? r - 1 : 2;
    static const float base[3] = { 225.0f, 350.0f, 475.0f };
    return player.CalculateMagicDamage(t, base[idx] + 1.10f * player.AP());
}

// C#: GetComboDamage = 2Q + W + R + 2 AA.
static float GetComboDamage(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const auto tBase = AIBaseClient(target.Handle());
    float result = 0.0f;
    if (Q.IsReady()) {
        result += 2.0f * QDamage(tBase);
    }
    if (W.IsReady()) {
        result += WDamage(tBase);
    }
    if (R.IsReady()) {
        result += RDamage(tBase);
    }
    result += 2.0f * Damage::GetAutoAttackDamage(player, tBase);
    return result;
}

// Forward declarations — đúng thứ tự file C#.
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnInterrupterSpell(const Events::ProcessSpellEventArgs& args);
static void OnAllGapcloser(const GapCloserEventArgs& args);
static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args);
static void OnDraw();
static void AutoWLogic();
static bool CanCastSpell(const Spell& spl, const AIHeroClient& obj);
static bool CastR(int minTargets, bool priority);
static void RLogic();
static int GetEHits(const Vector3& to, std::vector<AIHeroClient>* out);
static bool CastE(const AIHeroClient& target, int minTargets);
static int GetHits(Spell& spell, std::vector<AIHeroClient>* out);
static bool CastW(int minTargets);
static int GetBestQLocation(const AIHeroClient& mainTarget, Vector3* outPos);
static bool CastQ(const AIBaseClient& target);
static void JungleClear();
static void LaneClear();
static void Harass();
static void Combo();
static void BallManagerUpdate();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Orianna", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuBool("CW", "Use W"));
    ComboMenu->Add(new MenuBool("CE", "Use E"));

    RMenu = MenuRoot->AddSubMenu(new Menu("R Settings", "R Set"));
    RMenu->Add(new MenuList("UseR", "Use R",
        std::vector<std::string>{ "Only Combo", "Always", "Disable" }, 0));
    RMenu->Add(new MenuList("UseRKillable", "Use R Killable",
        std::vector<std::string>{ "Always", "Only 1v1", "Disable" }, 1));
    RMenu->Add(new MenuSlider("RHits", "R min HitCount", 3, 2, 5));
    RMenu->Add(new MenuSlider("UseRImportant", "Or target Priority >= X (6 = give up)", 5, 1, 6));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HQ", "Use Q"));
    HarassMenu->Add(new MenuBool("HW", "Use W", false));
    HarassMenu->Add(new MenuSlider("HMana", "Don't Harass if Mana <= X%", 40, 0, 100));
    HarassMenu->Add(new MenuKeyBind("AutoH", "Auto Harass", 'Y', KeyBindType::Toggle));

    FarmMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    FarmMenu->Add(new MenuBool("LQ", "Use Q"));
    FarmMenu->Add(new MenuBool("LW", "Use W"));
    FarmMenu->Add(new MenuBool("LE", "Use E", false));
    FarmMenu->Add(new MenuSlider("LaneClearManaCheck", "Don't Lane/Jung if Mana <= X%", 40, 0, 100));

    JungleMenu = MenuRoot->AddSubMenu(new Menu("JungleClear Settings", "JungleClear"));
    JungleMenu->Add(new MenuBool("UseQJFarm", "Use Q"));
    JungleMenu->Add(new MenuBool("UseWJFarm", "Use W"));
    JungleMenu->Add(new MenuBool("UseEJFarm", "Use E"));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("QRange", "Draw Q"));
    DrawMenu->Add(new MenuBool("WRange", "Draw W"));
    DrawMenu->Add(new MenuBool("ERange", "Draw E"));
    DrawMenu->Add(new MenuBool("RRange", "Draw R"));
    DrawMenu->Add(new MenuBool("QOnBallRange", "Draw Ball"));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("SheildE", "Auto E Protect Me"));
    MiscMenu->Add(new MenuSlider("AutoW", "Auto W if Can Hit >= X", 2, 1, 5));
    MiscMenu->Add(new MenuBool("AutoEInitiators", "Auto E if Enemy is Gap"));
    MiscMenu->Add(new MenuBool("InterruptSpells", "Use R Interrupt"));
    InterruptListMenu = MiscMenu->AddSubMenu(new Menu("InterruptList", "Interrupt List"));
    for (const auto& obj : GameObjects::EnemyHeroes()) {
        const std::string name = obj.CharacterName();
        if (name.empty()) {
            continue;
        }
        const std::string key = "rupt." + name;
        InterruptListMenu->Add(new MenuBool(key.c_str(), name.c_str()));
    }

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 825.0f);
    Q.SetSkillshot(0.0f, 80.0f, 1400.0f, false, SpellType::Line);
    Q.DamageType = DamageType::Magical;

    W = Spell(SpellSlot::W, 225.0f);
    W.SetSkillshot(0.0f, 250.0f, FLT_MAX, false, SpellType::Circle);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 1120.0f);
    E.SetSkillshot(0.0f, 80.0f, 1700.0f, false, SpellType::Line);

    R = Spell(SpellSlot::R, 400.0f);
    R.SetSkillshot(0.5f, 375.0f, FLT_MAX, false, SpellType::Circle);
    R.DamageType = DamageType::Magical;

    BallPosition = player.Position();
    BallTick = SDK::Variables::TickCount();

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpellCast;
    Events::hook.OnProcessSpell += &OnInterrupterSpell;
    Events::hook.OnGapCloser += &OnAllGapcloser;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Orianna loaded</font>");
}

// OriannaBallManager: theo dõi vị trí bóng.
static void BallManagerUpdate() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (SDK::Variables::TickCount() - BallTick > 300 && player.HasBuff("orianaghostself")) {
        BallPosition = player.Position();
    }
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ally.HasBuff("orianaghost")) {
            BallPosition = ally.Position();
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

    BallManagerUpdate();
    if (!BallPosition.IsValid() || BallPosition.IsZero()) {
        return;
    }

    // Spell From/RangeCheckFrom = vị trí bóng (C#).
    Q.From = BallPosition;
    Q.RangeCheckFrom = player.PreviousPosition();
    W.From = BallPosition;
    W.RangeCheckFrom = BallPosition;
    E.From = BallPosition;
    R.From = BallPosition;
    R.RangeCheckFrom = BallPosition;

    if (KeyActive(HarassMenu, "AutoH") && Orbwalker::ActiveMode() != OrbwalkingMode::Combo) {
        Harass();
    }

    RLogic();
    AutoWLogic();

    // SheildE: MISSING API MissileWillHitMyHero — chỉ dùng OnProcessSpell (xem file).

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        if (!KeyActive(HarassMenu, "AutoH")) {
            Harass();
        }
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        JungleClear();
        break;
    default:
        break;
    }
}

// C#: OnInterrupterSpell — địch cast important spell trong list → Q + R.
static void OnInterrupterSpell(const Events::ProcessSpellEventArgs& args) {
    if (!Bool(MiscMenu, "InterruptSpells")) {
        return;
    }
    if (Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender.Ptr);
    if (!sender.IsValid() || sender.IsAlly()) {
        return;
    }
    if (!Extensions::IsCastingInterruptableSpell(sender, false)) {
        return;
    }
    if (!InterruptListMenu) {
        return;
    }
    const std::string key = "rupt." + sender.CharacterName();
    const auto* item = InterruptListMenu->Get<MenuBool>(key.c_str());
    if (!item || !item->Value) {
        return;
    }
    if (R.IsReady()) {
        Q.Cast(AIBaseClient(sender.Handle()));
        if (BallPosition.DistanceSqr(sender.PreviousPosition()) < R.Range * R.Range) {
            R.Cast(Player().PreviousPosition());
        }
    }
}

static void OnAllGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "AutoEInitiators") || !E.IsReady()) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (!sender.IsValid()) {
        return;
    }
    // C#: chỉ E khi ally突进 (dash) vào giữa >0 địch quanh 1000.
    if (sender.IsAlly() && E.IsInRange(sender) && sender.CountEnemyHeroesInRange(1000.0f) > 0) {
        E.CastOnUnit(AIBaseClient(sender.Handle()));
    }
}

static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    // OriannaBallManager: bóng bay tới điểm khi cast Q (OrianaIzunaCommand);
    // thu bóng về khi cast E-redirect (OrianaRedactCommand).
    if (Events::IsLocalPlayer(args.Sender)) {
        if (std::string(args.SpellName) == "OrianaIzunaCommand") {
            BallPosition = args.EndPosition;
            BallTick = SDK::Variables::TickCount();
        } else if (std::string(args.SpellName) == "OrianaRedactCommand") {
            const auto target = ObjectManager::GetUnitByNetworkId<AIBaseClient>(
                static_cast<int>(args.TargetNetworkId));
            BallPosition = target.IsValid() ? target.Position() : player.Position();
            BallTick = SDK::Variables::TickCount();
        }
    }
    const auto sender = AIHeroClient(args.Sender.Ptr);
    if (sender.IsValid() && sender.IsEnemy() && Bool(MiscMenu, "SheildE") && E.IsReady()) {
        // Địch cast/AA vào mình → E che chắn.
        if (args.Target.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
            if (!Orbwalker::IsAutoAttack(args.SpellName) || sender.IsMelee()) {
                E.CastOnUnit(AIBaseClient(player.Handle()));
            }
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "QRange", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFF1E90FFu);
    }
    if (Bool(DrawMenu, "WRange", false) && W.IsReady()) {
        Drawing::DrawCircle(BallPosition, W.Range, 0xFFCD853Fu);
    }
    if (Bool(DrawMenu, "ERange", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFF00FF00u);
    }
    if (Bool(DrawMenu, "RRange", false) && R.IsReady()) {
        Drawing::DrawCircle(BallPosition, R.Range, 0xFFFF0000u);
    }
    if (Bool(DrawMenu, "QOnBallRange", false)) {
        Drawing::DrawCircle(BallPosition, 50.0f, 0xFFFFFF00u);
    }
}

static void AutoWLogic() {
    if (W.IsReady()) {
        CastW(Slider(MiscMenu, "AutoW", 2));
    }
}

static bool CanCastSpell(const Spell& spl, const AIHeroClient& obj) {
    if (HealthPred(AIBaseClient(obj.Handle()), static_cast<int>(spl.Delay * 1000.0f)) <= 0.0f ||
        (obj.HealthPercent() <= 10.0f && obj.CountAllyHeroesInRange(400.0f) - 1 >= 1)) {
        return false;
    }
    return true;
}

static bool CastR(int minTargets, bool priority) {
    std::vector<AIHeroClient> hits;
    const int count = GetHits(R, &hits);
    bool prioHit = false;
    if (priority) {
        for (const auto& hero : hits) {
            if (HeroPriority(hero) >= Slider(RMenu, "UseRImportant", 5)) {
                prioHit = true;
                break;
            }
        }
    }
    if (count >= minTargets || (priority && prioHit)) {
        R.Cast(Player().PreviousPosition());
        return true;
    }
    return false;
}

static void RLogic() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const int useR = ListIndex(RMenu, "UseR", 0);
    if (useR == 2 || !R.IsReady()) {
        return;
    }
    if ((useR == 0 && Orbwalker::ActiveMode() == OrbwalkingMode::Combo) || useR == 1) {
        const int enemiesInQR = player.CountEnemyHeroesInRange(Q.Range + R.Width);
        const int useRKillable = ListIndex(RMenu, "UseRKillable", 1);
        if (useRKillable != 2 && (useRKillable == 0 || (enemiesInQR <= 1 && useRKillable == 1))) {
            for (const auto& obj : GameObjects::EnemyHeroes()) {
                if (!ValidHeroTarget(obj) ||
                    BallPosition.Distance(obj.PreviousPosition()) > R.Range) {
                    continue;
                }
                const float healthPreds = HealthPred(AIBaseClient(obj.Handle()), 500);
                if (!CanCastSpell(R, obj)) {
                    continue;
                }
                if (GetComboDamage(obj) < healthPreds) {
                    continue;
                }
                const auto predmove = Prediction::GetPrediction(AIBaseClient(obj.Handle()), R.Delay);
                if (HitchanceAtLeast(predmove.Hitchance, HitChance::High) ||
                    predmove.GetCastPosition().IsValid()) {
                    if (predmove.GetCastPosition().DistanceSqr(BallPosition) <= R.Width * R.Width) {
                        R.Cast(obj.PreviousPosition());
                        break;
                    }
                }
            }
        }
        CastR(Slider(RMenu, "RHits", 3), true);
    }
}

// C#: GetEHits — đếm hero mà E (line từ bóng) trúng khi bay tới `to`.
static int GetEHits(const Vector3& to, std::vector<AIHeroClient>* out) {
    int count = 0;
    const float oldRange = E.Range;
    E.Range = 10000.0f;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, 2000.0f)) {
            continue;
        }
        if (E.WillHit(enemy, to)) {
            ++count;
            if (out) {
                out->push_back(enemy);
            }
        }
    }
    E.Range = oldRange;
    return count;
}

static bool CastE(const AIHeroClient& target, int minTargets) {
    if (!target.IsValid()) {
        return false;
    }
    if (GetEHits(target.PreviousPosition(), nullptr) >= minTargets) {
        E.CastOnUnit(AIBaseClient(target.Handle()));
        return true;
    }
    return false;
}

// C#: GetHits — đếm hero bóng (spell circle từ BallPosition) trúng.
// Giữ 1-1: C# lọc `BallPosition.Distance(h.ServerPosition) < range` với range = spell.Range*spell.Range.
static int GetHits(Spell& spell, std::vector<AIHeroClient>* out) {
    int count = 0;
    const float rangeSqr = spell.Range * spell.Range;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy)) {
            continue;
        }
        if (BallPosition.Distance(enemy.PreviousPosition()) >= rangeSqr) {
            continue;
        }
        if (!CanCastSpell(spell, enemy)) {
            continue;
        }
        if (spell.WillHit(enemy, BallPosition) &&
            BallPosition.DistanceSqr(enemy.PreviousPosition()) < spell.Width * spell.Width) {
            ++count;
            if (out) {
                out->push_back(enemy);
            }
        }
    }
    return count;
}

static bool CastW(int minTargets) {
    if (GetHits(W, nullptr) >= minTargets) {
        W.Cast(Player().PreviousPosition());
        return true;
    }
    return false;
}

// C#: GetBestQLocation — MEC (minimal enclosing circle) chọn vị trí Q AoE tốt nhất.
// Trả về Item1 (1=chỉ 1 target, 2=W cluster, 3=R cluster) + outPos.
static int GetBestQLocation(const AIHeroClient& mainTarget, Vector3* outPos) {
    std::vector<Vector2> points;
    const auto qPred = Q.GetPrediction(mainTarget);
    if (!HitchanceAtLeast(qPred.Hitchance, HitChance::VeryHigh)) {
        if (outPos) {
            *outPos = Vector3{};
        }
        return 1;
    }
    points.push_back(qPred.GetUnitPosition().To2D());

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, Q.Range + R.Range)) {
            continue;
        }
        const auto pred = Q.GetPrediction(enemy);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            points.push_back(pred.GetUnitPosition().To2D());
        }
    }

    for (int j = 0; j < 5; ++j) {
        const auto mec = SDK::ConvexHull::GetMec(points);
        if (mec.Radius < (R.Range - 75.0f) && static_cast<int>(points.size()) >= 3 && R.IsReady()) {
            if (outPos) {
                *outPos = Vector3::From2D(mec.Center);
            }
            return 3;
        }
        if (mec.Radius < (W.Range - 75.0f) && static_cast<int>(points.size()) >= 2 && W.IsReady()) {
            if (outPos) {
                *outPos = Vector3::From2D(mec.Center);
            }
            return 2;
        }
        if (points.size() == 1) {
            if (outPos) {
                *outPos = Vector3::From2D(mec.Center);
            }
            return 1;
        }
        if (mec.Radius < Q.Width && points.size() == 2) {
            if (outPos) {
                *outPos = Vector3::From2D(mec.Center);
            }
            return 2;
        }
        float maxdist = -1.0f;
        int maxdistindex = 1;
        for (int i = 1; i < static_cast<int>(points.size()); ++i) {
            const float distance = points[i].DistanceSqr(points[0]);
            if (distance > maxdist) {
                maxdistindex = i;
                maxdist = distance;
            }
        }
        points.erase(points.begin() + maxdistindex);
    }

    if (outPos) {
        *outPos = Vector3::From2D(points[0]);
    }
    return 1;
}

static bool CastQ(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return false;
    }
    const auto qPred = Q.GetPrediction(target);
    if (!HitchanceAtLeast(qPred.Hitchance, HitChance::High)) {
        return false;
    }
    if (qPred.GetCastPosition().Distance(player.PreviousPosition()) > Q.Range) {
        return false;
    }
    // Target đang chạy (không đối mặt mình) → Q hơi lố ra sau.
    return Q.Cast(qPred.GetCastPosition());
}

static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    std::vector<AIMinionClient> mobs;
    for (const auto& j : GameObjects::Jungle()) {
        if (ValidTarget(j, Q.Range)) {
            mobs.push_back(j);
        }
    }
    if (mobs.empty()) {
        return;
    }
    std::sort(mobs.begin(), mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) { return a.MaxHealth() < b.MaxHealth(); });
    const auto mob = mobs[0];
    const bool conditionUseW = Bool(JungleMenu, "UseWJFarm") && W.IsReady() &&
        W.WillHit(mob.PreviousPosition(), BallPosition);
    if (conditionUseW) {
        W.Cast(player.PreviousPosition());
    }
    if (Bool(JungleMenu, "UseQJFarm") && Q.IsReady()) {
        Q.Cast(mob.PreviousPosition());
    }
    if (Bool(JungleMenu, "UseEJFarm") && E.IsReady() && !conditionUseW) {
        AIHeroClient closestAlly;
        float bestDist = FLT_MAX;
        for (const auto& h : GameObjects::AllyHeroes()) {
            if (ValidHeroTarget(h, E.Range)) {
                const float d = h.Distance(mob);
                if (d < bestDist) {
                    bestDist = d;
                    closestAlly = h;
                }
            }
        }
        if (closestAlly.IsValid()) {
            E.CastOnUnit(AIBaseClient(closestAlly.Handle()));
        } else {
            E.CastOnUnit(AIBaseClient(player.Handle()));
        }
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() < static_cast<float>(Slider(FarmMenu, "LaneClearManaCheck", 40))) {
        return;
    }
    std::vector<AIBaseClient> allMinions;
    std::vector<AIBaseClient> rangedMinions;
    for (const auto& m : GameObjects::EnemyMinions()) {
        if (ValidTarget(m, Q.Range + W.Width)) {
            allMinions.push_back(AIBaseClient(m.Handle()));
            if ((m.GetMinionType() & MinionTypes::Ranged) == MinionTypes::Ranged) {
                rangedMinions.push_back(AIBaseClient(m.Handle()));
            }
        }
    }

    if (Bool(FarmMenu, "LQ") && Q.IsReady()) {
        if (Bool(FarmMenu, "LW")) {
            const auto qLoc = Q.GetCircularFarmLocation(allMinions, W.Range);
            const auto q2Loc = Q.GetCircularFarmLocation(rangedMinions, W.Range);
            const auto& best = (qLoc.MinionsHit > q2Loc.MinionsHit + 1) ? qLoc : q2Loc;
            if (best.MinionsHit > 0) {
                Q.Cast(Vector3::From2D(best.Position));
                return;
            }
        } else {
            for (const auto& minion : allMinions) {
                if (AutoAttack::InAutoAttackRange(minion)) {
                    continue;
                }
                const int ms = std::max(static_cast<int>(minion.PreviousPosition().Distance(BallPosition) / Q.Speed * 1000.0f) - 100, 0);
                if (HealthPred(minion, ms) < 50.0f) {
                    Q.Cast(minion.PreviousPosition());
                    return;
                }
            }
        }
    }

    if (Bool(FarmMenu, "LW") && W.IsReady()) {
        int n = 0;
        int d = 0;
        for (const auto& m : allMinions) {
            if (m.Distance(BallPosition) <= W.Range) {
                ++n;
                if (WDamage(m) > m.Health()) {
                    ++d;
                }
            }
        }
        if (n >= 3 || d >= 2) {
            W.Cast(player.PreviousPosition());
            return;
        }
    }

    if (Bool(FarmMenu, "LE", false) && E.IsReady()) {
        if (E.GetLineFarmLocation(allMinions).MinionsHit >= 3) {
            E.CastOnUnit(AIBaseClient(player.Handle()));
            return;
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() < static_cast<float>(Slider(HarassMenu, "HMana", 40))) {
        return;
    }
    const auto target = TSGetTarget(Q.Range, DamageType::Magical);
    if (target.IsValid()) {
        if (Bool(HarassMenu, "HQ") && Q.IsReady()) {
            CastQ(AIBaseClient(target.Handle()));
            return;
        }
        if (Bool(HarassMenu, "HW", false) && W.IsReady()) {
            CastW(1);
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const auto target = TSGetTarget(Q.Range, DamageType::Magical);
    if (!target.IsValid()) {
        return;
    }

    if (Bool(ComboMenu, "CW") && W.IsReady()) {
        CastW(1);
    }

    const int enemiesInQR = player.CountEnemyHeroesInRange(Q.Range + R.Width);
    if (enemiesInQR <= 1) {
        if (Bool(ComboMenu, "CE") && E.IsReady()) {
            for (const auto& ally : GameObjects::AllyHeroes()) {
                if (!ValidHeroTarget(ally, E.Range)) {
                    continue;
                }
                if (ally.CountEnemyHeroesInRange(300.0f) >= 1) {
                    E.CastOnUnit(AIBaseClient(ally.Handle()));
                }
                CastE(ally, 1);
            }
        }
        if (Bool(ComboMenu, "CQ") && Q.IsReady()) {
            CastQ(AIBaseClient(target.Handle()));
            return;
        }
    } else {
        if (Bool(ComboMenu, "CE") && E.IsReady()) {
            if (player.CountEnemyHeroesInRange(800.0f) <= 2) {
                CastE(player, 1);
            } else {
                CastE(player, 2);
            }
            for (const auto& ally : GameObjects::AllyHeroes()) {
                if (ValidHeroTarget(ally, E.Range) && ally.CountEnemyHeroesInRange(300.0f) >= 2) {
                    E.CastOnUnit(AIBaseClient(ally.Handle()));
                }
            }
        }
        if (!Q.IsReady() && !W.IsReady() && !R.IsReady() && E.IsReady() &&
            player.HealthPercent() < 15.0f && enemiesInQR > 0) {
            CastE(player, 0);
        }
        if (Bool(ComboMenu, "CQ") && Q.IsReady()) {
            Vector3 qLoc;
            const int item1 = GetBestQLocation(target, &qLoc);
            if (item1 > 1) {
                Q.Cast(qLoc);
                return;
            } else {
                CastQ(AIBaseClient(target.Handle()));
                return;
            }
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpellCast;
    Events::hook.OnProcessSpell -= &OnInterrupterSpell;
    Events::hook.OnGapCloser -= &OnAllGapcloser;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Orianna
