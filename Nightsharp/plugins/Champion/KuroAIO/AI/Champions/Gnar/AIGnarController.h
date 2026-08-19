#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIGnarGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <initializer_list>

namespace Plugins::KuroAIO::AI::Controllers::Gnar {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::OrbwalkerHeroTarget;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* RageMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline FormState Form = FormState::Mini;
inline float Fury = 0.0f;
inline int FormLastObservedTick = 0;
inline int MegaExpireTick = 0;
inline int TiredExpireTick = 0;
inline int PlayerOverrideUntil = 0;
inline int LastLocalSpellTick = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int QMissileNetworkId = 0;
inline bool QMissileMega = false;
inline bool QInFlight = false;
inline int QFlightExpireTick = 0;
inline int LastQPickupTick = 0;
inline int BoulderObjectId = 0;
inline Vector3 BoulderPosition = {};
inline int BoulderExpireTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int GapcloserTargetId = 0;
inline Vector3 GapcloserEndpoint = {};
inline int GapcloserExpireTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int IncomingThreatUntil = 0;
inline int LastDecisionTargetId = 0;

inline bool Mega() { return Form == FormState::Mega; }

inline bool CanUse(int index, Mode mode, bool reactive = false) {
    if (!Ready(index)) return false;
    return reactive || SpellEnabled(index, mode);
}

inline bool IsTransformSoonName(const char* name) {
    return Engine::TextContains(name, "GnarTransformSoon");
}

inline bool IsTransformTiredName(const char* name) {
    return Engine::TextContains(name, "GnarTransformTired");
}

inline bool IsMegaTransformName(const char* name) {
    return Engine::TextContains(name, "GnarTransform") &&
           !IsTransformSoonName(name) && !IsTransformTiredName(name);
}

inline bool IsMiniQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellEventNameContainsAny(args, { "GnarQ", "GnarQMissile" }) &&
           !SpellEventNameContainsAny(args, { "GnarBigQ" });
}

inline bool IsMegaQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellEventNameContainsAny(args, { "GnarBigQ", "GnarBigQMissile" });
}

inline bool IsMegaWEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellEventNameContainsAny(args, { "GnarBigW" });
}

inline bool IsEEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellEventNameContainsAny(args, { "GnarE", "GnarBigE" });
}

inline bool IsREvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellEventNameContainsAny(args, { "GnarR" });
}

inline bool IsQMissile(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName,
          args.SpellName, args.MissileName },
        { "GnarQMissile", "Gnar_Q_mis", "GnarBigQMissile",
          "GnarBig_Q_Rock" });
}

inline bool IsMegaQMissile(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName,
          args.SpellName, args.MissileName },
        { "GnarBigQ", "GnarBig_Q_Rock" });
}

inline bool IsBoulderObject(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName, args.SpellName },
        { "GnarBigQ", "GnarBoulder", "Gnar_Q_Rock" });
}

inline bool DisplacementImmune(const AIHeroClient& target) {
    return ControllerHelpers::HasAnyBuff(target, {
        "OlafRagnarok", "SionR", "MalphiteR", "ViR", "WarwickR",
        "HecarimUlt", "VolibearR", "ShyvanaTransform",
        "UdyrE2Activation", "KSanteRTransform"
    });
}

inline void RefreshQCollision() {
    if (!Engine::RuntimeSpells[0]) return;
    Engine::RuntimeSpells[0]->SetCollisionObjects(
        SDK::CollisionableObjects::Minions |
        SDK::CollisionableObjects::Heroes |
        SDK::CollisionableObjects::YasuoWall);
}

