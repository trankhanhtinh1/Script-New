#pragma once

#include "../AIChampionEngine.h"
#include "../AIMarksmanControllerHelpers.h"
#include "AICorkiGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <string>

namespace Plugins::KuroAIO::AI::Controllers::Corki {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttackEvent;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::Now;
using ControllerHelpers::SpellEventNameContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* MissileMenu = nullptr;
inline Menu* ValkyrieMenu = nullptr;
inline Menu* ResourceMenu = nullptr;
inline BarrageState RState{};
inline bool PackageLoaded = false;
inline bool PackageObserved = false;
inline int LastStateObservationTick = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int PlayerOverrideUntil = 0;
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

inline bool HasAnyPlayerBuff(const AIHeroClient& player,
                             std::initializer_list<const char*> names) {
    if (!player.IsValid()) return false;
    for (const char* name : names) {
        if (name && name[0] && player.HasBuff(name)) return true;
    }
    return false;
}

inline bool BigOneBuff(const AIHeroClient& player) {
    return HasAnyPlayerBuff(player, {
        "CorkiMissileBarrageCounterBig", "corkimissilebarragecounterbig"
    });
}

inline bool PackageBuff(const AIHeroClient& player) {
    return HasAnyPlayerBuff(player, {
        "corkiloadedsound", "CorkiLoaded", "CorkiPackage",
        "CorkiSpecialDelivery", "specialdelivery"
    });
}

inline bool PackageRuntimeSpell(const AIHeroClient& player) {
    if (!player.IsValid()) return false;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::W);
    if (!spell.IsValid()) return false;
    return ControllerHelpers::AnyTextContains(
        { spell.Name().c_str(), spell.ScriptName().c_str(), spell.IconName().c_str() },
        { "dangerzone", "package", "specialdelivery" });
}

inline void RefreshRuntimeRange() {
    if (Engine::RuntimeSpells[1])
        Engine::RuntimeSpells[1]->Range = ValkyrieReach(PackageLoaded);
    if (Engine::RuntimeSpells[3])
        Engine::RuntimeSpells[3]->Range = BarrageReach(RState);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const auto r = player.Spellbook().GetSpell(SDK::SpellSlot::R);
    if (r.IsValid()) ObserveAmmo(RState, r.Ammo(), r.MaxAmmo());
    if (!RState.AmmoObserved && Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->IsReady()) RState.Ammo = std::max(1, RState.Ammo);

    const bool big = BigOneBuff(player);
    const bool normal = player.HasBuff("CorkiMissileBarrageCounterNormal") ||
                        player.HasBuff("corkimissilebarragecounternormal");
    int normalCount = -1;
    if (normal) {
        normalCount = player.HasBuff("CorkiMissileBarrageCounterNormal")
            ? player.GetBuffCount("CorkiMissileBarrageCounterNormal")
            : player.GetBuffCount("corkimissilebarragecounternormal");
    }
    ObserveBigOne(RState, big, normal, normalCount);
    const bool package = PackageBuff(player) || PackageRuntimeSpell(player);
    if (package || PackageObserved) PackageLoaded = package;
    if (package) PackageObserved = true;
    LastStateObservationTick = Now();
    RefreshRuntimeRange();
}

inline bool ModeManaAllowed(Mode mode, float threshold, bool urgent = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (mode == Mode::Combo || mode == Mode::Automatic || mode == Mode::Flee)
        threshold = std::min(threshold, 15.0f);
    return ResourcePolicy(player.ManaPercent(), threshold, urgent);
}

inline bool ClearQPrediction(const AIHeroClient& target,
                             SDK::PredictionOutput* output = nullptr) {
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(0, target, SDK::HitChance::High, false,
                                    &prediction) &&
        !PredictionProjectileWall(0, prediction, 120.0f);
    if (output) *output = prediction;
    return hit;
}

inline bool ClearRPrediction(const AIHeroClient& target,
                             SDK::PredictionOutput* output = nullptr) {
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(3, target, SDK::HitChance::High, true,
                                    &prediction) &&
        !PredictionProjectileWall(3, prediction,
                                  RState.BigOneReady ? 60.0f : 40.0f);
    if (output) *output = prediction;
    return hit;
}

inline float BarrageDamage(const AIHeroClient& target) {
    const float base = SpellDamage(3, target);
    return RState.BigOneReady ? base * 2.0f : base;
}

