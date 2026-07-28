#pragma once

#include "YasuoHelper.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::Yasuo {

inline Menu* MenuRoot = nullptr;
inline Menu* RangeMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* EQMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* KeyMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 475.0f };
inline Spell Q3{ SpellSlot::Q, 1100.0f };
inline Spell W{ SpellSlot::W, 100.0f };
inline Spell E{ SpellSlot::E, 475.0f };
inline Spell R{ SpellSlot::R, 1400.0f };
inline Spell Flash{ SpellSlot::Unknown, 425.0f };
inline SpellSlot FlashSlot = SpellSlot::Unknown;

inline bool Loaded = false;
inline int LastE = 0;
inline int LastQ = 0;
inline int LastDashQ = 0;
inline int LastEQFlash = 0;
inline bool EObjectsReady = false;
inline std::vector<AIBaseClient> EObjectsCache{};

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnProcessSpell(const Events::ProcessSpellEventArgs& args);
static void OnDash(const Events::Dash::DashArgs& args);
static void OnDraw();
static void OnUnload();

static bool IsFlashReady() {
    return FlashSlot != SpellSlot::Unknown && Flash.IsReady();
}

static Spell& CurrentQ() {
    return HaveQ3() ? Q3 : Q;
}

static float CurrentQRange() {
    return HaveQ3()
        ? static_cast<float>(Slider(RangeMenu, "Q3Range", 1100))
        : static_cast<float>(Slider(RangeMenu, "QRange", 475));
}

static bool AllowTurret() {
    return Key(KeyMenu, "AllowTurret");
}

static bool CastPosition(Spell& spell, const Vector3& position) {
    return spell.IsReady() && !position.IsZero() && spell.Cast(position);
}

static bool CastE(const AIBaseClient& target) {
    if (!E.IsReady() || !ValidTarget(target, E.Range) || !CanE(target)) {
        return false;
    }

    if (!AllowDashTo(target, AllowTurret())) {
        return false;
    }

    if (E.CastOnUnit(target)) {
        LastE = SDK::Variables::TickCount();
        return true;
    }
    return false;
}

static void UpdateEObjectsCache() {
    EObjectsCache.clear();

    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, E.Range) && CanE(minion)) {
            EObjectsCache.push_back(AIBaseClient(minion.Handle()));
        }
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (ValidTarget(monster, E.Range) && CanE(monster)) {
            EObjectsCache.push_back(AIBaseClient(monster.Handle()));
        }
    }
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        if (ValidTarget(hero, E.Range) && CanE(hero)) {
            EObjectsCache.push_back(AIBaseClient(hero.Handle()));
        }
    }

    EObjectsReady = !EObjectsCache.empty();
}

static bool HasEObjectNear(const Vector3& position, float range, bool includeHeroes = true) {
    if (position.IsZero()) {
        return false;
    }

    const float rangeSqr = range * range;
    const auto check = [&](const AIBaseClient& unit) {
        return ValidTarget(unit) &&
               (includeHeroes || !unit.IsHero()) &&
               unit.Position().DistanceSqr2D(position) <= rangeSqr;
    };

    for (const auto& unit : EObjectsCache) {
        if (check(unit)) {
            return true;
        }
    }

    if (!EObjectsReady) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (check(minion)) {
                return true;
            }
        }
        for (const auto& monster : GameObjects::Jungle()) {
            if (check(monster)) {
                return true;
            }
        }
        if (includeHeroes) {
            for (const auto& hero : GameObjects::EnemyHeroes()) {
                if (check(hero)) {
                    return true;
                }
            }
        }
    }

    return false;
}

static AIBaseClient BestCursorDashObject(const Vector3& cursor, float cursorRange) {
    if (!EObjectsReady || cursor.IsZero()) {
        return {};
    }

    const bool allowTower = AllowTurret();
    const float rangeSqr = cursorRange * cursorRange;
    AIBaseClient best;
    float bestCursorDistance = FLT_MAX;

    for (const auto& unit : EObjectsCache) {
        if (!ValidTarget(unit, E.Range) || !CanE(unit)) {
            continue;
        }

        const float cursorDistance = unit.Position().DistanceSqr2D(cursor);
        if (cursorDistance > rangeSqr || cursorDistance >= bestCursorDistance) {
            continue;
        }

        if (!AllowDashTo(PosAfterE(unit), allowTower)) {
            continue;
        }

        best = unit;
        bestCursorDistance = cursorDistance;
    }

    return best;
}

