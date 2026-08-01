#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIKaisaGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Kaisa {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* PlasmaMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline int LastCastTick[4] = {};
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline Mode LastMode = Mode::None;
inline EvolutionState Evolutions{};

struct TrackedPlasma {
    int NetworkId = 0;
    PlasmaState State{};
};
inline std::array<TrackedPlasma, 8> PlasmaTargets{};

inline int FindPlasmaSlot(int networkId, bool allocate = false) {
    if (networkId == 0) return -1;
    for (std::size_t i = 0; i < PlasmaTargets.size(); ++i) {
        if (PlasmaTargets[i].NetworkId == networkId) return static_cast<int>(i);
    }
    if (!allocate) return -1;
    for (std::size_t i = 0; i < PlasmaTargets.size(); ++i) {
        if (PlasmaTargets[i].NetworkId == 0) {
            PlasmaTargets[i].NetworkId = networkId;
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline bool HasKaisaMark(const AIBaseClient& target) {
    return target.IsValid() &&
        (target.HasBuff("KaisaPassiveMarker") ||
         target.HasBuff("kaisapassivemarker"));
}

inline int PlasmaStacks(const AIBaseClient& target) {
    if (!target.IsValid()) return 0;
    const int id = static_cast<int>(target.NetworkId());
    const int slot = FindPlasmaSlot(id);
    const int observed = std::max(
        target.GetBuffCount("KaisaPassiveMarker"),
        target.GetBuffCount("kaisapassivemarker"));
    if (observed > 0) return ClampPlasmaStacks(observed);
    return slot >= 0 ? PlasmaTargets[slot].State.Stacks : 0;
}

inline void PollEvolutionState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Evolutions.Poll(
        player.HasBuff("KaisaQEvolved") || player.HasBuff("kaisaqevolved"),
        player.HasBuff("KaisaWEvolved") || player.HasBuff("kaisawEvolved"),
        player.HasBuff("KaisaEEvolved") || player.HasBuff("kaisaeevolved"));
}

inline void PollPlasma() {
    const int now = Now();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid()) continue;
        const int id = static_cast<int>(enemy.NetworkId());
        const int slot = FindPlasmaSlot(id, true);
        if (slot < 0) continue;
        const int observed = std::max(
            enemy.GetBuffCount("KaisaPassiveMarker"),
            enemy.GetBuffCount("kaisapassivemarker"));
        PlasmaTargets[slot].State.Poll(
            now, observed, HasKaisaMark(enemy));
    }
}

inline bool IsolatedTarget(const AIHeroClient& target) {
    return target.IsValid() && Engine::CountEnemiesAt(target.Position(), 275.0f) <= 1;
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 70.0f)) {
        return 0.0f;
    }
    const float missile = SpellDamage(0, target);
    return QDamageForTarget(missile, QMissiles(Evolutions.QEvolved),
                            IsolatedTarget(target));
}

inline bool QReadyFor(const AIHeroClient& target, Mode mode) {
    const auto player = GameObjects::Player();
    return CanUse(0, mode) && player.IsValid() &&
        Engine::ValidEnemy(target, kQRange + target.BoundingRadius()) &&
        !ControllerHelpers::HasSpellShieldOrImmunity(target) &&
        !OrbwalkerAttackProjectileBlocked(target);
}

inline bool CastQ(const AIHeroClient& target, Mode mode) {
    if (!QReadyFor(target, mode) ||
        !CastThrottlePassed(LastCastTick[0], 40)) return false;
    const bool lethal = QDamage(target) >= target.Health() + target.AllShield();
    const bool attackRoute = OrbwalkerAttackRoute(target);
    if (!lethal && attackRoute && Orbwalker::CanAttack()) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode) {
    if (!CanUse(1, mode) || !Engine::ValidEnemy(target, kWRange) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastCastTick[1], 55)) return false;
    SDK::PredictionOutput prediction{};
    if (!PredictionHits(1, target, SDK::HitChance::High, true, &prediction) ||
        PredictionProjectileWall(1, prediction, 75.0f)) return false;
    const bool execute = target.HealthPercent() <= 45.0f ||
        PlasmaStacks(target) >= 3;
    if (!execute && OrbwalkerAttackRoute(target)) return false;
    if (!Engine::ControllerCastPosition(1, prediction.GetCastPosition())) return false;
    LastCastTick[1] = Now();
    const int id = static_cast<int>(target.NetworkId());
    const int slot = FindPlasmaSlot(id, true);
    if (slot >= 0) PlasmaTargets[slot].State.Add(Now(), Evolutions.WEvolved ? 3 : 2);
    return true;
}

