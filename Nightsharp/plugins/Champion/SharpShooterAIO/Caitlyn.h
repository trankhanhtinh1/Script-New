#pragma once

// ============================================================================
// SharpShooter AIO — Caitlyn
// Port từ SharpShooterCSHarp/Plugins/Caitlyn.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Piltover Peacemaker — skillshot line 1250, delay 0.625, width 60,
//                           speed 2200. Poke khi không có địch trong tầm AA,
//                           finish/immobile khi có.
//   W Yordle Snap Trap    — bẫy circle 820. Auto đặt lên mục tiêu bất động
//                           (bỏ qua nếu đã có bẫy sẵn gần đó).
//   E 90 Caliber Net      — skillshot line 800 (đẩy lùi bản thân). Anti-melee,
//                           anti-gapcloser, dash tới con trỏ (keybind).
//   R Ace in the Hole     — targeted toàn cầu. Finish mục tiêu cô lập (không địch
//                           quanh mình 1500 / quanh target 500), không bị chắn.
//
// Cơ chế:
//   * Track bẫy "Cupcake Trap" của mình qua GameObjects::OnCreate/OnDelete để
//     không đặt W chồng lên bẫy sẵn.
//   * Auto-attack mục tiêu dính bẫy (buff "caitlynyordletrapinternal") qua
//     CoreControl::IssueAttack.
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreControl.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Caitlyn {

using SDK::Core::Utils::AutoAttack;

inline const char* const kTrapName = "Cupcake Trap";

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1250.0f };
inline Spell W{ SpellSlot::W, 820.0f };
inline Spell E{ SpellSlot::E, 800.0f };
inline Spell R{ SpellSlot::R, 2000.0f };

inline bool Loaded = false;

// Bẫy của mình đang đặt: networkId + vị trí.
struct TrapObj {
    int networkId = 0;
    Vector3 position{};
};
inline std::vector<TrapObj> MyTraps;

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

static AIHeroClient HeroFromInfo(const Core::Events::ObjectInfo& info) {
    ::Core::Objects::ObjectHandle handle{};
    handle.address = info.Ptr;
    handle.index = info.Index;
    handle.networkId = info.NetworkId;
    handle.type = info.Type;
    return AIHeroClient(handle);
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

// Có bẫy sẵn của mình gần vị trí (<=100)?
static bool TrapNear(const Vector3& pos) {
    for (const auto& trap : MyTraps) {
        if (pos.Distance(trap.position) <= 100.0f) {
            return true;
        }
    }
    return false;
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnCreateObject(const GameObject& object);
static void OnDeleteObject(const GameObject& object);
static void OnProcessSpell(const ProcessSpellEventArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void Combo();
static void Mixed();
static void Clear();
static void AutoR();
static void AutoW();
static void AutoAttackTrapped();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Caitlyn", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useR", "Use R (isolated finish)"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q", false));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (E)"));
    MiscMenu->Add(new MenuBool("interrupter", "Interrupter (W)"));
    MiscMenu->Add(new MenuBool("autoR", "Auto R on Killable Target"));
    MiscMenu->Add(new MenuBool("autoW", "Auto W on Immobile Target"));
    MiscMenu->Add(new MenuBool("attackTrapped", "Auto Attack Trapped Target"));
    MiscMenu->Add(new MenuBool("antiMelee", "Use Anti-Melee (E)"));
    MiscMenu->Add(new MenuKeyBind("dashE", "Dash to Cursor (E)", 'G', KeyBindType::Press));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1250.0f);
    Q.SetSkillshot(0.625f, 60.0f, 2200.0f, false, SpellType::Line);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 820.0f);
    W.SetSkillshot(1.0f, 100.0f, FLT_MAX, false, SpellType::Circle);
    W.DamageType = DamageType::Physical;

    E = Spell(SpellSlot::E, 800.0f);
    E.SetSkillshot(0.125f, 70.0f, 1600.0f, true, SpellType::Line);
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 2000.0f);
    R.DamageType = DamageType::Physical;

    MyTraps.clear();

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;
    GameObjects::AddOnCreate(&OnCreateObject);
    GameObjects::AddOnDelete(&OnDeleteObject);

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Caitlyn loaded</font>");
}

static void OnCreateObject(const GameObject& object) {
    if (!Loaded || !object.IsValid()) {
        return;
    }
    if (object.IsAlly() && object.Name() == kTrapName) {
        MyTraps.push_back(TrapObj{ object.NetworkId(), object.Position() });
    }
}

static void OnDeleteObject(const GameObject& object) {
    if (!Loaded || !object.IsValid()) {
        return;
    }
    if (object.Name() != kTrapName) {
        return;
    }
    const int id = object.NetworkId();
    MyTraps.erase(
        std::remove_if(
            MyTraps.begin(),
            MyTraps.end(),
            [id](const TrapObj& t) { return t.networkId == id; }),
        MyTraps.end());
}

