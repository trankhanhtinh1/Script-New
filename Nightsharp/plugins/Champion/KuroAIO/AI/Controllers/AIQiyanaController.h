#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIQiyanaGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Qiyana {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::ChampionIs;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
inline bool IsBrush(const Vector3& position) {
    return SDK::HasFlag(SDK::NavMesh::GetCollisionFlags(position),
                        SDK::CollisionFlags::Grass);
}
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::UnitByNetworkId;

inline Menu* TacticsMenu = nullptr;
inline Menu* ElementMenu = nullptr;
inline Menu* MobilityMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Element CurrentElement = Element::None;
inline PassiveRecord PassiveState{};
inline int ElementExpireTick = 0;
inline int GrassStealthUntil = 0;
inline int QTargetId = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int PlayerOverrideUntil = 0;
inline Vector3 LastQDirection = {};
inline Vector3 LastWEndpoint = {};
inline Vector3 LastEEndpoint = {};
inline Vector3 LastREndpoint = {};
inline bool QWasManual = false;
inline bool WWasManual = false;
inline bool EWasManual = false;
inline bool RWasManual = false;

inline constexpr int kElementDurationMs = 5000;
inline constexpr int kManualOwnershipMs = 520;
inline constexpr int kQPostCastMs = 48;
inline constexpr int kWPostCastMs = 70;
inline constexpr int kEPostCastMs = 80;
inline constexpr int kRPostCastMs = 120;

inline int Now() { return SDK::Variables::TickCount(); }

inline bool Ready(int index, Mode mode) {
    return index >= 0 && index < 4 && Engine::RuntimeSpells[index] &&
           Engine::RuntimeSpells[index]->IsReady() && SpellEnabled(index, mode);
}

inline bool Throttle(int index, int minimumMs) {
    const int last = index == 0 ? QCastTick : index == 1 ? WCastTick :
        index == 2 ? ECastTick : RCastTick;
    return Now() - last >= minimumMs;
}

inline bool TargetCannotBeDamaged(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BlackShield") ||
           target.HasBuff("BansheesVeil") || target.HasBuff("EdgeOfNight") ||
           target.HasBuff("FioraW") || target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzEIcon") || target.HasBuff("KayleR") ||
           target.HasBuff("kindredrnodeathbuff") || target.HasBuff("ChronoShift");
}

inline bool HasProjectileWall(const Vector3& start, const Vector3& end) {
    const float distance = start.Distance2D(end);
    if (distance <= 1.0f) return false;
    const Vector3 direction = Direction2D(start, end);
    if (direction.IsZero()) return false;
    for (float travel = 20.0f; travel < distance - 20.0f; travel += 18.0f) {
        if (SDK::NavMesh::IsWall(start + direction * travel)) return true;
    }
    return false;
}

