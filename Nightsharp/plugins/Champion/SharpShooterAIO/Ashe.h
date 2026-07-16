#pragma once

// ============================================================================
// SharpShooter AIO — Ashe
// Port từ SharpShooterCSHarp/Plugins/Ashe.cs (LeagueSharp/EloBuddy) sang
// NightSharp C++. Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Ranger's Focus  — self-buff, cast khi có buff "asheqcastready" và đang
//                        auto-attack mục tiêu trong tầm (không range/skillshot).
//   W Volley          — skillshot line, 1200, delay 0.25, width 60, speed 1500,
//                        collision (minion) true.
//   R Enchanted Crystal Arrow — skillshot line, 2500, delay 0.25, width 130,
//                        speed 1600, magic, không collision.
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreBuffs.h"

#include <algorithm>
#include <cfloat>

namespace Plugins::SharpAIO::Ashe {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, FLT_MAX };
inline Spell W{ SpellSlot::W, 1200.0f };
inline Spell R{ SpellSlot::R, 2500.0f };

inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;
inline DWORD LastClearEvalTick = 0;

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

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
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

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static AIHeroClient GetTargetNoCollision(Spell& spell) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTargetNoCollision(&spell) : AIHeroClient();
}

static bool CastPosition(Spell& spell, const Vector3& position) {
    return spell.Cast(position);
}

static bool ManaOkay(int percent) {
    const auto player = Player();
    return player.IsValid() && player.ManaPercent() >= static_cast<float>(percent);
}

// Q Ranger's Focus sẵn sàng khi có buff "asheqcastready" (đủ 4 stack Focus).
// Buff này chỉ cần TỒN TẠI là Q dùng được — không cần check netid/caster, cũng
// không dựa vào IsActive (dùng HasBuffRaw để tránh bị loại do endTime bất thường).
static bool QCastReady() {
    const auto player = Player();
    return player.IsValid() && Q.IsReady() &&
           ::CoreBuffs::HasBuffRaw(player.Address(), "asheqcastready");
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
    return target.Health() + target.MagicalShield() + target.PhysicalShield() < damage - 2.0;
}

// ── Damage tính tay theo wiki (leagueoflegends.com/en-us/Ashe/LoL) — KHÔNG
//    dùng Spell::GetDamage ──
// Q Ranger's Focus (physical): active biến auto-attack thành flurry 5 mũi tên,
//     mỗi mũi 22/23/24/25/26% AD (tổng flurry 110/115/120/125/130% AD; flurry
//     đầu +1 mũi = 132/138/144/150/156% AD). Không có AP ratio. Đây KHÔNG phải
//     nuke độc lập — sát thương đi kèm đòn đánh thường, nên không killsteal
//     riêng được => trả 0 cho slot Q.
// W Volley (physical, 1 mũi tên trúng 1 mục tiêu đầu tiên):
//     base 60/95/130/165/200 + 100% total AD  (mỗi mũi chỉ trúng 1 địch).
// R Enchanted Crystal Arrow (MAGIC, 3 rank — clamp idx như Corki R):
//     base 200/400/600 + 120% AP.  Global 2500, no-collision => killsteal
//     tầm xa nhất.
// Trả về damage đã trừ giáp/kháng phép qua Damage::CalculateDamage.
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float ap = player.AP();

    switch (slot) {
    case SpellSlot::W: {
        const int rank = W.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 60.0f, 95.0f, 130.0f, 165.0f, 200.0f };
        const float raw = base[rank - 1] + 1.00f * player.BonusAttackDamage();
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    case SpellSlot::R: {
        const int rank = R.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[3] = { 200.0f, 400.0f, 600.0f };
        const int idx = (rank - 1 < 3) ? rank - 1 : 2;
        const float raw = base[idx] + 1.20f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    default:
        // Q Ranger's Focus không killsteal độc lập (sát thương qua auto-attack).
        return 0.0f;
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnProcessSpell(const ProcessSpellEventArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void AutoKillsteal();
static void Combo();
static void Mixed();
static void Clear();
static void AutoR();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Ashe", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuSlider("wMana", "W If Mana > %", 25, 0, 100));
    ComboMenu->Add(new MenuBool("useR", "Use R"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useW", "Use W"));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 61, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useW", "Use W", false));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useW", "Use W"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("killsteal", "Auto Killsteal (R/W)"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (R)"));
    MiscMenu->Add(new MenuBool("interrupter", "Interrupter (R)"));
    MiscMenu->Add(new MenuBool("autoR", "Auto R on immobile targets"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, FLT_MAX);

    W = Spell(SpellSlot::W, 1200.0f);
    W.SetSkillshot(0.25f, 60.0f, 1500.0f, true, SpellType::Line);
    W.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 2500.0f);
    R.SetSkillshot(0.25f, 130.0f, 1600.0f, false, SpellType::Line);
    R.DamageType = DamageType::Magical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Ashe loaded</font>");
}

