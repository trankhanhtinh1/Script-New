#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::Viktor {

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* RBlacklistMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneMenu = nullptr;
inline Menu* JungleMenu = nullptr;
inline Menu* KillstealMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 652.0f };
inline Spell W{ SpellSlot::W, 800.0f };
inline Spell E{ SpellSlot::E, 525.0f };
inline Spell R{ SpellSlot::R, 700.0f };

inline bool Loaded = false;
inline bool QPendingAttack = false;
inline uint32_t PendingQTargetId = 0;
inline int PendingQSince = 0;
inline int LastUpdateTick = 0;
inline int LastClearTick = 0;
inline int LastRFollowTick = 0;
inline int LastAutoWTick = 0;

static constexpr float kERange = 525.0f;
static constexpr float kEMaxRange = 1200.0f;
static constexpr float kELength = 700.0f;
static constexpr float kESpeed = 1050.0f;

struct LaserFarmLocation {
    Vector2 Start = {};
    Vector2 End = {};
    int MinionsHit = 0;
};

static bool ShouldRunNow(int& lastTick, int intervalMs) {
    const int now = SDK::Variables::TickCount();
    if (lastTick > 0 && now - lastTick < intervalMs) {
        return false;
    }
    lastTick = now;
    return true;
}

static bool HasName(const Events::ProcessSpellEventArgs& args, const char* name) {
    return EqualsIgnoreCase(args.SpellName, name) ||
           EqualsIgnoreCase(args.ScriptName, name) ||
           EqualsIgnoreCase(args.SpellSlotName, name) ||
           EqualsIgnoreCase(args.PayloadSpellName, name);
}

static bool IsRInitialCast() {
    return EqualsIgnoreCase(R.Instance().Name().c_str(), "ViktorChaosStorm");
}

static bool HasActiveChaosStorm() {
    const auto player = Player();
    if (player.IsValid() &&
        (player.HasBuff("ViktorChaosStorm") || player.HasBuff("viktorchaosstorm") ||
         player.HasBuff("ViktorChaosStormGuide") || player.HasBuff("viktorchaosstormguide"))) {
        return true;
    }
    return EqualsIgnoreCase(R.Instance().Name().c_str(), "ViktorChaosStormGuide");
}

static bool HasHardCrowdControl(const AIBaseClient& target) {
    return SDK::HasBuffOfType(target, SDK::BuffType::Stun) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Snare) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Taunt) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Fear) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Charm) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Suppression) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Knockup) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Knockback) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Asleep);
}

static float EffectiveMagicalHealth(const AIBaseClient& target) {
    return target.Health() + target.AllShield() + target.MagicalShield();
}

static std::string RBlacklistKey(const AIHeroClient& enemy) {
    return "BlockR." + std::to_string(enemy.NetworkId());
}

static bool RBlocked(const AIHeroClient& enemy) {
    if (!RBlacklistMenu || !enemy.IsValid()) {
        return false;
    }
    const std::string key = RBlacklistKey(enemy);
    const auto* item = RBlacklistMenu->Get<MenuBool>(key.c_str());
    return item && item->Value;
}

static int CountEnemiesNear(const Vector3& position, float range) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) && enemy.Position().Distance2D(position) <= range) {
            ++count;
        }
    }
    return count;
}

static float DistancePointSegmentSqr(const Vector2& point,
                                     const Vector2& start,
                                     const Vector2& end) {
    const Vector2 segment = end - start;
    const float lengthSqr = segment.LengthSqr();
    if (lengthSqr <= FLT_EPSILON) {
        return point.DistanceSqr(start);
    }
    const float t = std::clamp((point - start).Dot(segment) / lengthSqr, 0.0f, 1.0f);
    return point.DistanceSqr(start + segment * t);
}

static void ResetEGeometry() {
    E.Range = kERange;
    E.From = {};
    E.RangeCheckFrom = {};
    E.Speed = kESpeed;
}

static bool CastE(Vector3 start, Vector3 end) {
    start.y = NavMesh::GetHeightForPosition(start);
    end.y = NavMesh::GetHeightForPosition(end);
    ResetEGeometry();
    return E.Cast(start, end);
}