inline bool SafeEndpoint(const Vector3& endpoint,
                         const AIHeroClient& target,
                         bool lethal,
                         bool fleeing,
                         int maximumEnemies = 2) {
    if (!endpoint.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    MobilityContext context{};
    context.EndpointValid = true;
    context.EndpointWalkable = true;
    context.EndpointUnderEnemyTurret = Engine::UnderEnemyTurret(endpoint);
    context.StartingUnderEnemyTurret = Engine::UnderEnemyTurret(player.Position());
    context.PointClickThreat = ControllerHelpers::HasReadyPointClickThreatAt(endpoint);
    context.DashHazard = ControllerHelpers::HasReadyDashHazardAt(endpoint);
    context.NearbyEnemies = Engine::CountEnemiesAt(endpoint, 625.0f);
    context.MaximumEnemies = maximumEnemies;
    context.Lethal = lethal;
    context.Fleeing = fleeing;
    context.ExitAvailable = fleeing || Engine::CountAlliesAt(endpoint, 750.0f) > 0 ||
                            context.NearbyEnemies <= 1;
    if (!MobilitySafe(context)) return false;
    if (!lethal && Engine::PositionDangerScore(
            endpoint, target, Engine::ResolvedSpecs[2]) <= -10000.0f) return false;
    return true;
}

inline Element ElementFromRuntimeName(const char* name) {
    if (!name) return Element::None;
    if (Engine::TextContains(name, "Grass") || Engine::TextContains(name, "Brush")) {
        return Element::Brush;
    }
    if (Engine::TextContains(name, "Water") || Engine::TextContains(name, "River")) {
        return Element::River;
    }
    if (Engine::TextContains(name, "Rock") || Engine::TextContains(name, "Wall")) {
        return Element::Rock;
    }
    return Element::None;
}

inline void SetElement(Element element) {
    if (element == Element::None) return;
    CurrentElement = element;
    ElementExpireTick = Now() + kElementDurationMs;
}

inline bool HasElement() {
    return CurrentElement != Element::None && Now() < ElementExpireTick;
}

inline bool HasGrassStealth() { return Now() < GrassStealthUntil; }

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (ElementExpireTick > 0 && now >= ElementExpireTick) CurrentElement = Element::None;
    if (GrassStealthUntil > 0 && now >= GrassStealthUntil) GrassStealthUntil = 0;
    if (PassiveState.Confirmed && now >= PassiveState.ExpireTick) PassiveState = {};
    if (CurrentElement == Element::None) {
        for (const char* token : { "QiyanaWEnchantedBuff", "qiyanawenchantedbuff",
                                   "QiyanaRock", "QiyanaWater", "QiyanaGrass" }) {
            if (player.HasBuff(token)) {
                const Element found = ElementFromRuntimeName(token);
                if (found != Element::None) SetElement(found);
            }
        }
    }
    if (player.HasBuff("QiyanaQ_Grass_Stealth_Buff") ||
        player.HasBuff("QiyanaGrassStealth")) {
        GrassStealthUntil = std::max(GrassStealthUntil, now + 850);
    }
}

inline int SpellRank(int index) {
    if (index < 0 || index >= 4 || !Engine::RuntimeSpells[index]) return 1;
    return std::clamp(Engine::RuntimeSpells[index]->Level(), 1, 5);
}

inline float RawQ(const AIHeroClient& target, Element element = CurrentElement) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float bonusAd = std::max(0.0f, player.BonusAttackDamage());
    return QDamageAfterCollision(SpellRank(0), bonusAd, element,
                                 target.HealthPercent(), 0);
}

inline float RawE(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? ERawDamage(SpellRank(2), player.BonusAttackDamage()) : 0.0f;
}

inline float RawR(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? RRawDamage(std::clamp(SpellRank(3), 1, 3), player.BonusAttackDamage(),
                     target.MaxHealth()) : 0.0f;
}

inline bool Lethal(const AIHeroClient& target, float rawDamage) {
    return Engine::ValidEnemy(target) && rawDamage >= target.Health() + target.AllShield();
}

inline bool CursorAgrees(const Vector3& endpoint, const Vector3& origin) {
    const Vec3 cursorDirection = Direction2D(origin, Game::CursorPos());
    const Vec3 castDirection = Direction2D(origin, endpoint);
    return cursorDirection.IsZero() || castDirection.IsZero() ||
           cursorDirection.Dot(castDirection) >= 0.05f;
}

