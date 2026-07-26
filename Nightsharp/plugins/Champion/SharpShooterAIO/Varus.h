#pragma once

// ============================================================================
// SharpShooter AIO — Varus
// Port từ SharpShooterCSHarp/Plugins/Varus.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h. Charged spell theo Spell::SetCharged.
//
// Kỹ năng:
//   Q Piercing Arrow — charged skillshot line. SetCharged(min 250, max 1600,
//                      1.2s). Giữ charge tới khi tầm >= "Q Min Charge" rồi bắn.
//   W Blighted Quiver— on-hit stack (không tự cast; dùng để ưu tiên target có
//                      >=3 stack "varuswdebuff" cho Q/E).
//   E Hail of Arrows — skillshot circle 925, delay 1.0, radius 250.
//   R Chain of Corruption — skillshot line 1200, delay 0.25, width 120 (snare).
//
// Ghi chú port:
//   * IsCharging()/StartCharging()/CurrentRange() từ Spell wrapper. Range charge
//     hiện tại đọc qua Q.CurrentRange() (tương đương _q.Range mid-charge của C#).
//   * Before-attack: chặn auto-attack khi đang charge Q (giữ nhịp charge).
//   * _eLastCastTime gate 1500ms để không spam StartCharging ngay sau khi cast E.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Varus {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1600.0f };
inline Spell W{ SpellSlot::W, FLT_MAX };
inline Spell E{ SpellSlot::E, 925.0f };
inline Spell R{ SpellSlot::R, 1200.0f };

inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;
inline DWORD ELastCastTick = 0;

// ── Q charged: tự track như Xerath. Wrapper Spell::IsCharging()/CurrentRange()
//    phụ thuộc buff-name "VarusQ" + chargedCastedT_ (chỉ set trong OnDoCast);
//    với Varus các mốc này hay lệch → IsCharging() chỉ đúng ~300ms rồi
//    CurrentRange() trả max (bỏ qua "Q Min Charge") hoặc kẹt không nhả.
//    Nên ta tự đặt cờ khi gửi lệnh charge và tự tính range ramp theo tick.
inline bool s_qCharging = false;
inline DWORD s_qChargeStartTick = 0;
inline DWORD s_qChargeReqTick = 0;

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

// Killable rút gọn (loại buff bất tử + so máu/shield vật lý).
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

static float BlightDetonationDamage(const AIBaseClient& target) {
    const auto player = Player();
    const int rank = W.Instance().Level();
    const int stacks = std::clamp(target.GetBuffCount("varuswdebuff"), 0, 3);
    if (!player.IsValid() || !target.IsValid() || rank < 1 || stacks == 0) {
        return 0.0f;
    }
    static const float percent[5] = { 0.03f, 0.035f, 0.04f, 0.045f, 0.05f };
    float rawPerStack = target.MaxHealth() *
        (percent[rank - 1] + 0.00013f * player.AP());
    if (target.IsMinion() && AIMinionClient(target.Handle()).IsJungle()) {
        rawPerStack = std::min(rawPerStack, 120.0f);
    }
    return Damage::CalculateDamage(player, target, DamageType::Magical,
        rawPerStack * static_cast<float>(stacks));
}

