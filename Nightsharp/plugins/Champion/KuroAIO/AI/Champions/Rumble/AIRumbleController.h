#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIRumbleGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Rumble {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::HasNearbyJungleTarget;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictionAtLeast;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::ValidHostileUnitInGameplayRange;

inline Menu* TacticsMenu = nullptr;
inline Menu* HeatMenu = nullptr;
inline Menu* ShieldMenu = nullptr;
inline Menu* HarpoonMenu = nullptr;
inline Menu* EqualizerMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline float ObservedHeat = 0.0f;
inline HeatBand CurrentHeatBand = HeatBand::Cold;
inline bool Overheated = false;
inline bool DangerZoneBuffObserved = false;
inline int EAmmo = 2;
inline int EMaximumAmmo = 2;
inline bool EAmmoObserved = false;
inline int LastHeatPollTick = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int QActiveUntil = 0;
inline int ShieldUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline Mode LastKnownMode = Mode::None;
inline Vector3 GapcloserEnd = {};
inline RLine LastRLine = {};
inline Vector3 LastQAim = {};
inline Vector3 LastEAim = {};

using ControllerHelpers::Now;

using ControllerHelpers::Ready;

inline bool TargetCannotBeDamaged(const AIHeroClient& target,
                                  bool projectile = false) {
    if (!Engine::ValidEnemy(target) || IsCommonUntargetableOrImmune(target)) return true;
    return projectile && HasSpellShieldOrImmunity(target);
}

inline int ObservedHarpoonAmmo() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return -1;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::E);
    if (!spell.IsValid()) return -1;
    const int maximum = spell.MaxAmmo();
    const int ammo = spell.Ammo();
    if (maximum != 2 || ammo < 0 || ammo > maximum) return -1;
    EMaximumAmmo = maximum;
    return ammo;
}

inline bool HasOverheatBuff(const AIHeroClient& player) {
    return ControllerHelpers::HasAnyBuff(player, {
        "RumbleOverheat", "rumbleoverheat", "RumbleOverheatAttack"
    });
}

inline bool HasDangerZoneBuff(const AIHeroClient& player) {
    return ControllerHelpers::HasAnyBuff(player, {
        "RumbleDangerZoneBuff", "rumbledangerzonebuff"
    });
}

inline void ReconcileState(bool force = false) {
    const int now = Now();
    if (!force && now - LastHeatPollTick < 35) return;
    LastHeatPollTick = now;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const float liveHeat = player.Mana();
    if (std::isfinite(liveHeat) && liveHeat >= -0.5f &&
        liveHeat <= kMaximumHeat + 1.0f) ObservedHeat = ClampHeat(liveHeat);
    Overheated = HasOverheatBuff(player);
    DangerZoneBuffObserved = HasDangerZoneBuff(player);
    CurrentHeatBand = ClassifyHeat(ObservedHeat, Overheated);
    const int ammo = ObservedHarpoonAmmo();
    if (ammo >= 0) {
        EAmmo = ammo;
        EAmmoObserved = true;
    } else {
        EAmmo = ReconciledHarpoonAmmo(-1, 0, EAmmo);
    }
    if (QActiveUntil > 0 && now > QActiveUntil) QActiveUntil = 0;
    if (ShieldUntil > 0 && now > ShieldUntil) ShieldUntil = 0;
}

inline bool SecondHarpoonWindow() {
    return EAmmo == 1 && LastECastTick > 0 && Now() - LastECastTick <= 6100;
}

inline void RegisterOwnedCast(int slot) {
    const int now = Now();
    const bool secondHarpoon = slot == 2 && SecondHarpoonWindow();
    ObservedHeat = HeatAfterCast(ObservedHeat, slot, secondHarpoon);
    CurrentHeatBand = ClassifyHeat(ObservedHeat, Overheated);
    if (slot == 0) {
        LastQCastTick = now;
        QActiveUntil = now + 3000;
    } else if (slot == 1) {
        LastWCastTick = now;
        ShieldUntil = now + 1500;
    } else if (slot == 2) {
        LastECastTick = now;
        EAmmo = std::max(0, EAmmo - 1);
    } else if (slot == 3) LastRCastTick = now;
}

