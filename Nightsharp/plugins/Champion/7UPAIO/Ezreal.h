#pragma once

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::AIO7UP::Ezreal {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* Misc = nullptr;
inline Menu* KillStealMenu = nullptr;
inline Menu* RMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1200.0f };
inline Spell W{ SpellSlot::W, 1200.0f };
inline Spell E{ SpellSlot::E, 475.0f };
inline Spell R{ SpellSlot::R, 5000.0f };
inline Spell EQ{ SpellSlot::Q, 1625.0f };
inline Spell Ignite{ SpellSlot::Unknown, 600.0f };

inline bool Loaded = false;
inline DWORD LastHookBuffCastTick = 0;
inline DWORD LastComboEvalTick = 0;
inline DWORD LastLaneClearEvalTick = 0;
inline DWORD LastJungleClearEvalTick = 0;

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

static bool Key(Menu* menu, const char* key, bool fallback = false) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item ? item->Active : fallback;
}

static bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) {
        return false;
    }

    lastTick = now;
    return true;
}

static bool CastPosition(Spell& spell,
                         const Vector3& position,
                         const char* action,
                         const AIBaseClient& target = AIBaseClient()) {
    (void)action;
    (void)target;
    return spell.Cast(position);
}

static bool CastUnit(Spell& spell, const AIBaseClient& target, const char* action) {
    (void)action;
    return spell.Cast(target) == CastStates::SuccessfullyCasted;
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static bool EqualsIgnoreCase(const char* left, const char* right) {
    return left && right && left[0] && right[0] && _stricmp(left, right) == 0;
}

static bool ContainsIgnoreCase(const std::string& value, const char* needle) {
    if (value.empty() || !needle || !needle[0]) {
        return false;
    }

    std::string lowerValue = value;
    std::string lowerNeedle = needle;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowerValue.find(lowerNeedle) != std::string::npos;
}

static std::string RuntimeCharacterName(const AIBaseClient& unit) {
    if (!unit.IsValid()) {
        return {};
    }

    std::string cached = unit.CharacterName();
    if (!cached.empty()) {
        return cached;
    }

    char direct[96] = {};
    if (::Core::Objects::ReadCharacterName(
            unit.Address(),
            direct,
            static_cast<int>(sizeof(direct))) &&
        direct[0]) {
        return direct;
    }

    if (::Core::Objects::ReadName(
            unit.Address(),
            direct,
            static_cast<int>(sizeof(direct))) &&
        direct[0]) {
        return direct;
    }

    return {};
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

static bool IsLargeJungleMob(const AIMinionClient& minion) {
    const auto type = minion.GetJungleType();
    return type == JungleType::Legendary || type == JungleType::Epic || type == JungleType::Large;
}

static bool IsEpicJungleMob(const AIMinionClient& minion) {
    const auto type = minion.GetJungleType();
    if (type == JungleType::Legendary || type == JungleType::Epic) {
        return true;
    }

    const std::string name = RuntimeCharacterName(minion);
    return ContainsIgnoreCase(name, "dragon") ||
           ContainsIgnoreCase(name, "baron") ||
           ContainsIgnoreCase(name, "riftherald") ||
           ContainsIgnoreCase(name, "voidgrub") ||
           ContainsIgnoreCase(name, "atakhan") ||
           ContainsIgnoreCase(name, "sentinel");
}

static int JunglePriority(const AIMinionClient& minion) {
    const auto type = minion.GetJungleType();
    if (type == JungleType::Legendary) {
        return 5000;
    }
    if (type == JungleType::Epic) {
        return 4000;
    }
    if (IsEpicJungleMob(minion)) {
        return 4000;
    }
    if (type == JungleType::Large) {
        return 3000;
    }
    if (type == JungleType::Small) {
        return 1000;
    }
    return 0;
}

static int LaneMinionPriority(const AIMinionClient& minion) {
    const MinionTypes type = minion.GetMinionType();
    if (HasFlag(type, MinionTypes::Super)) {
        return 4000;
    }
    if (HasFlag(type, MinionTypes::Siege)) {
        return 3000;
    }
    if (HasFlag(type, MinionTypes::Melee)) {
        return 2000;
    }
    if (HasFlag(type, MinionTypes::Ranged)) {
        return 1000;
    }
    return 0;
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static float HealthRegenRate(const AIBaseClient& unit) {
    return unit.IsValid() ? ::CoreAIHeroClient::HealthRegenRate(unit.Address()) : 0.0f;
}

static float FlatPhysicalDamageMod(const AIBaseClient& unit) {
    return unit.IsValid() ? ::CoreAIHeroClient::FlatPhysicalDamageMod(unit.Address()) : 0.0f;
}

static bool IsImportantSpellCaster(const AIHeroClient& hero) {
    return hero.IsValid() && Extensions::IsCastingInterruptableSpell(hero, true);
}

static Vector3 ExtendFromPlayer(const Vector3& target, float distance) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }
    Vector3 result = player.PreviousPosition().Extend(target, distance);
    result.y = NavMesh::GetHeightForPosition(result);
    return result;
}

static bool IsWall(const Vector3& position) {
    return NavMesh::IsWall(position);
}

struct PullBuffData {
    const char* BuffName;
    const char* SourceChampion;
};

static constexpr PullBuffData kPullBuffs[] = {
    { "ThreshQ", "Thresh" },
    { "rocketgrab2", "Blitzcrank" },
    { "RocketGrab", "Blitzcrank" },
    { "PykeQ", "Pyke" },
    { "PykeQRange", "Pyke" },
    { "NautilusAnchorDragRoot", "Nautilus" },
    { "NautilusAnchorDrag", "Nautilus" },
    { "DariusAxeGrabCone", "Darius" },
    { "KledQMark", "Kled" },
    { "SwainERoot", "Swain" },
    { "SwainPassivePullMoveBuff", "Swain" },
};

static const PullBuffData* FindPullBuff(const char* buffName) {
    if (!buffName || !buffName[0]) {
        return nullptr;
    }

    for (const auto& entry : kPullBuffs) {
        if (EqualsIgnoreCase(buffName, entry.BuffName)) {
            return &entry;
        }
    }
    return nullptr;
}

static bool HeroNameEquals(const AIHeroClient& hero, const char* championName) {
    if (!hero.IsValid() || !championName || !championName[0]) {
        return false;
    }

    const std::string cached = hero.CharacterName();
    if (!cached.empty() && EqualsIgnoreCase(cached.c_str(), championName)) {
        return true;
    }

    char direct[64] = {};
    if (::Core::Objects::ReadCharacterName(
            hero.Address(),
            direct,
            static_cast<int>(sizeof(direct)))) {
        return EqualsIgnoreCase(direct, championName);
    }
    return false;
}

static AIHeroClient FindEnemyHeroByChampionName(const char* championName, float range) {
    const auto player = Player();
    if (!player.IsValid() || !championName || !championName[0]) {
        return {};
    }

    AIHeroClient best;
    float bestDistance = range;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, range) || !HeroNameEquals(enemy, championName)) {
            continue;
        }

        const float distance = player.Distance(enemy);
        if (!best.IsValid() || distance < bestDistance) {
            best = enemy;
            bestDistance = distance;
        }
    }
    return best;
}

