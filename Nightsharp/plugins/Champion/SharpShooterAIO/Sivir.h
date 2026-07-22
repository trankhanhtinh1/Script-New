#pragma once

// ============================================================================
// SharpShooter AIO — Sivir
// Port từ SharpShooterCSHarp/Plugins/Sivir.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Boomerang Blade — skillshot line 1250, delay 0.25, width 90, speed 1350.
//   W Ricochet        — on-hit buff, cast sau đòn đánh thường (combo/laneclear).
//   E Spell Shield    — auto chặn kỹ năng targeted của địch (OnProcessSpell).
//   R On The Hunt     — buff tốc chạy (chỉ vẽ range, không auto).
//
// Ghi chú port:
//   * DelayAction humanizer của bản C# không có tương đương trong SDK →
//     E cast ngay lập tức khi phát hiện spell targeted. Toggle "humanizer"
//     giữ trong menu nhưng chỉ mang tính tương thích (// TODO SDK: no DelayAction).
//   * W cast sau đòn đánh dùng OnAfterAttack (tương đương OnSpellCast auto-attack
//     trong bản C#).
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Sivir {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1250.0f };
inline Spell W{ SpellSlot::W, FLT_MAX };
inline Spell E{ SpellSlot::E, FLT_MAX };
inline Spell R{ SpellSlot::R, 1000.0f };

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

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static AIHeroClient HeroFromInfo(const ::Core::Events::ObjectInfo& info) {
    ::Core::Objects::ObjectHandle handle{};
    handle.address = info.Ptr;
    handle.index = info.Index;
    handle.networkId = info.NetworkId;
    handle.type = info.Type;
    return AIHeroClient(handle);
}

// Killsteal: loại buff bất tử phổ biến rồi so máu + shield với damage tính được.
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

// ── Damage tính tay theo wiki (leagueoflegends.com) — KHÔNG dùng DamageData ──
// Q Boomerang Blade (physical): 60/85/110/135/160 + 70% bonus AD + 60% AP
//   Boomerang gây damage giảm dần mỗi mục tiêu bị đánh trúng (đi ra) và có
//   phiên bản "return" tối thiểu, nhưng cho killsteal ta dùng full first-hit.
// W Ricochet     : on-hit bounce (sửa đòn đánh thường), không phải nuke đơn lẻ.
// E Spell Shield : không gây damage (chỉ chặn + heal).
// R On The Hunt  : không gây damage trực tiếp (buff tốc chạy + giảm hồi chiêu).
// → Chỉ Q là spell killsteal. Các slot khác trả về 0.
// Trả về damage đã trừ giáp qua Damage::CalculateDamage.
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    switch (slot) {
    case SpellSlot::Q: {
        const int rank = Q.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        const float bonusAd = player.BonusAttackDamage();
        const float ap = player.AP();
        static const float base[5] = { 60.0f, 85.0f, 110.0f, 135.0f, 160.0f };
        const float raw = base[rank - 1] + 0.70f * bonusAd + 0.60f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    default:
        return 0.0f;
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnProcessSpell(const ProcessSpellEventArgs& args);
static void OnAfterAttack(OrbwalkingActionArgs& args);
static void AutoKillsteal();
static void Combo();
static void Mixed();
static void Clear();
static void AutoQ();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Sivir", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useW", "Use W (Ricochet)"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 61, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q"));
    LaneClearMenu->Add(new MenuBool("useW", "Use W (Ricochet)"));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useW", "Use W (Ricochet)"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("autoQ", "Auto Q on immobile Target"));
    MiscMenu->Add(new MenuBool("autoE", "Auto E against targeted spells"));
    MiscMenu->Add(new MenuBool("killsteal", "Auto Killsteal (Q)"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1250.0f);
    Q.SetSkillshot(0.25f, 90.0f, 1350.0f, false, SpellType::Line);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, FLT_MAX);
    E = Spell(SpellSlot::E, FLT_MAX);
    R = Spell(SpellSlot::R, 1000.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Orbwalker::OnAfterAttack += &OnAfterAttack;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Sivir loaded</font>");
}

// E Spell Shield: chặn kỹ năng targeted của địch nhắm vào mình.
static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded || !Bool(MiscMenu, "autoE") || !E.IsReady()) {
        return;
    }
    if (args.IsAutoAttack || args.Target.Ptr == 0) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || player.NetworkId() != static_cast<int>(args.Target.NetworkId)) {
        return;
    }

    const AIHeroClient sender = HeroFromInfo(args.Sender);
    if (!sender.IsValid() || !sender.IsHero() || !sender.IsEnemy()) {
        return;
    }

    // Bỏ qua summoner spell và TormentedSoil (giống bản C#).
    std::string lower = args.SpellName;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower.find("summoner") != std::string::npos ||
        lower.find("tormentedsoil") != std::string::npos) {
        return;
    }

    // TODO SDK: bản C# dùng DelayAction humanizer; SDK chưa có → cast ngay.
    E.Cast();
}