inline OverheatContext MakeOverheatContext(const AIHeroClient& target,
                                           bool allowIntentional,
                                           bool jungle,
                                           bool emergency,
                                           bool followupRequired) {
    const bool valid = Engine::ValidEnemy(target);
    float projectedDamage = 0.0f;
    if (valid) {
        if (Ready(0, Mode::Combo)) projectedDamage += Engine::RuntimeSpells[0]->GetDamage(target);
        if (EAmmo > 0 && Engine::RuntimeSpells[2]) projectedDamage += Engine::RuntimeSpells[2]->GetDamage(target);
    }
    return { allowIntentional, valid && InAutoAttackRange(target, 85.0f),
        valid && projectedDamage >= target.Health() + target.AllShield(),
        jungle, emergency, followupRequired };
}

inline bool HeatAllows(int slot,
                       const AIHeroClient& target,
                       bool secondHarpoon,
                       bool allowIntentional = false,
                       bool jungle = false,
                       bool emergency = false,
                       bool followupRequired = false) {
    if (Overheated) return false;
    return HeatPolicyAllows(ObservedHeat, slot, secondHarpoon,
        MakeOverheatContext(target, allowIntentional, jungle, emergency,
                            followupRequired));
}

using ControllerHelpers::PreserveAttack;

inline Vector3 PredictedAim(const AIHeroClient& target,
                            int slot,
                            SDK::HitChance required,
                            bool requireNoCollision) {
    if (!Engine::ValidEnemy(target) || !Engine::RuntimeSpells[slot]) return {};
    const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
    if (!PredictionAtLeast(prediction, required)) return {};
    if (requireNoCollision && !prediction.CollisionObjects.empty()) return {};
    Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = prediction.GetUnitPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = target.Position();
    return aim;
}

inline bool CastQ(const AIHeroClient& target, Mode mode,
                  bool allowIntentionalOverheat = false,
                  bool reactive = false) {
    if (!Ready(0, mode) || !CastThrottleReady(0, reactive) ||
        TargetCannotBeDamaged(target) || PreserveAttack(reactive) ||
        !HeatAllows(0, target, false, allowIntentionalOverheat, false, false,
                    Ready(2, mode) && EAmmo > 0)) return false;
    const auto player = GameObjects::Player();
    const Vector3 aim = PredictedAim(target, 0,
        reactive ? SDK::HitChance::Medium : SDK::HitChance::High, false);
    if (!player.IsValid() || !aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > kQRange + target.BoundingRadius() ||
        !PointInQConicalFlame(player.Position(), aim, aim, target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastQAim = aim;
    RegisterOwnedCast(0);
    return true;
}

inline bool CastW(Mode mode, const AIHeroClient& target,
                  bool emergency = false, bool primeDanger = false) {
    if (!Ready(1, mode) || !CastThrottleReady(1, emergency) ||
        PreserveAttack(emergency)) return false;
    const bool meaningful = emergency || primeDanger ||
        Engine::ValidEnemy(target, 720.0f) || mode == Mode::Flee;
    if (!meaningful || !HeatAllows(1, target, false, false, false, emergency)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    RegisterOwnedCast(1);
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode,
                  bool killSecure = false, bool fleeing = false,
                  bool reactive = false) {
    if (!Ready(2, mode) || EAmmo <= 0 ||
        !CastThrottleReady(2, reactive || SecondHarpoonWindow()) ||
        TargetCannotBeDamaged(target, true) || PreserveAttack(reactive)) return false;
    const bool second = SecondHarpoonWindow();
    if (PreserveHarpoonCharge(EAmmo, killSecure, fleeing, second) &&
        Bool(HarpoonMenu, "ReserveCharge", true)) return false;
    if (!HeatAllows(2, target, second, killSecure, false, fleeing)) return false;
    const auto player = GameObjects::Player();
    const Vector3 aim = PredictedAim(target, 2,
        (reactive || Engine::IsHardCrowdControlled(target))
            ? SDK::HitChance::Medium : SDK::HitChance::High, true);
    if (!player.IsValid() || !aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > kERange + target.BoundingRadius() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kEWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastEAim = aim;
    RegisterOwnedCast(2);
    return true;
}

inline float ConservativeComboDamage(const AIHeroClient& target,
                                     bool includeUltimate) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    float damage = 0.0f;
    if (Ready(0, Mode::Combo)) damage += Engine::RuntimeSpells[0]->GetDamage(target);
    if (EAmmo > 0 && Engine::RuntimeSpells[2]) damage +=
        Engine::RuntimeSpells[2]->GetDamage(target) * static_cast<float>(std::min(EAmmo, 2));
    if (includeUltimate && Ready(3, Mode::Combo)) damage +=
        RDamagePerSecond(SpellRank(3), GameObjects::Player().AP()) * 2.5f;
    return std::max(0.0f, damage);
}

inline bool TargetLethal(const AIHeroClient& target, bool includeUltimate) {
    return Engine::ValidEnemy(target) && ConservativeComboDamage(target, includeUltimate) >=
        target.Health() + target.AllShield();
}

inline std::vector<Vector3> PredictedEnemyPoints(float delaySeconds) {
    std::vector<Vector3> points;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy, kRRange + kRLineLength))
            points.push_back(ControllerHelpers::PredictPosition(enemy, delaySeconds));
    }
    return points;
}

