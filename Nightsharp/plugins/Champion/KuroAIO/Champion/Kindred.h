#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::Kindred {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* EBlacklistMenu = nullptr;
inline Menu* LaneMenu = nullptr;
inline Menu* JungleMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* AntiGapMenu = nullptr;
inline Menu* DashMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 340.0f };
inline Spell W{ SpellSlot::W, 900.0f };
inline Spell E{ SpellSlot::E, 575.0f };
inline Spell R{ SpellSlot::R, 500.0f };

inline bool Loaded = false;
inline bool ForcedETarget = false;

static bool SameUnit(const GameObject& left, const GameObject& right) {
    if (!left.IsValid() || !right.IsValid()) {
        return false;
    }
    if (left.Address() == right.Address()) {
        return true;
    }
    return left.NetworkId() != 0 && left.NetworkId() == right.NetworkId();
}

static float EffectivePhysicalHealth(const AIBaseClient& unit) {
    return unit.Health() + unit.PhysicalShield();
}

static AIHeroClient HeroFromInfo(const ::Core::Events::ObjectInfo& info) {
    ::Core::Objects::ObjectHandle handle{};
    handle.address = info.Ptr;
    handle.index = info.Index;
    handle.networkId = info.NetworkId;
    handle.type = info.Type;
    return AIHeroClient(handle);
}

static bool ValidAlly(const AIHeroClient& ally, float range = FLT_MAX) {
    const auto player = Player();
    if (!player.IsValid() || !ally.IsValid() || ally.IsDead() ||
        ally.IsZombie() || ally.IsInvulnerable() || !ally.IsVisible() ||
        !ally.IsTargetable() || ally.Team() != player.Team()) {
        return false;
    }
    return range >= FLT_MAX ||
           player.Position().DistanceSqr2D(ally.Position()) <= range * range;
}

static std::string EBlacklistKey(const AIHeroClient& enemy) {
    return std::string("notcast.") + enemy.CharacterName();
}

static bool EBlacklisted(const AIHeroClient& enemy) {
    if (!EBlacklistMenu) {
        return false;
    }
    const std::string key = EBlacklistKey(enemy);
    const auto* item = EBlacklistMenu->Get<MenuBool>(key.c_str());
    return item && item->Value;
}

static int CountEnemyHeroesNear(const Vector3& position, float range) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) &&
            enemy.Position().DistanceSqr2D(position) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

static bool PointUnderTurret(const Vector3& position, bool enemyTurret) {
    const auto player = Player();
    const float radius = player.IsValid() ? player.BoundingRadius() : 65.0f;
    const auto turrets = enemyTurret
        ? GameObjects::EnemyTurrets()
        : GameObjects::AllyTurrets();
    for (const auto& turret : turrets) {
        if (!turret.IsValid() || turret.IsDead()) {
            continue;
        }
        const float range = turret.AttackRange() + turret.BoundingRadius() + radius;
        if (turret.Position().DistanceSqr2D(position) <= range * range) {
            return true;
        }
    }
    return false;
}

static float AttackRangeFrom(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !ValidHeroTarget(target)) {
        return 0.0f;
    }
    return AutoAttack::GetRealAutoAttackRange(player, target) +
           player.BoundingRadius() + 35.0f;
}

static bool CanAttackFrom(const Vector3& position,
                          const AIHeroClient& preferredTarget = AIHeroClient()) {
    if (!Bool(DashMenu, "AAcheck", true)) {
        return true;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }

    if (ValidHeroTarget(preferredTarget)) {
        const float range = AttackRangeFrom(preferredTarget);
        return range > 0.0f &&
               position.DistanceSqr2D(preferredTarget.Position()) <= range * range;
    }

    const auto orbTarget = Orbwalker::GetTarget();
    if (orbTarget.IsValid()) {
        const AIBaseClient target(orbTarget.Handle());
        if (ValidTarget(target)) {
            const float range = player.AttackRange() + player.BoundingRadius() +
                                target.BoundingRadius() + 50.0f;
            return position.DistanceSqr2D(target.Position()) <= range * range;
        }
    }

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy)) {
            continue;
        }
        const float range = player.AttackRange() + player.BoundingRadius() +
                            enemy.BoundingRadius() + 50.0f;
        if (position.DistanceSqr2D(enemy.Position()) <= range * range) {
            return true;
        }
    }
    return false;
}

