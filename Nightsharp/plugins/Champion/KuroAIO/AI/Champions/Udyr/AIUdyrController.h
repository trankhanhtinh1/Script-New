#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIUdyr.h"
#include "AIUdyrGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Udyr {

using namespace Geometry;
using ControllerHelpers::Lethal;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Stance CurrentStance = Stance::None;
inline RecastState CurrentRecastState = RecastState::Ready;
inline int StanceCastTick = 0;
inline int StanceTargetId = 0;
inline int StormTargetId = 0;
inline int StormObjectId = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline std::array<int, 4> LastCastTick{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        CooldownAvailable(Engine::RuntimeSpells[slot]->IsReady(),
                          Now() - LastCastTick[static_cast<std::size_t>(slot)], 45) &&
        SpellEnabled(slot, mode) && (reactive || LastCastTick[static_cast<std::size_t>(slot)] + 45 <= Now());
}

inline bool PreserveAttack(bool reactive) {
    return !reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline int SlotFor(Stance stance) {
    switch (stance) {
    case Stance::WildingClaw: return 0;
    case Stance::IronMantle: return 1;
    case Stance::BlazingStampede: return 2;
    case Stance::WingborneStorm: return 3;
    default: return -1;
    }
}

inline bool ManaGate(int slot, Mode mode, bool urgent) {
    const float floor = static_cast<float>(
        mode == Mode::Harass ? Slider(TacticsMenu, "HarassMana", 45) :
        (mode == Mode::Jungle ? Slider(FarmMenu, "JungleMana", 25) :
         (mode == Mode::LaneClear || mode == Mode::LastHit ? Slider(FarmMenu, "LaneMana", 30) : 0)));
    return urgent || PlayerManaPercent() >= floor;
}

inline float StanceDamage(const AIHeroClient& target, Stance stance) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    if (stance == Stance::WildingClaw)
        return player.CalculatePhysicalDamage(target, QMaxHealthDamage(
            SpellRank(0), target.MaxHealth(), player.BonusAttackDamage()));
    if (stance == Stance::WingborneStorm)
        return player.CalculateMagicDamage(target, StormPulseDamage(
            SpellRank(3), player.AP(), player.TotalAttackDamage()));
    return 0.0f;
}

inline bool CommitSafe(const Vector3& endpoint, bool defensive, bool fleeing, bool lethal) {
    const int nearby = Engine::CountEnemiesAt(endpoint, 300.0f);
    return MobilitySafe(SDK::NavMesh::IsWall(endpoint),
                        Engine::UnderEnemyTurret(endpoint) &&
                            !Engine::UnderEnemyTurret(GameObjects::Player().Position()),
                        nearby, Slider(TacticsMenu, "MaxCommitEnemies", 2),
                        defensive, fleeing) || lethal;
}
inline bool CastStance(Stance stance, const AIHeroClient& target, Mode mode,
                       bool reactive = false, bool fleeing = false) {
    const auto player = GameObjects::Player();
    const int slot = SlotFor(stance);
    if (!player.IsValid() || slot < 0 || PreserveAttack(reactive) || !ManaGate(slot, mode, reactive)) return false;
    const int now = Now();
    const int elapsed = now - StanceCastTick;
    const bool recast = CurrentStance == stance &&
        InRecastWindow(CurrentRecastState, CurrentStance, elapsed) && RecastSafeTail(elapsed);
    if (recast) {
        if (stance == Stance::IronMantle && player.HealthPercent() > 68.0f && !reactive) return false;
        if (stance == Stance::WingborneStorm && Engine::ValidEnemy(target) &&
            !StormCovers(player.Position(), PredictPosition(target, 0.15f), target.BoundingRadius())) return false;
        if (!Engine::ControllerCastSelf(slot)) return false;
        CurrentRecastState = RecastState::RecastPending;
        LastCastTick[static_cast<std::size_t>(slot)] = now;
        return true;
    }
    if (CurrentRecastState == RecastState::RecastPending || CurrentStance == stance ||
        !Ready(slot, mode, reactive)) return false;
    if (stance == Stance::WildingClaw) {
        if (Protected(target) ||
            player.Position().Distance2D(target.Position()) >
                kStanceReach + target.BoundingRadius() ||
            !LightningReachable(player.Position(), PredictPosition(target, 0.10f),
                                target.BoundingRadius())) return false;
    } else if (stance == Stance::BlazingStampede) {
        if (Engine::ValidEnemy(target) &&
            player.Position().Distance2D(target.Position()) >
                kStanceReach + target.BoundingRadius()) return false;
        const Vector3 endpoint = Engine::ValidEnemy(target) ? PredictPosition(target, 0.10f) :
            player.Position() + Vector3{1.0f, 0.0f, 0.0f};
        const bool lethal = Engine::ValidEnemy(target) && Lethal(target, StanceDamage(target, stance));
        if (!CommitSafe(endpoint, reactive || player.HealthPercent() <= 38.0f, fleeing, lethal)) return false;
    } else if (stance == Stance::WingborneStorm) {
        if (Protected(target) ||
            !StormTargetValid(player.Position(), PredictPosition(target, 0.15f),
                              target.BoundingRadius(), false)) return false;
    } else if (stance == Stance::IronMantle) {
        const bool threatened = IncomingThreatUntil >= now;
        if (!reactive && !MantleRecastValuable(player.HealthPercent(), threatened)) return false;
    }
    if (!Engine::ControllerCastSelf(slot)) return false;
    CurrentStance = stance;
    CurrentRecastState = RecastState::RecastWindow;
    StanceCastTick = now;
    StanceTargetId = Engine::ValidEnemy(target) ? static_cast<int>(target.NetworkId()) : 0;
    if (stance == Stance::WingborneStorm) StormTargetId = StanceTargetId;
    LastCastTick[static_cast<std::size_t>(slot)] = now;
    return true;
}