inline RLine BestRLine(const AIHeroClient& primary, int& hitCount) {
    hitCount = 0;
    if (!Engine::ValidEnemy(primary)) return {};
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    const Vector3 center = ControllerHelpers::PredictPosition(primary, 0.5833f);
    if (!center.IsValid() || center.IsZero()) return {};
    std::array<Vector3, 4> directions{};
    int directionCount = 0;
    const Vector3 movement = primary.PathEnd().IsValid() && !primary.PathEnd().IsZero()
        ? SharedGeometry::Direction2D(primary.Position(), primary.PathEnd()) : Vector3{};
    if (!movement.IsZero()) directions[directionCount++] = movement;
    const Vector3 radial = SharedGeometry::Direction2D(player.Position(), center);
    if (!radial.IsZero()) {
        directions[directionCount++] = radial;
        directions[directionCount++] = SharedGeometry::Rotate2D(radial, SharedGeometry::kPi * 0.5f);
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (directionCount >= static_cast<int>(directions.size())) break;
        if (!Engine::ValidEnemy(enemy) || enemy.NetworkId() == primary.NetworkId()) continue;
        const Vector3 between = SharedGeometry::Direction2D(center,
            ControllerHelpers::PredictPosition(enemy, 0.5833f));
        if (!between.IsZero()) directions[directionCount++] = between;
    }
    const auto points = PredictedEnemyPoints(0.5833f);
    RLine best{};
    float bestScore = -FLT_MAX;
    for (int i = 0; i < directionCount; ++i) {
        const RLine candidate = CenteredRLine(player.Position(), center, directions[i]);
        if (!candidate.Valid || !RLineContacts(candidate, center, primary.BoundingRadius())) continue;
        const int hits = RLineHitCount(candidate, points, 65.0f);
        const float miss = SharedGeometry::ProjectPointToSegment2D(center,
            candidate.Start, candidate.End).Distance;
        const bool follows = !movement.IsZero() &&
            std::fabs(movement.Dot(directions[i])) >= 0.72f;
        const float score = RLineScore(hits, true, miss, follows);
        if (score > bestScore) { bestScore = score; best = candidate; hitCount = hits; }
    }
    return best;
}