// ── Damage tính tay theo wiki (wiki.leagueoflegends.com/en-us/Varus/LoL) ─────
//    KHÔNG dùng Spell::GetDamage. Số trích nguyên văn từ wiki:
//
// Q Piercing Arrow (PHYSICAL, charged skillshot):
//    Min (0 charge) : 53.33/100/146.67/193.33/240 (+ 80% bonus AD)
//    Max (full chg) : 80/150/220/290/360           (+ 120% bonus AD)
//    Charge scaling : damage tăng 0%→+50% theo channel time (1.0x→1.5x);
//                     ratio bonus AD cũng ramp 80%→120% cùng lúc.
//    (không phải +66%; +0%..+67% là mức GIẢM damage theo số enemy trúng.)
//    → Killsteal thường nhả shot đã charge đầy, nên dùng FULL-charge (max)
//      cho check kill: 80/150/220/290/360 + 120% bonus AD.
//
// W Blighted Quiver (MAGIC, on-hit passive — KHÔNG tự cast):
//    On-hit  : 4/13/22/31/40 (+ 15% bonus AD) (+ 25% AP)
//    Per Blight stack (detonate): 3/3.5/4/4.5/5% (+ 1.3% per 100 AP) max HP.
//    Là passive on-hit → không nuke được, không wire killsteal (chỉ ghi chú).
//
// E Hail of Arrows (PHYSICAL, AoE zone):
//    60/90/120/150/180 (+ 90% bonus AD)
//
// R Chain of Corruption (MAGIC, root nuke, 3 rank):
//    150/250/350 (+ 100% AP)
//
// Trả về damage đã trừ giáp/kháng phép qua Damage::CalculateDamage.
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float bonusAd = player.BonusAttackDamage();
    const float ap = player.AP();

    switch (slot) {
    case SpellSlot::Q: {
        const int rank = Q.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        // Full-charge (max) — dùng cho killsteal vì shot thường nhả khi đầy charge.
        static const float base[5] = { 80.0f, 150.0f, 220.0f, 290.0f, 360.0f };
        const float raw = base[rank - 1] + 1.20f * bonusAd;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw) +
               BlightDetonationDamage(target);
    }
    case SpellSlot::E: {
        const int rank = E.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 60.0f, 90.0f, 120.0f, 150.0f, 180.0f };
        const float raw = base[rank - 1] + 0.90f * bonusAd;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw) +
               BlightDetonationDamage(target);
    }
    case SpellSlot::R: {
        const int rank = R.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        // R chỉ 3 rank → clamp index về [0..2].
        static const float base[3] = { 150.0f, 250.0f, 350.0f };
        const int idx = (rank - 1 < 3) ? rank - 1 : 2;
        const float raw = base[idx] + 1.00f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    default:
        // W là on-hit passive, không nuke → 0.
        return 0.0f;
    }
}

// Tìm hero có >=3 stack W ("varuswdebuff") trong tầm cho.
static AIHeroClient GetWDebuffTarget(float range) {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy, range) && enemy.GetBuffCount("varuswdebuff") >= 3) {
            return enemy;
        }
    }
    return AIHeroClient();
}

// ── Q charged (tự track, kiểu Xerath) ───────────────────────────────────────
// Đang trong lúc charge Q? Đọc castContext+0x38 của client (nguồn mà
// InitChargeState ghi và release xoá) thay vì cờ tự track: cờ nội bộ chỉ biết các
// charge do file này mở, nên lệch khi người chơi tự bấm Q hoặc khi charge tự hết.
static bool QIsCharging() {
    return CoreNewCastSpell::IsCharging(static_cast<std::int32_t>(SpellSlot::Q)) ||
           s_qCharging;
}

// Tầm Q Varus theo thời gian charge (giây). Công thức thực tế trong game:
//   t <= 0        → 895  (tầm nền chưa charge)
//   0.25 <= t     → 350 + 1400*(t - 0.25), tăng 1400/giây
//   t >= ~1.06    → cap 1480 (đạt max, không phải 1600)
// Clamp về [ChargedMinRange, kVarQMaxRange].
inline constexpr float kVarQMaxRange = 1625.0f;
static float VarQRangeAtTime(float chargeSec) {
    const float progress = std::clamp(chargeSec / 1.25f, 0.0f, 1.0f);
    return 925.0f + 700.0f * progress;
}

// Tầm Q hiện tại theo thời gian đã charge. Ưu tiên mốc mở charge do chính client
// ghi lại, nên charge do người chơi tự bấm cũng đo đúng; chỉ khi không lấy được
// mốc đó mới quay về tick nội bộ.
static float QCurrentRange() {
    if (!QIsCharging()) {
        return kVarQMaxRange;
    }

    const float chargeStart =
        CoreNewCastSpell::ChargeStartTime(static_cast<std::int32_t>(SpellSlot::Q));
    const float elapsedSec = chargeStart > 0.0f
        ? std::max(0.0f, Game::Time() - chargeStart)
        : static_cast<float>(GetTickCount() - s_qChargeStartTick) / 1000.0f;
    return VarQRangeAtTime(elapsedSec);
}

// Bắt đầu charge Q (gửi UpdateChargedSpell không release). Chống spam bằng gate tick.
static void QStartCharge() {
    if (s_qCharging) {
        return;
    }
    if (s_qChargeReqTick != 0 &&
        static_cast<int>(GetTickCount() - s_qChargeReqTick) <= 400 + Game::Ping()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    player.Spellbook().UpdateChargedSpell(SpellSlot::Q, Game::CursorPos(), false);
    s_qChargeReqTick = GetTickCount();
    s_qChargeStartTick = GetTickCount();
    s_qCharging = true;
}

// Nhả Q charge tới vị trí (release=true) và xóa cờ charge.
static void QReleaseAt(const Vector3& position) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    Vector3 pos = position;
    pos.y = NavMesh::GetHeightForPosition(pos.x, pos.z);
    player.Spellbook().UpdateChargedSpell(SpellSlot::Q, pos, true);
    s_qCharging = false;
    s_qChargeStartTick = 0;
}