// Q Ranger's Focus: kích hoạt khi auto-attack trúng hero (combo) hoặc jungle
// mob (laneclear) trong lúc có buff "asheqcastready".
static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded || !args.IsAutoAttack) {
        return;
    }
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || !QCastReady()) {
        return;
    }

    ::Core::Objects::ObjectHandle handle{};
    handle.address = args.Target.Ptr;
    handle.index = args.Target.Index;
    handle.networkId = args.Target.NetworkId;
    handle.type = args.Target.Type;

    const OrbwalkingMode mode = Orbwalker::ActiveMode();

    if (mode == OrbwalkingMode::Combo) {
        if (!Bool(ComboMenu, "useQ")) {
            return;
        }
        const AIHeroClient target(handle);
        if (ValidHeroTarget(target) && AutoAttack::InAutoAttackRange(target)) {
            Q.Cast();
        }
        return;
    }

    if (mode == OrbwalkingMode::LaneClear) {
        if (!Bool(JungleClearMenu, "useQ")) {
            return;
        }
        const AIMinionClient minion(handle);
        if (ValidTarget(minion, player.AttackRange() + player.BoundingRadius() + minion.BoundingRadius()) &&
            minion.Team() == GameObjectTeam::Neutral) {
            Q.Cast();
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

    // Auto killsteal: chạy mọi mode, không phụ thuộc combo (giống Corki).
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

    AutoR();
}

// Auto killsteal: R Enchanted Crystal Arrow (global 2500, no-collision — tầm xa
// nhất) → W Volley (skillshot line 1200). Q Ranger's Focus không killsteal độc
// lập (sát thương đi qua auto-attack) nên không tính ở đây. Damage tính tay theo
// wiki qua SpellDamage, chỉ cast khi IsKillable.
static void AutoKillsteal() {
    if (!Bool(MiscMenu, "killsteal")) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // R Enchanted Crystal Arrow: no-collision, tầm 2500.
    if (R.IsReady()) {
        const auto target = GetTarget(R.Range, DamageType::Magical);
        if (ValidHeroTarget(target, R.Range) && IsKillable(target, SpellDamage(SpellSlot::R, target))) {
            const auto pred = R.GetPrediction(target, true);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                CastPosition(R, pred.GetCastPosition());
                return;
            }
        }
    }

    // W Volley: skillshot line 1200, collision (minion) — mỗi mũi trúng 1 địch.
    if (W.IsReady()) {
        const auto target = GetTargetNoCollision(W);
        if (ValidHeroTarget(target, W.Range) && IsKillable(target, SpellDamage(SpellSlot::W, target))) {
            const auto pred = W.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                CastPosition(W, pred.GetCastPosition());
            }
        }
    }
}

static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Q Ranger's Focus: bật ngay khi buff sẵn sàng và có hero trong tầm đánh.
    // Không đợi OnProcessSpell (event AA) vì buff có thể đủ giữa 2 đòn -> chủ
    // động toggle để không bỏ lỡ. Q là self-cast, không endposition.
    if (Bool(ComboMenu, "useQ") && QCastReady()) {
        const auto qTarget = GetTarget(AutoAttack::GetRealAutoAttackRange(player),
                                       DamageType::Physical);
        if (ValidHeroTarget(qTarget) && AutoAttack::InAutoAttackRange(qTarget)) {
            Q.Cast();
        }
    }

    if (Bool(ComboMenu, "useW") && W.IsReady() && ManaOkay(Slider(ComboMenu, "wMana", 25))) {
        const auto target = GetTargetNoCollision(W);
        if (ValidHeroTarget(target, W.Range)) {
            W.Cast(target);
        }
    }

    if (Bool(ComboMenu, "useR") && R.IsReady()) {
        const auto target = GetTarget(R.Range, DamageType::Magical);
        if (ValidHeroTarget(target, R.Range)) {
            const auto pred = R.GetPrediction(target, true);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                const bool killable =
                    IsKillable(target, SpellDamage(SpellSlot::R, target)) &&
                    !AutoAttack::InAutoAttackRange(target);
                if (killable) {
                    CastPosition(R, pred.GetCastPosition());
                } else if (ValidHeroTarget(target, 600.0f)) {
                    CastPosition(R, pred.GetCastPosition());
                } else if (R.Speed > 1.0f &&
                           HitchanceAtLeast(pred.Hitchance, HitChance::Immobile)) {
                    R.Cast(target);
                }
            }
        }
    }
}

static void Mixed() {
    if (!Bool(HarassMenu, "useW") || !W.IsReady() || !ManaOkay(Slider(HarassMenu, "Mana", 61))) {
        return;
    }

    const auto target = GetTargetNoCollision(W);
    if (ValidHeroTarget(target, W.Range)) {
        W.Cast(target);
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid() || !ShouldRunNow(LastClearEvalTick, 120)) {
        return;
    }

    // Lane clear W: chọn vị trí bắn trúng nhiều lính nhất trên đường.
    if (Bool(LaneClearMenu, "useW") && W.IsReady() && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
        auto minions = GameObjects::EnemyLaneMinions();
        if (minions.empty()) {
            minions = GameObjects::EnemyMinions();
        }
        std::vector<AIBaseClient> targets;
        targets.reserve(minions.size());
        for (const auto& minion : minions) {
            if (ValidTarget(minion, W.Range)) {
                targets.push_back(AIBaseClient(minion.Handle()));
            }
        }
        if (!targets.empty()) {
            const auto farm = W.GetLineFarmLocation(targets);
            if (farm.MinionsHit >= 1) {
                CastPosition(W, Vector3::From2D(farm.Position));
            }
        }
    }

    // Jungle clear W: mob máu cao nhất trong tầm.
    if (Bool(JungleClearMenu, "useW") && W.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20))) {
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
            const auto pred = W.GetPrediction(mob);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                W.Cast(mob);
                break;
            }
        }
    }
}

static void AutoR() {
    if (!Bool(MiscMenu, "autoR") || !R.IsReady() || R.Speed <= 1.0f) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const auto target = GetTarget(R.Range, DamageType::Magical);
    if (!ValidHeroTarget(target, R.Range)) {
        return;
    }

    const auto pred = R.GetPrediction(target);
    if (HitchanceAtLeast(pred.Hitchance, HitChance::Immobile)) {
        CastPosition(R, pred.GetCastPosition());
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

    // Bỏ qua Master Yi (giống bản C#).
    const std::string name = sender.CharacterName();
    if (_stricmp(name.c_str(), "MasterYi") == 0) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (args.End.Distance2D(player.Position()) <= 200.0f) {
        R.Cast(sender);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Ashe