inline bool CastR(const AIHeroClient& target, Mode mode,
                  bool defensive = false) {
    if (!Ready(3, mode) || !CastThrottleReady(3, defensive) ||
        TargetCannotBeDamaged(target) || PreserveAttack(defensive)) return false;
    int hits = 0;
    const RLine line = BestRLine(target, hits);
    if (!line.Valid) return false;
    const auto player = GameObjects::Player();
    const bool lethal = TargetLethal(target, true);
    const RSafetyContext safety{
        Engine::UnderEnemyTurret(player.Position()),
        Engine::UnderEnemyTurret(target.Position()), lethal, defensive,
        mode == Mode::Automatic,
        Engine::CountAlliesAt(target.Position(), 900.0f) > 0, hits };
    if (!RCastSafe(safety)) return false;
    if (!defensive && hits < Slider(EqualizerMenu, "MinimumTargets", 2) &&
        !lethal) return false;
    if (!Engine::ControllerCastVector(3, line.Start, line.End)) return false;
    LastRLine = line;
    RegisterOwnedCast(3);
    return true;
}

inline AIHeroClient CooperativeTarget() {
    return Engine::SelectTarget(kRRange + kRLineLength);
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    const float eDamage = EAmmo > 0 && Engine::RuntimeSpells[2]
        ? Engine::RuntimeSpells[2]->GetDamage(target) : 0.0f;
    if (eDamage >= target.Health() + target.AllShield() &&
        CastE(target, mode, true, false, true)) return true;
    const float qDamage = Ready(0, mode) ? Engine::RuntimeSpells[0]->GetDamage(target) : 0.0f;
    if (qDamage >= target.Health() + target.AllShield() && CastQ(target, mode, true, true)) return true;
    if (mode != Mode::Harass && mode != Mode::LaneClear && mode != Mode::Jungle &&
        mode != Mode::LastHit && TargetLethal(target, true)) return CastR(target, mode);
    return false;
}

inline bool TryDefensive(const AIHeroClient& threat, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool emergency = IncomingThreatUntil >= Now() ||
        player.HealthPercent() <= Slider(ShieldMenu, "EmergencyHP", 34);
    if (!emergency && mode != Mode::Flee) return false;
    const Mode castMode = mode == Mode::None ? Mode::Automatic : mode;
    if (Bool(ShieldMenu, "DefensiveW", true) && CastW(castMode, threat, true)) return true;
    return Engine::ValidEnemy(threat) && Bool(HarpoonMenu, "Peel", true) &&
        CastE(threat, castMode, false, true, true);
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const bool lethal = TargetLethal(target, true);
    if ((lethal || Engine::CountEnemiesAt(target.Position(), 500.0f) >= 2) &&
        CastR(target, Mode::Combo)) return true;
    if (ObservedHeat >= 30.0f && ObservedHeat < kDangerZoneHeat &&
        Bool(HeatMenu, "PrimeDangerWithW", true) && CastW(Mode::Combo, target, false, true)) return true;
    if (CastE(target, Mode::Combo, lethal)) return true;
    if (CastQ(target, Mode::Combo, Bool(HeatMenu, "AllowComboOverheat", true))) return true;
    return CastW(Mode::Combo, target);
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) ||
        ObservedHeat >= Slider(HeatMenu, "HarassHeatCeiling", 120)) return false;
    if (CastE(target, Mode::Harass)) return true;
    return CastQ(target, Mode::Harass);
}

inline int ConeFarmHits(const Vector3& aim, const std::vector<AIBaseClient>& units) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int hits = 0;
    for (const auto& unit : units) {
        if (ValidHostileUnitInGameplayRange(unit, kQConeDistance) &&
            PointInQConicalFlame(player.Position(), aim, unit.Position(), unit.BoundingRadius())) ++hits;
    }
    return hits;
}

inline bool TryFarmQ(const std::vector<AIBaseClient>& units, Mode mode, bool jungle) {
    if (!Ready(0, mode) || !CastThrottleReady(0) || PreserveAttack(false) || units.empty() ||
        !HeatAllows(0, {}, false, jungle && Bool(HeatMenu, "AllowJungleOverheat", true), jungle)) return false;
    Vector3 bestAim{};
    int bestHits = 0;
    for (const auto& unit : units) {
        if (!ValidHostileUnitInGameplayRange(unit, kQRange)) continue;
        const int hits = ConeFarmHits(unit.Position(), units);
        if (hits > bestHits) { bestHits = hits; bestAim = unit.Position(); }
    }
    const int minimum = jungle ? 1 : Slider(FarmMenu, "QMinimumMinions", 3);
    if (bestHits < minimum || bestAim.IsZero()) return false;
    if (!Engine::ControllerCastPosition(0, bestAim)) return false;
    LastQAim = bestAim;
    RegisterOwnedCast(0);
    return true;
}

