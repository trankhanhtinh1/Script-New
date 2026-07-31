#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AINocturneGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

namespace Plugins::KuroAIO::AI::Controllers::Nocturne {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PredictionAtLeast;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Ready;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline bool PassiveReady = true;
inline int PassiveReadyTick = 0;
inline int PassiveLastAttackTick = 0;
inline int PassiveLastHitCount = 0;
inline float PassiveLastExpectedHeal = 0.0f;

inline Vector3 QTrailStart = {};
inline Vector3 QTrailEnd = {};
inline int QTrailExpireTick = 0;
inline int QTargetId = 0;
inline int QCastTick = 0;

inline bool WShieldActive = false;
inline bool WBlockSteroidActive = false;
inline int WShieldExpireTick = 0;
inline int WSteroidExpireTick = 0;
inline int WCastTick = 0;
inline int IncomingSpellUntil = 0;
inline int IncomingSpellImpactTick = 0;
inline int IncomingCasterId = 0;
inline bool IncomingHardCrowdControl = false;

inline int ETargetId = 0;
inline int ECastTick = 0;
inline int ETetherExpireTick = 0;
inline bool ETetherConfirmed = false;
inline bool EFearConfirmed = false;

inline bool RActive = false;
inline bool RControllerOwned = false;
inline bool RDashIssued = false;
inline int RFirstCastTick = 0;
inline int RExpireTick = 0;
inline int RTargetId = 0;
inline int RCastTick = 0;
inline bool PendingControllerR = false;

inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PlayerOverrideUntil = 0;
inline int GapcloserTargetId = 0;
inline Vector3 GapcloserEnd = {};
inline int GapcloserExpireTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;

inline bool HasMana(float percent) {
    const auto player = GameObjects::Player();
    return player.IsValid() && player.ManaPercent() >= percent;
}

inline bool BuffToken(const char* name,
                      std::initializer_list<const char*> tokens) {
    return ControllerHelpers::TextContainsAny(name, tokens);
}

inline bool PlayerPassiveBuff() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        (player.HasBuff("NocturneUmbraBlades") ||
         player.HasBuff("NocturnePassive") ||
         player.HasBuff("NocturneUmbraBladesReady"));
}

inline bool PlayerTrailBuff() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        (player.HasBuff("NocturneDuskbringer") ||
         player.HasBuff("NocturneDuskbringerBuff"));
}

inline bool PlayerShieldBuff() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        (player.HasBuff("NocturneShroudofDarkness") ||
         player.HasBuff("NocturneWSpellShield"));
}

inline bool PlayerBlockSteroidBuff() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        (player.HasBuff("NocturneShroudofDarknessAttackSpeed") ||
         player.HasBuff("NocturneWAttackSpeed"));
}

inline bool RuntimeRRecast() {
    return RuntimeNameContains(3, "paranoia2") ||
           RuntimeNameContains(3, "paranoiadash") ||
           RuntimeNameContains(3, "recast");
}

inline AIHeroClient TetherTarget() {
    return ETargetId != 0 ? HeroByNetworkId(ETargetId) : AIHeroClient{};
}

inline bool TargetHasTether(const AIHeroClient& target) {
    return target.IsValid() &&
        (target.HasBuff("NocturneUnspeakableHorror") ||
         target.HasBuff("NocturneUnspeakableHorrorDebuff") ||
         target.HasBuff("NocturneE"));
}

inline bool TargetFeared(const AIHeroClient& target) {
    return target.IsValid() &&
        (target.HasBuff("NocturneUnspeakableHorrorFear") ||
         target.HasBuff("NocturneUnspeakableHorror2") ||
         Engine::IsHardCrowdControlled(target));
}

inline void ClearTether() {
    ETargetId = 0;
    ETetherExpireTick = 0;
    ETetherConfirmed = false;
    EFearConfirmed = false;
}

inline void ClearR() {
    RActive = false;
    RControllerOwned = false;
    RDashIssued = false;
    RFirstCastTick = 0;
    RExpireTick = 0;
    RTargetId = 0;
    PendingControllerR = false;
}

