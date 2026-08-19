#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIZyraGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>

namespace Plugins::KuroAIO::AI::Controllers::Zyra {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;

using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::UnitByNetworkId;
using ControllerHelpers::PlayerManaPercent;

using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::TextContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;
inline std::array<int, 4> LastCastTick{};
inline std::array<PlantState, 16> Plants{};
inline std::array<int, 8> SeedIds{};
inline int ActivePlantCount = 0;
inline int ActiveSeedCount = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int RTargetId = 0;
inline int RPendingUntil = 0;
inline bool ROwned = false;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;

inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool RuntimeBuff(const AIHeroClient& hero,
                        std::initializer_list<const char*> names) {
    if (!hero.IsValid()) return false;
    for (const char* name : names) {
        if (name && hero.HasBuff(name)) return true;
    }
    return false;
}

inline int RuntimeSeeds() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::W);
    return spell.IsValid() ? std::max(0, spell.Ammo()) : 1;
}

inline bool OffensiveReady(int slot, Mode mode, bool reactive = false) {
    if (!SpellEnabled(slot, mode) || !Ready(slot, mode) || !Throttle(slot)) return false;
    if (!reactive && PreserveAttack(false)) return false;
    return true;
}

inline bool ManaEnough(Mode mode) {
    const float minimum = mode == Mode::Harass ? 42.0f :
        (mode == Mode::LaneClear || mode == Mode::Jungle ? 34.0f : 20.0f);
    return PlayerManaPercent() >= minimum;
}

inline AIHeroClient SelectTarget(const AIHeroClient& selected, float range) {
    return PreferredEnemyTarget(selected, range);
}

inline bool HasNearbyPlant(const Vector3& position, float range = 180.0f) {
    for (const auto& plant : Plants) {
        if (PlantAliveAt(plant, Now()) && plant.Position.Distance2D(position) <= range) return true;
    }
    return false;
}

inline int CountLineBlockers(const Vector3& origin, const Vector3& aim,
                             const AIHeroClient& target) {
    int blockers = 0;
    const int targetId = static_cast<int>(target.NetworkId());
    for (const auto& minion : GameObjects::EnemyLaneMinions()) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable() ||
            minion.NetworkId() == targetId) continue;
        const auto projection = SharedGeometry::ProjectPointToSegment2D(
            minion.Position(), origin, aim);
        if (projection.T > 0.02f && projection.T < 0.98f &&
            projection.Distance <= kEWidth + minion.BoundingRadius()) ++blockers;
    }
    return blockers;
}