inline bool TryFarmE(const std::vector<AIBaseClient>& units, Mode mode,
                     bool lastHit, bool jungle) {
    if (!Ready(2, mode) || EAmmo <= 0 || !CastThrottleReady(2) || PreserveAttack(false)) return false;
    const bool second = SecondHarpoonWindow();
    if (!HeatAllows(2, {}, second, jungle && Bool(HeatMenu, "AllowJungleOverheat", true), jungle)) return false;
    AIBaseClient best{};
    float bestHealth = FLT_MAX;
    for (const auto& unit : units) {
        if (!ValidHostileUnitInGameplayRange(unit, kERange)) continue;
        const int travel = static_cast<int>(250.0f +
            GameObjects::Player().Position().Distance2D(unit.Position()) / kESpeed * 1000.0f);
        const float health = SDK::HealthPrediction::GetPrediction(unit, std::clamp(travel, 250, 1100));
        const float damage = Engine::RuntimeSpells[2]->GetDamage(unit);
        if (lastHit && (health <= 0.0f || damage < health)) continue;
        if (!lastHit && !jungle && Bool(HarpoonMenu, "ReserveCharge", true) &&
            EAmmo <= 1 && !second) continue;
        if (health < bestHealth) { bestHealth = health; best = unit; }
    }
    if (!best.IsValid() || !Engine::ControllerCastPosition(2, best.Position())) return false;
    LastEAim = best.Position();
    RegisterOwnedCast(2);
    return true;
}

inline bool TryFarm(Mode mode) {
    const bool jungle = mode == Mode::Jungle ||
        (mode == Mode::LaneClear && HasNearbyJungleTarget(kQRange) && Engine::ClearUnits(false).empty());
    const bool lastHit = mode == Mode::LastHit;
    auto units = Engine::ClearUnits(jungle);
    if (units.empty()) return false;
    if (lastHit) return TryFarmE(units, Mode::LastHit, true, false);
    if (TryFarmQ(units, jungle ? Mode::Jungle : Mode::LaneClear, jungle)) return true;
    return TryFarmE(units, jungle ? Mode::Jungle : Mode::LaneClear, false, jungle);
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (TryDefensive(threat, Mode::Flee)) return true;
    return Engine::ValidEnemy(threat) && GameObjects::Player().HealthPercent() <=
        Slider(EqualizerMenu, "FleeRHP", 28) && CastR(threat, Mode::Flee, true);
}