inline void RefreshState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;

    if (PlayerPassiveBuff() || (PassiveReadyTick > 0 && now >= PassiveReadyTick)) {
        PassiveReady = true;
        PassiveReadyTick = 0;
    }
    if (QTrailExpireTick <= now && !PlayerTrailBuff()) {
        QTrailStart = {};
        QTrailEnd = {};
        QTrailExpireTick = 0;
    }
    WShieldActive = PlayerShieldBuff() || WShieldExpireTick > now;
    WBlockSteroidActive = PlayerBlockSteroidBuff() || WSteroidExpireTick > now;
    if (!WShieldActive) WShieldExpireTick = 0;
    if (!WBlockSteroidActive) WSteroidExpireTick = 0;

    if (ETargetId != 0) {
        const auto target = TetherTarget();
        ETetherConfirmed = TargetHasTether(target);
        EFearConfirmed = TargetFeared(target);
        if (!Engine::ValidEnemy(target) ||
            (now > ETetherExpireTick && !ETetherConfirmed && !EFearConfirmed) ||
            (!EFearConfirmed && !TetherMaintained(
                player.Position(), target.Position(), target.BoundingRadius()))) {
            ClearTether();
        }
    }

    const bool runtimeR = RuntimeRRecast() || player.HasBuff("NocturneParanoia");
    if (runtimeR) {
        RActive = true;
        if (RExpireTick <= now) RExpireTick = now + 6000;
    } else if (RActive && (RExpireTick <= now || RDashIssued)) {
        ClearR();
    }
    if (IncomingSpellUntil <= now) {
        IncomingCasterId = 0;
        IncomingSpellImpactTick = 0;
        IncomingHardCrowdControl = false;
    }
}

inline float SpellDamage(int index, const AIHeroClient& target) {
    return index >= 0 && index < 4 && Engine::RuntimeSpells[index] &&
           target.IsValid()
        ? Engine::RuntimeSpells[index]->GetDamage(target)
        : 0.0f;
}

inline float ExpectedFollowupDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    float damage = SpellDamage(3, target);
    if (Ready(0)) damage += SpellDamage(0, target);
    if (Ready(2)) damage += SpellDamage(2, target);
    damage += SDK::Damage::GetAutoAttackDamage(player, target, true) * 2.0f;
    return damage;
}

inline bool Lethal(const AIHeroClient& target, float damage) {
    return target.IsValid() && damage >= target.Health() + target.AllShield();
}

inline bool RLandingSafe(const AIHeroClient& target, bool manual) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) ||
        IsCommonUntargetableOrImmune(target)) return false;
    const Vector3 landing = PredictPosition(target, 0.18f);
    const float distance = player.Position().Distance2D(target.Position());
    const bool lethal = Lethal(target, ExpectedFollowupDamage(target));
    ParanoiaSafety context{};
    context.DestinationValid = landing.IsValid() && !landing.IsZero();
    context.InRange = distance <= ParanoiaRange(SpellRank(3)) +
                                 target.BoundingRadius();
    context.TargetDamageable = true;
    context.Wall = !context.DestinationValid || SDK::NavMesh::IsWall(landing);
    context.Turret = Engine::UnderEnemyTurret(landing) &&
                     !Bool(RMenu, "AllowTurret", false);
    context.PointClickLockdown = HasReadyPointClickThreatAt(landing);
    context.DashHazard = HasReadyDashHazardAt(landing);
    context.Lethal = lethal;
    context.Manual = manual;
    context.EnemiesAtLanding = Engine::CountEnemiesAt(landing, 650.0f);
    context.AlliesAtLanding = Engine::CountAlliesAt(landing, 700.0f);
    context.MaximumEnemies = Slider(RMenu, "MaxEnemies", 2);
    return SafeParanoiaCommit(context) &&
           Engine::PositionDangerScore(
               landing, target, Engine::ResolvedSpecs[3]) > -10000.0f;
}

inline AIHeroClient SelectCombatTarget(const AIHeroClient& preferred,
                                       float range) {
    if (Engine::ValidEnemy(preferred, range)) return preferred;
    const auto tether = TetherTarget();
    if (Engine::ValidEnemy(tether, range)) return tether;
    return Engine::SelectTarget(range);
}

inline bool PreservePassiveAttack(const AIHeroClient& target) {
    return PassiveReady && Bool(PassiveMenu, "PreserveAttack", true) &&
           Engine::ValidEnemy(target) && InAutoAttackRange(target, 35.0f) &&
           Orbwalker::CanAttack() && !target.IsDashing();
}

