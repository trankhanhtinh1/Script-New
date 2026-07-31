#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "../Profiles/AIShaco.h"
#include "AIShacoGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace Plugins::KuroAIO::AI::Controllers::Shaco {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline bool Stealthed = false;
inline bool QEmpowered = false;
inline int StealthExpireTick = 0;
inline int LastQCastTick = 0;
inline int LastAttackTargetId = 0;
inline int IncomingThreatTargetId = 0;
inline int IncomingThreatUntil = 0;
inline Vector3 IncomingThreatEndpoint{};
inline int CloneNetworkId = 0;
inline Vector3 ClonePosition{};
inline int CloneSpawnTick = 0;
inline int CloneExpireTick = 0;
inline bool CloneConfirmed = false;
inline int CloneExplosionWindowUntil = 0;
inline std::array<int, 4> LastCastTick{};

struct BoxRecord {
    int NetworkId = 0;
    Vector3 Position{};
    int CastTick = 0;
    int ArmTick = 0;
    int ExpireTick = 0;
    bool Active = false;
    bool Triggered = false;
};
inline std::array<BoxRecord, 12> Boxes{};

inline bool NameHas(const char* value, const char* token) {
    return value && token && Engine::TextContains(value, token);
}

inline BoxRecord* FindBox(int networkId, bool create) {
    BoxRecord* empty = nullptr;
    for (auto& box : Boxes) {
        if (box.NetworkId == networkId && networkId != 0) return &box;
        if (!empty && box.NetworkId == 0) empty = &box;
    }
    if (!create || !empty || networkId == 0) return empty;
    empty->NetworkId = networkId;
    return empty;
}

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
           Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
           (reactive || LastCastTick[static_cast<std::size_t>(slot)] + 45 <= Now());
}

inline bool IsUnsafeTarget(const AIHeroClient& target) {
    return target.IsDashing() && !Engine::IsHardCrowdControlled(target);
}

inline bool QEndpointFor(const AIHeroClient& target, Vector3& endpoint,
                         bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kDeceiveRange + 80.0f) ||
        IsUnsafeTarget(target)) return false;
    const Vector3 predicted = PredictPosition(target, 0.08f);
    const Vector3 desired = BehindTargetEndpoint(player.Position(), predicted, 65.0f);
    endpoint = ClampBlinkEndpoint(player.Position(), desired);
    if (!BlinkEndpointSafe(player.Position(), endpoint, kDeceiveRange,
                           SDK::NavMesh::IsWall(endpoint),
                           Engine::UnderEnemyTurret(endpoint),
                           Engine::CountEnemiesAt(endpoint, 350.0f),
                           reactive ? 3 : Slider(TacticsMenu, "MaxQEnemies", 2))) {
        return false;
    }
    return !ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, 20.0f);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode, reactive) ||
        (!reactive && Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true))) return false;
    Vector3 endpoint{};
    if (Engine::ValidEnemy(target, kDeceiveRange + 80.0f)) {
        if (!QEndpointFor(target, endpoint, reactive)) return false;
    } else if (mode == Mode::Flee) {
        endpoint = ClampBlinkEndpoint(player.Position(), Game::CursorPos());
        if (!BlinkEndpointSafe(player.Position(), endpoint, kDeceiveRange,
                               SDK::NavMesh::IsWall(endpoint),
                               Engine::UnderEnemyTurret(endpoint),
                               Engine::CountEnemiesAt(endpoint, 350.0f), 0)) return false;
    } else {
        return false;
    }
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    LastCastTick[0] = LastQCastTick = Now();
    Stealthed = true;
    QEmpowered = true;
    StealthExpireTick = Now() + kStealthMs;
    return true;
}

inline Vector3 BoxAim(const AIHeroClient& target, Mode mode) {
    const auto player = GameObjects::Player();
    if (Engine::ValidEnemy(target, kBoxRange)) {
        const Vector3 predicted = PredictPosition(target, 0.25f);
        const Vec3 midpoint = player.Position() +
            (predicted - player.Position()) * 0.55f;
        return midpoint;
    }
    if (mode == Mode::Flee || mode == Mode::Jungle || mode == Mode::LaneClear) {
        return Game::CursorPos();
    }
    return {};
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) ||
        (!reactive && Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true))) return false;
    const Vector3 position = BoxAim(target, mode);
    if (!BoxPlacementValid(player.Position(), position, kBoxRange,
                           SDK::NavMesh::IsWall(position),
                           Engine::UnderEnemyTurret(position))) return false;
    if (!Engine::ControllerCastPosition(1, position)) return false;
    BoxRecord* record = FindBox(0, true);
    if (record) {
        *record = {};
        record->Position = position;
        record->CastTick = Now();
        record->ArmTick = Now() + kBoxArmMs;
        record->ExpireTick = Now() + kBoxLifetimeMs;
        record->Active = true;
    }
    LastCastTick[1] = Now();
    return true;
}

