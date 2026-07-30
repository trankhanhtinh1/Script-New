#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AINaafiriGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Naafiri {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::ObjectEventIsAllied;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

struct PackmateRecord {
    int NetworkId = 0;
    Vector3 Position = {};
    float HealthPercent = 100.0f;
    int LastSeenTick = 0;
    bool Confirmed = false;
};

struct BleedRecord {
    int TargetId = 0;
    int AppliedTick = 0;
    int ExpireTick = 0;
    bool Confirmed = false;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* PackMenu = nullptr;
inline Menu* DaggerMenu = nullptr;
inline Menu* PursuitMenu = nullptr;
inline Menu* DashMenu = nullptr;
inline Menu* HuntMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<PackmateRecord, 8> Packmates = {};
inline std::array<BleedRecord, 12> Bleeds = {};
inline int ObservedPackmates = 0;
inline int PackSpawnDueTick = 0;
inline int PackResetUntil = 0;
inline int QFirstCastTick = 0;
inline int QRecastExpireTick = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int WTargetId = 0;
inline int WChannelUntil = 0;
inline int WTakedownUntil = 0;
inline int WRecastExpireTick = 0;
inline int WShieldUntil = 0;
inline int VisionUntil = 0;
inline int RExpireTick = 0;
inline int RUntargetableUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int IncomingTargetedUntil = 0;
inline int PlayerOverrideUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int FocusTargetId = 0;
inline int FocusUntil = 0;
inline bool QRecastActive = false;
inline bool WRecastActive = false;
inline bool WChannelActive = false;
inline bool RActive = false;
inline bool VisionActive = false;
inline bool QWasManual = false;
inline bool WWasManual = false;
inline bool EWasManual = false;
inline bool RWasManual = false;
inline Vector3 LastQAim = {};
inline Vector3 LastWPathEnd = {};
inline Vector3 LastEEndpoint = {};

inline int Now() { return SDK::Variables::TickCount(); }

inline bool Ready(int index, Mode mode) {
    return index >= 0 && index < 4 && Engine::RuntimeSpells[index] &&
           Engine::RuntimeSpells[index]->IsReady() && SpellEnabled(index, mode);
}

inline bool Throttle(int index, int delayMs) {
    const int tick = index == 0 ? QCastTick : index == 1 ? WCastTick :
                     index == 2 ? ECastTick : RCastTick;
    return Now() - tick >= delayMs;
}

inline bool CannotDamage(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           ControllerHelpers::HasAnyBuff(target, {
               "SivirE", "NocturneShroudofDarkness", "MorganaE",
               "BlackShield", "BansheesVeil", "EdgeOfNight",
               "VladimirSanguinePool", "FizzEIcon", "KayleR",
               "kindredrnodeathbuff", "zhonyasringshield",
           });
}

inline bool IsPackmateName(const char* name, const char* characterName) {
    return ControllerHelpers::AnyTextContains(
        { name, characterName },
        { "naafiripackmate", "naafirihound", "naafiripet" });
}

inline PackmateRecord* FindPackmate(int networkId, bool create = false) {
    if (networkId == 0) return nullptr;
    for (auto& record : Packmates) {
        if (record.NetworkId == networkId) return &record;
    }
    if (!create) return nullptr;
    for (auto& record : Packmates) {
        if (record.NetworkId == 0 || record.LastSeenTick + 1200 < Now()) {
            record = {};
            record.NetworkId = networkId;
            return &record;
        }
    }
    return nullptr;
}

inline void ObservePackmate(int networkId,
                            const Vector3& position,
                            float healthPercent = 100.0f) {
    PackmateRecord* record = FindPackmate(networkId, true);
    if (!record) return;
    record->Position = position;
    record->HealthPercent = std::clamp(healthPercent, 0.0f, 100.0f);
    record->LastSeenTick = Now();
    record->Confirmed = true;
}

inline void ScanPackmates() {
    const int now = Now();
    for (auto& record : Packmates) {
        if (record.NetworkId != 0 && record.LastSeenTick + 900 < now) record = {};
    }
    for (const auto& minion : GameObjects::AllyMinions()) {
        if (!minion.IsValid() || minion.IsDead() ||
            !IsPackmateName(minion.Name().c_str(), minion.CharacterName().c_str())) {
            continue;
        }
        ObservePackmate(static_cast<int>(minion.NetworkId()), minion.Position(),
                        minion.HealthPercent());
    }
    ObservedPackmates = 0;
    for (const auto& record : Packmates) {
        if (record.NetworkId != 0 && record.Confirmed &&
            record.LastSeenTick + 900 >= now) ++ObservedPackmates;
    }
}

inline BleedRecord* FindBleed(int targetId, bool create = false) {
    if (targetId == 0) return nullptr;
    for (auto& bleed : Bleeds) {
        if (bleed.TargetId == targetId) return &bleed;
    }
    if (!create) return nullptr;
    for (auto& bleed : Bleeds) {
        if (bleed.TargetId == 0 || bleed.ExpireTick < Now()) {
            bleed = {};
            bleed.TargetId = targetId;
            return &bleed;
        }
    }
    return nullptr;
}

inline void ObserveBleed(int targetId, int durationMs) {
    BleedRecord* bleed = FindBleed(targetId, true);
    if (!bleed) return;
    bleed->AppliedTick = Now();
    bleed->ExpireTick = Now() + std::clamp(durationMs, 200, 5600);
    bleed->Confirmed = true;
    FocusTargetId = targetId;
    FocusUntil = std::max(FocusUntil, bleed->ExpireTick);
}

inline bool IsBleeding(int targetId) {
    const BleedRecord* bleed = FindBleed(targetId);
    return bleed && bleed->Confirmed && bleed->ExpireTick > Now();
}

inline int BleedRemainingMs(int targetId) {
    const BleedRecord* bleed = FindBleed(targetId);
    return bleed && bleed->Confirmed ? std::max(0, bleed->ExpireTick - Now()) : 0;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    ScanPackmates();
    for (auto& bleed : Bleeds) {
        if (bleed.TargetId != 0 && bleed.ExpireTick <= now) bleed = {};
    }
    QRecastActive = (QFirstCastTick > 0 && now <= QRecastExpireTick &&
                     (ControllerHelpers::RuntimeNameContains(0, "Recast") ||
                      player.HasBuff("NaafiriQRecast"))) ||
                    QRecastAvailable(QFirstCastTick, now);
    if (QFirstCastTick > 0 && now > QRecastExpireTick) {
        QFirstCastTick = QRecastExpireTick = 0;
        QRecastActive = false;
    }
    WRecastActive = now <= WRecastExpireTick &&
        (ControllerHelpers::RuntimeNameContains(1, "Recast") ||
         player.HasBuff("NaafiriWRecast") ||
         player.HasBuff("NaafiriW2"));
    WChannelActive = (WChannelActive && now <= WChannelUntil) ||
                     player.HasBuff("NaafiriWChannel") ||
                     player.HasBuff("NaafiriWDash");
    if (now > WChannelUntil) WChannelActive = false;
    RActive = now <= RExpireTick || player.HasBuff("NaafiriR") ||
              player.HasBuff("NaafiriRMoveSpeed");
    VisionActive = now <= VisionUntil || player.HasBuff("NaafiriRVision");
    if (!RActive && now > RExpireTick) RExpireTick = RUntargetableUntil = 0;
    if (!VisionActive && now > VisionUntil) VisionUntil = 0;
    if (FocusUntil < now) FocusTargetId = FocusUntil = 0;
    if (PackSpawnDueTick == 0 && ObservedPackmates < PackmateCap(player.Level())) {
        PackSpawnDueTick = now + static_cast<int>(
            PackmateSpawnCooldownSeconds(player.Level()) * 1000.0f);
    }
    if (ObservedPackmates >= PackmateCap(player.Level())) PackSpawnDueTick = 0;
}

inline float BonusAD() {
    const auto player = GameObjects::Player();
    return player.IsValid() ? player.BonusAttackDamage() : 0.0f;
}

inline float QDamage(const AIHeroClient& target, bool recast) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    float raw = 0.0f;
    if (recast && IsBleeding(static_cast<int>(target.NetworkId()))) {
        const float remaining = QBleedRemaining(
            QFullBleedRawDamage(SpellRank(0), BonusAD()),
            BleedRemainingMs(static_cast<int>(target.NetworkId())));
        raw = QSecondRawDamage(
            SpellRank(0), BonusAD(),
            1.0f - target.Health() / std::max(1.0f, target.MaxHealth()),
            remaining);
    } else {
        raw = QFirstRawDamage(SpellRank(0), BonusAD()) +
              QFullBleedRawDamage(SpellRank(0), BonusAD());
    }
    return player.CalculatePhysicalDamage(target, raw);
}

inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(
              target, WRawDamage(SpellRank(1), BonusAD(), ObservedPackmates))
        : 0.0f;
}

inline float EDamage(const AIHeroClient& target, const EHitResult& hit) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(
              target, ERawDamage(SpellRank(2), BonusAD(), hit))
        : 0.0f;
}

inline bool Lethal(const AIHeroClient& target, float damage) {
    return Engine::ValidEnemy(target) && damage >= target.Health() + target.AllShield();
}

inline std::vector<LineBody> QPathBodies(const AIHeroClient& target,
                                         const Vector3& targetPrediction) {
    std::vector<LineBody> bodies;
    bodies.reserve(72);
    const int targetId = static_cast<int>(target.NetworkId());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kQRange + 120.0f)) continue;
        const int id = static_cast<int>(enemy.NetworkId());
        bodies.push_back({ id == targetId ? targetPrediction : PredictPosition(enemy, 0.38f),
                           enemy.BoundingRadius(), id, true, true, false,
                           id == targetId });
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (minion.IsValid() && !minion.IsDead() && minion.IsTargetable()) {
            bodies.push_back({ minion.Position(), minion.BoundingRadius(),
                               static_cast<int>(minion.NetworkId()), true,
                               false, false, false });
        }
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() && monster.IsTargetable()) {
            bodies.push_back({ monster.Position(), monster.BoundingRadius(),
                               static_cast<int>(monster.NetworkId()), true,
                               false, true, false });
        }
    }
    return bodies;
}