inline bool CastAwakened(Stance stance, const AIHeroClient& target, Mode mode,
                         bool reactive = false, bool fleeing = false) {
    return CurrentStance == stance &&
        InRecastWindow(CurrentRecastState, stance, Now() - StanceCastTick) &&
        CastStance(stance, target, mode, reactive, fleeing);
}

inline bool CastObjectiveStance(Stance stance, Mode mode, const AIMinionClient& monster) {
    const auto player = GameObjects::Player();
    const int slot = SlotFor(stance);
    if (!player.IsValid() || !monster.IsValid() || slot < 0 ||
        !ManaGate(slot, mode, false) || PreserveAttack(false)) return false;
    const int now = Now();
    const int elapsed = now - StanceCastTick;
    if (CurrentStance == stance &&
        InRecastWindow(CurrentRecastState, stance, elapsed) && RecastSafeTail(elapsed)) {
        if (!Engine::ControllerCastSelf(slot)) return false;
        CurrentRecastState = RecastState::RecastPending;
        LastCastTick[static_cast<std::size_t>(slot)] = now;
        StormTargetId = static_cast<int>(monster.NetworkId());
        return true;
    }
    if (CurrentRecastState == RecastState::RecastPending || CurrentStance == stance ||
        !Ready(slot, mode)) return false;
    if (player.Position().Distance2D(monster.Position()) > kStanceReach + monster.BoundingRadius()) return false;
    const bool defensive = player.HealthPercent() <= 42.0f;
    if (!CommitSafe(player.Position(), defensive, false, false)) return false;
    if (stance == Stance::WingborneStorm &&
        !StormCovers(player.Position(), monster.Position(), monster.BoundingRadius())) return false;
    if (!Engine::ControllerCastSelf(slot)) return false;
    CurrentStance = stance;
    CurrentRecastState = RecastState::RecastWindow;
    StanceCastTick = now;
    StanceTargetId = static_cast<int>(monster.NetworkId());
    StormTargetId = StanceTargetId;
    LastCastTick[static_cast<std::size_t>(slot)] = now;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return;
    const bool low = player.HealthPercent() <= 38.0f;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 500.0f);
    if (low && (CastAwakened(Stance::IronMantle, target, Mode::Combo) ||
                CastStance(Stance::IronMantle, target, Mode::Combo))) return;
    if (enemies >= 2 && (CastAwakened(Stance::WingborneStorm, target, Mode::Combo) ||
                         CastStance(Stance::WingborneStorm, target, Mode::Combo))) return;
    if (player.Position().Distance2D(target.Position()) > kAttackReach + target.BoundingRadius() &&
        (CastAwakened(Stance::BlazingStampede, target, Mode::Combo) ||
         CastStance(Stance::BlazingStampede, target, Mode::Combo))) return;
    if (target.HealthPercent() <= 42.0f &&
        (CastAwakened(Stance::WildingClaw, target, Mode::Combo) ||
         CastStance(Stance::WildingClaw, target, Mode::Combo))) return;
    if (CastAwakened(Stance::WingborneStorm, target, Mode::Combo) ||
        CastStance(Stance::WingborneStorm, target, Mode::Combo)) return;
    (void)CastAwakened(Stance::IronMantle, target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || PlayerManaPercent() < Slider(TacticsMenu, "HarassMana", 45)) return;
    if (Engine::CountEnemiesAt(target.Position(), 450.0f) >= 2 &&
        (CastAwakened(Stance::WingborneStorm, target, Mode::Harass) ||
         CastStance(Stance::WingborneStorm, target, Mode::Harass))) return;
    if (CastAwakened(Stance::WildingClaw, target, Mode::Harass) ||
        CastStance(Stance::WildingClaw, target, Mode::Harass)) return;
    (void)CastAwakened(Stance::IronMantle, target, Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ManaGate(0, mode, mode == Mode::LastHit)) return;
    const bool jungle = mode == Mode::Jungle;
    if (jungle) {
        const auto objective = SelectJungleTarget(kStanceReach, 0.15f, 100000.0f);
        if (objective.IsValid() &&
            (CastObjectiveStance(Stance::WingborneStorm, mode, objective) ||
             CastObjectiveStance(Stance::WildingClaw, mode, objective) ||
             CastObjectiveStance(Stance::IronMantle, mode, objective))) return;
    }
    if (jungle && Engine::ValidEnemy(target) &&
        (CastAwakened(Stance::WingborneStorm, target, mode) ||
         CastStance(Stance::WingborneStorm, target, mode))) return;
    if (Engine::ValidEnemy(target) &&
        (CastAwakened(Stance::WildingClaw, target, mode) ||
         CastStance(Stance::WildingClaw, target, mode))) return;
    if (jungle && Engine::ValidEnemy(target) &&
        (CastAwakened(Stance::IronMantle, target, mode) ||
         CastStance(Stance::IronMantle, target, mode))) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (CastAwakened(Stance::BlazingStampede, target, Mode::Flee, true, true) ||
        CastStance(Stance::BlazingStampede, target, Mode::Flee, true, true)) return;
    if (GameObjects::Player().HealthPercent() <= 50.0f &&
        (CastAwakened(Stance::IronMantle, target, Mode::Flee, true, true) ||
         CastStance(Stance::IronMantle, target, Mode::Flee, true, true))) return;
    (void)CastAwakened(Stance::WingborneStorm, target, Mode::Flee, true, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool threatened = IncomingThreatUntil >= Now();
    if (threatened && (CastAwakened(Stance::BlazingStampede, target, Mode::Automatic, true) ||
                       CastStance(Stance::BlazingStampede, target, Mode::Automatic, true))) return;
    if (player.HealthPercent() <= 35.0f &&
        (CastAwakened(Stance::IronMantle, target, Mode::Automatic, true) ||
         CastStance(Stance::IronMantle, target, Mode::Automatic, true))) return;
    if (Engine::ValidEnemy(target) && target.HealthPercent() <= 32.0f)
        (void)CastAwakened(Stance::WildingClaw, target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const int now = Now();
    if (CurrentRecastState == RecastState::RecastWindow && now > StanceCastTick + kRecastWindowMs) {
        CurrentRecastState = RecastState::Ready;
        CurrentStance = Stance::None;
        StanceTargetId = 0;
    } else if (CurrentRecastState == RecastState::RecastPending && now > StanceCastTick + 700) {
        CurrentRecastState = RecastState::Ready;
        CurrentStance = Stance::None;
    }
    if (IncomingThreatUntil < now) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
    if (StormObjectId != 0 && now > StanceCastTick + kStormDurationMs) StormObjectId = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const float range = mode == Mode::Flee ? 850.0f : kLightningReach + 100.0f;
    const AIHeroClient target = PreferredEnemyTarget(selected, range);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode, target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Udyr stance tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Udyr jungle posture"));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at stance commit", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Minimum harass mana percent", 45, 0, 100));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast protection (ms)", 650, 0, 2000));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve attack windup", true));
    FarmMenu->Add(new MenuSlider("LaneMana", "Minimum lane-clear mana percent", 30, 0, 100));
    FarmMenu->Add(new MenuSlider("JungleMana", "Minimum jungle mana percent", 25, 0, 100));
}