// Nhả Q vào predict của target (dùng prediction skillshot). Trả true nếu đã nhả.
static bool QReleaseAtTarget(const AIBaseClient& target) {
    if (!ValidUnit(target)) {
        return false;
    }
    const auto pred = Q.GetPrediction(target);
    if (!HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
        return false;
    }
    QReleaseAt(pred.GetCastPosition());
    return true;
}

// Đồng bộ cờ charge với thực tế game: nếu ta tưởng đang charge nhưng buff
// "VarusQLaunch" đã biến mất sau khi vượt quá thời gian charge tối đa (đã nhả
// hoặc bị hủy), thì reset cờ để không kẹt. Gọi mỗi frame.
static void QSyncChargeState() {
    if (!s_qCharging) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        s_qCharging = false;
        s_qChargeStartTick = 0;
        return;
    }
    const int elapsed = static_cast<int>(GetTickCount() - s_qChargeStartTick);
    // Sau grace period (đủ để buff charge xuất hiện, ~ping+300ms), bám theo buff
    // thật "VarusQLaunch": buff biến mất = đã nhả hoặc bị hủy → reset cờ ngay.
    const int grace = 300 + Game::Ping();
    if (elapsed > grace && !player.HasBuff("VarusQLaunch")) {
        s_qCharging = false;
        s_qChargeStartTick = 0;
        return;
    }
    // Chốt chặn cứng: quá ChargeDuration + 1200ms thì coi như hết charge.
    if (elapsed > Q.ChargeDuration + 1200) {
        s_qCharging = false;
        s_qChargeStartTick = 0;
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnProcessSpell(const ProcessSpellEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void AutoKillsteal();
static void Combo();
static void Mixed();
static void Clear();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Varus", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q (charged)"));
    ComboMenu->Add(new MenuSlider("qMinCharge", "Q Min Charge Range", 800, 0, 1600));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("useR", "Use R"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("qMinCharge", "Q Min Charge Range", 1600, 0, 1600));
    HarassMenu->Add(new MenuBool("useE", "Use E", false));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q", false));
    LaneClearMenu->Add(new MenuBool("useE", "Use E", false));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useE", "Use E", false));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("killsteal", "Auto Killsteal (Q/E/R)"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (R/E)"));
    MiscMenu->Add(new MenuBool("interrupter", "Interrupter (R)"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1625.0f);
    Q.SetSkillshot(0.25f, 70.0f, 1500.0f, false, SpellType::Line);
    // Charge tối đa ~1.5s tới cap 1480 (đúng công thức game, không phải 1600).
    Q.SetCharged("VarusQ", "VarusQLaunch", 925, 1625, 1.25f);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, FLT_MAX);

    E = Spell(SpellSlot::E, 925.0f);
    E.SetSkillshot(1.0f, 250.0f, 1750.0f, false, SpellType::Circle);
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 1200.0f);
    R.SetSkillshot(0.25f, 120.0f, 1200.0f, false, SpellType::Line);
    R.DamageType = DamageType::Magical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Varus loaded</font>");
}

static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded || !Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (args.Slot == static_cast<int>(SpellSlot::E)) {
        ELastCastTick = GetTickCount();
    }
}

// Chặn auto-attack khi đang charge Q (giữ nhịp charge, giống args.Process = !IsCharging).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }
    if (QIsCharging()) {
        args.Process = false;
    }
}