static AIHeroClient FindClosestEnemyHero(float range) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    AIHeroClient best;
    float bestDistance = range;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, range)) {
            continue;
        }

        const float distance = player.Distance(enemy);
        if (!best.IsValid() || distance < bestDistance) {
            best = enemy;
            bestDistance = distance;
        }
    }
    return best;
}

static bool IsGoodDashPosition(const Vector3& position) {
    const auto player = Player();
    if (!player.IsValid() || (std::fabs(position.x) < 0.01f && std::fabs(position.z) < 0.01f)) {
        return false;
    }

    if (IsWall(position) || NavMesh::IsWallBetween(player.PreviousPosition(), position, E.Range / 5.0f)) {
        return false;
    }

    return true;
}

static Vector3 NormalizeDashPosition(Vector3 position) {
    position.y = NavMesh::GetHeightForPosition(position);
    return position;
}

static Vector3 GetPullBuffDashPosition(const AIHeroClient& source) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    if (source.IsValid()) {
        const Vector3 away = NormalizeDashPosition(
            player.PreviousPosition().Extend(source.PreviousPosition(), -E.Range));
        if (IsGoodDashPosition(away)) {
            return away;
        }
    }

    const Vector3 cursor = NormalizeDashPosition(
        player.PreviousPosition().Extend(Game::CursorPos(), E.Range));
    if (IsGoodDashPosition(cursor)) {
        return cursor;
    }

    return {};
}