static Vector3 BuildLaserEnd(const Vector3& start,
                             const Vector3& desiredEnd,
                             const Vector3& targetPosition) {
    Vector3 end = desiredEnd;
    if (!end.IsValid() || end.IsZero() || start.Distance2D(end) < 25.0f) {
        end = start.Extend(targetPosition, kELength);
    }
    if (start.Distance2D(end) > kELength) {
        end = start.Extend(end, kELength);
    }
    return end;
}

static Vector3 SelectOuterEStart(const Vector3& idealStart, const AIHeroClient& target) {
    AIBaseClient best;
    float bestHealth = -1.0f;

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (enemy.Address() == target.Address() || !ValidHeroTarget(enemy, kEMaxRange) ||
            enemy.DistanceToPlayer() >= kERange ||
            enemy.Position().Distance2D(idealStart) > 150.0f) {
            continue;
        }
        if (enemy.Health() > bestHealth) {
            best = enemy;
            bestHealth = enemy.Health();
        }
    }

    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidTarget(minion, kEMaxRange) || minion.DistanceToPlayer() >= kERange ||
            minion.Position().Distance2D(idealStart) > 150.0f) {
            continue;
        }
        if (minion.Health() > bestHealth) {
            best = minion;
            bestHealth = minion.Health();
        }
    }
    return best.IsValid() ? best.Position() : idealStart;
}

static AIHeroClient BestSecondaryETarget(const Vector3& start, const AIHeroClient& primary) {
    AIHeroClient result;
    float bestHealth = -1.0f;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (enemy.Address() == primary.Address() || !ValidHeroTarget(enemy, kEMaxRange)) {
            continue;
        }

        E.From = start;
        E.RangeCheckFrom = start;
        E.Range = kELength;
        const auto prediction = E.GetPrediction(enemy);
        if (prediction.Hitchance >= HitChance::High &&
            prediction.GetCastPosition().Distance2D(start) <= kELength * 0.9f &&
            enemy.Health() > bestHealth) {
            result = enemy;
            bestHealth = enemy.Health();
        }
    }
    return result;
}

static bool PredictCastE(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !E.IsReady() || !ValidHeroTarget(target, kEMaxRange)) {
        return false;
    }

    Vector3 start;
    Vector3 end;
    bool canCast = false;
    const bool inNormalRange = target.DistanceToPlayer() < kERange;

    if (inNormalRange) {
        E.Speed = kESpeed * 0.9f;
        E.From = target.Position().Extend(player.Position(), kELength * 0.1f);
        E.RangeCheckFrom = player.Position();
        E.Range = kERange;
        const auto startPrediction = E.GetPrediction(target);
        start = startPrediction.GetCastPosition().Distance2D(player.Position()) < kERange
            ? startPrediction.GetCastPosition()
            : target.Position();

        const auto secondary = BestSecondaryETarget(start, target);
        const AIHeroClient endTarget = secondary.IsValid() ? secondary : target;
        E.From = start;
        E.RangeCheckFrom = start;
        E.Range = kELength;
        E.Speed = kESpeed;
        const auto endPrediction = E.GetPrediction(endTarget);
        canCast = endPrediction.Hitchance >= HitChance::High;
        end = BuildLaserEnd(start, endPrediction.GetCastPosition(), endTarget.Position());
    } else {
        const Vector3 idealStart = player.Position().Extend(target.Position(), kERange);
        start = SelectOuterEStart(idealStart, target);
        E.From = start;
        E.RangeCheckFrom = start;
        E.Range = kELength;
        E.Speed = kESpeed;
        const auto prediction = E.GetPrediction(target);
        canCast = prediction.Hitchance >= HitChance::High;
        end = BuildLaserEnd(start, prediction.GetCastPosition(), target.Position());
    }

    if (start.Distance2D(player.Position()) > kERange + 15.0f ||
        start.Distance2D(end) > kELength + target.BoundingRadius()) {
        canCast = false;
    }
    ResetEGeometry();
    return canCast && CastE(start, end);
}

static std::vector<AIBaseClient> FarmUnits(bool jungle) {
    std::vector<AIBaseClient> result;
    if (jungle) {
        for (const auto& monster : GameObjects::Jungle()) {
            if (ValidTarget(monster, kEMaxRange)) {
                result.emplace_back(monster.Handle());
            }
        }
    } else {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (ValidTarget(minion, kEMaxRange)) {
                result.emplace_back(minion.Handle());
            }
        }
    }
    return result;
}

