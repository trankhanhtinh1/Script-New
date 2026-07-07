#pragma once

// ============================================================================
// SharpShooter AIO — Miss Fortune
// Port từ SharpShooterCSHarp/Plugins/MissFortune.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Double Up   — targeted 650, có "Q2" bounce: chọn primary target sao cho
//                   longRangeTarget (tối đa Q.Range+450) nằm trong cone 40° phía
//                   sau primary → viên đạn nảy trúng.
//   W Strut       — self-buff (không range), cast trước khi đánh thường (combo).
//   E Make It Rain— skillshot circle 1000, delay 0.5, radius 100.
//   R Bullet Time — channel 1400. Có keybind huỷ R (IssueMove tới cursor).
//
// Ghi chú port:
//   * LoveTap (passive): đổi mục tiêu auto-attack qua Orbwalker::ForceTarget khi
//     target hiện tại không chết trong 2 đòn để reset stack passive.
//   * "Block Movement order While Using R": bản C# chặn PlayerIssueOrder (inbound
//     order hook). SDK NightSharp chưa expose hook order inbound →
//     // TODO SDK: không port được phần chặn; R-cancel (outbound) vẫn hoạt động.
//   * Q2 bounce dùng SectorPoly (cone) + IsInside, tương đương Geometry.Sector C#.
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreControl.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::MissFortune {

using SDK::Core::Utils::AutoAttack;

inline const char* const kRBuffName = "missfortunebulletsound";

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 650.0f };
inline Spell W{ SpellSlot::W, FLT_MAX };
inline Spell E{ SpellSlot::E, 1000.0f };
inline Spell R{ SpellSlot::R, 1400.0f };

inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;
inline int LoveTapTargetNetworkId = -1;

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

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void OnAfterAttack(OrbwalkingActionArgs& args);
static void Combo();
static void Mixed();
static void Clear();
static void AutoHarass();
static void HandleRCancel();
static bool Q2Logic();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Miss Fortune", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useQ2", "Use Q2 (bounce)"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q", false));
    HarassMenu->Add(new MenuBool("useQ2", "Use Q2 (bounce)"));
    HarassMenu->Add(new MenuBool("useE", "Use E", false));
    HarassMenu->Add(new MenuBool("autoHarass", "Auto Harass"));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useE", "Use E", false));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useE", "Use E", false));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("q2KillOnly", "Harass Q2 Only if Kills Unit", false));
    MiscMenu->Add(new MenuBool("loveTap", "LoveTap: new AA target for passive"));
    MiscMenu->Add(new MenuKeyBind("cancelR", "Cancel R", 'T', KeyBindType::Press));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 650.0f);
    Q.SetTargetted(0.25f, 1400.0f);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, FLT_MAX);

    E = Spell(SpellSlot::E, 1000.0f);
    E.SetSkillshot(0.5f, 100.0f, FLT_MAX, false, SpellType::Circle);

    R = Spell(SpellSlot::R, 1400.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnAfterAttack += &OnAfterAttack;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Miss Fortune loaded</font>");
}

static bool HasRBuff() {
    const auto player = Player();
    return player.IsValid() && player.HasBuff(kRBuffName);
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
        return;
    }

    // Không cast spell khi đang channel R (giống bản C#: chặn khi có RBuff).
    if (!HasRBuff()) {
        switch (Orbwalker::ActiveMode()) {
        case OrbwalkingMode::Combo:
            Combo();
            break;
        case OrbwalkingMode::Harass:
            Mixed();
            break;
        case OrbwalkingMode::LaneClear:
            Clear();
            break;
        default:
            break;
        }

        AutoHarass();
    }

    HandleRCancel();
}

static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }

    if (Q.IsReady()) {
        if (Bool(ComboMenu, "useQ2")) {
            Q2Logic();
        }
        if (Bool(ComboMenu, "useQ")) {
            const auto target = GetTarget(Q.Range, DamageType::Physical);
            if (ValidHeroTarget(target, Q.Range)) {
                Q.CastOnUnit(target);
            }
        }
    }

    if (Bool(ComboMenu, "useE") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target, E.Range)) {
            const auto pred = E.GetPrediction(target, true);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::Medium)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }
}

