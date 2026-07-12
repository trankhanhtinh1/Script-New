#pragma once

#include "../../../SDK/SDK.h"
#include "Damage.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::ziblldev9898::Locke {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* KillStealMenu = nullptr;

// CDragon locke.bin.json: Q=950, W=250, E=425, R=1000
inline Spell Q{ SpellSlot::Q, 950.0f };
inline Spell W{ SpellSlot::W, 250.0f };
inline Spell E{ SpellSlot::E, 425.0f };
inline Spell R{ SpellSlot::R, 1000.0f };

inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;
inline DWORD LastHarassEvalTick = 0;
inline DWORD LastLaneClearEvalTick = 0;
inline DWORD LastJungleClearEvalTick = 0;
inline DWORD LastWCastTick = 0;

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

    const std::string name = minion.CharacterName();
    auto contains = [](const std::string& value, const char* needle) {
        if (value.empty() || !needle || !needle[0]) {
            return false;
        }
        std::string lowerValue = value;
        std::string lowerNeedle = needle;
        std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lowerValue.find(lowerNeedle) != std::string::npos;
    };
    return contains(name, "dragon") ||
           contains(name, "baron") ||
           contains(name, "riftherald") ||
           contains(name, "voidgrub") ||
           contains(name, "atakhan") ||
           contains(name, "sentinel");
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

    const auto pred = spell.GetPrediction(target, false, spell.CurrentRange() - 50.0f);
    if (!pred.CollisionObjects.empty()) {
        return;
    }

    if (HitchanceAtLeast(pred.Hitchance, hitChance)) {
        CastPosition(spell, pred.GetCastPosition(), action, target);
    }
}

static double QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }
    return Damage::GetLockeQMissileDamage(player, target, Q.Level());
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

static double EDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }
    return Damage::GetLockeE1Damage(player, target, E.Level());
}

static double E2Damage(const AIBaseClient& target, int qStacks = 0) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }
    return Damage::GetLockeE2Damage(player, target, E.Level()) +
        Damage::GetLockeAutoAttackDamage(
            player, target, Q.Level(), qStacks, true);
}

static int GetActiveBuffStacksDirect(uintptr_t obj, const char* name);

static double RDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }
    return Damage::GetLockeRDamage(player, target, R.Level());
}

// CDragon: ExecutionThreshold = [10%, 11%, 12%] by rank + 0.5% per LockeRStack
static float GetRExecuteThreshold() {
    const auto player = Player();
    if (!player.IsValid()) {
        return 0.0f;
    }

    const int rStacks = GetActiveBuffStacksDirect(player.Address(), "LockeRStack");
    return Damage::LockeRExecuteThreshold(R.Level(), rStacks);
}

static bool ComboCanKillWithoutE(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return false;
    }

    const int qStacks = GetActiveBuffStacksDirect(target.Address(), "LockeQ");
    double damage = Damage::GetLockeAutoAttackDamage(
        player, target, Q.Level(), qStacks, true);
    if (Q.IsReady()) {
        damage += QDamage(target);
    }
    return damage >= target.Health() + target.AllShield();
}

// Total combo damage: AA + Q + E1 AOE + E2 empowered AA
static double GetComboDamage(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }

    const int qStacks = GetActiveBuffStacksDirect(target.Address(), "LockeQ");
    const bool qWillCast = Q.IsReady();
    double damage = Damage::GetLockeAutoAttackDamage(
        player, target, Q.Level(), qStacks, true);
    if (qWillCast) {
        damage += QDamage(target);
    }
    if (E.IsReady()) {
        damage += EDamage(target);
        damage += E2Damage(target, qWillCast ? 1 : 0);
    }
    return damage;
}

