#pragma once

// ============================================================================
// SharpShooter AIO — Draven
// Port từ SharpShooterCSHarp/Plugins/Draven.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Spinning Axe   — on-hit buff, cast trước khi đánh thường khi số rìu < 2.
//   W Blood Rush     — tốc chạy/tốc đánh; cast khi vào tầm đánh thường (nếu chưa
//                      có buff "dravenfurybuff") hoặc để bắt rìu sắp rơi.
//   E Stand Aside    — skillshot line 1000, delay 0.25, width 130, speed 1400.
//   R Whirling Death — skillshot line toàn cầu 2500. Finish mục tiêu killable.
//
// Cơ chế bắt rìu (Spinning Axe):
//   Rìu rơi tạo object "Draven_Base_Q_reticle_self.troy". Track qua
//   GameObjects::OnCreate/OnDelete với ExpireTime = now + 1200ms. Khi bật
//   "Auto Catch Axe", lái điểm orbwalk (SetOrbwalkerPosition) tới rìu gần con
//   trỏ nhất; nếu sắp hết hạn thì cast W để chạy nhanh tới.
//
// Ghi chú port:
//   * AxeCount = buff "dravenspinningattack".Count + số reticle đang track.
//   * Bản C# còn chặn/điều hướng lệnh MoveTo qua PlayerIssueOrder để pathing
//     đúng chỗ rìu rơi. SDK NightSharp chưa có inbound order hook →
//     // TODO SDK: không port phần chỉnh MoveTo; bù lại dùng SetOrbwalkerPosition.
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreControl.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Draven {

using SDK::Core::Utils::AutoAttack;

inline const char* const kReticleName = "Draven_Base_Q_reticle_self.troy";

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, FLT_MAX };
inline Spell W{ SpellSlot::W, FLT_MAX };
inline Spell E{ SpellSlot::E, 1000.0f };
inline Spell R{ SpellSlot::R, 2500.0f };

inline bool Loaded = false;

// Rìu đang rơi: networkId + hạn dùng (ms) + vị trí lần cuối thấy.
struct AxeDrop {
    int networkId = 0;
    DWORD expireTick = 0;
    Vector3 position{};
};
inline std::vector<AxeDrop> AxeDrops;
inline int BestAxeNetworkId = 0;

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

static bool IsKillable(const AIBaseClient& target, double damage) {
    if (!ValidUnit(target)) {
        return false;
    }
    if (target.HasBuff("kindredrnodeathbuff") || target.HasBuff("Undying Rage") ||
        target.HasBuff("JudicatorIntervention") || target.HasBuff("BansheesVeil") ||
        target.HasBuff("SivirShield") || target.HasBuff("ShroudofDarkness")) {
        return false;
    }
    return target.Health() + target.PhysicalShield() < damage - 2.0;
}

// Số rìu đang có: buff dravenspinningattack (số charge) + reticle đang track.
static int AxeCount() {
    const auto player = Player();
    const int buffCount = player.IsValid() ? player.GetBuffCount("dravenspinningattack") : 0;
    return buffCount + static_cast<int>(AxeDrops.size());
}

static void PurgeExpiredAxes() {
    const DWORD now = GetTickCount();
    AxeDrops.erase(
        std::remove_if(
            AxeDrops.begin(),
            AxeDrops.end(),
            [now](const AxeDrop& a) { return now > a.expireTick + 400; }),
        AxeDrops.end());
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnCreateObject(const GameObject& object);
static void OnDeleteObject(const GameObject& object);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void AutoCatchAxe();
static void Combo();
static void Mixed();
static void Clear();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Draven", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q (spin axe)"));
    ComboMenu->Add(new MenuBool("useW", "Use W (blood rush)"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuSlider("eMana", "Min Mana % to use E", 20, 0, 100));
    ComboMenu->Add(new MenuBool("useR", "Use R (finisher)"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuBool("useW", "Use W", false));
    HarassMenu->Add(new MenuBool("useE", "Use E", false));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q", false));
    LaneClearMenu->Add(new MenuBool("useE", "Use E", false));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useE", "Use E"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (E)"));
    MiscMenu->Add(new MenuBool("interrupter", "Interrupter (E)"));
    MiscMenu->Add(new MenuBool("autoCatch", "Auto Catch Axe"));
    MiscMenu->Add(new MenuSlider("catchRange", "Axe Catch Range", 600, 0, 2000));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, FLT_MAX);
    W = Spell(SpellSlot::W, FLT_MAX);

    E = Spell(SpellSlot::E, 1000.0f);
    E.SetSkillshot(0.25f, 130.0f, 1400.0f, false, SpellType::Line);
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 2500.0f);
    R.SetSkillshot(0.40f, 160.0f, 2000.0f, true, SpellType::Line);
    R.DamageType = DamageType::Physical;

    AxeDrops.clear();
    BestAxeNetworkId = 0;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;
    GameObjects::AddOnCreate(&OnCreateObject);
    GameObjects::AddOnDelete(&OnDeleteObject);

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Draven loaded</font>");
}

