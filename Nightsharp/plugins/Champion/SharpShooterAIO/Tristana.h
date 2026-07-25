#pragma once

// ============================================================================
// SharpShooter AIO — Tristana
// Port từ SharpShooterCSHarp/Plugins/Tristana.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Rapid Fire      — self-buff, cast trên before-attack khi trong tầm AA.
//   W Rocket Jump     — skillshot circle 1170, delay 0.5, radius 270 (chỉ vẽ range).
//   E Explosive Charge— targeted, tầm = AA range + 65, gây nổ stack.
//   R Buster Shot     — targeted knockback, tầm = AA range + 65.
//
// Ghi chú port:
//   * Menu E-targets per-enemy của bản C# được rút gọn thành 1 toggle "useE"
//     (tối giản kiểu 7UP), vẫn giữ logic chọn mục tiêu ưu tiên trong tầm E.
//   * IsWillDieByTristanaE / IsKillableAndValidTarget port thành helper cục bộ.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <string>

namespace Plugins::SharpAIO::Tristana {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, FLT_MAX };
inline Spell W{ SpellSlot::W, 1170.0f };
inline Spell E{ SpellSlot::E, 550.0f };
inline Spell R{ SpellSlot::R, 550.0f };

inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;

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

static float HealthRegenRate(const AIBaseClient& unit) {
    return unit.IsValid() ? ::CoreAIHeroClient::HealthRegenRate(unit.Address()) : 0.0f;
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

// Tầm auto-attack thực + 65 (E/R của Tristana bằng tầm đánh thường + 65).
static float AutoAttackRangePlus65() {
    const auto player = Player();
    if (!player.IsValid()) {
        return 550.0f;
    }
    return AutoAttack::GetRealAutoAttackRange(player, AttackableUnit()) + 65.0f;
}

// Port rút gọn của ExtraExtensions.IsKillableAndValidTarget: loại các buff bất
// tử phổ biến rồi so máu + hồi máu + shield với sát thương.
static bool IsKillable(const AIBaseClient& target, double calculatedDamage, DamageType damageType) {
    if (!ValidUnit(target)) {
        return false;
    }

    if (target.HasBuff("kindredrnodeathbuff") ||
        target.HasBuff("Undying Rage") ||
        target.HasBuff("JudicatorIntervention") ||
        target.HasBuff("BansheesVeil") ||
        target.HasBuff("SivirShield") ||
        target.HasBuff("ShroudofDarkness")) {
        return false;
    }

    const auto player = Player();
    if (player.IsValid() && player.HasBuff("summonerexhaust")) {
        calculatedDamage *= 0.6;
    }

    if (target.HasBuff("FerociousHowl")) {
        calculatedDamage *= 0.3;
    }

    const float shield = damageType == DamageType::Physical
        ? target.PhysicalShield()
        : target.MagicalShield();
    return target.Health() + HealthRegenRate(target) + shield < calculatedDamage - 2.0;
}

// ── Damage tính tay theo wiki (leagueoflegends.com) — KHÔNG dùng GetSpellDamage ──
// Q Rapid Fire       : chỉ tăng tốc đánh (60/75/90/105/120%), KHÔNG gây damage.
// W Rocket Jump (MAGIC):
//     "Magic Damage: 70 / 105 / 140 / 175 / 210 (+ 100% bonus AD) (+ 50% AP)"
// E Explosive Charge (PHYSICAL, kích nổ theo stack):
//     base 0 stack : "60 / 85 / 110 / 135 / 160 (+ 80% bonus AD) (+ 50% AP)"
//     mỗi stack    : "15 / 21.25 / 27.5 / 33.75 / 40 (+ 20% bonus AD) (+ 12.5% AP)"
//     tối đa 4 stack (từ đánh thường/kỹ năng) → full 4 stack:
//     "120 / 170 / 220 / 270 / 320 (+ 160% bonus AD) (+ 100% AP)"
//     (60+4*15=120; 80%+4*20%=160% bonus AD; 50%+4*12.5%=100% AP — khớp wiki).
//     Số stack đọc từ buff "tristanaecharge" (clamp 0..4); 0 stack = E cơ bản.
// R Buster Shot (MAGIC, 3 rank):
//     "Magic Damage: 225 / 275 / 325 (+ 70% bonus AD) (+ 100% AP)"
// Trả về damage đã trừ giáp/kháng phép qua Damage::CalculateDamage.
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float bonusAd = player.BonusAttackDamage();
    const float ap = player.AP();

    switch (slot) {
    case SpellSlot::W: {
        const int rank = W.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 70.0f, 105.0f, 140.0f, 175.0f, 210.0f };
        const float raw = base[rank - 1] + 1.00f * bonusAd + 0.50f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    case SpellSlot::E: {
        const int rank = E.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 60.0f, 85.0f, 110.0f, 135.0f, 160.0f };
        static const float perStack[5] = { 15.0f, 21.25f, 27.5f, 33.75f, 40.0f };
        int stacks = target.GetBuffCount("tristanaecharge");
        if (stacks < 0) {
            stacks = 0;
        }
        if (stacks > 4) {
            stacks = 4;
        }
        const float raw =
            base[rank - 1] + 0.80f * bonusAd + 0.50f * ap +
            static_cast<float>(stacks) *
                (perStack[rank - 1] + 0.20f * bonusAd + 0.125f * ap);
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    case SpellSlot::R: {
        const int rank = R.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        // R chỉ có 3 rank — clamp index về [0,2].
        static const float base[3] = { 225.0f, 275.0f, 325.0f };
        const int idx = (rank - 1 < 3) ? rank - 1 : 2;
        const float raw = base[idx] + 0.70f * bonusAd + 1.00f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    default:
        return 0.0f;
    }
}

// E detonation damage: SpellDamage đã tự đọc số stack ("tristanaecharge") của mục
// tiêu, nên chỉ cần gọi lại. Với mục tiêu đã dính đủ stack → mô hình full-stack.
static double TristanaEDamage(const AIBaseClient& target) {
    return static_cast<double>(SpellDamage(SpellSlot::E, target));
}

static bool WillDieByTristanaE(const AIBaseClient& target) {
    if (!target.HasBuff("tristanaecharge")) {
        return false;
    }
    return IsKillable(target, TristanaEDamage(target), DamageType::Physical);
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnPlayAnimation(const PlayAnimationEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void OnAfterAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void Combo();
static void Mixed();
static void Clear();
static void KillSteal();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Tristana", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("useR", "Use R (finisher)"));
    ComboMenu->Add(new MenuBool("useRE", "Use R to detonate E on grouped enemies", false));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useE", "Use E"));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 0, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useE", "Use E"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("killsteal", "Killsteal (R)"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (R)"));
    MiscMenu->Add(new MenuBool("interrupter", "Interrupter (R)"));
    MiscMenu->Add(new MenuBool("autoETurret", "Auto E on Turret"));
    MiscMenu->Add(new MenuBool("antiRengar", "Anti-Rengar (R on leap)", true));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, FLT_MAX);

    W = Spell(SpellSlot::W, 1170.0f);
    W.SetSkillshot(0.5f, 270.0f, 1500.0f, false, SpellType::Circle);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 550.0f);
    E.DamageType = DamageType::Physical;
    R = Spell(SpellSlot::R, 550.0f);
    R.DamageType = DamageType::Physical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;
    Events::hook.OnPlayAnimation += &OnPlayAnimation;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Tristana loaded</font>");
}

// Q Rapid Fire: bật trước khi đánh thường (combo, hoặc jungle mob khi laneclear).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }

    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(targetBase) || !AutoAttack::InAutoAttackRange(targetBase)) {
        return;
    }

    if (targetBase.IsTurret() && Bool(MiscMenu, "autoETurret") && E.IsReady()) {
        E.CastOnUnit(targetBase);
    }

    if (!Q.IsReady()) {
        return;
    }

    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo) {
        if (Bool(ComboMenu, "useQ")) {
            Q.Cast();
        }
    } else if (mode == OrbwalkingMode::LaneClear) {
        if (Bool(JungleClearMenu, "useQ") && targetBase.IsMinion() &&
            targetBase.Team() == GameObjectTeam::Neutral) {
            Q.Cast();
        }
    }
}