inline float ShivDamage(bool execute) {
    const auto player = GameObjects::Player();
    const float base = 55.0f + 0.55f * player.AP() + 0.50f * player.BonusAttackDamage();
    return base + (execute ? 35.0f + 0.25f * player.AP() : 0.0f);
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kShivRange) ||
        !Ready(2, mode, reactive) || IsUnsafeTarget(target) ||
        (!reactive && Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true))) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    if (!prediction.GetCastPosition().IsValid() ||
        prediction.GetCastPosition().IsZero() ||
        !prediction.CollisionObjects.empty()) return false;
    const Vector3 aim = prediction.GetCastPosition();
    if (SDK::NavMesh::IsWall(aim) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 70.0f)) return false;
    const bool execute = target.HealthPercent() <= 30.0f;
    const float damage = ShivDamage(execute);
    const bool lethal = ShivExecuteLethal(
        damage, 0.0f, target.Health(), target.AllShield(),
        target.HealthPercent(), 30.0f);
    if (!lethal && mode == Mode::Harass && target.HealthPercent() > 55.0f) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode, reactive) || CloneConfirmed) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kCloneExplosionRadius);
    const bool threatened = IncomingThreatUntil >= Now();
    const bool low = player.HealthPercent() <= Slider(TacticsMenu, "RHealth", 65);
    const bool explosionPayoff = Engine::ValidEnemy(target, 500.0f) &&
        CloneExplosionWorthwhile(player.Position(), target.Position(),
                                 target.BoundingRadius(), enemies, true);
    if (!reactive && !threatened && !low && !explosionPayoff) return false;
    if (Engine::UnderEnemyTurret(player.Position()) && !low) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    CloneSpawnTick = Now();
    CloneExpireTick = Now() + kCloneLifetimeMs;
    CloneExplosionWindowUntil = CloneExpireTick;
    CloneConfirmed = true;
    CloneNetworkId = 0;
    ClonePosition = player.Position();
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kShivRange + 60.0f)) return;
    if (CastE(target, Mode::Combo, false)) return;
    if (CastQ(target, Mode::Combo, false)) return;
    if (CastR(target, Mode::Combo, false)) return;
    (void)CastW(target, Mode::Combo, false);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "HarassMana", 42)) return;
    if (CastW(target, Mode::Harass, false)) return;
    if (CastQ(target, Mode::Harass, false)) return;
    (void)CastE(target, Mode::Harass, false);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "ClearMana", 35)) return;
    if (mode == Mode::Jungle && player.HealthPercent() < 68.0f) {
        (void)CastW({}, mode, true);
    }
    (void)Engine::TryFarm(mode);
}

inline void LastHit(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, kShivRange) && target.HealthPercent() <= 30.0f) {
        (void)CastE(target, Mode::LastHit, true);
    }
    (void)Engine::TryFarm(Mode::LastHit);
}