static AIBaseClient BestCachedDashObjectNear(const Vector3& desired,
                                             float desiredRange,
                                             bool preferFarthest,
                                             bool allowTower,
                                             bool includeHeroes = true) {
    const auto player = Player();
    if (!player.IsValid() || desired.IsZero()) {
        return {};
    }

    if (!EObjectsReady) {
        return BestDashObjectNear(
            desired,
            E.Range,
            desiredRange,
            preferFarthest,
            allowTower,
            includeHeroes);
    }

    AIBaseClient best;

    auto consider = [&](const AIBaseClient& unit) {
        if (!ValidTarget(unit, E.Range) || !CanE(unit)) {
            return;
        }

        if (!includeHeroes && unit.IsHero()) {
            return;
        }

        const Vector3 after = PosAfterE(unit);
        if (!AllowDashTo(after, allowTower)) {
            return;
        }

        const float afterDistance = after.Distance2D(desired);
        if (afterDistance > desiredRange && afterDistance >= player.Position().Distance2D(desired)) {
            return;
        }

        if (!best.IsValid()) {
            best = unit;
            return;
        }

        const bool unitIsHero = unit.IsHero();
        const bool bestIsHero = best.IsHero();
        if (unitIsHero != bestIsHero) {
            if (!unitIsHero) {
                best = unit;
            }
            return;
        }

        const float unitDist = std::max(50.0f, afterDistance);
        const float bestDist = std::max(50.0f, PosAfterE(best).Distance2D(desired));
        if (std::abs(unitDist - bestDist) > 0.001f) {
            if (unitDist < bestDist) {
                best = unit;
            }
            return;
        }

        const float unitPlayerDist = unit.DistanceToPlayer();
        const float bestPlayerDist = best.DistanceToPlayer();
        if ((preferFarthest && unitPlayerDist > bestPlayerDist) ||
            (!preferFarthest && unitPlayerDist < bestPlayerDist)) {
            best = unit;
        }
    };

    for (const auto& unit : EObjectsCache) {
        consider(unit);
    }

    return best;
}

static bool CastQOnTarget(const AIBaseClient& target,
                          bool allowQ,
                          bool allowQ3,
                          HitChance minHitChance = HitChance::High) {
    Spell& spell = CurrentQ();
    const bool wind = HaveQ3();
    if (!spell.IsReady() || (wind && !allowQ3) || (!wind && !allowQ)) {
        return false;
    }

    spell.Range = CurrentQRange();

    // 1. Try to Q the passed target if valid and in range
    if (ValidTarget(target, spell.Range)) {
        const auto prediction = spell.GetPrediction(target);
        const Vector3 castPosition = prediction.GetCastPosition();
        if (!castPosition.IsZero() && spell.IsInRange(castPosition) &&
            static_cast<int>(prediction.Hitchance) >= static_cast<int>(minHitChance)) {
            if (CastPosition(spell, castPosition)) {
                LastQ = SDK::Variables::TickCount();
                return true;
            }
        }
    }

    // 2. Fallback: try to Q the best target within our actual Q range
    const auto smartTarget = GetPhysicalTarget(spell.Range);
    if (ValidHeroTarget(smartTarget, spell.Range) && smartTarget.NetworkId() != target.NetworkId()) {
        const auto prediction = spell.GetPrediction(smartTarget);
        const Vector3 castPosition = prediction.GetCastPosition();
        if (!castPosition.IsZero() && spell.IsInRange(castPosition) &&
            static_cast<int>(prediction.Hitchance) >= static_cast<int>(minHitChance)) {
            if (CastPosition(spell, castPosition)) {
                LastQ = SDK::Variables::TickCount();
                return true;
            }
        }
    }

    return false;
}