inline bool TryAutomatic(const AIHeroClient& target) {
    const AIHeroClient gapcloser = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    const AIHeroClient interrupt = ControllerHelpers::HeroByNetworkId(InterruptTargetId);
    const AIHeroClient threat = Engine::ValidEnemy(gapcloser)
        ? gapcloser : NearestEnemyToPlayer(target, 950.0f);
    const bool defensive = IncomingThreatUntil >= Now() || GapcloserExpireTick >= Now();
    const bool interrupting = InterruptExpireTick >= Now() && Engine::ValidEnemy(interrupt);
    const bool killSecure = Engine::ValidEnemy(target) && TargetLethal(target, false);
    if (!AutomaticAllowed({ defensive, interrupting, killSecure, false })) return false;
    if (defensive && TryDefensive(threat, Mode::Automatic)) return true;
    if (interrupting && CastE(interrupt, Mode::Automatic, false, true, true)) return true;
    return killSecure && TryKillSecure(target, Mode::Automatic);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    (void)selected;
    ReconcileState();
    LastKnownMode = mode;
    const AIHeroClient target = CooperativeTarget();
    const AIHeroClient threat = NearestEnemyToPlayer(target, 950.0f);
    if (mode == Mode::Flee) { (void)TryFlee(threat); return true; }
    if (mode == Mode::Automatic || mode == Mode::None) { (void)TryAutomatic(target); return true; }
    if (TryDefensive(threat, mode) || TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(
            args, 230.0f, 110.0f, 250, 280, 260, 1500, 450);
        if (threat.Valid && threat.CrossesPlayer) {
            IncomingThreatUntil = now + (threat.LikelyHardCrowdControl ? 800 : 420);
            IncomingThreatTargetId = static_cast<int>(args.Sender.NetworkId);
        }
        return;
    }
    if (args.Slot < 0 || args.Slot >= 4) return;
    if (!Engine::WasControllerCast(args.Slot)) {
        if (args.Slot == 0) { LastQCastTick = now; QActiveUntil = now + 3000; }
        else if (args.Slot == 1) { LastWCastTick = now; ShieldUntil = now + 1500; }
        else if (args.Slot == 2) LastECastTick = now;
        else if (args.Slot == 3) LastRCastTick = now;
    }
    ReconcileState(true);
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (ControllerHelpers::TextContainsAny(args.BuffName,
            { "RumbleOverheat", "rumbleoverheat" })) {
        Overheated = added;
        if (added) CurrentHeatBand = HeatBand::Overheated;
    }
    if (ControllerHelpers::TextContainsAny(args.BuffName,
            { "RumbleDangerZoneBuff", "rumbledangerzone" }))
        DangerZoneBuffObserved = added;
    if (ControllerHelpers::TextContainsAny(args.BuffName,
            { "RumbleShield", "rumbleshield" })) ShieldUntil = added ? Now() + 1500 : 0;
    ReconcileState(true);
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (ControllerHelpers::CaptureGapcloser(args, GapcloserTargetId, GapcloserEnd,
            GapcloserExpireTick, 850.0f, 950)) {
        IncomingThreatTargetId = GapcloserTargetId;
        IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 800);
    }
}

inline const char* HeatBandName() {
    switch (CurrentHeatBand) {
    case HeatBand::DangerZone: return "Danger";
    case HeatBand::Overheated: return "Overheat";
    default: return "Cold";
    }
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFFF8A45u, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF55B8EEu, 1.2f, 48);
    if (LastRLine.Valid) Drawing::DrawLine(LastRLine.Start, LastRLine.End, 0xFFFF5544u, 2.5f);
    Vector2 screen{};
    if (Drawing::WorldToScreen(player.Position(), screen)) {
        char state[160]{};
        _snprintf_s(state, _TRUNCATE,
            "Rumble | %.0f heat | %s | E %d/2", ObservedHeat,
            HeatBandName(), EAmmo);
        Drawing::DrawText(screen.x - 150.0f, screen.y - 110.0f,
            0xFFFFFFFFu, state);
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("RumbleOneTrick", "Rumble heat and Equalizer"));
    HeatMenu = TacticsMenu->AddSubMenu(new Menu("HeatBands", "Cold, danger, overheat"));
    HeatMenu->Add(new MenuBool("PrimeDangerWithW", "Prime Danger Zone with useful W", true));
    HeatMenu->Add(new MenuBool("AllowComboOverheat", "Allow lethal combo overheat", true));
    HeatMenu->Add(new MenuBool("AllowJungleOverheat", "Allow jungle overheat", true));
    HeatMenu->Add(new MenuSlider("HarassHeatCeiling", "Stop harass at heat", 120, 50, 140));
    ShieldMenu = TacticsMenu->AddSubMenu(new Menu("ScrapShield", "W shield, speed and heat setup"));
    ShieldMenu->Add(new MenuBool("DefensiveW", "Use W for incoming danger", true));
    ShieldMenu->Add(new MenuSlider("EmergencyHP", "Emergency W health (%)", 34, 10, 75));
    HarpoonMenu = TacticsMenu->AddSubMenu(new Menu("ElectroHarpoon", "E prediction and charge economy"));
    HarpoonMenu->Add(new MenuBool("ReserveCharge", "Reserve last unpaired harpoon", true));
    HarpoonMenu->Add(new MenuBool("Peel", "Use E to peel", true));
    EqualizerMenu = TacticsMenu->AddSubMenu(new Menu("Equalizer", "R escape path and turret safety"));
    EqualizerMenu->Add(new MenuSlider("MinimumTargets", "Minimum R targets", 2, 1, 5));
    EqualizerMenu->Add(new MenuSlider("FleeRHP", "Defensive R below health (%)", 28, 5, 65));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("RumbleFarm", "Heat-aware farm and last hit"));
    FarmMenu->Add(new MenuSlider("QMinimumMinions", "Minimum lane minions for Q", 3, 1, 7));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("RumbleCoach", "State and placement overlays"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw ranges and heat state", false));
}