inline bool CastQ(const AIHeroClient& target,
                  Mode mode,
                  bool guaranteed = false) {
    if (!Engine::ValidEnemy(target, kQRange + 35.0f) || !Ready(0) ||
        !SpellEnabled(0, mode) || !CastThrottleReady(0) ||
        !HasMana(static_cast<float>(Slider(QMenu, "MinimumMana", 16))) ||
        (Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true)) ||
        (!guaranteed && PreservePassiveAttack(target))) {
        return false;
    }
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    SDK::HitChance needed = guaranteed || Engine::IsHardCrowdControlled(target)
        ? SDK::HitChance::Medium
        : SDK::HitChance::High;
    if (!PredictionAtLeast(prediction, needed) ||
        ProjectileWallBlocksFromPlayer(prediction.GetCastPosition(), 60.0f)) {
        return false;
    }
    const auto player = GameObjects::Player();
    const Vector3 endpoint = ClampQEndpoint(
        player.Position(), prediction.GetCastPosition());
    if (endpoint.IsZero()) return false;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    QCastTick = Now();
    QTargetId = static_cast<int>(target.NetworkId());
    QTrailStart = player.Position();
    QTrailEnd = endpoint;
    QTrailExpireTick = QCastTick + 5000;
    return true;
}

inline bool CastFleeQ() {
    if (!Ready(0) || !SpellEnabled(0, Mode::Flee) ||
        !CastThrottleReady(0) ||
        !HasMana(static_cast<float>(Slider(QMenu, "FleeMana", 12)))) {
        return false;
    }
    const auto player = GameObjects::Player();
    const Vector3 endpoint = ClampQEndpoint(player.Position(), Game::CursorPos());
    if (endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    QCastTick = Now();
    QTargetId = 0;
    QTrailStart = player.Position();
    QTrailEnd = endpoint;
    QTrailExpireTick = QCastTick + 5000;
    return true;
}

inline bool CastWReactive() {
    const int now = Now();
    if (IncomingSpellUntil <= now || WShieldActive || !Ready(1) ||
        !SpellEnabled(1, Mode::Automatic) ||
        !Bool(WMenu, "Automatic", true) || !CastThrottleReady(1, true) ||
        !HasMana(static_cast<float>(Slider(WMenu, "ReserveMana", 8)))) {
        return false;
    }
    const auto player = GameObjects::Player();
    const bool urgent = IncomingHardCrowdControl ||
        IncomingSpellImpactTick <= now + 450 ||
        player.HealthPercent() <= Slider(WMenu, "DamageShieldHp", 48);
    if (!urgent || !Engine::ControllerCastSelf(1)) return false;
    WCastTick = now;
    WShieldActive = true;
    WShieldExpireTick = now + 1500;
    IncomingSpellUntil = 0;
    return true;
}

inline bool CastE(const AIHeroClient& target,
                  Mode mode,
                  bool reactive = false) {
    if (!Engine::ValidEnemy(target, kETargetRange + target.BoundingRadius()) ||
        IsCommonUntargetableOrImmune(target) || !Ready(2) ||
        !SpellEnabled(2, mode) || !CastThrottleReady(2, reactive) ||
        !HasMana(static_cast<float>(Slider(EMenu, "MinimumMana", 14)))) {
        return false;
    }
    if (ETargetId != 0 && Now() <= ETetherExpireTick) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    ECastTick = Now();
    ETargetId = static_cast<int>(target.NetworkId());
    ETetherExpireTick = ECastTick + 2250;
    ETetherConfirmed = true;
    EFearConfirmed = false;
    return true;
}

inline bool CastRFirst(const AIHeroClient& target, bool manual) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RActive || !Ready(3) || !CastThrottleReady(3) ||
        !Engine::ValidEnemy(target, ParanoiaRange(SpellRank(3))) ||
        !HasMana(static_cast<float>(Slider(RMenu, "ReserveMana", 20))) ||
        !RLandingSafe(target, manual)) return false;
    const float distance = player.Position().Distance2D(target.Position());
    const bool lethal = Lethal(target, ExpectedFollowupDamage(target));
    if (!manual && distance < Slider(RMenu, "MinimumDistance", 700)) return false;
    if (!manual && !lethal &&
        target.HealthPercent() > Slider(RMenu, "TargetHp", 48)) return false;
    PendingControllerR = true;
    RControllerOwned = true;
    RTargetId = static_cast<int>(target.NetworkId());
    if (!Engine::ControllerCastSelf(3)) {
        PendingControllerR = false;
        RControllerOwned = false;
        RTargetId = 0;
        return false;
    }
    RCastTick = RFirstCastTick = Now();
    RActive = true;
    RDashIssued = false;
    RExpireTick = RFirstCastTick + 6000;
    PendingControllerR = false;
    return true;
}