inline bool CastQ(const AIHeroClient& target, Mode mode) {
    if (!CanUse(0, mode) || !Engine::ValidEnemy(target, kQRange + 35.0f) ||
        !CastThrottlePassed(LastQCastTick, 24)) return false;
    const bool lethal = SpellDamage(0, target) >= target.Health() + target.AllShield();
    if (!ModeManaAllowed(mode, mode == Mode::Harass ? 42.0f : 12.0f, lethal))
        return false;
    SDK::PredictionOutput prediction{};
    const bool hit = ClearQPrediction(target, &prediction);
    PhosphorusContext context{};
    context.InRange = context.PredictionHigh = hit;
    context.ProjectileWall = PredictionProjectileWall(0, prediction, 120.0f);
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    context.AttackAvailable = LocalAttackAvailable(target);
    context.AfterAttack = RecentlyAttackedTarget(
        target, LastAfterAttackTargetId, LastAfterAttackTick, 430);
    context.Lethal = lethal;
    context.TargetOutsideAttackRange = !InAutoAttackRange(target);
    if (!ShouldCastPhosphorus(context) ||
        !Engine::ControllerCastPosition(0, prediction.GetCastPosition())) return false;
    LastQCastTick = Now();
    return true;
}

inline bool GatlingHits(const AIHeroClient& target, Vector3* aim = nullptr) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    const Vector3 predicted = ControllerHelpers::PredictPosition(target, 0.35f);
    if (!predicted.IsValid() || predicted.IsZero()) return false;
    if (aim) *aim = predicted;
    return PointInCone({player.Position().x, player.Position().z},
                       {predicted.x, predicted.z}, {predicted.x, predicted.z},
                       kERange, kEConeHalfAngleDegrees, target.BoundingRadius());
}

inline bool CastE(const AIHeroClient& target, Mode mode) {
    if (!CanUse(2, mode) || !Engine::ValidEnemy(target, kERange + 50.0f) ||
        !CastThrottlePassed(LastECastTick, 60)) return false;
    const bool lethal = SpellDamage(2, target) >= target.Health() + target.AllShield();
    if (!ModeManaAllowed(mode, mode == Mode::Harass ? 50.0f : 16.0f, lethal))
        return false;
    Vector3 aim{};
    GatlingContext context{};
    context.InCone = GatlingHits(target, &aim);
    context.TargetStable = InAutoAttackRange(target) || IsImmobile(target) ||
                           !IsEscaping(target, 0.45f);
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    context.HasFollowup = InAutoAttackRange(target) || ClearQPrediction(target);
    context.Lethal = lethal;
    if (!ShouldStartGatling(context) || !Engine::ControllerCastPosition(2, aim))
        return false;
    LastECastTick = Now();
    return true;
}

inline bool SafeValkyrieEndpoint(const Vector3& endpoint,
                                const AIHeroClient& threat,
                                bool defensive,
                                bool emergency,
                                bool lethal,
                                bool createsFollowup) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    ValkyrieContext context{};
    context.EndpointValid = endpoint.IsValid() && !endpoint.IsZero();
    context.Walkable = context.EndpointValid && !SDK::NavMesh::IsWall(endpoint);
    context.UnderEnemyTurret = context.EndpointValid && Engine::UnderEnemyTurret(endpoint);
    context.PointClickThreat = context.EndpointValid && HasReadyPointClickThreatAt(endpoint);
    context.DashThreat = context.EndpointValid && HasReadyDashHazardAt(endpoint);
    context.Defensive = defensive;
    context.Emergency = emergency;
    context.Lethal = lethal;
    context.CreatesFollowup = createsFollowup;
    context.AttackAvailable = threat.IsValid() && LocalAttackAvailable(threat);
    context.PackageLoaded = PackageLoaded;
    context.PreservePackage = Bool(ValkyrieMenu, "PreservePackage", true);
    context.TravelDistance = context.EndpointValid
        ? player.Position().Distance2D(endpoint) : FLT_MAX;
    context.EnemiesAtEndpoint = context.EndpointValid
        ? Engine::CountEnemiesAt(endpoint, 625.0f) : 99;
    context.MaximumEnemies = Slider(ValkyrieMenu, "MaximumEnemies", 1);
    return context.EndpointValid &&
        Engine::PositionDangerScore(endpoint, threat, Engine::ResolvedSpecs[1]) > -10000.0f &&
        ShouldValkyrie(context);
}