static bool CastQDuringDash() {
    const auto player = Player();
    if (!player.IsValid() || !player.IsDashing() || !Q.IsReady()) {
        return false;
    }

    const int now = SDK::Variables::TickCount();
    if (now - LastQ < 200) {
        return false;
    }

    const float radius = static_cast<float>(Slider(RangeMenu, "EQRange", 240));

    // Check enemy heroes in EQ radius
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) &&
            enemy.Position().Distance2D(player.Position()) <= radius + enemy.BoundingRadius() + 30.0f) {
            if (Q.Cast(player.Position())) {
                LastQ = now;
                return true;
            }
        }
    }

    // Check minions in EQ radius when in clear mode
    if (IsClearMode() && Bool(ClearMenu, "QClear")) {
        for (const auto& minion : ClearUnits(radius + 30.0f)) {
            if (Q.Cast(player.Position())) {
                LastQ = now;
                return true;
            }
        }
    }

    return false;
}

static bool CastEQCircleQ() {
    const auto player = Player();
    if (!player.IsValid() || !Q.IsReady()) {
        return false;
    }

    if (Q.Cast(player.Position())) {
        LastQ = SDK::Variables::TickCount();
        return true;
    }
    return false;
}

static AIBaseClient BestEQRDashObject(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !ValidHeroTarget(target) || !E.IsReady()) {
        return {};
    }

    const bool allowTower = AllowTurret();
    const Vector3 desired = target.Position();
    const float maxAfterDistance =
        static_cast<float>(Slider(RangeMenu, "EQRange", 240)) + target.BoundingRadius();

    AIBaseClient best;
    float bestAfterDistance = FLT_MAX;
    float bestPlayerDistance = FLT_MAX;

    auto consider = [&](const AIBaseClient& unit) {
        if (!ValidTarget(unit, E.Range) || !CanE(unit)) {
            return;
        }

        const Vector3 after = PosAfterE(unit);
        if (!AllowDashTo(after, allowTower)) {
            return;
        }

        const float afterDistance = after.Distance2D(desired);
        if (afterDistance > maxAfterDistance) {
            return;
        }

        const float playerDistance = unit.DistanceToPlayer();
        if (!best.IsValid() ||
            afterDistance < bestAfterDistance - 0.001f ||
            (std::abs(afterDistance - bestAfterDistance) <= 0.001f &&
             playerDistance < bestPlayerDistance)) {
            best = unit;
            bestAfterDistance = afterDistance;
            bestPlayerDistance = playerDistance;
        }
    };

    if (EObjectsReady) {
        for (const auto& unit : EObjectsCache) {
            consider(unit);
        }
    } else {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            consider(minion);
        }
        for (const auto& monster : GameObjects::Jungle()) {
            consider(monster);
        }
        for (const auto& hero : GameObjects::EnemyHeroes()) {
            consider(hero);
        }
    }

    return best;
}

static bool TryEQR(const AIHeroClient& target, const Vector3& castPosition) {
    const auto player = Player();
    if (!player.IsValid() ||
        !Bool(RMenu, "REQR", true) ||
        !ValidHeroTarget(target, R.Range) ||
        !Q.IsReady()) {
        return false;
    }

    // Check turret safety for R position
    if (!AllowTurret() && UnderTower(castPosition)) {
        return false;
    }

    if (player.IsDashing()) {
        if (CastEQCircleQ()) {
            return R.Cast(castPosition);
        }
        return false;
    }

    if (!E.IsReady()) {
        return false;
    }

    const auto dash = BestEQRDashObject(target);
    if (!dash.IsValid() || !CastE(dash)) {
        return false;
    }

    if (CastEQCircleQ()) {
        return R.Cast(castPosition);
    }

    return false;
}

static bool TryR() {
    if (!R.IsReady() || !Bool(RMenu, "RCombo") || !Bool(RMenu, "REQR", true)) {
        return false;
    }

    R.Range = static_cast<float>(Slider(RangeMenu, "RRange", 1400));
    const int maxHealth = Slider(RMenu, "RHealth", 100);

    for (const auto& target : EnemyHeroesByHealth(R.Range)) {
        if (!IsAirborne(target) || target.HealthPercent() > static_cast<float>(maxHealth)) {
            continue;
        }

        const Vector3 castPosition = target.Position();
        if (!AllowTurret() && UnderTower(castPosition)) {
            continue;
        }

        // ONLY execute EQR (E+Q before R). Normal R is manual only by user decision.
        if (TryEQR(target, castPosition)) {
            return true;
        }
    }
    return false;
}