inline bool CastE(Mode mode, bool defensive = false) {
    if (!CanUse(2, mode, defensive) ||
        !CastThrottlePassed(LastCastTick[2], 55)) return false;
    if (!defensive && !OrbwalkerAttackRoute(ControllerHelpers::NearestEnemyToPlayer({}, 700.0f))) {
        return false;
    }
    if (!Engine::ControllerCastSelf(2)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool BuildSafeREndpoint(const AIHeroClient& target,
                               const SDK::PredictionOutput& prediction,
                               bool& safe) {
    safe = false;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() || !HasKaisaMark(target)) return false;
    const Vector3 predicted = prediction.GetCastPosition().IsValid()
        ? prediction.GetCastPosition() : target.Position();
    if (!RTargetReachable(player.Position().Distance2D(predicted),
                          Engine::RuntimeSpells[3]
                              ? Engine::RuntimeSpells[3]->CurrentRange()
                              : kRMinRange,
                          target.BoundingRadius())) return false;
    const Vector3 endpoint = RDashEndpoint(player.Position(), predicted);
    REndpointContext context{};
    context.Valid = endpoint.IsValid() && !endpoint.IsZero();
    context.MarkedTarget = true;
    context.TerrainSafe = context.Valid && !SDK::NavMesh::IsWall(endpoint);
    context.TurretSafe = context.Valid && !Engine::UnderEnemyTurret(endpoint);
    context.NearbyEnemies = context.Valid ? Engine::CountEnemiesAt(endpoint, 575.0f) : 99;
    context.MaximumEnemies = Slider(UltimateMenu, "MaxEndpointEnemies", 2);
    context.AllowTurret = Bool(UltimateMenu, "AllowTurretDive", false);
    safe = SafeREndpoint(context);
    return safe;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool manual = false,
                  bool peel = false) {
    if (!Engine::RuntimeSpells[3] || !Engine::RuntimeSpells[3]->IsReady() ||
        !Engine::ValidEnemy(target, kRMaxRange) ||
        !HasKaisaMark(target) || !CastThrottlePassed(LastCastTick[3], 90)) {
        return false;
    }
    if (!manual && !CanUse(3, mode)) return false;
    SDK::PredictionOutput prediction{};
    if (!PredictionHits(3, target, SDK::HitChance::VeryHigh, false, &prediction)) return false;
    bool endpointSafe = false;
    if (!BuildSafeREndpoint(target, prediction, endpointSafe)) return false;
    const bool lethal = target.HealthPercent() <= 28.0f;
    const bool followup = CountAlliedFollowup(target.Position(), 750.0f, true) > 0;
    if (!ShouldCastR(manual, lethal, peel, followup, endpointSafe)) return false;
    if (!Engine::ControllerCastPosition(3, prediction.GetCastPosition())) return false;
    LastCastTick[3] = Now();
    return true;
}

inline MarksmanTargeting::TargetContext TargetFacts(const AIHeroClient& target,
                                                     Mode mode) {
    const auto player = GameObjects::Player();
    const float distance = player.IsValid()
        ? player.Position().Distance2D(target.Position()) : FLT_MAX;
    const bool attack = OrbwalkerAttackRoute(target);
    const bool q = QReadyFor(target, mode) && distance <= kQRange + target.BoundingRadius();
    SDK::PredictionOutput wPrediction{};
    const bool w = CanUse(1, mode) && distance <= kWRange &&
        PredictionHits(1, target, SDK::HitChance::High, true, &wPrediction) &&
        !PredictionProjectileWall(1, wPrediction, 75.0f);
    SDK::PredictionOutput rPrediction{};
    const bool r = HasKaisaMark(target) && Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->IsReady() &&
        PredictionHits(3, target, SDK::HitChance::VeryHigh, false, &rPrediction);
    auto context = BaseTargetContext(
        target, EstimatedDamage(target, {q, w, false, false}, attack ? 1 : 0));
    context.AutoReachable = attack;
    context.DirectSpellReachable = q || w;
    context.SetupReachable = r;
    context.ExecuteReachable = q && QDamage(target) >= target.Health() + target.AllShield();
    context.ProjectileBlocked = !w && !r && !q;
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred, Mode mode) {
    const auto target = ControllerHelpers::SelectReachableEnemy(
        preferred, kRMaxRange,
        [mode](const AIHeroClient& enemy) { return TargetFacts(enemy, mode); });
    return target;
}

inline void RefreshOrbwalkerFocus(Mode mode, const AIHeroClient& target) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    auto owned = OwnedOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil, 900.0f);
    if (!combat || !owned.IsValid() || !OrbwalkerAttackRoute(owned)) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        owned = {};
    }
    if (combat && !owned.IsValid() && Engine::ValidEnemy(target) &&
        PlasmaStacks(target) > 0) {
        (void)SetTemporaryOrbwalkerFocus(target, 900.0f, 800,
                                         OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline bool TryAutomatic(const AIHeroClient& preferred) {
    if (ManualUltimatePressed()) {
        const auto target = SelectSmartTarget(preferred, Mode::Automatic);
        if (Engine::ValidEnemy(target) && CastR(target, Mode::Automatic, true)) return true;
    }
    if (GapcloserExpireTick >= Now()) {
        if (CastE(Mode::Automatic, true)) return true;
    }
    if (InterruptExpireTick >= Now()) {
        const auto target = ControllerHelpers::HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(target) && CastR(target, Mode::Automatic, false, true)) return true;
    }
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = SelectSmartTarget(preferred, Mode::Automatic);
    if (!Engine::ValidEnemy(target)) return false;
    return (QDamage(target) >= target.Health() + target.AllShield() &&
            CastQ(target, Mode::Automatic)) ||
           (PlasmaStacks(target) >= 3 && CastW(target, Mode::Automatic));
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    PollEvolutionState();
    PollPlasma();
    if (TryAutomatic(preferred)) return true;
    if (mode == Mode::Flee) return CastE(mode, true);
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        return Engine::TryFarm(mode);
    }
    if (mode != Mode::Combo && mode != Mode::Harass) return false;
    const auto target = SelectSmartTarget(preferred, mode);
    if (!Engine::ValidEnemy(target)) return false;
    RefreshOrbwalkerFocus(mode, target);
    if (CastW(target, mode)) return true;
    if (CastQ(target, mode)) return true;
    return CastE(mode, false);
}