// Direct buff enumeration — bypasses event cache which can return 0
// when multiple buffs share the same name (one inactive, one active).
static int GetActiveBuffStacksDirect(uintptr_t obj, const char* name) {
    uintptr_t buffs[256] = {};
    const int count = CoreBuffs::Enumerate(obj, buffs, 256);
    const float gameTime = CoreBuffs::ResolveGameTime();
    char buf[96] = {};
    int bestStacks = 0;
    for (int i = 0; i < count; ++i) {
        CoreBuffs::BuffRef buff{ buffs[i] };
        if (!buff.IsActive(gameTime)) {
            continue;
        }
        if (!buff.ReadName(buf, static_cast<int>(sizeof(buf)))) {
            continue;
        }
        if (CoreBuffs::NameMatchesQuery(buf, name)) {
            const int s = buff.GetStacks();
            if (s > bestStacks) {
                bestStacks = s;
            }
        }
    }
    return bestStacks;
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void Orbwalker_OnAfterAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void Combo();
static void ComboELogic();
static void ComboRLogic();
static void ComboWLogic();
static void Harass();
static void LaneClear();
static void JungleClear();
static void KillSteal();
static void OnDraw();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.ziblldev9898", "Locke", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuSlider("eMinStacks", "Min Q Stacks to E", 2, 1, 3));
    ComboMenu->Add(new MenuBool("useEGapclose", "Use E to Escape Gapclosers"));
    ComboMenu->Add(new MenuSlider("eGapcloseHpPercent", "Don't Escape Below HP %", 50, 0, 100));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuSlider("wRecastHpPercent", "Recast W Below HP %", 30, 0, 100));
    ComboMenu->Add(new MenuSlider("wRecastExpiryTime", "Recast W When Buff Has X*0.1s Left", 15, 5, 30));
    ComboMenu->Add(new MenuBool("useR", "Use R"));
    ComboMenu->Add(new MenuBool("rExecute", "Use R Execute", true));
    ComboMenu->Add(new MenuBool("rAoe", "Use R AoE", true));
    ComboMenu->Add(new MenuSlider("rMinEnemies", "Min Enemies for R AoE", 4, 1, 5));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("ManaHarass", "Mana Harass", 30, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("ManaLC", "Mana Clear", 30, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuSlider("ManaJC", "Mana Clear", 30, 0, 100));

    KillStealMenu = MenuRoot->AddSubMenu(new Menu("KillSteal Settings", "KillSteal"));
    KillStealMenu->Add(new MenuBool("killstealQ", "Use Q"));
    KillStealMenu->Add(new MenuBool("killstealR", "Use R"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    // CDragon locke.bin.json: Q castRange=950, mMissileWidth=60, mSpeed=1650, mCastTime=0.25
    Q = Spell(SpellSlot::Q, 950.0f);
    Q.SetSkillshot(0.25f, 60.0f, 1650.0f, false, SpellType::Line);

    // CDragon: W castRange=250, Self targeting, canCastWhileCC
    W = Spell(SpellSlot::W, 250.0f);

    // CDragon: E castRange=425, LocationClamped, castTime=0.175
    // E has 2 phases: teleport (LockeE) -> buff LockeEAttackReady -> empowered AA (LockeEAttack)
    // Logic E combo will be implemented later with W/R
    E = Spell(SpellSlot::E, 425.0f);
    E.Delay = 0.175f;

    // CDragon: R castRange=1000, castRadius=425, castTime=0.25, missileTravelTime=0.5
    R = Spell(SpellSlot::R, 1000.0f);
    R.SetSkillshot(0.25f, 425.0f, FLT_MAX, false, SpellType::Circle);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnAfterAttack += &Orbwalker_OnAfterAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#8ec5ff' size='20'>ziblldev9898 - Locke loaded</font>");
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
        // After AA: cast Q if ready
        // - E ready: Q to build stacks for E
        // - E cooldown: Q->AA loop (we're already in AA range since we just attacked)
        if (useQ && Q.IsReady()) {
            CastSkillshot(Q, target, HitChance::High, "after-attack-Q-combo");
        }
    } else if (Orbwalker::ActiveMode() == OrbwalkingMode::Harass) {
        if (qHarass && Q.IsReady()) {
            CastSkillshot(Q, target, HitChance::High, "after-attack-Q-harass");
        }
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

    // CDragon: After E teleport, player gets LockeEAttackReady buff.
    // The next auto-attack becomes LockeEAttack (empowered, mIgnoreRangeCheck=true).
    // Force orbwalker to attack target so the empowered AA triggers.
    if (player.HasBuff("LockeEAttackReady")) {
        const auto target = GetTarget(700.0f, DamageType::Magical);
        if (ValidHeroTarget(target, 700.0f)) {
            Orbwalker::ForceTarget(target);
        }
        return;
    }

    const auto mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo) {
    }

    switch (mode) {
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
    default:
        break;
    }

    KillSteal();
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (!ShouldRunNow(LastComboEvalTick, 80)) {
        return;
    }

    const auto target = GetTarget(Q.Range, DamageType::Physical);
    const bool useQ = Bool(ComboMenu, "useQ");
    const bool useE = Bool(ComboMenu, "useE");

    // Q logic:
    // - E ready: cast Q to build stacks (any range within Q.Range)
    // - E cooldown + target in AA range + 0 Q stacks: cast Q, then block Q for AA
    // - E cooldown + target in AA range + >=1 Q stack: don't cast Q, let orbwalker AA
    // - E cooldown + target outside AA range: save Q (don't cast)
    if (useQ && Q.IsReady() && ValidHeroTarget(target, Q.Range)) {
        if (E.IsReady()) {
            CastSkillshot(Q, target, HitChance::High, "combo-Q-stack");
        } else {
            const float aaRange = AutoAttack::GetRealAutoAttackRange(target);
            if (target.DistanceToPlayer() <= aaRange) {
                const int qStacks = GetActiveBuffStacksDirect(target.Address(), "LockeQ");
                if (qStacks == 0) {
                    CastSkillshot(Q, target, HitChance::High, "combo-Q-aaloop");
                }
            }
        }
    }

    const bool useW = Bool(ComboMenu, "useW");
    if (useW && W.IsReady()) {
        ComboWLogic();
    }

    if (useE && E.IsReady()) {
        ComboELogic();
    }

    const bool useR = Bool(ComboMenu, "useR");
    if (useR && R.IsReady()) {
        ComboRLogic();
    }
}

static void ComboELogic() {
    const auto player = Player();
    if (!player.IsValid() || !E.IsReady()) {
        return;
    }

    // Don't cast E phase 1 if we already have E attack buff (phase 2)
    if (player.HasBuff("LockeEAttackReady")) {
        return;
    }

    const int minStacks = Slider(ComboMenu, "eMinStacks", 2);
    const float eEffectiveRange = E.Range + 100.0f;

    // Two modes:
    // 1. Kill combo: enough damage to kill → E1 gapclose (no Q stacks needed), E2 attack finishes
    // 2. Normal combo: not enough damage → E1 AOE (needs Q stacks)
    AIHeroClient killTarget;      // nearest enemy we can kill with full combo
    float killTargetDist = FLT_MAX;
    AIHeroClient normalTarget;    // enemy with best Q stacks within E AOE range
    int bestStacks = 0;

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) {
            continue;
        }

        const float dist = enemy.DistanceToPlayer();
        const double comboDmg = GetComboDamage(enemy);
        const bool canKill = comboDmg >= enemy.Health() + enemy.AllShield();

        // Kill combo: search within Q.Range, no stacks needed, pick nearest
        if (canKill && dist <= Q.Range && dist < killTargetDist) {
            killTarget = enemy;
            killTargetDist = dist;
        }

        // Normal combo: search within E AOE range, need stacks
        if (!canKill && dist <= eEffectiveRange) {
            const int stacks = GetActiveBuffStacksDirect(enemy.Address(), "LockeQ");
            if (stacks >= minStacks && stacks > bestStacks) {
                normalTarget = enemy;
                bestStacks = stacks;
            }
        }
    }

    // Priority 1: Kill combo — E1 gapclose, E2 attack will finish
    if (killTarget.IsValid()) {
        float castDist = killTargetDist;
        if (castDist < 150.0f) {
            castDist = 150.0f;
        }
        if (castDist > E.Range) {
            castDist = E.Range;
        }

        const Vector3 castEPos = ExtendFromPlayer(killTarget.Position(), castDist);
        if (castEPos.IsZero()) {
            return;
        }
        CastPosition(E, castEPos, "combo-E-kill", killTarget);
        return;
    }

    // Priority 2: Normal combo — E1 AOE damage, need Q stacks
    if (normalTarget.IsValid()) {
        const float targetDist = normalTarget.DistanceToPlayer();
        float castDist = targetDist;
        if (castDist < 150.0f) {
            castDist = 150.0f;
        }
        if (castDist > E.Range) {
            castDist = E.Range;
        }

        const Vector3 castEPos = ExtendFromPlayer(normalTarget.Position(), castDist);
        if (castEPos.IsZero()) {
            return;
        }
        CastPosition(E, castEPos, "combo-E-normal", normalTarget);
        return;
    }
}