static bool TryEQToTarget(const AIHeroClient& target) {
    if (!Bool(EQMenu, "UseEQ") || !E.IsReady() || !Q.IsReady(300) ||
        !ValidHeroTarget(target, static_cast<float>(Slider(RangeMenu, "EGapRange", 925)))) {
        return false;
    }

    const float eqRange = static_cast<float>(Slider(RangeMenu, "EQRange", 240));
    const float bonus = static_cast<float>(Slider(EQMenu, "EBonus", 65));
    const bool preferFarthest = List(EMenu, "EMode", 0) == 1;
    const Vector3 desired = PredictedPosition(CurrentQ(), target);

    AIBaseClient dash = BestCachedDashObjectNear(
        desired,
        eqRange + bonus + target.BoundingRadius(),
        preferFarthest,
        AllowTurret());

    if (!dash.IsValid()) {
        return false;
    }

    // Cast E on dash target. Q will be dynamically triggered by CastQDuringDash() / OnDash
    // when Yasuo comes within EQ radius of the target!
    if (CastE(dash)) {
        return true;
    }
    return false;
}

static bool TryEGapClose(const AIHeroClient& target) {
    if (Orbwalker::IsAutoAttacking() ||
        (Orbwalker::AttackCooldownRemaining() <= 200 && Orbwalker::GetTarget().IsValid())) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid() || !E.IsReady() || !Bool(EMenu, "UseE")) {
        return false;
    }

    // Do NOT dash E to gapclose if already in auto attack range of the target!
    if (AutoAttack::InAutoAttackRange(target)) {
        return false;
    }

    const float gapRange = static_cast<float>(Slider(RangeMenu, "EGapRange", 925));
    if (!ValidHeroTarget(target, gapRange)) {
        return false;
    }

    const bool allowTower = AllowTurret();
    const bool preferFarthest = List(EMenu, "EMode", 0) == 1;

    const Vector3 desired = PredictedPosition(CurrentQ(), target);
    if (desired.IsZero()) {
        return false;
    }

    const float currentDistance = player.Position().Distance2D(desired);

    // Direct E on target if target is in E range and not in AA range
    if (ValidTarget(target, E.Range) && CanE(target) && AllowDashTo(target, allowTower)) {
        return CastE(target);
    }

    // Minion / Monster gapclose E
    AIBaseClient dash = BestCachedDashObjectNear(
        desired,
        Q.Range,
        preferFarthest,
        allowTower
    );

    if (!dash.IsValid()) {
        return false;
    }

    const Vector3 after = PosAfterE(dash);
    if (!AllowDashTo(after, allowTower)) {
        return false;
    }

    const float afterDistance = after.Distance2D(desired);
    constexpr float minImproveDistance = 120.0f;

    if (afterDistance + minImproveDistance >= currentDistance) {
        return false;
    }

    return CastE(dash);
}

static Vector3 BestEQFlashPosition() {
    AIHeroClient bestTarget;
    Vector3 bestPosition;
    int bestCount = 0;

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, 875.0f)) {
            continue;
        }

        const Vector3 position = enemy.Position();
        const int count = CountEnemyHeroesNear(position, 175.0f);
        if (count > bestCount) {
            bestTarget = enemy;
            bestPosition = position;
            bestCount = count;
        }
    }

    if (!bestPosition.IsZero()) {
        return bestPosition;
    }

    const auto target = GetPhysicalTarget(875.0f);
    return ValidHeroTarget(target, 875.0f) ? target.Position() : Vector3();
}

static bool TryEQFlash(bool requireKey) {
    if ((requireKey && !Key(KeyMenu, "EQFlash")) ||
        !HaveQ3() ||
        !Q.IsReady() ||
        !IsFlashReady()) {
        return false;
    }

    const int now = SDK::Variables::TickCount();
    if (now - LastEQFlash < 800) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }

    const Vector3 flashPosition = BestEQFlashPosition();
    if (flashPosition.IsZero()) {
        return false;
    }

    if (!player.IsDashing()) {
        if (!E.IsReady()) {
            return false;
        }

        AIBaseClient dash = BestCachedDashObjectNear(
            flashPosition,
            Flash.Range + 170.0f,
            false,
            AllowTurret());
        return dash.IsValid() && CastE(dash);
    }

    if (flashPosition.Distance2D(player.Position()) > Flash.Range + 170.0f) {
        return false;
    }

    Q.Cast(player.Position());
    if (Flash.Cast(flashPosition)) {
        LastEQFlash = now;
        LastQ = now;
        return true;
    }
    return false;
}