inline bool CastWFlee(const AIHeroClient& threat, bool emergency) {
    if (!CanUse(1, Mode::Flee, true) || !CastThrottlePassed(LastWCastTick, 90) ||
        !ModeManaAllowed(Mode::Flee, 8.0f, emergency)) return false;
    const Vector3 endpoint = Engine::BestSafePosition(
        Engine::ResolvedSpecs[1], threat, AimPolicy::SafeCursor);
    if (!SafeValkyrieEndpoint(endpoint, threat, true, emergency, false, false) ||
        !Engine::ControllerCastPosition(1, endpoint)) return false;
    LastWCastTick = Now();
    return true;
}

inline bool CastWOffensive(const AIHeroClient& target, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !CanUse(1, mode) ||
        !Engine::ValidEnemy(target, ValkyrieReach(PackageLoaded) + 650.0f) ||
        !CastThrottlePassed(LastWCastTick, 90)) return false;
    const bool lethal = SpellDamage(1, target) +
        (ClearQPrediction(target) ? SpellDamage(0, target) : 0.0f) >=
        target.Health() + target.AllShield();
    if (!lethal || !ModeManaAllowed(mode, 10.0f, true)) return false;
    const Vector3 predicted = ControllerHelpers::PredictPosition(target, 0.35f);
    if (!predicted.IsValid() || predicted.IsZero()) return false;
    const float travel = std::clamp(player.Position().Distance2D(predicted) - 500.0f,
                                    0.0f, ValkyrieReach(PackageLoaded));
    if (travel < 80.0f) return false;
    const Vector3 endpoint = Engine::Extend(player.Position(), predicted, travel);
    if (!SafeValkyrieEndpoint(endpoint, target, false, false, true,
                               endpoint.Distance2D(predicted) <= kQRange) ||
        !Engine::ControllerCastPosition(1, endpoint)) return false;
    LastWCastTick = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode,
                  bool killSecure, bool manual = false) {
    if (!Engine::RuntimeSpells[3] || !Engine::RuntimeSpells[3]->IsReady() ||
        RState.Ammo <= 0 ||
        !Engine::ValidEnemy(target, BarrageReach(RState) + 35.0f) ||
        !CastThrottlePassed(LastRCastTick, 60)) return false;
    if (!manual && !CanUse(3, mode)) return false;
    const bool lethal = BarrageDamage(target) >= target.Health() + target.AllShield();
    if (!ModeManaAllowed(mode, mode == Mode::Harass ? 46.0f : 10.0f,
                         lethal || manual)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = ClearRPrediction(target, &prediction);
    BarrageContext context{};
    context.Ammo = RState.Ammo;
    context.ReserveAmmo = Slider(MissileMenu, "ReserveAmmo", 1);
    context.BigOne = RState.BigOneReady;
    context.PreserveBigOne = Bool(MissileMenu, "PreserveBigOne", true) && !manual;
    context.InReach = context.PredictionHigh = hit;
    context.Collision = !prediction.CollisionObjects.empty();
    context.ProjectileWall = PredictionProjectileWall(
        3, prediction, RState.BigOneReady ? 60.0f : 40.0f);
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    context.AttackAvailable = LocalAttackAvailable(target);
    context.Lethal = lethal || manual;
    context.KillSecure = killSecure && lethal;
    context.Combo = mode == Mode::Combo;
    context.Harass = mode == Mode::Harass;
    if (!ShouldCastBarrage(context) ||
        !Engine::ControllerCastPosition(3, prediction.GetCastPosition())) return false;
    LastRCastTick = Now();
    ConsumeBarrage(RState);
    RefreshRuntimeRange();
    return true;
}

inline MarksmanTargeting::TargetContext TargetFacts(const AIHeroClient& target,
                                                     Mode mode) {
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool attack = InAutoAttackRange(target) &&
        !ControllerHelpers::ProjectileWallBlocksFromPlayer(target.Position(), 0.0f);
    const bool q = CanUse(0, mode) && distance <= kQRange + 35.0f &&
                   ClearQPrediction(target);
    const bool e = CanUse(2, mode) && distance <= kERange + 35.0f;
    const bool r = CanUse(3, mode) && RState.Ammo > 0 &&
                   distance <= BarrageReach(RState) + 35.0f &&
                   ClearRPrediction(target);
    const std::array<bool, 4> reachable = {q, false, e, r};
    float estimated = EstimatedDamage(target, reachable, attack ? 2 : 0);
    if (r && RState.BigOneReady) estimated += SpellDamage(3, target);
    auto context = BaseTargetContext(target, estimated);
    context.AutoReachable = attack;
    context.DirectSpellReachable = q || e;
    context.SetupReachable = e;
    context.ExecuteReachable = r;
    context.ProjectileBlocked = !attack && !q && !r;
    if (RState.BigOneReady && r) context.Priority += 55.0f;
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred, Mode mode) {
    AIHeroClient cooperation = preferred;
    if (!Engine::ValidEnemy(cooperation))
        cooperation = ControllerHelpers::PlayerSelectedEnemy(kRBigRange + 80.0f);
    if (!Engine::ValidEnemy(cooperation))
        cooperation = ControllerHelpers::OrbwalkerHeroTarget(kRBigRange + 80.0f);
    LastSmartTarget = ControllerHelpers::SelectReachableEnemy(
        cooperation, kRBigRange + 80.0f,
        [mode](const AIHeroClient& enemy) { return TargetFacts(enemy, mode); });
    return LastSmartTarget;
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = SelectSmartTarget(preferred, Mode::Automatic);
    if (!Engine::ValidEnemy(target)) return false;
    if (SpellDamage(0, target) >= target.Health() + target.AllShield() &&
        CastQ(target, Mode::Automatic)) return true;
    return CastR(target, Mode::Automatic, true);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastE(target, mode)) return true;
    if (CastQ(target, mode)) return true;
    if (CastR(target, mode, false)) return true;
    return mode == Mode::Combo && CastWOffensive(target, mode);
}

