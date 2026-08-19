#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "../../Profiles/AILux.h"
#include "AILuxGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Lux {

using namespace Geometry;
using MarksmanControllerHelpers::CanUse;
using MarksmanControllerHelpers::ManualUltimatePressed;
using MarksmanControllerHelpers::PredictionHits;
using MarksmanControllerHelpers::PredictionProjectileWall;
using MarksmanControllerHelpers::SpellDamage;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureBeforeAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerSelectedEnemy;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellEventNameContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* UltimateMenu = nullptr;

inline std::array<int, 48> MarkIds{};
inline std::array<int, 48> MarkExpiry{};
inline std::array<bool, 48> MarkConfirmed{};
inline int PendingMarkTargetId = 0;
inline int PendingMarkUntil = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastWTargetId = 0;
inline int WReturnUntil = 0;
inline bool WReturnExpected = false;
inline bool WShieldObserved = false;
inline int ETargetId = 0;
inline EZoneState EZone{};
inline bool RChannelActive = false;
inline bool RManualStarted = false;
inline int RChannelUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserUntil = 0;
inline Vector3 GapcloserEndpoint{};

inline int MarkIndex(int networkId, bool create = true) {
    if (networkId == 0) return -1;
    for (std::size_t i = 0; i < MarkIds.size(); ++i) {
        if (MarkIds[i] == networkId) return static_cast<int>(i);
    }
    if (!create) return -1;
    for (std::size_t i = 0; i < MarkIds.size(); ++i) {
        if (MarkIds[i] == 0) {
            MarkIds[i] = networkId;
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline bool HasIllumination(const AIBaseClient& target) {
    return target.IsValid() &&
        (target.HasBuff("LuxIllumination") ||
         target.HasBuff("LuxIlluminationPassive") ||
         target.HasBuff("LuxPMark"));
}

inline bool MarkActive(int networkId) {
    const int index = MarkIndex(networkId, false);
    if (index >= 0 && MarkExpiry[static_cast<std::size_t>(index)] > Now()) {
        return true;
    }
    const auto target = HeroByNetworkId(networkId);
    return HasIllumination(target);
}

inline void ApplyPredictedMark(int networkId, bool confirmed = false) {
    const int index = MarkIndex(networkId);
    if (index < 0) return;
    MarkExpiry[static_cast<std::size_t>(index)] = Now() +
        static_cast<int>(kPassiveSeconds * 1000.0f);
    MarkConfirmed[static_cast<std::size_t>(index)] = confirmed;
}

inline void ConsumeMark(int networkId) {
    const int index = MarkIndex(networkId, false);
    if (index < 0) return;
    MarkExpiry[static_cast<std::size_t>(index)] = 0;
    MarkConfirmed[static_cast<std::size_t>(index)] = false;
}

inline void ReconcileMarks() {
    const int now = Now();
    for (std::size_t i = 0; i < MarkIds.size(); ++i) {
        if (MarkIds[i] == 0) continue;
        const auto target = HeroByNetworkId(MarkIds[i]);
        if (!target.IsValid() || target.IsDead()) {
            MarkIds[i] = 0;
            MarkExpiry[i] = 0;
            MarkConfirmed[i] = false;
            continue;
        }
        if (HasIllumination(target)) {
            MarkExpiry[i] = std::max(MarkExpiry[i], now +
                static_cast<int>(kPassiveSeconds * 1000.0f));
            MarkConfirmed[i] = true;
        } else if (MarkExpiry[i] <= now) {
            MarkExpiry[i] = 0;
            MarkConfirmed[i] = false;
        }
    }
    if (PendingMarkUntil <= now) {
        PendingMarkTargetId = 0;
        PendingMarkUntil = 0;
    }
}

inline bool AttackWindingUp() {
    return Orbwalker::IsWindingUp();
}

inline bool PredictionFor(int slot, const AIHeroClient& target,
                          SDK::HitChance chance, SDK::PredictionOutput& output,
                          bool noCollision = false) {
    if (!target.IsValid() || !Engine::RuntimeSpells[slot]) return false;
    return PredictionHits(slot, target, chance, noCollision, &output) &&
        output.GetCastPosition().IsValid() &&
        !output.GetCastPosition().IsZero();
}

inline AIHeroClient CombatTarget(const AIHeroClient& preferred, float range) {
    return PreferredEnemyTarget(preferred, range);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!Engine::ValidEnemy(target, kQRange + 45.0f) ||
        !CanUse(0, mode, reactive) || Now() - LastQCastTick < 24 ||
        IsCommonUntargetableOrImmune(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool lethal = SpellDamage(0, target) +
        (MarkActive(static_cast<int>(target.NetworkId()))
             ? Geometry::PassiveDamage(player.Level(), player.AP()) : 0.0f) >=
        target.Health() + target.AllShield();
    if (!reactive && AttackWindingUp() && !lethal) return false;
    SDK::PredictionOutput prediction{};
    if (!PredictionFor(0, target, SDK::HitChance::High, prediction, true)) return false;
    const float distance = player.Position().Distance2D(prediction.GetCastPosition());
    const bool blocked = PredictionProjectileWall(0, prediction, kQWidth * 0.5f);
    const float firstCollision = prediction.CollisionObjects.empty() ? -1.0f : distance;
    const bool firstTarget = prediction.CollisionObjects.empty() ||
        QFirstCollisionRoot(distance, firstCollision, target.BoundingRadius(), 0.0f);
    if (!QRootAllowed(true, blocked, firstTarget, AttackWindingUp(), lethal)) return false;
    if (!Engine::ControllerCastPosition(0, prediction.GetCastPosition())) return false;
    LastQCastTick = Now();
    PendingMarkTargetId = static_cast<int>(target.NetworkId());
    PendingMarkUntil = LastQCastTick + 1000;
    ApplyPredictedMark(PendingMarkTargetId);
    return true;
}

inline AIHeroClient ProtectedAlly() {
    const auto ally = SelectProtectionAlly(1250.0f, LastWTargetId, WReturnUntil);
    return Engine::ValidAlly(ally) ? ally : AIHeroClient{};
}

inline Vector3 WEndpoint(const AIHeroClient& threat, bool fleeing) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    const auto ally = ProtectedAlly();
    if (ally.IsValid()) return ally.Position();
    const Vector3 direction = fleeing
        ? Game::CursorPos() : (threat.IsValid() ? threat.Position() : Game::CursorPos());
    if (!direction.IsValid() || direction.IsZero()) return {};
    return Engine::Extend(player.Position(), direction, kWRange);
}

inline bool CastW(const AIHeroClient& threat, Mode mode, bool fleeing = false,
                  bool manual = false) {
    if (!CanUse(1, mode, true) || Now() - LastWCastTick < 70) return false;
    const auto player = GameObjects::Player();
    const Vector3 endpoint = WEndpoint(threat, fleeing);
    if (!player.IsValid() || endpoint.IsZero() || !endpoint.IsValid()) return false;
    const auto ally = ProtectedAlly();
    const bool incoming = Engine::CountEnemiesAt(
        ally.IsValid() ? ally.Position() : player.Position(), 700.0f) > 0;
    const bool low = ally.IsValid() && ally.HealthPercent() < 55.0f;
    if (!WReturnShieldWorthwhile(true, endpoint.IsValid(), low, incoming, manual || fleeing)) return false;
    if (!Engine::ControllerCastPosition(1, endpoint)) return false;
    LastWCastTick = Now();
    LastWTargetId = ally.IsValid() ? static_cast<int>(ally.NetworkId()) :
        static_cast<int>(player.NetworkId());
    WReturnUntil = Now() + 1800;
    WReturnExpected = true;
    WShieldObserved = false;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!CanUse(2, mode, reactive) || Now() - LastECastTick < 55) return false;
    if (EZone.Active) {
        const bool lethal = target.IsValid() && SpellDamage(2, target) >=
            target.Health() + target.AllShield();
        const bool rooted = target.IsValid() &&
            (target.HasBuff("LuxLightBinding") || target.HasBuff("LuxESlow"));
        const bool expiring = EZone.ExpiresAt <=
            static_cast<float>(Now() + 220) / 1000.0f;
        if (target.IsValid() && ShouldDetonateZone(
                EZone, target.Position(), target.BoundingRadius(), lethal, rooted,
                expiring, false, Engine::UnderEnemyTurret(EZone.Center)) &&
            Engine::ControllerCastPosition(2, EZone.Center)) {
            LastECastTick = Now();
            EZone.DetonationReady = false;
            EZone.Active = false;
            ETargetId = 0;
            return true;
        }
    }
    if (!Engine::ValidEnemy(target, kERange + 40.0f) ||
        IsCommonUntargetableOrImmune(target)) return false;
    SDK::PredictionOutput prediction{};
    if (!PredictionFor(2, target, SDK::HitChance::High, prediction, false) ||
        PredictionProjectileWall(2, prediction, 90.0f)) return false;
    if (!Engine::ControllerCastPosition(2, prediction.GetCastPosition())) return false;
    LastECastTick = Now();
    ETargetId = static_cast<int>(target.NetworkId());
    ApplyPredictedMark(ETargetId);
    EZone = StartZone(prediction.GetCastPosition(),
                      static_cast<float>(LastECastTick) / 1000.0f);
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool manual = false) {
    if (!Engine::RuntimeSpells[3] || !Engine::RuntimeSpells[3]->IsReady() ||
        (!manual && !CanUse(3, mode)) || Now() - LastRCastTick < 180 ||
        !Engine::ValidEnemy(target, kRRange) || RChannelActive) return false;
    SDK::PredictionOutput prediction{};
    if (!PredictionFor(3, target, SDK::HitChance::VeryHigh, prediction, false)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool lethal = SpellDamage(3, target) >= target.Health() + target.AllShield();
    RLineContext context{};
    context.TargetValid = target.IsValid();
    context.PredictionVeryHigh = prediction.Hitchance >= SDK::HitChance::VeryHigh;
    context.ProjectileWall = PredictionProjectileWall(3, prediction, kRWidth * 0.5f);
    context.LineSafe = SafeBeamLine(context.ProjectileWall,
        Engine::UnderEnemyTurret(target.Position()),
        Engine::CountEnemiesAt(player.Position(), 850.0f), 0, lethal);
    context.Lethal = lethal;
    context.Manual = manual;
    context.ChannelActive = RChannelActive;
    context.UnderTurret = Engine::UnderEnemyTurret(player.Position());
    context.Distance = player.Position().Distance2D(prediction.GetCastPosition());
    if (!ShouldCastR(context)) return false;
    if (!Engine::ControllerCastPosition(3, prediction.GetCastPosition())) return false;
    LastRCastTick = Now();
    RChannelActive = true;
    RManualStarted = manual;
    RChannelUntil = Now() + 1800;
    return true;
}

inline bool TryManualR(const AIHeroClient& preferred) {
    if (!ManualUltimatePressed() || RChannelActive) return false;
    const auto selected = PlayerSelectedEnemy(kRRange);
    const auto target = CombatTarget(selected.IsValid() ? selected : preferred, kRRange);
    return target.IsValid() && CastR(target, Mode::Automatic, true);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!target.IsValid()) return false;
    const int id = static_cast<int>(target.NetworkId());
    if (MarkActive(id) && CastQ(target, mode)) return true;
    if (EZone.Active && CastE(target, mode)) return true;
    if (CastQ(target, mode)) return true;
    if (CastE(target, mode)) return true;
    return CastW(target, mode);
}

inline bool TryAutomatic(const AIHeroClient& preferred) {
    if (TryManualR(preferred)) return true;
    const auto target = CombatTarget(preferred, kRRange);
    if (!target.IsValid()) return false;
    const bool lethal = SpellDamage(3, target) >= target.Health() + target.AllShield();
    if (lethal && CastR(target, Mode::Automatic, false)) return true;
    if (GapcloserUntil > Now()) {
        const auto threat = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(threat, kQRange) && CastQ(threat, Mode::Automatic, true)) return true;
    }
    return TryCombat(target, Mode::Automatic);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    ReconcileMarks();
    const int now = Now();
    if (EZone.Active && EZone.ExpiresAt <= static_cast<float>(now) / 1000.0f) EZone = {};
    if (WReturnExpected && WReturnUntil <= now) WReturnExpected = false;
    if (RChannelActive) {
        const auto player = GameObjects::Player();
        const bool observed = player.IsValid() &&
            (player.HasBuff("LuxR") || player.HasBuff("LuxFinalSpark") ||
             player.HasBuff("LuxLightstrikeToggle"));
        if (!observed && RChannelUntil <= now) {
            RChannelActive = false;
            RManualStarted = false;
        } else {
            return true;
        }
    }
    if (TryManualR(preferred)) return true;
    if (mode == Mode::Automatic) return TryAutomatic(preferred);
    if (mode == Mode::Flee) {
        if (CastW(preferred, mode, true, true)) return true;
        return Engine::ValidEnemy(preferred, kQRange) && CastQ(preferred, mode, true);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        return Engine::TryFarm(mode);
    }
    if (mode == Mode::Combo || mode == Mode::Harass) {
        const auto target = CombatTarget(preferred, kERange + 50.0f);
        return TryCombat(target, mode);
    }
    return false;
}

inline void ObserveLocalSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId != 0
            ? args.TargetNetworkId : args.Target.NetworkId);
        LastAutoTick = now;
        if (MarkActive(LastAutoTargetId)) ConsumeMark(LastAutoTargetId);
        return;
    }
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        SpellEventNameContainsAny(args, {"luxlightbinding", "luxq"})) {
        LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               SpellEventNameContainsAny(args, {"luxprismaticwave", "luxw"})) {
        LastWCastTick = now;
        WReturnExpected = true;
        WReturnUntil = now + 1800;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               SpellEventNameContainsAny(args, {"luxlightstrike", "luxe"})) {
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               SpellEventNameContainsAny(args, {"luxr", "finalspark"})) {
        LastRCastTick = now;
        RChannelActive = true;
        RChannelUntil = now + 1800;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    ObserveLocalSpell(args);
}