inline void OnLoad() {
    ObservedHeat = 0.0f; CurrentHeatBand = HeatBand::Cold;
    Overheated = DangerZoneBuffObserved = false;
    EAmmo = EMaximumAmmo = 2; EAmmoObserved = false;
    LastHeatPollTick = LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    QActiveUntil = ShieldUntil = IncomingThreatUntil = IncomingThreatTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    LastAutoTargetId = LastAutoTick = 0;
    LastKnownMode = Mode::None; GapcloserEnd = LastQAim = LastEAim = {}; LastRLine = {};
    ReconcileState(true);
}
inline void OnUnload() {
    TacticsMenu = HeatMenu = ShieldMenu = HarpoonMenu = EqualizerMenu = nullptr;
    FarmMenu = CoachMenu = nullptr; LastRLine = {};
}

inline constexpr const char* Scenarios[] = {
    "Read Riot 26.15 and CommunityDragon 16.15 as the pinned Summoner's Rift baseline",
    "Classify Cold below 50, Danger Zone from 50, and Overheat at 150 heat",
    "Reconcile heat from the resource bar and overheat/danger buffs every poll",
    "Reconcile local spell events without double-applying controller heat",
    "Treat first Electro Harpoon as 20 heat and paired second harpoon as free",
    "Reject ordinary casts that would accidentally overheat",
    "Allow planned overheat only for lethal close-range autos, jungle DPS or emergency defense",
    "Preserve follow-up spells instead of silencing the combo too early",
    "Aim Flamespitter through its 64-degree full cone and 600 cast range",
    "Keep Q facing on predicted target movement for its three-second duration",
    "Use Scrap Shield for both shield value and one-second speed route",
    "Use W to enter Danger Zone only while the shield or speed is useful",
    "Track two Electro Harpoon ammo charges by event and spellbook polling",
    "Reserve the last unpaired harpoon outside kill, peel and second-shot windows",
    "Require E prediction, first-body collision clearance and projectile-wall clearance",
    "Build Equalizer as a 1050-unit vector line inside its 1700 start range",
    "Score R along target escape movement and alternative multi-target axes",
    "Require the primary target to contact the R trail",
    "Reject nonlethal single-target R into an unsupported enemy turret",
    "Reject nondefensive R while Rumble is tanking an enemy turret",
    "Combo primes Danger Zone, uses harpoons, keeps Q facing and converts a safe overheat",
    "Harass reserves heat, a harpoon charge and all unsolicited Equalizer casts",
    "LaneClear, Jungle and LastHit own heat-aware farm decisions",
    "Flee uses W speed, E slow and only defensive Equalizer placement",
    "Automatic mode permits defense, interrupt pressure and kill secure but never engage",
    "Preserve auto-attack windup before nonreactive spell casts",
    "Reconcile observed spell state while keeping all casts autonomous",
    "Never automate summoner spells or item actives",
    "Keep profile metadata separate from the owned decision loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Rumble;
    controller.ControllerId = "champion.kuroaio.ai.rumble.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIRumble.md";
    controller.ImplementationSummary = "150-heat band planning, Q cone facing, W defensive speed, paired harpoon economy and escape-path Equalizer turret safety.";
    controller.Scenarios = Scenarios; controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad; controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu; controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw; controller.OnProcessSpell = &OnProcessSpell;
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>; controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;

    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>; controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1500, 250, 6000>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Rumble
