#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::Katarina {

inline Menu* MenuRoot = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 625.0f };
inline Spell W{ SpellSlot::W, 340.0f };
inline Spell E{ SpellSlot::E, 775.0f };
inline Spell R{ SpellSlot::R, 550.0f };

struct Dagger {
    GameObject Unit;
    Vector3 Position;
    int CreateTick = 0;
    int NetworkId = 0;
};

inline bool Loaded = false;
inline bool UpdateR = false;
inline int LastR = 0;
inline int LastCastE = 0;
inline std::vector<Dagger> Daggers;

static std::string RuntimeName(const GameObject& object) {
    return GetObjectCharacterName(object);
}

static bool IsHiddenMinion(const GameObject& object) {
    if (!object.IsValid()) {
        return false;
    }
    const std::string name = GetObjectName(object);
    return EqualsIgnoreCase(name.c_str(), "Katarina_Base_W_Indicator_Ally");
}

static bool HaveRBuff() {
    return Player().HasBuff("KatarinaRSound");
}


static bool IsOwnDagger(const Dagger& dagger) {
    return dagger.Unit.IsValid();
}

static bool IsDaggerReady(const Dagger& dagger, int minAgeMs = 1000) {
    return IsOwnDagger(dagger) && SDK::Variables::TickCount() - dagger.CreateTick >= minAgeMs;
}

static void PruneDaggers() {
    const int now = SDK::Variables::TickCount();
    Daggers.erase(
        std::remove_if(
            Daggers.begin(),
            Daggers.end(),
            [now](const Dagger& dagger) {
                return now - dagger.CreateTick >= 5000 ||
                       !dagger.Unit.IsValid() ||
                       dagger.Position.IsZero();
            }),
        Daggers.end());
}

static bool HasOwnDagger() {
    return std::any_of(Daggers.begin(), Daggers.end(), [](const Dagger& dagger) {
        return IsOwnDagger(dagger);
    });
}

static int CountOwnDaggersNear(const AIBaseClient& target, float range) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& dagger : Daggers) {
        if (IsOwnDagger(dagger) &&
            dagger.Position.DistanceSqr2D(target.Position()) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

static int SpellRank(const Spell& spell, int maxRank) {
    return std::clamp(spell.Level(), 0, maxRank);
}

static float TotalAttackDamage() {
    const auto player = Player();
    return player.IsValid() ? player.TotalAttackDamage() : 0.0f;
}

static float BonusAttackSpeed() {
    const auto player = Player();
    return player.IsValid() ? std::max(0.0f, player.AttackSpeedMod() - 1.0f) : 0.0f;
}

static float PassiveDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    // Voracity / Sinister Steel: 68-240 based on champion level
    // +60% bonus AD +70/80/90/100% AP based on champion level.
    static constexpr float baseDamage[] = {
        0.0f, 68.0f, 72.0f, 77.0f, 82.0f, 89.0f, 96.0f, 103.0f, 112.0f,
        121.0f, 131.0f, 142.0f, 154.0f, 166.0f, 180.0f, 194.0f, 208.0f,
        225.0f, 240.0f
    };
    const int level = std::clamp(player.Level(), 1, 18);
    float apRatio = 1.00f;
    if (level < 6) {
        apRatio = 0.70f;
    } else if (level < 11) {
        apRatio = 0.80f;
    } else if (level < 16) {
        apRatio = 0.90f;
    }

    const float raw =
        baseDamage[level] +
        apRatio * player.AP() +
        0.60f * player.BonusAttackDamage();
    return player.CalculateMagicDamage(target, raw);
}

static float QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    static constexpr float baseDamage[] = { 0.0f, 80.0f, 115.0f, 150.0f, 185.0f, 220.0f };
    const int rank = SpellRank(Q, 5);
    if (rank <= 0) {
        return 0.0f;
    }

    return player.CalculateMagicDamage(target, baseDamage[rank] + 0.40f * player.AP());
}

static float EDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    static constexpr float baseDamage[] = { 0.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f };
    const int rank = SpellRank(E, 5);
    if (rank <= 0) {
        return 0.0f;
    }

    return player.CalculateMagicDamage(
        target,
        baseDamage[rank] + 0.40f * TotalAttackDamage() + 0.25f * player.AP());
}