static bool InMeleeAttackRange(const Vector3& position) {
    if (!Bool(DashMenu, "notDashAARange", true)) {
        return false;
    }
    const auto player = Player();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy) || !enemy.IsMelee()) {
            continue;
        }
        const float range = enemy.AttackRange() + enemy.BoundingRadius() +
                            player.BoundingRadius() + 10.0f;
        if (position.DistanceSqr2D(enemy.Position()) <= range * range) {
            return true;
        }
    }
    return false;
}

static bool DashPathHasWall(const Vector3& destination) {
    if (!Bool(DashMenu, "WallCheck", true)) {
        return false;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return true;
    }
    for (int i = 1; i <= 5; ++i) {
        const Vector3 point = player.Position().Extend(
            destination,
            Q.Range * static_cast<float>(i) / 5.0f);
        if (NavMesh::IsWall(point)) {
            return true;
        }
    }
    return NavMesh::IsWallBetween(player.Position(), destination, 35.0f);
}

static bool IsGoodDashPosition(const Vector3& destination) {
    const auto player = Player();
    if (!player.IsValid() || destination.IsZero() ||
        player.Position().Distance2D(destination) > Q.Range + 15.0f ||
        DashPathHasWall(destination)) {
        return false;
    }
    if (Key(DashMenu, "TurretCheck", true) && PointUnderTurret(destination, true)) {
        return false;
    }
    if (InMeleeAttackRange(destination)) {
        return false;
    }

    const int checkRange = Slider(DashMenu, "CheckRange", 450);
    const int maximumEnemies = Slider(DashMenu, "EnemyCheck", 3);
    const int enemiesAtEnd = CountEnemyHeroesNear(
        destination,
        static_cast<float>(checkRange));
    if (enemiesAtEnd <= maximumEnemies) {
        return true;
    }
    return enemiesAtEnd <= CountEnemyHeroesNear(player.Position(), 400.0f);
}

static Vector3 CursorDashPosition() {
    const auto player = Player();
    return player.IsValid()
        ? player.Position().Extend(Game::CursorPos(), Q.Range)
        : Vector3();
}

static Vector3 SideDashPosition(const AIHeroClient& preferredTarget) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    AIHeroClient target = preferredTarget;
    if (!ValidHeroTarget(target)) {
        const auto orbTarget = Orbwalker::GetTarget();
        if (!orbTarget.IsValid() || !orbTarget.IsHero()) {
            return {};
        }
        target = AIHeroClient(orbTarget.Handle());
    }
    if (!ValidHeroTarget(target)) {
        return {};
    }

    const Vector3 origin = player.Position();
    const Vector3 targetPosition = target.Position();
    const float dx = targetPosition.x - origin.x;
    const float dz = targetPosition.z - origin.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length <= 0.001f) {
        return {};
    }
    const float perpendicularX = -dz / length;
    const float perpendicularZ = dx / length;
    const Vector3 right{
        origin.x + perpendicularX * Q.Range,
        origin.y,
        origin.z + perpendicularZ * Q.Range
    };
    const Vector3 left{
        origin.x - perpendicularX * Q.Range,
        origin.y,
        origin.z - perpendicularZ * Q.Range
    };
    return right.DistanceSqr2D(Game::CursorPos()) < left.DistanceSqr2D(Game::CursorPos())
        ? right
        : left;
}

static bool HasMeleePressure(const Vector3& position) {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy) || !enemy.IsMelee()) {
            continue;
        }
        const float pressureRange = enemy.AttackRange() + enemy.BoundingRadius() +
                                    player.BoundingRadius() + Q.Range * 0.75f;
        if (position.DistanceSqr2D(enemy.Position()) <=
            pressureRange * pressureRange) {
            return true;
        }
    }
    return false;
}