static void CastSkillshot(Spell& spell,
                          const AIBaseClient& target,
                          HitChance hitChance = HitChance::High,
                          const char* action = "skillshot") {
    if (!spell.IsReady()) {
        return;
    }

    if (!ValidTarget(target, spell.CurrentRange())) {
        return;
    }

    const auto pred = spell.GetPrediction(target);
    if (!pred.CollisionObjects.empty()) {
        return;
    }

    if (HitchanceAtLeast(pred.Hitchance, hitChance)) {
        CastPosition(spell, pred.GetCastPosition(), action, target);
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void Orbwalker_OnAfterAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void OneKeyCastR();
static void AutoRLogic();
static void Combo();
static void ComboELogic(const AIHeroClient& target);
static void Harass();
static void LaneClear();
static void JungleClear();
static void LastHit();
static double EzrealQManualDamage(const AIBaseClient& target);
static double QDamage(const AIBaseClient& target);
static void KillSteal();
static void OnBuffAdd(const Events::BuffEventArgs& args);
static void OnUnload();



static void BuildMenu() {
    MenuRoot = new Menu("champion.7upaio", "7UP - Ezreal", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("ComboECheck", "Use E |Safe Check"));
    ComboMenu->Add(new MenuBool("ComboEWall", "Use E |Wall Check"));
    ComboMenu->Add(new MenuBool("useR", "Use R"));
    ComboMenu->Add(new MenuKeyBind("SemiR", "Semi R", SDK::Keys::T, KeyBindType::Press));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuBool("useW", "Use W"));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("ManaCL", "Mana Clear", 15, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useW", "Use W"));
    JungleClearMenu->Add(new MenuSlider("ManaCL", "Mana Clear", 15, 0, 100));

    RMenu = MenuRoot->AddSubMenu(new Menu("R Settings", "RMenu"));
    RMenu->Add(new MenuBool("AutoR", "Auto R"));
    RMenu->Add(new MenuSlider("RRange", "Auto R |Min Cast Range >= x", 900, 0, 1500));
    RMenu->Add(new MenuSlider("RMaxRange", "Auto R |Max Cast Range >= x", 3000, 1500, 5000));

    Misc = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    Misc->Add(new MenuBool("gapcloser", "Gapcloser"));
    Misc->Add(new MenuBool("hookE", "Use E on Hook/Pull Buff"));

    KillStealMenu = MenuRoot->AddSubMenu(new Menu("KillSteal Settings", "KillSteal"));
    KillStealMenu->Add(new MenuBool("killstealQ", "Use Q"));

    MenuRoot->Attach();
}
static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1200.0f);
    Q.SetSkillshot(0.25f, 53.0f, 2000.0f, true, SpellType::Line);
    W = Spell(SpellSlot::W, 1200.0f);
    W.SetSkillshot(0.25f, 55.0f, 1700.0f, true, SpellType::Line);
    W.SetCollisionObjects(
        SDK::CollisionableObjects::Heroes |
        SDK::CollisionableObjects::Building |
        SDK::CollisionableObjects::YasuoWall);
    E = Spell(SpellSlot::E, 475.0f);
    E.Delay = 0.65f;
    R = Spell(SpellSlot::R, 5000.0f);
    R.SetSkillshot(1.0f, 160.0f, 2200.0f, false, SpellType::Line);

    EQ = Spell(SpellSlot::Q, 1625.0f);
    EQ.SetSkillshot(0.90f, 57.0f, 1350.0f, true, SpellType::Line);

    Ignite = Spell(player.GetSpellSlot("summonerdot"), 600.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnAfterAttack += &Orbwalker_OnAfterAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;
    Events::hook.OnBuffAdd += &OnBuffAdd;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>7UP - Ezreal loaded</font>");
}


static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    const auto targetBase = AIBaseClient(args.Target.Handle());
    const bool useW = Bool(ComboMenu, "useW");

    if (!ValidUnit(targetBase)) {
        return;
    }

    if (!targetBase.IsHero()) {
        return;
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
        const auto target = AIHeroClient(targetBase.Handle());
        if (ValidHeroTarget(target, W.Range) && useW && W.IsReady()) {
            const auto pred = W.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                CastPosition(W, pred.GetCastPosition(), "before-attack-W", target);
            }
        }
    }
}

