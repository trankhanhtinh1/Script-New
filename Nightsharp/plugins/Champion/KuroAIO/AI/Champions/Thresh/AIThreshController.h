#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIThresh.h"
#include "AIThreshGeometry.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Thresh {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline int Souls = 0;
inline int QTargetId = 0;
inline int QRecastUntil = 0;
inline int LanternObjectId = 0;
inline int LanternCastTick = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline std::array<int, 4> LastCastTick{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    if (slot < 0 || slot >= 4 || !Engine::RuntimeSpells[slot] ||
        !Engine::RuntimeSpells[slot]->IsReady() || !SpellEnabled(slot, mode)) return false;
    return reactive || Now() - LastCastTick[static_cast<std::size_t>(slot)] >= 45;
}

inline bool ManaGate(int slot, Mode mode, bool reactive = false) {
    const float reserve = static_cast<float>(
        mode == Mode::Harass ? Slider(TacticsMenu, "HarassMana", 58) :
        (mode == Mode::LaneClear || mode == Mode::LastHit ? Slider(FarmMenu, "LaneMana", 30) :
         (mode == Mode::Jungle ? Slider(FarmMenu, "JungleMana", 25) : 0)));
    return reactive || ControllerHelpers::CurrentResource() >=
        SpellCost(slot) + reserve;
}

inline bool PreserveAttack(bool reactive) {
    return !reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline AIHeroClient ProtectedAlly() {
    return SelectProtectionAlly(1100.0f);
}

inline bool SafeCommit(const Vector3& endpoint, bool defensive, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid()) return false;
    const bool wall = SDK::NavMesh::IsWall(endpoint);
    const bool turret = Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position());
    const int enemies = Engine::CountEnemiesAt(endpoint, 600.0f);
    const int allies = Engine::CountAlliesAt(endpoint, 750.0f);
    return defensive || FlayCommitSafe(endpoint, wall, turret, enemies, allies,
                                       Slider(TacticsMenu, "MaxCommitEnemies", 2),
                                       defensive, lethal);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PreserveAttack(reactive) || !ManaGate(0, mode, reactive)) return false;
    const int now = Now();
    if (QRecastUntil >= now && QTargetId != 0) {
        const auto hooked = GameObjects::GetUnitByNetworkId<AIHeroClient>(
            static_cast<std::uint32_t>(QTargetId));
        if (!Engine::ValidEnemy(hooked)) return false;
        const bool lethal = Lethal(hooked, player.CalculateMagicDamage(hooked,
            EDamage(SpellRank(2), player.AP(), static_cast<float>(Souls))));
        if (!HookRecastSafe(player.Position(), hooked.Position(),
                SDK::NavMesh::IsWall(hooked.Position()),
                Engine::UnderEnemyTurret(hooked.Position()) &&
                    !Engine::UnderEnemyTurret(player.Position()),
                Engine::CountEnemiesAt(hooked.Position(), 600.0f),
                CountAlliedFollowup(hooked.Position(), 850.0f),
                Slider(TacticsMenu, "MaxCommitEnemies", 2),
                static_cast<int>(hooked.NetworkId()) == QTargetId, lethal) &&
            SafeCommit(hooked.Position(), fleeing || reactive, lethal) &&
            Engine::ControllerCastSelf(0)) {
            QRecastUntil = 0;
            LastCastTick[0] = now;
            return true;
        }
        return false;
    }
    if (!Ready(0, mode, reactive) || Protected(target)) return false;
    const Vec3 predicted = PredictPosition(target, kQCastSeconds);
    const HookPlan plan = BuildHookPlan(player.Position(), predicted,
                                        target.BoundingRadius());
    if (!plan.Valid || ProjectileWallBlocksFromPlayer(plan.Aim, kQHalfWidth)) return false;
    if (Engine::ControllerCastPosition(0, plan.Aim)) {
        QTargetId = static_cast<int>(target.NetworkId());
        QRecastUntil = now + static_cast<int>(kQRecastSeconds * 1000.0f);
        LastCastTick[0] = now;
        return true;
    }
    return false;
}

