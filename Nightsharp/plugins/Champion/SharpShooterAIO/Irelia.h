#pragma once

// ============================================================================
// SharpShooter AIO — Irelia
// Port từ CSharpFiles/Irelia/Irelia.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Viktor.h.
//
// Kỹ năng:
//   Q Bladesurge   — targeted dash 600, reset khi giết/mark.
//   W Defiant Dance — charged line 800, block damage khi charge.
//   E Flawless Duet — 2-part: E1 đặt mốc (line 840), E2 nối 2 mốc để stun.
//   R Vanguard's Edge — line 900, tường kiếm.
//
// Ghi chú port (giữ 1-1 với C#, damage recheck theo wiki V25.x — xem comment):
//   * E1/E2 state: đọc spellbook E name ("IreliaE"=E1 chưa đặt / "IreliaE2"=đã đặt),
//     PosE = vị trí mốc E1 (track qua OnProcessSpell IreliaEMissile).
//     GetE2Prediction: spell ảo tầm 50000 từ PosE để tính nối mốc (UpdateSourcePosition).
//   * CheckItem: bảng item on-hit (Sheen/Trinity/Divine/Titanic/Wits/BladeKing/
//     RecurveBow/BlackCleaver) cho GetQDmg — item id từ SDK ItemData.
//   * RaySetDist: math thuần (2 nghiệm giao đường-tròn) — port 1-1.
//   * W-dodge (UnitDodge::EvadeTarget): DB spell nguy hiểm + block bằng W khi HP thấp.
//   * MISSING API (xem missapi.md):
//     - Geometry::Circle(...).Points quét wall/building + GrassObject cho E gap-close
//       → không có; giữ nhánh fallback (E tới fastGapMinion / player.ServerPosition).
//     - target.Buffs enumeration (EndTime) cho GetStunDuration/GetPassiveDuration
//       → xấp xỉ bằng HasBuffOfType (CC) / HasBuff("ireliamark").
//     - HaveSpellShield() → không có; xấp xỉ = false (không lọc shield khi Flee E).
//   * Damage wiki V25.x: Q 5/25/45/65/85 (+70% AD) [C# dùng 0.6 → sửa 0.7];
//     W min 10/20/30/40/50 (+40%AD+50%AP), max 30/60/90/120/150 (+120%AD+150%AP)
//     [C# min *15/0.4, max *45/1.2 → sửa theo wiki]; passive 10..61 theo level (+20% bonus AD).
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Irelia {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* FleeMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* OtherMenu = nullptr;
inline Menu* EvadeMenu = nullptr;
inline Menu* EvadeAAMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 600.0f };
inline Spell W{ SpellSlot::W, 800.0f };
inline Spell E{ SpellSlot::E, 840.0f };
inline Spell R{ SpellSlot::R, 900.0f };

inline bool Loaded = false;

// State (C#).
inline int ChargingW = 0;
inline int FirstE = 0;
inline Vector3 PosE{};
inline int EcastTime = 0;
inline bool CastRForE = true;
inline bool CastEForR = true;
inline int E1Delay = 0;

// Item state (C#).
inline bool RecurveBow = false;
inline bool BladeKing = false;
inline bool WitsEnd = false;
inline bool Titanic = false;
inline bool Divine = false;
inline float DivineTimer = 0.0f;
inline bool Sheen = false;
inline float SheenTimer = 0.0f;
inline bool Black = false;
inline bool Trinity = false;
inline float TrinityTimer = 0.0f;
inline int last_item_update = 0;

static AIHeroClient Player() {
    return ObjectManager::Player();
}

static std::string RuntimeObjectName(const GameObject& object) {
    char name[128] = {};
    if (object.IsValid() &&
        ::Core::Objects::ReadName(object.Address(), name, static_cast<int>(sizeof(name))) && name[0]) {
        return name;
    }
    return object.CharacterName();
}

static bool CastEPosition(const Vector3& position) {
    const auto player = Player();
    if (!player.IsValid() || !position.IsValid()) {
        return false;
    }
    const Vector3 castPos = player.Position().Distance(position) <= E.Range
        ? position
        : player.Position().Extend(position, E.Range - 5.0f);
    return E.Cast(castPos);
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

static std::vector<AIHeroClient> TSGetTargets(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTargets(range, damageType) : std::vector<AIHeroClient>();
}

static int HeroPriority(const AIHeroClient& hero) {
    auto* prio = SDK::Modes::Priority::Instance();
    return prio ? prio->GetHeroPriority(hero) : 0;
}

static std::string ESlotName() {
    const auto player = Player();
    return player.IsValid() ? player.Spellbook().GetSpell(SpellSlot::E).Name() : std::string();
}

// Forward declarations — đúng thứ tự file C#.
static void CastR();
static void JungleClearE();
static void QLogic_Minions(const AIBaseClient& targets);
static void QLogic(const GameUpdateEventArgs& args);
static void FleeLogic();
static void Harass();
static bool PointInE2Circle(const Vector3& point);
static void Combo();
static bool CheckTower(const AIBaseClient& target);
static PredictionOutput GetE2Prediction(const AIBaseClient& unit);
static Vector3 RaySetDist(const Vector3& start, const Vector3& path, const Vector3& center, float dist);
static float GetStunDuration(const AIBaseClient& target);
static float GetPassiveDuration(const AIBaseClient& target);
static void CheckItem();
static float GetWDmg(const AIBaseClient& unit, float time);
static float GetQDmg(const AIBaseClient& target);
static bool CanCastQ(const AIBaseClient& target);
static void OnBuffRemove(const BuffEventArgs& args);
static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args);
static void OnPlayAnimation(const Events::PlayAnimationEventArgs& args);
static void OnObjectCreate(const GameObject& obj);
static void OnObjectDelete(const GameObject& obj);
static void OnDraw();
static void OnInterrupterSpell(const Events::ProcessSpellEventArgs& args);
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void EvadeInit();
static void Evade_OnDoCastBack(const Events::ProcessSpellEventArgs& args);
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Irelia", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuBool("CQGap", "-> Use Minion Gap"));
    ComboMenu->Add(new MenuSlider("CQMinionsHealth", "-> Q Minion if My Health <= X%", 45, 0, 100));
    ComboMenu->Add(new MenuBool("ForceQ", "-> (1v1) if can Kill. ForceQ + AA Damage"));
    ComboMenu->Add(new MenuBool("CW", "Use W"));
    ComboMenu->Add(new MenuBool("CWQ", "Try W + Q Killable Minion"));
    ComboMenu->Add(new MenuSlider("CWTick", "W Charge Time", 100, 0, 1500));
    ComboMenu->Add(new MenuBool("CWOnlyMarks", "Only add Passive Count"));
    ComboMenu->Add(new MenuBool("CE", "Use E"));
    ComboMenu->Add(new MenuKeyBind("CR", "Use R", 'A', KeyBindType::Toggle));
    ComboMenu->Add(new MenuSlider("CRCheakHealth", "-> When Target HP <= X%", 25, 0, 100));
    ComboMenu->Add(new MenuBool("CROnlyKill", "Only Can Killable Cast"));
    ComboMenu->Add(new MenuBool("CRWaitQ", "-> if Q not Ready. Don't Cast R.", false));
    ComboMenu->Add(new MenuKeyBind("CRKEY", "Smart R Key", 'T', KeyBindType::Press));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HQ", "Use Q"));
    HarassMenu->Add(new MenuBool("HQAA", "-> Attack Hero Use AA (EAQ)"));
    HarassMenu->Add(new MenuBool("HQFarm", "Use Q Kill Minion"));
    HarassMenu->Add(new MenuSlider("HQFarmMagic", "Don't Kill minion if Mana <= X%", 30, 0, 100));
    HarassMenu->Add(new MenuBool("HE", "Use E"));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("LQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("LQMagic", "Don't Laneclear/JungleClear if Mana <= X%", 20, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("JungleClear Settings", "JungleClear"));
    JungleClearMenu->Add(new MenuBool("JQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("JE", "Use E"));

    FleeMenu = MenuRoot->AddSubMenu(new Menu("Flee Settings", "Flee"));
    FleeMenu->Add(new MenuBool("FQ", "Use Q"));
    FleeMenu->Add(new MenuBool("FE", "Use E"));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("DW", "Draw W"));
    DrawMenu->Add(new MenuBool("DE", "Draw E"));
    DrawMenu->Add(new MenuBool("DR", "Draw R"));

    OtherMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    OtherMenu->Add(new MenuBool("EInterrupter", "Use E Interrupt"));
    OtherMenu->Add(new MenuKeyBind("OQUnderTower", "UnderTower", 'A', KeyBindType::Toggle));
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 600.0f);
    W = Spell(SpellSlot::W, 800.0f);
    E = Spell(SpellSlot::E, 840.0f);
    R = Spell(SpellSlot::R, 900.0f);
    Q.SetTargetted(0.0f, 1400.0f + player.MoveSpeed());
    W.SetSkillshot(0.25f, 120.0f, 2300.0f, false, SpellType::Line);
    W.SetCharged("IreliaW", "ireliawdefense", 800, 800, 0.0f);
    E.SetSkillshot(0.0f, 70.0f, 2000.0f, false, SpellType::Line);
    R.SetSkillshot(0.4f, 160.0f, 2000.0f, true, SpellType::Line);

    SheenTimer = static_cast<float>(SDK::Variables::TickCount());
    DivineTimer = static_cast<float>(SDK::Variables::TickCount());
    TrinityTimer = static_cast<float>(SDK::Variables::TickCount());

    BuildMenu();
    EvadeInit();
    MenuRoot->Attach();

    Events::hook.OnPlayAnimation += &OnPlayAnimation;
    Events::hook.OnGameUpdate += &QLogic;
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnBuffRemove += &OnBuffRemove;
    Events::hook.OnProcessSpell += &OnProcessSpellCast;
    Events::hook.OnProcessSpell += &OnInterrupterSpell;
    Events::hook.OnProcessSpell += &Evade_OnDoCastBack;
    GameObjects::AddOnCreate(&OnObjectCreate);
    GameObjects::AddOnDelete(&OnObjectDelete);
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Irelia loaded</font>");
}