// Anti-melee E: địch cận chiến auto-attack mình → E đẩy nó ra.
static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded || !Bool(MiscMenu, "antiMelee") || !E.IsReady() || !args.IsAutoAttack) {
        return;
    }
    if (args.Target.Ptr == 0) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || player.NetworkId() != static_cast<int>(args.Target.NetworkId)) {
        return;
    }
    const AIHeroClient sender = HeroFromInfo(args.Sender);
    if (sender.IsValid() && sender.IsHero() && sender.IsEnemy() && sender.IsMelee()) {
        E.Cast(sender.Position());
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(ComboMenu, "useQ") && Q.IsReady()) {
        // Không có địch trong tầm AA → poke thẳng target.
        bool anyInAaRange = false;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy) && AutoAttack::InAutoAttackRange(enemy)) {
                anyInAaRange = true;
                break;
            }
        }

        if (!anyInAaRange) {
            const auto target = GetTarget(Q.Range, DamageType::Physical);
            if (ValidHeroTarget(target, Q.Range)) {
                const auto pred = Q.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                    Q.Cast(pred.GetCastPosition());
                }
            }
        } else {
            // Có địch trong tầm AA: ưu tiên immobile, rồi killable.
            AIHeroClient immobile;
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!ValidHeroTarget(enemy, Q.Range)) {
                    continue;
                }
                const auto pred = Q.GetPrediction(enemy);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::Immobile)) {
                    immobile = enemy;
                    break;
                }
            }
            if (immobile.IsValid()) {
                const auto pred = Q.GetPrediction(immobile);
                Q.Cast(pred.GetCastPosition());
            } else {
                for (const auto& enemy : GameObjects::EnemyHeroes()) {
                    if (ValidHeroTarget(enemy, Q.Range) && IsKillable(enemy, Q.GetDamage(enemy))) {
                        const auto pred = Q.GetPrediction(enemy);
                        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                            Q.Cast(pred.GetCastPosition());
                            break;
                        }
                    }
                }
            }
        }
    }

    if (Bool(ComboMenu, "useR")) {
        AutoR();
    }
}

static void Mixed() {
    if (!Bool(HarassMenu, "useQ") || !Q.IsReady() || !ManaOkay(Slider(HarassMenu, "Mana", 60))) {
        return;
    }
    const auto target = GetTarget(Q.Range, DamageType::Physical);
    if (ValidHeroTarget(target, Q.Range)) {
        const auto pred = Q.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
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
    if (Bool(LaneClearMenu, "useQ", false) && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
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

// R finish: mục tiêu cô lập (không địch quanh mình 1500 & quanh target 500),
// không bị hero chắn đường.
static void AutoR() {
    if (!R.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    for (const auto& target : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(target, R.Range) || AutoAttack::InAutoAttackRange(target)) {
            continue;
        }
        if (!IsKillable(target, R.GetDamage(target))) {
            continue;
        }

        // Cô lập: không có địch khác quanh mình 1500 và quanh target 500.
        bool isolated = true;
        for (const auto& other : GameObjects::EnemyHeroes()) {
            if (other.NetworkId() == target.NetworkId() || other.IsDead()) {
                continue;
            }
            if (player.Distance(other) <= 1500.0f || target.Distance(other) <= 500.0f) {
                isolated = false;
                break;
            }
        }
        if (!isolated) {
            continue;
        }

        // Không bị hero khác chắn đường.
        SDK::PredictionInput input;
        input.Unit = player;
        input.Delay = 0.5f;
        input.Speed = 1500.0f;
        input.Radius = 500.0f;
        input.SetCollisionObjects(SDK::CollisionableObjects::Heroes);
        const auto collisions = Collision::GetCollision({ target.ServerPosition() }, input);
        bool blocked = false;
        for (const auto& unit : collisions) {
            if (unit.NetworkId() != target.NetworkId()) {
                blocked = true;
                break;
            }
        }
        if (!blocked) {
            R.CastOnUnit(AIBaseClient(target.Handle()));
            break;
        }
    }
}

// W auto lên mục tiêu bất động (>0.5s), bỏ qua nếu đã có bẫy sẵn gần đó.
static void AutoW() {
    if (!Bool(MiscMenu, "autoW") || !W.IsReady()) {
        return;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, W.Range)) {
            continue;
        }
        const auto pred = W.GetPrediction(enemy);
        if (static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::Immobile)) {
            if (!TrapNear(enemy.Position())) {
                W.Cast(enemy.Position());
            }
            break;
        }
    }
}

// Auto-attack mục tiêu dính bẫy (buff caitlynyordletrapinternal).
static void AutoAttackTrapped() {
    if (!Bool(MiscMenu, "attackTrapped") || !Orbwalker::CanAttack()) {
        return;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy, 1300.0f) && enemy.HasBuff("caitlynyordletrapinternal")) {
            CoreControl::IssueAttack(enemy.Address(), enemy.Position());
            break;
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    R.Range = 1500.0f + 500.0f * static_cast<float>(R.Level());

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

    if (Bool(MiscMenu, "autoR")) {
        AutoR();
    }
    AutoW();

    // Dash tới con trỏ (keybind): E ngược hướng đẩy Cait tới cursor.
    if (KeyActive(MiscMenu, "dashE") && E.IsReady()) {
        E.Cast(player.Position().Extend(Game::CursorPos(), -(E.Range / 2.0f)));
    }

    AutoAttackTrapped();
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "gapcloser") || !E.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || args.End.Distance2D(player.Position()) > 200.0f) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (ValidHeroTarget(sender, E.Range)) {
        E.Cast(sender.Position());
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;
    GameObjects::RemoveOnCreate(&OnCreateObject);
    GameObjects::RemoveOnDelete(&OnDeleteObject);

    MyTraps.clear();
    Loaded = false;
}

} // namespace Plugins::SharpAIO::Caitlyn