static void Mixed() {
    const auto player = Player();
    if (!player.IsValid() || !ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        return;
    }

    if (Q.IsReady()) {
        if (Bool(HarassMenu, "useQ2")) {
            Q2Logic();
        }
        if (Bool(HarassMenu, "useQ", false)) {
            const auto target = GetTarget(Q.Range, DamageType::Physical);
            if (ValidHeroTarget(target, Q.Range)) {
                Q.CastOnUnit(target);
            }
        }
    }

    if (Bool(HarassMenu, "useE", false) && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target, E.Range)) {
            const auto pred = E.GetPrediction(target, true);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::Medium)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Lane clear E: bắn cụm lính (>=4).
    if (Bool(LaneClearMenu, "useE", false) && E.IsReady() && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
        auto minions = GameObjects::EnemyLaneMinions();
        if (minions.empty()) {
            minions = GameObjects::EnemyMinions();
        }
        std::vector<AIBaseClient> targets;
        targets.reserve(minions.size());
        for (const auto& minion : minions) {
            if (ValidTarget(minion, E.Range)) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        if (!targets.empty()) {
            const auto farm = E.GetCircularFarmLocation(targets);
            if (farm.MinionsHit >= 4) {
                E.Cast(Vector3::From2D(farm.Position));
            }
        }
    }

    // Jungle clear Q / E: mob máu cao nhất trong tầm.
    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !ValidTarget(mob, 600.0f) || mob.IsPlant() || mob.IsPet();
            }),
        mobs.end());
    std::sort(
        mobs.begin(),
        mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) {
            return a.MaxHealth() > b.MaxHealth();
        });

    if (mobs.empty()) {
        return;
    }
    const auto& mob = mobs.front();

    if (Bool(JungleClearMenu, "useQ") && Q.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
        ValidTarget(mob, Q.Range)) {
        Q.CastOnUnit(mob);
    }

    if (Bool(JungleClearMenu, "useE", false) && E.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
        ValidTarget(mob, E.Range)) {
        E.Cast(mob.Position());
    }
}

static void AutoHarass() {
    if (!Bool(HarassMenu, "autoHarass") || !Q.IsReady()) {
        return;
    }

    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo || mode == OrbwalkingMode::Harass) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsRecalling() || !ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        return;
    }

    if (Bool(HarassMenu, "useQ2")) {
        Q2Logic();
    }
    if (Bool(HarassMenu, "useQ", false)) {
        const auto target = GetTarget(Q.Range, DamageType::Physical);
        if (ValidHeroTarget(target, Q.Range)) {
            Q.CastOnUnit(target);
        }
    }
}

// W Strut: bật trước khi đánh thường trong combo (nếu target trong tầm AA).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }

    const auto targetBase = AIBaseClient(args.Target.Handle());

    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo &&
        Bool(ComboMenu, "useW") && W.IsReady() &&
        ValidUnit(targetBase) && AutoAttack::InAutoAttackRange(targetBase)) {
        W.Cast();
    }

    // LoveTap: nếu đang đánh 1 hero mà không giết được trong 2 đòn và đó lại là
    // target đã tap lần trước → chuyển sang hero khác trong tầm AA để reset stack.
    if (Bool(MiscMenu, "loveTap")) {
        const auto player = Player();
        if (targetBase.IsValid() && targetBase.IsHero() && player.IsValid()) {
            const AIHeroClient hero(args.Target.Handle());
            const float twoHits = player.GetAutoAttackDamage(targetBase, true) * 2.0f;
            if (hero.Health() + hero.PhysicalShield() > twoHits &&
                hero.NetworkId() == LoveTapTargetNetworkId) {
                AIHeroClient newTarget;
                for (const auto& enemy : GameObjects::EnemyHeroes()) {
                    if (ValidHeroTarget(enemy) && AutoAttack::InAutoAttackRange(enemy) &&
                        enemy.NetworkId() != LoveTapTargetNetworkId) {
                        newTarget = enemy;
                        break;
                    }
                }
                if (newTarget.IsValid()) {
                    args.Process = false;
                    Orbwalker::ForceTarget(AttackableUnit(newTarget.Handle()));
                } else {
                    Orbwalker::ForceTarget(AttackableUnit());
                }
            }
        }
    } else {
        Orbwalker::ForceTarget(AttackableUnit());
    }
}

