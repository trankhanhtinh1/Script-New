#pragma once

#include "Common.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::AIO7UP::Viktor {

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* TestMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* WaveMenu = nullptr;
inline Menu* LastHitMenu = nullptr;
inline Menu* FleeMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* ROneTargetMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 600.0f };
inline Spell W{ SpellSlot::W, 800.0f };
inline Spell E{ SpellSlot::E, 500.0f };
inline Spell R{ SpellSlot::R, 700.0f };
inline Spell EMax{ SpellSlot::E, 1225.0f };

inline bool Loaded = false;
inline DWORD LastUpdateTick = 0;
inline DWORD LastClearTick = 0;
inline DWORD LastRFollowTick = 0;
inline DWORD LastAutoWTick = 0;

static constexpr float kEMaxRange = 1225.0f;
static constexpr float kELength = 700.0f;
static constexpr float kESpeed = 1050.0f;
static constexpr float kERange = 500.0f;

using namespace Common;

struct LaserFarmLocation {
    Vector2 Start = {};
    Vector2 End = {};
    int MinionsHit = 0;
};

static bool IsRInitialCast() {
    const std::string name = R.Instance().Name();
    return _stricmp(name.c_str(), "ViktorChaosStorm") == 0;
}

static bool HasActiveChaosStorm() {
    const auto player = Player();
    if (player.IsValid() &&
        (player.HasBuff("ViktorChaosStorm") ||
         player.HasBuff("viktorchaosstorm") ||
         player.HasBuff("ViktorChaosStormGuide") ||
         player.HasBuff("viktorchaosstormguide"))) {
        return true;
    }

    const std::string name = R.Instance().Name();
    return _stricmp(name.c_str(), "ViktorChaosStormGuide") == 0;
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.7upaio", "7UP - Viktor", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo"));
    ComboMenu->Add(new MenuBool("comboUseQ", "Use Q"));
    ComboMenu->Add(new MenuBool("comboUseW", "Use W"));
    ComboMenu->Add(new MenuBool("comboUseE", "Use E"));
    ComboMenu->Add(new MenuBool("comboUseR", "Use R"));
    ComboMenu->Add(new MenuBool("qAuto", "Dont autoattack without passive", false));
    ComboMenu->Add(new MenuKeyBind("comboActive", "Combo active", SDK::Keys::Space, KeyBindType::Press))->Permashow();

    RMenu = MenuRoot->AddSubMenu(new Menu("Rconfig", "R config"));
    RMenu->Add(new MenuList("HitR", "Auto R if: ", { "1 target", "2 targets", "3 targets", "4 targets", "5 targets" }, 3));
    RMenu->Add(new MenuBool("AutoFollowR", "Auto Follow R"));
    RMenu->Add(new MenuSlider("rTicks", "Ultimate ticks to count", 2, 1, 14));
    ROneTargetMenu = RMenu->AddSubMenu(new Menu("Ronetarget", "R one target"));
    ROneTargetMenu->Add(new MenuKeyBind("forceR", "Force R on target", SDK::Keys::T, KeyBindType::Press))->Permashow();
    ROneTargetMenu->Add(new MenuBool("rLastHit", "1 target ulti"));
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        const std::string key = "RU" + hero.CharacterName();
        const std::string label = "Use R on: " + hero.CharacterName();
        ROneTargetMenu->Add(new MenuBool(key.c_str(), label.c_str()));
    }

    TestMenu = MenuRoot->AddSubMenu(new Menu("Testfeatures", "Test features"));
    TestMenu->Add(new MenuBool("spPriority", "Prioritize kill over dmg"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass"));
    HarassMenu->Add(new MenuBool("harassUseQ", "Use Q"));
    HarassMenu->Add(new MenuBool("harassUseE", "Use E"));
    HarassMenu->Add(new MenuSlider("harassMana", "Mana usage in percent (%)", 30, 0, 100));
    HarassMenu->Add(new MenuSlider("eDistance", "Harass range with E", static_cast<int>(kEMaxRange), static_cast<int>(kERange), static_cast<int>(kEMaxRange)));
    HarassMenu->Add(new MenuKeyBind("harassActive", "Harass active", SDK::Keys::C, KeyBindType::Press))->Permashow();

    WaveMenu = MenuRoot->AddSubMenu(new Menu("WaveClear", "WaveClear"));
    WaveMenu->Add(new MenuBool("waveUseQ", "Use Q"));
    WaveMenu->Add(new MenuBool("waveUseE", "Use E"));
    WaveMenu->Add(new MenuSlider("waveNumE", "Minions to hit with E", 2, 1, 10));
    WaveMenu->Add(new MenuSlider("waveMana", "Mana usage in percent (%)", 30, 0, 100));
    WaveMenu->Add(new MenuKeyBind("waveActive", "WaveClear active", SDK::Keys::V, KeyBindType::Press))->Permashow();
    WaveMenu->Add(new MenuKeyBind("jungleActive", "JungleClear active", SDK::Keys::G, KeyBindType::Press))->Permashow();

    LastHitMenu = MenuRoot->AddSubMenu(new Menu("LastHit", "LastHit"));
    LastHitMenu->Add(new MenuKeyBind("waveUseQLH", "Use Q", SDK::Keys::A, KeyBindType::Press))->Permashow();

    FleeMenu = MenuRoot->AddSubMenu(new Menu("Flee", "Flee"));
    FleeMenu->Add(new MenuKeyBind("FleeActive", "Flee mode", SDK::Keys::Z, KeyBindType::Press))->Permashow();

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc", "Misc"));
    MiscMenu->Add(new MenuBool("rInterrupt", "Use R to interrupt dangerous spells"));
    MiscMenu->Add(new MenuBool("wInterrupt", "Use W to interrupt dangerous spells"));
    MiscMenu->Add(new MenuBool("autoW", "Use W to continue CC"));
    MiscMenu->Add(new MenuBool("miscGapcloser", "Use W against gapclosers"));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Drawings", "Drawings"));
    DrawMenu->Add(new MenuBool("drawRangeQ", "Draw Q Range"));
    DrawMenu->Add(new MenuBool("drawRangeW", "Draw W Range"));
    DrawMenu->Add(new MenuBool("drawRangeE", "Draw E Range"));
    DrawMenu->Add(new MenuBool("drawRangeR", "Draw R Range"));

    MenuRoot->Attach();
}

static float DistancePointSegmentSqr(const Vector2& point, const Vector2& start, const Vector2& end) {
    const Vector2 segment = end - start;
    const float lenSqr = segment.LengthSqr();
    if (lenSqr <= FLT_EPSILON) {
        return point.DistanceSqr(start);
    }

    const float t = std::clamp(((point - start).Dot(segment)) / lenSqr, 0.0f, 1.0f);
    const Vector2 projection = start + segment * t;
    return point.DistanceSqr(projection);
}

static bool CastE(const Vector3& start, const Vector3& end) {
    // Death Ray is a two-position (vector) skillshot. E.Cast(start, end) routes
    // through CoreCastSpell::CastVectorSpell → the native CastSpellVector
    // (sub_97A980) path, which emits the opcode-271 cast packet with explicit
    // world start/end coords — so the ray direction is exact (no cursor raycast).
    // Reset the prediction-mutated geometry first; the vector cast itself only
    // consumes the verbatim start/end below.
    E.Range = kERange;
    E.From = {};
    E.RangeCheckFrom = {};
    E.Speed = kESpeed;
    return E.Cast(start, end);
}

static void ResetEGeometry() {
    E.Range = kERange;
    E.From = {};
    E.RangeCheckFrom = {};
    E.Speed = kESpeed;
}

static Vector3 BuildLaserEnd(const Vector3& start,
                             const Vector3& desiredEnd,
                             const Vector3& targetPosition) {
    const auto player = Player();
    Vector3 end = desiredEnd;
    if (!end.IsValid() || end.IsZero() || start.Distance2D(end) < 25.0f) {
        Vector3 directionPoint = targetPosition;
        if (player.IsValid() && player.Position().Distance2D(targetPosition) > 25.0f) {
            directionPoint = player.Position().Extend(
                targetPosition,
                player.Position().Distance2D(targetPosition) + kELength);
        }
        end = start.Extend(directionPoint, kELength);
    }

    if (player.IsValid() && start.Distance2D(end) < 25.0f) {
        end = start.Extend(player.Position(), -kELength);
    }

    end.y = NavMesh::GetHeightForPosition(end);
    return end;
}

static Vector3 SelectOuterEStart(const Vector3& startPoint, const AIHeroClient& target) {
    AIBaseClient best;
    float bestHealth = -1.0f;

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (enemy.Address() == target.Address() ||
            !ValidHeroTarget(enemy, kEMaxRange) ||
            enemy.DistanceToPlayer() >= kERange ||
            enemy.Position().Distance2D(startPoint) > 150.0f) {
            continue;
        }

        if (enemy.Health() > bestHealth) {
            best = enemy;
            bestHealth = enemy.Health();
        }
    }

    if (best.IsValid()) {
        return best.Position();
    }

    for (const auto& minion : EnemyLaneMinionBases(kEMaxRange)) {
        if (minion.DistanceToPlayer() >= kERange ||
            minion.Position().Distance2D(startPoint) > 150.0f) {
            continue;
        }

        if (minion.Health() > bestHealth) {
            best = minion;
            bestHealth = minion.Health();
        }
    }

    return best.IsValid() ? best.Position() : startPoint;
}