static void Orbwalker_OnAfterAttack(OrbwalkingActionArgs& args) {
    const auto targetBase = AIBaseClient(args.Target.Handle());
    const bool useQ = Bool(ComboMenu, "useQ");
    const bool qHarass = Bool(HarassMenu, "useQ");

    if (!ValidUnit(targetBase) || !targetBase.IsHero()) {
        return;
    }

    const auto target = AIHeroClient(targetBase.Handle());
    if (!ValidHeroTarget(target)) {
        return;
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
        if (useQ) {
            CastSkillshot(Q, target, HitChance::High, "after-attack-Q-combo");
        }
    } else if (Orbwalker::ActiveMode() == OrbwalkingMode::Harass ||
               Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear) {
        if (qHarass) {
            CastSkillshot(Q, target, HitChance::High, "after-attack-Q-harass");
        }
    }
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(Misc, "gapcloser")) {
        return;
    }

    const auto sender = AIHeroClient(args.Sender);
    if (!E.IsReady() || !ValidHeroTarget(sender, E.Range)) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (sender.IsMelee() &&
        ValidHeroTarget(sender, sender.AttackRange() + sender.BoundingRadius() + 100.0f)) {
        CastPosition(E, ExtendFromPlayer(sender.PreviousPosition(), -E.Range), "gapcloser-E-melee", sender);
    }

    if (sender.IsDashing() &&
        (args.End.Distance2D(player.Position()) <= 250.0f ||
         sender.PreviousPosition().Distance2D(player.Position()) <= 300.0f)) {
        CastPosition(E, ExtendFromPlayer(sender.PreviousPosition(), -E.Range), "gapcloser-E-dash", sender);
    }

    if (!IsImportantSpellCaster(sender)) {
        return;
    }

    if (sender.PreviousPosition().Distance2D(player.Position()) <= 300.0f) {
        CastPosition(E, ExtendFromPlayer(sender.PreviousPosition(), -E.Range), "gapcloser-E-important", sender);
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.IsDead()) {
        return;
    }
    if (player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }
    if (player.Spellbook().IsWindingUp()) {
        return;
    }

    if (R.Level() > 0) {
        R.Range = static_cast<float>(Slider(RMenu, "RMaxRange", 3000));
    }

    if (Key(ComboMenu, "SemiR")) {
        OneKeyCastR();
    }

    if (Bool(RMenu, "AutoR") && R.IsReady() && player.CountEnemyHeroesInRange(1000.0f) == 0) {
        AutoRLogic();
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        return;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        JungleClear();
        break;
    case OrbwalkingMode::LastHit:
        LastHit();
        break;
    default:
        break;
    }

    KillSteal();
}

static void OneKeyCastR() {

    if (!R.IsReady()) {
        return;
    }

    const auto target = GetTarget(R.Range, DamageType::Physical);
    if (ValidHeroTarget(target, R.Range) &&
        !ValidHeroTarget(target, static_cast<float>(Slider(RMenu, "RRange", 900)))) {
        const auto pred = R.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            CastPosition(R, pred.GetCastPosition(), "semiR", target);
        }
    }
}