static float DashSafetyScore(const Vector3& position) {
    const auto player = Player();
    if (!player.IsValid()) {
        return -FLT_MAX;
    }

    float score = PointUnderTurret(position, false) ? 1400.0f : 0.0f;
    score -= static_cast<float>(CountEnemyHeroesNear(
                 position,
                 static_cast<float>(Slider(DashMenu, "CheckRange", 450)))) * 900.0f;

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy)) {
            continue;
        }
        const float distance = position.Distance2D(enemy.Position());
        const float attackReach = enemy.AttackRange() + enemy.BoundingRadius() +
                                  player.BoundingRadius();
        if (enemy.IsMelee()) {
            score += std::min(distance, 900.0f) * 1.4f;
            if (distance < attackReach + 120.0f) {
                score -= 1800.0f + (attackReach + 120.0f - distance) * 8.0f;
            }
        } else if (distance < attackReach + 50.0f) {
            score -= (attackReach + 50.0f - distance) * 1.5f;
        }
    }
    return score;
}

static Vector3 SmartDashPosition(const AIHeroClient& target, bool emergency) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    const bool hasTarget = ValidHeroTarget(target);
    const Vector3 origin = player.Position();
    const Vector3 targetPosition = hasTarget ? target.Position() : Vector3();
    const float currentTargetDistance = hasTarget
        ? origin.Distance2D(targetPosition)
        : 0.0f;
    const float attackRange = hasTarget ? AttackRangeFrom(target) : 0.0f;
    const bool retreat = emergency || HasMeleePressure(origin);
    const bool chase = hasTarget && !retreat &&
                       currentTargetDistance > attackRange - 35.0f;

    constexpr float pi = 3.14159265358979323846f;
    Vector3 best = {};
    float bestScore = -FLT_MAX;
    for (int i = 0; i < 32; ++i) {
        const float angle = 2.0f * pi * static_cast<float>(i) / 32.0f;
        Vector3 point{
            origin.x + std::cos(angle) * Q.Range,
            origin.y,
            origin.z + std::sin(angle) * Q.Range
        };
        point.y = NavMesh::GetHeightForPosition(point);
        const bool isKiting = hasTarget && point.Distance2D(targetPosition) > currentTargetDistance;
        const bool enforceAA = Bool(DashMenu, "AAcheck", true) && !retreat && !isKiting;
        if (!IsGoodDashPosition(point) ||
            (!emergency && enforceAA && !CanAttackFrom(point, target))) {
            continue;
        }

        float score = DashSafetyScore(point) -
                      point.Distance2D(Game::CursorPos()) * 0.10f;
        if (hasTarget) {
            const float targetDistance = point.Distance2D(targetPosition);
            const float radialChange = targetDistance - currentTargetDistance;

            if (emergency) {
                score += radialChange * 8.0f;
            } else {
                float idealDistance = attackRange * 0.84f;
                if (target.IsMelee()) {
                    const float threatReach = target.AttackRange() +
                                              target.BoundingRadius() +
                                              player.BoundingRadius() + 110.0f;
                    idealDistance = std::min(
                        attackRange - 25.0f,
                        std::max(attackRange * 0.78f, threatReach));
                }
                idealDistance = std::max(100.0f, idealDistance);

                score -= std::abs(targetDistance - idealDistance) * 3.0f;
                if (targetDistance <= attackRange) {
                    score += 700.0f;
                } else {
                    score -= (targetDistance - attackRange) * 12.0f;
                }

                if (retreat) {
                    score += radialChange * 6.0f;
                } else if (chase) {
                    score -= radialChange * 5.0f;
                } else {
                    // At a healthy range, prefer a lateral Q that preserves spacing.
                    score -= std::abs(radialChange) * 1.25f;
                }
            }
        }

        if (score > bestScore) {
            bestScore = score;
            best = point;
        }
    }
    return best;
}

static Vector3 FindDashPosition(const AIHeroClient& target, bool asap = false) {
    Vector3 result = {};
    switch (List(DashMenu, "DashMode", 2)) {
    case 0: result = CursorDashPosition(); break;
    case 1: result = SideDashPosition(target); break;
    default: result = SmartDashPosition(target, asap); break;
    }

    if (!IsGoodDashPosition(result)) {
        return {};
    }
    if (!asap) {
        const auto player = Player();
        const bool retreat = HasMeleePressure(player.Position());
        const bool isKiting = result.Distance2D(target.Position()) > player.Position().Distance2D(target.Position());
        const bool enforceAA = Bool(DashMenu, "AAcheck", true) && !retreat && !isKiting;
        if (enforceAA && !CanAttackFrom(result, target)) {
            return {};
        }
    }
    return result;
}