static bool PredictCastE(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !E.IsReady() || !ValidHeroTarget(target, kEMaxRange)) {
        return false;
    }

    const bool inRange = target.Position().Distance2D(player.Position()) < kERange;
    Vector3 start;
    Vector3 end;
    bool canCast = false;

    if (inRange) {
        E.Speed = kESpeed * 0.9f;
        E.From = target.Position().Extend(player.Position(), kELength * 0.1f);
        E.RangeCheckFrom = player.Position();
        E.Range = kERange;

        const auto startPrediction = E.GetPrediction(target);
        if (startPrediction.GetCastPosition().Distance2D(player.Position()) < kERange) {
            start = startPrediction.GetCastPosition();
        } else {
            start = target.Position();
            E.Speed = kESpeed;
        }

        start.y = NavMesh::GetHeightForPosition(start);
        E.From = start;
        E.RangeCheckFrom = start;
        E.Range = kELength;

        const auto endPrediction = E.GetPrediction(target);
        canCast = HitchanceAtLeast(endPrediction.Hitchance, HitChance::High);
        end = BuildLaserEnd(start, endPrediction.GetCastPosition(), target.Position());
    } else {
        const Vector3 startPoint = player.Position().Extend(target.Position(), kERange);
        start = SelectOuterEStart(startPoint, target);
        start.y = NavMesh::GetHeightForPosition(start);

        E.Speed = kESpeed;
        E.From = start;
        E.RangeCheckFrom = start;
        E.Range = kELength;

        const auto prediction = E.GetPrediction(target);
        canCast = HitchanceAtLeast(prediction.Hitchance, HitChance::High);
        end = BuildLaserEnd(start, prediction.GetCastPosition(), target.Position());
    }

    if (start.Distance2D(end) > kELength + target.BoundingRadius()) {
        canCast = false;
    }

    if (!canCast) {
        ResetEGeometry();
        return false;
    }

    return CastE(start, end);
}