static void CastR() {
    const auto target = TSGetTarget(R.Range, DamageType::Physical);
    if (ValidHeroTarget(target, R.Range) && R.IsReady()) {
        const auto pos = R.GetPrediction(target);
        if (!pos.GetCastPosition().IsZero() &&
            pos.GetCastPosition().Distance(target.PreviousPosition()) < R.Range &&
            HitchanceAtLeast(pos.Hitchance, HitChance::VeryHigh)) {
            R.Cast(pos.GetCastPosition());
        }
    }
}

static void JungleClearE() {
    const auto player = Player();
    if (!Bool(JungleClearMenu, "JE") || player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "LQMagic", 20))) {
        return;
    }

    AIMinionClient jungle;
    for (const auto& j : GameObjects::Jungle()) {
        if (ValidTarget(j, E.Range) && static_cast<int>(j.GetJungleType()) >= static_cast<int>(JungleType::Large)) {
            jungle = j;
            break;
        }
    }
    if (!jungle.IsValid()) {
        return;
    }

    const auto jBase = AIBaseClient(jungle.Handle());
    if (E1Delay < SDK::Variables::TickCount() && ESlotName() == "IreliaE" && E.IsReady()) {
        const auto waypoints = jungle.GetWaypoints();
        if (!waypoints.empty()) {
            const Vector3 pathStartPos = waypoints.front();
            const Vector3 pathEndPos = waypoints.back();
            Vector3 pathNorm = (pathEndPos - pathStartPos).Normalized();
            const auto tempPred = Prediction::GetPrediction(jBase, 1.2f);

            if (jungle.GetWaypoints().empty() || !jungle.IsMoving()) {
                if (jungle.DistanceToPlayer() <= E.Range) {
                    const Vector3 castl = player.PreviousPosition() +
                        (jungle.PreviousPosition() - player.PreviousPosition()).Normalized() * 900.0f;
                    CastEPosition(castl);
                    E1Delay = SDK::Variables::TickCount() + 1500;
                }
            } else {
                const float distl = player.Distance(tempPred.GetCastPosition());
                if (distl <= E.Range) {
                    const float dist2 = player.Distance(jungle.PreviousPosition());
                    if (distl < dist2) {
                        pathNorm = pathNorm * -1.0f;
                    }
                    const Vector3 cast2 = RaySetDist(jungle.PreviousPosition(), pathNorm, player.PreviousPosition(), E.Range);
                    CastEPosition(cast2);
                    E1Delay = SDK::Variables::TickCount() + 1500;
                }
            }
        }
    }
    if (PosE.IsValid()) {
        if (!jungle.HasBuff("ireliamark") && player.HasBuff("IreliaE")) {
            const auto herosPos = GetE2Prediction(jBase);
            if (HitchanceAtLeast(herosPos.Hitchance, HitChance::High)) {
                const Vector3 EPOS2 = PosE + (herosPos.GetCastPosition() - PosE).Normalized() *
                    (herosPos.GetCastPosition().Distance(PosE) + 500.0f);
                CastEPosition(EPOS2);
            }
        }
    }
}

static void QLogic_Minions(const AIBaseClient& targets) {
    const auto player = Player();
    if (CanCastQ(targets) && player.ManaPercent() < Q.Instance().ManaCost() * 2.0f) {
        Q.CastOnUnit(targets);
        return;
    }
    const int passiveCount = player.GetBuffCount("ireliapassivestacks");
    if (player.HealthPercent() <= static_cast<float>(Slider(ComboMenu, "CQMinionsHealth", 45)) || passiveCount < 4) {
        AIMinionClient repard;
        for (const auto& x : GameObjects::EnemyMinions()) {
            if (ValidTarget(x, Q.Range) && x.Health() < GetQDmg(AIBaseClient(x.Handle())) &&
                x.Distance(targets) < AutoAttack::GetRealAutoAttackRange(player, targets) + 15.0f) {
                repard = x;
                break;
            }
        }
        if (repard.IsValid()) {
            Q.CastOnUnit(AIBaseClient(repard.Handle()));
        }
    }
}