inline void ReconcileForm() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Fury = std::clamp(player.Mana(), 0.0f, 100.0f);
    const int now = Now();
    const bool megaBuff = player.HasBuff("GnarTransform");
    const bool soonBuff = player.HasBuff("GnarTransformSoon");
    const bool tiredBuff = player.HasBuff("GnarTransformTired");
    TransformObservation observation{};
    observation.MegaBuff = megaBuff;
    observation.TransformSoonBuff = soonBuff;
    observation.TiredBuff = tiredBuff;
    observation.Fury = Fury;
    observation.Previous = Form;
    observation.MegaGraceActive = Form == FormState::Mega && MegaExpireTick > now;
    Form = ResolveTransformState(observation);
    if (megaBuff) {
        MegaExpireTick = std::max(MegaExpireTick, now + 450);
        TiredExpireTick = 0;
    }
    if (tiredBuff) {
        TiredExpireTick = std::max(TiredExpireTick, now + 450);
        MegaExpireTick = 0;
    }
    if (Form == FormState::Mega && MegaExpireTick <= now) {
        Form = tiredBuff ? FormState::Tired :
               (soonBuff || Fury >= 100.0f ? FormState::TransformReady
                                           : FormState::Mini);
    }
    if (Form == FormState::Tired && !tiredBuff && TiredExpireTick <= now) {
        Form = soonBuff || Fury >= 100.0f
            ? FormState::TransformReady : FormState::Mini;
    }
    FormLastObservedTick = now;
    if (QFlightExpireTick < now) {
        QInFlight = false;
        QMissileNetworkId = 0;
    }
    if (BoulderExpireTick < now) {
        BoulderObjectId = 0;
        BoulderPosition = {};
    }
    RefreshQCollision();
}

inline AIHeroClient SelectTarget(const AIHeroClient& preferred,
                                 float range = 1300.0f) {
    if (Engine::ValidEnemy(preferred, range)) return preferred;
    const auto orbwalker = OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orbwalker, range)) return orbwalker;
    return NearestEnemyToPlayer(preferred, range);
}

inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || IsCommonUntargetableOrImmune(target) ||
           ControllerHelpers::HasSpellShieldOrImmunity(target);
}

inline bool QPrediction(const AIHeroClient& target,
                        SDK::PredictionOutput& prediction) {
    if (!Engine::RuntimeSpells[0]) return false;
    const SDK::HitChance required = target.IsDashing() ||
            Engine::IsHardCrowdControlled(target)
        ? SDK::HitChance::Medium : SDK::HitChance::High;
    const bool hits = PredictionHits(0, target, required, false, &prediction);
    if (!hits || !prediction.GetCastPosition().IsValid() ||
        prediction.GetCastPosition().IsZero()) return false;
    QReachContext reach{};
    reach.Form = Form;
    reach.Distance = GameObjects::Player().Position().Distance2D(
        prediction.GetUnitPosition());
    reach.TargetRadius = target.BoundingRadius();
    reach.PredictionAccepted = true;
    reach.ProjectileWall = PredictionProjectileWall(
        0, prediction, Mega() ? kMegaQWidth * 0.5f : kMiniQWidth * 0.5f);
    reach.InterveningBody = Mega() && !prediction.CollisionObjects.empty();
    return QCanReach(reach);
}

inline bool CastQ(const AIHeroClient& target, Mode mode,
                  bool reactive = false) {
    if (!CanUse(0, mode, reactive) || TargetBlocked(target) ||
        Now() - LastQCastTick < 45) return false;
    SDK::PredictionOutput prediction{};
    if (!QPrediction(target, prediction)) return false;
    const bool lethal = SpellDamage(0, target) >=
                        target.Health() + target.AllShield();
    if (Orbwalker::IsWindingUp() && !lethal && !reactive) return false;
    if (!Engine::ControllerCastPosition(0, prediction.GetCastPosition())) return false;
    LastQCastTick = Now();
    QInFlight = true;
    QMissileMega = Mega();
    QFlightExpireTick = LastQCastTick + (Mega() ? 1400 : 2600);
    return true;
}

inline bool CastWallop(const AIHeroClient& target, Mode mode,
                       bool reactive = false) {
    if (!Mega() || !CanUse(1, mode, reactive) || TargetBlocked(target) ||
        Now() - LastWCastTick < 60) return false;
    const Vector3 aim = PredictPosition(target, 0.60f);
    const auto player = GameObjects::Player();
    if (!aim.IsValid() || aim.IsZero() ||
        !WallopHits(player.Position(), aim, aim, target.BoundingRadius())) return false;
    const bool lethal = SpellDamage(1, target) >=
                        target.Health() + target.AllShield();
    if (Orbwalker::IsWindingUp() && !lethal && !reactive) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastWCastTick = Now();
    return true;
}

