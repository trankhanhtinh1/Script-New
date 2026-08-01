#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AITeemo.h"
#include "AITeemoGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Teemo {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Slider;

inline Menu* TacticsMenu = nullptr;
inline std::array<int, 4> LastCastTick{};
inline std::array<int, 16> PoisonTargetId{};
inline std::array<int, 16> PoisonExpireTick{};
inline std::array<int, 16> PoisonAppliedTick{};
inline int LastAttackTargetId = 0;
inline int IncomingThreatTargetId = 0;
inline int IncomingThreatUntil = 0;
inline Vector3 IncomingThreatEndpoint{};
inline int LastMushroomCastTick = 0;
inline int LastMushroomId = 0;

struct MushroomState {
    int NetworkId = 0;
    Vector3 Position{};
    int CastTick = 0;
    int ArmTick = 0;
    bool Alive = false;
    bool Armed = false;
    bool Vision = false;
};
inline std::array<MushroomState, 24> Mushrooms{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        (reactive || LastCastTick[static_cast<std::size_t>(slot)] + 42 <= Now());
}

inline bool MushroomObject(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
        {args.Sender.Name, args.Sender.CharacterName, args.SpellName, args.MissileName},
        {"teemomushroom", "teemortrap", "teemor"});
}

inline MushroomState* FindMushroom(int id, bool create = false) {
    if (id == 0) return nullptr;
    for (auto& mushroom : Mushrooms) if (mushroom.NetworkId == id) return &mushroom;
    if (!create) return nullptr;
    for (auto& mushroom : Mushrooms) {
        if (!mushroom.Alive) {
            mushroom = {};
            mushroom.NetworkId = id;
            return &mushroom;
        }
    }
    return &Mushrooms.front();
}

inline int SpellRank(int slot) {
    if (slot < 0 || slot >= 4) return 0;
    const auto spell = GameObjects::Player().Spellbook().GetSpell(
        static_cast<SDK::SpellSlot>(slot));
    return spell.IsValid() ? std::clamp(spell.Level(), 0, 5) : 0;
}

inline bool HasPoison(const AIHeroClient& target) {
    const int id = static_cast<int>(target.NetworkId());
    for (std::size_t i = 0; i < PoisonTargetId.size(); ++i) {
        if (PoisonTargetId[i] == id && PoisonExpireTick[i] >= Now()) return true;
    }
    return false;
}

inline void ObservePoison(int id, int durationMs = 4000) {
    if (id == 0) return;
    std::size_t slot = 0;
    for (std::size_t i = 0; i < PoisonTargetId.size(); ++i) {
        if (PoisonTargetId[i] == id) { slot = i; break; }
        if (PoisonTargetId[i] == 0 || PoisonExpireTick[i] < Now()) slot = i;
    }
    PoisonTargetId[slot] = id;
    PoisonAppliedTick[slot] = Now();
    PoisonExpireTick[slot] = Now() + durationMs;
}

inline void ReconcilePoison() {
    const int now = Now();
    for (std::size_t i = 0; i < PoisonTargetId.size(); ++i) {
        if (PoisonTargetId[i] != 0 && PoisonExpireTick[i] < now) {
            PoisonTargetId[i] = 0;
            PoisonAppliedTick[i] = 0;
            PoisonExpireTick[i] = 0;
        }
    }
}

inline bool TargetSafeToCommit(const AIHeroClient& target, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return false;
    if (!lethal && Engine::UnderEnemyTurret(target.Position())) return false;
    if (!lethal && Engine::CountEnemiesAt(target.Position(), 650.0f) >
        Slider(TacticsMenu, "MaxCommitEnemies", 2)) return false;
    if (target.IsInvulnerable() || ControllerHelpers::HasSpellShieldOrImmunity(target)) return false;
    return true;
}