inline bool CastLantern(const AIHeroClient& ally, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidAlly(ally, kWRange + kWPickupRadius) ||
        ally.NetworkId() == player.NetworkId() || !Ready(1, mode, reactive) ||
        !ManaGate(1, mode, reactive) || PreserveAttack(reactive)) return false;
    const LanternPlan plan = BuildLanternPlan(player.Position(), ally.Position(),
                                               Souls, SpellRank(1), player.AP());
    if (!plan.Valid) return false;
    const bool threatened = ally.HealthPercent() <= Slider(TacticsMenu, "AllyRescueHealth", 58) ||
        Engine::CountEnemiesAt(ally.Position(), 550.0f) >
            Engine::CountAlliesAt(ally.Position(), 700.0f) + 1;
    if (!threatened && mode != Mode::Flee && mode != Mode::Automatic) return false;
    const bool safe = LanternRescueSafe(ally.Position(),
        Engine::CountEnemiesAt(ally.Position(), 600.0f),
        Engine::CountAlliesAt(ally.Position(), 750.0f),
        Engine::UnderEnemyTurret(ally.Position()) &&
            !Engine::UnderEnemyTurret(player.Position()), true,
        Slider(TacticsMenu, "MaxCommitEnemies", 2));
    if (!safe && !reactive) return false;
    if (Engine::ControllerCastPosition(1, ally.Position())) {
        LanternCastTick = Now();
        LastCastTick[1] = LanternCastTick;
        return true;
    }
    return false;
}

inline bool CastFlay(const AIHeroClient& target, Mode mode, bool reactive = false,
                     bool toward = true) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PreserveAttack(reactive) || !ManaGate(2, mode, reactive) ||
        !Ready(2, mode, reactive) || !Engine::ValidEnemy(target) || Protected(target)) return false;
    const Vec3 predicted = PredictPosition(target, 0.18f);
    const Vec3 direction = FlayDirection(player.Position(), predicted, toward);
    if (!FlayHits(player.Position(), direction, predicted, target.BoundingRadius())) return false;
    const Vec3 endpoint = FlayEndpoint(player.Position(), predicted, toward);
    const bool defensive = reactive || mode == Mode::Flee || player.HealthPercent() <= 38.0f;
    const bool lethal = Lethal(target, player.CalculateMagicDamage(target,
        EDamage(SpellRank(2), player.AP(), static_cast<float>(Souls))));
    if (!SafeCommit(endpoint, defensive, lethal)) return false;
    if (Engine::ControllerCastVector(2, player.Position(),
                                     player.Position() + direction * kERange)) {
        LastCastTick[2] = Now();
        return true;
    }
    return false;
}

inline bool CastBox(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode, reactive) ||
        !ManaGate(3, mode, reactive) || PreserveAttack(reactive)) return false;
    const AIHeroClient ally = ProtectedAlly();
    const bool defensive = reactive || mode == Mode::Flee ||
        player.HealthPercent() <= Slider(TacticsMenu, "BoxHealth", 42);
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const int allies = Engine::CountAlliesAt(player.Position(), 800.0f);
    if (enemies < (defensive ? 1 : 2) || (!defensive && allies <= 0)) return false;
    if (Engine::ValidEnemy(target) && target.IsInvulnerable()) return false;
    const bool safe = BoxSafe(player.Position(),
        Engine::ValidEnemy(target) ? target.Position() : Vec3{}, ally.Position(),
        false, enemies, allies, Engine::UnderEnemyTurret(player.Position()),
        defensive ? 0 : 1);
    if (!safe && !defensive) return false;
    if (Engine::ControllerCastSelf(3)) {
        LastCastTick[3] = Now();
        return true;
    }
    return false;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Combo) || CastFlay(target, Mode::Combo, false, true)) return;
    if (CastBox(target, Mode::Combo)) return;
    (void)CastLantern(ProtectedAlly(), Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Harass) || CastFlay(target, Mode::Harass, false, true)) return;
    (void)CastLantern(ProtectedAlly(), Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastFlay(target, mode, false, true)) return;
    if (mode == Mode::Jungle && Engine::ValidEnemy(target) && CastQ(target, mode)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastFlay(target, Mode::Flee, true, false)) return;
    if (CastLantern(ProtectedAlly(), Mode::Flee, true)) return;
    (void)CastQ(target, Mode::Flee, true, true);
}