// Rìu rơi tạo reticle → bắt đầu track (hạn 1200ms).
static void OnCreateObject(const GameObject& object) {
    if (!Loaded || !object.IsValid()) {
        return;
    }
    if (object.Name() == kReticleName) {
        AxeDrops.push_back(AxeDrop{ object.NetworkId(), GetTickCount() + 1200, object.Position() });
    }
}

static void OnDeleteObject(const GameObject& object) {
    if (!Loaded || !object.IsValid()) {
        return;
    }
    if (object.Name() != kReticleName) {
        return;
    }
    const int id = object.NetworkId();
    AxeDrops.erase(
        std::remove_if(
            AxeDrops.begin(),
            AxeDrops.end(),
            [id](const AxeDrop& a) { return a.networkId == id; }),
        AxeDrops.end());
}

// Bắt rìu: lái điểm orbwalk tới rìu gần con trỏ nhất trong tầm bắt; cast W nếu
// không kịp tới trước khi rìu hết hạn.
static void AutoCatchAxe() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (!Bool(MiscMenu, "autoCatch")) {
        Orbwalker::SetOrbwalkerPosition(Game::CursorPos());
        BestAxeNetworkId = 0;
        return;
    }

    // Cập nhật vị trí reticle đang track từ object hiện tại.
    for (auto& axe : AxeDrops) {
        for (const auto& obj : GameObjects::AllGameObjects()) {
            if (obj.IsValid() && obj.NetworkId() == axe.networkId) {
                axe.position = obj.Position();
                break;
            }
        }
    }

    const float catchRange = static_cast<float>(Slider(MiscMenu, "catchRange", 600));
    const Vector3 cursor = Game::CursorPos();

    const AxeDrop* best = nullptr;
    for (const auto& axe : AxeDrops) {
        if (cursor.Distance(axe.position) > catchRange) {
            continue;
        }
        if (!best || axe.expireTick < best->expireTick) {
            best = &axe;
        }
    }

    if (!best) {
        BestAxeNetworkId = 0;
        Orbwalker::SetOrbwalkerPosition(cursor);
        return;
    }

    BestAxeNetworkId = best->networkId;

    // Ước lượng thời gian chạy tới rìu; nếu không kịp thì bật W.
    const float travelMs =
        player.MoveSpeed() > 1.0f
            ? player.Position().Distance(best->position) / player.MoveSpeed() * 1000.0f
            : FLT_MAX;
    if (static_cast<float>(GetTickCount()) + travelMs >= static_cast<float>(best->expireTick) &&
        W.IsReady() && !player.HasBuff("dravenfurybuff")) {
        const OrbwalkingMode mode = Orbwalker::ActiveMode();
        if ((mode == OrbwalkingMode::Combo && Bool(ComboMenu, "useW")) ||
            (mode == OrbwalkingMode::Harass && Bool(HarassMenu, "useW", false))) {
            W.Cast();
        }
    }

    // Lái tới rìu (hoặc giữ nhịp orbwalk khi đã sát rìu).
    if (best->position.Distance(player.Position()) < 120.0f) {
        Orbwalker::SetOrbwalkerPosition(cursor);
    } else {
        Orbwalker::SetOrbwalkerPosition(best->position);
    }
}