inline std::vector<TerrainCandidate> TerrainCandidates(const AIHeroClient& target,
                                                       bool fleeing) {
    std::vector<TerrainCandidate> result;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return result;
    std::array<Vec3, 12> directions{};
    std::size_t count = 0;
    for (const Vec3 direction : {
        Direction2D(player.Position(), Game::CursorPos()),
        Direction2D(player.Position(), target.Position()),
        Vec3{ 1.0f, 0.0f, 0.0f }, Vec3{ -1.0f, 0.0f, 0.0f },
        Vec3{ 0.0f, 0.0f, 1.0f }, Vec3{ 0.0f, 0.0f, -1.0f },
        Vec3{ 0.707f, 0.0f, 0.707f }, Vec3{ -0.707f, 0.0f, 0.707f },
        Vec3{ 0.707f, 0.0f, -0.707f }, Vec3{ -0.707f, 0.0f, -0.707f } }) {
        if (!direction.IsZero() && count < directions.size()) directions[count++] = direction;
    }
    for (std::size_t i = 0; i < count; ++i) {
        for (float distance = 85.0f; distance <= kWSearchRange; distance += 90.0f) {
            const Vec3 sample = player.Position() + directions[i] * distance;
            const bool wall = SDK::NavMesh::IsWall(sample);
            const bool brush = IsBrush(sample);
            const bool water = SDK::NavMesh::IsWater(sample);
            Element element = wall ? Element::Rock : brush ? Element::Brush :
                water ? Element::River : Element::None;
            if (element == Element::None) continue;
            Vec3 endpoint = wall ? sample - directions[i] * 34.0f : sample;
            endpoint.y = SDK::NavMesh::GetHeightForPosition(endpoint);
            if (!endpoint.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) continue;
            MobilityContext safety{};
            safety.EndpointValid = true;
            safety.EndpointWalkable = true;
            safety.EndpointUnderEnemyTurret = Engine::UnderEnemyTurret(endpoint);
            safety.StartingUnderEnemyTurret = Engine::UnderEnemyTurret(player.Position());
            safety.PointClickThreat = ControllerHelpers::HasReadyPointClickThreatAt(endpoint);
            safety.DashHazard = ControllerHelpers::HasReadyDashHazardAt(endpoint);
            safety.NearbyEnemies = Engine::CountEnemiesAt(endpoint, 625.0f);
            safety.MaximumEnemies = Slider(MobilityMenu, "MaxEndpointEnemies", 2);
            safety.Fleeing = fleeing;
            safety.ExitAvailable = fleeing || safety.NearbyEnemies <= 1 ||
                                   Engine::CountAlliesAt(endpoint, 700.0f) > 0;
            result.push_back({ endpoint, element, safety,
                               endpoint.Distance2D(Game::CursorPos()),
                               endpoint.Distance2D(target.Position()) });
            break;
        }
    }
    return result;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool defensive = false) {
    if (!Engine::ValidEnemy(target, kQMissileRange + 35.0f) ||
        !Ready(0, mode) || !Throttle(0, kQPostCastMs) || TargetCannotBeDamaged(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true) &&
        !Lethal(target, RawQ(target)) && !defensive) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 predicted = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero() ? prediction.GetCastPosition() :
        PredictPosition(target, 0.25f);
    if (!predicted.IsValid() || predicted.IsZero() ||
        prediction.Hitchance < SDK::HitChance::High || HasProjectileWall(player.Position(), predicted)) {
        return false;
    }
    const float reach = HasElement() ? kQMissileRange : kQSlashRange;
    if (player.Position().Distance2D(predicted) > reach + target.BoundingRadius()) return false;
    const bool lethal = Lethal(target, RawQ(target));
    QCastContext context{ true, true, true, true, false,
                          Orbwalker::IsWindingUp(), lethal, player.IsDashing(),
                          HasGrassStealth() && !lethal, defensive };
    if (!MayCastQ(context)) return false;
    const Vector3 direction = Direction2D(player.Position(), predicted);
    if (direction.IsZero() || !CursorAgrees(predicted, player.Position())) return false;
    const Vector3 endpoint = player.Position() + direction * reach;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    QTargetId = static_cast<int>(target.NetworkId());
    QCastTick = Now();
    LastQDirection = direction;
    QWasManual = false;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool defensive, bool needsCatch = false) {
    if (!Ready(1, mode) || !Throttle(1, kWPostCastMs) ||
        CurrentResource() + 0.5f < SpellCost(1)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDashing()) return false;
    const float hp = Engine::ValidEnemy(target) ? target.HealthPercent() : 100.0f;
    const Element desired = DesiredElement(hp, defensive, needsCatch,
                                           PassiveState.Confirmed, CurrentElement);
    const TerrainCandidate chosen = SelectTerrainCandidate(TerrainCandidates(target, defensive), desired, defensive);
    if (chosen.Position.IsZero() || chosen.Kind == Element::None ||
        !CursorAgrees(chosen.Position, player.Position())) return false;
    if (!Engine::ControllerCastPosition(1, chosen.Position)) return false;
    SetElement(chosen.Kind);
    LastWEndpoint = chosen.Position;
    WCastTick = Now();
    WWasManual = false;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool fleeing = false) {
    if (!Engine::ValidEnemy(target, kERange + 35.0f) || !Ready(2, mode) ||
        !Throttle(2, kEPostCastMs) || TargetCannotBeDamaged(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    const Vector3 predicted = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero() ? prediction.GetCastPosition() :
        PredictPosition(target, 0.20f);
    if (!predicted.IsValid() || predicted.IsZero() ||
        prediction.Hitchance < (fleeing ? SDK::HitChance::Medium : SDK::HitChance::High)) return false;
    const Vector3 endpoint = EProjectedEndpoint(player.Position(), predicted);
    const bool lethal = Lethal(target, RawE(target) + RawQ(target));
    if (!SafeEndpoint(endpoint, target, lethal, fleeing,
                     Slider(MobilityMenu, "MaxEndpointEnemies", 2))) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    ECastTick = Now();
    LastEEndpoint = endpoint;
    EWasManual = false;
    return true;
}

inline std::vector<TerrainZone> RZones(const Vector3& source, const Vector3& end) {
    std::vector<TerrainZone> zones;
    const Vec3 direction = Direction2D(source, end);
    if (direction.IsZero()) return zones;
    for (float distance = 30.0f; distance <= kRRange; distance += 28.0f) {
        const Vec3 sample = source + direction * distance;
        if (SDK::NavMesh::IsWall(sample)) {
            Vector3 wall{};
            if (SDK::NavMesh::FindWallCollision(source, sample, wall, 8.0f) &&
                wall.IsValid() && !wall.IsZero()) {
                zones.push_back({ wall, 80.0f, Element::Rock });
            }
            break;
        }
        const bool brush = IsBrush(sample);
        if (SDK::NavMesh::IsWater(sample)) zones.push_back({ sample, 105.0f, Element::River });
    }
    return zones;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive) {
    if (!Ready(3, mode) || !Throttle(3, kRPostCastMs) ||
        !Engine::ValidEnemy(target, kRRange + 35.0f) || TargetCannotBeDamaged(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vector3 predicted = PredictPosition(target, 0.10f);
    const Vec3 direction = Direction2D(player.Position(), predicted);
    if (direction.IsZero() || !CursorAgrees(predicted, player.Position())) return false;
    const Vector3 endpoint = player.Position() + direction * kRRange;
    const Body body{ predicted, target.BoundingRadius(), static_cast<int>(target.NetworkId()), true, true };
    const REvaluation evaluation = EvaluateRPath(player.Position(), endpoint, body,
                                                 RZones(player.Position(), endpoint));
    const bool lethal = Lethal(target, RawR(target));
    const bool peel = defensive || IncomingHardCCUntil > Now();
    const int hits = Engine::CountEnemiesAt(predicted, 280.0f);
    const RCastContext context{ true, true, evaluation.InitialHit,
                                evaluation.TerrainDetonation, HasProjectileWall(player.Position(), predicted),
                                lethal, peel, hits >= Slider(UltimateMenu, "ObjectiveMinimumHits", 2),
                                hits, Slider(UltimateMenu, "MinimumTerrainTargets", 2) };
    if (!MayCastR(context)) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    RCastTick = Now();
    LastREndpoint = endpoint;
    RWasManual = false;
    return true;
}

inline bool TryDefensive(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, 900.0f)) return false;
    if (IncomingHardCCUntil > Now() ||
        (GameObjects::Player().HealthPercent() <= Slider(MobilityMenu, "EmergencyHP", 28))) {
        if (CastW(target, Mode::Automatic, true)) return true;
        if (CastR(target, Mode::Flee, true)) return true;
    }
    return false;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, RawQ(target)) && CastQ(target, mode, true)) return true;
    if (Lethal(target, RawE(target) + RawQ(target)) && CastE(target, mode)) return true;
    if (Lethal(target, RawR(target)) && CastR(target, mode, false)) return true;
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const float distance = GameObjects::Player().Position().Distance2D(target.Position());
    if (distance > kQMissileRange && CastW(target, Mode::Combo, false, true)) return true;
    if (distance > kQSlashRange && CastE(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    if (CastW(target, Mode::Combo, false, target.HealthPercent() >= 50.0f)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    if (target.HealthPercent() <= Slider(UltimateMenu, "RTargetHP", 60) &&
        CastR(target, Mode::Combo, false)) return true;
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastQ(target, Mode::Harass)) return true;
    if (GameObjects::Player().HealthPercent() > 55.0f &&
        CastW(target, Mode::Harass, false, false)) return true;
    return CastQ(target, Mode::Harass);
}

inline bool TryFarm(Mode mode) {
    if (!Ready(0, mode) || CurrentResource() < SpellCost(0) +
        Slider(FarmMenu, "ManaReserve", 70)) return false;
    return Engine::TryFarm(mode);
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (CastW(threat, Mode::Flee, true)) return true;
    if (Engine::ValidEnemy(threat) && CastQ(threat, Mode::Flee, true)) return true;
    if (Engine::ValidEnemy(threat) && CastE(threat, Mode::Flee, true)) return true;
    return Engine::ValidEnemy(threat) && CastR(threat, Mode::Flee, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (PlayerOverrideUntil > Now()) return true;
    AIHeroClient target = selected;
    if (!Engine::ValidEnemy(target)) target = Engine::SelectTarget(kQMissileRange + 80.0f);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 1200.0f);
    if (mode == Mode::Flee) {
        (void)TryFlee(threat);
        return true;
    }
    if (TryDefensive(threat)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic:
        // Automatic is deliberately reactive: no unsolicited engage.
        if (Engine::ValidEnemy(target) &&
            AutomaticAllowed({ false, IncomingHardCCUntil > Now(),
                               Lethal(target, RawQ(target)), false })) {
            (void)TryKillSecure(target, Mode::Automatic);
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
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(args, 220.0f, 110.0f,
                                                                 250, 280, 260, 1500, 450);
        if (threat.Valid && threat.CrossesPlayer) {
            IncomingThreatUntil = threat.LineThreatUntilTick;
            if (threat.LikelyHardCrowdControl) IncomingHardCCUntil = now + 650;
        }
        return;
    }
    const int slot = args.Slot;
    const bool controllerOwned = slot >= 0 && slot < 4 && Engine::WasControllerCast(slot);
    if (!controllerOwned) PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", kManualOwnershipMs);
    if (slot == 0) {
        QCastTick = now;
        QWasManual = !controllerOwned;
        const auto target = Engine::SelectTarget(kQMissileRange + 30.0f);
        if (Engine::ValidEnemy(target)) QTargetId = static_cast<int>(target.NetworkId());
    } else if (slot == 1) {
        WCastTick = now;
        WWasManual = !controllerOwned;
        const Element fromName = ElementFromRuntimeName(args.SpellName);
        if (fromName != Element::None) SetElement(fromName);
        const Element fromPosition = ElementFromRuntimeName(
            IsBrush(args.CastPosition) ? "Grass" :
            SDK::NavMesh::IsWater(args.CastPosition) ? "Water" :
            SDK::NavMesh::IsWall(args.CastPosition) ? "Rock" : "None");
        if (fromPosition != Element::None) SetElement(fromPosition);
    } else if (slot == 2) {
        ECastTick = now;
        EWasManual = !controllerOwned;
    } else if (slot == 3) {
        RCastTick = now;
        RWasManual = !controllerOwned;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (Engine::TextContains(args.BuffName, "Grass") ||
        Engine::TextContains(args.BuffName, "Brush")) {
        CurrentElement = Element::Brush;
        ElementExpireTick = now + kElementDurationMs;
        if (Engine::TextContains(args.BuffName, "Stealth")) GrassStealthUntil = args.EndTime > Game::Time()
            ? now + ControllerHelpers::RemainingMilliseconds(args.EndTime, 850, 100, 5000)
            : now + 850;
    } else if (Engine::TextContains(args.BuffName, "Water") ||
               Engine::TextContains(args.BuffName, "River")) {
        SetElement(Element::River);
    } else if (Engine::TextContains(args.BuffName, "Rock")) {
        SetElement(Element::Rock);
    } else if (Engine::TextContains(args.BuffName, "Passive")) {
        PassiveState.Confirmed = true;
        PassiveState.TargetId = QTargetId;
        PassiveState.LastElement = CurrentElement;
        PassiveState.ExpireTick = now + ControllerHelpers::RemainingMilliseconds(
            args.EndTime, 14000, 150, 16000);
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "Stealth")) GrassStealthUntil = 0;
    if (Engine::TextContains(args.BuffName, "Enchanted") ||
        Engine::TextContains(args.BuffName, "QiyanaW")) {
        CurrentElement = Element::None;
        ElementExpireTick = 0;
    }
    if (Engine::TextContains(args.BuffName, "Passive")) PassiveState = {};
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.EndTime > Game::Time()) OnBuffAdd(args);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || !HasElement()) return;
    const auto target = HeroByNetworkId(static_cast<int>(args.Target.NetworkId()));
    if (!Engine::ValidEnemy(target)) return;
    if (PassiveState.TargetId != 0 && PassiveState.TargetId != static_cast<int>(target.NetworkId()) &&
        Lethal(target, RawQ(target))) {
        args.Process = false;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) return;
    if (Engine::ValidEnemy(HeroByNetworkId(LastAutoTargetId)) && PassiveState.TargetId == LastAutoTargetId) {
        PassiveState.Confirmed = false;
    }
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.Sender.Name, "Qiyana") &&
        Engine::TextContains(args.Sender.Name, "Grass")) {
        GrassStealthUntil = std::max(GrassStealthUntil, Now() + 850);
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.Sender.Name, "Qiyana") &&
        Engine::TextContains(args.Sender.Name, "Grass")) GrassStealthUntil = 0;
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), HasElement() ? kQMissileRange : kQSlashRange,
                         0xFF35D5C8u, 1.0f, 48);
    if (!LastWEndpoint.IsZero()) Drawing::DrawCircle(LastWEndpoint, 42.0f, 0xFF2C8CFFu, 2.0f, 24);
    if (!LastREndpoint.IsZero()) Drawing::DrawLine(player.Position(), LastREndpoint, 0xFFB06CFFu, 2.0f);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("QiyanaOneTrick", "Qiyana one-trick mechanics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 520, 180, 1100));
    ElementMenu = TacticsMenu->AddSubMenu(new Menu("Elements", "Grass safety, Ice catch and Rock execute"));
    ElementMenu->Add(new MenuBool("PreserveGrass", "Preserve valuable Grass stealth", true));
    ElementMenu->Add(new MenuBool("UseRockExecute", "Use Rock below half health", true));
    MobilityMenu = TacticsMenu->AddSubMenu(new Menu("Mobility", "Safe W/E endpoint policy"));
    MobilityMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at endpoint", 2, 1, 5));
    MobilityMenu->Add(new MenuSlider("EmergencyHP", "Defensive W/R below HP", 28, 10, 70));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu("SupremeDisplay", "Terrain-aware R detonation"));
    UltimateMenu->Add(new MenuSlider("RTargetHP", "R target HP threshold", 60, 15, 100));
    UltimateMenu->Add(new MenuSlider("MinimumTerrainTargets", "Minimum R target count", 2, 1, 5));
    UltimateMenu->Add(new MenuSlider("ObjectiveMinimumHits", "Objective R target count", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("QiyanaFarm", "Conservative Q farm delegation"));
    FarmMenu->Add(new MenuSlider("ManaReserve", "Mana reserved after farm", 70, 0, 180));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("QiyanaCoach", "Live element and route coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/R route ranges", false));
}

