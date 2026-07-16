#pragma once

// ============================================================================
// SharpShooter AIO — Renata Glasc
// Port từ CSharpFiles/Renata/Renata.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Thresh.h.
//
// Kỹ năng:
//   Q Handshake      — hook state machine: RenataQ (bắt) → RenataQRecast (fling).
//   W Bailout        — targeted ally save (heal/rez-ish); auto theo % + damage-pred.
//   E Loyalty Program — skillshot line 800 (shield ally / hurt enemy).
//   R Hostile Takeover — skillshot line AoE 2000, delay 0.75, width 250, speed 650.
//
// Ghi chú port (giữ 1-1 tới mức API cho phép):
//   * Q state: Q.Instance().Name() == "RenataQ"/"RenataQRecast".
//   * GetBestBackPosition: thuật fling — ưu tiên trap đồng minh, địch đang cast,
//     kéo vào trụ/đám đồng minh; ring 30 điểm bán kính 250.
//   * Trap tracking (Jhin/Cait/Jinx) qua OnCreate/OnDelete.
//   * AllyChampSaver: dự đoán damage tới ally soulbound (AA + spell) để W cứu.
//   * MISSING API:
//       - GetFirstWallPoint(v2,v2) → NavMesh::IsWallBetween (bool chặn tường).
//       - IsCastingImporantSpell() → Extensions::IsCastingInterruptableSpell.
//       - GetSummonerSpellDamage(Ignite) → không có; bỏ term ignite. Xem missapi.md.
//       - Vector position .IsUnderAllyTurret()/.CountAllysHerosInRangeFix() →
//         helper cục bộ (turret list + distance). Xem missapi.md.
//   * Không có bảng damage tự viết → không cần đối chiếu wiki (spell dùng prediction).
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace Plugins::SharpAIO::Renata {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HookMenu = nullptr;
inline Menu* BailoutMenu = nullptr;
inline Menu* WBlackMenu = nullptr;
inline Menu* HostileMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 900.0f };
inline Spell W{ SpellSlot::W, 800.0f };
inline Spell E{ SpellSlot::E, 800.0f };
inline Spell R{ SpellSlot::R, 2000.0f };

inline bool Loaded = false;
inline int Delay = 0;

enum class CastState { NotReady, First, Second };

// Trap đồng minh (Jhin/Caitlyn/Jinx) để fling địch vào.
struct TrapInfo {
    AIBaseClient Pointer;
    int Width = 0;
    int ValidTime = 0;
};
inline std::vector<TrapInfo> TrapList;

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

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
}

static bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

static bool ValidAllyTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return hero.IsValid() && !hero.IsDead() && hero.Health() > 0.0f &&
        hero.Distance(Player()) <= range;
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

// Ring N điểm quanh center (thay Geometry.Circle.Points).
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

// C# GetFirstWallPoint(a,b) == Vector2.Zero  ⟺  không có tường chặn.
static bool WallBlocked(const Vector3& from, const Vector3& to) {
    return NavMesh::IsWallBetween(from, to, 50.0f);
}

// Đếm hero đồng minh quanh 1 vị trí.
static int CountAlliesAround(const Vector3& pos, float range) {
    int count = 0;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ally.IsValid() && !ally.IsDead() && ally.Position().Distance(pos) <= range) {
            ++count;
        }
    }
    return count;
}

// MISSING API: vị trí .IsUnderAllyTurret() — helper cục bộ qua turret list (~900).
static bool IsUnderAllyTurretPos(const Vector3& pos) {
    for (const auto& t : GameObjects::AllyTurrets()) {
        if (t.IsValid() && !t.IsDead() && t.Position().Distance(pos) <= 900.0f) {
            return true;
        }
    }
    return false;
}

static bool IsUnderEnemyTurretPos(const Vector3& pos) {
    for (const auto& t : GameObjects::EnemyTurrets()) {
        if (t.IsValid() && !t.IsDead() && t.Position().Distance(pos) <= 900.0f) {
            return true;
        }
    }
    return false;
}