static AIHeroClient KindredTarget(float range) {
    const auto player = Player();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy)) {
            continue;
        }
        const float expandedRange = range + player.BoundingRadius() + enemy.BoundingRadius();
        if (player.Position().DistanceSqr2D(enemy.Position()) <=
                expandedRange * expandedRange &&
            (enemy.HasBuff("KindredHitTracker") ||
             enemy.HasBuff("kindredhittracker") ||
             enemy.HasBuff("kindredecharge"))) {
            return enemy;
        }
    }
    return GetPhysicalTarget(range);
}

static bool FastBlackE() {
    if (!E.IsReady()) {
        return false;
    }
    const auto player = Player();
    AIHeroClient best = {};
    float bestScore = FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, E.Range) || !EBlacklisted(enemy)) {
            continue;
        }
        const float score = EffectivePhysicalHealth(enemy) -
            Damage::GetAutoAttackDamage(player, enemy, true) * 3.0f;
        if (score < bestScore) {
            bestScore = score;
            best = enemy;
        }
    }
    return best.IsValid() && E.CastOnUnit(best);
}

static void ClearForcedETarget() {
    if (ForcedETarget) {
        Orbwalker::ForceTarget(AttackableUnit());
        ForcedETarget = false;
    }
}

static void UpdateForcedETarget() {
    if (!Bool(MenuRoot, "AttackE", true)) {
        ClearForcedETarget();
        return;
    }

    const auto player = Player();
    AIHeroClient marked = {};
    std::vector<AIHeroClient> attackable;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy) || !AutoAttack::InAutoAttackRange(enemy)) {
            continue;
        }
        attackable.push_back(enemy);
        if (!marked.IsValid() && enemy.HasBuff("kindredecharge")) {
            marked = enemy;
        }
    }
    if (!marked.IsValid()) {
        ClearForcedETarget();
        return;
    }

    const float markedDamage = Damage::GetAutoAttackDamage(player, marked, true);
    AIHeroClient forced = marked;
    if (EffectivePhysicalHealth(marked) > markedDamage * 3.0f) {
        for (const auto& enemy : attackable) {
            if (SameUnit(enemy, marked)) {
                continue;
            }
            const float damage = Damage::GetAutoAttackDamage(player, enemy, true);
            if (EffectivePhysicalHealth(enemy) <= damage * 2.0f) {
                forced = enemy;
                break;
            }
        }
    }

    Orbwalker::ForceTarget(AttackableUnit(forced.Handle()));
    ForcedETarget = true;
}

static bool TryClassicUltimateOn(const AIHeroClient& ally) {
    const auto player = Player();
    if (!R.IsReady() || !ValidAlly(ally, R.Range) || ally.IsRecalling()) {
        return false;
    }
    if (ally.HealthPercent() < 10.0f &&
        player.CountEnemyHeroesInRange(R.Range + 400.0f) >= 1 &&
        ally.CountEnemyHeroesInRange(675.0f) >= 1) {
        return R.Cast();
    }
    return SDK::HealthPrediction::GetPrediction(ally, 300) <= 100.0f && R.Cast();
}

static bool AutoR() {
    if (!Bool(MenuRoot, "autoR", true) || !R.IsReady()) {
        return false;
    }
    const auto player = Player();
    if (player.IsValid() && TryClassicUltimateOn(player)) {
        return true;
    }
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!SameUnit(ally, player) && TryClassicUltimateOn(ally)) {
            return true;
        }
    }
    return false;
}