// Dựng AIHeroClient từ ObjectInfo (named-field, không phụ thuộc thứ tự struct).
static AIHeroClient HeroFromInfo(const ::Core::Events::ObjectInfo& info) {
    ::Core::Objects::ObjectHandle handle{};
    handle.address = info.Ptr;
    handle.index = info.Index;
    handle.networkId = info.NetworkId;
    handle.type = info.Type;
    return AIHeroClient(handle);
}

// Anti-Rengar: Rengar nhảy (animation "Spell5") → R hất văng cắt combo lao vào.
static void OnPlayAnimation(const PlayAnimationEventArgs& args) {
    if (!Loaded || !Bool(MiscMenu, "antiRengar") || !R.IsReady()) {
        return;
    }
    if (std::strcmp(args.Animation, "Spell5") != 0) {
        return;
    }
    const AIHeroClient sender = HeroFromInfo(args.Sender);
    if (!sender.IsValid() || !sender.IsHero() || !sender.IsEnemy()) {
        return;
    }
    if (_stricmp(sender.CharacterName().c_str(), "Rengar") != 0) {
        return;
    }
    if (ValidHeroTarget(sender, R.Range)) {
        R.CastOnUnit(AIBaseClient(sender.Handle()));
    }
}

// Auto E lên trụ địch sau khi đánh thường.
static void OnAfterAttack(OrbwalkingActionArgs&) {}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
        return;
    }

    // E/R có tầm phụ thuộc tầm đánh thường hiện tại.
    const float castRange = AutoAttackRangePlus65();
    E.Range = castRange;
    R.Range = castRange;

    // Auto killsteal: chạy đầu mỗi tick, mọi mode (ưu tiên trước combo).
    KillSteal();

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