static LaserFarmLocation GetBestLaserFarmLocation(bool jungle) {
    const auto player = Player();
    LaserFarmLocation best;
    if (!player.IsValid()) {
        return best;
    }

    std::vector<AIBaseClient> units = jungle ? JungleBases(kEMaxRange) : EnemyLaneMinionBases(kEMaxRange);
    if (units.empty()) {
        return best;
    }

    std::vector<Vector2> positions;
    positions.reserve(units.size() * units.size());
    for (const auto& unit : units) {
        positions.push_back(unit.Position().To2D());
    }

    const int originalCount = static_cast<int>(positions.size());
    for (int i = 0; i < originalCount; ++i) {
        for (int j = i + 1; j < originalCount; ++j) {
            positions.push_back((positions[i] + positions[j]) * 0.5f);
        }
    }

    for (const auto& startUnit : units) {
        if (player.Distance(startUnit) > kERange) {
            continue;
        }

        const Vector2 start = startUnit.Position().To2D();
        for (const auto& pos : positions) {
            if (pos.DistanceSqr(start) > kELength * kELength) {
                continue;
            }

            const Vector2 end = start + (pos - start).Normalized() * kELength;
            int hitCount = 0;
            for (const auto& unitPos : positions) {
                if (DistancePointSegmentSqr(unitPos, start, end) <= 140.0f * 140.0f) {
                    ++hitCount;
                }
            }

            if (hitCount >= best.MinionsHit) {
                best.Start = start;
                best.End = end;
                best.MinionsHit = hitCount;
            }
        }
    }

    const int minimumHit = Slider(WaveMenu, "waveNumE", 2);
    if ((!jungle && best.MinionsHit < minimumHit) || (jungle && best.MinionsHit <= 0)) {
        best.MinionsHit = 0;
    }
    return best;
}