static LaserFarmLocation GetBestLaserFarmLocation(bool jungle) {
    const auto player = Player();
    LaserFarmLocation best;
    const auto units = FarmUnits(jungle);
    if (!player.IsValid() || units.empty()) {
        return best;
    }

    std::vector<Vector2> unitPositions;
    std::vector<Vector2> candidatePositions;
    unitPositions.reserve(units.size());
    candidatePositions.reserve(units.size() * units.size());
    for (const auto& unit : units) {
        unitPositions.push_back(unit.Position().To2D());
        candidatePositions.push_back(unit.Position().To2D());
    }
    const int count = static_cast<int>(unitPositions.size());
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            candidatePositions.push_back((unitPositions[i] + unitPositions[j]) * 0.5f);
        }
    }

    for (const auto& startUnit : units) {
        if (startUnit.DistanceToPlayer() > kERange) {
            continue;
        }
        const Vector2 start = startUnit.Position().To2D();
        for (const auto& candidate : candidatePositions) {
            const Vector2 direction = candidate - start;
            if (direction.LengthSqr() < 25.0f * 25.0f ||
                direction.LengthSqr() > kELength * kELength) {
                continue;
            }
            const Vector2 end = start + direction.Normalized() * kELength;
            int hitCount = 0;
            for (const auto& unitPosition : unitPositions) {
                if (DistancePointSegmentSqr(unitPosition, start, end) <= 140.0f * 140.0f) {
                    ++hitCount;
                }
            }
            if (hitCount > best.MinionsHit) {
                best.Start = start;
                best.End = end;
                best.MinionsHit = hitCount;
            }
        }
    }
    return best;
}

static bool PredictCastMinionE(bool jungle) {
    const auto farm = GetBestLaserFarmLocation(jungle);
    const int required = jungle ? 1 : Slider(LaneMenu, "EHits", 3);
    if (farm.MinionsHit < required) {
        return false;
    }
    return CastE(Vector3::From2D(farm.Start), Vector3::From2D(farm.End));
}

static float RDamage(const AIBaseClient& target) {
    if (!target.IsValid() || R.Level() <= 0) {
        return 0.0f;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return 0.0f;
    }
    const int level = std::clamp(R.Level(), 1, 3);
    const int extraTicks = Slider(ComboMenu, "RTicks", 3);
    const float initialDamage = 100.0f + 75.0f * static_cast<float>(level - 1) + 0.5f * player.AP();
    const float tickDamage = 65.0f + 40.0f * static_cast<float>(level - 1);
    return player.CalculateMagicDamage(
        target,
        initialDamage + tickDamage * static_cast<float>(extraTicks));
}

static bool IsPendingQKill(const AIHeroClient& target) {
    return QPendingAttack && target.IsValid() && PendingQTargetId == target.NetworkId();
}

static bool CanCastR(const AIHeroClient& target) {
    if (!Bool(ComboMenu, "ConserveR", true)) {
        return true;
    }
    if (!ValidHeroTarget(target, R.Range)) {
        return false;
    }
    if (Q.IsReady() && Bool(ComboMenu, "UseQ", true) &&
        ValidHeroTarget(target, Q.Range) &&
        Q.GetDamage(target) >= EffectiveMagicalHealth(target)) {
        return false;
    }
    if (IsPendingQKill(target)) {
        return false;
    }
    if (E.IsReady() && Bool(ComboMenu, "UseE", true) &&
        ValidHeroTarget(target, kEMaxRange) &&
        E.GetDamage(target) >= EffectiveMagicalHealth(target)) {
        return false;
    }
    if (SDK::HealthPrediction::GetPrediction(target, 250) <= 0.0f) {
        return false;
    }
    return !(target.HealthPercent() <= 10.0f && target.CountAllyHeroesInRange(400.0f) >= 2);
}

static bool CastW(const AIHeroClient& target) {
    if (!W.IsReady() || !ValidHeroTarget(target, W.Range)) {
        return false;
    }
    const int mode = List(ComboMenu, "WMode", 1);
    if (mode == 0) {
        return W.CastIfHitchanceMinimum(target, HitChance::Medium) == CastStates::SuccessfullyCasted;
    }
    if (mode == 1 && target.GetPathLength() < 2 &&
        (SDK::HasBuffOfType(target, SDK::BuffType::Slow) || HasHardCrowdControl(target))) {
        return W.CastIfHitchanceMinimum(target, HitChance::Medium) == CastStates::SuccessfullyCasted;
    }
    return false;
}