static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }

    // Snapshot 1 lần: GameObjects::EnemyHeroes() copy cả vector dưới mutex mỗi lần
    // gọi, nên vòng lặp lồng (RE combo) và lần quét R trước đây trả nhiều bản
    // copy/frame. Snapshot là cache đã đóng băng nên tái dùng cho kết quả y hệt.
    const auto comboEnemies = GameObjects::EnemyHeroes();

    if (Bool(ComboMenu, "useE") && E.IsReady()) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(target, E.Range)) {
            E.CastOnUnit(target);
        }
    }

    // RE combo (C# ComboUseRE): target đã dính >=3 E-stack và đứng gần địch khác →
    // R hất target vào cụm để E kích nổ lan (AoE). PosAfterR = đẩy ra xa 1000u theo
    // hướng player→target; nếu có >=1 địch quanh điểm đó thì R để lan nổ.
    if (Bool(ComboMenu, "useRE", false) && R.IsReady() && !Player().Spellbook().IsWindingUp()) {
        for (const auto& enemy : comboEnemies) {
            if (!ValidHeroTarget(enemy, R.Range) || enemy.GetBuffCount("tristanaecharge") < 3) {
                continue;
            }
            // Chỉ khi E đủ giết (4 stack) — R chỉ để lan nổ, không phí ult vô ích.
            const auto player = Player();
            const double eFull = SpellDamage(SpellSlot::E, enemy) +
                (W.IsReady() ? Damage::GetAutoAttackDamage(player, enemy) * 2.0 : 0.0);
            if (static_cast<double>(enemy.Health()) >= eFull) {
                continue;
            }
            const Vector3 posAfterR = player.Position().Extend(enemy.ServerPosition(), 1000.0f);
            int nearby = 0;
            for (const auto& other : comboEnemies) {
                if (other.NetworkId() != enemy.NetworkId() && !other.IsDead() &&
                    other.Distance(posAfterR) <= 300.0f) {
                    ++nearby;
                }
            }
            if (nearby >= 1) {
                R.CastOnUnit(AIBaseClient(enemy.Handle()));
                return;
            }
        }
    }

    if (Bool(ComboMenu, "useR") && R.IsReady()) {
        // R chỉ dùng để finish: mục tiêu chết bởi R và không chết sẵn bởi E.
        AIHeroClient best;
        float bestHealth = -1.0f;
        for (const auto& enemy : comboEnemies) {
            if (!ValidHeroTarget(enemy, R.Range)) {
                continue;
            }
            if (WillDieByTristanaE(enemy)) {
                continue;
            }
            if (IsKillable(enemy, SpellDamage(SpellSlot::R, enemy), DamageType::Magical) &&
                enemy.Health() > bestHealth) {
                best = enemy;
                bestHealth = enemy.Health();
            }
        }
        if (best.IsValid()) {
            R.CastOnUnit(best);
        }
    }
}