inline float ExpectedPoisonDamage(const AIHeroClient& target, float seconds = 4.0f) {
    const auto player = GameObjects::Player();
    return player.IsValid() ? player.CalculateMagicDamage(target,
        PoisonTotalDamage(SpellRank(2), player.AP(), player.BonusAttackDamage(), seconds)) : 0.0f;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 40.0f) ||
        !Ready(0, mode, reactive) || ControllerHelpers::PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    if (!prediction.GetCastPosition().IsValid() || prediction.GetCastPosition().IsZero() ||
        !prediction.CollisionObjects.empty()) return false;
    const Vector3 aim = prediction.GetCastPosition();
    if (!BlindingDartReach(player.Position(), aim, target.BoundingRadius()) ||
        !ProjectilePathClear(player.Position(), aim, aim, 100.0f, target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 44.0f)) return false;
    if (Engine::ControllerCastPredicted(0, target, SDK::HitChance::High)) {
        LastCastTick[0] = Now();
        ObservePoison(static_cast<int>(target.NetworkId()));
        return true;
    }
    return false;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) ||
        (!reactive && player.HealthPercent() < Slider(TacticsMenu, "MinimumWHealth", 24)) ||
        (!reactive && !Engine::CanAct(false))) return false;
    const bool chasing = target.IsValid() &&
        player.Position().Distance2D(target.Position()) > 350.0f;
    const bool fleeing = mode == Mode::Flee || IncomingThreatUntil >= Now();
    if (!chasing && !fleeing && mode != Mode::Jungle) return false;
    if (Engine::ControllerCastSelf(1)) {
        LastCastTick[1] = Now();
        return true;
    }
    return false;
}

inline bool CastMushroom(const Vector3& requested, const AIHeroClient& target,
                         Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode, reactive) || !requested.IsValid() || requested.IsZero() ||
        (!reactive && Orbwalker::IsWindingUp() && Bool(Engine::HumanMenu, "PreserveAttacks", true))) return false;
    const bool turret = Engine::UnderEnemyTurret(requested);
    const int enemies = Engine::CountEnemiesAt(requested, 500.0f);
    const bool wall = SDK::NavMesh::IsWall(requested);
    if (!TrapLandingValid(player.Position(), requested, Engine::RuntimeSpells[3]->CurrentRange(),
                          wall, turret, enemies, Slider(TacticsMenu, "MaxTrapEnemies", 2))) return false;
    if (target.IsValid() && requested.Distance2D(PredictPosition(target, 0.45f)) < 110.0f &&
        mode != Mode::Flee && enemies == 0) return false;
    if (!Engine::ControllerCastPosition(3, requested)) return false;
    LastCastTick[3] = LastMushroomCastTick = Now();
    LastMushroomId = 0;
    return true;
}

inline Vector3 BestTrapPoint(const AIHeroClient& target, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    std::array<Vector3, 8> candidates{};
    std::size_t count = 0;
    if (target.IsValid()) {
        const Vector3 predicted = PredictPosition(target, 0.45f);
        candidates[count++] = predicted;
        const Vector3 toward = Direction2D(player.Position(), predicted);
        if (!toward.IsZero()) {
            candidates[count++] = predicted + toward * 120.0f;
            candidates[count++] = predicted - toward * 130.0f;
        }
    }
    candidates[count++] = Game::CursorPos();
    if (IncomingThreatEndpoint.IsValid()) candidates[count++] = IncomingThreatEndpoint;
    float bestScore = -100000.0f;
    Vector3 best{};
    for (std::size_t i = 0; i < count; ++i) {
        const auto& point = candidates[i];
        if (!TrapLandingValid(player.Position(), point, Engine::RuntimeSpells[3]
            ? Engine::RuntimeSpells[3]->CurrentRange() : kRRange, SDK::NavMesh::IsWall(point),
            Engine::UnderEnemyTurret(point), Engine::CountEnemiesAt(point, 500.0f),
            Slider(TacticsMenu, "MaxTrapEnemies", 2))) continue;
        const bool choke = i > 0 || mode == Mode::Flee;
        const float score = TrapValue(point, target.IsValid() ? PredictPosition(target, 0.45f) : point,
            Engine::CountEnemiesAt(point, 500.0f), choke, Engine::UnderEnemyTurret(point));
        if (score > bestScore) { bestScore = score; best = point; }
    }
    return best;
}