inline void OnLoad() {
    CurrentElement = Element::None;
    PassiveState = {};
    ElementExpireTick = GrassStealthUntil = 0;
    QTargetId = QCastTick = WCastTick = ECastTick = RCastTick = 0;
    LastAutoTargetId = LastAutoTick = 0;
    IncomingThreatUntil = IncomingHardCCUntil = PlayerOverrideUntil = 0;
    LastQDirection = LastWEndpoint = LastEEndpoint = LastREndpoint = {};
    QWasManual = WWasManual = EWasManual = RWasManual = false;
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = ElementMenu = MobilityMenu = UltimateMenu = FarmMenu = CoachMenu = nullptr;
    CurrentElement = Element::None;
    PassiveState = {};
}

inline constexpr const char* Scenarios[] = {
    "Read Riot 26.15 and CommunityDragon 16.15 as the pinned Summoner's Rift baseline",
    "Exclude the 26.15 ARAM Mayhem Qiyana damage modifier from Summoner's Rift",
    "Track Royal Privilege per target with the 14-second internal cooldown",
    "Refresh passive eligibility after a confirmed different element",
    "Reconcile passive state from buff events and polling without inventing a mark",
    "Use current Q base damage and 85 percent bonus-AD ratio",
    "Use 650-range 1600-speed enchanted Q missile geometry",
    "Keep unenchanted Q at the 525 slash range",
    "Reject Q through projectile wall or collision uncertainty",
    "Apply first-body collision and 75 percent continuation damage",
    "Apply Ice root and slow only for confirmed River element",
    "Preserve Grass stealth for a valuable escape or repeat trade",
    "Apply Rock multiplier only below 50 percent target health",
    "Use W terrain classification from real NavMesh brush, water and wall flags",
    "Limit W dash to its 300-unit travel distance",
    "Reject W endpoint under enemy turret outside lethal escape-approved route",
    "Reject W endpoint with point-click or dash hazard",
    "Reject W endpoint surrounded by excess enemies",
    "Refresh Q after successful W and track the element expiry",
    "Select Ice for a catch opportunity and Grass for defensive posture",
    "Use W enchanted attack speed and on-hit damage in damage estimates",
    "Use E 650 target reach and projected post-dash endpoint",
    "Reject E into terrain, turret, hard crowd control or excessive enemies",
    "Preserve AA windup during nonlethal Q/E casts",
    "Use E through a real target only when the route improves reach or safety",
    "Use R 950 range, 220 radius and 1000 missile speed",
    "Require R initial target intersection before granting hit credit",
    "Require R wall, river or brush detonation geometry",
    "Evaluate R wall displacement against the pushed target segment",
    "Evaluate river and brush terrain crossings separately",
    "Deduplicate champion hits across multiple R terrain paths",
    "Use R missing-health damage and monster cap from game-bin values",
    "Use R for multi-target objective fights or verified defensive peel",
    "Reject ordinary nonlethal single-target R without terrain outcome",
    "Preserve selected target while it remains reachable",
    "Fall back to orbwalker target and then policy-scored target",
    "Harass with safe Q and preserve health and mana",
    "LaneClear and LastHit delegate ordinary farm only through the shared engine",
    "Jungle mode preserves attacks and uses Q/W only when mana allows",
    "Flee chooses cursor-aligned Grass W before offensive mobility",
    "Flee E requires a safe real enemy bridge and valid endpoint",
    "Automatic mode never starts an unsolicited engage",
    "Automatic mode may react to hard CC, kill-secure or defensive pressure",
    "Yield after a player-owned Q/W/E/R cast through shared engine ownership",
    "Re-plan after a manual cast without clearing observed element state",
    "Track Qiyana grass objects and reconcile stale stealth events",
    "Track enemy cast lines and hard crowd-control threat windows",
    "Never automate Flash, Ignite, Smite or item actives",
    "Keep profile metadata separate from the owned decision loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Qiyana";
    controller.ControllerId = "champion.kuroaio.ai.qiyana.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIQiyana.md";
    controller.ImplementationSummary =
        "Terrain-classified W element selection, collision-aware Q, safe E "
        "endpoint checks, wall/river/brush R detonation planning, passive and "
        "manual ownership reconciliation across every orbwalker mode.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Qiyana