static void AutoRLogic() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const float minRange = static_cast<float>(Slider(RMenu, "RRange", 900));
    for (const auto& target : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(target, R.Range) || target.DistanceToPlayer() < minRange) {
            continue;
        }

        const float regen = HealthRegenRate(target) * 2.0f;
        if (!target.IsMoving() && ValidHeroTarget(target, EQ.Range) &&
            player.GetSpellDamage(target, SpellSlot::R) +
                player.GetSpellDamage(target, SpellSlot::Q) * 3.0f >=
                target.Health() + regen) {
            CastUnit(R, target, "autoR-immobile");
        }

        if (player.GetSpellDamage(target, SpellSlot::R) > target.Health() + regen &&
            target.Path().size() < 2 &&
            HitchanceAtLeast(R.GetPrediction(target).Hitchance, HitChance::High)) {
            CastUnit(R, target, "autoR-kill");
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (!ShouldRunNow(LastComboEvalTick, 80)) {
        return;
    }

    const auto target = GetTarget(EQ.Range, DamageType::Physical);
    const bool useQ = Bool(ComboMenu, "useQ");
    const bool useW = Bool(ComboMenu, "useW");
    const bool useE = Bool(ComboMenu, "useE");
    const bool useR = Bool(ComboMenu, "useR");

    if (!ValidHeroTarget(target, EQ.Range)) {
        return;
    }

    if (useE && E.IsReady() && ValidHeroTarget(target, EQ.Range)) {
        ComboELogic(target);
    }

    if (useW && W.IsReady() && ValidHeroTarget(target, W.Range)) {
        const auto wPred = W.GetPrediction(target);
        if (HitchanceAtLeast(wPred.Hitchance, HitChance::High)) {
            if (AutoAttack::InAutoAttackRange(target)) {
                CastPosition(W, wPred.GetCastPosition(), "combo-W-aaPred", target);
            } else if (Q.IsReady()) {
                const auto qPred = Q.GetPrediction(target);
                if (HitchanceAtLeast(qPred.Hitchance, HitChance::High)) {
                    CastPosition(W, qPred.GetCastPosition(), "combo-W-qPred", target);
                }
            }
        }
    }

    if (useQ && Q.IsReady() && ValidHeroTarget(target, Q.Range)) {
        const auto qPred = Q.GetPrediction(target);
        if (HitchanceAtLeast(qPred.Hitchance, HitChance::High)) {
            CastPosition(Q, qPred.GetCastPosition(), "combo-Q", target);
        }
    }

    if (!useR || !R.IsReady()) {
        return;
    }

    if (player.IsUnderEnemyTurret() || player.CountEnemyHeroesInRange(800.0f) > 1) {
        return;
    }

    const float minRRange = static_cast<float>(Slider(RMenu, "RRange", 900));
    for (const auto& rTarget : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(rTarget, R.Range) || rTarget.DistanceToPlayer() < minRRange) {
            continue;
        }

        if (rTarget.Health() < player.GetSpellDamage(rTarget, SpellSlot::R) &&
            HitchanceAtLeast(R.GetPrediction(rTarget).Hitchance, HitChance::High) &&
            rTarget.DistanceToPlayer() > Q.Range + E.Range / 2.0f) {
            CastUnit(R, target, "combo-R-kill-far");
        }

        if (ValidHeroTarget(rTarget, Q.Range + E.Range) &&
            player.GetSpellDamage(rTarget, SpellSlot::R) +
                (Q.IsReady() ? player.GetSpellDamage(rTarget, SpellSlot::Q) : 0.0f) +
                (W.IsReady() ? player.GetSpellDamage(rTarget, SpellSlot::W) : 0.0f) >
                rTarget.Health() + HealthRegenRate(rTarget) * 2.0f) {
            CastUnit(R, rTarget, "combo-R-chain-kill");
        }
    }
}

static void ComboELogic(const AIHeroClient& target) {
    const auto player = Player();
    const bool eCheck = Bool(ComboMenu, "ComboECheck");
    const bool eWall = Bool(ComboMenu, "ComboEWall");

    if (!player.IsValid() || !ValidHeroTarget(target)) {
        return;
    }

    if (!eCheck || player.IsUnderEnemyTurret() || player.CountEnemyHeroesInRange(1200.0f) > 2) {
        return;
    }

    if (target.DistanceToPlayer() <= AutoAttack::GetRealAutoAttackRange(player, target)) {
        return;
    }

    const auto tryCastE = [&](const Vector3& castPos) -> bool {
        if (eWall && IsWall(castPos)) {
            return false;
        }
        return CastPosition(E, castPos, "combo-E", target);
    };

    const Vector3 castEPos = ExtendFromPlayer(target.PreviousPosition(), 475.0f);

    if (target.Health() <
            player.GetSpellDamage(target, SpellSlot::E) +
            Damage::GetAutoAttackDamage(player, target) &&
        target.PreviousPosition().Distance(Game::CursorPos()) <
            player.PreviousPosition().Distance(Game::CursorPos())) {
        tryCastE(castEPos);
        return;
    }

    if (target.Health() <
            player.GetSpellDamage(target, SpellSlot::E) +
            player.GetSpellDamage(target, SpellSlot::W) &&
        W.IsReady() &&
        target.PreviousPosition().Distance(Game::CursorPos()) + 350.0f <
            player.PreviousPosition().Distance(Game::CursorPos())) {
        tryCastE(castEPos);
        return;
    }

    if (target.Health() <
            player.GetSpellDamage(target, SpellSlot::E) +
            player.GetSpellDamage(target, SpellSlot::Q) &&
        Q.IsReady() &&
        target.PreviousPosition().Distance(Game::CursorPos()) + 300.0f <
            player.PreviousPosition().Distance(Game::CursorPos())) {
        tryCastE(castEPos);
    }
}