static float EOnHitDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    // Shunpo applies on-hit effects, but it does not add the basic attack's
    // base physical damage. GetPassiveDamage returns only those extra effects.
    return Damage::GetPassiveDamage(player, target);
}

static float RDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    const int rank = SpellRank(R, 3);
    if (rank <= 0) {
        return 0.0f;
    }

    static constexpr float baseMagicDamage[] = { 0.0f, 375.0f, 562.5f, 750.0f };
    const float magicDamage = player.CalculateMagicDamage(
        target,
        baseMagicDamage[rank] + 2.85f * player.AP());
    const float physicalDamage = player.CalculatePhysicalDamage(
        target,
        (2.40f + 7.50f * BonusAttackSpeed()) * player.BonusAttackDamage());

    const float damage = magicDamage + physicalDamage;

    // Giữ nguyên hệ số khoảng cách của R theo yêu cầu.
    return target.DistanceToPlayer() <= 350.0f ? damage : damage * 0.60f;
}

static bool UnderTower(const Vector3& position) {
    const auto player = Player();
    const float extraRadius = player.IsValid() ? player.BoundingRadius() : 65.0f;
    const float range = 850.0f + extraRadius;
    const float rangeSqr = range * range;

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*for (const auto& turret : GameObjects::EnemyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) {
            continue;
        }
        if (turret.Position().DistanceSqr2D(position) <= rangeSqr) {
            return true;
        }
    }*/

    for (const auto& spawn : GameObjects::EnemySpawnPoints()) {
        if (spawn.IsValid() && spawn.Position().DistanceSqr2D(position) <= rangeSqr) {
            return true;
        }
    }
    return false;
}

static bool AllowDashTo(const Vector3& position) {
    if (position.IsZero() || NavMesh::IsWall(position)) {
        return false;
    }
    return Key(MenuRoot, "Turret") || !UnderTower(position);
}

static bool CastE(const Vector3& position) {
    const int now = SDK::Variables::TickCount();
    if (!E.IsReady() ||
        now - LastCastE <= 250 ||
        position.IsZero() ||
        Player().Distance(position) > E.Range) {
        return false;
    }

    if (!E.Cast(position)) {
        return false;
    }

    LastCastE = SDK::Variables::TickCount();
    return true;
}

static bool CastQ(const AIBaseClient& target) {
    return Q.IsReady() &&
           ValidTarget(target, Q.Range) &&
           Q.Cast(target) == CastStates::SuccessfullyCasted;
}

static const Dagger* BestDaggerNearTarget(const AIBaseClient& target, float range) {
    const Dagger* best = nullptr;
    const auto player = Player();
    for (const auto& dagger : Daggers) {
        if (!IsOwnDagger(dagger) ||
            player.Distance(dagger.Position) > E.Range + 150.0f ||
            dagger.Position.Distance2D(target.Position()) > range) {
            continue;
        }

        if (!best ||
            player.Distance(dagger.Position) < player.Distance(best->Position)) {
            best = &dagger;
        }
    }
    return best;
}

static const Dagger* BestDaggerNearPosition(const Vector3& position, float range) {
    const Dagger* best = nullptr;
    const auto player = Player();
    for (const auto& dagger : Daggers) {
        if (!IsOwnDagger(dagger) ||
            player.Distance(dagger.Position) > E.Range + 150.0f ||
            dagger.Position.Distance2D(position) > range) {
            continue;
        }

        if (!best ||
            player.Distance(dagger.Position) < player.Distance(best->Position)) {
            best = &dagger;
        }
    }
    return best;
}