static bool LogicR() {
    if (!R.IsReady() || !IsRInitialCast() || List(ComboMenu, "RMode", 1) == 2) {
        return false;
    }

    const int minimumEnemies = Slider(ComboMenu, "RHits", 3);
    for (const auto& enemy : EnemyHeroes(R.Range)) {
        if (CountEnemiesNear(enemy.Position(), 300.0f) < minimumEnemies) {
            continue;
        }
        if (List(ComboMenu, "WMode", 1) == 2 && W.IsReady() && ValidHeroTarget(enemy, W.Range)) {
            const auto prediction = W.GetPrediction(enemy);
            if (prediction.Hitchance >= HitChance::Medium && W.Cast(prediction.GetCastPosition())) {
                return true;
            }
        }
        const auto prediction = R.GetPrediction(enemy, true);
        if (prediction.Hitchance >= HitChance::High &&
            prediction.AoeTargetsHitCount >= minimumEnemies && R.Cast(prediction.GetCastPosition())) {
            return true;
        }
    }

    for (const auto& enemy : EnemyHeroesByHealth(R.Range)) {
        if (RBlocked(enemy)) {
            continue;
        }
        const auto prediction = R.GetPrediction(enemy);
        if (prediction.Hitchance < HitChance::High) {
            continue;
        }
        if (List(ComboMenu, "RMode", 1) == 0 &&
            enemy.HealthPercent() <= Slider(ComboMenu, "RHealth", 40)) {
            return R.Cast(prediction.GetCastPosition());
        }
        if (List(ComboMenu, "RMode", 1) == 1 &&
            RDamage(enemy) >= EffectiveMagicalHealth(enemy) && CanCastR(enemy)) {
            return R.Cast(prediction.GetCastPosition());
        }
    }
    return false;
}

static void Combo() {
    if (!IsComboMode() || Orbwalker::IsWindingUp()) {
        return;
    }

    const int wMode = List(ComboMenu, "WMode", 1);
    if (wMode < 2 && W.IsReady()) {
        const auto target = GetMagicalTarget(W.Range);
        if (target.IsValid() && CastW(target)) {
            return;
        }
    }
    if (Bool(ComboMenu, "UseE", true) && E.IsReady()) {
        const auto target = GetMagicalTarget(kEMaxRange);
        if (target.IsValid() && PredictCastE(target)) {
            return;
        }
    }
    if (LogicR()) {
        return;
    }
    if (Bool(ComboMenu, "UseQ", true) && Q.IsReady()) {
        const auto target = GetMagicalTarget(Q.Range + 75.0f);
        if (target.IsValid() && ValidHeroTarget(target, Q.Range + target.BoundingRadius()) &&
            Q.Cast(target) == CastStates::SuccessfullyCasted) {
            QPendingAttack = true;
            PendingQSince = SDK::Variables::TickCount();
            if (Q.GetDamage(target) >= EffectiveMagicalHealth(target)) {
                PendingQTargetId = target.NetworkId();
            }
        }
    }
}

static void Harass() {
    if (!IsHarassMode() || Player().ManaPercent() <= Slider(HarassMenu, "Mana", 60)) {
        return;
    }
    if (Bool(HarassMenu, "UseQ", true) && Q.IsReady()) {
        const auto target = GetMagicalTarget(Q.Range);
        if (target.IsValid() && Q.Cast(target) == CastStates::SuccessfullyCasted) {
            return;
        }
    }
    if (Bool(HarassMenu, "UseE", true) && E.IsReady()) {
        const auto target = GetMagicalTarget(kEMaxRange);
        if (target.IsValid()) {
            (void)PredictCastE(target);
        }
    }
}