static void ComboRLogic() {
    const auto player = Player();
    if (!player.IsValid() || !R.IsReady()) {
        return;
    }

    const bool rExecute = Bool(ComboMenu, "rExecute");
    const bool rAoe = Bool(ComboMenu, "rAoe");
    const int minEnemies = Slider(ComboMenu, "rMinEnemies", 4);
    const float executeThreshold = GetRExecuteThreshold();

    // Mode 1: R Execute — find lowest HP% enemy below threshold
    if (rExecute) {
        AIHeroClient executeTarget;
        float lowestHpPercent = 1.0f;

        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) {
                continue;
            }
            if (enemy.DistanceToPlayer() > R.Range) {
                continue;
            }
            if (enemy.HasBuff("JudicatorIntervention") ||
                enemy.HasBuff("kindredrnodeathbuff") ||
                enemy.HasBuff("Undying Rage") ||
                enemy.HasBuff("FioraW") ||
                enemy.HasBuff("BlitzcrankManaBarrierCO")) {
                continue;
            }

            const float maxHp = enemy.MaxHealth();
            if (maxHp <= 0.0f) {
                continue;
            }
            const float hpPercent = (enemy.Health() + enemy.AllShield()) / maxHp;

            // Check execute threshold OR raw damage kill
            const bool thresholdMet = hpPercent <= executeThreshold;
            const bool damageKill = RDamage(enemy) >= enemy.Health() + enemy.AllShield();

            if (thresholdMet || damageKill) {
                if (hpPercent < lowestHpPercent) {
                    lowestHpPercent = hpPercent;
                    executeTarget = enemy;
                }
            }
        }

        if (executeTarget.IsValid()) {
            CastPosition(R, executeTarget.Position(), "combo-R-execute", executeTarget);
            return;
        }
    }

    // Mode 2: R AoE — find position hitting most enemies
    if (rAoe) {
        std::vector<AIHeroClient> enemiesInRange;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) {
                continue;
            }
            if (enemy.DistanceToPlayer() > R.Range + R.Width) {
                continue;
            }
            enemiesInRange.push_back(enemy);
        }

        if (static_cast<int>(enemiesInRange.size()) >= minEnemies) {
            // Find best cluster: try each enemy as seed, collect all within R.Width,
            // then compute centroid of the group for optimal AoE coverage
            Vector3 bestPos = {};
            int bestCount = 0;

            for (const auto& seed : enemiesInRange) {
                const Vector3 seedPos = seed.Position();
                std::vector<Vector3> cluster;
                for (const auto& other : enemiesInRange) {
                    if (seedPos.Distance2D(other.Position()) <= R.Width) {
                        cluster.push_back(other.Position());
                    }
                }
                const int count = static_cast<int>(cluster.size());
                if (count <= bestCount) {
                    continue;
                }
                // Compute centroid of cluster
                Vector3 centroid = {};
                for (const auto& pos : cluster) {
                    centroid.x += pos.x;
                    centroid.z += pos.z;
                }
                centroid.x /= static_cast<float>(count);
                centroid.z /= static_cast<float>(count);
                // Verify centroid still covers all enemies in cluster
                int covered = 0;
                for (const auto& pos : cluster) {
                    const float dx = pos.x - centroid.x;
                    const float dz = pos.z - centroid.z;
                    if (dx * dx + dz * dz <= R.Width * R.Width) {
                        covered++;
                    }
                }
                if (covered > bestCount) {
                    bestCount = covered;
                    bestPos = centroid;
                }
            }

            if (bestCount >= minEnemies) {
                // Clamp cast position to R.Range from player
                const float dist = player.Position().Distance2D(bestPos);
                Vector3 castPos = bestPos;
                if (dist > R.Range) {
                    castPos = ExtendFromPlayer(bestPos, R.Range);
                }
                CastPosition(R, castPos, "combo-R-aoe", AIHeroClient());
                return;
            }
        }
    }
}

