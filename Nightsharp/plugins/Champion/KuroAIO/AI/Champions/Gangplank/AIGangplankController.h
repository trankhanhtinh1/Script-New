#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIGangplankGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <string>

namespace Plugins::KuroAIO::AI::Controllers::Gangplank {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasAnyBuff;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::ObjectEventIsAllied;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<BarrelState, kMaximumTrackedBarrels> Barrels{};
inline TrialByFireState PassiveState{};
inline int LastCastTick[4]{};
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int ForcedBarrelId = 0;

inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int CleanseThreatUntil = 0;
inline int LastBarrelRefreshTick = 0;
inline int LastModeTick = 0;
inline Mode LastMode = Mode::None;
inline UltimateUpgrades Upgrades{};

using ControllerHelpers::Ready;

inline bool Throttle(int slot, int delay = 90) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

using ControllerHelpers::PreserveAttack;

using ControllerHelpers::Protected;

using ControllerHelpers::TotalAttackDamage;

using ControllerHelpers::AP;

inline bool IsBarrelName(const char* first, const char* second = nullptr) {
    return ControllerHelpers::AnyTextContains(
        { first, second }, { "GangplankBarrel", "gangplankbarrel", "PowderKeg" });
}

inline bool IsBarrelObject(const SDK::Events::ObjectEventArgs& args) {
    return args.Sender.IsValid() && IsBarrelName(
        args.Sender.Name, args.Sender.CharacterName);
}

inline BarrelState* FindBarrel(int id) {
    if (id == 0) return nullptr;
    for (auto& barrel : Barrels) {
        if (barrel.NetworkId == id) return &barrel;
    }
    return nullptr;
}

inline const BarrelState* NearestBarrel(const Vec3& origin, float range = FLT_MAX) {
    const BarrelState* best = nullptr;
    float bestDistance = range;
    for (const auto& barrel : Barrels) {
        if (!barrel.Alive || !barrel.Position.IsValid()) continue;
        const float distance = origin.Distance2D(barrel.Position);
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = &barrel;
        }
    }
    return best;
}

inline AIMinionClient BarrelUnit(const BarrelState& barrel) {
    return barrel.NetworkId != 0
        ? GameObjects::GetUnitByNetworkId<AIMinionClient>(barrel.NetworkId)
        : AIMinionClient{};
}

inline int ObservedBarrelHealth(int id) {
    const auto unit = GameObjects::GetUnitByNetworkId<AIMinionClient>(id);
    if (!unit.IsValid()) return 3;
    return std::clamp(static_cast<int>(std::ceil(unit.Health())), 1, 3);
}

inline void ObserveBarrel(int id, const Vec3& position, int health,
                          bool allied, int now) {
    if (id == 0 || !position.IsValid() || position.IsZero()) return;
    BarrelState* slot = FindBarrel(id);
    if (!slot) {
        for (auto& candidate : Barrels) {
            if (!candidate.Alive || IsBarrelExpired(candidate, now)) {
                slot = &candidate;
                break;
            }
        }
    }
    if (!slot) return;
    *slot = ReconcileBarrelObservation(*slot, id, position, health, allied, now);
}

inline void RefreshBarrels() {
    const int now = Now();
    if (now - LastBarrelRefreshTick < 80) return;
    LastBarrelRefreshTick = now;
    std::array<bool, kMaximumTrackedBarrels> seen{};
    for (const auto& object : ObjectManager::AllObjects()) {
        if (!object.IsValid() || !IsBarrelName(object.Name().c_str(),
                                                object.CharacterName().c_str())) {
            continue;
        }
        const auto player = GameObjects::Player();
        if (player.IsValid() && object.Team() != player.Team()) continue;
        const int id = object.NetworkId();
        ObserveBarrel(id, object.Position(), ObservedBarrelHealth(id), true, now);
        for (std::size_t i = 0; i < Barrels.size(); ++i) {
            if (Barrels[i].NetworkId == id) seen[i] = true;
        }
    }
    for (std::size_t i = 0; i < Barrels.size(); ++i) {
        auto& barrel = Barrels[i];
        if (barrel.Alive && (!seen[i] && now - barrel.LastSeenTick > 320)) {
            barrel.Alive = false;
        }
        if (IsBarrelExpired(barrel, now)) barrel.Alive = false;
    }
}