inline bool TryManualR(const AIHeroClient& preferred) {
    if (!ManualUltimatePressed()) return false;
    const auto selected = ControllerHelpers::PlayerSelectedEnemy(BarrageReach(RState) + 50.0f);
    const auto target = SelectSmartTarget(selected.IsValid() ? selected : preferred,
                                          Mode::Automatic);
    return Engine::ValidEnemy(target) && CastR(target, Mode::Automatic, false, true);
}

inline bool TryGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto threat = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(threat, 850.0f)) return false;
    return CastWFlee(threat, GameObjects::Player().HealthPercent() <=
        Slider(ValkyrieMenu, "EmergencyHP", 34));
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    ReconcileState();
    if (PlayerOverrideUntil > Now()) return true;
    if (TryManualR(preferred) || TryGapcloser() || TryKillSecure(preferred)) return true;
    if (mode == Mode::Flee) {
        const auto threat = ControllerHelpers::NearestEnemyToPlayer(preferred, 950.0f);
        return CastWFlee(threat, GameObjects::Player().HealthPercent() <=
            Slider(ValkyrieMenu, "EmergencyHP", 34));
    }
    if (mode == Mode::Combo || mode == Mode::Harass)
        return TryCombat(SelectSmartTarget(preferred, mode), mode);
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        if (!ModeManaAllowed(mode, mode == Mode::LastHit ? 34.0f : 48.0f)) return false;
        return Engine::TryFarm(mode);
    }
    return false;
}

inline bool IsPackageBuffName(const char* name) {
    return ControllerHelpers::TextContainsAny(name,
        {"corkiloaded", "corkipackage", "specialdelivery", "dangerzone"});
}
inline bool IsBigOneBuffName(const char* name) {
    return ControllerHelpers::TextContainsAny(name,
        {"corkimissilebarragecounterbig", "corkirbig"});
}
inline bool IsNormalCounterBuffName(const char* name) {
    return ControllerHelpers::TextContainsAny(name,
        {"corkimissilebarragecounternormal", "corkirnormal"});
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender)) return;
    if (IsPackageBuffName(args.BuffName)) {
        PackageLoaded = added;
        PackageObserved = true;
    }
    if (IsBigOneBuffName(args.BuffName)) {
        if (added) ObserveBigOne(RState, true, false, -1);
        else if (RState.BigOneObserved) RState.BigOneReady = false;
    }
    if (IsNormalCounterBuffName(args.BuffName) && added)
        ObserveBigOne(RState, false, true, args.Count);
    LastStateObservationTick = Now();
    RefreshRuntimeRange();
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, true); }
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, false); }
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, true); }