inline bool SafeDestination(const Vector3& position, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !FinitePosition(position) || SDK::NavMesh::IsWall(position)) return false;
    if (!reactive && Engine::UnderEnemyTurret(position)) return false;
    return reactive || Engine::CountEnemiesAt(position, 500.0f) <=
        Slider(RMenu, "MaxREnemies", 3);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ManaEnough(mode) || HasSpellShieldOrImmunity(target) ||
        !Engine::ValidEnemy(target, kQRange + 35.0f) ||
        !OffensiveReady(0, mode, reactive)) return false;
    Vector3 aim = PredictPosition(target, kQDelay);
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero() &&
            prediction.Hitchance >= SDK::HitChance::High) aim = prediction.GetCastPosition();
    }
    if (!FinitePosition(aim) || player.Position().Distance2D(aim) > kQRange ||
        SDK::NavMesh::IsWall(aim)) return false;
    bool awakens = false;
    for (const auto& plant : Plants) {
        if (PlantAliveAt(plant, Now()) && QActivatesPlant(player.Position(), aim, plant.Position)) {
            awakens = true;
            break;
        }
    }
    if (!awakens && mode == Mode::Combo && !HasNearbyPlant(aim, 240.0f) &&
        Engine::CountEnemiesAt(aim, 160.0f) == 0) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ManaEnough(mode) || RuntimeSeeds() <= 0 ||
        !Engine::ValidEnemy(target, kWRange + 35.0f) ||
        !OffensiveReady(1, mode, reactive)) return false;
    const Vector3 aim = PredictPosition(target, 0.25f);
    if (!SeedPlacementValid(player.Position(), aim, SDK::NavMesh::IsWall(aim),
                            !reactive && Engine::UnderEnemyTurret(aim))) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ManaEnough(mode) || HasSpellShieldOrImmunity(target) ||
        !Engine::ValidEnemy(target, kERange + 35.0f) ||
        !OffensiveReady(2, mode, reactive)) return false;
    Vector3 aim = PredictPosition(target, kEDelay +
        player.Position().Distance2D(target.Position()) / kESpeed);
    if (Engine::RuntimeSpells[2]) {
        const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
        if (prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero() &&
            prediction.Hitchance >= SDK::HitChance::High) aim = prediction.GetCastPosition();
    }
    const int blockers = CountLineBlockers(player.Position(), aim, target);
    if (!ERootHits(player.Position(), aim, PredictPosition(target, kEDelay),
                   target.BoundingRadius(), blockers,
                   ProjectileWallBlocksFromPlayer(aim, kEWidth))) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline int CountRTargets(const Vector3& center) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy, kRRange + 50.0f) &&
            RZoneContains(center, PredictPosition(enemy, kRDelay), enemy.BoundingRadius())) ++count;
    }
    return count;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ManaEnough(mode) || HasSpellShieldOrImmunity(target) ||
        !Engine::ValidEnemy(target, kRRange + 35.0f) ||
        !OffensiveReady(3, mode, reactive) || PlayerMobilityLocked()) return false;
    const Vector3 center = PredictPosition(target, kRDelay);
    const int hits = CountRTargets(center);
    if (!RZoneContains(center, PredictPosition(target, kRDelay), target.BoundingRadius()) ||
        hits < Slider(RMenu, "MinimumRTargets", 2) ||
        !RCastSafe(center, SDK::NavMesh::IsWall(center), Engine::UnderEnemyTurret(center),
                   Engine::CountEnemiesAt(center, 500.0f), Slider(RMenu, "MaxREnemies", 3)) ||
        (!reactive && Engine::CountEnemiesAt(player.Position(), 500.0f) >
            Slider(RMenu, "MaxCommitEnemies", 3))) return false;
    if (!Engine::ControllerCastPosition(3, center)) return false;
    LastCastTick[3] = Now();
    RTargetId = static_cast<int>(target.NetworkId());
    RPendingUntil = Now() + static_cast<int>(kRDelay * 1000.0f) + 500;
    ROwned = true;
    return true;
}