inline bool EndpointSafe(const Vector3& endpoint,
                         const AIHeroClient& threat,
                         bool defensive,
                         bool lethal,
                         bool bouncePossible,
                         const Vector3& bounceEndpoint) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero()) return false;
    const bool currentTurret = Engine::UnderEnemyTurret(player.Position());
    const bool endpointTurret = Engine::UnderEnemyTurret(endpoint) && !currentTurret;
    bool bounceSafe = true;
    if (bouncePossible && bounceEndpoint.IsValid() && !bounceEndpoint.IsZero()) {
        bounceSafe = !SDK::NavMesh::IsWall(bounceEndpoint) &&
                     (!Engine::UnderEnemyTurret(bounceEndpoint) ||
                      currentTurret || defensive) &&
                     !HasReadyDashHazardAt(bounceEndpoint) &&
                     !HasReadyPointClickThreatAt(bounceEndpoint) &&
                     Engine::CountEnemiesAt(bounceEndpoint, 600.0f) <=
                         Engine::CountAlliesAt(bounceEndpoint, 750.0f) + 1;
    }
    MobilityContext context{};
    context.EndpointValid = true;
    context.EndpointWalkable = !SDK::NavMesh::IsWall(endpoint);
    context.EndpointEnemyTurret = endpointTurret;
    context.EndpointDashHazard = HasReadyDashHazardAt(endpoint);
    context.EndpointPointClickThreat = HasReadyPointClickThreatAt(endpoint);
    context.BouncePossible = bouncePossible;
    context.BounceEndpointSafe = bounceSafe;
    context.Defensive = defensive;
    context.Lethal = lethal;
    context.EscapesThreat = defensive && Engine::ValidEnemy(threat) &&
        endpoint.Distance2D(threat.Position()) >
            player.Position().Distance2D(threat.Position());
    context.CursorAgrees = CursorDirectionAgrees(endpoint, -0.12f);
    context.EnemiesAtEndpoint = Engine::CountEnemiesAt(endpoint, 600.0f);
    context.AlliesAtEndpoint = Engine::CountAlliesAt(endpoint, 750.0f);
    context.MaximumCommitEnemies = Slider(EMenu, "MaxEndpointEnemies", 2);
    return MobilitySafe(context);
}

inline bool CastMobility(const AIHeroClient& target, Mode mode,
                         bool defensive, bool reactive = false) {
    if (!CanUse(2, mode, reactive) || Now() - LastECastTick < 90 ||
        ControllerHelpers::PlayerMobilityLocked()) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    Vector3 requested = defensive ? Game::CursorPos() :
        (Engine::ValidEnemy(target) ? PredictPosition(target, 0.35f)
                                    : Game::CursorPos());
    if (!requested.IsValid() || requested.IsZero()) return false;
    const bool mega = Mega();
    const Vector3 endpoint = mega
        ? CrunchLanding(player.Position(), requested)
        : HopLanding(player.Position(), requested);
    const Vector3 bounceEndpoint = mega ? Vector3{} :
        HopBounceLanding(player.Position(), endpoint, requested);
    const bool lethal = Engine::ValidEnemy(target) &&
        SpellDamage(2, target) >= target.Health() + target.AllShield();
    if (!EndpointSafe(endpoint, target, defensive, lethal, !mega,
                      bounceEndpoint)) return false;
    if (!defensive && InAutoAttackRange(target) && !lethal) return false;
    if (!Engine::ControllerCastPosition(2, requested)) return false;
    LastECastTick = Now();
    return true;
}

inline bool KnockbackFindsWall(const Vector3& targetPosition,
                               Vector3& endpoint) {
    const auto player = GameObjects::Player();
    endpoint = KnockbackEndpoint(player.Position(), targetPosition);
    if (!endpoint.IsValid() || endpoint.IsZero()) return false;
    Vector3 wall{};
    return SDK::NavMesh::FindWallCollision(
               targetPosition, endpoint, wall, 8.0f) ||
           SDK::NavMesh::IsWall(endpoint);
}