static void LaneClear() {
    if (!IsClearMode() || Player().ManaPercent() <= Slider(LaneMenu, "Mana", 40) ||
        !ShouldRunNow(LastClearTick, 100)) {
        return;
    }

    if (Bool(LaneMenu, "UseQ", true) && Q.IsReady()) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!ValidTarget(minion, Q.Range)) {
                continue;
            }
            const float health = Q.GetHealthPrediction(minion);
            if (health > 0.0f && health < Q.GetDamage(minion) &&
                Q.Cast(minion) == CastStates::SuccessfullyCasted) {
                return;
            }
        }
    }
    if (Bool(LaneMenu, "UseE", true) && E.IsReady()) {
        (void)PredictCastMinionE(false);
    }
}

static void JungleClear() {
    if (!IsClearMode() || Player().ManaPercent() <= Slider(LaneMenu, "Mana", 40)) {
        return;
    }
    if (Bool(JungleMenu, "UseQ", true) && Q.IsReady()) {
        auto monsters = GameObjects::Jungle();
        std::sort(monsters.begin(), monsters.end(), [](const AIMinionClient& left, const AIMinionClient& right) {
            return left.MaxHealth() > right.MaxHealth();
        });
        for (const auto& monster : monsters) {
            if (ValidTarget(monster, Q.Range) &&
                Q.Cast(monster) == CastStates::SuccessfullyCasted) {
                return;
            }
        }
    }
    if (Bool(JungleMenu, "UseE", true) && E.IsReady()) {
        (void)PredictCastMinionE(true);
    }
}

static void Killsteal() {
    for (const auto& enemy : EnemyHeroesByHealth(kEMaxRange)) {
        if (Bool(KillstealMenu, "UseQ", true) && Q.IsReady() &&
            ValidHeroTarget(enemy, Q.Range) &&
            Q.GetDamage(enemy) >= EffectiveMagicalHealth(enemy) &&
            Q.Cast(enemy) == CastStates::SuccessfullyCasted) {
            PendingQTargetId = enemy.NetworkId();
            QPendingAttack = true;
            PendingQSince = SDK::Variables::TickCount();
            return;
        }
        if (Bool(KillstealMenu, "UseE", true) && E.IsReady() &&
            E.GetDamage(enemy) >= EffectiveMagicalHealth(enemy) && PredictCastE(enemy)) {
            return;
        }
    }
}

static void AutoWCC() {
    if (!Bool(MiscMenu, "AutoWCC", true) || !W.IsReady() ||
        !ShouldRunNow(LastAutoWTick, 100)) {
        return;
    }
    for (const auto& enemy : EnemyHeroes(W.Range)) {
        const auto prediction = W.GetPrediction(enemy);
        if ((HasHardCrowdControl(enemy) || prediction.Hitchance == HitChance::Immobile) &&
            W.Cast(prediction.GetCastPosition())) {
            return;
        }
    }
}

static void FollowR() {
    if (!Bool(ComboMenu, "FollowR", true) || !HasActiveChaosStorm()) {
        return;
    }
    const int now = SDK::Variables::TickCount();
    if (LastRFollowTick > 0 && now - LastRFollowTick < 500) {
        return;
    }
    const auto target = GetMagicalTarget(1100.0f);
    if (target.IsValid() && R.Cast(target.Position())) {
        LastRFollowTick = now;
    }
}

static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    const AIBaseClient target(args.Target.Handle());
    const auto player = Player();
    if (!Bool(MiscMenu, "DisableAA", false) || !target.IsHero() || !IsComboMode() ||
        player.Level() < Slider(MiscMenu, "DisableAALevel", 12)) {
        return;
    }
    const bool qUnavailable = !Q.IsReady() || player.Mana() < Q.Instance().ManaCost();
    const bool eUnavailable = !E.IsReady() || player.Mana() < E.Instance().ManaCost();
    args.Process = qUnavailable && eUnavailable && player.HasBuff("viktorpowertransferreturn");
}

static void OnNonKillableMinion(OrbwalkingActionArgs& args) {
    if ((!IsClearMode() && Orbwalker::ActiveMode() != OrbwalkingMode::LastHit) ||
        !Bool(LaneMenu, "UseQ", true) || !Q.IsReady() ||
        Player().ManaPercent() <= Slider(LaneMenu, "Mana", 40)) {
        return;
    }
    const AIBaseClient target(args.Target.Handle());
    if (!ValidTarget(target, Q.Range)) {
        return;
    }
    const float health = Q.GetHealthPrediction(target);
    if (health > 0.0f && health < Q.GetDamage(target)) {
        (void)Q.Cast(target);
    }
}