// Q Spinning Axe: nạp rìu trước khi đánh thường khi số rìu < 2.
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded || !Q.IsReady() || AxeCount() >= 2) {
        return;
    }

    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(targetBase)) {
        return;
    }

    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo) {
        if (targetBase.IsHero() && Bool(ComboMenu, "useQ")) {
            Q.Cast();
        }
    } else if (mode == OrbwalkingMode::Harass) {
        if (targetBase.IsHero() && Bool(HarassMenu, "useQ") && ManaOkay(Slider(HarassMenu, "Mana", 60))) {
            Q.Cast();
        }
    } else if (mode == OrbwalkingMode::LaneClear) {
        const bool lane = Bool(LaneClearMenu, "useQ", false) && ManaOkay(Slider(LaneClearMenu, "Mana", 60));
        const bool jungle = Bool(JungleClearMenu, "useQ") && targetBase.Team() == GameObjectTeam::Neutral &&
            ManaOkay(Slider(JungleClearMenu, "Mana", 20));
        if (lane || jungle) {
            Q.Cast();
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // W: khi có địch trong tầm đánh thường và chưa có buff tốc chạy.
    if (Bool(ComboMenu, "useW") && W.IsReady() && !player.HasBuff("dravenfurybuff")) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy) && AutoAttack::InAutoAttackRange(enemy)) {
                W.Cast();
                break;
            }
        }
    }

    // E: đẩy/ngắt.
    if (Bool(ComboMenu, "useE") && E.IsReady() && ManaOkay(Slider(ComboMenu, "eMana", 20))) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(target, E.Range)) {
            const auto pred = E.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                E.Cast(pred.GetCastPosition());
            }
        }
    }

    // R: finish mục tiêu ngoài tầm đánh, killable với 2 lần R.
    if (Bool(ComboMenu, "useR") && R.IsReady()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, R.Range) || AutoAttack::InAutoAttackRange(enemy)) {
                continue;
            }
            if (IsKillable(enemy, R.GetDamage(enemy) * 2.0)) {
                const auto pred = R.GetPrediction(enemy);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    R.Cast(pred.GetCastPosition());
                    break;
                }
            }
        }
    }
}

static void Mixed() {
    const auto player = Player();
    if (!player.IsValid() || !ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        return;
    }

    if (Bool(HarassMenu, "useW", false) && W.IsReady() && !player.HasBuff("dravenfurybuff")) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy) && AutoAttack::InAutoAttackRange(enemy)) {
                W.Cast();
                break;
            }
        }
    }

    if (Bool(HarassMenu, "useE", false) && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(target, E.Range)) {
            const auto pred = E.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
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

    // Lane clear E: bắn hàng lính (>=3).
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
            const auto farm = E.GetLineFarmLocation(targets);
            if (farm.MinionsHit >= 3) {
                E.Cast(Vector3::From2D(farm.Position));
            }
        }
    }

    // Jungle clear E: mob máu cao nhất trong tầm.
    if (Bool(JungleClearMenu, "useE") && E.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20))) {
        auto mobs = GameObjects::Jungle();
        mobs.erase(
            std::remove_if(
                mobs.begin(),
                mobs.end(),
                [](const AIMinionClient& mob) {
                    return !ValidTarget(mob, E.Range) || mob.IsPlant() || mob.IsPet();
                }),
            mobs.end());
        std::sort(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& a, const AIMinionClient& b) {
                return a.MaxHealth() > b.MaxHealth();
            });
        if (!mobs.empty()) {
            E.Cast(mobs.front().Position());
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    PurgeExpiredAxes();
    AutoCatchAxe();

    if (Game::IsChatOpen()) {
        return;
    }

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
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "gapcloser") || !E.IsReady()) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (ValidHeroTarget(sender, E.Range)) {
        E.Cast(sender);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;
    GameObjects::RemoveOnCreate(&OnCreateObject);
    GameObjects::RemoveOnDelete(&OnDeleteObject);

    AxeDrops.clear();
    BestAxeNetworkId = 0;
    Loaded = false;
}

} // namespace Plugins::SharpAIO::Draven