inline bool CastUltimate(const AIHeroClient& target, Mode mode,
                         bool defensive, bool interrupt,
                         bool manual = false) {
    if (!Mega() || !CanUse(3, mode, defensive || interrupt || manual) ||
        TargetBlocked(target) || Now() - LastRCastTick < 140) return false;
    const auto player = GameObjects::Player();
    const Vector3 predicted = PredictPosition(target, 0.25f);
    if (!predicted.IsValid() || predicted.IsZero()) return false;
    Vector3 endpoint{};
    const bool wall = KnockbackFindsWall(predicted, endpoint);
    const bool endpointSafe = endpoint.IsValid() && !endpoint.IsZero() &&
        (!Engine::UnderEnemyTurret(player.Position()) || defensive || wall);
    const bool lethal = GnarDamage(
        SpellRank(3), player.TotalAttackDamage(), player.AP(), wall) >=
        target.Health() + target.AllShield();
    UltimateContext context{};
    context.Mega = true;
    context.Ready = true;
    context.TargetInRadius = player.Position().Distance2D(predicted) <=
                             kGnarRadius + target.BoundingRadius();
    context.TargetDamageable = !IsCommonUntargetableOrImmune(target) &&
                               !ControllerHelpers::HasSpellShieldOrImmunity(target);
    context.TargetDisplacementImmune = DisplacementImmune(target);
    context.WallCollision = wall;
    context.Lethal = lethal;
    context.Defensive = defensive;
    context.Interrupt = interrupt;
    context.EndpointSafe = endpointSafe;
    context.FollowupReady = Ready(1) || Ready(0);
    context.ChampionHits = Engine::CountEnemiesAt(player.Position(), kGnarRadius);
    context.MinimumChampionHits = Slider(RMenu, "MinimumTargets", 2);
    if (!manual && !MayCastGnar(context)) return false;
    if (manual && (!context.TargetInRadius || !context.TargetDamageable ||
                   context.TargetDisplacementImmune)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastRCastTick = Now();
    return true;
}

inline bool TryManualR(const AIHeroClient& selected) {
    if (!Key(RMenu, "ManualR", false)) return false;
    const AIHeroClient target = SelectTarget(selected, kGnarRadius + 100.0f);
    return Engine::ValidEnemy(target) &&
           CastUltimate(target, Mode::Automatic, false, false, true);
}

inline bool TryMegaCombo(const AIHeroClient& target) {
    if (CastUltimate(target, Mode::Combo, false, false)) return true;
    if (CastWallop(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    return Bool(EMenu, "ComboCrunch", true) &&
           CastMobility(target, Mode::Combo, false);
}

inline bool TryMiniCombo(const AIHeroClient& target) {
    if (CastQ(target, Mode::Combo)) return true;
    return Bool(EMenu, "ComboHop", false) &&
           target.HealthPercent() <= Slider(EMenu, "HopTargetHealth", 24) &&
           CastMobility(target, Mode::Combo, false);
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Mega() && Bool(WMenu, "HarassWallop", true) &&
        CastWallop(target, Mode::Harass)) return true;
    return CastQ(target, Mode::Harass);
}

inline bool TryFarm(Mode mode) {
    const bool lane = mode == Mode::LaneClear;
    const bool jungle = mode == Mode::Jungle;
    const bool lastHit = mode == Mode::LastHit;
    if (!lane && !jungle && !lastHit) return false;
    RageContext rage{};
    rage.Form = Form;
    rage.Fury = Fury;
    rage.ChampionNearby = Engine::CountEnemiesAt(
        GameObjects::Player().Position(), 1250.0f) > 0;
    rage.ObjectiveFight = jungle;
    rage.LaneClear = lane;
    rage.LastHit = lastHit;
    rage.DesiredMegaWindow = Bool(RageMenu, "PrepareMega", true) &&
                             Fury >= Slider(RageMenu, "PrepareAtFury", 72);
    if (!MayGenerateRage(rage)) return false;
    return Engine::TryFarm(mode);
}

inline bool TryFlee(const AIHeroClient& target) {
    if (Mega() && Engine::ValidEnemy(target)) {
        const int nearby = Engine::CountEnemiesAt(
            GameObjects::Player().Position(), kGnarRadius);
        if (nearby >= Slider(RMenu, "FleeMinimumTargets", 2) &&
            CastUltimate(target, Mode::Flee, true, false)) return true;
        if (CastWallop(target, Mode::Flee, true)) return true;
    }
    return CastMobility(target, Mode::Flee, true, true);
}

inline bool TryAutomatic(const AIHeroClient& selected) {
    const int now = Now();
    AIHeroClient gapTarget = GapcloserExpireTick >= now
        ? HeroByNetworkId(GapcloserTargetId) : AIHeroClient{};
    AIHeroClient interruptTarget = InterruptExpireTick >= now
        ? HeroByNetworkId(InterruptTargetId) : AIHeroClient{};
    const bool defensive = Engine::ValidEnemy(gapTarget) || IncomingThreatUntil >= now;
    const bool interrupt = Engine::ValidEnemy(interruptTarget);
    AIHeroClient target = interrupt ? interruptTarget :
        (Engine::ValidEnemy(gapTarget) ? gapTarget : SelectTarget(selected, 1250.0f));
    bool killSecure = Engine::ValidEnemy(target) && Ready(0) &&
        SpellDamage(0, target) >= target.Health() + target.AllShield();
    AutomaticContext gate{};
    gate.ManualOwnership = PlayerOverrideUntil >= now;
    gate.Defensive = defensive;
    gate.Interrupt = interrupt;
    gate.KillSecure = killSecure;
    gate.EndpointKnownSafe = true;
    if (!AutomaticAllowed(gate) || !Engine::ValidEnemy(target)) return false;
    if (Mega()) {
        if ((defensive || interrupt) &&
            CastUltimate(target, Mode::Automatic, defensive, interrupt)) return true;
        if ((defensive || interrupt) &&
            CastWallop(target, Mode::Automatic, true)) return true;
    } else if (defensive && Bool(EMenu, "AutomaticEscape", true) &&
               CastMobility(target, Mode::Automatic, true, true)) return true;
    return killSecure && CastQ(target, Mode::Automatic, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileForm();
    const AIHeroClient target = SelectTarget(selected, 1350.0f);
    LastDecisionTargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : 0;
    if (TryManualR(selected)) return true;
    if (PlayerOverrideUntil >= Now()) return true;
    if (mode == Mode::Automatic) {
        (void)TryAutomatic(selected);
    } else if (mode == Mode::Flee) {
        (void)TryFlee(target);
    } else if (mode == Mode::Combo && Engine::ValidEnemy(target)) {
        (void)(Mega() ? TryMegaCombo(target) : TryMiniCombo(target));
    } else if (mode == Mode::Harass) {
        (void)TryHarass(target);
    } else if (mode == Mode::LaneClear || mode == Mode::Jungle ||
               mode == Mode::LastHit) {
        (void)TryFarm(mode);
    }
    return true;
}

inline void ObserveLocalSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    int slot = args.Slot;
    if (IsMiniQEvent(args) || IsMegaQEvent(args)) slot = 0;
    else if (IsMegaWEvent(args)) slot = 1;
    else if (IsEEvent(args)) slot = 2;
    else if (IsREvent(args)) slot = 3;
    if (slot < 0 || slot >= 4) return;
    const int now = Now();
    const bool controllerOwned = Engine::WasControllerCast(slot);
    LastLocalSpellTick = now;
    if (slot == 0) {
        LastQCastTick = now;
        QInFlight = true;
        QMissileMega = IsMegaQEvent(args) || Mega();
        QFlightExpireTick = now + (QMissileMega ? 1400 : 2600);
        if (IsMegaQEvent(args)) {
            Form = FormState::Mega;
            MegaExpireTick = std::max(MegaExpireTick, now + kMegaDurationMs);
        }
    } else if (slot == 1) {
        LastWCastTick = now;
        if (IsMegaWEvent(args)) {
            Form = FormState::Mega;
            MegaExpireTick = std::max(MegaExpireTick, now + kMegaDurationMs);
        }
    } else if (slot == 2) {
        LastECastTick = now;
        if (SpellEventNameContainsAny(args, { "GnarBigE" })) {
            Form = FormState::Mega;
            MegaExpireTick = std::max(MegaExpireTick, now + kMegaDurationMs);
        }
    } else if (slot == 3) {
        LastRCastTick = now;
        Form = FormState::Mega;
        MegaExpireTick = std::max(MegaExpireTick, now + kMegaDurationMs);
    }
    if (!controllerOwned) {
        PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 520);
    }
}

inline void ObserveEnemySpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid()) return;
    const bool aimedNearPlayer =
        (args.EndPosition.IsValid() && !args.EndPosition.IsZero() &&
         args.EndPosition.Distance2D(player.Position()) <= 360.0f) ||
        (args.CastPosition.IsValid() && !args.CastPosition.IsZero() &&
         args.CastPosition.Distance2D(player.Position()) <= 360.0f);
    if (aimedNearPlayer) IncomingThreatUntil = Now() + 900;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) ObserveLocalSpell(args);
    else ObserveEnemySpell(args);
}