static bool PredictCastMinionE(bool jungle) {
    const auto farm = GetBestLaserFarmLocation(jungle);
    if (farm.MinionsHit <= 0) {
        return false;
    }

    return CastE(Vector3::From2D(farm.Start), Vector3::From2D(farm.End));
}

static float QEmpoweredDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    static constexpr float base[] = { 20.0f, 40.0f, 60.0f, 80.0f, 100.0f };
    const int level = std::clamp(Q.Level(), 1, 5);
    return player.CalculateMagicDamage(target, base[level - 1] + 0.5f * player.AP() + player.AD());
}

static float TotalDmg(const AIBaseClient& enemy, bool useQ, bool useE, bool useR, bool qRange) {
    const auto player = Player();
    if (!player.IsValid() || !enemy.IsValid()) {
        return 0.0f;
    }

    float damage = 0.0f;
    const bool inQRange = !qRange || enemy.DistanceToPlayer() <= Q.Range + player.BoundingRadius() + enemy.BoundingRadius();

    if (useQ && inQRange) {
        if (Q.IsReady()) {
            damage += player.GetSpellDamage(enemy, SpellSlot::Q);
            damage += QEmpoweredDamage(enemy);
        } else if (player.HasBuff("viktorpowertransferreturn")) {
            damage += QEmpoweredDamage(enemy);
        }
    }

    if (useE && E.IsReady()) {
        damage += player.GetSpellDamage(enemy, SpellSlot::E);
    }

    if (useR && R.Level() > 0 && R.IsReady() && IsRInitialCast()) {
        const int ticks = Slider(RMenu, "rTicks", 2);
        damage += player.GetSpellDamage(enemy, SpellSlot::R) * static_cast<float>(ticks + 1);
    }

    if (player.HasItem(3100)) {
        damage += player.CalculateMagicDamage(enemy, 0.5f * player.AP() + 0.75f * player.BaseAttackDamage());
    }
    if (player.HasItem(3057)) {
        damage += player.CalculatePhysicalDamage(enemy, 0.5f * player.BaseAttackDamage());
    }

    return damage;
}

static bool ShouldAllowAttacks() {
    const auto player = Player();
    if (!player.IsValid()) {
        return true;
    }

    if (Key(ComboMenu, "comboActive") || Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
        if (!Bool(ComboMenu, "qAuto", false)) {
            return true;
        }

        const bool qBlocked = !Q.IsReady() || player.Mana() < Q.Instance().ManaCost();
        const bool eBlocked = !E.IsReady() || player.Mana() < E.Instance().ManaCost();
        const bool qAutoOk = player.HasBuff("viktorpowertransferreturn");
        return qBlocked && eBlocked && qAutoOk;
    }

    if (Key(HarassMenu, "harassActive") || Orbwalker::ActiveMode() == OrbwalkingMode::Harass) {
        return (!Q.IsReady() || player.Mana() < Q.Instance().ManaCost()) &&
               (!E.IsReady() || player.Mana() < E.Instance().ManaCost());
    }

    return true;
}