static void Mixed() {
    if (!Bool(HarassMenu, "useE") || !E.IsReady() || !ManaOkay(Slider(HarassMenu, "Mana", 0))) {
        return;
    }

    const auto target = GetTarget(E.Range, DamageType::Physical);
    if (ValidHeroTarget(target, E.Range)) {
        E.CastOnUnit(target);
    }
}

static void Clear() {
    if (!Bool(JungleClearMenu, "useE") || !E.IsReady() || !ManaOkay(Slider(JungleClearMenu, "Mana", 20))) {
        return;
    }

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

    for (const auto& mob : mobs) {
        if (ValidTarget(mob, E.Range)) {
            E.CastOnUnit(mob);
            return;
        }
    }
}

// Auto killsteal — chạy mỗi tick, mọi mode (gọi từ Game_OnUpdate), gated bởi
// toggle "killsteal". Damage tính tay qua SpellDamage (KHÔNG dùng GetDamage).
// Thứ tự: E (kích nổ stack, không tốn ult) → R (nuke đơn mục tiêu, chính) → W.
static void KillSteal() {
    if (!Bool(MiscMenu, "killsteal")) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // E Explosive Charge: chỉ khi mục tiêu đã dính stack "tristanaecharge".
    // SpellDamage tự đọc số stack → mô hình sát thương kích nổ theo stack hiện có.
    if (E.IsReady()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, E.Range)) {
                continue;
            }
            if (WillDieByTristanaE(enemy)) {
                E.CastOnUnit(enemy);
                return;
            }
        }
    }

    // R Buster Shot (MAGIC): finisher đơn mục tiêu — ưu tiên mục tiêu máu cao nhất
    // vẫn hạ được, bỏ qua ai đã chết chắc bởi E.
    if (R.IsReady()) {
        AIHeroClient best;
        float bestHealth = -1.0f;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, R.Range) || WillDieByTristanaE(enemy)) {
                continue;
            }
            if (IsKillable(enemy, SpellDamage(SpellSlot::R, enemy), DamageType::Magical) &&
                enemy.Health() > bestHealth) {
                best = enemy;
                bestHealth = enemy.Health();
            }
        }
        if (best.IsValid()) {
            R.CastOnUnit(best);
            return;
        }
    }

    // W Rocket Jump (MAGIC): killsteal phụ khi E/R chưa sẵn hoặc chưa đủ. W là
    // jump, cast tới vị trí mục tiêu (skillshot circle) — chỉ dùng khi hạ gục được.
    if (W.IsReady()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, W.Range)) {
                continue;
            }
            if (IsKillable(enemy, SpellDamage(SpellSlot::W, enemy), DamageType::Magical)) {
                W.Cast(enemy);
                return;
            }
        }
    }
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "gapcloser") || !R.IsReady()) {
        return;
    }

    const auto sender = AIHeroClient(args.Sender);
    if (!ValidHeroTarget(sender, R.Range)) {
        return;
    }

    const auto player = Player();
    if (player.IsValid() && args.End.Distance2D(player.Position()) <= 200.0f) {
        R.CastOnUnit(sender);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;
    Events::hook.OnPlayAnimation -= &OnPlayAnimation;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Tristana