static void OnDoCast(const Events::ProcessSpellEventArgs& args) {
    if (Events::IsLocalPlayer(args.Sender) && HasName(args, "ViktorPowerTransfer")) {
        QPendingAttack = true;
        PendingQSince = SDK::Variables::TickCount();
    }
}

static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    if (Events::IsLocalPlayer(args.Sender) && HasName(args, "ViktorPowerTransferReturn")) {
        QPendingAttack = false;
        PendingQTargetId = 0;
        PendingQSince = 0;
    }
}

static void OnTeleport(const Events::Teleport::TeleportEventArgs& args) {
    if (!Bool(MiscMenu, "AutoWCC", true) || !W.IsReady() ||
        args.Type != TeleportType::Teleport || args.Status != TeleportStatus::Start ||
        !args.IsTarget) {
        return;
    }
    const AIBaseClient target(args.Object);
    if (ValidTarget(target, W.Range)) {
        (void)W.Cast(target.Position());
    }
}

static void OnInterruptable(const Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    if (args.DangerLevel < DangerLevel::High) {
        return;
    }
    const AIHeroClient target(args.Sender);
    if (!ValidHeroTarget(target, R.Range)) {
        return;
    }
    if (Bool(MiscMenu, "InterruptW", true) && W.IsReady() && ValidHeroTarget(target, W.Range) &&
        SDK::Game::Time() + 1.5f + W.Delay >= args.EndTime) {
        const auto prediction = W.GetPrediction(target);
        if (W.Cast(prediction.GetCastPosition())) {
            return;
        }
    }
    if (Bool(MiscMenu, "InterruptR", true) && R.IsReady() && IsRInitialCast()) {
        (void)R.Cast(target.Position());
    }
}

static void OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "AntiGap", true) || !W.IsReady()) {
        return;
    }
    const auto player = Player();
    const AIHeroClient sender(args.Sender);
    if (player.IsValid() && ValidHeroTarget(sender, W.Range) &&
        args.Start.Distance2D(player.Position()) > args.End.Distance2D(player.Position()) &&
        args.End.Distance2D(player.Position()) <= W.Range) {
        (void)W.Cast(args.End);
    }
}