static bool TryWBlock(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() ||
        !W.IsReady() ||
        !Bool(WMenu, "WBlock") ||
        (Bool(WMenu, "WComboOnly", false) && !IsComboMode()) ||
        player.HealthPercent() > static_cast<float>(Slider(WMenu, "WHealth", 100))) {
        return false;
    }

    if (!args.Sender.IsValid() ||
        args.Sender.Team == static_cast<std::uint32_t>(player.Team()) ||
        args.Sender.Type != ::Core::Objects::ObjectType::AIHeroClient ||
        args.Slot < static_cast<int>(SpellSlot::Q) ||
        args.Slot > static_cast<int>(SpellSlot::R)) {
        return false;
    }

    const bool targetsPlayer =
        (args.Target.IsValid() && args.Target.NetworkId == static_cast<std::uint32_t>(player.NetworkId())) ||
        (args.TargetNetworkId != 0 &&
         args.TargetNetworkId != 0xFFFFFFFFu &&
         args.TargetNetworkId == static_cast<std::uint32_t>(player.NetworkId()));

    if (!targetsPlayer) {
        return false;
    }

    const AIBaseClient sender(args.Sender.Ptr, args.Sender.Type);
    const Vector3 castPosition = sender.IsValid()
        ? player.Position().Extend(sender.Position(), 100.0f)
        : player.Position();
    return W.Cast(castPosition);
}