inline bool CastQ(const AIHeroClient& target,
                  Mode mode,
                  bool force = false) {
    const bool recast = QRecastActive && QRecastAvailable(QFirstCastTick, Now());
    if (!Engine::ValidEnemy(target, kQRange + 60.0f) || !Ready(0, mode) ||
        !Throttle(0, 55) || CannotDamage(target)) return false;
    const auto player = GameObjects::Player();
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero()) {
        const float delay = kQCastDelay +
            player.Position().Distance2D(target.Position()) / kQSpeed;
        aim = PredictPosition(target, delay);
    }
    const Vector3 direction = SharedGeometry::Direction2D(player.Position(), aim);
    if (direction.IsZero() ||
        player.Position().Distance2D(aim) > kQRange + target.BoundingRadius() ||
        ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    if (!force && prediction.Hitchance < SDK::HitChance::High &&
        !Engine::IsHardCrowdControlled(target) && !target.IsDashing()) return false;
    const Vector3 end = player.Position() + direction * kQRange;
    const QPathResult path = EvaluateQPath(
        player.Position(), end, QPathBodies(target, aim),
        static_cast<int>(target.NetworkId()));
    if (!path.IntendedTargetHit) return false;
    if (!force && Orbwalker::IsWindingUp() && !Lethal(target, QDamage(target, recast))) {
        return false;
    }
    if (recast && !force && !IsBleeding(static_cast<int>(target.NetworkId())) &&
        Now() < QRecastExpireTick - Slider(DaggerMenu, "RecastPanicMs", 520)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    QCastTick = Now();
    LastQAim = aim;
    QWasManual = false;
    FocusTargetId = static_cast<int>(target.NetworkId());
    FocusUntil = Now() + 4300;
    if (recast) {
        QRecastActive = false;
        QFirstCastTick = QRecastExpireTick = 0;
    } else {
        QFirstCastTick = Now();
        QRecastExpireTick = Now() + kQRecastWindowMs;
    }
    return true;
}

inline std::vector<LineBody> PursuitChampions(const AIHeroClient& target,
                                              const Vector3& prediction) {
    std::vector<LineBody> bodies;
    bodies.reserve(GameObjects::EnemyHeroes().size());
    const int targetId = static_cast<int>(target.NetworkId());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kWRange + 180.0f)) continue;
        const int id = static_cast<int>(enemy.NetworkId());
        bodies.push_back({ id == targetId ? prediction : PredictPosition(enemy, 0.90f),
                           enemy.BoundingRadius(), id, true, true, false,
                           id == targetId });
    }
    return bodies;
}