inline void Combo(const AIHeroClient& target) {
    if (!TargetSafeToCommit(target)) return;
    const bool lethalPoison = ExpectedPoisonDamage(target) >= target.Health() + target.AllShield();
    if (CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastMushroom(BestTrapPoint(target, Mode::Combo), target, Mode::Combo)) return;
    if (lethalPoison && !HasPoison(target)) (void)CastQ(target, Mode::Combo, true);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "HarassMana", 42) ||
        !TargetSafeToCommit(target)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (!HasPoison(target) && CastMushroom(BestTrapPoint(target, Mode::Harass), target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (mode == Mode::Jungle && Ready(3, mode) && player.ManaPercent() >= 30.0f &&
        ControllerHelpers::HasNearbyJungleTarget(850.0f)) {
        const auto monster = ControllerHelpers::SelectJungleTarget(850.0f);
        (void)CastMushroom(monster.IsValid() ? monster.Position() : Game::CursorPos(), {}, mode);
    }
    (void)Engine::TryFarm(mode);
}

inline void LastHit() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < 28.0f || !Ready(0, Mode::LastHit)) return;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable()) continue;
        const float damage = player.CalculateMagicDamage(minion,
            PoisonImpactDamage(SpellRank(0), player.AP(), player.BonusAttackDamage()));
        if (damage >= minion.Health() && player.Position().Distance2D(minion.Position()) <= kQRange) {
            if (Engine::ControllerCastUnit(0, minion)) { LastCastTick[0] = Now(); return; }
        }
    }
}

inline void Flee(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Engine::ValidEnemy(target, 700.0f) && CastW(target, Mode::Flee, true)) return;
    const Vector3 cursor = Game::CursorPos();
    if (Ready(3, Mode::Flee, true) && SafeFleeDestination(player.Position(), cursor,
            target.IsValid() ? target.Position() : Vector3{}, 500.0f,
            SDK::NavMesh::IsWall(cursor), Engine::UnderEnemyTurret(cursor),
            Engine::CountEnemiesAt(cursor, 500.0f), 1)) {
        if (CastMushroom(cursor, target, Mode::Flee, true)) return;
    }
    (void)CastQ(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (IncomingThreatUntil >= Now() && Engine::ValidEnemy(target, kQRange) &&
        CastQ(target, Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(target, 900.0f) && target.IsDashing() &&
        CastMushroom(BestTrapPoint(target, Mode::Automatic), target, Mode::Automatic, true)) return;
    if (player.HealthPercent() < Slider(TacticsMenu, "EmergencyHealth", 27)) {
        (void)CastW(target, Mode::Automatic, true);
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcilePoison();
    const int now = Now();
    for (auto& mushroom : Mushrooms) {
        if (!mushroom.Alive) continue;
        mushroom.Armed = TrapCanArm(mushroom.CastTick, now);
        mushroom.Vision = mushroom.Armed;
        if (mushroom.Position.IsZero() || mushroom.Position.Distance2D(GameObjects::Player().Position()) > 3000.0f)
            mushroom.Alive = false;
    }
    if (IncomingThreatUntil < now) { IncomingThreatUntil = 0; IncomingThreatTargetId = 0; IncomingThreatEndpoint = {}; }
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, 980.0f);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle: Farm(mode); break;
    case Mode::LastHit: LastHit(); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Teemo poison and mushroom tactics"));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 42, 0, 100));
    TacticsMenu->Add(new MenuSlider("MinimumWHealth", "Minimum health for W pursuit", 24, 1, 100));
    TacticsMenu->Add(new MenuSlider("EmergencyHealth", "Emergency W health", 27, 1, 100));
    TacticsMenu->Add(new MenuSlider("MaxTrapEnemies", "Maximum enemies at mushroom", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at commit point", 2, 1, 5));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    PoisonTargetId.fill(0);
    PoisonExpireTick.fill(0);
    PoisonAppliedTick.fill(0);
    Mushrooms.fill({});
    LastAttackTargetId = 0;
    IncomingThreatTargetId = 0;
    IncomingThreatUntil = 0;
    IncomingThreatEndpoint = {};
    LastMushroomCastTick = 0;
    LastMushroomId = 0;
}

inline void OnUnload() { TacticsMenu = nullptr; OnLoad(); }


inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (args.IsAutoAttack && args.TargetNetworkId != 0) {
        LastAttackTargetId = static_cast<int>(args.TargetNetworkId);
        ObservePoison(LastAttackTargetId);
    }
    const int slot = static_cast<int>(args.Slot);
    if (slot >= 0 && slot < 4 && Engine::WasControllerCast(slot)) {
        LastCastTick[static_cast<std::size_t>(slot)] = Now();
    }
}
inline bool PoisonBuffName(const char* name) {
    return ControllerHelpers::AnyTextContains(
        {name}, {"teemopoison", "teemoedot", "toxicshot"});
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) || !PoisonBuffName(args.BuffName)) return;
    ObservePoison(static_cast<int>(args.Sender.NetworkId));
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == 0) return;
    for (std::size_t i = 0; i < PoisonTargetId.size(); ++i) {
        if (PoisonTargetId[i] == id && PoisonBuffName(args.BuffName)) PoisonExpireTick[i] = 0;
    }
}
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) || !PoisonBuffName(args.BuffName)) return;
    ObservePoison(static_cast<int>(args.Sender.NetworkId),
        args.EndTime > Game::Time()
            ? static_cast<int>((args.EndTime - Game::Time()) * 1000.0f)
            : 0);
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack) LastAttackTargetId = static_cast<int>(args.TargetNetworkId);
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
        IncomingThreatEndpoint = analysis.Enemy.Position();
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) ObservePoison(static_cast<int>(args.Target.NetworkId()));
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
                           IncomingThreatUntil, 700.0f, 1100);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !MushroomObject(args) || !ControllerHelpers::ObjectEventIsAllied(args)) return;
    const int id = static_cast<int>(args.Sender.NetworkId != 0 ? args.Sender.NetworkId : args.Sender.Index);
    auto* mushroom = FindMushroom(id, true);
    if (!mushroom) return;
    mushroom->Position = args.Sender.Position;
    mushroom->CastTick = LastMushroomCastTick > 0 ? LastMushroomCastTick : Now();
    mushroom->ArmTick = mushroom->CastTick + 1000;
    mushroom->Alive = true;
    mushroom->Armed = false;
    mushroom->Vision = false;
    LastMushroomId = id;
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId != 0 ? args.Sender.NetworkId : args.Sender.Index);
    if (auto* mushroom = FindMushroom(id)) *mushroom = {};
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!ControllerHelpers::MissileEventIsLocal(args)) return;
    if (Engine::TextContains(args.SpellName, "TeemoR") || Engine::TextContains(args.MissileName, "Teemo_R_Mis"))
        LastMushroomCastTick = Now();
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!ControllerHelpers::MissileEventIsLocal(args)) return;
    if (Engine::TextContains(args.SpellName, "TeemoR") ||
        Engine::TextContains(args.MissileName, "Teemo_R_Mis")) {
        LastMushroomCastTick = 0;
    }
}