static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (AIBaseClient(args.Target.Handle()).IsHero()) {
        args.Process = ShouldAllowAttacks();
    }
}

static void QLastHit(const AIBaseClient& minion) {
    if (!Q.IsReady()) {
        return;
    }

    const bool castQ = Key(LastHitMenu, "waveUseQLH") ||
        (Bool(WaveMenu, "waveUseQ") && Key(WaveMenu, "waveActive"));
    if (!castQ || !ValidTarget(minion, Q.Range)) {
        return;
    }

    if (Q.GetDamage(minion) > minion.Health()) {
        CastUnit(Q, minion);
    }
}

static void OnNonKillableMinion(OrbwalkingActionArgs& args) {
    const auto target = AIBaseClient(args.Target.Handle());
    if (ValidTarget(target, Q.Range)) {
        QLastHit(target);
    }
}

static bool RAllowedOn(const AIHeroClient& hero) {
    if (!hero.IsValid()) {
        return false;
    }

    const std::string key = "RU" + hero.CharacterName();
    return Bool(ROneTargetMenu, key.c_str(), true);
}

static void OnCombo() {
    const bool useQ = Bool(ComboMenu, "comboUseQ") && Q.IsReady();
    const bool useW = Bool(ComboMenu, "comboUseW") && W.IsReady();
    const bool useE = Bool(ComboMenu, "comboUseE") && E.IsReady();
    const bool useR = Bool(ComboMenu, "comboUseR") && R.IsReady();

    auto eTarget = GetTarget(kEMaxRange, DamageType::Magical);
    const auto qTarget = GetTarget(Q.Range, DamageType::Magical);
    const auto rTarget = GetTarget(R.Range, DamageType::Magical);

    if (Bool(TestMenu, "spPriority") && R.IsReady() &&
        qTarget.IsValid() && eTarget.IsValid() && qTarget.Address() != eTarget.Address() &&
        qTarget.Health() < TotalDmg(qTarget, true, true, false, false)) {
        eTarget = qTarget;
    }

    if (useR && IsRInitialCast() && ValidHeroTarget(rTarget, R.Range) &&
        Bool(ROneTargetMenu, "rLastHit") && RAllowedOn(rTarget) &&
        TotalDmg(rTarget, true, true, false, false) < rTarget.Health() &&
        TotalDmg(rTarget, true, true, true, true) > rTarget.Health()) {
        CastPosition(R, rTarget.Position(), rTarget);
        return;
    }

    if (useE && ValidHeroTarget(eTarget, kEMaxRange) && PredictCastE(eTarget)) {
        return;
    }

    if (useQ && ValidHeroTarget(qTarget, Q.Range)) {
        CastUnit(Q, qTarget);
        return;
    }

    if (useW) {
        const auto target = GetTarget(W.Range, DamageType::Magical);
        if (ValidHeroTarget(target, W.Range) &&
            (target.GetPathLength() < 2 || target.CountEnemyHeroesInRange(250.0f) > 2)) {
            const auto pred = W.GetPrediction(target);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
                CastPosition(W, pred.GetCastPosition(), target);
                return;
            }
        }
    }

    if (useR && IsRInitialCast()) {
        const int minTargets = ListIndex(RMenu, "HitR", 3) + 1;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, R.Range)) {
                continue;
            }

            const auto pred = R.GetPrediction(enemy, true);
            if (pred.AoeTargetsHitCount >= minTargets &&
                HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                CastPosition(R, pred.GetCastPosition(), enemy);
                return;
            }
        }
    }
}