static bool Combo() {
    const auto player = Player();
    if (Orbwalker::IsWindingUp()) {
        return false;
    }

    if (Bool(ComboMenu, "CQ", true) && Q.IsReady()) {
        const float aaRange = AutoAttack::GetRealAutoAttackRange(player);
        const auto target = GetPhysicalTarget(aaRange + Q.Range);
        if (ValidHeroTarget(target) && !AutoAttack::InAutoAttackRange(target)) {
            const Vector3 dashPos = player.Position().Extend(target.Position(), Q.Range);
            if (IsGoodDashPosition(dashPos) && Q.Cast(dashPos)) {
                return true;
            }
        }
    }

    if (Bool(ComboMenu, "CE", true) && E.IsReady()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, E.Range) || EBlacklisted(enemy)) {
                continue;
            }
            if (Bool(ComboMenu, "comboAdvancedE", true) &&
                EffectivePhysicalHealth(enemy) <=
                    Damage::GetAutoAttackDamage(player, enemy, true) * 2.0f) {
                continue;
            }
            if (E.CastOnUnit(enemy)) {
                return true;
            }
        }
    }

    if (Bool(ComboMenu, "CW", true) && W.IsReady()) {
        const float minimumDistance = static_cast<float>(
            Slider(ComboMenu, "comboDistanceW", 450));
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, W.Range) &&
                enemy.Distance(player) <= minimumDistance &&
                W.Cast(enemy.Position())) {
                return true;
            }
        }
    }
    return false;
}

static bool IsLargeJungleMonster(const AIMinionClient& mob) {
    const JungleType type = mob.GetJungleType();
    return type == JungleType::Large ||
           type == JungleType::Epic ||
           type == JungleType::Legendary;
}

static bool LaneClear() {
    const auto player = Player();
    bool casted = false;
    if (player.ManaPercent() >= static_cast<float>(Slider(LaneMenu, "Lmana", 40))) {
        std::vector<AIMinionClient> minions;
        const float attackRange = AutoAttack::GetRealAutoAttackRange(player);
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (ValidTarget(minion, attackRange)) {
                minions.push_back(minion);
            }
        }
        if (static_cast<int>(minions.size()) >= Slider(LaneMenu, "LQC", 3)) {
            if (Bool(LaneMenu, "LQ", true) && Q.IsReady()) {
                casted = Q.Cast(Game::CursorPos()) || casted;
            }
            if (Bool(LaneMenu, "LW", false) && W.IsReady()) {
                casted = W.Cast() || casted;
            }
        }
    }

    if (player.ManaPercent() < static_cast<float>(Slider(JungleMenu, "Jmana", 40))) {
        return casted;
    }

    std::vector<AIMinionClient> mobs;
    const float attackRange = AutoAttack::GetRealAutoAttackRange(player);
    for (const auto& mob : GameObjects::Jungle()) {
        if (ValidTarget(mob, attackRange) && !mob.IsPlant() && !mob.IsPet()) {
            mobs.push_back(mob);
        }
    }
    if (mobs.empty()) {
        return casted;
    }

    bool hasBigMob = false;
    for (const auto& mob : mobs) {
        if (IsLargeJungleMonster(mob) && ValidTarget(mob, W.Range)) {
            hasBigMob = true;
        }
        if (Bool(JungleMenu, "JE", true) && E.IsReady() &&
            IsLargeJungleMonster(mob) && ValidTarget(mob, E.Range) &&
            mob.Health() > Damage::GetAutoAttackDamage(player, mob, true) * 2.0f) {
            casted = E.CastOnUnit(mob) || casted;
            break;
        }
    }
    if (Bool(JungleMenu, "JQ", true) && Q.IsReady()) {
        casted = Q.Cast(Game::CursorPos()) || casted;
    }
    if (Bool(JungleMenu, "JW", true) && W.IsReady() &&
        (hasBigMob || mobs.size() >= 3)) {
        casted = W.Cast() || casted;
    }
    return casted;
}

static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded || !Bool(MenuRoot, "autoR", true) || !R.IsReady() ||
        !args.IsAutoAttack || args.Target.Ptr == 0) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsZombie()) {
        return;
    }

    const AIHeroClient sender = HeroFromInfo(args.Sender);
    const AIHeroClient target = HeroFromInfo(args.Target);
    if (!sender.IsValid() || !sender.IsEnemy() || !target.IsValid() ||
        !ValidAlly(target, R.Range)) {
        return;
    }
    if (Damage::GetAutoAttackDamage(sender, target, true) * 1.2f > target.Health()) {
        (void)R.Cast();
    }
}