static void ComboWLogic() {
    const auto player = Player();
    if (!player.IsValid() || !W.IsReady()) {
        return;
    }

    const bool hasWBuff = player.HasBuff("LockeW");

    // State 1: W not active — cast W when engaging
    if (!hasWBuff) {
        // Guard: if we just cast W, the LockeW buff may not have applied yet.
        // W.IsReady() stays true during the recast window, so without this guard
        // we'd re-enter State 1 and waste the recast (heal) immediately.
        if (LastWCastTick != 0 && GetTickCount() - LastWCastTick < 1000) {
            return;
        }

        bool shouldCast = false;

        // Check: enemy in AA range
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) {
                continue;
            }
            const float aaRange = AutoAttack::GetRealAutoAttackRange(enemy);
            if (enemy.DistanceToPlayer() <= aaRange) {
                shouldCast = true;
                break;
            }
        }

        // Check: E on cooldown (just used E = engaged)
        if (!shouldCast && !E.IsReady() && E.Level() > 0) {
            shouldCast = true;
        }

        if (shouldCast) {
            W.Cast();
            LastWCastTick = GetTickCount();
        }
        return;
    }

    // State 2: W active — consider recast for heal
    const int recastHpPercent = Slider(ComboMenu, "wRecastHpPercent", 30);
    const int recastExpiryRaw = Slider(ComboMenu, "wRecastExpiryTime", 15);
    const float recastExpirySec = static_cast<float>(recastExpiryRaw) * 0.1f;

    const float playerHpPercent = player.HealthPercent();
    bool shouldRecast = false;

    // Check: HP below threshold
    if (playerHpPercent <= static_cast<float>(recastHpPercent)) {
        shouldRecast = true;
    }

    // Check: buff about to expire
    if (!shouldRecast) {
        const float gameTime = CoreBuffs::ResolveGameTime();
        const auto wBuff = CoreBuffs::FindActiveByName(player.Address(), "LockeW", gameTime);
        if (wBuff.IsValid()) {
            const float remaining = wBuff.GetRemainingTime(gameTime);
            if (remaining > 0.0f && remaining <= recastExpirySec) {
                shouldRecast = true;
            }
        }
    }

    if (shouldRecast) {
        W.Cast();
        LastWCastTick = GetTickCount();
    }
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(ComboMenu, "useEGapclose") || !E.IsReady()) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    const auto sender = AIHeroClient(args.Sender);
    if (!ValidHeroTarget(sender, 900.0f)) {
        return;
    }

    // Don't escape if player HP is below threshold — save E for offensive combo
    const int hpThreshold = Slider(ComboMenu, "eGapcloseHpPercent", 50);
    if (player.HealthPercent() < static_cast<float>(hpThreshold)) {
        return;
    }

    // Don't escape if we can kill the enemy without E — fight instead of flee
    if (ComboCanKillWithoutE(sender)) {
        return;
    }

    // Only escape if enemy is close enough to be a threat
    if (sender.DistanceToPlayer() > 600.0f &&
        args.End.Distance2D(player.Position()) > 300.0f) {
        return;
    }

    // Teleport away from enemy
    const Vector3 escapePos = ExtendFromPlayer(sender.Position(), -E.Range);
    if (escapePos.IsZero()) {
        return;
    }

    CastPosition(E, escapePos, "gapclose-E-escape", sender);
}