inline bool CastW(const AIHeroClient& target,
                  Mode mode,
                  bool defensive = false) {
    if (!Engine::ValidEnemy(target, kWRange + 25.0f) || !target.IsVisible() ||
        !Ready(1, mode) || !Throttle(1, 120) || CannotDamage(target)) return false;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const float arrival = WArrivalSeconds(distance);
    const Vector3 predicted = PredictPosition(target, arrival);
    if (!predicted.IsValid() || predicted.IsZero() ||
        player.Position().Distance2D(predicted) > kWRange + target.BoundingRadius()) {
        return false;
    }
    const PursuitPathResult path = EvaluatePursuitPath(
        player.Position(), predicted, PursuitChampions(target, predicted),
        static_cast<int>(target.NetworkId()));
    const bool lethal = Lethal(target, WDamage(target));
    PursuitContext context{};
    context.Ready = true;
    context.TargetValid = true;
    context.TargetVisible = target.IsVisible();
    context.TargetSpellShielded = ControllerHelpers::HasSpellShieldOrImmunity(target);
    context.PathReachesTarget = path.ReachesTarget;
    context.ChannelInterruptThreat = IncomingHardCCUntil > Now() && !defensive;
    context.LandingWalkable = !SDK::NavMesh::IsWall(predicted);
    context.LandingUnderEnemyTurret = Engine::UnderEnemyTurret(predicted);
    context.StartedUnderEnemyTurret = Engine::UnderEnemyTurret(player.Position());
    context.Lethal = lethal;
    context.HasExit = Ready(2, mode) ||
                      Engine::CountAlliesAt(predicted, 700.0f) > 0;
    context.EnemiesAtLanding = Engine::CountEnemiesAt(predicted, 600.0f);
    context.MaximumEnemies = Slider(PursuitMenu, "MaxLandingEnemies", 2);
    if (!PursuitSafe(context)) return false;
    if (!WRecastActive && !defensive &&
        distance < Slider(PursuitMenu, "MinimumDistance", 360) && !lethal) {
        return false;
    }
    if (!Engine::ControllerCastUnit(1, target)) return false;
    WCastTick = Now();
    WTargetId = static_cast<int>(target.NetworkId());
    WChannelActive = true;
    WChannelUntil = Now() + static_cast<int>(arrival * 1000.0f) + 180;
    LastWPathEnd = predicted;
    WWasManual = false;
    FocusTargetId = WTargetId;
    FocusUntil = WChannelUntil + 1800;
    if (WRecastActive) {
        WRecastActive = false;
        WRecastExpireTick = 0;
        WShieldUntil = Now() + kWShieldDurationMs;
    } else {
        WTakedownUntil = Now() + kWTakedownWindowMs;
    }
    return true;
}