static void ApplyDefaultSettings() {
    SetBool(QMenu, "UseQ", true);
    SetBool(QMenu, "UseQ3", true);
    SetBool(QMenu, "HarassQ", true);
    SetBool(WMenu, "WBlock", true);
    SetBool(WMenu, "WComboOnly", false);
    SetSlider(WMenu, "WHealth", 100);
    SetBool(EMenu, "UseE", true);
    SetBool(EQMenu, "UseEQ", true);
    SetBool(RMenu, "RCombo", true);
    SetBool(RMenu, "REQR", true);
    SetSlider(RMenu, "RHealth", 100);
    SetSlider(RMenu, "RDelay", 0);
    SetBool(ClearMenu, "QClear", true);
    SetBool(ClearMenu, "Q3Clear", true);
    SetSlider(ClearMenu, "Q3Minions", 3);
    SetBool(ClearMenu, "EClear", false);
    SetBool(DrawMenu, "DrawQ", true);
    SetBool(DrawMenu, "DrawE", true);
    SetBool(DrawMenu, "DrawR", false);
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.yasuo", "Kuro - Yasuo", true);

    RangeMenu = MenuRoot->AddSubMenu(new Menu("YasuoRanges", "Range Settings"));
    RangeMenu->Add(new MenuSlider("QRange", "Q Range", 475, 350, 500));
    RangeMenu->Add(new MenuSlider("Q3Range", "Q3 Range", 1100, 850, 1200));
    RangeMenu->Add(new MenuSlider("ERange", "E Range", 475, 350, 500));
    RangeMenu->Add(new MenuSlider("EQRange", "EQ Radius", 240, 150, 300));
    RangeMenu->Add(new MenuSlider("EGapRange", "E Gap Range", 925, 475, 1200));
    RangeMenu->Add(new MenuSlider("RRange", "R Range", 1400, 900, 1500));

    QMenu = MenuRoot->AddSubMenu(new Menu("YasuoQ", "Q Settings"));
    QMenu->Add(new MenuBool("UseQ", "Q Combo"));
    QMenu->Add(new MenuBool("UseQ3", "Q3 Combo"));
    QMenu->Add(new MenuBool("HarassQ", "Q Harass"));

    WMenu = MenuRoot->AddSubMenu(new Menu("YasuoW", "W Settings"));
    WMenu->Add(new MenuBool("WBlock", "W Block Targeted Spells"));
    WMenu->Add(new MenuBool("WComboOnly", "W Combo Only", false));
    WMenu->Add(new MenuSlider("WHealth", "W if Health % <=", 100, 1, 100));

    EMenu = MenuRoot->AddSubMenu(new Menu("YasuoE", "E Settings"));
    EMenu->Add(new MenuBool("UseE", "E Combo"));
    EMenu->Add(new MenuList("EMode", "Dash Object Mode", { "Closest Gap", "Farthest Gap" }, 0));

    EQMenu = MenuRoot->AddSubMenu(new Menu("YasuoEQ", "EQ Settings"));
    EQMenu->Add(new MenuBool("UseEQ", "EQ Combo"));
    EQMenu->Add(new MenuSlider("EBonus", "EQ Bonus Range", 65, 0, 250));

    RMenu = MenuRoot->AddSubMenu(new Menu("YasuoR", "R Settings"));
    RMenu->Add(new MenuBool("RCombo", "R Combo"));
    RMenu->Add(new MenuBool("REQR", "Use EQR Before R"));
    RMenu->Add(new MenuSlider("RHealth", "R if Target Health % <=", 100, 1, 100));
    RMenu->Add(new MenuSlider("RDelay", "R Delay (ms)", 150, 0, 600));

    ClearMenu = MenuRoot->AddSubMenu(new Menu("YasuoClear", "Clear Settings"));
    ClearMenu->Add(new MenuBool("QClear", "Q Clear"));
    ClearMenu->Add(new MenuBool("Q3Clear", "Q3 Clear"));
    ClearMenu->Add(new MenuSlider("Q3Minions", "Q3 Minions >=", 3, 1, 6));
    ClearMenu->Add(new MenuBool("EClear", "E Clear", false));

    KeyMenu = MenuRoot->AddSubMenu(new Menu("YasuoKeys", "Keys Settings"));
    KeyMenu->Add(new MenuKeyBind("Flee", "Flee E", SDK::Keys::E, KeyBindType::Press));
    KeyMenu->Add(new MenuKeyBind("AutoQ", "Auto Q Harass", SDK::Keys::A, KeyBindType::Toggle));
    KeyMenu->Add(new MenuKeyBind("AutoStack", "Auto Q Stack", SDK::Keys::N, KeyBindType::Toggle));
    KeyMenu->Add(new MenuKeyBind("AllowTurret", "Allow Turret Dash", SDK::Keys::T, KeyBindType::Toggle))->Permashow();
    KeyMenu->Add(new MenuKeyBind("EQFlash", "EQ Flash", SDK::Keys::G, KeyBindType::Press))->Permashow();
    KeyMenu->Add(new MenuKeyBind("ComboEQFlash", "Combo EQ Flash", SDK::Keys::H, KeyBindType::Toggle))->Permashow();

    DrawMenu = MenuRoot->AddSubMenu(new Menu("YasuoDraw", "Draw Settings"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("DrawE", "Draw E"));
    DrawMenu->Add(new MenuBool("DrawR", "Draw R", false));

    MenuRoot->Add(new MenuBool("reset", "Reset Yasuo", false));
    MenuRoot->Attach();
}

static void CheckMenu() {
    if (auto* reset = MenuRoot ? MenuRoot->Get<MenuBool>("reset") : nullptr) {
        if (reset->Value) {
            ApplyDefaultSettings();
            reset->Value = false;
        }
    }

    Q.Range = static_cast<float>(Slider(RangeMenu, "QRange", 475));
    Q3.Range = static_cast<float>(Slider(RangeMenu, "Q3Range", 1100));
    E.Range = static_cast<float>(Slider(RangeMenu, "ERange", 475));
    R.Range = static_cast<float>(Slider(RangeMenu, "RRange", 1400));
}

static void Combo() {
    if (!IsComboMode()) {
        return;
    }

    if (Key(KeyMenu, "ComboEQFlash") && TryEQFlash(false)) {
        return;
    }

    if (CastQDuringDash()) {
        return;
    }
    if (TryR()) {
        return;
    }

    const float range = std::max(R.Range, static_cast<float>(Slider(RangeMenu, "EGapRange", 925)));
    const auto target = GetPhysicalTarget(range);
    if (!ValidHeroTarget(target, range)) {
        return;
    }

    if (!HaveQ3() &&
        CastQOnTarget(target, Bool(QMenu, "UseQ"), Bool(QMenu, "UseQ3"), HitChance::Medium)) {
        return;
    }

    if (TryEQToTarget(target)) {
        return;
    }
    if (TryEGapClose(target)) {
        return;
    }

    (void)CastQOnTarget(target, Bool(QMenu, "UseQ"), Bool(QMenu, "UseQ3"));
}

static void Harass() {
    if (!Q.IsReady() || (!IsHarassMode() && !Key(KeyMenu, "AutoQ"))) {
        return;
    }

    const auto target = GetPhysicalTarget(CurrentQRange());
    if (ValidHeroTarget(target, CurrentQRange())) {
        (void)CastQOnTarget(target, Bool(QMenu, "HarassQ"), Bool(QMenu, "UseQ3"));
    }
}

static void Clear() {
    if (!IsClearMode() && !IsLastHitMode()) {
        return;
    }

    if (Q.IsReady() && Bool(ClearMenu, "QClear")) {
        if (HaveQ3() && Bool(ClearMenu, "Q3Clear") && IsClearMode()) {
            const auto units = ClearUnits(Q3.Range);
            const auto farm = Q3.GetLineFarmLocation(units, Q3.Width);
            if (farm.MinionsHit >= Slider(ClearMenu, "Q3Minions", 3)) {
                if (Q3.Cast(Vector3::From2D(farm.Position))) {
                    LastQ = SDK::Variables::TickCount();
                    return;
                }
            }
        }

        AIBaseClient lastHit;
        float bestHealth = FLT_MAX;
        for (const auto& unit : ClearUnits(Q.Range)) {
            if (unit.Health() <= Q.GetDamage(unit) && unit.Health() < bestHealth) {
                lastHit = unit;
                bestHealth = unit.Health();
            }
        }

        if (lastHit.IsValid() && Q.Cast(lastHit.Position())) {
            LastQ = SDK::Variables::TickCount();
            return;
        }

        if (IsClearMode() || Key(KeyMenu, "AutoStack")) {
            for (const auto& unit : ClearUnits(Q.Range)) {
                if (Q.Cast(unit.Position())) {
                    LastQ = SDK::Variables::TickCount();
                    return;
                }
            }
        }
    }

    if (IsClearMode() && Bool(ClearMenu, "EClear") && E.IsReady()) {
        for (const auto& unit : ClearUnits(E.Range)) {
            if (CanE(unit) && AllowDashTo(unit, AllowTurret()) && CastE(unit)) {
                return;
            }
        }
    }
}

static void Flee() {
    if (!Key(KeyMenu, "Flee")) {
        return;
    }

    const auto player = Player();
    const Vector3 cursor = Game::CursorPos();
    if (!player.IsValid() || cursor.IsZero()) {
        return;
    }

    Orbwalker::Move(cursor);
    if (!E.IsReady()) {
        return;
    }

    const auto cursorDash = BestCursorDashObject(cursor, 160.0f);
    if (cursorDash.IsValid()) {
        (void)CastE(cursorDash);
        return;
    }

    AIBaseClient best;
    float bestDistance = player.Position().Distance2D(cursor);
    const auto& units = EObjectsReady ? EObjectsCache : GameObjects::Enemy();
    for (const auto& unit : units) {
        if (!ValidTarget(unit, E.Range) || !CanE(unit)) {
            continue;
        }

        const Vector3 after = PosAfterE(unit);
        if (!AllowDashTo(after, AllowTurret())) {
            continue;
        }

        const float distance = after.Distance2D(cursor);
        if (distance < bestDistance) {
            best = unit;
            bestDistance = distance;
        }
    }

    if (best.IsValid()) {
        (void)CastE(best);
    }
}

static void AutoStack() {
    if (!Key(KeyMenu, "AutoStack") || HaveQ3() || !Q.IsReady() || IsComboMode()) {
        return;
    }

    for (const auto& unit : ClearUnits(Q.Range)) {
        if (Q.Cast(unit.Position())) {
            LastQ = SDK::Variables::TickCount();
            return;
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() || Game::IsChatOpen()) {
        return;
    }

    CheckMenu();
    UpdateEObjectsCache();
    if (Key(KeyMenu, "EQFlash")) {
        (void)TryEQFlash(true);
    }
    Flee();
    Combo();
    Harass();
    Clear();
    AutoStack();
}

static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Events::IsLocalPlayer(args.Sender)) {
        const int slot = args.Slot;
        const int now = SDK::Variables::TickCount();
        if (slot == static_cast<int>(SpellSlot::Q)) {
            LastQ = now;
        } else if (slot == static_cast<int>(SpellSlot::E)) {
            LastE = now;
        }
        return;
    }

    (void)TryWBlock(args);
}