static Vector3 GetBestECastPos(
    const AIBaseClient& target,
    const Dagger& dagger,
    float maxTargetDistance = 340.0f) {
    const auto player = Player();
    const Vector3 A = player.Position();
    const Vector3 B = dagger.Position;
    const Vector3 T = target.Position();
    const float R_A = E.Range;
    const float playerRadius = player.BoundingRadius();
    const float targetRadius = target.BoundingRadius();
    const float R_B = 150.0f + playerRadius;
    const float distToDagger = T.Distance2D(B);
    Vector3 bestPos;

    if (distToDagger <= 150.0f) {
        bestPos = T.Extend(B, 10.0f);
    } else if (distToDagger <= 150.0f + targetRadius + playerRadius - 5.0f) {
        bestPos = T.Extend(B, targetRadius - 5.0f);
    } else {
        bestPos = B.Extend(T, R_B);
    }

    if (A.Distance2D(bestPos) <= R_A) {
        bestPos.y = NavMesh::GetHeightForPosition(bestPos);
        if (T.Distance2D(bestPos) <= maxTargetDistance) {
            return bestPos;
        }
        return Vector3();
    }

    const float d = A.Distance2D(B);
    if (d > R_A + R_B) {
        return Vector3();
    }

    Vector3 P_B = B.Extend(T, R_B);
    if (A.Distance2D(P_B) <= R_A) {
        P_B.y = NavMesh::GetHeightForPosition(P_B);
        if (T.Distance2D(P_B) <= maxTargetDistance) {
            return P_B;
        }
        return Vector3();
    }

    Vector3 P_A = A.Extend(T, R_A);
    if (B.Distance2D(P_A) <= R_B) {
        P_A.y = NavMesh::GetHeightForPosition(P_A);
        if (T.Distance2D(P_A) <= maxTargetDistance) {
            return P_A;
        }
        return Vector3();
    }

    if (d > 0.0f) {
        const float a = (R_A * R_A - R_B * R_B + d * d) / (2.0f * d);
        const float h_sqr = R_A * R_A - a * a;
        if (h_sqr >= 0.0f) {
            const float h = std::sqrt(h_sqr);
            const Vector3 dir = (B - A).Normalized();
            const Vector3 P_m = A + dir * a;
            const Vector3 perp{-dir.z, 0.0f, dir.x};
            Vector3 I1 = P_m + perp * h;
            Vector3 I2 = P_m - perp * h;
            I1.y = NavMesh::GetHeightForPosition(I1);
            I2.y = NavMesh::GetHeightForPosition(I2);
            Vector3 bestI = (T.Distance2D(I1) < T.Distance2D(I2)) ? I1 : I2;
            if (T.Distance2D(bestI) <= maxTargetDistance) {
                return bestI;
            }
        }
    }

    return Vector3();
}

static AIHeroClient BestDaggerTarget(float range) {
    AIHeroClient best;
    float bestScore = FLT_MAX;
    const auto player = Player();
    for (const auto& target : EnemyHeroes(range)) {
        if (!ValidHeroTarget(target, range)) {
            continue;
        }

        bool canReach = false;
        if (player.Distance(target.Position()) <= E.Range) {
            canReach = true;
        } else {
            for (const auto& dagger : Daggers) {
                if (IsOwnDagger(dagger) &&
                    player.Distance(dagger.Position) <= E.Range + 150.0f) {
                    Vector3 testPos = GetBestECastPos(target, dagger);
                    if (!testPos.IsZero()) {
                        canReach = true;
                        break;
                    }
                }
            }
        }

        if (!canReach) {
            continue;
        }

        const int daggersNear = CountOwnDaggersNear(target, 350.0f);
        float score = target.Health() - PassiveDamage(target) * static_cast<float>(daggersNear);
        
        if (daggersNear > 0) {
            score -= 1000.0f;
        }

        if (!best.IsValid() || score < bestScore) {
            best = target;
            bestScore = score;
        }
    }
    return best;
}

static bool ShouldCancelRForKillsteal(const AIBaseClient& target) {
    if (!HaveRBuff()) {
        return true;
    }
    if (Key(RMenu, "NeverCancelR")) {
        return false;
    }
    if (target.DistanceToPlayer() > R.Range) {
        return true;
    }
    if (SDK::Variables::TickCount() - LastR >= 1500) {
        return true;
    }
    if (Player().CountEnemyHeroesInRange(R.Range) == 0) {
        return true;
    }
    return false;
}

static bool ShouldKeepR() {
    if (!HaveRBuff()) {
        return false;
    }
    if (Key(RMenu, "NeverCancelR") && Player().CountEnemyHeroesInRange(R.Range) > 0) {
        return true;
    }
    if (Player().CountEnemyHeroesInRange(R.Range) == 0) {
        return false;
    }
    if (SDK::Variables::TickCount() - LastR < 1500) {
        return true;
    }
    return false;
}