inline bool CastE(const AIHeroClient& target,
                  Mode mode,
                  bool fleeing = false) {
    if (!Ready(2, mode) || !Throttle(2, 75)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    Vector3 requested = Game::CursorPos();
    if (!fleeing && Engine::ValidEnemy(target, kEVirtualRange + kESecondRadius)) {
        requested = PredictPosition(target, 0.28f);
    }
    const Vector3 endpoint = EEndpoint(player.Position(), requested);
    if (!endpoint.IsValid() || endpoint.IsZero()) return false;
    const EHitResult hit = Engine::ValidEnemy(target)
        ? EvaluateEHit(player.Position(), endpoint, PredictPosition(target, 0.28f),
                       target.BoundingRadius())
        : EHitResult{};
    const bool lethal = Engine::ValidEnemy(target) &&
                        Lethal(target, EDamage(target, hit));
    DashContext context{};
    context.EndpointValid = true;
    context.EndpointWalkable = !SDK::NavMesh::IsWall(endpoint);
    context.EndpointUnderEnemyTurret = Engine::UnderEnemyTurret(endpoint);
    context.StartingUnderEnemyTurret = Engine::UnderEnemyTurret(player.Position());
    context.PointClickThreat = IncomingTargetedUntil > Now();
    context.DashHazard = IncomingHardCCUntil > Now();
    context.Lethal = lethal;
    context.Fleeing = fleeing;
    context.NearbyEnemies = Engine::CountEnemiesAt(endpoint, 525.0f);
    context.MaximumEnemies = Slider(DashMenu, "MaxDashEnemies", 2);
    if (!DashSafe(context)) return false;
    if (!fleeing && !hit.SecondSlash && !lethal) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    ECastTick = Now();
    LastEEndpoint = endpoint;
    PackResetUntil = Now() + 650;
    EWasManual = false;
    return true;
}

inline bool CastR(Mode mode,
                  const AIHeroClient& target,
                  bool fleeing = false) {
    if (!Ready(3, mode) || !Throttle(3, 100)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int cap = PackmateCap(player.Level());
    RContext context{};
    context.Ready = true;
    context.AlreadyActive = RActive;
    context.IncomingTargetedThreat = IncomingTargetedUntil > Now();
    context.NeedsPackReset = PackResetUntil > Now();
    context.NeedsPackmates = ObservedPackmates < cap;
    context.NeedsChase = Engine::ValidEnemy(target) &&
        player.Position().Distance2D(target.Position()) > 620.0f;
    context.AllIn = mode == Mode::Combo;
    context.Fleeing = fleeing;
    context.LethalFollowup = Engine::ValidEnemy(target) &&
        QDamage(target, QRecastActive) + WDamage(target) >=
            target.Health() + target.AllShield();
    context.CurrentPackmates = ObservedPackmates;
    context.OrdinaryPackCap = cap;
    if (!ShouldCastR(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RCastTick = Now();
    RActive = true;
    RExpireTick = Now() + kRDurationMs;
    RUntargetableUntil = Now() + kRUntargetableMs;
    ObservedPackmates = PackmatesAfterR(ObservedPackmates, player.Level());
    RWasManual = false;
    return true;
}

inline AIHeroClient CooperatingTarget(const AIHeroClient& selected) {
    if (Engine::ValidEnemy(selected)) return selected;
    if (WChannelActive && WTargetId != 0) {
        const AIHeroClient target = HeroByNetworkId(WTargetId);
        if (Engine::ValidEnemy(target, kWRange + 350.0f)) return target;
    }
    if (FocusTargetId != 0 && FocusUntil >= Now()) {
        const AIHeroClient target = HeroByNetworkId(FocusTargetId);
        if (Engine::ValidEnemy(target, kQRange + 80.0f)) return target;
    }
    if (QRecastActive) {
        AIHeroClient best{};
        float bestHealth = FLT_MAX;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (Engine::ValidEnemy(enemy, kQRange + 50.0f) &&
                IsBleeding(static_cast<int>(enemy.NetworkId())) &&
                enemy.Health() < bestHealth) {
                best = enemy;
                bestHealth = enemy.Health();
            }
        }
        if (best.IsValid()) return best;
    }
    return Engine::SelectTarget(kQRange + 80.0f);
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, QDamage(target, QRecastActive)) &&
        CastQ(target, mode, true)) return true;
    const EHitResult full{ true, true, true };
    if (Lethal(target, EDamage(target, full)) && CastE(target, mode)) return true;
    if (Lethal(target, WDamage(target)) && CastW(target, mode)) return true;
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    if (QRecastActive && IsBleeding(static_cast<int>(target.NetworkId())) &&
        (BleedRemainingMs(static_cast<int>(target.NetworkId())) <=
             Slider(DaggerMenu, "BleedCashoutMs", 2800) ||
         target.HealthPercent() <= Slider(DaggerMenu, "BleedCashoutHP", 48)) &&
        CastQ(target, Mode::Combo)) return true;
    if (!RActive && (ObservedPackmates < PackmateCap(player.Level()) ||
                     distance >= 680.0f) && CastR(Mode::Combo, target)) return true;
    if (distance >= Slider(PursuitMenu, "MinimumDistance", 360) &&
        CastW(target, Mode::Combo)) return true;
    if (!QRecastActive && CastQ(target, Mode::Combo)) return true;
    if (distance <= kEVirtualRange + kESecondRadius &&
        CastE(target, Mode::Combo)) return true;
    if (QRecastActive && CastQ(target, Mode::Combo)) return true;
    if (WRecastActive && CastW(target, Mode::Combo)) return true;
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) ||
        ControllerHelpers::PlayerManaPercent() < Slider(DaggerMenu, "HarassMana", 48)) {
        return false;
    }
    if (QRecastActive && IsBleeding(static_cast<int>(target.NetworkId())) &&
        CastQ(target, Mode::Harass)) return true;
    if (!QRecastActive && CastQ(target, Mode::Harass)) return true;
    return Bool(DashMenu, "HarassE", false) &&
           GameObjects::Player().HealthPercent() >= 70.0f &&
           CastE(target, Mode::Harass);
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (IncomingTargetedUntil > Now() && CastR(Mode::Flee, threat, true)) return true;
    if (CastE(threat, Mode::Flee, true)) return true;
    if (!RActive && CastR(Mode::Flee, threat, true)) return true;
    return Engine::ValidEnemy(threat) && CastQ(threat, Mode::Flee, true);
}