inline int BarrelIndex(const BarrelState& barrel) {
    for (std::size_t i = 0; i < Barrels.size(); ++i) {
        if (Barrels[i].NetworkId == barrel.NetworkId) return static_cast<int>(i);
    }
    return -1;
}

inline bool TargetInChain(const AIHeroClient& target, const BarrelState& source) {
    if (!Engine::ValidEnemy(target)) return false;
    const int index = BarrelIndex(source);
    if (index < 0) return false;
    const auto chain = BuildBarrelChain(Barrels, static_cast<std::size_t>(index));
    return ChainHitsPosition(Barrels, chain, PredictPosition(target, 0.25f),
                             target.BoundingRadius());
}

inline bool FarmInChain(const BarrelState& source) {
    const int index = BarrelIndex(source);
    if (index < 0) return false;
    const auto chain = BuildBarrelChain(Barrels, static_cast<std::size_t>(index));
    for (const auto& minion : GameObjects::EnemyLaneMinions()) {
        if (minion.IsValid() && !minion.IsDead() && minion.IsTargetable() &&
            ChainHitsPosition(Barrels, chain, minion.Position(), minion.BoundingRadius())) {
            return true;
        }
    }
    return false;
}

inline bool EnemyCanWinBarrelRace(const BarrelState& source) {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && source.Position.Distance2D(enemy.Position()) <= 425.0f) {
            return true;
        }
    }
    return false;
}

inline bool ShouldTriggerBarrel(const BarrelState& source,
                                const AIHeroClient& target,
                                bool attackWindup,
                                bool exactAttackTarget,
                                bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float attackRange = player.AttackRange() + player.BoundingRadius() + 35.0f;
    const int level = std::max(1, player.Level());
    if (!IsBarrelTriggerTarget(source, player.Position(), attackRange, level, Now())) return false;
    const bool targetHit = TargetInChain(target, source);
    const bool farm = FarmInChain(source);
    const BarrelTriggerContext context{
        true, true, attackWindup, exactAttackTarget, targetHit,
        EnemyCanWinBarrelRace(source), farm, lethal, 1};
    return MayTriggerBarrel(context);
}

inline bool CastQBarrel(const BarrelState& barrel, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    const auto unit = BarrelUnit(barrel);
    if (!player.IsValid() || !unit.IsValid() || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive) || !ShouldTriggerBarrel(barrel, {}, false, false, reactive)) {
        return false;
    }
    if (!Engine::ControllerCastUnit(0, unit)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastQTarget(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 100.0f) ||
        Protected(target) || !Ready(0, mode) || !Throttle(0) || PreserveAttack(reactive)) {
        return false;
    }
    const bool lethal = player.CalculatePhysicalDamage(target,
        ParrrleyRawDamage(SpellRank(0), TotalAttackDamage())) >=
        target.Health() + target.AllShield();
    if (!reactive && Orbwalker::CanAttack() &&
        player.Position().Distance2D(target.Position()) <=
            player.AttackRange() + target.BoundingRadius() + 20.0f && !lethal) {
        return false;
    }
    if (!Engine::ControllerCastUnit(0, target)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastQMinion(const AIMinionClient& minion, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !minion.IsValid() || !Ready(0, mode) ||
        !Throttle(0) || PreserveAttack(false)) return false;
    if (!Engine::ControllerCastUnit(0, minion)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastBarrel(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2) || PreserveAttack(reactive)) return false;
    const Vec3 desired = Engine::ValidEnemy(target)
        ? PredictPosition(target, 0.25f)
        : player.Position() + SharedGeometry::Direction2D(player.Position(), Game::CursorPos()) * 550.0f;
    if (!desired.IsValid() || desired.IsZero() ||
        player.Position().Distance2D(desired) > kEPlacementRange || SDK::NavMesh::IsWall(desired)) return false;
    const BarrelState* anchor = NearestBarrel(desired, kBarrelChainRange);
    Vec3 castPosition = desired;
    if (anchor) castPosition = ChainPlacement(anchor->Position, desired);
    if (!castPosition.IsValid() || castPosition.IsZero()) return false;
    if (!Engine::ControllerCastPosition(2, castPosition)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastRemoveScurvy(Mode mode, bool reactive = true) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 120)) return false;
    const bool airborne = SDK::HasBuffOfType(player, SDK::BuffType::Knockup) ||
        SDK::HasBuffOfType(player, SDK::BuffType::Knockback);
    const bool cc = Engine::IsHardCrowdControlled(player) || CleanseThreatUntil > Now();
    const CleanseContext context{
        true, cc, airborne, false, Orbwalker::IsWindingUp(),
        player.HealthPercent() <= Slider(WMenu, "EmergencyHp", 28),
        std::max(0, CleanseThreatUntil - Now()),
        Slider(WMenu, "MinimumCcMs", 450)};
    if (!ShouldCastRemoveScurvy(context)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    CleanseThreatUntil = 0;
    return true;
}