static bool ShouldQOnDashEnd(const Vector3& endPosition) {
    if (endPosition.IsZero() || !Q.IsReady()) {
        return false;
    }

    const float eqRange = static_cast<float>(Slider(RangeMenu, "EQRange", 240));
    if (IsComboMode() || IsHarassMode()) {
        if (CountEnemyHeroesNear(endPosition, eqRange + 35.0f) > 0) {
            return true;
        }

        if ((HaveQ1() || HaveQ2()) &&
            Key(KeyMenu, "AutoStack") &&
            HasEObjectNear(endPosition, eqRange, false)) {
            return true;
        }
    }

    return IsClearMode() &&
           Bool(ClearMenu, "QClear") &&
           HasEObjectNear(endPosition, eqRange, false);
}

static void OnDash(const Events::Dash::DashArgs& args) {
    const auto player = Player();
    if (!player.IsValid() ||
        !args.IsDash ||
        args.NetworkId != static_cast<std::uint32_t>(player.NetworkId())) {
        return;
    }

    const int now = SDK::Variables::TickCount();
    if (now - LastDashQ < 250 || !ShouldQOnDashEnd(args.EndPos)) {
        return;
    }

    LastDashQ = now;
    SDK::Utils::DelayAction::Add(10, []() {
        const auto player = Player();
        if (player.IsValid() && player.IsDashing()) {
            (void)CastEQCircleQ();
        }
    });
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    if (Bool(DrawMenu, "DrawQ")) {
        Drawing::DrawCircle(
            player.Position(),
            HaveQ3() ? Q3.Range : Q.Range,
            HaveQ3() ? 0xFF00D7FFu : 0xFFFFFFFFu,
            1.5f,
            64);
    }

    if (Bool(DrawMenu, "DrawE")) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFF77DD77u, 1.5f, 64);
        Drawing::DrawCircle(
            player.Position(),
            static_cast<float>(Slider(RangeMenu, "EQRange", 240)),
            0xAAFFCC00u,
            1.0f,
            48);
    }

    if (Bool(DrawMenu, "DrawR", false)) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFF5555u, 1.5f, 64);
    }
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }

    if (auto* item = KeyMenu ? KeyMenu->Get<MenuKeyBind>("AllowTurret") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = KeyMenu ? KeyMenu->Get<MenuKeyBind>("EQFlash") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = KeyMenu ? KeyMenu->Get<MenuKeyBind>("ComboEQFlash") : nullptr) {
        item->RemovePermashow();
    }

    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    RangeMenu = nullptr;
    QMenu = nullptr;
    WMenu = nullptr;
    EMenu = nullptr;
    EQMenu = nullptr;
    RMenu = nullptr;
    ClearMenu = nullptr;
    KeyMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 475.0f);
    Q.SetSkillshot(0.25f, 55.0f, 20000.0f, false, SkillshotType::SkillshotLine);
    Q3 = Spell(SpellSlot::Q, 1100.0f);
    Q3.SetSkillshot(0.275f, 80.0f, 1200.0f, false, SkillshotType::SkillshotLine);
    W = Spell(SpellSlot::W, 100.0f);
    W.SetSkillshot(0.0f, 100.0f, FLT_MAX, false, SkillshotType::SkillshotLine);
    E = Spell(SpellSlot::E, 475.0f);
    E.SetTargetted(0.0f, 1000.0f);
    R = Spell(SpellSlot::R, 1400.0f);

    FlashSlot = player.GetSpellSlot("summonerflash");
    if (FlashSlot != SpellSlot::Unknown) {
        Flash = Spell(FlashSlot, 425.0f);
        Flash.SetSkillshot(0.0f, 175.0f, FLT_MAX, false, SkillshotType::SkillshotCircle);
    }

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnDash += &OnDash;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Yasuo loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnDash -= &OnDash;
    Drawing::OnDraw -= &OnDraw;
    Orbwalker::AttackEnabled(true);
    Orbwalker::MoveEnabled(true);
    Orbwalker::SetOrbwalkerPosition({});
    EObjectsCache.clear();
    EObjectsReady = false;
    RemoveMenu();
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Yasuo