inline bool TryFarm(Mode mode) {
    if (CurrentResource() < SpellCost(0) + Slider(FarmMenu, "ManaReserve", 70)) {
        return false;
    }
    // The owned loop deliberately exposes only Q to the shared farm selector.
    // W/R are champion-only commits and E is retained as the pack reset/escape.
    return Engine::TryFarm(mode);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (PlayerOverrideUntil > Now() || WChannelActive) return true;
    const AIHeroClient target = CooperatingTarget(selected);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 1100.0f);
    if (mode == Mode::Flee) {
        (void)TryFlee(threat);
        return true;
    }
    if (IncomingTargetedUntil > Now() &&
        GameObjects::Player().HealthPercent() <= Slider(HuntMenu, "DefensiveRHP", 38) &&
        CastR(mode == Mode::None ? Mode::Automatic : mode, target, true)) return true;
    if (TryKillSecure(target, mode == Mode::None ? Mode::Automatic : mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && IncomingHardCCUntil > Now()) {
            (void)CastQ(target, Mode::Automatic, true);
        }
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(
            args, 230.0f, 120.0f, 260, 300, 280, 1600, 500);
        const bool targetsPlayer = args.TargetNetworkId ==
            static_cast<std::uint32_t>(player.NetworkId());
        if (targetsPlayer) IncomingTargetedUntil = now + 850;
        if (threat.Valid && threat.CrossesPlayer && threat.LikelyHardCrowdControl) {
            IncomingHardCCUntil = now + 700;
        }
        return;
    }
    const int slot = args.Slot;
    const bool owned = slot >= 0 && slot < 4 && Engine::WasControllerCast(slot);
    if (!owned && slot >= 0 && slot < 4) {
        PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
    }
    if (slot == 0) {
        QCastTick = now;
        QWasManual = !owned;
        const bool recast = QRecastActive ||
            Engine::TextContains(args.SpellName, "Recast");
        if (recast) {
            QRecastActive = false;
            QFirstCastTick = QRecastExpireTick = 0;
        } else {
            QFirstCastTick = now;
            QRecastExpireTick = now + kQRecastWindowMs;
        }
    } else if (slot == 1) {
        WCastTick = now;
        WWasManual = !owned;
        WTargetId = static_cast<int>(args.TargetNetworkId != 0
            ? args.TargetNetworkId : args.Target.NetworkId);
        WChannelActive = true;
        WChannelUntil = now + 1500;
        FocusTargetId = WTargetId;
        FocusUntil = now + 3000;
    } else if (slot == 2) {
        ECastTick = now;
        EWasManual = !owned;
        PackResetUntil = now + 650;
        PackSpawnDueTick = 0;
    } else if (slot == 3) {
        RCastTick = now;
        RWasManual = !owned;
        RActive = true;
        RExpireTick = now + kRDurationMs;
        RUntargetableUntil = now + kRUntargetableMs;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (ControllerHelpers::TextContainsAny(args.BuffName,
                { "NaafiriQRecast" })) {
            QRecastActive = true;
            if (QFirstCastTick == 0) QFirstCastTick = now;
            QRecastExpireTick = std::max(QRecastExpireTick, now + 600);
        }
        if (ControllerHelpers::TextContainsAny(args.BuffName,
                { "NaafiriWChannel", "NaafiriWDash" })) {
            WChannelActive = true;
            WChannelUntil = now + ControllerHelpers::RemainingMilliseconds(
                args.EndTime, 1500, 100, 2400);
        }
        if (ControllerHelpers::TextContainsAny(args.BuffName,
                { "NaafiriWRecast", "NaafiriW2" })) {
            WRecastActive = true;
            WRecastExpireTick = now + ControllerHelpers::RemainingMilliseconds(
                args.EndTime, kWRecastWindowMs, 400, kWRecastWindowMs);
        }
        if (ControllerHelpers::TextContainsAny(args.BuffName,
                { "NaafiriR", "NaafiriRMoveSpeed" })) {
            RActive = true;
            RExpireTick = now + ControllerHelpers::RemainingMilliseconds(
                args.EndTime, kRDurationMs, 250, 6500);
        }
        if (Engine::TextContains(args.BuffName, "NaafiriRVision")) {
            VisionActive = true;
            VisionUntil = now + ControllerHelpers::RemainingMilliseconds(
                args.EndTime, kWVisionDurationMs, 250, 5200);
            WRecastActive = true;
            WRecastExpireTick = std::max(WRecastExpireTick,
                                         now + kWRecastWindowMs);
        }
        if (Engine::TextContains(args.BuffName, "NaafiriRShield")) {
            WShieldUntil = now + ControllerHelpers::RemainingMilliseconds(
                args.EndTime, kWShieldDurationMs, 200, 4200);
        }
        return;
    }
    if (Engine::TextContains(args.BuffName, "NaafiriQBleed")) {
        ObserveBleed(static_cast<int>(args.Sender.NetworkId),
                     ControllerHelpers::RemainingMilliseconds(
                         args.EndTime, kQBleedDurationMs, 300, 5600));
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "NaafiriQRecast")) {
            QRecastActive = false;
        }
        if (ControllerHelpers::TextContainsAny(args.BuffName,
                { "NaafiriWChannel", "NaafiriWDash" })) WChannelActive = false;
        if (ControllerHelpers::TextContainsAny(args.BuffName,
                { "NaafiriWRecast", "NaafiriW2" })) WRecastActive = false;
        if (ControllerHelpers::TextContainsAny(args.BuffName,
                { "NaafiriR", "NaafiriRMoveSpeed" })) RActive = false;
        if (Engine::TextContains(args.BuffName, "NaafiriRVision")) {
            VisionActive = false;
        }
        return;
    }
    if (Engine::TextContains(args.BuffName, "NaafiriQBleed")) {
        if (BleedRecord* bleed = FindBleed(
                static_cast<int>(args.Sender.NetworkId))) *bleed = {};
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime > Game::Time()) OnBuffAdd(args);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !ObjectEventIsAllied(args) ||
        !IsPackmateName(args.Sender.Name, args.Sender.CharacterName)) return;
    ObservePackmate(static_cast<int>(args.Sender.NetworkId), args.Sender.Position);
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (PackmateRecord* record = FindPackmate(id)) {
        *record = {};
        PackSpawnDueTick = Now() + static_cast<int>(
            PackmateSpawnCooldownSeconds(GameObjects::Player().Level()) * 1000.0f);
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || WChannelActive) return;
    if (QRecastActive && QRecastExpireTick - Now() < 250 &&
        static_cast<int>(args.Target.NetworkId()) == FocusTargetId) {
        args.Process = false;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawState", false)) return;
    Drawing::DrawCircle(player.Position(), QRecastActive ? kQRange : kWRange,
                        QRecastActive ? 0xFFD94A55u : 0xFF7D2436u, 1.7f, 48);
    if (!LastQAim.IsZero()) Drawing::DrawLine(player.Position(), LastQAim,
                                              0xFFD94A55u, 2.0f);
    if (!LastWPathEnd.IsZero()) Drawing::DrawLine(player.Position(), LastWPathEnd,
                                                  0xFFFFB347u, 2.4f);
    if (!LastEEndpoint.IsZero()) Drawing::DrawCircle(LastEEndpoint, kESecondRadius,
                                                      0xFFB52A52u, 1.8f, 40);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "NaafiriOneTrick", "Naafiri pack mechanics"));
    TacticsMenu->Add(new MenuSlider(
        "ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    PackMenu = TacticsMenu->AddSubMenu(new Menu("Pack", "Packmate state"));
    PackMenu->Add(new MenuBool("TrackPack", "Track live packmate objects", true));
    DaggerMenu = TacticsMenu->AddSubMenu(new Menu("Daggers", "Bleed and recast"));
    DaggerMenu->Add(new MenuSlider("HarassMana", "Harass minimum mana percent", 48, 0, 100));
    DaggerMenu->Add(new MenuSlider("BleedCashoutMs", "Cash bleed with this time left", 2800, 500, 4800));
    DaggerMenu->Add(new MenuSlider("BleedCashoutHP", "Cash bleed below target HP", 48, 10, 100));
    DaggerMenu->Add(new MenuSlider("RecastPanicMs", "Unmarked recast expiry guard", 520, 100, 1200));
    PursuitMenu = TacticsMenu->AddSubMenu(new Menu("Pursuit", "Target dash path"));
    PursuitMenu->Add(new MenuSlider("MinimumDistance", "Minimum nonlethal W distance", 360, 0, 800));
    PursuitMenu->Add(new MenuSlider("MaxLandingEnemies", "Maximum W landing enemies", 2, 1, 5));
    DashMenu = TacticsMenu->AddSubMenu(new Menu("Eviscerate", "Dash and pack reset"));
    DashMenu->Add(new MenuSlider("MaxDashEnemies", "Maximum E endpoint enemies", 2, 1, 5));
    DashMenu->Add(new MenuBool("HarassE", "Allow E in harass", false));
    HuntMenu = TacticsMenu->AddSubMenu(new Menu("Call", "Pack empowerment"));
    HuntMenu->Add(new MenuSlider("DefensiveRHP", "Defensive R health percent", 38, 10, 80));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Conservative farm"));
    FarmMenu->Add(new MenuSlider("ManaReserve", "Mana reserve", 70, 0, 250));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "State visualization"));
    CoachMenu->Add(new MenuBool("DrawState", "Draw routes and recast range", false));
}