inline void OnDraw() {
    const int now = Now();
    for (auto& mushroom : Mushrooms) {
        if (mushroom.Alive && mushroom.Armed && mushroom.ArmTick + 120000 < now) mushroom.Alive = false;
    }
}

inline constexpr const char* Scenarios[] = {
    "Blinding Dart predicted targeted hit rejects collision and projectile walls",
    "Toxic Shot poison application, four-second refresh and lethal damage gate",
    "Move Quick chase speed versus incoming-threat retreat safety",
    "Noxious Trap ammo, one-second arming and vision-object reconciliation",
    "Mushroom bounce spacing, explosion radius and chokepoint placement",
    "Lane clear and last-hit mana floors without spending E as a cast",
    "Jungle poison pressure and objective mushroom setup",
    "Flee cursor separation, wall/turret and enemy-count rejection",
    "Turret dive denial and automatic anti-gapcloser mushroom response",
    "Manual cast and buff/object deletion polling reconciliation",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Teemo;
    controller.ControllerId = "champion.kuroaio.ai.teemo.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AITeemo.md";
    controller.ImplementationSummary =
        "Poison-aware dart and on-hit sequencing, Move Quick threat retreat, mushroom "
        "ammo/arming/vision tracking, safe bounce placement and mode-specific farm logic.";
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
    controller.OnBuffUpdate = &OnBuffUpdate;
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

} // namespace Plugins::KuroAIO::AI::Controllers::Teemo