static bool DoComboE(AIBaseClient& outTarget, bool checkForDagger = true) {
    if (HaveRBuff() && Key(RMenu, "NeverCancelR")) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid() || !E.IsReady()) {
        return false;
    }

    AIHeroClient target = HasOwnDagger()
        ? BestDaggerTarget(E.Range + 400.0f)
        : GetMagicalTarget(E.Range + 400.0f);
    if (!ValidHeroTarget(target, E.Range + 400.0f)) {
        return false;
    }

    outTarget = AIBaseClient(target.Handle());

    const Dagger* dagger = BestDaggerNearTarget(target, 490.0f);
    if (dagger && IsOwnDagger(*dagger)) {
        Vector3 castPos = GetBestECastPos(target, *dagger);
        if (AllowDashTo(castPos) && CastE(castPos)) {
            return true;
        }
    }

    if (checkForDagger && !Q.IsReady() && !W.IsReady() && !dagger) {
        return false;
    }

    Vector3 castPos = target.Position();
    Vector3 pathPos = target.Position() + target.Direction() * 100.0f;
    if (target.IsMoving()) {
        const auto waypoints = target.Path();
        if (waypoints.size() >= 2) {
            pathPos = waypoints[1];
        }
    }

    if (Q.IsReady()) {
        if (target.IsMoving()) {
            castPos = target.Position().Extend(pathPos, -10.0f);
        } else {
            castPos = target.Position();
        }
    } else if (W.IsReady()) {
        if (target.IsMoving()) {
            castPos = target.Position().Extend(pathPos, target.BoundingRadius() + 50.0f);
        } else {
            castPos = target.Position();
        }
    } else {
        castPos = target.Position();
    }

    castPos.y = NavMesh::GetHeightForPosition(castPos);
    if (player.Distance(castPos) <= E.Range && AllowDashTo(castPos)) {
        return CastE(castPos);
    }
    
    Vector3 fallbackPos = target.Position().Extend(player.Position(), -50.0f);
    fallbackPos.y = NavMesh::GetHeightForPosition(fallbackPos);
    if (player.Distance(fallbackPos) <= E.Range && AllowDashTo(fallbackPos)) {
        return CastE(fallbackPos);
    }

    return false;
}

static bool TryEKillSteal() {
    if (!E.IsReady() || !Bool(EMenu, "EKs")) {
        return false;
    }

    for (const auto& target : EnemyHeroes(E.Range)) {
        if (!ShouldCancelRForKillsteal(target)) {
            continue;
        }
        const float damage = EDamage(target) + EOnHitDamage(target);
        if (target.Health() <= damage &&
            AllowDashTo(target.Position()) &&
            CastE(target.Position())) {
            return true;
        }
    }
    return false;
}

static bool TryDaggerKillSteal() {
    if (!E.IsReady() || !Bool(EMenu, "EKs")) {
        return false;
    }

    for (const auto& dagger : Daggers) {
        if (!IsDaggerReady(dagger) || Player().Distance(dagger.Position) > E.Range + 150.0f) {
            continue;
        }

        for (const auto& target : EnemyHeroes(E.Range + W.Range)) {
            if (!ShouldCancelRForKillsteal(target)) {
                continue;
            }
            if (dagger.Position.Distance2D(target.Position()) > 490.0f ||
                target.Health() > PassiveDamage(target)) {
                continue;
            }

            Vector3 castPos = GetBestECastPos(target, dagger);
            if (castPos.IsZero() || castPos.Distance2D(target.Position()) > W.Range) {
                continue;
            }
            if (AllowDashTo(castPos) && CastE(castPos)) {
                return true;
            }
        }
    }
    return false;
}

static bool TryQKillSteal() {
    if (!Q.IsReady() || !Bool(QMenu, "useQKS")) {
        return false;
    }

    for (const auto& target : EnemyHeroes(Q.Range)) {
        if (!ShouldCancelRForKillsteal(target)) {
            continue;
        }
        if (target.Health() <= QDamage(target) && CastQ(target)) {
            return true;
        }
    }
    return false;
}