static void OnAfterAttack(OrbwalkingActionArgs& args) {
    const auto player = Player();
    if (!Loaded || Orbwalker::ActiveMode() != OrbwalkingMode::Combo ||
        !Bool(ComboMenu, "CQ", true) || !Q.IsReady() ||
        !player.IsValid() || player.IsDead() || player.IsRecalling() ||
        Game::IsChatOpen()) {
        return;
    }

    const AIBaseClient attacked(args.Target.Handle());
    if (attacked.IsValid() && !attacked.IsHero()) {
        return;
    }

    AIHeroClient target = attacked.IsValid()
        ? AIHeroClient(attacked.Handle())
        : AIHeroClient();
    if (!ValidHeroTarget(target)) {
        target = KindredTarget(
            AutoAttack::GetRealAutoAttackRange(player) + Q.Range + 150.0f);
    }
    if (!ValidHeroTarget(target)) {
        return;
    }

    Vector3 position = FindDashPosition(target, false);
    if (position.IsZero()) {
        position = Game::CursorPos();
    }

    if (!position.IsZero()) {
        (void)Q.Cast(position);
    }
}

static void OnGapcloser(const GapCloserEventArgs& args) {
    const auto player = Player();
    const AIHeroClient sender(args.Sender);
    if (!Loaded || !player.IsValid() || !ValidHeroTarget(sender) ||
        args.Start.Distance2D(player.Position()) <= args.End.Distance2D(player.Position())) {
        return;
    }
    if (Bool(AntiGapMenu, "AntiGapE", true) && E.IsReady() &&
        ValidHeroTarget(sender, E.Range) && E.CastOnUnit(sender)) {
        return;
    }
    if (Bool(AntiGapMenu, "AntiGapQ", true) && Q.IsReady() &&
        ValidHeroTarget(sender, 400.0f)) {
        const Vector3 position = FindDashPosition(sender, true);
        if (!position.IsZero()) {
            (void)Q.Cast(position);
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!Loaded || !player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DQ", true) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFFFFFFu, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DW", true) && W.IsReady()) {
        Drawing::DrawCircle(
            player.Position(),
            static_cast<float>(Slider(ComboMenu, "comboDistanceW", 450)),
            0xFFFFD700u,
            1.5f,
            64);
    }
    if (Bool(DrawMenu, "DE", true) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFF1E90FFu, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DR", true) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFADFF2Fu, 1.5f, 64);
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!Loaded || !player.IsValid() || player.IsDead() || player.IsRecalling() ||
        Game::IsChatOpen()) {
        ClearForcedETarget();
        return;
    }

    if (Key(MenuRoot, "FastE", false) && FastBlackE()) {
        return;
    }
    UpdateForcedETarget();
    if (AutoR()) {
        return;
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        (void)Combo();
        break;
    case OrbwalkingMode::LaneClear:
        (void)LaneClear();
        break;
    default:
        break;
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.kindred", "Kuro - Kindred", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q", true));
    ComboMenu->Add(new MenuBool("CW", "Use W", true));
    ComboMenu->Add(new MenuBool("CE", "Use E", true));
    EBlacklistMenu = ComboMenu->AddSubMenu(new Menu("Eset", "E Blacklist"));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid()) {
            continue;
        }
        const std::string key = EBlacklistKey(enemy);
        EBlacklistMenu->Add(new MenuBool(
            key.c_str(), enemy.CharacterName().c_str(), false));
    }
    ComboMenu->Add(new MenuBool(
        "comboAdvancedE",
        "Don't E if target health is within two attacks",
        true));
    ComboMenu->Add(new MenuSlider(
        "comboDistanceW", "Use W maximum enemy distance", 450, 180, 900));

    LaneMenu = MenuRoot->AddSubMenu(new Menu("LaneClear", "Lane Clear Settings"));
    LaneMenu->Add(new MenuBool("LQ", "Use Q", true));
    LaneMenu->Add(new MenuSlider("LQC", "Minimum minions for Q", 3, 1, 3));
    LaneMenu->Add(new MenuBool("LW", "Use W", false));
    LaneMenu->Add(new MenuSlider("Lmana", "Minimum mana percent", 40, 0, 100));

    JungleMenu = MenuRoot->AddSubMenu(new Menu("JungleClear", "Jungle Clear Settings"));
    JungleMenu->Add(new MenuBool("JQ", "Use Q", true));
    JungleMenu->Add(new MenuBool("JW", "Use W", true));
    JungleMenu->Add(new MenuBool("JE", "Use E on large monsters", true));
    JungleMenu->Add(new MenuSlider("Jmana", "Minimum mana percent", 40, 0, 100));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw Settings"));
    DrawMenu->Add(new MenuBool("DQ", "Draw Q", false));
    DrawMenu->Add(new MenuBool("DW", "Draw W trigger distance", false));
    DrawMenu->Add(new MenuBool("DE", "Draw E", false));
    DrawMenu->Add(new MenuBool("DR", "Draw R", false));

    AntiGapMenu = MenuRoot->AddSubMenu(
        new Menu("AntiGapcloser", "Anti-Gapcloser Settings"));
    AntiGapMenu->Add(new MenuBool("AntiGapQ", "Anti-Gap Q", true));
    AntiGapMenu->Add(new MenuBool("AntiGapE", "Anti-Gap E", true));

    DashMenu = MenuRoot->AddSubMenu(new Menu("QDash", "Dash Spell Settings"));
    DashMenu->Add(new MenuList(
        "DashMode", "Dash mode", { "Mouse", "Side", "Smart" }, 2));
    DashMenu->Add(new MenuSlider(
        "EnemyCheck", "Maximum enemies at dash end", 3, 1, 5));
    DashMenu->Add(new MenuSlider(
        "CheckRange", "Enemy detection range", 450, 100, 800));
    DashMenu->Add(new MenuBool("WallCheck", "Don't dash through walls", true));
    DashMenu->Add(new MenuKeyBind(
        "TurretCheck",
        "Don't dash under enemy turret",
        SDK::Keys::A,
        KeyBindType::Toggle,
        true))->Permashow();
    DashMenu->Add(new MenuBool(
        "AAcheck", "Keep an enemy in attack range", true));
    DashMenu->Add(new MenuBool(
        "notDashAARange", "Avoid melee enemy attack range", true));

    MenuRoot->Add(new MenuBool("autoR", "Auto R", true));
    MenuRoot->Add(new MenuBool("AttackE", "Force attack E target", true));
    MenuRoot->Add(new MenuKeyBind(
        "FastE",
        "Fast E on blacklisted target",
        SDK::Keys::E,
        KeyBindType::Press))->Permashow();

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }
    if (auto* item = MenuRoot->Get<MenuKeyBind>("FastE")) {
        item->RemovePermashow();
    }
    if (auto* item = DashMenu ? DashMenu->Get<MenuKeyBind>("TurretCheck") : nullptr) {
        item->RemovePermashow();
    }
    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    EBlacklistMenu = nullptr;
    LaneMenu = nullptr;
    JungleMenu = nullptr;
    DrawMenu = nullptr;
    AntiGapMenu = nullptr;
    DashMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 340.0f);
    W = Spell(SpellSlot::W, 900.0f);
    E = Spell(SpellSlot::E, 575.0f);
    R = Spell(SpellSlot::R, 500.0f);
    Q.SetSkillshot(0.25f, 30.0f, 1400.0f, false, SkillshotType::SkillshotLine);
    Q.DamageType = DamageType::Physical;
    W.DamageType = DamageType::Magical;
    E.SetTargetted(0.1f, FLT_MAX);
    E.DamageType = DamageType::Physical;

    ForcedETarget = false;
    BuildMenu();
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnGapCloser += &OnGapcloser;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#D7A9FF' size='20'>Kuro - Kindred loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnGapCloser -= &OnGapcloser;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Drawing::OnDraw -= &OnDraw;
    ClearForcedETarget();
    RemoveMenu();
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Kindred