inline bool CastRDash(const AIHeroClient& target, bool manual) {
    if (!RActive || RDashIssued || !RControllerOwned ||
        Now() < RFirstCastTick + 140 || !Ready(3) ||
        !CastThrottleReady(3, true) || !RLandingSafe(target, manual)) {
        return false;
    }
    PendingControllerR = true;
    if (!Engine::ControllerCastUnit(3, target)) {
        PendingControllerR = false;
        return false;
    }
    RCastTick = Now();
    RDashIssued = true;
    PendingControllerR = false;
    return true;
}

inline int NearbyPassiveUnits(bool jungle) {
    const auto player = GameObjects::Player();
    int count = 0;
    const auto& units = jungle ? GameObjects::Jungle()
                               : GameObjects::EnemyMinions();
    for (const auto& unit : units) {
        if (unit.IsValid() && !unit.IsDead() && unit.IsTargetable() &&
            PassiveCleaveHits(player.Position(), unit.Position(),
                              unit.BoundingRadius())) ++count;
    }
    return count;
}

inline bool TryFarmQ(Mode mode) {
    const bool jungle = mode == Mode::Jungle;
    if (!Ready(0) || !SpellEnabled(0, mode) || !CastThrottleReady(0) ||
        !HasMana(static_cast<float>(Slider(
            FarmMenu, jungle ? "JungleMana" : "LaneMana",
            jungle ? 20 : 42)))) return false;
    const auto player = GameObjects::Player();
    if (PassiveReady && player.HealthPercent() <
            Slider(PassiveMenu, "HealPriorityHp", 72) &&
        NearbyPassiveUnits(jungle) >= (jungle ? 1 : 2)) {
        return false;
    }
    const auto& units = jungle ? GameObjects::Jungle()
                               : GameObjects::EnemyMinions();
    Vector3 best{};
    int bestHits = 0;
    for (const auto& candidate : units) {
        if (!candidate.IsValid() || candidate.IsDead() ||
            !candidate.IsTargetable() ||
            player.Position().Distance2D(candidate.Position()) > kQRange + 50.0f) {
            continue;
        }
        const Vector3 endpoint = ClampQEndpoint(
            player.Position(), candidate.Position());
        int hits = 0;
        for (const auto& unit : units) {
            if (unit.IsValid() && !unit.IsDead() &&
                QPathHits(player.Position(), endpoint, unit.Position(),
                          unit.BoundingRadius())) ++hits;
        }
        if (hits > bestHits) {
            bestHits = hits;
            best = endpoint;
        }
    }
    const int minimum = jungle ? 1 : Slider(FarmMenu, "QHits", 3);
    if (bestHits < minimum || best.IsZero() ||
        !Engine::ControllerCastPosition(0, best)) return false;
    QCastTick = Now();
    QTargetId = 0;
    QTrailStart = player.Position();
    QTrailEnd = best;
    QTrailExpireTick = QCastTick + 5000;
    return true;
}

inline bool TryReactiveE() {
    int id = 0;
    if (GapcloserExpireTick > Now()) id = GapcloserTargetId;
    else if (InterruptExpireTick > Now()) id = InterruptTargetId;
    if (id == 0) return false;
    const auto target = HeroByNetworkId(id);
    return Engine::ValidEnemy(target, kETargetRange + 50.0f) &&
           CastE(target, Mode::Automatic, true);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = SelectCombatTarget(preferred, kQRange + 35.0f);
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, SpellDamage(2, target)) &&
        CastE(target, Mode::Automatic, true)) return true;
    if (Lethal(target, SpellDamage(0, target)) &&
        CastQ(target, Mode::Automatic, true)) return true;
    if (Bool(RMenu, "AutoExecute", true) && Ready(3)) {
        const auto globalTarget = Engine::ValidEnemy(preferred,
            ParanoiaRange(SpellRank(3)))
            ? preferred : Engine::SelectTarget(ParanoiaRange(SpellRank(3)));
        if (Engine::ValidEnemy(globalTarget) &&
            Lethal(globalTarget, ExpectedFollowupDamage(globalTarget))) {
            return CastRFirst(globalTarget, false);
        }
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& preferred) {
    const auto pursuer = ControllerHelpers::NearestEnemyToPlayer(preferred, 850.0f);
    if (Engine::ValidEnemy(pursuer, kETargetRange + 50.0f) &&
        CastE(pursuer, Mode::Flee, true)) return true;
    return CastFleeQ();
}