static void OnHarass() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() < static_cast<float>(Slider(HarassMenu, "harassMana", 30))) {
        return;
    }

    if (Bool(HarassMenu, "harassUseQ") && Q.IsReady()) {
        const auto target = GetTarget(Q.Range, DamageType::Magical);
        if (ValidHeroTarget(target, Q.Range)) {
            CastUnit(Q, target);
            return;
        }
    }

    if (Bool(HarassMenu, "harassUseE") && E.IsReady()) {
        const float range = static_cast<float>(Slider(HarassMenu, "eDistance", static_cast<int>(kEMaxRange)));
        const auto target = GetTarget(range, DamageType::Magical);
        if (ValidHeroTarget(target, range)) {
            PredictCastE(target);
        }
    }
}

static void OnWaveClear() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() < static_cast<float>(Slider(WaveMenu, "waveMana", 30))) {
        return;
    }

    if (!ShouldRunNow(LastClearTick, 120)) {
        return;
    }

    if (Bool(WaveMenu, "waveUseQ") && Q.IsReady()) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (ValidTarget(minion, player.AttackRange()) &&
                minion.CharacterName().find("Siege") != std::string::npos &&
                Q.GetDamage(minion) > minion.Health()) {
                QLastHit(minion);
                break;
            }
        }
    }

    if (Bool(WaveMenu, "waveUseE") && E.IsReady()) {
        PredictCastMinionE(false);
    }
}

static void OnJungleClear() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() < static_cast<float>(Slider(WaveMenu, "waveMana", 30))) {
        return;
    }

    if (!ShouldRunNow(LastClearTick, 120)) {
        return;
    }

    if (Bool(WaveMenu, "waveUseQ") && Q.IsReady()) {
        auto mobs = GameObjects::Jungle();
        std::sort(mobs.begin(), mobs.end(), [](const AIMinionClient& a, const AIMinionClient& b) {
            return a.MaxHealth() > b.MaxHealth();
        });
        for (const auto& mob : mobs) {
            if (ValidTarget(mob, Q.Range) && CastUnit(Q, mob)) {
                return;
            }
        }
    }

    if (Bool(WaveMenu, "waveUseE") && E.IsReady()) {
        PredictCastMinionE(true);
    }
}

static void Flee() {
    Orbwalker::Move(Game::CursorPos());

    const auto player = Player();
    if (!player.IsValid() || !Q.IsReady() ||
        !(player.HasBuff("viktorqaug") || player.HasBuff("viktorqeaug") ||
          player.HasBuff("viktorqwaug") || player.HasBuff("viktorqweaug"))) {
        return;
    }

    AIBaseClient best;
    float bestDistance = Q.Range;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, Q.Range) && player.Distance(minion) < bestDistance) {
            best = AIBaseClient(minion.Handle());
            bestDistance = player.Distance(minion);
        }
    }
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(hero, Q.Range) && player.Distance(hero) < bestDistance) {
            best = AIBaseClient(hero.Handle());
            bestDistance = player.Distance(hero);
        }
    }

    if (best.IsValid()) {
        CastUnit(Q, best);
    }
}

static void AutoW() {
    if (!W.IsReady() || !Bool(MiscMenu, "autoW")) {
        return;
    }
    if (!ShouldRunNow(LastAutoWTick, 120)) {
        return;
    }

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, W.Range)) {
            continue;
        }

        const bool hardCc =
            CoreBuffs::HasBuffType(enemy.Address(), 5) ||
            CoreBuffs::HasBuffType(enemy.Address(), 8) ||
            CoreBuffs::HasBuffType(enemy.Address(), 9) ||
            CoreBuffs::HasBuffType(enemy.Address(), 11) ||
            CoreBuffs::HasBuffType(enemy.Address(), 22) ||
            CoreBuffs::HasBuffType(enemy.Address(), 24) ||
            enemy.IsRecalling();
        const auto pred = W.GetPrediction(enemy);
        if (hardCc || pred.Hitchance == HitChance::Immobile) {
            CastPosition(W, pred.GetCastPosition(), enemy);
            return;
        }
    }
}

static void ForceR() {
    if (!Key(ROneTargetMenu, "forceR") || !R.IsReady()) {
        return;
    }

    const auto target = GetTarget(R.Range, DamageType::Magical);
    if (ValidHeroTarget(target, R.Range) && RAllowedOn(target)) {
        CastPosition(R, target.Position(), target);
    }
}