// C#: Qedtarget — hero địch dính "RenataQ" do CHÍNH mình cast.
static AIHeroClient Qedtarget() {
    const auto player = Player();
    if (!player.IsValid()) {
        return AIHeroClient();
    }
    for (const auto& e : GameObjects::EnemyHeroes()) {
        if (!e.IsValid() || !e.HasBuff("RenataQ")) {
            continue;
        }
        const uintptr_t caster = e.GetBuffCaster("RenataQ");
        if (caster == player.Address()) {
            return e;
        }
    }
    return AIHeroClient();
}

// C#: IsHookChamp — hero trong high-priority hook list.
static bool IsHookChamp(const AIHeroClient& unit) {
    if (!HookMenu || !unit.IsValid()) {
        return false;
    }
    const std::string key = "zq." + unit.CharacterName();
    const auto* item = HookMenu->Get<MenuBool>(key.c_str());
    return item ? item->Value : false;
}

// C#: GetWPriority — slider ưu tiên W cho ally (0 = không dùng).
static int GetWPriority(const AIHeroClient& unit) {
    if (!WBlackMenu || !unit.IsValid()) {
        return 0;
    }
    const std::string key = "notw." + unit.CharacterName();
    const auto* item = WBlackMenu->Get<MenuSlider>(key.c_str());
    return item ? item->Value : 0;
}

static float GetBuffLaveTime(const AIBaseClient& target, const char* buffName) {
    if (!target.IsValid()) {
        return 0.0f;
    }
    const auto buff = ::CoreBuffs::FindByName(target.Address(), buffName);
    if (!buff.IsValid()) {
        return 0.0f;
    }
    return buff.GetEndTime() - Game::Time();
}

static CastState GetQState() {
    if (!Q.IsReady()) {
        return CastState::NotReady;
    }
    const std::string name = Q.Instance().Name();
    if (name == "RenataQ") {
        return CastState::First;
    }
    if (name == "RenataQRecast") {
        return CastState::Second;
    }
    return CastState::NotReady;
}

// Forward declarations — đúng thứ tự file C#.
static void RLogic();
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void WLogic();
static void Combo();
static void OnObjectCreate(const GameObject& obj);
static void OnObjectDelete(const GameObject& obj);
static bool IsSafePos(const Vector3& pos);
static Vector3 GetBestBackPosition(const AIBaseClient& unit);
static void OnDraw();
static void OnUnload();

// ── AllyChampSaver: dự đoán damage tới ally soulbound để W cứu kịp ──
namespace AllyChampSaver {
    inline AIHeroClient SoulBound;
    // Cặp (arrival_time, damage) — thay Dictionary<float,float>.
    inline std::vector<std::pair<float, float>> IncDamage;
    inline std::vector<std::pair<float, float>> InstDamage;

    static float IncomingDamage() {
        float sum = 0.0f;
        for (const auto& e : IncDamage) sum += e.second;
        for (const auto& e : InstDamage) sum += e.second;
        return sum;
    }