inline bool TryCombat(const AIHeroClient& preferred, Mode mode) {
    if (mode == Mode::Combo && !RActive && Ready(3) &&
        Bool(RMenu, "Combo", true)) {
        const float range = ParanoiaRange(SpellRank(3));
        const auto globalTarget = Engine::ValidEnemy(preferred, range)
            ? preferred : Engine::SelectTarget(range);
        if (Engine::ValidEnemy(globalTarget) &&
            CastRFirst(globalTarget, false)) return true;
    }
    const auto tether = TetherTarget();
    const auto target = Engine::ValidEnemy(tether, kQRange + 35.0f)
        ? tether : SelectCombatTarget(preferred, kQRange + 35.0f);
    if (!Engine::ValidEnemy(target)) return false;
    if (CastQ(target, mode, ETetherConfirmed || EFearConfirmed)) return true;
    if (CastE(target, mode, false)) return true;
    return false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    RefreshState();
    if (CastWReactive()) return true;

    const bool manualR = Key(RMenu, "ManualR", false);
    if (RActive) {
        if (!RControllerOwned) return false;
        auto target = HeroByNetworkId(RTargetId);
        if (!Engine::ValidEnemy(target, ParanoiaRange(SpellRank(3)))) {
            target = Engine::ValidEnemy(preferred, ParanoiaRange(SpellRank(3)))
                ? preferred : AIHeroClient{};
        }
        return Engine::ValidEnemy(target) && CastRDash(target, manualR);
    }
    if (manualR) {
        const auto target = Engine::ValidEnemy(preferred,
            ParanoiaRange(SpellRank(3)))
            ? preferred : Engine::SelectTarget(ParanoiaRange(SpellRank(3)));
        if (Engine::ValidEnemy(target) && CastRFirst(target, true)) return true;
    }
    if (Now() < PlayerOverrideUntil) return false;
    if (TryReactiveE()) return true;
    if (TryKillSecure(preferred)) return true;
    if (mode == Mode::Flee) return TryFlee(preferred);
    if (mode == Mode::Combo || mode == Mode::Harass) {
        return TryCombat(preferred, mode);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) {
        return TryFarmQ(mode);
    }
    return false;
}

inline void RecordEnemySpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (args.IsAutoAttack) return;
    const auto analysis = AnalyzeEnemyCast(
        args, 220.0f, 115.0f, 220, 250, 180, 1400, 1100);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) {
        return;
    }
    const auto player = GameObjects::Player();
    const float distance = analysis.Enemy.Position().Distance2D(player.Position());
    if (!analysis.LikelyHardCrowdControl && !analysis.TargetsPlayer &&
        distance > 1300.0f) return;
    const int now = Now();
    IncomingCasterId = static_cast<int>(analysis.Enemy.NetworkId());
    IncomingHardCrowdControl = analysis.LikelyHardCrowdControl;
    IncomingSpellUntil = now + 1150;
    const int castMs = ControllerHelpers::NormalizedCastDelayMs(args.CastDelay, 250);
    IncomingSpellImpactTick = now + std::clamp(castMs, 0, 1050);
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.IsAutoAttack) return;
    const bool controllerOwned = PendingControllerR ||
        (args.Slot >= 0 && args.Slot < 4 && Engine::WasControllerCast(args.Slot));
    if (!controllerOwned) PlayerOverrideUntil = now + 360;

    if (args.Slot == 0 || SpellEventNameContainsAny(
            args, { "nocturneduskbringer", "nocturneq" })) {
        QCastTick = now;
        QTrailStart = args.StartPosition.IsValid() && !args.StartPosition.IsZero()
            ? args.StartPosition : GameObjects::Player().Position();
        Vector3 end = args.CastPosition;
        if (!end.IsValid() || end.IsZero()) end = args.EndPosition;
        QTrailEnd = ClampQEndpoint(QTrailStart, end);
        QTrailExpireTick = now + 5000;
        QTargetId = static_cast<int>(args.TargetNetworkId);
    } else if (args.Slot == 1 || SpellEventNameContainsAny(
                   args, { "nocturneshroudofdarkness", "nocturnew" })) {
        WCastTick = now;
        WShieldActive = true;
        WShieldExpireTick = now + 1500;
    } else if (args.Slot == 2 || SpellEventNameContainsAny(
                   args, { "nocturneunspeakablehorror", "nocturnee" })) {
        ECastTick = now;
        ETargetId = static_cast<int>(args.TargetNetworkId);
        ETetherExpireTick = now + 2250;
        ETetherConfirmed = ETargetId != 0;
    } else if (args.Slot == 3 || SpellEventNameContainsAny(
                   args, { "nocturneparanoia", "paranoia2" })) {
        const bool dash = SpellEventNameContainsAny(
            args, { "paranoia2", "paranoiadash" }) ||
            (RActive && now > RFirstCastTick + 120);
        if (dash) {
            RDashIssued = true;
            RCastTick = now;
        } else {
            RActive = true;
            RDashIssued = false;
            RFirstCastTick = RCastTick = now;
            RExpireTick = now + 6000;
            RTargetId = static_cast<int>(args.TargetNetworkId);
            RControllerOwned = controllerOwned;
        }
    }
}