static void Harass() {
    if (!Bool(HarassMenu, "useQ") || !Q.IsReady()) {
        return;
    }

    const auto target = GetTarget(Q.Range, DamageType::Physical);
    CastSkillshot(Q, target, HitChance::High, "harass-Q");
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const bool useQ = Bool(LaneClearMenu, "useQ");
    const float manaPercent = player.ManaPercent();
    const int minMana = Slider(LaneClearMenu, "ManaCL", 15);
    if (!useQ || manaPercent < static_cast<float>(minMana)) {
        return;
    }

    if (!Q.IsReady()) {
        return;
    }

    if (!ShouldRunNow(LastLaneClearEvalTick, 120)) {
        return;
    }

    std::vector<AIMinionClient> minions = GameObjects::EnemyLaneMinions();
    if (minions.empty()) {
        minions = GameObjects::EnemyMinions();
    }

    minions.erase(
        std::remove_if(
            minions.begin(),
            minions.end(),
            [](const AIMinionClient& minion) {
                return !ValidTarget(minion, Q.Range) ||
                       minion.IsJungle() ||
                       minion.IsPlant() ||
                       minion.IsPet() ||
                       minion.IsClone();
            }),
        minions.end());

    if (minions.empty()) {
        return;
    }

    std::sort(
        minions.begin(),
        minions.end(),
        [&](const AIMinionClient& a, const AIMinionClient& b) {
            const bool aKillable = QDamage(a) >= a.Health();
            const bool bKillable = QDamage(b) >= b.Health();
            if (aKillable != bKillable) {
                return aKillable;
            }

            const int aPriority = LaneMinionPriority(a);
            const int bPriority = LaneMinionPriority(b);
            if (aPriority != bPriority) {
                return aPriority > bPriority;
            }

            return a.DistanceToPlayer() < b.DistanceToPlayer();
        });

    for (const auto& minion : minions) {
        if (!ValidTarget(minion, Q.Range)) {
            continue;
        }

        const float qDamage = static_cast<float>(QDamage(minion));
        if (qDamage < minion.Health()) {
            continue;
        }

        const auto pred = Q.GetPrediction(minion, false, -1.0f, Q.CollisionObjects);
        const Vector3 castPos = pred.GetCastPosition();
        if (!pred.CollisionObjects.empty()) {
            continue;
        }

        if (!HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            continue;
        }

        if (castPos.Distance2D(player.Position()) > Q.Range) {
            continue;
        }

        CastPosition(Q, castPos, "laneclear-Q", minion);
        return;
    }

}

static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const bool useQ = Bool(JungleClearMenu, "useQ");
    const bool useW = Bool(JungleClearMenu, "useW");
    const float manaPercent = player.ManaPercent();
    const int minMana = Slider(JungleClearMenu, "ManaCL", 15);

    if (manaPercent < static_cast<float>(minMana)) {
        return;
    }

    if (!ShouldRunNow(LastJungleClearEvalTick, 120)) {
        return;
    }

    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !ValidTarget(mob, Q.Range) ||
                       mob.IsPlant() ||
                       mob.IsPet() ||
                       mob.IsClone();
            }),
        mobs.end());
    std::sort(
        mobs.begin(),
        mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) {
            const int aPriority = JunglePriority(a);
            const int bPriority = JunglePriority(b);
            if (aPriority != bPriority) {
                return aPriority > bPriority;
            }
            if (std::fabs(a.MaxHealth() - b.MaxHealth()) > 1.0f) {
                return a.MaxHealth() > b.MaxHealth();
            }
            return a.DistanceToPlayer() < b.DistanceToPlayer();
        });

    if (mobs.empty()) {
        return;
    }

    if (useW && W.IsReady()) {
        for (const auto& obj : mobs) {
            const bool epic = IsEpicJungleMob(obj);
            if (!epic || !ValidTarget(obj, W.Range)) {
                continue;
            }

            CastPosition(W, obj.Position(), "jungleclear-W-epic", obj);
            return;
        }
    }

    if (!useQ || !Q.IsReady()) {
        return;
    }

    for (const auto& mob : mobs) {
        if (!ValidTarget(mob, Q.Range)) {
            continue;
        }

        const auto pred = Q.GetPrediction(mob, false, -1.0f, Q.CollisionObjects);
        if (pred.CollisionObjects.empty() &&
            HitchanceAtLeast(pred.Hitchance, HitChance::High) &&
            pred.GetCastPosition().Distance2D(player.Position()) <= Q.Range) {
            CastPosition(Q, pred.GetCastPosition(), "jungleclear-Q", mob);
            return;
        }
    }
}