inline void Flee(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Engine::ValidEnemy(target, 425.0f) && IncomingThreatUntil >= Now()) {
        if (CastW(target, Mode::Flee, true)) return;
    }
    if (CastQ({}, Mode::Flee, true)) return;
    if (player.HealthPercent() < 35.0f) (void)CastR(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    if (IncomingThreatUntil >= Now() && Engine::ValidEnemy(target, 625.0f)) {
        if (CastQ(target, Mode::Automatic, true)) return;
        if (CastE(target, Mode::Automatic, true)) return;
    }
    if (Engine::ValidEnemy(target, kShivRange) && target.HealthPercent() <= 30.0f) {
        (void)CastE(target, Mode::Automatic, true);
    }
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    const bool liveStealth = player.IsValid() &&
        (player.HasBuff("Deceive") || player.HasBuff("DeceiveStealth") ||
         player.HasBuff("ShacoQ"));
    Stealthed = liveStealth || (Stealthed && now <= StealthExpireTick);
    if (Stealthed && now > StealthExpireTick) {
        Stealthed = false;
        QEmpowered = false;
    }
    if (CloneConfirmed && (now > CloneExpireTick ||
                           player.HasBuff("ShacoRDeath"))) {
        CloneConfirmed = false;
        CloneNetworkId = 0;
        ClonePosition = {};
    }
    for (auto& box : Boxes) {
        if (box.NetworkId != 0 && box.ExpireTick > 0 && now > box.ExpireTick) {
            box = {};
            continue;
        }
        if (box.NetworkId == 0 && box.Active && now > box.ExpireTick) {
            box = {};
            continue;
        }
        if (box.Active) box.Triggered = BoxCanFear(box.CastTick, now);
    }
    if (IncomingThreatUntil < now) {
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(
        selected, kShivRange + 70.0f);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle: Farm(mode); break;
    case Mode::LastHit: LastHit(target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Shaco tactics"));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 42, 0, 100));
    TacticsMenu->Add(new MenuSlider("ClearMana", "Clear mana percent", 35, 0, 100));
    TacticsMenu->Add(new MenuSlider("MaxQEnemies", "Maximum enemies at Deceive endpoint", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("RHealth", "Allow Hallucinate below health percent", 65, 1, 100));
}

inline void OnLoad() {
    TacticsMenu = nullptr;
    Stealthed = false;
    QEmpowered = false;
    StealthExpireTick = LastQCastTick = LastAttackTargetId = 0;
    IncomingThreatTargetId = IncomingThreatUntil = 0;
    IncomingThreatEndpoint = {};
    CloneNetworkId = CloneSpawnTick = CloneExpireTick = CloneExplosionWindowUntil = 0;
    ClonePosition = {};
    CloneConfirmed = false;
    LastCastTick.fill(0);
    Boxes.fill({});
}

inline void OnUnload() { OnLoad(); }
inline void OnDraw() {}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::WasControllerCast(static_cast<int>(args.Slot))) {
            if (args.Slot == static_cast<int>(SDK::SpellSlot::Q)) LastQCastTick = Now();
            if (args.Slot == static_cast<int>(SDK::SpellSlot::R)) CloneSpawnTick = Now();
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatEndpoint = {};
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick,
                                       analysis.LineThreatUntilTick);
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAttackTargetId = static_cast<int>(args.TargetNetworkId);
        if (QEmpowered) QEmpowered = false;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "deceive") ||
        Engine::TextContains(args.BuffName, "shacoq")) {
        Stealthed = true;
        StealthExpireTick = Now() + kStealthMs;
    }
    if (Engine::TextContains(args.BuffName, "shacor")) CloneConfirmed = true;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "deceive") ||
        Engine::TextContains(args.BuffName, "shacoq")) {
        Stealthed = false;
        QEmpowered = false;
    }
    if (Engine::TextContains(args.BuffName, "shacor")) CloneConfirmed = false;
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.EndTime <= Game::Time() &&
        (Engine::TextContains(args.BuffName, "deceive") ||
         Engine::TextContains(args.BuffName, "shacoq"))) {
        Stealthed = false;
        QEmpowered = false;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAttackTargetId = static_cast<int>(args.Target.NetworkId());
        if (QEmpowered) QEmpowered = false;
    }
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
                           IncomingThreatUntil, 650.0f, 1000);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const char* name = args.Sender.Name;
    const char* character = args.Sender.CharacterName;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (ControllerHelpers::ObjectEventIsAllied(args) &&
        (NameHas(name, "JackInTheBox") || NameHas(name, "ShacoBox") ||
         NameHas(character, "JackInTheBox"))) {
        BoxRecord* box = FindBox(id, true);
        if (box) {
            box->NetworkId = id;
            box->Position = args.Sender.Position;
            box->CastTick = Now();
            box->ArmTick = Now() + kBoxArmMs;
            box->ExpireTick = Now() + kBoxLifetimeMs;
            box->Active = true;
        }
    }
    if (ControllerHelpers::ObjectEventIsAllied(args) &&
        IsCloneIdentity(name, character)) {
        CloneNetworkId = id;
        ClonePosition = args.Sender.Position;
        CloneConfirmed = true;
        CloneSpawnTick = Now();
        CloneExpireTick = Now() + kCloneLifetimeMs;
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == CloneNetworkId) {
        CloneConfirmed = false;
        CloneNetworkId = 0;
        CloneExplosionWindowUntil = Now() + 300;
    }
    for (auto& box : Boxes) if (box.NetworkId == id) box = {};
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (ControllerHelpers::ObjectEventIsAllied(args) && NameHas(args.Sender.Name, "TwoShiv")) {
        LastCastTick[2] = Now();
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    (void)args;
}

inline constexpr const char* Scenarios[] = {
    "Deceive stealth blink endpoint, wall and turret safety",
    "Jack-in-the-Box arm timer, fear trigger and object reconciliation",
    "Two-Shiv prediction, collision and below-30-percent execute gate",
    "Hallucinate clone identity, lifetime and explosion payoff",
    "selected target policy and post-Deceive backstab windup protection",
    "lane clear, jungle low-health box setup and last-hit shiv",
    "flee Deceive cursor endpoint and unsafe mobility rejection",
    "automatic gapcloser/interrupt reaction and polling state expiry",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Shaco";
    controller.ControllerId = "champion.kuroaio.ai.shaco.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIShaco.md";
    controller.ImplementationSummary =
        "Shaco-specific stealth blink endpoint, armed box records, poison execute and clone identity/explosion controller.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Shaco