inline void ObserveLocalSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender) || args.IsAutoAttack) return;
    const int slot = args.Slot;
    if (slot >= 0 && slot < 4) LastCastTick[slot] = Now();
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    const auto focus = OwnedOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil, 900.0f);
    if (focus.IsValid()) (void)RedirectBeforeAttackToFocus(args, focus);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAfterAttackTargetId, LastAfterAttackTick) ||
        !args.Target.IsValid() || !args.Target.IsHero()) return;
    const auto target = AIHeroClient(args.Target.Handle());
    if (!Engine::ValidEnemy(target, 900.0f)) return;
    const int slot = FindPlasmaSlot(static_cast<int>(target.NetworkId()), true);
    if (slot >= 0) PlasmaTargets[slot].State.Add(Now());
    if (LastMode == Mode::Combo || LastMode == Mode::Harass) {
        (void)SetTemporaryOrbwalkerFocus(target, 900.0f, 800,
                                         OwnedFocusTargetId, OwnedFocusUntil);
    }
}



inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("KaisaMechanics", "Kai'Sa Mechanics"));
    PlasmaMenu = TacticsMenu->AddSubMenu(new Menu("PlasmaLogic", "Plasma and evolutions"));
    PlasmaMenu->Add(new MenuSeparator("IsolatedQ", "Prefer isolated Q missile damage"));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu("KillerInstinct", "Killer Instinct"));
    UltimateMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at R endpoint", 2, 0, 5));
    UltimateMenu->Add(new MenuBool("AllowTurretDive", "Allow marked-target R under turret", false));
}

inline void OnLoad() {
    for (int& tick : LastCastTick) tick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = 0;
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    LastMode = Mode::None;
    Evolutions = {};
    PlasmaTargets = {};
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = PlasmaMenu = UltimateMenu = nullptr;
    LastMode = Mode::None;
}

inline constexpr const char* Scenarios[] = {
    "Poll Q/W/E evolution buffs and never assume an evolution from item stats",
    "Reconcile four Plasma marks from buff telemetry and expire stale local state",
    "Add one Plasma stack after a confirmed basic attack on the owned target",
    "Apply two or three W marks only after a confirmed collision-aware cast",
    "Use isolated-target Q missile scaling without inventing multi-target hits",
    "Preserve an immediately available auto attack unless Q is lethal",
    "Use high-confidence W prediction and reject collision or projectile walls",
    "Select the marked selected target when its Q/W/R route remains reachable",
    "Require a marked target and current R reach before Killer Instinct",
    "Compute an R dash endpoint behind the predicted marked target",
    "Reject R endpoints in NavMesh walls, enemy turrets or excess enemy density",
    "Allow manual R only through the same mark, reach and endpoint safety checks",
    "Use E defensively for gapclosers and offensively only with a real attack route",
    "Clear owned orbwalker focus on mode exit, invalid target or unload",
    "Use automatic branches for kill secure, interrupt, anti-gapcloser and farming",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Kaisa;
    controller.ControllerId = "champion.kuroaio.ai.kaisa.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIKaisa.md";
    controller.ImplementationSummary =
        "Plasma/evolution polling, isolated Q damage routing, collision-aware W, "
        "attack-preserving marksman modes and safe marked-target R endpoint planning.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 700, 900>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 3000, 250, 5000>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Kaisa