    static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args);
    static void OnTick();
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Renata", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuBool("CE", "Use E"));

    HookMenu = MenuRoot->AddSubMenu(new Menu("Hook List", "High Priority Hook List"));
    for (const auto& obj : GameObjects::EnemyHeroes()) {
        const std::string name = obj.CharacterName();
        if (name.empty()) {
            continue;
        }
        auto* prio_ = SDK::Modes::Priority::Instance();
        const int prio = prio_ ? prio_->GetHeroPriority(obj) : 0;
        const std::string key = "zq." + name;
        HookMenu->Add(new MenuBool(key.c_str(), name.c_str(), prio >= 3));
    }

    BailoutMenu = MenuRoot->AddSubMenu(new Menu("Bailout Settings", "Bailout"));
    BailoutMenu->Add(new MenuList("WMode", "Use W Mode",
        std::vector<std::string>{ "Only Combo", "Always", "Disable" }, 1));
    BailoutMenu->Add(new MenuList("UseWHELP", "Use W Help ally",
        std::vector<std::string>{ "Auto", "W Health", "Disable" }, 0));
    BailoutMenu->Add(new MenuSlider("UseWHELP2", "Enable when ally health <= X%", 20, 0, 100));
    BailoutMenu->Add(new MenuSlider("UseWHELP21", "ally enemy count >= X", 1, 1, 5));
    WBlackMenu = BailoutMenu->AddSubMenu(new Menu("W Priority", "W Priority Set (0 is disable)"));
    for (const auto& obj : GameObjects::AllyHeroes()) {
        const std::string name = obj.CharacterName();
        if (name.empty()) {
            continue;
        }
        const std::string key = "notw." + name;
        WBlackMenu->Add(new MenuSlider(key.c_str(), name.c_str(), 3, 0, 5));
    }

    HostileMenu = MenuRoot->AddSubMenu(new Menu("Hostile Settings", "Hostile Takeover"));
    HostileMenu->Add(new MenuList("RMode", "Use R Mode",
        std::vector<std::string>{ "Only Combo", "Always", "Disable" }, 0));
    HostileMenu->Add(new MenuSlider("RMinHits", "Use R Min hits X enemy", 2, 1, 5));
    HostileMenu->Add(new MenuSlider("RCheckRange", "Use R When Enemy dist player <= X", 900, 400, 2000));
    HostileMenu->Add(new MenuSlider("Count", "Don't R if has Enemy in x Range", 400, 0, 1000));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DQ", "Q Range"));
    DrawMenu->Add(new MenuBool("DW", "W Range", false));
    DrawMenu->Add(new MenuBool("DE", "E Range"));
    DrawMenu->Add(new MenuBool("DR", "R Range"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 900.0f);
    Q.SetSkillshot(0.25f, 70.0f, 1450.0f, true, SpellType::Line);

    W = Spell(SpellSlot::W, 800.0f);
    W.SetTargetted(0.0f, FLT_MAX);

    E = Spell(SpellSlot::E, 800.0f);
    E.SetSkillshot(0.25f, 110.0f, 1450.0f, false, SpellType::Line);

    R = Spell(SpellSlot::R, 2000.0f);
    R.SetSkillshot(0.75f, 250.0f, 650.0f, false, SpellType::Line);

    BuildMenu();

    // Nạp trap đã tồn tại sẵn.
    for (const auto& obj : GameObjects::AllGameObjects()) {
        OnObjectCreate(obj);
    }

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &AllyChampSaver::OnProcessSpellCast;
    Drawing::OnDraw += &OnDraw;
    GameObjects::AddOnCreate(&OnObjectCreate);
    GameObjects::AddOnDelete(&OnObjectDelete);

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Renata loaded</font>");
}

static void RLogic() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (!R.IsReady() || player.CountEnemyHeroesInRange(static_cast<float>(Slider(HostileMenu, "Count", 400))) != 0) {
        return;
    }
    const int rMode = ListIndex(HostileMenu, "RMode", 0);
    if ((rMode == 0 && Orbwalker::ActiveMode() == OrbwalkingMode::Combo) || rMode == 1) {
        const float checkRange = static_cast<float>(Slider(HostileMenu, "RCheckRange", 900));
        const int minHits = Slider(HostileMenu, "RMinHits", 2);
        for (const auto& obj : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(obj, R.Range)) {
                continue;
            }
            if (obj.DistanceToPlayer() <= checkRange) {
                const auto pred = R.GetPrediction(obj);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh) &&
                    pred.GetAoeTargetsHitCount() >= minHits) {
                    R.Cast(pred.GetCastPosition());
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

    // Khoá AA khi đang Q-recast hoặc vừa mới cast Q (<=400ms).
    Orbwalker::AttackEnabled(
        !(GetQState() == CastState::Second ||
          (SDK::Variables::TickCount() - Q.LastCastAttemptT <= 400)));

    WLogic();
    AllyChampSaver::OnTick();
    RLogic();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    default:
        break;
    }

    // Dọn trap hết hạn.
    TrapList.erase(
        std::remove_if(TrapList.begin(), TrapList.end(),
            [](const TrapInfo& t) {
                return !t.Pointer.IsValid() || t.Pointer.IsDead() ||
                    SDK::Variables::TickCount() > t.ValidTime;
            }),
        TrapList.end());
}