static void QLogic(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    if (Bool(ComboMenu, "CQ") && Orbwalker::ActiveMode() == OrbwalkingMode::Combo && Q.IsReady()) {
        const int passiveCount = player.GetBuffCount("ireliapassivestacks");
        std::vector<AIHeroClient> canQ;
        for (const auto& x : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(x, Q.Range) && CanCastQ(AIBaseClient(x.Handle()))) {
                canQ.push_back(x);
            }
        }
        AIHeroClient targets;
        if (!canQ.empty()) {
            int bestPrio = -1;
            for (const auto& x : canQ) {
                const int prio = HeroPriority(x);
                if (prio > bestPrio) {
                    bestPrio = prio;
                    targets = x;
                }
            }
        } else {
            targets = TSGetTarget(1000.0f, DamageType::Physical);
        }
        if (targets.IsValid()) {
            const auto tBase = AIBaseClient(targets.Handle());
            if (ValidHeroTarget(targets, Q.Range) && CanCastQ(tBase) && CheckTower(tBase)) {
                if (AutoAttack::InAutoAttackRange(targets) &&
                    player.CountEnemyHeroesInRange(600.0f) <= 1 &&
                    player.HealthPercent() > static_cast<float>(Slider(ComboMenu, "CQMinionsHealth", 45))) {
                    if (GetStunDuration(tBase) <= 0.0f || GetPassiveDuration(tBase) <= 200.0f) {
                        Q.CastOnUnit(tBase);
                        return;
                    }
                } else {
                    Q.CastOnUnit(tBase);
                    return;
                }
            } else if (Bool(ComboMenu, "CQGap") &&
                       (!ValidHeroTarget(targets, Q.Range) || (!CanCastQ(tBase) && !AutoAttack::InAutoAttackRange(targets)))) {
                std::vector<AIMinionClient> gapMinions;
                for (const auto& m : GameObjects::EnemyMinions()) {
                    if (ValidTarget(m, Q.Range) && m.Health() < GetQDmg(AIBaseClient(m.Handle())) &&
                        m.Distance(targets) < targets.DistanceToPlayer() && CheckTower(AIBaseClient(m.Handle()))) {
                        gapMinions.push_back(m);
                    }
                }
                if (!gapMinions.empty()) {
                    bool isCastQ = false;
                    if ((passiveCount >= 3 || (passiveCount >= 2 && CanCastQ(tBase))) &&
                        player.HealthPercent() > static_cast<float>(Slider(ComboMenu, "CQMinionsHealth", 45))) {
                        AIMinionClient best;
                        float bestDist = FLT_MAX;
                        for (const auto& m : gapMinions) {
                            const float d = m.Distance(targets);
                            if (d < bestDist) { bestDist = d; best = m; }
                        }
                        if (best.IsValid()) { Q.Cast(best.PreviousPosition()); isCastQ = true; }
                    } else {
                        AIMinionClient best;
                        float bestDist = FLT_MAX;
                        for (const auto& m : gapMinions) {
                            const float d = m.DistanceToPlayer();
                            if (d < bestDist) { bestDist = d; best = m; }
                        }
                        if (best.IsValid()) { Q.Cast(best.PreviousPosition()); isCastQ = true; }
                    }
                    if (player.Mana() >= Q.Instance().ManaCost() * 2.0f + E.Instance().ManaCost()) {
                        if (isCastQ && Bool(ComboMenu, "CE") && E.IsReady() && !PosE.IsValid() &&
                            E1Delay < SDK::Variables::TickCount()) {
                            // MISSING API: GrassObject + Geometry::Circle wall/building scan — xem missapi.md.
                            // Giữ fallback C#: E tới fastGapMinion (>300 dist, gần target nhất), else player.
                            AIMinionClient fastGap;
                            float fastDist = FLT_MAX;
                            for (const auto& x : gapMinions) {
                                if (x.DistanceToPlayer() > 300.0f) {
                                    const float d = x.Distance(targets);
                                    if (d < fastDist) { fastDist = d; fastGap = x; }
                                }
                            }
                            if (fastGap.IsValid()) {
                                if (static_cast<int>(CastEPosition(fastGap.PreviousPosition())) != 0) {
                                    E1Delay = SDK::Variables::TickCount() + 1500;
                                    return;
                                }
                            }
                            if (static_cast<int>(CastEPosition(player.PreviousPosition())) != 0) {
                                E1Delay = SDK::Variables::TickCount() + 1500;
                                return;
                            }
                        }
                    }
                }
            }
            QLogic_Minions(tBase);
        }
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear && Q.IsReady() &&
        player.ManaPercent() > static_cast<float>(Slider(LaneClearMenu, "LQMagic", 20))) {
        if (Bool(LaneClearMenu, "LQ")) {
            for (const auto& x : GameObjects::EnemyMinions()) {
                if (ValidTarget(x, Q.Range) && x.Health() < GetQDmg(AIBaseClient(x.Handle())) &&
                    CheckTower(AIBaseClient(x.Handle()))) {
                    Q.CastOnUnit(AIBaseClient(x.Handle()));
                    break;
                }
            }
        }
        if (Bool(JungleClearMenu, "JQ")) {
            for (const auto& x : GameObjects::Jungle()) {
                if (ValidTarget(x, Q.Range) && CanCastQ(AIBaseClient(x.Handle())) && CheckTower(AIBaseClient(x.Handle()))) {
                    Q.CastOnUnit(AIBaseClient(x.Handle()));
                    break;
                }
            }
        }
    }
}