inline bool Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ManaEnough(mode)) return false;
    if (mode == Mode::LastHit) {
        if (!Ready(0, mode) || !Throttle(0)) return false;
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable() ||
                player.Position().Distance2D(minion.Position()) > kQRange) continue;
            if (!Engine::RuntimeSpells[0] || Engine::RuntimeSpells[0]->GetDamage(minion) < minion.Health()) continue;
            if (Engine::ControllerCastPosition(0, minion.Position())) {
                LastCastTick[0] = Now();
                return true;
            }
        }
        return false;
    }
    if (RuntimeSeeds() > 0 && Ready(1, mode) && Throttle(1) &&
        Bool(FarmMenu, "UseSeeds", true)) {
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (minion.IsValid() && !minion.IsDead() && minion.IsTargetable() &&
                player.Position().Distance2D(minion.Position()) <= kWRange &&
                SeedPlacementValid(player.Position(), minion.Position(),
                    SDK::NavMesh::IsWall(minion.Position()), Engine::UnderEnemyTurret(minion.Position())) &&
                Engine::ControllerCastPosition(1, minion.Position())) {
                LastCastTick[1] = Now();
                return true;
            }
        }
    }
    return false;
}
inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    for (auto& plant : Plants) {
        if (plant.NetworkId == 0) continue;
        const auto object = UnitByNetworkId(plant.NetworkId);
        if (!object.IsValid()) {
            plant.Alive = false;
        } else {
            plant.Position = object.Position();
            plant.Alive = true;
        }
    }
    for (int& id : SeedIds) {
        if (id == 0) continue;
        if (!UnitByNetworkId(id).IsValid()) id = 0;
    }
    ActivePlantCount = PrunePlants(Plants, now);
    ActiveSeedCount = 0;
    for (const int id : SeedIds) if (id != 0) ++ActiveSeedCount;
    if (RPendingUntil != 0 && now >= RPendingUntil) {
        RPendingUntil = 0;
        ROwned = false;
    }
    if (IncomingHardCcUntil <= now) IncomingHardCcUntil = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    const Mode decisionMode = mode == Mode::None ? Mode::Automatic : mode;
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const auto target = SelectTarget(selected, kERange + 35.0f);
    if (!Engine::ValidEnemy(target)) {
        return (decisionMode == Mode::LaneClear || decisionMode == Mode::Jungle ||
                decisionMode == Mode::LastHit) ? Farm(decisionMode) : false;
    }
    switch (decisionMode) {
    case Mode::Combo:
        if (CastE(target, decisionMode)) return true;
        if (CastW(target, decisionMode)) return true;
        if (CastQ(target, decisionMode)) return true;
        return CastR(target, decisionMode);
    case Mode::Harass:
        if (CastE(target, decisionMode)) return true;
        if (HasNearbyPlant(target.Position(), 240.0f)) return CastQ(target, decisionMode);
        return false;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        return Farm(decisionMode);
    case Mode::Flee:
        if (CastE(target, decisionMode, true)) return true;
        return CastR(target, decisionMode, true);
    case Mode::Automatic:
        if (IncomingHardCcUntil > Now() && CastE(target, decisionMode, true)) return true;
        if (CastE(target, decisionMode, true)) return true;
        if (CastW(target, decisionMode, true)) return true;
        if (CastQ(target, decisionMode, true)) return true;
        return CastR(target, decisionMode, true);
    default:
        return false;
    }
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (!Engine::WasControllerCast(slot)) ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 600);
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args, 220.0f, 100.0f, 300, 250, 220, 1500, 500);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) {
        IncomingHardCcUntil = std::max(IncomingHardCcUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
        if (ROwned) ROwned = false;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId != 0 ? args.TargetNetworkId : args.Target.NetworkId);
        LastAutoTick = Now();
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender) && TextContainsAny(args.BuffName, {"ZyraR", "zyraroot"})) ROwned = true;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender) && TextContainsAny(args.BuffName, {"ZyraR", "zyraroot"})) ROwned = false;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (ROwned) args.Process = false;
    (void)args;
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) LastAutoTick = Now();
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)args;
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 800);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptExpireTick, 1100, 250, 5000);
    IncomingHardCcUntil = std::max(IncomingHardCcUntil, InterruptExpireTick);
}