inline void UpdateTransformBuff(const SDK::Events::BuffEventArgs& args,
                                bool added) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (IsTransformTiredName(args.BuffName)) {
        if (added) {
            Form = FormState::Tired;
            TiredExpireTick = now + kTiredDurationMs;
            MegaExpireTick = 0;
        } else if (Form == FormState::Tired) {
            Form = Fury >= 100.0f ? FormState::TransformReady : FormState::Mini;
            TiredExpireTick = 0;
        }
        return;
    }
    if (IsTransformSoonName(args.BuffName)) {
        if (added && Form != FormState::Mega) Form = FormState::TransformReady;
        else if (!added && Form == FormState::TransformReady) Form = FormState::Mini;
        return;
    }
    if (IsMegaTransformName(args.BuffName)) {
        if (added) {
            Form = FormState::Mega;
            MegaExpireTick = now + kMegaDurationMs;
            TiredExpireTick = 0;
        } else if (Form == FormState::Mega) {
            Form = FormState::Tired;
            MegaExpireTick = 0;
            TiredExpireTick = now + kTiredDurationMs;
        }
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) || !IsQMissile(args)) return;
    QMissileNetworkId = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    QMissileMega = IsMegaQMissile(args) || Mega();
    QInFlight = true;
    QFlightExpireTick = Now() + (QMissileMega ? 1500 : 3000);
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!IsQMissile(args)) return;
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (QMissileNetworkId != 0 && id != QMissileNetworkId) return;
    const auto player = GameObjects::Player();
    if (player.IsValid() && args.Sender.Position.IsValid() &&
        args.Sender.Position.Distance2D(player.Position()) <=
            kBoulderPickupRadius + player.BoundingRadius()) LastQPickupTick = Now();
    QMissileNetworkId = 0;
    QInFlight = false;
    QFlightExpireTick = 0;
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !IsBoulderObject(args)) return;
    BoulderObjectId = static_cast<int>(args.Sender.NetworkId);
    BoulderPosition = args.Sender.Position;
    BoulderExpireTick = Now() + static_cast<int>(kBoulderLifetimeSeconds * 1000.0f);
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id != BoulderObjectId && !IsBoulderObject(args)) return;
    const auto player = GameObjects::Player();
    if (player.IsValid() && args.Sender.Position.IsValid() &&
        args.Sender.Position.Distance2D(player.Position()) <=
            kBoulderPickupRadius + player.BoundingRadius()) LastQPickupTick = Now();
    BoulderObjectId = 0;
    BoulderPosition = {};
    BoulderExpireTick = 0;
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), Mega() ? kMegaQRange : kMiniQRange,
                        Mega() ? 0xFFE67C55u : 0xFF65C7F2u, 1.5f, 48);
    if (Mega() && Ready(3)) {
        Drawing::DrawCircle(player.Position(), kGnarRadius,
                            0xFFFFD166u, 2.0f, 40);
    }
    if (BoulderObjectId != 0 && BoulderPosition.IsValid()) {
        Drawing::DrawCircle(BoulderPosition, kBoulderPickupRadius,
                            0xFF9BD66Fu, 1.5f, 32);
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "GnarOneTrick", "Gnar transform and displacement tactics"));
    TacticsMenu->Add(new MenuSlider(
        "ManualOwnershipMs", "Yield player spell (ms)", 520, 180, 1200));
    RageMenu = TacticsMenu->AddSubMenu(new Menu("Rage", "Transform preparation"));
    RageMenu->Add(new MenuBool("PrepareMega", "Build rage for nearby objectives", true));
    RageMenu->Add(new MenuSlider("PrepareAtFury", "Prepare Mega from fury", 72, 40, 95));
    QMenu = TacticsMenu->AddSubMenu(new Menu(
        "BoomerangBoulder", "Q prediction and pickup"));
    QMenu->Add(new MenuBool("RespectFirstBody", "Boulder must hit first body", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu(
        "HyperWallop", "Hyper spacing and Mega stun"));
    WMenu->Add(new MenuBool("HarassWallop", "Use Wallop in harass", true));
    EMenu = TacticsMenu->AddSubMenu(new Menu("HopCrunch", "Mobility endpoint safety"));
    EMenu->Add(new MenuBool("ComboCrunch", "Use safe Crunch follow-up", true));
    EMenu->Add(new MenuBool("ComboHop", "Allow lethal Mini Hop", false));
    EMenu->Add(new MenuSlider("HopTargetHealth", "Mini Hop target HP <=", 24, 5, 55));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 1, 5));
    EMenu->Add(new MenuBool("AutomaticEscape", "Automatic safe anti-dive Hop", true));
    RMenu = TacticsMenu->AddSubMenu(new Menu(
        "GnarUltimate", "GNAR! wall and peel policy"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum proactive targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("FleeMinimumTargets", "Minimum flee targets", 2, 1, 5));
    RMenu->Add(new MenuKeyBind("ManualR", "Manual safe GNAR! [T]",
                               SDK::Keys::T, KeyBindType::Press));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Rage-aware lane, jungle and last-hit"));
    FarmMenu->Add(new MenuSeparator(
        "Policy", "Lane clear pauses before an unplanned transform"));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "Form and pickup visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw current Q, R and boulder", false));
}