inline void OnProcessSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) ObserveLocalSpell(args);
    else RecordEnemySpell(args);
}

inline void OnDoCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    int target = 0;
    int tick = 0;
    if (CaptureLocalAutoAttack(args, target, tick)) {
        LastAutoTargetId = target;
        LastAutoTick = tick;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    int targetId = 0;
    int tick = 0;
    if (!CaptureAfterAttack(args, targetId, tick)) return;
    LastAutoTargetId = targetId;
    LastAutoTick = tick;
    const AIBaseClient target(args.Target.Address());
    const bool championOrMonster = target.IsHero() ||
        (target.IsMinion() && AIMinionClient(target.Address()).IsJungle());
    const bool consumed = PassiveReady;
    const float remaining = PassiveReadyTick > tick
        ? static_cast<float>(PassiveReadyTick - tick) / 1000.0f : 0.0f;
    const float next = PassiveCooldownAfterAttack(
        remaining, championOrMonster, consumed);
    PassiveReady = next <= 0.0f;
    PassiveReadyTick = PassiveReady ? 0 : tick + static_cast<int>(next * 1000.0f);
    PassiveLastAttackTick = tick;
    if (consumed) {
        PassiveLastHitCount = 1;
        const auto player = GameObjects::Player();
        auto count = [&](const auto& unit) {
            if (unit.IsValid() && !unit.IsDead() &&
                PassiveCleaveHits(target.Position(), unit.Position(),
                                  unit.BoundingRadius())) ++PassiveLastHitCount;
        };
        for (const auto& unit : GameObjects::EnemyMinions()) count(unit);
        for (const auto& unit : GameObjects::Jungle()) count(unit);
        for (const auto& unit : GameObjects::EnemyHeroes()) {
            if (unit.NetworkId() != target.NetworkId()) count(unit);
        }
        PassiveLastExpectedHeal = PassiveHealPerTarget(
            player.Level(), player.AP(), false) * PassiveLastHitCount;
    }
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    const int now = Now();
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) {
        if (BuffToken(args.BuffName,
                { "nocturneumbrablades", "nocturnepassive" })) {
            PassiveReady = added;
            if (added) PassiveReadyTick = 0;
        }
        if (BuffToken(args.BuffName,
                { "nocturneduskbringer", "nocturneqbuff" }) && added) {
            QTrailExpireTick = std::max(QTrailExpireTick,
                now + ControllerHelpers::RemainingMilliseconds(
                    args.EndTime, 5000, 250, 6500));
        }
        if (BuffToken(args.BuffName,
                { "nocturneshroudofdarkness", "nocturnewspellshield" })) {
            WShieldActive = added;
            WShieldExpireTick = added
                ? now + ControllerHelpers::RemainingMilliseconds(
                    args.EndTime, 1500, 100, 1800) : 0;
        }
        if (BuffToken(args.BuffName,
                { "nocturneshroudofdarknessattackspeed",
                  "nocturnewattackspeed" })) {
            WBlockSteroidActive = added;
            WSteroidExpireTick = added
                ? now + ControllerHelpers::RemainingMilliseconds(
                    args.EndTime, 5000, 250, 6000) : 0;
        }
        if (BuffToken(args.BuffName,
                { "nocturneparanoia", "paranoiadash" })) {
            if (added) {
                RActive = true;
                RExpireTick = now + ControllerHelpers::RemainingMilliseconds(
                    args.EndTime, 6000, 250, 7000);
            } else {
                ClearR();
            }
        }
        return;
    }
    if (id == ETargetId && BuffToken(args.BuffName,
            { "nocturneunspeakablehorror", "nocturnee" })) {
        ETetherConfirmed = added;
        if (added) ETetherExpireTick = now +
            ControllerHelpers::RemainingMilliseconds(
                args.EndTime, 2000, 100, 2600);
    }
    if (id == ETargetId && BuffToken(args.BuffName,
            { "nocturneunspeakablehorrorfear", "nocturneunspeakablehorror2" })) {
        EFearConfirmed = added;
        if (!added && !ETetherConfirmed) ClearTether();
    }
}





inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "Draw", true)) return;
    if (Bool(CoachMenu, "QRange", true)) {
        Drawing::DrawCircle(player.Position(), kQRange, 0xFF7150A8u, 1.5f, 48);
    }
    if (QTrailExpireTick > Now() && !QTrailStart.IsZero() &&
        !QTrailEnd.IsZero()) {
        Drawing::DrawLine(QTrailStart, QTrailEnd, 0xFF8E62D4u, 3.0f);
    }
    const auto tether = TetherTarget();
    if (Engine::ValidEnemy(tether) && ETetherExpireTick > Now()) {
        const bool safe = TetherMaintained(
            player.Position(), tether.Position(), tether.BoundingRadius());
        Drawing::DrawLine(player.Position(), tether.Position(),
            safe ? 0xFFCC66FFu : 0xFFFF5555u, 3.0f);
        Drawing::DrawCircle(player.Position(), kETetherRadius,
            0x88CC66FFu, 1.2f, 40);
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "NocturneOneTrick", "Nocturne one-trick mechanics"));
    PassiveMenu = TacticsMenu->AddSubMenu(new Menu(
        "UmbraBlades", "Umbra Blades cleave/heal"));
    PassiveMenu->Add(new MenuBool(
        "PreserveAttack", "Preserve ready passive AA over ordinary Q", true));
    PassiveMenu->Add(new MenuSlider(
        "HealPriorityHp", "Let passive cleave heal before farm Q below HP (%)",
        72, 20, 100));

    QMenu = TacticsMenu->AddSubMenu(new Menu(
        "Duskbringer", "Duskbringer trail"));
    QMenu->Add(new MenuSlider(
        "MinimumMana", "Minimum mana for combat Q (%)", 16, 0, 80));
    QMenu->Add(new MenuSlider(
        "FleeMana", "Minimum mana for flee Q (%)", 12, 0, 80));

    WMenu = TacticsMenu->AddSubMenu(new Menu(
        "Shroud", "Shroud of Darkness spell shield"));
    WMenu->Add(new MenuBool(
        "Automatic", "Shield targeted/crossing hostile abilities", true));
    WMenu->Add(new MenuSlider(
        "DamageShieldHp", "Shield non-CC damage below HP (%)", 48, 5, 100));
    WMenu->Add(new MenuSlider(
        "ReserveMana", "Minimum mana for W (%)", 8, 0, 50));

    EMenu = TacticsMenu->AddSubMenu(new Menu(
        "Horror", "Unspeakable Horror tether"));
    EMenu->Add(new MenuSlider(
        "MinimumMana", "Minimum mana for E (%)", 14, 0, 80));
    EMenu->Add(new MenuSeparator(
        "TetherOwnership", "Active tether target owns Q/E follow-up until fear"));

    RMenu = TacticsMenu->AddSubMenu(new Menu(
        "Paranoia", "Paranoia darkness/dash safety"));
    RMenu->Add(new MenuKeyBind(
        "ManualR", "Safe Paranoia engage [T]", SDK::Keys::T,
        KeyBindType::Press));
    RMenu->Add(new MenuBool("Combo", "Allow scored Combo R", true));
    RMenu->Add(new MenuBool(
        "AutoExecute", "Allow lethal automatic R", true));
    RMenu->Add(new MenuSlider(
        "TargetHp", "Combo R target max HP (%)", 48, 5, 100));
    RMenu->Add(new MenuSlider(
        "MinimumDistance", "Do not R when local actions already reach", 700,
        350, 1400));
    RMenu->Add(new MenuSlider(
        "MaxEnemies", "Maximum enemies at R landing", 2, 1, 5));
    RMenu->Add(new MenuSlider(
        "ReserveMana", "Minimum mana for R (%)", 20, 0, 80));
    RMenu->Add(new MenuBool(
        "AllowTurret", "Allow R landing under enemy turret", false));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "FarmLogic", "Passive-aware farm"));
    FarmMenu->Add(new MenuSlider(
        "QHits", "Minimum lane units hit by Q", 3, 1, 8));
    FarmMenu->Add(new MenuSlider(
        "LaneMana", "Minimum lane-clear mana (%)", 42, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "JungleMana", "Minimum jungle mana (%)", 20, 0, 100));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "Nocturne coach overlays"));
    CoachMenu->Add(new MenuBool("Draw", "Draw mechanics", true));
    CoachMenu->Add(new MenuBool("QRange", "Draw Q range", true));
}