static void WLogic() {
    if (!W.IsReady()) {
        return;
    }
    const int wMode = ListIndex(BailoutMenu, "WMode", 1);
    if ((wMode == 0 && Orbwalker::ActiveMode() == OrbwalkingMode::Combo) || wMode == 1) {
        const int useWHelp = ListIndex(BailoutMenu, "UseWHELP", 0);
        if (useWHelp != 2) {
            if (useWHelp == 1) {
                // W Health mode: heal ally theo priority giảm dần.
                std::vector<AIHeroClient> allies;
                for (const auto& x : GameObjects::AllyHeroes()) {
                    if (ValidAllyTarget(x, W.Range)) {
                        allies.push_back(x);
                    }
                }
                std::sort(allies.begin(), allies.end(),
                    [](const AIHeroClient& a, const AIHeroClient& b) {
                        return GetWPriority(a) > GetWPriority(b);
                    });
                for (const auto& obj : allies) {
                    if (GetWPriority(obj) != 0) {
                        if (obj.HealthPercent() <= static_cast<float>(Slider(BailoutMenu, "UseWHELP2", 20)) &&
                            obj.CountEnemyHeroesInRange(500.0f) >= Slider(BailoutMenu, "UseWHELP21", 1)) {
                            W.CastOnUnit(AIBaseClient(obj.Handle()));
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

    if (Bool(ComboMenu, "CE") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target)) {
            const auto pred = E.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                const Vector3 endpos = player.Position().Extend(pred.GetCastPosition(), E.Range);
                E.Cast(endpos);
            }
        }
    }

    if (Bool(ComboMenu, "CQ")) {
        if (GetQState() == CastState::First && SDK::Variables::TickCount() > Delay) {
            const auto target = GetTarget(Q.Range, DamageType::Magical);
            if (ValidHeroTarget(target, Q.Range)) {
                const auto pred = Q.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
                    Q.Cast(pred.GetCastPosition());
                    Delay = SDK::Variables::TickCount() + 500;
                }
            }
        }
        if (GetQState() == CastState::Second) {
            const auto hero = Qedtarget();
            if (ValidHeroTarget(hero)) {
                if (GetBuffLaveTime(AIBaseClient(hero.Handle()), "RenataQ") < 0.1f ||
                    Extensions::IsCastingInterruptableSpell(hero, false)) {
                    Q.Cast(GetBestBackPosition(AIBaseClient(hero.Handle())));
                }
            }
        }
    }
}

static void OnObjectCreate(const GameObject& obj) {
    const auto base = AIBaseClient(obj.Handle());
    if (!base.IsValid()) {
        return;
    }
    const std::string name = base.Name();
    // Jhin trap.
    if (name == "Noxious Trap" && base.MaxHealth() == 6.0f && base.IsAlly()) {
        TrapList.push_back(TrapInfo{ base, 160, SDK::Variables::TickCount() + 180000 });
        return;
    }
    // Caitlyn trap.
    if (name == "Cupcake Trap" && base.MaxHealth() == 100.0f && base.IsAlly()) {
        TrapList.push_back(TrapInfo{ base, 15, SDK::Variables::TickCount() + 180000 });
        return;
    }
    // Jinx trap.
    if (name == "Noxious Trap" && base.MaxHealth() == 1.0f && base.IsAlly()) {
        TrapList.push_back(TrapInfo{ base, 115, SDK::Variables::TickCount() + 3000 });
        return;
    }
}

static void OnObjectDelete(const GameObject& obj) {
    const auto base = AIBaseClient(obj.Handle());
    if (!base.IsValid()) {
        return;
    }
    const int netId = base.NetworkId();
    TrapList.erase(
        std::remove_if(TrapList.begin(), TrapList.end(),
            [netId](const TrapInfo& t) { return t.Pointer.NetworkId() == netId; }),
        TrapList.end());
}

static bool IsSafePos(const Vector3& pos) {
    return CountAlliesAround(pos, 400.0f) <= 2;
}

// Thuật fling Q2: chọn điểm kéo địch tối ưu.
static Vector3 GetBestBackPosition(const AIBaseClient& unit) {
    const auto player = Player();
    if (!player.IsValid() || !unit.IsValid()) {
        return player.IsValid() ? player.Position() : Vector3();
    }

    // 1) Trap đồng minh gần địch, không bị tường chặn.
    for (const auto& trap : TrapList) {
        if (trap.Pointer.IsValid() && trap.Pointer.Distance(unit) <= 250.0f &&
            !WallBlocked(unit.PreviousPosition(), trap.Pointer.Position())) {
            return trap.Pointer.Position();
        }
    }

    // 2) Địch khác trong 250, không tường chặn — ưu tiên đang cast/dash.
    std::vector<AIHeroClient> enemies;
    for (const auto& x : GameObjects::EnemyHeroes()) {
        if (x.NetworkId() != unit.NetworkId() &&
            !WallBlocked(unit.PreviousPosition(), x.PreviousPosition()) &&
            ValidHeroTarget(x) &&
            x.PreviousPosition().Distance(unit.PreviousPosition()) <= 250.0f + x.BoundingRadius()) {
            enemies.push_back(x);
        }
    }
    for (const auto& x : enemies) {
        if (Extensions::IsCastingInterruptableSpell(x, false) || x.IsDashing()) {
            return x.PreviousPosition();
        }
    }
    if (!enemies.empty()) {
        return enemies.front().PreviousPosition();
    }

    // 3) Ring 30 điểm bán kính 250.
    for (const auto& ringPt : RingPoints(unit.PreviousPosition(), 250.0f, 30)) {
        const Vector3 obj = unit.PreviousPosition().Extend(ringPt, 250.0f);
        if (WallBlocked(unit.PreviousPosition(), obj)) {
            continue;
        }
        // Kéo vào trụ đồng minh.
        if (IsUnderAllyTurretPos(obj) &&
            (!unit.IsUnderAllyTurret() || unit.DistanceToPlayer() > obj.Distance(player.Position()))) {
            if (player.IsUnderAllyTurret()) {
                return player.Position();
            }
            if (IsSafePos(obj)) {
                return obj;
            }
        }
        // Địch trong trụ địch, điểm không — kéo ra gần hơn.
        if (unit.IsUnderEnemyTurret() && !IsUnderEnemyTurretPos(obj) &&
            unit.DistanceToPlayer() > obj.Distance(player.Position())) {
            if (IsSafePos(obj)) {
                return obj;
            }
        }
        // Kéo vào đám đồng minh (hook champ).
        const auto toHero = AIHeroClient(unit.Handle());
        if (toHero.IsValid() && IsHookChamp(toHero)) {
            if (CountAlliesAround(unit.PreviousPosition(), 500.0f) < CountAlliesAround(obj, 500.0f)) {
                return obj;
            }
        }
    }

    // 4) Máu thấp & địch khoẻ hơn → fling ra xa.
    if (player.HealthPercent() <= 35.0f && unit.HealthPercent() > player.HealthPercent() &&
        CountAlliesAround(player.Position(), 500.0f) <= 2 &&
        unit.PreviousPosition().Extend(player.Position(), 250.0f).Distance(player.Position()) <= 400.0f) {
        return player.Position().Extend(unit.PreviousPosition(), player.Distance(unit) + 400.0f);
    }

    return player.Position();
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFF0000u);
    }
    if (Bool(DrawMenu, "DW", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFF00FF00u);
    }
    if (Bool(DrawMenu, "DE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFF5F9EA0u);
    }
    if (Bool(DrawMenu, "DR", false) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFF4500u);
    }
}