static void Drawing_OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DrawQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFDEB887u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawW", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFFFF5A5Au, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), kERange, 0xFF9450DCu, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawMaxE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), kEMaxRange, 0xFFFFDC46u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawR", false) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFFA53Cu, 1.5f, 64);
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    if (!ShouldRunNow(LastUpdateTick, 35)) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() || Game::IsChatOpen()) {
        return;
    }

    if (QPendingAttack && PendingQSince > 0 &&
        SDK::Variables::TickCount() - PendingQSince > 4000 &&
        !player.HasBuff("viktorpowertransferreturn")) {
        QPendingAttack = false;
        PendingQTargetId = 0;
        PendingQSince = 0;
    }

    FollowR();
    Killsteal();
    AutoWCC();
    Combo();
    Harass();
    if (IsClearMode()) {
        LaneClear();
        JungleClear();
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.viktor", "Kuro - Viktor", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("UseQ", "Use Q", true));
    ComboMenu->Add(new MenuList("WMode", "Use W", { "Always", "Only Slow or CC", "With R", "Never" }, 1));
    ComboMenu->Add(new MenuBool("UseE", "Use E", true));
    ComboMenu->Add(new MenuList("RMode", "Use R", { "Health below percent", "Killable", "Disable" }, 1));
    ComboMenu->Add(new MenuSlider("RHealth", "R target health percent", 40, 0, 100));
    ComboMenu->Add(new MenuSlider("RTicks", "Extra R damage ticks", 3, 1, 6));
    ComboMenu->Add(new MenuSlider("RHits", "R minimum enemies", 3, 1, 5));
    ComboMenu->Add(new MenuBool("FollowR", "Auto follow with R", true));
    ComboMenu->Add(new MenuBool("ConserveR", "Conserve R when another spell can kill", true));

    RBlacklistMenu = ComboMenu->AddSubMenu(new Menu("RBlacklist", "R Blacklist"));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string key = RBlacklistKey(enemy);
        std::string label = enemy.CharacterName();
        if (label.empty()) {
            label = "Enemy " + std::to_string(enemy.NetworkId());
        }
        RBlacklistMenu->Add(new MenuBool(key.c_str(), label.c_str(), false));
    }

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass Settings"));
    HarassMenu->Add(new MenuBool("UseQ", "Use Q", true));
    HarassMenu->Add(new MenuBool("UseE", "Use E", true));
    HarassMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 60, 0, 100));

    LaneMenu = MenuRoot->AddSubMenu(new Menu("LaneClear", "Lane Clear Settings"));
    LaneMenu->Add(new MenuBool("UseQ", "Use Q", true));
    LaneMenu->Add(new MenuBool("UseE", "Use E", true));
    LaneMenu->Add(new MenuSlider("EHits", "E minimum minions", 3, 1, 6));
    LaneMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 40, 0, 100));

    JungleMenu = MenuRoot->AddSubMenu(new Menu("JungleClear", "Jungle Clear Settings"));
    JungleMenu->Add(new MenuBool("UseQ", "Use Q", true));
    JungleMenu->Add(new MenuBool("UseE", "Use E", true));

    KillstealMenu = MenuRoot->AddSubMenu(new Menu("Killsteal", "Killsteal Settings"));
    KillstealMenu->Add(new MenuBool("UseQ", "Use Q", true));
    KillstealMenu->Add(new MenuBool("UseE", "Use E", true));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc", "Misc Settings"));
    MiscMenu->Add(new MenuBool("DisableAA", "Disable normal attacks in Combo", false));
    MiscMenu->Add(new MenuSlider("DisableAALevel", "Disable attacks from level", 12, 1, 18));
    MiscMenu->Add(new MenuBool("InterruptW", "Use W to interrupt dangerous spells", true));
    MiscMenu->Add(new MenuBool("InterruptR", "Use R to interrupt dangerous spells", true));
    MiscMenu->Add(new MenuBool("AutoWCC", "Auto W crowd-controlled targets", true));
    MiscMenu->Add(new MenuBool("AntiGap", "Use W against gapclosers", true));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw Settings"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q range", false));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W range", false));
    DrawMenu->Add(new MenuBool("DrawE", "Draw E start range", false));
    DrawMenu->Add(new MenuBool("DrawMaxE", "Draw E maximum range", false));
    DrawMenu->Add(new MenuBool("DrawR", "Draw R range", false));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }
    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    RBlacklistMenu = nullptr;
    HarassMenu = nullptr;
    LaneMenu = nullptr;
    JungleMenu = nullptr;
    KillstealMenu = nullptr;
    MiscMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 652.0f);
    W = Spell(SpellSlot::W, 800.0f);
    E = Spell(SpellSlot::E, kERange);
    R = Spell(SpellSlot::R, 700.0f);
    Q.SetTargetted(0.25f, 2000.0f);
    W.SetSkillshot(0.4f, 300.0f, FLT_MAX, false, SkillshotType::SkillshotCircle);
    E.SetSkillshot(0.0f, 90.0f, kESpeed, false, SkillshotType::SkillshotLine);
    R.SetSkillshot(0.6f, 450.0f, FLT_MAX, false, SkillshotType::SkillshotCircle);

    QPendingAttack = false;
    PendingQTargetId = 0;
    PendingQSince = 0;
    LastUpdateTick = 0;
    LastClearTick = 0;
    LastRFollowTick = 0;
    LastAutoWTick = 0;

    BuildMenu();
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Drawing::OnDraw += &Drawing_OnDraw;
    Events::hook.OnDoCast += &OnDoCast;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnTeleport += &OnTeleport;
    Events::hook.OnInterruptableTarget += &OnInterruptable;
    Events::hook.OnGapCloser += &OnGapcloser;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnNonKillableMinion += &OnNonKillableMinion;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Viktor loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Drawing::OnDraw -= &Drawing_OnDraw;
    Events::hook.OnDoCast -= &OnDoCast;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnTeleport -= &OnTeleport;
    Events::hook.OnInterruptableTarget -= &OnInterruptable;
    Events::hook.OnGapCloser -= &OnGapcloser;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnNonKillableMinion -= &OnNonKillableMinion;
    RemoveMenu();
    QPendingAttack = false;
    PendingQTargetId = 0;
    PendingQSince = 0;
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Viktor