inline bool CastCannonBarrage(const AIHeroClient& selected, Mode mode,
                              bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 150) ||
        PreserveAttack(reactive)) return false;
    const AIHeroClient target = Engine::ValidEnemy(selected, 5000.0f)
        ? selected : NearestEnemyToPlayer(selected, 5000.0f);
    if (!Engine::ValidEnemy(target)) return false;
    const Vec3 position = PredictPosition(target, 0.65f);
    if (!position.IsValid() || position.IsZero()) return false;
    int enemies = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && position.Distance2D(PredictPosition(enemy, 0.65f)) <=
            kRRadius + enemy.BoundingRadius()) ++enemies;
    }
    const int allies = Engine::CountAlliesAt(position, kRRadius);
    const float expected = player.CalculateMagicDamage(target,
        CannonBarrageConservativeRawDamage(SpellRank(3), AP(), 2, Upgrades));
    const RPolicyContext context{
        true, true, false,
        player.Position().Distance2D(target.Position()) <= 1000.0f,
        Engine::IsHardCrowdControlled(target),
        !target.IsDashing(), expected >= target.Health() + target.AllShield(),
        allies > 0 && allies < 100 && target.HealthPercent() < 40.0f,
        ControllerHelpers::HasNearbyEpicMonster(1800.0f), enemies, allies,
        Slider(RMenu, "MinimumTargets", 2)};
    if (!ShouldCastCannonBarrage(context)) return false;
    if (!Engine::ControllerCastPosition(3, position)) return false;
    LastCastTick[3] = Now();
    return true;
}

inline bool TryForceBarrelAttack(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Orbwalker::CanAttack()) return false;
    for (const auto& barrel : Barrels) {
        if (!ShouldTriggerBarrel(barrel, target, Orbwalker::IsWindingUp(),
                                 ForcedBarrelId == barrel.NetworkId)) continue;
        const auto unit = BarrelUnit(barrel);
        if (!unit.IsValid()) continue;
        Orbwalker::ForceTarget(AttackableUnit(unit.Handle()));
        ForcedBarrelId = barrel.NetworkId;
        return true;
    }
    if (ForcedBarrelId != 0) {
        Orbwalker::ForceTarget(AttackableUnit());
        ForcedBarrelId = 0;
    }
    return false;
}