inline void UpdateBuff(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (Engine::TextContains(args.BuffName, "luxillumination") ||
        Engine::TextContains(args.BuffName, "luxpmark")) {
        if (added) ApplyPredictedMark(id, true);
        else ConsumeMark(id);
    }
    if (IsLocalPlayer(args.Sender) &&
        (Engine::TextContains(args.BuffName, "luxshield") ||
         Engine::TextContains(args.BuffName, "prismaticwave"))) {
        WShieldObserved = added;
    }
    if (IsLocalPlayer(args.Sender) &&
        (Engine::TextContains(args.BuffName, "luxr") ||
         Engine::TextContains(args.BuffName, "finalspark") ||
         Engine::TextContains(args.BuffName, "lightstriketoggle"))) {
        RChannelActive = added;
        if (added) RChannelUntil = Now() + 1800;
        else RChannelUntil = 0;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { UpdateBuff(args, true); }
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { UpdateBuff(args, false); }

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureBeforeAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
    if (LastAutoTargetId != 0 && MarkActive(LastAutoTargetId)) ConsumeMark(LastAutoTargetId);
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint,
                           GapcloserUntil, 900, 1200);
}

inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Lux mechanics", "Lux mechanics"));
    PassiveMenu = TacticsMenu->AddSubMenu(new Menu("Illumination", "Passive marks"));
    PassiveMenu->Add(new MenuSeparator("RequireMark", "Track and consume Illumination marks before autos"));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu("FinalSpark", "Final Spark safety"));
    UltimateMenu->Add(new MenuSeparator("ManualOnly", "Manual R bypasses local damage preference but never line safety"));
}