inline void OnLoad() {
    PassiveReady = true;
    PassiveReadyTick = PassiveLastAttackTick = PassiveLastHitCount = 0;
    PassiveLastExpectedHeal = 0.0f;
    QTrailStart = QTrailEnd = {};
    QTrailExpireTick = QTargetId = QCastTick = 0;
    WShieldActive = WBlockSteroidActive = false;
    WShieldExpireTick = WSteroidExpireTick = WCastTick = 0;
    IncomingSpellUntil = IncomingSpellImpactTick = IncomingCasterId = 0;
    IncomingHardCrowdControl = false;
    ClearTether();
    ECastTick = 0;
    ClearR();
    RCastTick = 0;
    LastAutoTargetId = LastAutoTick = PlayerOverrideUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    RefreshState();
}

inline void OnUnload() {
    TacticsMenu = PassiveMenu = QMenu = WMenu = EMenu = RMenu = nullptr;
    FarmMenu = CoachMenu = nullptr;
    ClearTether();
    ClearR();
}

inline constexpr const char* Scenarios[] = {
    "Reconcile passive readiness from buff events and a 13-second polling clock",
    "Reduce passive cooldown by one second on minions and three on champions/monsters",
    "Model the 360-radius cleave and level/AP heal including reduced secondary-minion value",
    "Preserve a ready healing cleave before ordinary combat or farm Q",
    "Cast Q with prediction and projectile-wall rejection but no unit collision",
    "Track the five-second Q trail from both controller and manual casts",
    "Use Q trail direction toward the active fear-tether target",
    "Use W only for a targeted or player-crossing hostile spell",
    "Prioritize hard crowd control and low-health damage windows for W",
    "Reconcile W shield and successful-block attack-speed steroid from buffs and polling",
    "Apply E only inside its 425 target range and track the 465 break radius",
    "Keep Q/E decisions on the active tether target until fear or break",
    "Use E against committed gapclosers and interruptible targets before ordinary damage",
    "Start R darkness only for a target inside the current rank-scaled range",
    "Reject R when local actions already reach the target",
    "Reject R landing into wall, turret, excess enemies or ready lockdown",
    "Require ally parity or lethal follow-up at the R landing",
    "Recheck landing safety immediately before the R dash recast",
    "Never auto-recast a player-started Paranoia darkness window",
    "Allow the manual R key through the same range and safety gates",
    "Prefer the selected target while it has a currently reachable route",
    "Honor Combo, Harass, LaneClear, Jungle, LastHit and Flee mode ownership",
    "Use passive-aware Q line scoring for lane and jungle clear",
    "Respect spell readiness, mana floors, attack windup and cast throttles",
    "Observe manual Q/W/E/R and pause controller decisions rather than overwrite them",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Nocturne";
    controller.ControllerId = "champion.kuroaio.ai.nocturne.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AINocturne.md";
    controller.ImplementationSummary =
        "Passive cleave/heal clock and AA preservation; prediction-backed Q "
        "trail; event-timed W shield; polled 465 tether; rank-scaled two-stage "
        "Paranoia with landing danger, ally parity and manual ownership.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardExpiringBuffStateEvent<&UpdateBuffState>;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEnd, &GapcloserExpireTick, 850, 1100>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 650, 220, 2600>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Nocturne