inline void ObserveSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender) || args.IsAutoAttack) return;
    const int now = Now();
    int slot = -1;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        SpellEventNameContainsAny(args, {"phosphorusbomb", "corkiq"})) {
        slot = 0; LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               SpellEventNameContainsAny(args, {"carpetbomb", "valkyrie", "dangerzone"})) {
        slot = 1; LastWCastTick = now;
        if (PackageLoaded || SpellEventNameContainsAny(args, {"dangerzone"})) {
            PackageLoaded = false; PackageObserved = true;
        }
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               SpellEventNameContainsAny(args, {"ggun", "gatling"})) {
        slot = 2; LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               SpellEventNameContainsAny(args, {"missilebarrage", "corkir"})) {
        slot = 3; LastRCastTick = now;
        if (!Engine::WasControllerCast(3)) ConsumeBarrage(RState);
    }
    if (slot >= 0 && !Engine::WasControllerCast(slot))
        PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 520);
    RefreshRuntimeRange();
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint,
                           GapcloserExpireTick, 700.0f, 900);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("CorkiOneTrick", "Corki missile and package mechanics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 520, 180, 1100));
    MissileMenu = TacticsMenu->AddSubMenu(new Menu("MissileBarrage", "Missile Barrage ammo"));
    MissileMenu->Add(new MenuSlider("ReserveAmmo", "Reserve R charges", 1, 0, 3));
    MissileMenu->Add(new MenuBool("PreserveBigOne", "Preserve Big One unless lethal", true));
    ValkyrieMenu = TacticsMenu->AddSubMenu(new Menu("ValkyrieSafety", "Valkyrie endpoint safety"));
    ValkyrieMenu->Add(new MenuBool("PreservePackage", "Preserve Package except emergency/lethal", true));
    ValkyrieMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at W endpoint", 1, 0, 3));
    ValkyrieMenu->Add(new MenuSlider("EmergencyHP", "Emergency W health (%)", 34, 10, 70));
    ResourceMenu = TacticsMenu->AddSubMenu(new Menu("ResourceModes", "Resource and mode policy"));
    ResourceMenu->Add(new MenuSeparator("FarmPolicy", "Farm uses Q/E profile gates; R and W stay reserved"));
}

inline void OnLoad() {
    RState = {};
    PackageLoaded = PackageObserved = false;
    LastStateObservationTick = 0;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    PlayerOverrideUntil = 0;
    LastMode = Mode::None;
    LastSmartTarget = {};
    ReconcileState();
}
inline void OnUnload() {
    TacticsMenu = MissileMenu = ValkyrieMenu = ResourceMenu = nullptr;
    RState = {};
    PackageLoaded = PackageObserved = false;
    PlayerOverrideUntil = 0;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Read Riot 26.15 and CommunityDragon 16.15 as the pinned baseline",
    "Model Phosphorus Bomb as an 825-range travelling 1100-speed missile circle",
    "Reject Q across a projectile wall or while an attack is winding up",
    "Prefer an available attack and fire Q in the after-attack window",
    "Use Valkyrie only at a walkable turret-safe low-threat endpoint",
    "Reject W into point-click lockdown, dash threat or excess enemies",
    "Preserve the loaded Package except for emergency disengage or lethal mobility",
    "Reconcile Package state from buffs, runtime W identity and cast events",
    "Model Gatling Gun as a 600-range cone with a 28-degree half angle",
    "Start Gatling only when the target can remain in the cone and a follow-up exists",
    "Read R Ammo and MaxAmmo as the authoritative four-charge reservoir",
    "Maintain a cast-event ammo fallback while runtime ammo telemetry is absent",
    "Reconcile Big One state from Big and normal counter buff events and polling",
    "Use 1300 normal missile reach and 1500 Big One reach",
    "Reject R collision and projectile-wall paths",
    "Reserve configured R ammo and preserve Big One unless lethal or manually requested",
    "Apply the Big One two-times damage multiplier to execute planning",
    "Respect Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic mode ownership",
    "Gate harass and farm spells by mana while allowing lethal and emergency actions",
    "Prefer the selected target and cooperate with the orbwalker hero target",
    "Yield briefly after every player-owned Q, W, E or R cast",
    "Re-plan from polled and event state after manual casts without double-consuming ammo",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Corki";
    controller.ControllerId = "champion.kuroaio.ai.corki.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AICorki.md";
    controller.ImplementationSummary =
        "Missile Q and Gatling cone discipline, Package-aware safe Valkyrie, four-charge normal/Big One barrage reach, resource gates, manual ownership, and event-plus-poll reconciliation.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveSpell;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &OnBuffUpdate;
    controller.OnAfterAttack =
        &CaptureAfterAttackEvent<&LastAfterAttackTargetId, &LastAfterAttackTick>;
    controller.OnGapcloser = &OnGapcloser;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Corki