inline void OnLoad() {
    Form = FormState::Mini;
    Fury = 0.0f;
    FormLastObservedTick = MegaExpireTick = TiredExpireTick = 0;
    PlayerOverrideUntil = LastLocalSpellTick = 0;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    QMissileNetworkId = 0;
    QMissileMega = QInFlight = false;
    QFlightExpireTick = LastQPickupTick = 0;
    BoulderObjectId = BoulderExpireTick = 0;
    BoulderPosition = {};
    LastAfterAttackTargetId = LastAfterAttackTick = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = IncomingThreatUntil = 0;
    LastDecisionTargetId = 0;
    ReconcileForm();
}

inline void OnUnload() {
    TacticsMenu = RageMenu = QMenu = WMenu = EMenu = RMenu = nullptr;
    FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Use Riot 26.15 and CommunityDragon 16.15 Summoner's Rift data",
    "Resolve Mini, transform-ready, Mega and tired states from buffs and fury",
    "Reconcile transformation state through buff events, spell events and polling",
    "Preserve the 15 second Mega and tired transition windows conservatively",
    "Use distinct Mini boomerang and Mega boulder reach, width and missile speed",
    "Allow Mini Q continuation while requiring Mega boulder first-body clearance",
    "Reject Q through projectile denial and low-confidence prediction",
    "Track boomerang and boulder missile lifecycle and observed pickups",
    "Model Mini 40 percent and Mega 70 percent Q catch cooldown refunds",
    "Use Hyper as attack-owned passive damage and Wallop only in Mega form",
    "Predict Wallop's 550 by 200 line before committing the stun",
    "Clamp Hop to 475 and Crunch to 675 range",
    "Validate both first and possible bounce endpoints for Mini Hop",
    "Reject new turret, wall, dash-hazard and point-click-lockdown mobility endpoints",
    "Allow defensive mobility only when it increases separation or is already safe",
    "Project GNAR! targets 500 units away from Gnar and detect wall contact",
    "Require lethal, defensive, interrupt or multi-target follow-up value for GNAR!",
    "Reject GNAR! against displacement immunity or spell immunity",
    "Preserve selected targets, then orbwalker targets, then reachable fallback",
    "Preserve auto-attack windup unless the spell is lethal or reactive",
    "Combo uses Mini poke or Mega wall-control sequence without blind form forcing",
    "Harass avoids Hop, Crunch and GNAR! commitment",
    "LaneClear respects unplanned high-rage transform economy",
    "Jungle and LastHit retain explicit controller-owned mode paths",
    "Flee uses Mega peel before a verified safe Hop or Crunch",
    "Automatic mode performs only defense, interrupt or verified kill-secure actions",
    "Automatic Mini anti-dive mobility requires a known-safe endpoint",
    "Yield the immediate decision window after every player-owned spell",
    "Manual GNAR! remains player requested but still rejects known invalid targets",
    "Never automate Flash, summoner spells, items or movement orders"
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Gnar;
    controller.ControllerId = "champion.kuroaio.ai.gnar.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIGnar.md";
    controller.ImplementationSummary =
        "Mini/Mega rage reconciliation, return-projectile cooldown tracking, "
        "safe Hop/Crunch endpoints and wall-aware GNAR! displacement.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateTransformBuff, true>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateTransformBuff, false>;

    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAfterAttackTargetId, &LastAfterAttackTick>;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 650, 1100>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Gnar