// ── AllyChampSaver impl ──
namespace AllyChampSaver {
    static void OnTick() {
        const auto player = Player();
        if (!player.IsValid()) {
            return;
        }
        // Tìm soulbound: ally priority cao nhất (>0).
        int bestPrio = 0;
        AIHeroClient found;
        for (const auto& obj : GameObjects::AllyHeroes()) {
            if (!ValidAllyTarget(obj, W.Range)) {
                continue;
            }
            const int prio = GetWPriority(obj);
            if (prio == 0) {
                continue;
            }
            if (prio >= bestPrio) {
                bestPrio = prio;
                found = obj;
            }
        }
        SoulBound = found;

        const int wMode = ListIndex(BailoutMenu, "WMode", 1);
        if (((wMode == 0 && Orbwalker::ActiveMode() == OrbwalkingMode::Combo) || wMode == 1) && W.IsReady()) {
            if (SoulBound.IsValid()) {
                if ((SoulBound.HealthPercent() < 10.0f && SoulBound.CountEnemyHeroesInRange(500.0f) > 0) ||
                    IncomingDamage() > SoulBound.Health()) {
                    W.CastOnUnit(AIBaseClient(SoulBound.Handle()));
                }
            }
        }

        // Xoá entry đã tới hạn.
        const float now = Game::Time();
        IncDamage.erase(
            std::remove_if(IncDamage.begin(), IncDamage.end(),
                [now](const std::pair<float, float>& e) { return e.first < now; }),
            IncDamage.end());
        InstDamage.erase(
            std::remove_if(InstDamage.begin(), InstDamage.end(),
                [now](const std::pair<float, float>& e) { return e.first < now; }),
            InstDamage.end());
    }