// W Ricochet: bật sau đòn đánh thường (combo → hero, laneclear → minion).
static void OnAfterAttack(OrbwalkingActionArgs& args) {
    if (!Loaded || !W.IsReady()) {
        return;
    }

    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(targetBase)) {
        return;
    }

    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo && targetBase.IsHero() && Bool(ComboMenu, "useW")) {
        W.Cast();
    } else if (mode == OrbwalkingMode::LaneClear && targetBase.IsMinion()) {
        const bool lane = Bool(LaneClearMenu, "useW") && ManaOkay(Slider(LaneClearMenu, "Mana", 60));
        const bool jungle = Bool(JungleClearMenu, "useW") && ManaOkay(Slider(JungleClearMenu, "Mana", 20));
        if (lane || jungle) {
            W.Cast();
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
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

    AutoQ();
}

// Auto killsteal: chỉ Q Boomerang Blade gây damage trực tiếp (skillshot line
// 1250, physical). Tính damage tay theo wiki qua SpellDamage.
static void AutoKillsteal() {
    if (!Bool(MiscMenu, "killsteal") || !Q.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const auto target = GetTarget(Q.Range, DamageType::Physical);
    if (!ValidHeroTarget(target, Q.Range) || !IsKillable(target, SpellDamage(SpellSlot::Q, target))) {
        return;
    }
    const auto pred = Q.GetPrediction(target);
    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
        Q.Cast(pred.GetCastPosition());
    }
}

static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }
    if (!Bool(ComboMenu, "useQ") || !Q.IsReady()) {
        return;
    }
    const auto target = GetTarget(Q.Range, DamageType::Physical);
    if (ValidHeroTarget(target, Q.Range)) {
        const auto pred = Q.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            Q.Cast(pred.GetCastPosition());
        }
    }
}

static void Mixed() {
    if (!Bool(HarassMenu, "useQ") || !Q.IsReady() || !ManaOkay(Slider(HarassMenu, "Mana", 61))) {
        return;
    }
    const auto target = GetTarget(Q.Range, DamageType::Physical);
    if (ValidHeroTarget(target, Q.Range)) {
        const auto pred = Q.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            Q.Cast(pred.GetCastPosition());
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid() || !Q.IsReady()) {
        return;
    }

    // Lane clear Q: bắn hàng lính (>=4).
    if (Bool(LaneClearMenu, "useQ") && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
        auto minions = GameObjects::EnemyLaneMinions();
        if (minions.empty()) {
            minions = GameObjects::EnemyMinions();
        }
        std::vector<AIBaseClient> targets;
        targets.reserve(minions.size());
        for (const auto& minion : minions) {
            if (ValidTarget(minion, Q.Range)) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        if (!targets.empty()) {
            const auto farm = Q.GetLineFarmLocation(targets);
            if (farm.MinionsHit >= 4) {
                Q.Cast(Vector3::From2D(farm.Position));
                return;
            }
        }
    }

    // Jungle clear Q: mob máu cao nhất trong tầm.
    if (Bool(JungleClearMenu, "useQ") && ManaOkay(Slider(JungleClearMenu, "Mana", 20))) {
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
        if (!mobs.empty() && ValidTarget(mobs.front(), Q.Range)) {
            Q.Cast(mobs.front());
        }
    }
}

// Auto Q lên mục tiêu bất động (hitchance Immobile).
static void AutoQ() {
    if (!Bool(MiscMenu, "autoQ") || !Q.IsReady()) {
        return;
    }
    const auto target = GetTarget(Q.Range, DamageType::Physical);
    if (!ValidHeroTarget(target, Q.Range)) {
        return;
    }
    const auto pred = Q.GetPrediction(target, true);
    if (HitchanceAtLeast(pred.Hitchance, HitChance::Immobile)) {
        Q.Cast(pred.GetCastPosition());
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Sivir
