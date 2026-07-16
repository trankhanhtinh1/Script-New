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
#include <cctype>
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

static std::string RuntimeObjectName(const GameObject& object) {
    char name[128] = {};
    if (object.IsValid() &&
        ::Core::Objects::ReadName(object.Address(), name, static_cast<int>(sizeof(name))) && name[0]) {
        return name;
    }
    return object.CharacterName();
}

static bool IsAxeReticle(const GameObject& object) {
    std::string name = RuntimeObjectName(object);
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name.find("draven") != std::string::npos &&
           name.find("q_reticle_self") != std::string::npos;
}

static void TrackAxe(const GameObject& object) {
    if (!object.IsValid() || !IsAxeReticle(object)) {
        return;
    }
    const int id = object.NetworkId();
    const auto found = std::find_if(AxeDrops.begin(), AxeDrops.end(),
        [id](const AxeDrop& axe) { return axe.networkId == id; });
    if (found == AxeDrops.end()) {
        AxeDrops.push_back(AxeDrop{ id, GetTickCount() + 1200, object.Position() });
    }
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

// ── Damage tính tay theo wiki (leagueoflegends.com/en-us/Draven/LoL) ──
// KHÔNG dùng Spell::GetDamage. Draven KHÔNG có tỉ lệ AP; mọi kỹ năng scale bonus AD.
//
// Q Spinning Axe   (physical, on-hit): 40/45/50/55/60 + 75/85/95/105/115% bonus AD
//   → là bonus cộng thẳng vào đòn đánh thường kế tiếp (auto modifier), không phải
//     một instance sát thương độc lập. SpellDamage(Q) trả về phần bonus của rìu.
//     Vì vậy Q KHÔNG dùng cho killsteal độc lập (không tự bay tới địch).
// W Blood Rush     : không sát thương (chỉ tốc chạy + tốc đánh + ghosting).
// E Stand Aside    (physical, area)  : 75/110/145/180/215 + 50% bonus AD. Không AP ratio.
// R Whirling Death (physical, 3 rank): 200/300/400 + 110/130/150% bonus AD MỖI LƯỢT.
//   → rìu bay đi rồi có thể quay về (recast); "hit only once per pass" nên một mục
//     tiêu nằm trong đường bay ăn tối đa 2 lần (đi + về) = damage-per-pass × 2.
//     Killsteal giữ nguyên intent × 2.0. (Wiki tổng cả 2 lượt: 400/600/800
//     + 220/260/300% bonus AD; lượt về giảm dần theo số địch trúng 100%→50%.)
// Trả về damage đã trừ giáp qua Damage::CalculateDamage.
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float bonusAd = player.BonusAttackDamage();

    switch (slot) {
    case SpellSlot::Q: {
        const int rank = Q.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 40.0f, 45.0f, 50.0f, 55.0f, 60.0f };
        static const float ratio[5] = { 0.75f, 0.85f, 0.95f, 1.05f, 1.15f };
        const float raw = base[rank - 1] + ratio[rank - 1] * bonusAd;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    case SpellSlot::E: {
        const int rank = E.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 75.0f, 110.0f, 145.0f, 180.0f, 215.0f };
        const float raw = base[rank - 1] + 0.50f * bonusAd;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    case SpellSlot::R: {
        const int rank = R.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[3] = { 200.0f, 300.0f, 400.0f };
        static const float ratio[3] = { 1.10f, 1.30f, 1.50f };
        const int idx = (rank - 1 < 3) ? rank - 1 : 2;
        const float raw = base[idx] + ratio[idx] * bonusAd;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    default:
        return 0.0f;
    }
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
static void AutoKillsteal();
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
    MiscMenu->Add(new MenuBool("killsteal", "Auto Killsteal (E/R)"));
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
    TrackAxe(object);
}

static void OnDeleteObject(const GameObject& object) {
    if (!Loaded) {
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
    // Hoist snapshot AllGameObjects 1 lần (không copy lại mỗi rìu): frame đã
    // đóng băng nên list object không đổi giữa các vòng = kết quả y hệt.
    const auto allObjs = GameObjects::AllGameObjects();
    for (const auto& obj : allObjs) {
        TrackAxe(obj);
    }
    for (auto& axe : AxeDrops) {
        for (const auto& obj : allObjs) {
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

    // Hoist snapshot EnemyHeroes 1 lần cho cả nhánh useW và useR (frame đóng băng = y hệt).
    const auto comboHeroes = GameObjects::EnemyHeroes();

    // W: khi có địch trong tầm đánh thường và chưa có buff tốc chạy.
    if (Bool(ComboMenu, "useW") && W.IsReady() && !player.HasBuff("dravenfurybuff")) {
        for (const auto& enemy : comboHeroes) {
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
        for (const auto& enemy : comboHeroes) {
            if (!ValidHeroTarget(enemy, R.Range) || AutoAttack::InAutoAttackRange(enemy)) {
                continue;
            }
            if (IsKillable(enemy, SpellDamage(SpellSlot::R, enemy) * 2.0)) {
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

    // Auto killsteal: chạy mọi mode, không phụ thuộc combo.
    AutoKillsteal();

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

// Auto killsteal: R (toàn cầu 2500, ×2 vì rìu đi + về) → E (đẩy, physical).
// Q Spinning Axe là on-hit modifier, không tự bay tới địch → không dùng killsteal.
static void AutoKillsteal() {
    if (!Bool(MiscMenu, "killsteal")) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // R Whirling Death: rìu bay đi + quay về ăn tối đa 2 lần → damage-per-pass × 2.
    if (R.IsReady()) {
        const auto target = GetTarget(R.Range, DamageType::Physical);
        if (ValidHeroTarget(target, R.Range) &&
            IsKillable(target, SpellDamage(SpellSlot::R, target) * 2.0)) {
            const auto pred = R.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                R.Cast(pred.GetCastPosition());
                return;
            }
        }
    }

    // E Stand Aside: skillshot line 1000.
    if (E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(target, E.Range) &&
            IsKillable(target, SpellDamage(SpellSlot::E, target))) {
            const auto pred = E.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                E.Cast(pred.GetCastPosition());
                return;
            }
        }
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