inline void Automatic(const AIHeroClient& target) {
    const AIHeroClient ally = ProtectedAlly();
    if (Engine::ValidAlly(ally) &&
        (ally.HealthPercent() <= Slider(TacticsMenu, "AllyRescueHealth", 58) ||
         IncomingThreatUntil >= Now()) && CastLantern(ally, Mode::Automatic, true)) return;
    if (IncomingThreatUntil >= Now() && Engine::ValidEnemy(target) &&
        CastFlay(target, Mode::Automatic, true, false)) return;
    if (Engine::ValidEnemy(target) && target.HealthPercent() <= 28.0f)
        (void)CastQ(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const int now = Now();
    if (QRecastUntil < now) {
        QRecastUntil = 0;
        QTargetId = 0;
    }
    if (LanternObjectId != 0 && now > LanternCastTick + 7000) LanternObjectId = 0;
    if (IncomingThreatUntil < now) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = PreferredEnemyTarget(selected, kQRange + 100.0f);
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
    TacticsMenu = root->AddSubMenu(new Menu("Thresh hook and lantern tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Thresh farming posture"));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at hook or flay endpoint", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("AllyRescueHealth", "Ally health threshold for lantern rescue", 58, 1, 100));
    TacticsMenu->Add(new MenuSlider("BoxHealth", "Health threshold for defensive Box", 42, 1, 100));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Minimum harass mana percent", 58, 0, 100));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast protection (ms)", 650, 0, 2000));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve attack windup", true));
    FarmMenu->Add(new MenuSlider("LaneMana", "Minimum lane-clear mana percent", 30, 0, 100));
    FarmMenu->Add(new MenuSlider("JungleMana", "Minimum jungle mana percent", 25, 0, 100));
}

inline void OnLoad() {
    Souls = QTargetId = QRecastUntil = LanternObjectId = LanternCastTick = 0;
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
            if (args.Slot == 0) {
                if (QTargetId != 0 && QRecastUntil >= now) QRecastUntil = now + 450;
                else QRecastUntil = now + static_cast<int>(kQRecastSeconds * 1000.0f);
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
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "threshq")) {
            QRecastUntil = Now() + static_cast<int>(kQRecastSeconds * 1000.0f);
        } else if (Engine::TextContains(args.BuffName, "threshpassive") &&
                   args.Count >= 0) {
            Souls = std::max(Souls, args.Count);
        }
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "threshq")) {
        QRecastUntil = 0;
        QTargetId = 0;
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime <= Game::Time()) OnBuffRemove(args);
    else OnBuffAdd(args);
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
                           IncomingThreatUntil, kERange, 1000);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"threshsoul", "soul"})) {
        ++Souls;
        return;
    }
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"threshlantern", "lantern"})) {
        LanternObjectId = args.Sender.NetworkId != 0
            ? static_cast<int>(args.Sender.NetworkId) : 0;
        LanternCastTick = Now();
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.Sender.NetworkId != 0 ? static_cast<int>(args.Sender.NetworkId) : 0;
    if (id == LanternObjectId) LanternObjectId = 0;
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"threshq", "deathsentence"})) {
        QRecastUntil = std::max(QRecastUntil,
                                Now() + static_cast<int>(kQRecastSeconds * 1000.0f));
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "souls accumulate armor and scale Dark Passage shield strength",
    "Death Sentence prediction, collision, projectile wall and Q recast dash",
    "Q recast commits only with selected target and allied followup safety",
    "Dark Passage lantern placement rescues a threatened selected ally",
    "Flay uses explicit toward and away directions for engage and peel",
    "The Box five-wall cage protects selected and allied carries from unsafe trapping",
    "turret, enemy-count, wall, spell-shield and lethal mobility gates",
    "AA windup preservation and manual ownership protection",
    "event and polling reconciliation for hook, lantern, souls and threats",
    "distinct combo, harass, lane clear, jungle, last-hit, flee and automatic policies",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Thresh;
    controller.ControllerId = "champion.kuroaio.ai.thresh.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIThresh.md";
    controller.ImplementationSummary =
        "Owns soul armor and shield state, hook recast, lantern rescue, directional Flay, "
        "Box wall safety and all combat/farming decisions without shared champion logic.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Thresh