static void FleeLogic() {
    const auto player = Player();
    AIHeroClient target;
    float bestDist = FLT_MAX;
    for (const auto& x : GameObjects::EnemyHeroes()) {
        // MISSING API: HaveSpellShield() — xấp xỉ = false (không lọc shield).
        if (ValidHeroTarget(x, E.Range)) {
            const float d = x.DistanceToPlayer();
            if (d < bestDist) { bestDist = d; target = x; }
        }
    }
    if (Bool(FleeMenu, "FQ") && Q.IsReady()) {
        AIMinionClient minion;
        float md = FLT_MAX;
        for (const auto& x : GameObjects::EnemyMinions()) {
            if (ValidTarget(x, Q.Range) && x.Health() < GetQDmg(AIBaseClient(x.Handle())) &&
                x.DistanceToPlayer() < ObjectManager::Player().Position().Distance(Game::CursorPos())) {
                const float d = x.Distance(Game::CursorPos());
                if (d < md) { md = d; minion = x; }
            }
        }
        if (minion.IsValid()) {
            Q.Cast(minion.PreviousPosition());
        }
        AIMinionClient jungle;
        float jd = FLT_MAX;
        for (const auto& x : GameObjects::Jungle()) {
            if (ValidTarget(x, Q.Range) && x.Health() < GetQDmg(AIBaseClient(x.Handle())) &&
                x.DistanceToPlayer() < ObjectManager::Player().Position().Distance(Game::CursorPos())) {
                const float d = x.Distance(Game::CursorPos());
                if (d < jd) { jd = d; jungle = x; }
            }
        }
        if (jungle.IsValid()) {
            Q.Cast(jungle.PreviousPosition());
        }
    }
    if (Bool(FleeMenu, "FE") && target.IsValid()) {
        AIHeroClient enemy;
        float ed = FLT_MAX;
        for (const auto& x : TSGetTargets(E.Range, DamageType::Physical)) {
            const float d = x.DistanceToPlayer();
            if (d < ed) { ed = d; enemy = x; }
        }
        if (ValidHeroTarget(enemy, E.Range)) {
            const auto eBase = AIBaseClient(enemy.Handle());
            if (E1Delay < SDK::Variables::TickCount() && ESlotName() == "IreliaE" && E.IsReady()) {
                const auto waypoints = enemy.GetWaypoints();
                if (!waypoints.empty()) {
                    const Vector3 pathStartPos = waypoints.front();
                    const Vector3 pathEndPos = waypoints.back();
                    Vector3 pathNorm = (pathEndPos - pathStartPos).Normalized();
                    const auto tempPred = Prediction::GetPrediction(eBase, 0.25f);
                    if (enemy.GetWaypoints().empty() || !enemy.IsMoving()) {
                        if (enemy.DistanceToPlayer() <= E.Range) {
                            const Vector3 castl = player.PreviousPosition() +
                                (enemy.PreviousPosition() - player.PreviousPosition()).Normalized() * 900.0f;
                            CastEPosition(castl);
                            E1Delay = SDK::Variables::TickCount() + 1500;
                        }
                    } else {
                        const float distl = player.Distance(tempPred.GetCastPosition());
                        if (distl <= E.Range) {
                            const float dist2 = player.Distance(enemy.PreviousPosition());
                            if (distl < dist2) { pathNorm = pathNorm * -1.0f; }
                            const Vector3 cast2 = RaySetDist(enemy.PreviousPosition(), pathNorm, player.PreviousPosition(), E.Range);
                            CastEPosition(cast2);
                            E1Delay = SDK::Variables::TickCount() + 1500;
                        }
                    }
                }
            }
            if (PosE.IsValid()) {
                if (!enemy.HasBuff("ireliamark") && player.HasBuff("IreliaE")) {
                    const auto herosPos = GetE2Prediction(eBase);
                    if (HitchanceAtLeast(herosPos.Hitchance, HitChance::High)) {
                        const float offset = PosE.Distance(herosPos.GetCastPosition());
                        const float heOffset = E.Range - player.Distance(herosPos.GetCastPosition());
                        const Vector3 extendd = PosE.Extend(herosPos.GetCastPosition(), offset + heOffset);
                        if (PointInE2Circle(extendd)) {
                            CastEPosition(extendd);
                        } else {
                            CastEPosition(herosPos.GetCastPosition());
                        }
                    }
                }
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (Bool(HarassMenu, "HQ") && !player.Spellbook().IsWindingUp()) {
        if (Bool(HarassMenu, "HQFarm")) {
            if (static_cast<float>(Slider(HarassMenu, "HQFarmMagic", 30)) < player.ManaPercent()) {
                AIMinionClient minion;
                float md = FLT_MAX;
                for (const auto& x : GameObjects::EnemyMinions()) {
                    if (ValidTarget(x, Q.Range) && x.Health() < GetQDmg(AIBaseClient(x.Handle())) && !x.IsUnderEnemyTurret()) {
                        const float d = x.DistanceToPlayer();
                        if (d < md) { md = d; minion = x; }
                    }
                }
                if (minion.IsValid()) {
                    Q.CastOnUnit(AIBaseClient(minion.Handle()));
                }
            }
        }
        if (Bool(HarassMenu, "HQAA")) {
            for (const auto& x : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(x, Q.Range) && CanCastQ(AIBaseClient(x.Handle())) && !x.IsUnderEnemyTurret()) {
                    Q.CastOnUnit(AIBaseClient(x.Handle()));
                    break;
                }
            }
        }
    }
    if (Bool(HarassMenu, "HE")) {
        AIHeroClient enemy;
        float ed = FLT_MAX;
        for (const auto& x : TSGetTargets(E.Range, DamageType::Physical)) {
            const float d = x.DistanceToPlayer();
            if (d < ed) { ed = d; enemy = x; }
        }
        if (ValidHeroTarget(enemy, E.Range)) {
            const auto eBase = AIBaseClient(enemy.Handle());
            if (E1Delay < SDK::Variables::TickCount() && ESlotName() == "IreliaE" && E.IsReady()) {
                const auto waypoints = enemy.GetWaypoints();
                if (!waypoints.empty()) {
                    const Vector3 pathStartPos = waypoints.front();
                    const Vector3 pathEndPos = waypoints.back();
                    Vector3 pathNorm = (pathEndPos - pathStartPos).Normalized();
                    const auto tempPred = Prediction::GetPrediction(eBase, 0.25f);
                    if (enemy.GetWaypoints().empty() || !enemy.IsMoving()) {
                        if (enemy.DistanceToPlayer() <= E.Range) {
                            const Vector3 castl = player.PreviousPosition() +
                                (enemy.PreviousPosition() - player.PreviousPosition()).Normalized() * 900.0f;
                            CastEPosition(castl);
                            E1Delay = SDK::Variables::TickCount() + 1500;
                        }
                    } else {
                        const float distl = player.Distance(tempPred.GetCastPosition());
                        if (distl <= E.Range) {
                            const float dist2 = player.Distance(enemy.PreviousPosition());
                            if (distl < dist2) { pathNorm = pathNorm * -1.0f; }
                            const Vector3 cast2 = RaySetDist(enemy.PreviousPosition(), pathNorm, player.PreviousPosition(), E.Range);
                            CastEPosition(cast2);
                            E1Delay = SDK::Variables::TickCount() + 1500;
                        }
                    }
                }
            }
            if (PosE.IsValid()) {
                if (!enemy.HasBuff("ireliamark") && player.HasBuff("IreliaE")) {
                    const auto herosPos = GetE2Prediction(eBase);
                    if (HitchanceAtLeast(herosPos.Hitchance, HitChance::High)) {
                        const float offset = PosE.Distance(herosPos.GetCastPosition());
                        const float heOffset = E.Range - player.Distance(herosPos.GetCastPosition());
                        const Vector3 extendd = PosE.Extend(herosPos.GetCastPosition(), offset + heOffset);
                        if (PointInE2Circle(extendd)) {
                            CastEPosition(extendd);
                        } else {
                            CastEPosition(herosPos.GetCastPosition());
                        }
                    }
                }
            }
        }
    }
}

static bool PointInE2Circle(const Vector3& point) {
    // Geometry.Circle(Player, E.Range).IsInside(point) → distance check thuần.
    return Player().PreviousPosition().Distance(point) <= E.Range;
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(ComboMenu, "CE") && E.IsReady()) {
        const auto eenemy = TSGetTarget(E.Range, DamageType::Physical);
        if (eenemy.IsValid()) {
            if (E1Delay < SDK::Variables::TickCount() && ESlotName() == "IreliaE" && !PosE.IsValid()) {
                const auto way = eenemy.GetWaypoints();
                if (!way.empty()) {
                    const Vector3 pathStartPos = way.front();
                    const Vector3 pathEndPos = way.back();
                    Vector3 pathNorm = (pathEndPos - pathStartPos).Normalized();
                    const Vector3 tempPred = E.GetPrediction(eenemy).GetCastPosition();
                    if (eenemy.GetWaypoints().empty() || !eenemy.IsMoving()) {
                        if (eenemy.DistanceToPlayer() <= E.Range) {
                            const Vector3 castl = player.PreviousPosition().Extend(eenemy.PreviousPosition(), -E.Range);
                            if (static_cast<int>(CastEPosition(castl)) != 0) {
                                E1Delay = SDK::Variables::TickCount() + 1500;
                                return;
                            }
                        }
                    } else {
                        const float distl = player.Distance(tempPred);
                        if (distl <= E.Range) {
                            const float dist2 = player.Distance(eenemy.PreviousPosition());
                            if (distl < dist2) { pathNorm = pathNorm * -1.0f; }
                            const Vector3 cast2 = RaySetDist(eenemy.PreviousPosition(), pathNorm, player.PreviousPosition(), E.Range);
                            if (static_cast<int>(CastEPosition(cast2)) != 0) {
                                E1Delay = SDK::Variables::TickCount() + 1500;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    if (KeyActive(ComboMenu, "CR") && R.IsReady() && CastEForR) {
        const auto target = TSGetTarget(R.Range, DamageType::Physical);
        if (target.IsValid() && !target.HasBuff("ireliamark")) {
            if (!Bool(ComboMenu, "CRWaitQ", false) || Q.IsReady()) {
                const auto tBase = AIBaseClient(target.Handle());
                const float health = Prediction::Health::GetPrediction(tBase,
                    static_cast<int>(target.DistanceToPlayer() / R.Speed * 1000.0f));
                const bool cond = Bool(ComboMenu, "CROnlyKill")
                    ? (GetQDmg(tBase) + R.GetDamage(tBase) * 2.0f + E.GetDamage(tBase) >= health)
                    : (target.HealthPercent() <= static_cast<float>(Slider(ComboMenu, "CRCheakHealth", 25)) && GetQDmg(tBase) < health);
                if (cond) {
                    const auto pos = R.GetPrediction(target);
                    if (!pos.GetCastPosition().IsZero() &&
                        pos.GetCastPosition().Distance(target.PreviousPosition()) < R.Range &&
                        HitchanceAtLeast(pos.Hitchance, HitChance::VeryHigh) && pos.CollisionObjects.empty()) {
                        R.Cast(pos.GetCastPosition());
                    }
                }
            }
        }
    }
    if (Bool(ComboMenu, "CW")) {
        const auto wtarget = TSGetTarget(800.0f, DamageType::Physical);
        if (wtarget.IsValid()) {
            if (W.IsReady() && !W.IsCharging() &&
                (!Bool(ComboMenu, "CWOnlyMarks") || !player.HasBuff("ireliapassivestacksmax"))) {
                if (!Q.IsReady() || (Q.IsReady() && wtarget.DistanceToPlayer() < (300.0f + player.BoundingRadius()))) {
                    W.StartCharging();
                }
            }
            if (W.IsCharging()) {
                const auto posnext = Prediction::GetPrediction(AIBaseClient(wtarget.Handle()), 0.25f);
                const float chargingWTime = static_cast<float>(SDK::Variables::TickCount() - ChargingW);
                if (chargingWTime >= static_cast<float>(Slider(ComboMenu, "CWTick", 100)) ||
                    (wtarget.DistanceToPlayer() <= 800.0f && !(ObjectManager::Player().Position().Distance(posnext.GetUnitPosition()) <= 800.0f)) ||
                    (PosE.IsValid() && SDK::Variables::TickCount() - EcastTime > 2800)) {
                    if (Bool(ComboMenu, "CWQ") && Bool(ComboMenu, "CQ") && Q.IsReady()) {
                        AIMinionClient canQW;
                        for (const auto& x : GameObjects::EnemyMinions()) {
                            if (x.DistanceToPlayer() > 800.0f || !ValidTarget(x)) { continue; }
                            const auto xBase = AIBaseClient(x.Handle());
                            if (x.Distance(wtarget) < wtarget.DistanceToPlayer() &&
                                x.Health() < (GetWDmg(xBase, chargingWTime / 1000.0f) + GetQDmg(xBase)) &&
                                x.Health() > GetWDmg(xBase, chargingWTime / 1000.0f)) {
                                canQW = x;
                                break;
                            }
                        }
                        const auto wBase = AIBaseClient(wtarget.Handle());
                        if (wtarget.Health() > GetWDmg(wBase, chargingWTime / 1000.0f) + Damage::GetAutoAttackDamage(player, wBase) ||
                            (wtarget.Health() > player.Health() && player.HealthPercent() <= static_cast<float>(Slider(ComboMenu, "CQMinionsHealth", 45)))) {
                            if (canQW.IsValid()) {
                                W.ShootChargedSpell(canQW.PreviousPosition());
                                return;
                            }
                        }
                    }
                    W.ShootChargedSpell(posnext.GetCastPosition());
                }
            }
        }
    }
    if (Bool(ComboMenu, "CE") && E.IsReady()) {
        const auto eenemy = TSGetTarget(E.Range, DamageType::Physical);
        if (eenemy.IsValid()) {
            if (CastRForE && player.HasBuff("IreliaE") && PosE.IsValid()) {
                bool nonhavebuff = false;
                for (const auto& x : GameObjects::EnemyHeroes()) {
                    if (ValidHeroTarget(x, E.Range) && x.HasBuff("ireliamark") && (Q.IsReady() || player.IsDashing())) {
                        nonhavebuff = true;
                        break;
                    }
                }
                if (!nonhavebuff) {
                    const auto targets = TSGetTargets(E.Range, DamageType::Physical);
                    if (!targets.empty()) {
                        PredictionOutput best;
                        int bestHit = -1;
                        bool found = false;
                        for (const auto& i : targets) {
                            const auto pred = GetE2Prediction(AIBaseClient(i.Handle()));
                            if (HitchanceAtLeast(pred.Hitchance, HitChance::High) &&
                                ObjectManager::Player().Position().Distance(pred.GetCastPosition()) <= E.Range) {
                                if (pred.GetAoeTargetsHitCount() > bestHit) {
                                    bestHit = pred.GetAoeTargetsHitCount();
                                    best = pred;
                                    found = true;
                                }
                            }
                        }
                        if (found) {
                            const Vector3 castPos = best.GetCastPosition();
                            const float offset = PosE.Distance(castPos);
                            const float heOffset = E.Range - player.Distance(castPos);
                            const Vector3 extendd = PosE.Extend(castPos, offset + heOffset);
                            if (PointInE2Circle(extendd)) {
                                CastEPosition(extendd);
                            } else {
                                CastEPosition(castPos);
                            }
                        }
                    }
                }
            }
        }
    }
}

static bool CheckTower(const AIBaseClient& target) {
    if (!target.IsUnderEnemyTurret() || KeyActive(OtherMenu, "OQUnderTower")) {
        return true;
    }
    return false;
}

static PredictionOutput GetE2Prediction(const AIBaseClient& unit) {
    Spell secondE(SpellSlot::E, 50000.0f);
    secondE.SetSkillshot(0.72f, 70.0f, FLT_MAX, false, SpellType::Line);
    secondE.UpdateSourcePosition(PosE, PosE);
    return secondE.GetPrediction(unit);
}

static Vector3 RaySetDist(const Vector3& start, const Vector3& path, const Vector3& center, float dist) {
    const auto player = Player();
    const double a = start.x - center.x;
    const double b = start.y - center.y;
    const double c = start.z - center.z;
    const double x = path.x;
    const double y = path.y;
    const double z = path.z;
    const double nl = a * x + b * y + c * z;
    const double n2 =
        std::pow(z, 2) * std::pow(dist, 2) - std::pow(a, 2) * std::pow(z, 2) - std::pow(b, 2) * std::pow(z, 2) +
        2 * a * c * x * z + 2 * b * c * y * z + 2 * a * b * x * y +
        std::pow(dist, 2) * std::pow(x, 2) +
        std::pow(dist, 2) * std::pow(y, 2) -
        std::pow(a, 2) * std::pow(y, 2) -
        std::pow(b, 2) * std::pow(x, 2) -
        std::pow(c, 2) * std::pow(x, 2) -
        std::pow(c, 2) * std::pow(y, 2);
    const double n3 = std::pow(x, 2) + std::pow(y, 2) + std::pow(z, 2);
    const double r1 = -(nl + std::sqrt(n2)) / n3;
    const double r2 = -(nl - std::sqrt(n2)) / n3;
    const double r = std::max(r1, r2);
    Vector3 retPos;
    retPos.x = start.x + static_cast<float>(r) * path.x;
    retPos.y = start.y + static_cast<float>(r) * path.y;
    retPos.z = start.z + static_cast<float>(r) * path.z;
    if (retPos.Distance(start) <= 350.0f) {
        retPos = player.PreviousPosition().Extend(retPos, -E.Range);
    }
    return retPos;
}

// MISSING API: target.Buffs enumeration (EndTime) — xấp xỉ bằng HasBuffOfType.
// C#: max EndTime của buff CC (Charm/Knockback/Stun/Suppression/Snare) - Game.Time, đổi ms.
static float GetStunDuration(const AIBaseClient& target) {
    if (!target.IsValid() || target.IsDead()) {
        return 0.0f;
    }
    if (SDK::HasBuffOfType(target, SDK::BuffType::Charm) ||
        SDK::HasBuffOfType(target, SDK::BuffType::Knockback) ||
        SDK::HasBuffOfType(target, SDK::BuffType::Stun) ||
        SDK::HasBuffOfType(target, SDK::BuffType::Suppression) ||
        SDK::HasBuffOfType(target, SDK::BuffType::Snare)) {
        return 1000.0f;
    }
    return 0.0f;
}

// MISSING API: buff EndTime — xấp xỉ: có mark → 1000ms, else 0.
static float GetPassiveDuration(const AIBaseClient& target) {
    if (!target.IsValid() || target.IsDead()) {
        return 0.0f;
    }
    return target.HasBuff("ireliamark") ? 1000.0f : 0.0f;
}

static void CheckItem() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (SDK::Variables::TickCount() > last_item_update) {
        RecurveBow = player.HasItem(1043);
        BladeKing = player.HasItem(3153);
        WitsEnd = player.HasItem(3091);
        Titanic = player.HasItem(3748);
        Divine = player.HasItem(6632);
        Sheen = player.HasItem(3057);
        Black = player.HasItem(3071);
        Trinity = player.HasItem(3078);
        last_item_update = SDK::Variables::TickCount() + 5000;
    }
}

// W damage — wiki V25.x: min 10/20/30/40/50 (+40%AD +50%AP), max 30/60/90/120/150 (+120%AD +150%AP).
static float GetWDmg(const AIBaseClient& unit, float time) {
    const auto player = Player();
    if (!unit.IsValid() || (!W.IsReady() && !W.IsCharging())) {
        return 0.0f;
    }
    const int wLevel = W.Instance().Level();
    if (wLevel < 1) {
        return 0.0f;
    }
    if (time >= 0.75f) {
        const float baseDamage = 30.0f + static_cast<float>(wLevel - 1) * 30.0f;
        const float extra = 1.2f * player.AD() + 1.5f * player.AP();
        return player.CalculatePhysicalDamage(unit, baseDamage + extra);
    } else {
        const float baseDamage = 10.0f + static_cast<float>(wLevel - 1) * 10.0f;
        const float extra = 0.4f * player.AD() + 0.5f * player.AP();
        const float timeDamage = (baseDamage + extra) * (1.0f + static_cast<float>(time / 0.075 * 0.2));
        return player.CalculatePhysicalDamage(unit, timeDamage);
    }
}

// Q damage — wiki V25.x: 5/25/45/65/85 (+70% AD); passive 10..61 theo level (+20% bonus AD).
static float GetQDmg(const AIBaseClient& target) {
    const auto player = Player();
    if (!target.IsValid() || target.IsDead()) {
        return 0.0f;
    }
    const int qLevel = player.Spellbook().GetSpell(SpellSlot::Q).Level();
    if (qLevel == 0) {
        return 0.0f;
    }
    const float aaWithOnHit = Damage::GetAutoAttackDamage(player, target, true);
    const float aaBase = Damage::GetAutoAttackDamage(player, target, false);
    const float onHitBonus = std::max(0.0f, aaWithOnHit - aaBase);

    // wiki Q: 5 + (rank-1)*20 (+70% AD); minion: + (55 + 12*(level-1)).
    float normaldmg = 5.0f + static_cast<float>(qLevel - 1) * 20.0f + player.AD() * 0.70f;
    if (target.IsMinion() && !AIMinionClient(target.Handle()).IsJungle()) {
        normaldmg = 5.0f + static_cast<float>(qLevel - 1) * 20.0f + player.AD() * 0.70f +
            (50.0f + 11.0f * static_cast<float>(player.Level() - 1));
    }
    normaldmg = player.CalculatePhysicalDamage(target, normaldmg);
    return onHitBonus + normaldmg - 1.0f;
}

static bool CanCastQ(const AIBaseClient& target) {
    const auto player = Player();
    if (target.IsInvulnerable() || (target.HasBuff("UndyingRage") && target.HealthPercent() <= 10.0f)) {
        return false;
    }
    if (Bool(ComboMenu, "ForceQ") && target.IsHero()) {
        if (player.CountEnemyHeroesInRange(Q.Range + 100.0f) <= 1) {
            if (target.Health() <= GetQDmg(target) + Damage::GetAutoAttackDamage(player, target) * 1.0f) {
                return true;
            }
        }
    }
    if (target.HasBuff("ireliamark") || GetQDmg(target) >= target.Health()) {
        return true;
    }
    return false;
}

static void OnBuffRemove(const BuffEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const std::string name = args.BuffName;
    if (name == "sheen") { SheenTimer = static_cast<float>(SDK::Variables::TickCount()) + 1800.0f; }
    if (name == "6632buff") { DivineTimer = static_cast<float>(SDK::Variables::TickCount()) + 1800.0f; }
    if (name == "3078trinityforce") { TrinityTimer = static_cast<float>(SDK::Variables::TickCount()) + 1800.0f; }
}

static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const std::string name = args.SpellName;
    if (name == "IreliaW") {
        ChargingW = SDK::Variables::TickCount();
    }
    if (name == "IreliaE") {
        FirstE = 0;
        PosE = args.EndPosition;
        EcastTime = SDK::Variables::TickCount();
        FirstE = 1;
    }
    if (name == "IreliaEMissile" && FirstE == 1) {
        PosE = args.EndPosition;
        EcastTime = SDK::Variables::TickCount();
    }
    if (name == "IreliaE2") {
        FirstE = 0;
        PosE = Vector3{};
    }
}

static void OnPlayAnimation(const Events::PlayAnimationEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const std::string anim = args.Animation;
    if ((anim == "Spell1" || anim == "Spell3_02") && args.Process != nullptr) {
        *args.Process = false;
    }
}

static void OnObjectCreate(const GameObject& obj) {
    const std::string name = RuntimeObjectName(obj);
    if (name.rfind("Irelia", 0) == 0) {
        if (name.size() >= 6 && name.compare(name.size() - 6, 6, "_R_cas") == 0) {
            CastRForE = false;
        }
        if (name.size() >= 9 && name.compare(name.size() - 9, 9, "_E_cas_02") == 0) {
            CastEForR = false;
        }
    }
}

static void OnObjectDelete(const GameObject& obj) {
    const std::string name = RuntimeObjectName(obj);
    if (name.rfind("Irelia", 0) == 0) {
        if (name.size() >= 6 && name.compare(name.size() - 6, 6, "_R_Mis") == 0) {
            CastRForE = true;
        }
        if (name.size() >= 9 && name.compare(name.size() - 9, 9, "_E_Mis_02") == 0) {
            CastEForR = true;
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFFA500u);
    }
    if (Bool(DrawMenu, "DW", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFF00FF00u);
    }
    if (Bool(DrawMenu, "DE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFFFF0000u);
    }
    if (Bool(DrawMenu, "DR", false) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFDCDCDCu);
    }
}

// C#: interrupt E2 khi địch cast important spell.
static void OnInterrupterSpell(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender.Ptr);
    if (!sender.IsValid() || !sender.IsEnemy()) {
        return;
    }
    if (Bool(OtherMenu, "EInterrupter") && ValidHeroTarget(sender, E.Range) &&
        Extensions::IsCastingInterruptableSpell(sender, false)) {
        if (player.HasBuff("IreliaE") && PosE.IsValid()) {
            const auto predPos = GetE2Prediction(AIBaseClient(sender.Handle()));
            if (HitchanceAtLeast(predPos.Hitchance, HitChance::High)) {
                if (ObjectManager::Player().Position().Distance(predPos.GetCastPosition()) <= E.Range) {
                    CastEPosition(predPos.GetCastPosition());
                }
            }
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

    if (KeyActive(ComboMenu, "CRKEY") && R.IsReady()) {
        CastR();
    }
    CheckItem();
    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        JungleClearE();
        break;
    case OrbwalkingMode::Flee:
        FleeLogic();
        break;
    default:
        break;
    }
}

// ── W-dodge (UnitDodge::EvadeTarget) ──
struct EvadeSpellData {
    std::string ChampionName;
    std::string MissileName;   // = SpellNames[0] (dùng để match args.SpellName)
    std::string DisplayName;   // nhãn menu
    SpellSlot Slot = SpellSlot::Unknown;
    int HealthEvade = 100;
    float SpellRadius = 0.0f;
    float Spell2Delay = 0.0f;
};
inline std::vector<EvadeSpellData> EvadeSpells;

static void EvadeLoadSpellData() {
    EvadeSpells.clear();
    auto add = [](const char* champ, const char* missile, const char* disp,
                  SpellSlot slot, int hp, float radius = 0.0f, float delay = 0.0f) {
        EvadeSpellData d;
        d.ChampionName = champ; d.MissileName = missile; d.DisplayName = disp;
        d.Slot = slot; d.HealthEvade = hp; d.SpellRadius = radius; d.Spell2Delay = delay;
        EvadeSpells.push_back(d);
    };
    add("Garen", "garenqattack", "Garen (Q)", SpellSlot::Q, 100);
    add("Darius", "dariusexecute", "Darius (R)", SpellSlot::R, 100);
    add("Darius", "dariusnoxiantacticsonhattack", "Darius (W)", SpellSlot::W, 100);
    add("Zed", "dariusnoxiantacticsonhattack", "Zed (R)", SpellSlot::R, 100);
    add("Leesin", "blindmonkrkick", "Leesin (R)", SpellSlot::R, 100);
    add("Tristana", "tristana_base_e_explosion", "Tristana (E)", SpellSlot::E, 100);
    add("Tristana", "tristanar", "Tristana (R)", SpellSlot::R, 100);
    add("JarvanIV", "jarvanivcataclysm", "JarvanIV (R)", SpellSlot::R, 100);
    add("skarner", "detonatingshot", "skarner (R)", SpellSlot::R, 100);
    add("kalista", "detonatingshot", "kalista (E)", SpellSlot::E, 100);
    add("khazix", "khazixq", "khazix (Q)", SpellSlot::Q, 80);
    add("khazix", "khazixqlong", "khazix (Q)", SpellSlot::Q, 80);
    add("nocturne", "nocturneunspeakablehorror", "nocturne (E)", SpellSlot::E, 100, 0.0f, 1800.0f);
    add("nocturne", "nocturneparanoia2", "nocturne (R)", SpellSlot::R, 100);
    add("volibear", "volibearqattack", "volibear (Q)", SpellSlot::Q, 100);
    add("volibear", "volibearw", "volibear (W)", SpellSlot::W, 100);
    add("Singed", "fling", "Singed (E)", SpellSlot::E, 100);
    add("Blitzcrank", "powerfistattack", "Blitzcrank (E)", SpellSlot::E, 100);
    add("Renekton", "renektonexecute", "Renekton (W)", SpellSlot::W, 100);
    add("Renekton", "renektonsuperexecute", "Renekton (W)", SpellSlot::W, 100);
    add("Caitlyn", "caitlynaceintheholemissile", "Caitlyn (R)", SpellSlot::R, 100);
    add("Gangplank", "parley", "Gangplank (Q)", SpellSlot::Q, 100);
    add("MissFortune", "missfortunericochetshot", "MissFortune (Q)", SpellSlot::Q, 100);
    add("Pantheon", "pantheonw", "Pantheon (W)", SpellSlot::W, 100);
    add("TwistedFate", "goldcardattack", "TwistedFate (W)", SpellSlot::W, 100);
    add("Vayne", "vaynycondemn", "Vayne (E)", SpellSlot::E, 100);
    add("Talon", "talonqattack", "Talon (Q)", SpellSlot::Q, 100);
    add("Vi", "vir", "Vi (R)", SpellSlot::R, 100);
    add("Alistar", "headbutt", "Alistar (W)", SpellSlot::W, 100);
    add("Udyr", "udyrbearattack", "Udyr (E)", SpellSlot::E, 100);
    add("MasterYi", "alphastrike", "MasterYi (Q)", SpellSlot::Q, 100);
    add("MasterYi", "masteryidoublestrike", "MasterYi", SpellSlot::Unknown, 100);
    add("Rengar", "rengarqattack", "Rengar (Q)", SpellSlot::Q, 100);
    add("Kled", "kledwattack", "Kled", SpellSlot::Unknown, 100);
    add("XinZhao", "xinzhaoqthrust1", "XinZhao (Q)", SpellSlot::Q, 100);
    add("XinZhao", "xinzhaoqthrust2", "XinZhao (Q)", SpellSlot::Q, 100);
    add("XinZhao", "xinzhaoqthrust3", "XinZhao (Q)", SpellSlot::Q, 100);
    add("XinZhao", "xinzhaoe", "XinZhao (E)", SpellSlot::E, 100);
    add("Quinn", "quinne", "Quinn (E)", SpellSlot::E, 65);
    add("Lucian", "lucianq", "Lucian (Q)", SpellSlot::Q, 20);
    add("Jayce", "jaycetotheskies", "Jayce (Q)", SpellSlot::Q, 100);
    add("Jayce", "jaycethunderingblow", "Jayce (E)", SpellSlot::E, 100);
    add("Reksai", "reksaie", "Reksai (E)", SpellSlot::E, 100);
    add("Reksai", "reksair", "Reksai (R)", SpellSlot::R, 100);
    add("Reksai", "reksaiwburrowed", "Reksai (W)", SpellSlot::W, 100, 175.0f);
    add("Kayn", "kaynr", "Kayn (R)", SpellSlot::R, 100);
    add("Kayn", "hecarimrampattack", "Kayn (R)", SpellSlot::R, 100);
    add("Kindred", "kindrede", "Kindred (E)", SpellSlot::E, 100);
    add("DrMundo", "drmundoeattack", "DrMundo (E)", SpellSlot::E, 100);
    add("MonkeyKing", "monkeykingqattack", "MonkeyKing (Q)", SpellSlot::Q, 100);
    add("MonkeyKing", "monkeykingnimbus", "MonkeyKing (E)", SpellSlot::E, 100);
    add("Yorick", "yorickqattack", "Yorick (Q)", SpellSlot::Q, 100);
    add("Poppy", "poppye", "Poppy (E)", SpellSlot::E, 100);
    add("Warwick", "warwickq", "Warwick (Q)", SpellSlot::Q, 100);
    add("Rammus", "puncturingtaunt", "Rammus (E)", SpellSlot::E, 100);
}

static bool EvadeEnemyHas(const std::string& champ) {
    for (const auto& h : GameObjects::EnemyHeroes()) {
        if (_stricmp(h.CharacterName().c_str(), champ.c_str()) == 0) {
            return true;
        }
    }
    return false;
}

static void EvadeInit() {
    EvadeLoadSpellData();
    EvadeMenu = MenuRoot->AddSubMenu(new Menu("EvadeTarget", "W Dodge"));
    EvadeMenu->Add(new MenuBool("W", "Use W Dodge Spell"));
    EvadeAAMenu = EvadeMenu->AddSubMenu(new Menu("AA", "Attack"));
    EvadeAAMenu->Add(new MenuBool("B", "Basic Attack Dodge"));
    EvadeAAMenu->Add(new MenuSlider("BHpU", "-> When Health < (%)", 35, 0, 100));
    EvadeAAMenu->Add(new MenuBool("C", "Cric Attack Dodge"));
    EvadeAAMenu->Add(new MenuSlider("CHpU", "-> When Health < (%)", 40, 0, 100));
    for (const auto& spell : EvadeSpells) {
        if (EvadeEnemyHas(spell.ChampionName)) {
            if (!EvadeMenu->Get<MenuBool>(spell.MissileName.c_str())) {
                EvadeMenu->Add(new MenuBool(spell.MissileName.c_str(), spell.DisplayName.c_str(), true));
                EvadeMenu->Add(new MenuSlider((spell.MissileName + "H").c_str(),
                    "^-Dodge Spell When Health =< X%", spell.HealthEvade, 0, 100));
            }
        }
    }
}

static void Evade_OnDoCastBack(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender.Ptr);
    if (!sender.IsValid() || !sender.IsEnemy()) {
        return;
    }
    if (player.HasBuff("ireliawdefense") || !W.IsReady()) {
        return;
    }
    std::string spellName = args.SpellName;
    std::transform(spellName.begin(), spellName.end(), spellName.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const EvadeSpellData* found = nullptr;
    for (const auto& s : EvadeSpells) {
        if (s.MissileName.find(spellName) != std::string::npos) {
            found = &s;
            break;
        }
    }
    if (found != nullptr) {
        const auto* enabled = EvadeMenu->Get<MenuBool>(found->MissileName.c_str());
        const auto* hpSlider = EvadeMenu->Get<MenuSlider>((found->MissileName + "H").c_str());
        if (enabled && enabled->Value && hpSlider &&
            player.HealthPercent() <= static_cast<float>(hpSlider->Value)) {
            if (args.Target.NetworkId == 0) {
                if (sender.DistanceToPlayer() <= found->SpellRadius) {
                    W.Cast(sender.PreviousPosition());
                    return;
                }
            }
            if (args.Target.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
                // MISSING API: DelayAction — cast W ngay (bỏ delay Spell2Delay).
                W.Cast(sender.PreviousPosition());
            }
            return;
        }
    }
    if (found == nullptr && Orbwalker::IsAutoAttack(args.SpellName) &&
        args.Target.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
        if (spellName.find("crit") != std::string::npos && Bool(EvadeAAMenu, "B", false)) {
            if (player.HealthPercent() < static_cast<float>(Slider(EvadeAAMenu, "BHpU", 35))) {
                W.Cast(sender.PreviousPosition());
                return;
            }
        }
        if (spellName.find("basic") != std::string::npos && Bool(EvadeAAMenu, "C", false)) {
            if (player.HealthPercent() < static_cast<float>(Slider(EvadeAAMenu, "CHpU", 40))) {
                W.Cast(sender.PreviousPosition());
                return;
            }
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnPlayAnimation -= &OnPlayAnimation;
    Events::hook.OnGameUpdate -= &QLogic;
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnBuffRemove -= &OnBuffRemove;
    Events::hook.OnProcessSpell -= &OnProcessSpellCast;
    Events::hook.OnProcessSpell -= &OnInterrupterSpell;
    Events::hook.OnProcessSpell -= &Evade_OnDoCastBack;
    GameObjects::RemoveOnCreate(&OnObjectCreate);
    GameObjects::RemoveOnDelete(&OnObjectDelete);
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Irelia