inline PlantKind PlantKindFromName(const char* name) {
    if (TextContainsAny(name, {"Thorn", "thornspitter", "ZyraQPlant"})) return PlantKind::ThornSpitter;
    if (TextContainsAny(name, {"Vine", "vinelasher", "ZyraEPlant"})) return PlantKind::VineLasher;
    return PlantKind::Unknown;
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const PlantKind kind = PlantKindFromName(args.Sender.Name);
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (kind != PlantKind::Unknown && id != 0) {
        for (auto& plant : Plants) {
            if (plant.NetworkId == 0) {
                plant = BeginPlant(id, kind, args.Sender.Position, Now());
                break;
            }
        }
        return;
    }
    if (TextContainsAny(args.Sender.Name, {"ZyraSeed", "zyraseed"}) && id != 0) {
        for (int& seed : SeedIds) if (seed == 0) { seed = id; break; }
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    for (auto& plant : Plants) if (plant.NetworkId == id) plant = {};
    for (int& seed : SeedIds) if (seed == id) seed = 0;
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) { (void)args; }

inline void OnDraw() {
    if (!CoachMenu || !Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFCC66AAu, 1.2f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF66CCAAu, 1.2f, 40);
    Drawing::DrawCircle(player.Position(), kRRange, ROwned ? 0xFFFF5555u : 0xFFCC66FFu, 1.5f, 40);
    for (const auto& plant : Plants) if (PlantAliveAt(plant, Now()))
        Drawing::DrawCircle(plant.Position, 80.0f, 0xFF55DD88u, 1.0f, 20);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("ZyraOneTrick", "Zyra seed, plant and root control"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 600, 180, 1400));
    QMenu = TacticsMenu->AddSubMenu(new Menu("DeadlySpines", "Q plant awakening"));
    QMenu->Add(new MenuBool("RequirePlantCombo", "Prefer Q when a seed or plant is present", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("RampantGrowth", "W seed placement"));
    WMenu->Add(new MenuBool("ReserveSeed", "Reserve one seed for combo", true));
    EMenu = TacticsMenu->AddSubMenu(new Menu("GraspingRoots", "Predicted root collision"));
    EMenu->Add(new MenuBool("RespectProjectileWalls", "Reject E through projectile walls", true));
    RMenu = TacticsMenu->AddSubMenu(new Menu("Stranglethorns", "Knock-up zone safety"));
    RMenu->Add(new MenuSlider("MinimumRTargets", "Minimum champions in R zone", 2, 1, 5));
    RMenu->Add(new MenuSlider("MaxREnemies", "Maximum enemies at R destination", 3, 0, 5));
    RMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at player", 3, 0, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("ZyraFarm", "Distinct lane and jungle policy"));
    FarmMenu->Add(new MenuBool("UseSeeds", "Use W seeds for lane and jungle", true));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("ZyraCoach", "Plant lifecycle telemetry"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E/R ranges and live plants", false));
}

inline void OnLoad() {
    LastCastTick = {};
    Plants = {};
    SeedIds = {};
    ActivePlantCount = ActiveSeedCount = 0;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingHardCcUntil = 0;
    InterruptTargetId = InterruptExpireTick = RTargetId = RPendingUntil = 0;
    ROwned = false;
    LastAutoTargetId = LastAutoTick = 0;
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Pin Riot live 26.15 and CommunityDragon PC 16.15 Zyra values and runtime aliases",
    "Track passive seeds, W seed placement and Q/E-awakened plant objects through events and polling",
    "Expire Thorn Spitters and Vine Lashers after their eight-second plant lifetime",
    "Place W seeds only in reachable non-wall, non-turret terrain with a selected target then orbwalker fallback",
    "Use Q to awaken nearby plants while retaining direct ground damage when enemies commit",
    "Predict E roots at impact time, reject minion collision and projectile-wall blocked paths",
    "Count enemy champions inside the predicted R plant knock-up zone before committing",
    "Reject R centers in terrain, under enemy turret, or beyond the configured enemy-count gate",
    "Preserve auto-attack windup and yield after manual player spell ownership",
    "Reconcile R knock-up state and interrupt ownership on hard crowd-control pressure",
    "Handle Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes distinctly",
    "Use live spell readiness, mana, cooldown, mitigated damage, and champion target validity",
    "Complete every ChampionController callback including object and missile lifecycle hooks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Zyra;
    controller.ControllerId = "champion.kuroaio.ai.zyra.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIZyra.md";
    controller.ImplementationSummary =
        "Seed and plant object lifecycle reconciliation, Q/E plant awakening, W seed placement, "
        "predicted collision-aware roots, and terrain/turret-safe R knock-up zones.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Zyra