inline void OnLoad() {
    MarkIds.fill(0);
    MarkExpiry.fill(0);
    MarkConfirmed.fill(false);
    PendingMarkTargetId = PendingMarkUntil = 0;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAutoTargetId = LastAutoTick = LastWTargetId = WReturnUntil = 0;
    WReturnExpected = WShieldObserved = false;
    ETargetId = 0;
    EZone = {};
    RChannelActive = RManualStarted = false;
    RChannelUntil = 0;
    GapcloserTargetId = GapcloserUntil = 0;
    GapcloserEndpoint = {};
}

inline void OnUnload() {
    TacticsMenu = PassiveMenu = UltimateMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Reconcile Lux Illumination marks from buffs, predicted casts and expiry",
    "Consume the passive mark on the intended auto without stealing an attack",
    "Require Q Light Binding to root the first legal collision and reject minion blockers",
    "Preserve an auto windup unless Q is a confirmed lethal mark route",
    "Send W through a protected ally and track the returning double shield",
    "Reconcile LuxShield on self and allies while the W return window is live",
    "Hold E Lucent Singularity as a slow zone before a meaningful detonation",
    "Detonate E on lethal, rooted, slowed or expiring targets inside 295 radius",
    "Reject E and R casts through projectile walls and unsafe turret commit points",
    "Predict the global R beam at very-high confidence across its 3340 range",
    "Use automatic R only for isolated lethal lines and manual R for explicit intent",
    "Protect a manual Final Spark channel from the decision loop until it ends",
    "Prefer selected target, then orbwalker target, while keeping a reachable route",
    "Run Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic policies",
    "Reconcile local spell, auto, buff, gapcloser and missile lifecycle events",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Lux;
    controller.ControllerId = "champion.kuroaio.ai.lux.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AILux.md";
    controller.ImplementationSummary =
        "Illumination mark ledger, first-collision Q root, returning W shield state, "
        "recast-aware E slow zone and safe global Final Spark prediction with manual channel protection.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Lux