// Auto killsteal: chạy mọi mode, gate bằng toggle "killsteal".
// Ưu tiên R (root nuke, tầm xa) → Q (full-charge) → E (AoE zone).
// Damage tính TAY qua SpellDamage (số wiki), KHÔNG dùng Spell::GetDamage.
// Q: dùng full-charge damage vì killsteal thường nhả shot đã charge đầy;
//    nếu đang charge và đủ tầm thì release ngay, chưa charge thì bắt đầu charge.
static void AutoKillsteal() {
    if (!Bool(MiscMenu, "killsteal")) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // R Chain of Corruption: root nuke magic, tầm 1200.
    if (R.IsReady()) {
        const auto target = GetTarget(R.Range, DamageType::Magical);
        if (ValidHeroTarget(target, R.Range) &&
            IsKillable(AIBaseClient(target.Handle()), SpellDamage(SpellSlot::R, AIBaseClient(target.Handle())))) {
            const auto pred = R.GetPrediction(AIBaseClient(target.Handle()));
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                R.Cast(pred.GetCastPosition());
                return;
            }
        }
    }

    // Q Piercing Arrow: charged skillshot. Dùng full-charge damage cho check kill.
    if (Q.IsReady()) {
        const float maxR = static_cast<float>(Q.ChargedMaxRange);
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, maxR)) {
                continue;
            }
            const AIBaseClient base(enemy.Handle());
            if (!IsKillable(base, SpellDamage(SpellSlot::Q, base))) {
                continue;
            }
            if (QIsCharging()) {
                // Đang charge: đủ tầm hiện tại thì nhả ngay vào predict.
                if (ValidHeroTarget(enemy, QCurrentRange()) && QReleaseAtTarget(base)) {
                    return;
                }
            } else {
                // Chưa charge: bắt đầu charge để lần tick sau nhả.
                QStartCharge();
                return;
            }
        }
    }

    // E Hail of Arrows: AoE zone physical, tầm 925.
    if (E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(target, E.Range) &&
            IsKillable(AIBaseClient(target.Handle()), SpellDamage(SpellSlot::E, AIBaseClient(target.Handle())))) {
            E.Cast(target);
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Hoist snapshot EnemyHeroes 1 lần cho cả nhánh useQ-finish và useE-finish (frame đóng băng = y hệt).
    const auto comboHeroes = GameObjects::EnemyHeroes();

    // ── Q charged (tự track charge, kiểu Xerath) ──
    if (Bool(ComboMenu, "useQ") && Q.IsReady()) {
        const float minCharge = static_cast<float>(Slider(ComboMenu, "qMinCharge", 800));
        const float maxR = static_cast<float>(Q.ChargedMaxRange);
        const float curR = QCurrentRange();

        // Ưu tiên finish: có hero killable trong tầm Q max.
        AIHeroClient killable;
        for (const auto& enemy : comboHeroes) {
            if (ValidHeroTarget(enemy, maxR) &&
                IsKillable(AIBaseClient(enemy.Handle()), SpellDamage(SpellSlot::Q, AIBaseClient(enemy.Handle())))) {
                const auto pred = Q.GetPrediction(enemy);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    killable = enemy;
                    break;
                }
            }
        }

        if (killable.IsValid()) {
            // Kẻ có thể kết liễu: đủ tầm là nhả ngay (không cần chờ min-charge).
            if (QIsCharging()) {
                if (ValidHeroTarget(killable, curR)) {
                    QReleaseAtTarget(AIBaseClient(killable.Handle()));
                }
            } else {
                QStartCharge();
            }
        } else if (W.Level() > 0) {
            // Có W: ưu tiên hero >=3 stack; nếu không, giữ charge chờ.
            const auto wTarget = GetWDebuffTarget(maxR);
            if (wTarget.IsValid()) {
                if (QIsCharging()) {
                    if (curR >= minCharge && ValidHeroTarget(wTarget, curR)) {
                        QReleaseAtTarget(AIBaseClient(wTarget.Handle()));
                    }
                } else if ((Bool(ComboMenu, "useE") ? !E.IsReady() : true) &&
                           ELastCastTick + 1500 < GetTickCount()) {
                    QStartCharge();
                }
            } else if (QIsCharging() && curR >= minCharge) {
                const auto target = GetTarget(curR, DamageType::Physical);
                if (ValidHeroTarget(target, curR)) {
                    QReleaseAtTarget(AIBaseClient(target.Handle()));
                }
            }
        } else {
            if (QIsCharging()) {
                if (curR >= minCharge) {
                    const auto target = GetTarget(curR, DamageType::Physical);
                    if (ValidHeroTarget(target, curR)) {
                        QReleaseAtTarget(AIBaseClient(target.Handle()));
                    }
                }
            } else if (ValidHeroTarget(GetTarget(maxR, DamageType::Physical))) {
                QStartCharge();
            }
        }
    }

    // ── E ──
    if (Bool(ComboMenu, "useE") && E.IsReady()) {
        AIHeroClient killable;
        for (const auto& enemy : comboHeroes) {
            if (ValidHeroTarget(enemy, E.Range) &&
                IsKillable(AIBaseClient(enemy.Handle()), SpellDamage(SpellSlot::E, AIBaseClient(enemy.Handle())))) {
                killable = enemy;
                break;
            }
        }
        if (killable.IsValid()) {
            E.Cast(killable);
        } else if (W.Level() > 0) {
            const auto wTarget = GetWDebuffTarget(E.Range);
            if (wTarget.IsValid()) {
                E.Cast(wTarget);
            } else {
                const auto target = GetTarget(E.Range, DamageType::Physical);
                if (ValidHeroTarget(target, E.Range)) {
                    E.CastIfWillHit(AIBaseClient(target.Handle()), 3);
                }
            }
        } else {
            const auto target = GetTarget(E.Range, DamageType::Physical);
            if (ValidHeroTarget(target, E.Range)) {
                E.Cast(target);
            }
        }
    }

    // ── R ──
    if (Bool(ComboMenu, "useR") && R.IsReady()) {
        const auto target = GetTarget(R.Range - 500.0f, DamageType::Magical);
        if (ValidHeroTarget(target, R.Range)) {
            R.Cast(target);
        }
    }
}