inline bool CastNearestBarrelQ(const Vec3& anchor, Mode mode) {
    const BarrelState* barrel = NearestBarrel(anchor, kQRange);
    return barrel && CastQBarrel(*barrel, mode);
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (TryForceBarrelAttack(target)) return;
    if (CastNearestBarrelQ(target.Position(), Mode::Combo)) return;
    if (CastQTarget(target, Mode::Combo)) return;
    if (CastBarrel(target, Mode::Combo)) return;
    (void)CastCannonBarrage(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 45)) return;
    if (TryForceBarrelAttack(target)) return;
    if (CastNearestBarrelQ(target.Position(), Mode::Harass)) return;
    if (CastQTarget(target, Mode::Harass)) return;
    (void)CastBarrel(target, Mode::Harass);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 30)) return;
    const AIHeroClient noTarget{};
    if (TryForceBarrelAttack(noTarget)) return;
    if (mode == Mode::LastHit) {
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable() ||
                player.Position().Distance2D(minion.Position()) > kQRange) continue;
            const float qDamage = player.CalculatePhysicalDamage(minion,
                ParrrleyRawDamage(SpellRank(0), TotalAttackDamage()));
            if (qDamage >= minion.Health() && CastQMinion(minion, mode)) return;
        }
    }
    if (Ready(2, mode)) (void)CastBarrel(noTarget, mode);
    (void)Engine::TryFarm(mode);
}