static void Harass() {
    if (!Bool(HarassMenu, "useQ") || !Q.IsReady()) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const int minMana = Slider(HarassMenu, "ManaHarass", 30);
    if (player.ManaPercent() < static_cast<float>(minMana)) {
        return;
    }

    if (!ShouldRunNow(LastHarassEvalTick, 80)) {
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
    const int minMana = Slider(LaneClearMenu, "ManaLC", 30);
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
    const float manaPercent = player.ManaPercent();
    const int minMana = Slider(JungleClearMenu, "ManaJC", 30);

    if (!useQ || manaPercent < static_cast<float>(minMana)) {
        return;
    }

    if (!Q.IsReady()) {
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
                       !mob.IsVisible() ||
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

static void KillSteal() {
    // Q Killsteal
    if (Bool(KillStealMenu, "killstealQ") && Q.IsReady()) {
        for (const auto& target : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(target, Q.Range) ||
                target.HasBuff("JudicatorIntervention") ||
                target.HasBuff("kindredrnodeathbuff") ||
                target.HasBuff("Undying Rage") ||
                target.HasBuff("FioraW") ||
                target.HasBuff("BlitzcrankManaBarrierCO")) {
                continue;
            }

            const double damage = QDamage(target);
            const float shieldedHealth = target.Health() + target.AllShield();
            if (shieldedHealth <= damage) {
                CastSkillshot(Q, target, HitChance::High, "killsteal-Q");
                return;
            }
        }
    }

    // R Killsteal
    if (Bool(KillStealMenu, "killstealR") && R.IsReady()) {
        const float executeThreshold = GetRExecuteThreshold();

        for (const auto& target : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(target, R.Range) ||
                target.HasBuff("JudicatorIntervention") ||
                target.HasBuff("kindredrnodeathbuff") ||
                target.HasBuff("Undying Rage") ||
                target.HasBuff("FioraW") ||
                target.HasBuff("BlitzcrankManaBarrierCO")) {
                continue;
            }

            const float maxHp = target.MaxHealth();
            if (maxHp <= 0.0f) {
                continue;
            }
            const float hpPercent = (target.Health() + target.AllShield()) / maxHp;
            const bool thresholdMet = hpPercent <= executeThreshold;
            const bool damageKill = RDamage(target) >= target.Health() + target.AllShield();

            if (thresholdMet || damageKill) {
                CastPosition(R, target.Position(), "killsteal-R", target);
                return;
            }
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnAfterAttack -= &Orbwalker_OnAfterAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
    LastWCastTick = 0;
}

static void OnDraw() {
    if (!Loaded) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (!Drawing::IsEnabled()) {
        return;
    }

    // Draw E range circle
    Drawing::DrawCircle(player.Position(), E.Range, 0xFF00FFFF);

    // Draw R range circle when ready
    if (R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFF00FF);
    }

    // Draw W range circle when ready
    if (W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFF00FFAA);
    }

    // Draw Q stacks on each enemy
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) {
            continue;
        }

        int stacks = 0;
        try {
            stacks = GetActiveBuffStacksDirect(enemy.Address(), "LockeQ");
        } catch (...) {
            continue;
        }

        if (stacks <= 0) {
            continue;
        }

        const float dist = enemy.DistanceToPlayer();
        char text[64] = {};
        _snprintf_s(text, sizeof(text), _TRUNCATE, "Q: %d (dist=%.0f)", stacks, dist);

        const uint32_t color = stacks >= 3 ? 0xFF00FF00 : (stacks >= 2 ? 0xFFFFFF00 : 0xFFFF8800);

        Vec2 screenPos = {};
        const Vec3 worldPos = enemy.Position();
        if (Drawing::WorldToScreen(worldPos, screenPos) && screenPos.IsValid()) {
            Drawing::DrawText(screenPos.x - 30.0f, screenPos.y - 50.0f, color, text);
        }
    }
}

} // namespace Plugins::ziblldev9898::Locke