static bool TryEQKillSteal() {
    if (!Q.IsReady() ||
        !E.IsReady() ||
        !Bool(QMenu, "useQKS") ||
        !Bool(EMenu, "EKs")) {
        return false;
    }

    const auto target = GetMagicalTarget(E.Range + Q.Range);
    if (!ValidHeroTarget(target, E.Range + Q.Range) ||
        target.Health() > QDamage(target)) {
        return false;
    }

    if (!ShouldCancelRForKillsteal(target)) {
        return false;
    }

    for (const auto& dagger : Daggers) {
        if (IsOwnDagger(dagger) &&
            Player().Distance(dagger.Position) <= E.Range + 150.0f) {
            Vector3 castPos = GetBestECastPos(target, dagger, Q.Range);
            if (!castPos.IsZero() &&
                castPos.Distance2D(target.Position()) <= Q.Range &&
                AllowDashTo(castPos) &&
                CastE(castPos)) {
                return true;
            }
        }
    }

    AIBaseClient bestObject;
    float bestDistance = FLT_MAX;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidTarget(minion, E.Range) ||
            minion.Position().Distance2D(target.Position()) > Q.Range) {
            continue;
        }
        const float distance = minion.Position().Distance2D(target.Position());
        if (!bestObject.IsValid() || distance < bestDistance) {
            bestObject = AIBaseClient(minion.Handle());
            bestDistance = distance;
        }
    }

    if (bestObject.IsValid() && AllowDashTo(bestObject.Position())) {
        return CastE(bestObject.Position());
    }
    return false;
}

static bool TryCastW() {
    if (!W.IsReady()) {
        return false;
    }

    const auto target = GetMagicalTarget(W.Range);
    if (ValidHeroTarget(target, W.Range) &&
        target.DistanceToPlayer() <= static_cast<float>(Slider(WMenu, "WRange", 300))) {
        return W.Cast();
    }
    return false;
}

static bool TryCastR() {
    if (!R.IsReady()) {
        return false;
    }

    const auto target = GetMagicalTarget(R.Range);
    if (!ValidHeroTarget(target, R.Range)) {
        return false;
    }

    if (target.Health() <= RDamage(target)) {
        return R.Cast();
    }

    if (Key(RMenu, "RCombo")) {
        return R.Cast();
    }

    if (Player().CountEnemyHeroesInRange(R.Range) >= Slider(RMenu, "RCount", 3)) {
        return R.Cast();
    }
    return false;
}

static void EQ() {
    if (HaveRBuff() && Key(RMenu, "NeverCancelR")) {
        return;
    }

    AIBaseClient eTarget;
    bool eCasted = false;
    if (E.IsReady() && DoComboE(eTarget, Key(EMenu, "SaveEIfNoDaggers", true))) {
        eCasted = true;
    }

    if (!eCasted && E.IsReady(500)) {
        return;
    }

    AIBaseClient qTarget = eTarget;
    if (!ValidTarget(qTarget, Q.Range)) {
        qTarget = AIBaseClient(GetMagicalTarget(Q.Range).Handle());
    }
    (void)CastQ(qTarget);

    if (TryCastW()) {
        return;
    }
    (void)TryCastR();
}

static void QE() {
    if (HaveRBuff() && Key(RMenu, "NeverCancelR")) {
        return;
    }

    const auto qTarget = GetMagicalTarget(Q.Range);
    (void)CastQ(qTarget);

    AIBaseClient eTarget;
    bool eCasted = false;
    if (E.IsReady() && DoComboE(eTarget, Key(EMenu, "SaveEIfNoDaggers", true))) {
        eCasted = true;
    }

    if (!eCasted && E.IsReady(500)) {
        return;
    }

    if (TryCastW()) {
        return;
    }
    (void)TryCastR();
}

static void Combo() {
    switch (List(MenuRoot, "KataComboMode", 0)) {
    case 0:
        EQ();
        break;
    case 1:
        QE();
        break;
    default:
        QE();
        EQ();
        break;
    }
}

static void Harass() {
    if (!Bool(QMenu, "AutoQ")) {
        return;
    }

    const auto target = GetMagicalTarget(Q.Range);
    (void)CastQ(target);
}

static void LastHit() {
    if (!Q.IsReady()) {
        return;
    }

    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, Q.Range) &&
            minion.Health() <= QDamage(minion) &&
            CastQ(minion)) {
            return;
        }
    }
}