static void FollowR() {
    if (!Bool(RMenu, "AutoFollowR") || !HasActiveChaosStorm()) {
        return;
    }

    const DWORD now = GetTickCount();
    if (LastRFollowTick != 0 && now - LastRFollowTick < 500) {
        return;
    }

    const auto target = GetTarget(1100.0f, DamageType::Magical);
    if (ValidHeroTarget(target, 1100.0f) && CastPosition(R, target.Position(), target)) {
        LastRFollowTick = now;
    }
}

static void OnInterruptable(const Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    if (args.DangerLevel < DangerLevel::High) {
        return;
    }

    const auto unit = AIHeroClient(args.Sender);
    if (!ValidHeroTarget(unit, R.Range)) {
        return;
    }

    if (Bool(MiscMenu, "wInterrupt") && W.IsReady() && ValidHeroTarget(unit, W.Range)) {
        const auto pred = W.GetPrediction(unit);
        CastPosition(W, pred.GetCastPosition(), unit);
    } else if (Bool(MiscMenu, "rInterrupt") && R.IsReady() && IsRInitialCast()) {
        CastPosition(R, unit.Position(), unit);
    }
}

static void OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "miscGapcloser") || !W.IsReady()) {
        return;
    }

    const auto sender = AIHeroClient(args.Sender);
    const auto player = Player();
    if (player.IsValid() && ValidHeroTarget(sender, W.Range) &&
        args.End.Distance2D(player.Position()) < 200.0f) {
        CastPosition(W, args.End, sender);
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    if (!ShouldRunNow(LastUpdateTick, 40)) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() || Game::IsChatOpen()) {
        return;
    }

    if (Key(ComboMenu, "comboActive") || Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
        OnCombo();
    }
    if (Key(HarassMenu, "harassActive") || Orbwalker::ActiveMode() == OrbwalkingMode::Harass) {
        OnHarass();
    }
    if (Key(WaveMenu, "waveActive") || Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear) {
        OnWaveClear();
    }
    if (Key(WaveMenu, "jungleActive") || Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear) {
        OnJungleClear();
    }
    if (Key(FleeMenu, "FleeActive") || Orbwalker::ActiveMode() == OrbwalkingMode::Flee) {
        Flee();
    }

    ForceR();
    FollowR();
    AutoW();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 600.0f);
    W = Spell(SpellSlot::W, 800.0f);
    E = Spell(SpellSlot::E, kERange);
    R = Spell(SpellSlot::R, 700.0f);
    EMax = Spell(SpellSlot::E, kEMaxRange);

    Q.SetTargetted(0.25f, 2000.0f);
    W.SetSkillshot(0.25f, 300.0f, FLT_MAX, false, SpellType::Circle);
    E.SetSkillshot(0.0f, 80.0f, kESpeed, false, SpellType::Line);
    R.SetSkillshot(0.25f, 300.0f, FLT_MAX, false, SpellType::Circle);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnInterruptableTarget += &OnInterruptable;
    Events::hook.OnGapCloser += &OnGapcloser;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnNonKillableMinion += &OnNonKillableMinion;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>7UP - Viktor loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnInterruptableTarget -= &OnInterruptable;
    Events::hook.OnGapCloser -= &OnGapcloser;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnNonKillableMinion -= &OnNonKillableMinion;
    RemoveKeyPermashow(ComboMenu, "comboActive");
    RemoveKeyPermashow(ROneTargetMenu, "forceR");
    RemoveKeyPermashow(HarassMenu, "harassActive");
    RemoveKeyPermashow(WaveMenu, "waveActive");
    RemoveKeyPermashow(WaveMenu, "jungleActive");
    RemoveKeyPermashow(LastHitMenu, "waveUseQLH");
    RemoveKeyPermashow(FleeMenu, "FleeActive");

    Loaded = false;
}

} // namespace Plugins::AIO7UP::Viktor