inline void ReconcileState() {
    RefreshBarrels();
    PassiveState = NormalizeTrialByFire(PassiveState, Now());
    if (IncomingThreatUntil <= Now()) IncomingThreatUntil = 0;
    if (IncomingHardCCUntil <= Now()) IncomingHardCCUntil = 0;
    if (CleanseThreatUntil <= Now()) CleanseThreatUntil = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    LastModeTick = Now();
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, 5000.0f);
    if (CastRemoveScurvy(mode, true)) return true;
    if (false) return true;
    if (mode == Mode::Automatic && IncomingThreatUntil > Now() &&
        Engine::ValidEnemy(target)) {
        (void)CastRemoveScurvy(mode, true);
        return true;
    }
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee:
        (void)CastRemoveScurvy(mode, true);
        if (Engine::ValidEnemy(target)) (void)CastCannonBarrage(target, mode, true);
        break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target)) (void)CastCannonBarrage(target, mode, true);
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = now;
        }
        if (args.IsAutoAttack) PassiveState = ConsumeTrialByFire(now);
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) {
        IncomingHardCCUntil = std::max(IncomingHardCCUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
        CleanseThreatUntil = std::max(CleanseThreatUntil, IncomingHardCCUntil);
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (args.IsAutoAttack) {
        PassiveState = ConsumeTrialByFire(Now());
        LastAfterAttackTick = Now();
        LastAfterAttackTargetId = static_cast<int>(args.TargetNetworkId);
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "GangplankPassive")) PassiveState = { true, 0 };
        if (Engine::TextContains(args.BuffName, "DeathsDaughter")) Upgrades.DeathsDaughter = true;
        if (Engine::TextContains(args.BuffName, "FireAtWill")) Upgrades.FireAtWill = true;
        if (Engine::TextContains(args.BuffName, "RaiseMorale")) Upgrades.RaiseMorale = true;
        if (Engine::TextContains(args.BuffName, "stun") ||
            Engine::TextContains(args.BuffName, "root") ||
            Engine::TextContains(args.BuffName, "snare") ||
            Engine::TextContains(args.BuffName, "silence")) {
            CleanseThreatUntil = std::max(CleanseThreatUntil, Now() + 700);
        }
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    (void)args;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    if (args.Target.IsHero()) {
        LastAfterAttackTargetId = static_cast<int>(args.Target.NetworkId());
        return;
    }
    for (const auto& barrel : Barrels) {
        if (static_cast<int>(args.Target.NetworkId()) == barrel.NetworkId) {
            ForcedBarrelId = barrel.NetworkId;
            return;
        }
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAfterAttackTargetId, LastAfterAttackTick);
    for (auto& barrel : Barrels) {
        if (barrel.NetworkId == LastAfterAttackTargetId) {
            barrel.Alive = false;
            PassiveState = RefreshTrialByFireFromBarrel();
        }
    }
    ForcedBarrelId = 0;
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!IsBarrelObject(args) || !ObjectEventIsAllied(args)) return;
    ObserveBarrel(static_cast<int>(args.Sender.NetworkId), args.Sender.Position, 3, true, Now());
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    BarrelState* barrel = FindBarrel(id);
    if (barrel && (IsBarrelName(args.Sender.Name, args.Sender.CharacterName) || barrel->Alive)) {
        barrel->Alive = false;
        barrel->LastSeenTick = Now();
        PassiveState = RefreshTrialByFireFromBarrel();
    }
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFFFAA44u, 1.5f, 40);
    for (const auto& barrel : Barrels) {
        if (barrel.Alive) Drawing::DrawCircle(barrel.Position, kBarrelExplosionRadius,
            barrel.ObservedHealth == 1 ? 0xFFFF4444u : 0xFF44AAFFu, 1.0f, 32);
    }
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("GangplankOneTrick", "Gangplank barrel tactics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Parrrley"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Remove Scurvy"));
    WMenu->Add(new MenuSlider("MinimumCcMs", "Minimum removable CC (ms)", 450, 150, 1200));
    WMenu->Add(new MenuSlider("EmergencyHp", "Emergency cleanse HP", 28, 5, 70));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Powder Keg"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Cannon Barrage"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum nonlethal targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("GangplankFarm", "Q and keg farm"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum farm mana percent", 30, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("GangplankCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw barrel trigger zones", false));
}

inline void OnLoad() {
    Barrels = {};
    PassiveState = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAfterAttackTargetId = LastAfterAttackTick = ForcedBarrelId = 0;
    IncomingThreatUntil = IncomingHardCCUntil = CleanseThreatUntil = 0;
    LastBarrelRefreshTick = LastModeTick = 0;
    LastMode = Mode::None;
    Upgrades = {};
}

inline void OnUnload() {
    if (ForcedBarrelId != 0) Orbwalker::ForceTarget(AttackableUnit());
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Barrels = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin spell values and barrel timing to Riot 26.15 / CommunityDragon 16.15",
    "Reconcile allied Powder Keg objects from create, delete and polling observations",
    "Read barrel health from the live object and predict the next health tick conservatively",
    "Expire tracked barrels after lifetime or a bounded missing-object window",
    "Build connected barrel chains with the live 650 range and 325 explosion radius",
    "Place E at a clamped chain endpoint while respecting walls and cursor intent",
    "Trigger a one-health barrel with Q only when the chain target or farm value is observed",
    "Force the orbwalker to a safe one-health barrel for an auto trigger",
    "Reject Q and AA barrel races that an enemy can win before impact",
    "Preserve selected target first, then orbwalker target, then selector fallback",
    "Preserve an AA windup unless a lethal or explicit barrel trigger is committed",
    "Reconcile player-triggered Q W E or R state/",
    "Reconcile Trial by Fire cooldown and refresh it on observed barrel detonations",
    "Use W for removable crowd control and missing-health sustain, never airborne cleanse",
    "Use Q as an attack weave and conservative last-hit route without stealing a ready AA",
    "Use E for combo setup, harass setup, lane clear and jungle setup",
    "Use R only for lethal, ally-danger, objective or reliable multi-target value",
    "Track Cannon Barrage upgrade telemetry and choose a conservative upgrade policy",
    "Automatic mode is reactive and never starts an unobserved global engage",
    "Flee mode prioritizes cleanse and defensive barrage over local damage",
    "LaneClear, Jungle and LastHit each retain explicit mana and farm policy",
    "Reject protected, invulnerable and spell-shielded enemy targets",
    "Expose health, chain and range state through optional coach drawing only",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Gangplank;
    controller.ControllerId = "champion.kuroaio.ai.gangplank.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIGangplank.md";
    controller.ImplementationSummary =
        "Barrel object/health reconciliation, race-safe Q and AA triggers, passive timing, W cleanse and conservative global R policy.";
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
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Gangplank