static void UpdateOrbwalkerState() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (HaveRBuff()) {
        if (!UpdateR) {
            LastR = SDK::Variables::TickCount();
        }
        UpdateR = true;
        Orbwalker::AttackEnabled(false);
        Orbwalker::MoveEnabled(false);

        if (!Key(RMenu, "NeverCancelR") &&
            player.HealthPercent() <= 50.0f &&
            SDK::Variables::TickCount() - LastR > 500 &&
            SDK::Variables::TickCount() - LastR < 5000) {
            const auto target = BestDaggerTarget(E.Range + 400.0f);
            if (ValidHeroTarget(target, E.Range + 400.0f)) {
                const float damage =
                    (Q.IsReady() ? QDamage(target) : 0.0f) +
                    (E.IsReady() ? EDamage(target) : 0.0f) +
                    PassiveDamage(target) *
                        static_cast<float>(CountOwnDaggersNear(target, 350.0f));
                if (target.Health() <= damage) {
                    Orbwalker::AttackEnabled(true);
                    Orbwalker::MoveEnabled(true);
                }
            }
        }
        return;
    }

    UpdateR = false;
    Orbwalker::MoveEnabled(true);

    if (!Bool(WMenu, "WOrb") || !HasOwnDagger()) {
        Orbwalker::AttackEnabled(true);
        Orbwalker::SetOrbwalkerPosition({});
        return;
    }

    const Dagger* closeDagger = nullptr;
    for (const auto& dagger : Daggers) {
        if (!IsDaggerReady(dagger, 750) || Player().Distance(dagger.Position) >= 300.0f) {
            continue;
        }
        if (!closeDagger ||
            dagger.CreateTick < closeDagger->CreateTick ||
            player.Distance(dagger.Position) < player.Distance(closeDagger->Position)) {
            closeDagger = &dagger;
        }
    }

    if (!closeDagger) {
        Orbwalker::AttackEnabled(true);
        Orbwalker::SetOrbwalkerPosition({});
        return;
    }

    if (Orbwalker::IsWindingUp()) {
        Orbwalker::AttackEnabled(true);
        Orbwalker::SetOrbwalkerPosition({});
        return;
    }

    Orbwalker::AttackEnabled(false);
    const auto target = GetMagicalTarget(W.Range);
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo &&
        ValidHeroTarget(target, W.Range) &&
        player.Distance(closeDagger->Position) < 100.0f) {
        Orbwalker::SetOrbwalkerPosition(closeDagger->Position.Extend(target.Position(), 100.0f));
    } else {
        Orbwalker::SetOrbwalkerPosition({});
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    PruneDaggers();
    UpdateOrbwalkerState();

    if (player.IsDead() || Game::IsChatOpen()) {
        return;
    }

    if (ShouldKeepR()) {
        return;
    }

    if (TryDaggerKillSteal() || TryEKillSteal() || TryQKillSteal() || TryEQKillSteal()) {
        return;
    }

    if (Orbwalker::IsWindingUp()) {
        return;
    }

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LastHit:
        LastHit();
        break;
    default:
        break;
    }
}

static void OnDoCast(const Events::ProcessSpellEventArgs& args) {
    if (Events::IsLocalPlayer(args.Sender) &&
        args.Slot == static_cast<int>(SpellSlot::E)) {
        LastCastE = SDK::Variables::TickCount();
    }
}

static void OnObjectCreate(const GameObject& object) {
    if (!IsHiddenMinion(object)) {
        return;
    }

    const int networkId = object.NetworkId();
    const auto existing = std::find_if(Daggers.begin(), Daggers.end(), [networkId](const Dagger& dagger) {
        return dagger.NetworkId == networkId;
    });
    if (existing != Daggers.end()) {
        return;
    }

    Daggers.push_back({ object, object.Position(), SDK::Variables::TickCount(), networkId });
}