inline void OnLoad() {
    CurrentStance = Stance::None;
    CurrentRecastState = RecastState::Ready;
    StanceCastTick = StanceTargetId = StormTargetId = StormObjectId = 0;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingThreatTargetId = 0;
    IncomingThreatEndpoint = {};
    LastAutoTargetId = LastAutoTick = 0;
    LastCastTick.fill(0);
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    FarmMenu = nullptr;
    OnLoad();
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (args.Slot >= 0 && args.Slot < 4) {
            if (!Engine::WasControllerCast(args.Slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 650);
            LastCastTick[static_cast<std::size_t>(args.Slot)] = now;
            const Stance stance = args.Slot == 0 ? Stance::WildingClaw :
                (args.Slot == 1 ? Stance::IronMantle :
                 (args.Slot == 2 ? Stance::BlazingStampede : Stance::WingborneStorm));
            if (CurrentStance == stance && InRecastWindow(CurrentRecastState, stance, now - StanceCastTick))
                CurrentRecastState = RecastState::RecastPending;
            else {
                CurrentStance = stance;
                CurrentRecastState = RecastState::RecastWindow;
                StanceCastTick = now;
            }
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatTargetId = static_cast<int>(args.Sender.NetworkId);
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
        IncomingThreatEndpoint = args.EndPosition;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
        LastAutoTick = Now();
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const Stance previous = CurrentStance;
    if (Engine::TextContains(args.BuffName, "udyrq")) CurrentStance = Stance::WildingClaw;
    else if (Engine::TextContains(args.BuffName, "udyrw")) CurrentStance = Stance::IronMantle;
    else if (Engine::TextContains(args.BuffName, "udyre")) CurrentStance = Stance::BlazingStampede;
    else if (Engine::TextContains(args.BuffName, "udyrr")) {
        CurrentStance = Stance::WingborneStorm;
        StormObjectId = static_cast<int>(args.Sender.NetworkId);
    }
    if (CurrentStance != Stance::None) {
        const bool recastPending = CurrentRecastState == RecastState::RecastPending;
        if (previous != CurrentStance || CurrentRecastState == RecastState::Ready)
            StanceCastTick = Now();
        if (!recastPending) CurrentRecastState = RecastState::RecastWindow;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "udyr")) {
        CurrentRecastState = RecastState::Ready;
        CurrentStance = Stance::None;
        StanceTargetId = 0;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
        LastAutoTick = Now();
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
                           IncomingThreatUntil, kStanceReach, 1100);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"udyrr", "phoenix", "storm"})) {
        StormObjectId = args.Sender.NetworkId != 0 ? static_cast<int>(args.Sender.NetworkId) : 0;
        StormTargetId = StanceTargetId;
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.Sender.NetworkId != 0 ? static_cast<int>(args.Sender.NetworkId) : 0;
    if (id == StormObjectId) StormObjectId = 0;
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"udyrq", "udyrr", "udyrstorm"}))
        StormTargetId = StanceTargetId;
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Wilding Claw attack window and Awakened lightning against isolated targets",
    "Iron Mantle shield, heal and low-health defensive recast",
    "Blazing Stampede stun, movement and 1.5-second immunity posture",
    "Wingborne Storm slow, pulses, zone safety and moving target tracking",
    "stance recast state reconciled by spell, buff, object and polling callbacks",
    "prediction, collision and projectile-wall checks before stance commitment",
    "attack windup preservation and manual ownership protection",
    "combo and harass stance-aware priorities with selected/orbwalker target policy",
    "lane clear, jungle objective, last-hit, flee and automatic threat policies",
    "mana, cooldown, turret and enemy-count gates around safe mobility and zones",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Udyr;
    controller.ControllerId = "champion.kuroaio.ai.udyr.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIUdyr.md";
    controller.ImplementationSummary =
        "Owns four stance transitions, Awakened recasts, storm target tracking, defensive posture, "
        "and safe combat/farming decisions without shared champion logic.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Udyr