static void Mixed() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(HarassMenu, "useQ") && Q.IsReady()) {
        const float maxR = static_cast<float>(Q.ChargedMaxRange);
        const float curR = QCurrentRange();
        if (QIsCharging()) {
            if (curR >= maxR) {
                const auto target = GetTarget(curR, DamageType::Physical);
                if (ValidHeroTarget(target, curR)) {
                    QReleaseAtTarget(AIBaseClient(target.Handle()));
                }
            } else {
                for (const auto& enemy : GameObjects::EnemyHeroes()) {
                    if (ValidHeroTarget(enemy, curR) && IsKillable(enemy, SpellDamage(SpellSlot::Q, AIBaseClient(enemy.Handle())))) {
                        QReleaseAtTarget(AIBaseClient(enemy.Handle()));
                        break;
                    }
                }
            }
        } else if (ManaOkay(Slider(HarassMenu, "Mana", 60)) &&
                   ValidHeroTarget(GetTarget(maxR, DamageType::Physical))) {
            QStartCharge();
        }
    }

    if (Bool(HarassMenu, "useE", false) && E.IsReady() && ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(target, E.Range)) {
            E.Cast(target);
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Lane clear Q: charge tới max rồi bắn hàng lính.
    if (Bool(LaneClearMenu, "useQ", false) && Q.IsReady()) {
        auto minions = GameObjects::EnemyLaneMinions();
        if (minions.empty()) {
            minions = GameObjects::EnemyMinions();
        }
        std::vector<AIBaseClient> targets;
        targets.reserve(minions.size());
        for (const auto& minion : minions) {
            if (ValidTarget(minion, static_cast<float>(Q.ChargedMaxRange))) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        const auto farm = Q.GetLineFarmLocation(targets);
        if (QIsCharging()) {
            if (QCurrentRange() >= static_cast<float>(Q.ChargedMaxRange) && farm.MinionsHit >= 1) {
                QReleaseAt(Vector3::From2D(farm.Position));
            }
        } else if (farm.MinionsHit >= 4 && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
            QStartCharge();
        }
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

    // Jungle clear Q: mob máu cao nhất trong tầm.
    if (Bool(JungleClearMenu, "useQ") && Q.IsReady()) {
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
        if (!mobs.empty()) {
            const auto& mob = mobs.front();
            if (QIsCharging()) {
                if (QCurrentRange() >= static_cast<float>(Q.ChargedMaxRange) && ValidTarget(mob, QCurrentRange())) {
                    QReleaseAtTarget(mob);
                }
            } else if (ManaOkay(Slider(JungleClearMenu, "Mana", 20))) {
                QStartCharge();
            }
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    // Đồng bộ cờ charge tự-track mỗi frame (reset nếu buff VarusQ đã hết).
    QSyncChargeState();

    // Auto killsteal: chạy mọi mode, không phụ thuộc combo. Gate bằng "killsteal".
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

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "gapcloser")) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || args.End.Distance2D(player.Position()) > 200.0f) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (R.IsReady() && ValidHeroTarget(sender, R.Range)) {
        R.Cast(sender);
    } else if (E.IsReady() && ValidHeroTarget(sender, E.Range)) {
        E.Cast(sender);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Varus