static void LastHit() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (player.ManaPercent() >= static_cast<float>(Slider(LaneClearMenu, "ManaCL", 15))) {
        return;
    }

    if (!Q.IsReady()) {
        return;
    }

    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidTarget(minion, Q.Range) || !minion.IsMinion()) {
            continue;
        }

        if (minion.DistanceToPlayer() <= Q.Range &&
            minion.DistanceToPlayer() > AutoAttack::GetRealAutoAttackRange(minion) &&
            minion.Health() < player.GetSpellDamage(minion, SpellSlot::Q)) {
            const CastStates state = Q.CastIfHitchanceEquals(minion, HitChance::Medium);
            if (state == CastStates::SuccessfullyCasted) {
                return;
            }
        }
    }
}

static double EzrealQManualDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }

    static constexpr float qBase[] = { 0.0f, 20.0f, 45.0f, 70.0f, 95.0f, 120.0f };
    const int level = std::clamp(Q.Level(), 1, 5);
    const float raw = qBase[level] + 1.3f * player.AD() + 0.15f * player.AP();
    return player.CalculatePhysicalDamage(target, raw);
}

static double QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }

    const float sdkDamage = Q.GetDamage(target);
    if (sdkDamage > 0.0f) {
        return sdkDamage;
    }

    return EzrealQManualDamage(target);
}

static void KillSteal() {
    if (!Bool(KillStealMenu, "killstealQ") || !Q.IsReady()) {
        return;
    }

    for (const auto& target : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(target, W.Range) ||
            target.HasBuff("JudicatorIntervention") ||
            target.HasBuff("kindredrnodeathbuff") ||
            target.HasBuff("Undying Rage") ||
            target.HasBuff("FioraW") ||
            target.HasBuff("BlitzcrankManaBarrierCO")) {
            continue;
        }

        const double damage = QDamage(target);
        const float shieldedHealth = target.Health() + target.AllShield();
        if (Player().Distance(target) > 150.0f) {
            if (shieldedHealth <= damage) {
                CastSkillshot(Q, target, HitChance::High, "killsteal-Q");
            }
        } else {
            if (shieldedHealth <= damage * 1.5f) {
                CastSkillshot(Q, target, HitChance::High, "killsteal-Q-close");
            }
        }
    }
}

static void OnBuffAdd(const Events::BuffEventArgs& args) {
    if (!Loaded || !Bool(Misc, "hookE") || !E.IsReady()) {
        return;
    }

    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }

    const PullBuffData* pullBuff = FindPullBuff(args.BuffName);
    if (!pullBuff) {
        return;
    }

    const DWORD now = GetTickCount();
    if (LastHookBuffCastTick != 0 && now - LastHookBuffCastTick < 250) {
        return;
    }

    AIHeroClient source = FindEnemyHeroByChampionName(pullBuff->SourceChampion, 1800.0f);
    if (!source.IsValid()) {
        source = FindClosestEnemyHero(1400.0f);
    }

    const Vector3 dashPos = GetPullBuffDashPosition(source);
    if (dashPos.x == 0.0f && dashPos.z == 0.0f) {
        return;
    }

    if (CastPosition(E, dashPos, "buffadd-hook-E", source)) {
        LastHookBuffCastTick = now;
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnAfterAttack -= &Orbwalker_OnAfterAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;
    Events::hook.OnBuffAdd -= &OnBuffAdd;

    Loaded = false;
}

} // namespace Plugins::AIO7UP::Ezreal
