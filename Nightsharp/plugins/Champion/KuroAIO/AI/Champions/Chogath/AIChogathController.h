#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIChogathGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Chogath {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::MaximumBuffCount;
using ControllerHelpers::Now;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Protected;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;

inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int FeastStacks = 0;
inline int LastRTargetId = 0;
inline bool LastRWasEpic = false;
inline bool VorpalEnabled = false;
inline bool VorpalOwned = false;
inline int VorpalCastTick = 0;
inline Mode LastMode = Mode::None;

inline bool Ready(int slot, Mode mode = Mode::None) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() &&
        (mode == Mode::None || SpellEnabled(slot, mode));
}
inline bool Throttle(int slot, int delay = 100) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target) || Protected(target);
}
inline float AbilityPower() {
    const auto player = GameObjects::Player();
    return player.IsValid() ? player.AP() : 0.0f;
}
inline float RChampionDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    return RChampionRawDamage(SpellRank(3), AbilityPower(), 0.0f);
}
inline bool RChampionLethal(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
        RChampionDamage(target) >= target.Health() + target.AllShield();
}
inline bool RMonsterLethal(const AIMinionClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid() && !target.IsDead() &&
        RMonsterRawDamage(AbilityPower(), 0.0f) >= target.Health();
}
inline bool SafeEndpoint(const Vector3& endpoint, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    if (!lethal && Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position())) return false;
    return Engine::CountEnemiesAt(endpoint, 450.0f) <=
        Slider(RMenu, "MaxEndpointEnemies", 2);
}
inline int LiveFeastStacks() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return FeastStacks;
    const int observed = MaximumBuffCount(player,
        {"Feast", "ChogathFeast", "ChoGathFeast", "FeastStacks"});
    return observed > 0 ? std::clamp(observed, 0, 255) : FeastStacks;
}
inline void ReconcileState() {
    const int now = Now();
    FeastStacks = LiveFeastStacks();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool liveVorpal = player.HasBuff("VorpalSpikes") ||
        player.HasBuff("ChogathE") || player.HasBuff("ChogathEVorpalSpikes");
    if (liveVorpal) VorpalEnabled = true;
    if (!liveVorpal && VorpalEnabled && now - VorpalCastTick > 240)
        VorpalEnabled = VorpalOwned = false;
    if (InterruptExpireTick <= now) InterruptTargetId = 0;
    if (IncomingThreatUntil <= now) IncomingThreatUntil = 0;
    if (IncomingHardCCUntil <= now) IncomingHardCCUntil = 0;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !Ready(0, mode) ||
        !Throttle(0) || PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 predicted = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, kQDelay);
    const Vector3 aim = RuptureAim(player.Position(), predicted);
    if (aim.IsZero() || !RuptureHits(aim, predicted, target.BoundingRadius()) ||
        SDK::NavMesh::IsWallBetween(player.Position(), aim, kQRuptureRadius * 0.35f) ||
        (prediction.Hitchance < SDK::HitChance::High && !reactive)) return false;
    if (!reactive && Engine::UnderEnemyTurret(aim) &&
        !Engine::UnderEnemyTurret(player.Position())) return false;
    LastCastTick[0] = Now();
    return Engine::ControllerCastPosition(0, aim);
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !Ready(1, mode) ||
        !Throttle(1) || PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, 0.25f);
    const ConeHit cone = FeralScreamCone(player.Position(), aim, aim,
        target.BoundingRadius());
    if (!cone.Hits || player.Position().Distance2D(aim) > kWRange +
        target.BoundingRadius() || SDK::NavMesh::IsWallBetween(
            player.Position(), aim, 20.0f) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 20.0f)) return false;
    if (!reactive && Engine::UnderEnemyTurret(aim) &&
        !Engine::UnderEnemyTurret(player.Position())) return false;
    LastCastTick[1] = Now();
    return Engine::ControllerCastPosition(1, aim);
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 220.0f) ||
        !Ready(2, mode) || VorpalEnabled || !Throttle(2) ||
        PreserveAttack(reactive)) return false;
    if (!VorpalReach(player.Position(), PredictPosition(target, 0.15f),
        FeastStacks, true, target.BoundingRadius(), SpellRank(3))) return false;
    LastCastTick[2] = Now();
    VorpalOwned = true;
    VorpalCastTick = LastCastTick[2];
    if (!Engine::ControllerCastSelf(2)) {
        VorpalOwned = false;
        return false;
    }
    VorpalEnabled = true;
    return true;
}
inline bool CastRChampion(const AIHeroClient& target, Mode mode,
                          bool reactive = false) {
    const auto player = GameObjects::Player();
    const float range = FeastCastRange(FeastStacks);
    if (!player.IsValid() || TargetBlocked(target) || !Ready(3, mode) ||
        !Throttle(3, 160) || PreserveAttack(reactive) ||
        !RChampionLethal(target) || player.Position().Distance2D(target.Position()) >
            range + target.BoundingRadius()) return false;
    const bool turretOnly = Engine::UnderEnemyTurret(target.Position()) &&
        !Engine::UnderEnemyTurret(player.Position());
    if (turretOnly && !reactive) return false;
    if (!SafeEndpoint(target.Position(), true)) return false;
    LastCastTick[3] = Now();
    LastRTargetId = static_cast<int>(target.NetworkId());
    LastRWasEpic = false;
    return Engine::ControllerCastUnit(3, target);
}
inline bool CastRMonster(const AIMinionClient& monster, Mode mode,
                         bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !monster.IsValid() || monster.IsDead() ||
        !monster.IsTargetable() || !Ready(3, mode) || !Throttle(3, 160) ||
        PreserveAttack(reactive) || !RMonsterLethal(monster) ||
        player.Position().Distance2D(monster.Position()) > FeastCastRange(FeastStacks) +
            monster.BoundingRadius()) return false;
    if (IsEpicMonster(monster) && Engine::CountEnemiesAt(monster.Position(), 500.0f) >
        Slider(RMenu, "ObjectiveMaxEnemies", 3) && !reactive) return false;
    LastCastTick[3] = Now();
    LastRTargetId = static_cast<int>(monster.NetworkId());
    LastRWasEpic = IsEpicMonster(monster);
    return Engine::ControllerCastUnit(3, AIBaseClient(monster.Handle()));
}
inline bool FeastObjective(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    AIMinionClient best{};
    int bestPriority = -1;
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead() || !monster.IsTargetable() ||
            player.Position().Distance2D(monster.Position()) > FeastCastRange(FeastStacks) +
                monster.BoundingRadius() || !RMonsterLethal(monster)) continue;
        const int priority = IsEpicMonster(monster) ? 100 : 40;
        if (priority > bestPriority) { best = monster; bestPriority = priority; }
    }
    return best.IsValid() && CastRMonster(best, mode, reactive);
}
inline bool Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 35)) return false;
    if (mode == Mode::Jungle && FeastObjective(mode)) return true;
    // Shared farm policy is used only after the controller has given Feast
    // objective/stack priority and still owns the mode decision.
    return Engine::TryFarm(mode);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (!VorpalEnabled && CastE(target, Mode::Combo)) return;
    (void)CastRChampion(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 54)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target)) {
        if (CastW(target, Mode::Flee, true)) return;
        (void)CastQ(target, Mode::Flee, true);
    }
}
inline bool Automatic(const AIHeroClient& target) {
    if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(target))
        if (CastW(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && CastRChampion(target, Mode::Automatic, true)) return true;
    return FeastObjective(Mode::Automatic, true);
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const auto target = ControllerHelpers::PreferredEnemyTarget(selected,
        mode == Mode::Flee ? 1000.0f : std::max(kQRange, FeastCastRange(FeastStacks)));
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)Farm(mode); break;
    case Mode::Automatic: (void)Automatic(target); break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        LastCastTick[slot] = now;
        if (slot == 2) {
            VorpalEnabled = true;
            VorpalOwned = Engine::WasControllerCast(2);
            VorpalCastTick = now;
        }
        if (slot == 3) LastRTargetId = args.TargetNetworkId != 0
            ? static_cast<int>(args.TargetNetworkId) : LastRTargetId;
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
        IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick,
                                      analysis.LineThreatUntilTick));
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    (void)CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender) &&
        (Engine::TextContains(args.BuffName, "Feast") ||
         Engine::TextContains(args.BuffName, "ChogathFeast")))
        FeastStacks = std::clamp(args.Count, 0, 255);
    if (IsLocalPlayer(args.Sender) &&
        (Engine::TextContains(args.BuffName, "Vorpal") ||
         Engine::TextContains(args.BuffName, "ChogathE"))) VorpalEnabled = true;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "Vorpal") ||
        Engine::TextContains(args.BuffName, "ChogathE")) VorpalEnabled = false;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;    // Feast is winding-up protected by Riot data; never cancel a controller cast.
    // or controller Feast windup to invent an extra Vorpal attack.
    if (LastCastTick[3] > 0 && Now() - LastCastTick[3] < 300) args.Process = false;
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptExpireTick, 900, 250, 5000);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFB35A4Bu, 1.3f, 40);
    Drawing::DrawCircle(player.Position(), FeastCastRange(FeastStacks), 0xFF8A6AFFu, 1.4f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("ChogathOneTrick", "Cho'Gath Feast tactics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Rupture", "Delayed rupture prediction"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("FeralScream", "Silence cone"));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 54, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("VorpalSpikes", "Attack reset toggle"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("Feast", "Champion and objective execute"));
    RMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at Feast target", 2, 1, 5));
    RMenu->Add(new MenuSlider("ObjectiveMaxEnemies", "Maximum enemies around objective", 3, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Stack-aware farm policy"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum farm mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and Feast ranges", false));
}
inline void OnLoad() {
    LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = IncomingThreatUntil = 0;
    IncomingHardCCUntil = InterruptTargetId = InterruptExpireTick = 0;
    FeastStacks = LastRTargetId = 0;
    LastRWasEpic = VorpalEnabled = VorpalOwned = false;
    VorpalCastTick = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Predict delayed Q Rupture at 950 range and reject wall-only or low-confidence casts",
    "Use target movement prediction plus 230 radius and body-radius boundary checks",
    "Aim W Feral Scream as a 675-range 28-degree cone and preserve silence for reactive peel",
    "Reject W through terrain and unsafe turret-only endpoints",
    "Toggle E Vorpal Spikes only for a reachable empowered attack and retain attack-reset ownership",
    "Reconcile E active state from spell events, buff events and polling without toggling E",
    "Track Feast stacks from Feast buff counters, process-spell events and polling reconciliation",
    "Use true-damage Feast only at verified champion execute threshold",
    "Prioritize lethal epic jungle monsters/objectives over ordinary monster stacks",
    "Allow minion Feast stacks only below six while preserving champion and epic-monster stacks",
    "Respect Feast cast-range growth of 2.5 per stack capped at 25 and real target reach",
    "Reject turret-only or overcommitted Feast targets unless the execute is verified lethal",
    "Protect Feast and ordinary AA windups from controller casts and stale cast windows/",
    "Keep selected target precedence through PreferredEnemyTarget before selector fallback",
    "Run distinct Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic policies",
    "Use Automatic only for reactive silence, lethal champion Feast or lethal objective Feast",
    "Capture incoming enemy casts and interruptible windows for W defensive decisions",
    "Keep projectile-wall, terrain, prediction, collision, damage and safety gates explicit",
    "Draw Q and stack-scaled Feast ranges only when coaching is enabled",
    "Keep Cho'Gath profile/controller metadata aligned to Riot 26.15 and CommunityDragon 16.15",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Chogath;
    controller.ControllerId = "champion.kuroaio.ai.chogath.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIChogath.md";
    controller.ImplementationSummary =
        "Owned delayed Rupture prediction, silence cone, Vorpal toggle and"
        " champion/objective Feast execute with stack reconciliation.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Chogath