static void OnObjectDelete(const GameObject& object) {
    if (!IsHiddenMinion(object)) {
        return;
    }

    const int networkId = object.NetworkId();
    Daggers.erase(
        std::remove_if(
            Daggers.begin(),
            Daggers.end(),
            [networkId](const Dagger& dagger) {
                return dagger.NetworkId == networkId;
            }),
        Daggers.end());
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    if (Bool(DrawMenu, "DrawDaggers")) {
        for (const auto& dagger : Daggers) {
            if (!IsOwnDagger(dagger)) {
                continue;
            }
            Render::DrawRingImGui(dagger.Position, 340.0f + 150.0f, 340.0f, 0xFFFFD700u, 60);
            Drawing::DrawCircle(dagger.Position, 150.0f, 0xFF7FFF00u, 1.5f, 64);
        }
    }

    if (Bool(DrawMenu, "DrawQRange")) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFADFF2Fu, 1.5f, 64);
    }

    if (Game::Ping() >= 100) {
        Vector2 screen;
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char text[48] = {};
            _snprintf_s(text, sizeof(text), _TRUNCATE, "High Ping %d", Game::Ping());
            Drawing::DrawText(screen.x - 20.0f, screen.y + 20.0f, 0xFFFFFF00u, text);
        }
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.katarina", "Kuro - Katarina", true);
    MenuRoot->Add(new MenuList(
        "KataComboMode",
        "Combo Mode",
        { "E first then Q", "Q first then E", "Logic Swap Combo" },
        0))->Permashow();
    MenuRoot->Add(new MenuKeyBind("Turret", "Combo under Turret", SDK::Keys::T, KeyBindType::Toggle))->Permashow();

    QMenu = MenuRoot->AddSubMenu(new Menu("Qstg", "Q Settings"));
    QMenu->Add(new MenuBool("FindBestTarget", "Find Best Target"));
    QMenu->Add(new MenuBool("AutoQ", "Auto Q"));
    QMenu->Add(new MenuBool("useQKS", "Use Q KS"));

    WMenu = MenuRoot->AddSubMenu(new Menu("Wstg", "W Settings"));
    WMenu->Add(new MenuSlider("WRange", "W Range", 300, 200, 340));
    WMenu->Add(new MenuBool("WGapcloser", "W Gapcloser"));
    WMenu->Add(new MenuBool("WOrb", "Orbwalker to Dagger"));

    EMenu = MenuRoot->AddSubMenu(new Menu("Estg", "E Settings"));
    EMenu->Add(new MenuBool("UseELogic", "Use E Logic", false));
    EMenu->Add(new MenuBool("EKs", "Use E KS"));
    EMenu->Add(new MenuKeyBind("SaveEIfNoDaggers", "Save E", SDK::Keys::H, KeyBindType::Toggle, true))->Permashow();

    RMenu = MenuRoot->AddSubMenu(new Menu("Rstg", "R Settings"));
    RMenu->Add(new MenuKeyBind("RCombo", "R Combo toggle Key", SDK::Keys::A, KeyBindType::Toggle))->Permashow();
    RMenu->Add(new MenuKeyBind("NeverCancelR", "Never Canceling R", SDK::Keys::Z, KeyBindType::Toggle))->Permashow();
    RMenu->Add(new MenuBool("UseRIfKs", "Accept R combo if target Can kill"));
    RMenu->Add(new MenuSlider("RCount", "R Target in range", 3, 1, 5));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Drawstg", "Draw Settings"));
    DrawMenu->Add(new MenuBool("DrawDaggers", "Draw Daggers"));
    DrawMenu->Add(new MenuBool("DrawQRange", "Draw Q Range"));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }

    if (auto* item = MenuRoot->Get<MenuList>("KataComboMode")) {
        item->RemovePermashow();
    }
    if (auto* item = MenuRoot->Get<MenuKeyBind>("Turret")) {
        item->RemovePermashow();
    }
    if (auto* item = EMenu ? EMenu->Get<MenuKeyBind>("SaveEIfNoDaggers") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = RMenu ? RMenu->Get<MenuKeyBind>("RCombo") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = RMenu ? RMenu->Get<MenuKeyBind>("NeverCancelR") : nullptr) {
        item->RemovePermashow();
    }

    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    QMenu = nullptr;
    WMenu = nullptr;
    EMenu = nullptr;
    RMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 625.0f);
    Q.SetTargetted(0.25f, 2000.0f);
    W = Spell(SpellSlot::W, 340.0f);
    E = Spell(SpellSlot::E, 775.0f);
    R = Spell(SpellSlot::R, 550.0f);

    BuildMenu();

    LastCastE = 0;
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnDoCast += &OnDoCast;
    GameObjects::AddOnCreate(&OnObjectCreate);
    GameObjects::AddOnDelete(&OnObjectDelete);
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Katarina loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnDoCast -= &OnDoCast;
    GameObjects::RemoveOnCreate(&OnObjectCreate);
    GameObjects::RemoveOnDelete(&OnObjectDelete);
    Drawing::OnDraw -= &OnDraw;
    Orbwalker::AttackEnabled(true);
    Orbwalker::MoveEnabled(true);
    Orbwalker::SetOrbwalkerPosition({});

    Daggers.clear();
    LastCastE = 0;
    RemoveMenu();
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Katarina