// Sau khi đánh: Q reset lên hero (combo/harass) + lưu LoveTap target.
static void OnAfterAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }

    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (ValidUnit(targetBase)) {
        const OrbwalkingMode mode = Orbwalker::ActiveMode();
        if (targetBase.IsHero() && Q.IsReady()) {
            const bool combo = mode == OrbwalkingMode::Combo && Bool(ComboMenu, "useQ");
            const bool harass = mode == OrbwalkingMode::Harass && Bool(HarassMenu, "useQ", false);
            if (combo || harass) {
                Q.CastOnUnit(targetBase);
            }
        }
        LoveTapTargetNetworkId = targetBase.NetworkId();
    }
}

// Huỷ R: khi giữ phím cancel, ra lệnh di chuyển tới con trỏ để ngắt channel.
static void HandleRCancel() {
    if (KeyActive(MiscMenu, "cancelR") && HasRBuff()) {
        CoreControl::IssueMove(Game::CursorPos(), true);
    }
    // TODO SDK: "Block Movement order While Using R" cần inbound order hook
    // (PlayerIssueOrder) mà SDK NightSharp chưa expose — phần chặn di chuyển
    // trong lúc channel R không port được ở tầng plugin.
}

// Q2 bounce: chọn primary target trong Q.Range sao cho longRangeTarget (tối đa
// Q.Range + 450) nằm trong cone 40° phía sau primary → viên đạn nảy trúng.
static bool Q2Logic() {
    if (!Q.IsReady()) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }

    const float q2Range = Q.Range + 450.0f;
    const auto longRangeTarget = GetTarget(q2Range, DamageType::Physical);
    if (!ValidHeroTarget(longRangeTarget, q2Range)) {
        return false;
    }

    const bool killOnly = Bool(MiscMenu, "q2KillOnly", false);
    const float radian = 3.14159265358979323846f / 180.0f;

    // Ứng viên primary: lính/quái NotAlly + hero địch trong Q.Range.
    std::vector<AIBaseClient> targets;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, Q.Range)) {
            targets.push_back(AIBaseClient(minion.Handle()));
        }
    }
    for (const auto& jungle : GameObjects::Jungle()) {
        if (ValidTarget(jungle, Q.Range)) {
            targets.push_back(AIBaseClient(jungle.Handle()));
        }
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy, Q.Range)) {
            targets.push_back(AIBaseClient(enemy.Handle()));
        }
    }

    std::sort(
        targets.begin(),
        targets.end(),
        [](const AIBaseClient& a, const AIBaseClient& b) {
            return a.Health() < b.Health();
        });

    const Vector3 playerServer = player.ServerPosition();
    const Vector3 longServer = longRangeTarget.ServerPosition();

    for (const auto& target : targets) {
        if (killOnly && !(Q.GetDamage(target) >= target.Health())) {
            continue;
        }

        const Vector3 targetServer = target.ServerPosition();
        const Vector3 direction = targetServer.Extend(playerServer, -500.0f);

        // predict vị trí longRangeTarget (cone check). Thời gian đạn Q tới =
        // khoảng cách / tốc độ + delay, giống bản C# (Prediction.GetPrediction).
        const float travelTime =
            playerServer.Distance(longServer) / std::max(1.0f, Q.Speed) + Q.Delay;
        const auto pred = Prediction::GetPrediction(longRangeTarget, travelTime);

        SDK::SectorPoly cone40(targetServer, direction, 40.0f * radian, 450.0f);
        if (cone40.IsInside(longServer) && cone40.IsInside(pred.GetUnitPosition())) {
            if (longRangeTarget.NetworkId() == LoveTapTargetNetworkId) {
                return Q.CastOnUnit(target);
            }

            // Không có lính khác trong cone gần hơn longRangeTarget → an toàn bounce.
            bool blocked = false;
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (!ValidTarget(minion, q2Range) || minion.NetworkId() == target.NetworkId()) {
                    continue;
                }
                if (cone40.IsInside(minion.Position()) &&
                    targetServer.Distance(longServer) >= targetServer.Distance(minion.Position())) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) {
                return Q.CastOnUnit(target);
            }
        }
    }

    return false;
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::MissFortune