inline void OnLoad() {
    Packmates = {};
    Bleeds = {};
    ObservedPackmates = PackSpawnDueTick = PackResetUntil = 0;
    QFirstCastTick = QRecastExpireTick = QCastTick = 0;
    WCastTick = ECastTick = RCastTick = 0;
    WTargetId = WChannelUntil = WTakedownUntil = WRecastExpireTick = 0;
    WShieldUntil = VisionUntil = RExpireTick = RUntargetableUntil = 0;
    IncomingHardCCUntil = IncomingTargetedUntil = PlayerOverrideUntil = 0;
    LastAutoTargetId = LastAutoTick = FocusTargetId = FocusUntil = 0;
    QRecastActive = WRecastActive = WChannelActive = false;
    RActive = VisionActive = false;
    QWasManual = WWasManual = EWasManual = RWasManual = false;
    LastQAim = LastWPathEnd = LastEEndpoint = {};
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = PackMenu = DaggerMenu = PursuitMenu = DashMenu = nullptr;
    HuntMenu = FarmMenu = CoachMenu = nullptr;
    Packmates = {};
    Bleeds = {};
    QRecastActive = WRecastActive = WChannelActive = RActive = VisionActive = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin Summoner's Rift behavior to Riot 26.15 and CommunityDragon PC 16.15",
    "Reject ARAM, Arena and stale pre-rework Naafiri modifiers",
    "Track the ordinary packmate cap at levels one, nine, twelve and fifteen",
    "Track packmate objects by create, delete and allied-minion polling",
    "Reconcile stale packmate events without inventing a living hound",
    "Track the 30/25/20/15/10 second packmate spawn breakpoints",
    "Reduce passive recovery by four seconds per qualifying ability hit",
    "Reduce passive recovery by one second per enemy kill",
    "Use current packmate level interpolation and four-percent bonus-AD ratio",
    "Apply frenzy, non-champion and area-damage packmate modifiers separately",
    "Track Q first cast, 750-ms lockout and four-second recast window",
    "Track Q bleed per target from add, update, remove and expiry",
    "Use ten half-second bleed ticks over five seconds",
    "Cash only the remaining bleed when Q hits a bleeding target",
    "Scale Q2 base from zero to one hundred percent with missing health",
    "Scale Q2 bonus AD from forty to one hundred forty percent with missing health",
    "Heal on a confirmed Q2 champion or large-monster bleed cashout",
    "Use the current 900 range, 150 width and 1700 Q missile speed",
    "Treat Q as piercing rather than minion-colliding",
    "Send packmates to the first champion or large monster on the Q line",
    "Reject Q through a projectile wall",
    "Require high prediction unless the target is committed or controlled",
    "Preserve an attack windup for a nonlethal dagger",
    "Cooperate with the selected target before bleed and pursuit focus",
    "Keep a bleeding focus target through the Q recast window",
    "Use Hounds' Pursuit as a champion-targeted 900-range channel dash",
    "Forecast W landing after channel time plus 1800-speed travel",
    "Sort champion bodies along the W path",
    "Reject W when another champion intercepts before the intended target",
    "Reject W into a spell shield or untargetable target",
    "Reject W when a known hard-CC window can interrupt a nonlethal channel",
    "Reject unsafe W landings under a new enemy turret",
    "Require an E exit, allied follow-up or lethal W landing",
    "Limit enemies at the W landing point",
    "Track W target and channel state through event and polling recovery",
    "Track the seven-second W takedown qualification window",
    "Track the twelve-second W recast window",
    "Track the W recast shield for three seconds",
    "Track the four-second reveal pulse through NaafiriRVision",
    "Use current W base damage, full bonus-AD ratio and ten-percent pack scaling",
    "Use current W shield and level-scaled armor shred facts",
    "Clamp E travel between 250 and 450 units",
    "Evaluate E's path slash separately from its 230-radius endpoint slash",
    "Require the endpoint slash for ordinary offensive E",
    "Allow lethal E when only the relevant slash connects",
    "Reject E endpoints in terrain",
    "Reject nonlethal E into a new turret or excessive enemies",
    "Use cursor-owned E direction while fleeing",
    "Recall, untarget and fully reset living packmates on E",
    "Track E reset intent through spell events when object callbacks lag",
    "Use R as the current five-rank self pack empowerment",
    "Track R's one-second untargetability window",
    "Track R's five-second movement-speed, AD and pack state",
    "Add two temporary packmates without exceeding the empowered cap",
    "Use R to prepare a distant all-in when the pack is deficient",
    "Use R defensively against a confirmed targeted threat",
    "Do not recast R while its pack state is already active",
    "Combo cashes an urgent bleed before starting a new route",
    "Combo prepares the pack before a long Pursuit",
    "Combo uses Q before close E when no recast is pending",
    "Combo preserves Q2 for a confirmed bleeding target",
    "Harass uses double Q without unsolicited W or R commitment",
    "Harass E remains opt-in and health-gated",
    "LaneClear exposes Q only and reserves E for reset or escape",
    "Jungle mode preserves a configurable mana reserve",
    "LastHit never spends W or R",
    "Flee uses targeted-threat R, then cursor E, then defensive Q",
    "Automatic mode never opens an unsolicited W engage",
    "Automatic mode may answer hard crowd control with a dagger",
    "Automatic mode may execute verified lethal spell damage",
    "Track local manual Q, W, E and R casts without duplicating them",
    "Yield the complete decision loop after manual spell ownership",
    "Never automate Flash, Smite, Ignite, items, movement or basic attacks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Naafiri";
    controller.ControllerId = "champion.kuroaio.ai.naafiri.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AINaafiri.md";
    controller.ImplementationSummary =
        "Live packmate census, per-target Q bleed/recast accounting, body-block-aware "
        "W pursuit, dual-hit E dash/reset safety and reconciled R pack/vision state.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &OnBuffUpdate;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Naafiri