    static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
        if (!SoulBound.IsValid()) {
            return;
        }
        const auto sender = AIHeroClient(args.Sender.Ptr);
        if (!sender.IsValid() || !sender.IsEnemy()) {
            return;
        }
        const int wMode = ListIndex(BailoutMenu, "WMode", 1);
        if (!((wMode == 0 && Orbwalker::ActiveMode() == OrbwalkingMode::Combo) || wMode == 1)) {
            return;
        }

        // Auto attack tới soulbound.
        if (args.IsAutoAttack && args.Target.NetworkId != 0 &&
            static_cast<int>(args.Target.NetworkId) == SoulBound.NetworkId()) {
            const float missileSpeed = sender.GetSpell(SpellSlot::BasicAttack).MissileSpeed();
            const float arrive = missileSpeed > 0.0f
                ? SoulBound.PreviousPosition().Distance(sender.PreviousPosition()) / missileSpeed + Game::Time()
                : Game::Time();
            IncDamage.emplace_back(arrive,
                Damage::GetAutoAttackDamage(sender, AIBaseClient(SoulBound.Handle())));
            return;
        }

        // Spell (Q/W/E/R) tới soulbound.
        const SpellSlot slot = sender.GetSpellSlot(args.SpellName);
        // MISSING API: GetSummonerSpellDamage(Ignite) — bỏ term ignite (xem missapi.md).
        if (slot == SpellSlot::Q || slot == SpellSlot::W ||
            slot == SpellSlot::E || slot == SpellSlot::R) {
            const bool hitsTarget =
                (args.Target.NetworkId != 0 && static_cast<int>(args.Target.NetworkId) == SoulBound.NetworkId());
            if (hitsTarget) {
                InstDamage.emplace_back(Game::Time() + 2.0f,
                    sender.GetSpellDamage(AIBaseClient(SoulBound.Handle()), slot));
            }
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &AllyChampSaver::OnProcessSpellCast;
    Drawing::OnDraw -= &OnDraw;
    GameObjects::RemoveOnCreate(&OnObjectCreate);
    GameObjects::RemoveOnDelete(&OnObjectDelete);

    TrapList.clear();
    AllyChampSaver::IncDamage.clear();
    AllyChampSaver::InstDamage.clear();
    Loaded = false;
}

} // namespace Plugins::SharpAIO::Renata
