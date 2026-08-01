#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIVelkozGeometry.h"
#include "../../Profiles/AIVelkoz.h"
#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Velkoz {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContains;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;

inline std::array<int, 32> OrganicIds{};
inline std::array<DeconstructionState, 32> OrganicStates{};
inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOwnershipUntil = 0;
inline int PendingWTargetId = 0;
inline int PendingWStartTick = 0;
inline WStage PendingWStage = WStage::None;
inline int RTargetId = 0;
inline int RStartTick = 0;
inline bool RActive = false;
inline bool RControllerOwned = false;
inline int InterruptTargetId = 0;
inline int InterruptUntil = 0;
inline Vector3 InterruptEndpoint{};
inline int GapcloserTargetId = 0;
inline int GapcloserUntil = 0;
inline Vector3 GapcloserEndpoint{};

inline int IndexFor(int networkId) {
    if (networkId == 0) return -1;
    for (std::size_t i = 0; i < OrganicIds.size(); ++i)
        if (OrganicIds[i] == networkId) return static_cast<int>(i);
    for (std::size_t i = 0; i < OrganicIds.size(); ++i) {
        if (OrganicIds[i] == 0) {
            OrganicIds[i] = networkId;
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline DeconstructionState& StateFor(int networkId) {
    const int index = IndexFor(networkId);
    static DeconstructionState overflow{};
    return index >= 0 ? OrganicStates[static_cast<std::size_t>(index)] : overflow;
}

inline bool HasResearchMark(const AIHeroClient& target) {
    return target.IsValid() && (target.HasBuff("VelkozResearchProc") ||
        target.HasBuff("VelkozResearch") || target.HasBuff("VelkozRMark"));
}

inline int OrganicStackCount(const AIHeroClient& target) {
    if (!target.IsValid()) return 0;
    auto& state = StateFor(static_cast<int>(target.NetworkId()));
    const int stacks = OrganicStacks(state, Now());
    if (target.HasBuff("VelkozPassive") || target.HasBuff("VelkozDeconstruction"))
        return std::max(1, stacks);
    return stacks;
}

inline void RecordOrganicHit(const AIHeroClient& target) {
    if (!target.IsValid()) return;
    const int id = static_cast<int>(target.NetworkId());
    auto& state = StateFor(id);
    (void)AddOrganicStack(state, Now());
}

inline void ClearOrganic(int networkId) {
    const int index = IndexFor(networkId);
    if (index >= 0) OrganicStates[static_cast<std::size_t>(index)] = {};
}

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        CastThrottleReady(slot, reactive);
}

inline bool CanAct(bool reactive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Engine::IsPlayerCrowdControlled(player)) return false;
    return reactive || !ControllerHelpers::PreserveAttack(false);
}

inline AIHeroClient SelectEnemy(const AIHeroClient& selected, float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orb = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range)) return orb;
    return Engine::SelectTarget(range);
}

inline bool SafeRayPosition(const Vector3& endpoint, bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !RWallSafe(player.Position(), endpoint, false))
        return false;
    return RCommitSafe(player.Position(), SDK::NavMesh::IsWall(player.Position()),
        Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(player.Position(), 700.0f),
        Slider(RMenu, "MaximumEnemies", 3), lethal);
}

inline bool LethalR(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kRRange)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int rank = Engine::RuntimeSpells[3]
        ? static_cast<int>(Engine::RuntimeSpells[3]->Level()) : 0;
    const float raw = HasResearchMark(target)
        ? RTrueDamage(rank, player.AP(), true)
        : (Engine::RuntimeSpells[3] ? Engine::RuntimeSpells[3]->GetDamage(target) : 0.0f);
    return raw >= target.Health() + target.AllShield();
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange) ||
        !Ready(0, mode, reactive) || !CanAct(reactive) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero() || prediction.Hitchance < SDK::HitChance::High ||
        !QLineHits(player.Position(), aim, target.Position(), target.BoundingRadius()) ||
        !prediction.CollisionObjects.empty() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange) ||
        !Ready(1, mode, reactive) || !CanAct(reactive)) return false;
    const Vector3 aim = PredictPosition(target, kWDelay);
    if (!aim.IsValid() || aim.IsZero() ||
        !WLineHits(player.Position(), aim, target.Position(), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kWWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    PendingWTargetId = static_cast<int>(target.NetworkId());
    PendingWStartTick = Now();
    PendingWStage = WStage::First;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastWSecond() {
    if (PendingWStage != WStage::First || Now() - PendingWStartTick < 620) return false;
    const auto target = HeroByNetworkId(PendingWTargetId);
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange)) {
        PendingWStage = WStage::None; return false;
    }
    const Vector3 aim = WSecondEndpoint(player.Position(), target.Position());
    if (!aim.IsValid() || ControllerHelpers::ProjectileWallBlocksFromPlayer(aim,
        kWWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    PendingWStage = WStage::Second;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange) ||
        !Ready(2, mode, reactive) || !CanAct(reactive)) return false;
    const Vector3 aim = PredictPosition(target, kEDelay);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > kERange ||
        !ZoneContains(aim, target.Position(), kERadius, target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRange) || RActive ||
        !Ready(3, mode, reactive) || !CanAct(reactive) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target)) return false;
    if (!HasResearchMark(target) && !LethalR(target)) return false;
    const Vector3 aim = PredictPosition(target, 0.12f);
    if (!aim.IsValid() || aim.IsZero() || !RLineHits(player.Position(), aim,
        target.Position(), target.BoundingRadius()) || !SafeRayPosition(aim, LethalR(target))) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    RTargetId = static_cast<int>(target.NetworkId());
    RStartTick = LastCastTick[3] = Now();
    RActive = true;
    RControllerOwned = true;
    ApplyRayMark(StateFor(RTargetId), Now());
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "HarassMana", 52)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "ClearMana", 35)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, kERange) && CastE(target, Mode::Flee, true)) return;
    (void)CastQ(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    if (InterruptTargetId != 0 && InterruptUntil >= Now()) {
        const auto threat = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(threat, kERange) && CastE(threat, Mode::Automatic, true)) return;
        if (Engine::ValidEnemy(threat, kQRange) && CastQ(threat, Mode::Automatic, true)) return;
    }
    if (GapcloserTargetId != 0 && GapcloserUntil >= Now()) {
        const auto threat = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(threat, kERange) && CastE(threat, Mode::Automatic, true)) return;
    }
    if (Engine::ValidEnemy(target, kRRange)) (void)CastR(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const int now = Now();
    for (std::size_t i = 0; i < OrganicIds.size(); ++i) {
        if (OrganicIds[i] != 0 && OrganicStates[i].expiresAtMs > 0 &&
            OrganicStates[i].expiresAtMs <= now) OrganicStates[i] = {};
    }
    if (PendingWStage == WStage::Second && now - PendingWStartTick > 1800)
        PendingWStage = WStage::None;
    if (RActive) {
        const auto player = GameObjects::Player();
        const bool interrupted = player.IsValid() &&
            (Engine::IsPlayerCrowdControlled(player) ||
             (now - RStartTick > 180 && !player.Spellbook().IsChanneling()));
        if (interrupted || now - RStartTick > static_cast<int>(kRChannelSeconds * 1000.0f) + 300) {
            RActive = false; RControllerOwned = false; RTargetId = 0;
        }
    }
    if (InterruptUntil < now) InterruptTargetId = 0;
    if (GapcloserUntil < now) GapcloserTargetId = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (PendingWStage == WStage::First) (void)CastWSecond();
    if (ManualOwnershipUntil > Now()) return true;
    const float range = mode == Mode::Flee ? 900.0f : kRRange;
    const auto target = SelectEnemy(selected, range);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = Now();
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = Now() + Slider(TacticsMenu, "ManualOwnershipMs", 560);
            if (slot == 3) {
                RActive = true; RControllerOwned = false; RTargetId = 0; RStartTick = Now();
            }
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        InterruptTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        InterruptUntil = std::max(analysis.CommitmentUntilTick,
                                  analysis.LineThreatUntilTick);
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
    LastAutoTick = Now();
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const bool passive = Engine::TextContains(args.BuffName, "VelkozPassive") ||
        Engine::TextContains(args.BuffName, "VelkozDeconstruction");
    if (passive) RecordOrganicHit(HeroByNetworkId(static_cast<int>(args.Sender.NetworkId)));
    if (IsLocalPlayer(args.Sender) &&
        Engine::TextContains(args.BuffName, "VelkozR")) RActive = true;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.BuffName, "VelkozPassive") ||
        Engine::TextContains(args.BuffName, "VelkozDeconstruction"))
        ClearOrganic(static_cast<int>(args.Sender.NetworkId));
    if (IsLocalPlayer(args.Sender) &&
        Engine::TextContains(args.BuffName, "VelkozR")) RActive = false;
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.BuffName, "VelkozResearch"))
        ApplyRayMark(StateFor(static_cast<int>(args.Sender.NetworkId)), Now());
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (RActive) args.Process = false;
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); LastAutoTick = Now();
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); LastAutoTick = Now();
    }
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint,
                           GapcloserUntil, kERange, 900);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptUntil,
                         static_cast<int>(kERange), 250, 6000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args)) return;
    if (Engine::TextContains(args.SpellName, "VelkozR") ||
        Engine::TextContains(args.MissileName, "VelkozR")) RActive = true;
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("VelkozTactics", "Vel'Koz deconstruction tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 52, 10, 90));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Plasma Fission split beam"));
    QMenu->Add(new MenuBool("SplitBeam", "Use split beam on miss or collision", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Void Rift two stages"));
    WMenu->Add(new MenuBool("SecondStage", "Always fire second stage", true));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Tectonic Disruption"));
    EMenu->Add(new MenuBool("Interrupt", "Use E against interruptible channels", true));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Life Form Disintegration Ray"));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at channel start", 3, 1, 5));
    RMenu->Add(new MenuBool("RequireResearch", "Require research mark", true));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Wave and jungle farming"));
    FarmMenu->Add(new MenuSlider("ClearMana", "Minimum farm mana percent", 35, 0, 90));
}

inline void OnLoad() {
    OrganicIds.fill(0); OrganicStates.fill({}); LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = 0;
    PendingWTargetId = PendingWStartTick = RTargetId = RStartTick = 0;
    PendingWStage = WStage::None; RActive = RControllerOwned = false;
    InterruptTargetId = InterruptUntil = GapcloserTargetId = GapcloserUntil = 0;
    InterruptEndpoint = GapcloserEndpoint = {};
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Track Organic Deconstruction stacks per enemy with seven-second expiry and three-stack detonation",
    "Use confirmed passive buffs plus polling reconciliation instead of treating every cast as a hit",
    "Aim Plasma Fission with prediction, collision ordering, split-beam geometry and projectile-wall safety",
    "Keep Void Rift first and second line stages distinct and fire the second stage only after its delay",
    "Use Tectonic Disruption as an 800-range 225-radius knockup zone for peel and channel interruption",
    "Require real target reach, hitchance, spell immunity and resource gates before each cast",
    "Reserve Life Form Disintegration Ray for researched or lethal targets and compute true-damage state",
    "Reject ray channels through walls, enemy turrets or unsafe enemy-count commitments",
    "Interrupt and clear the ray on crowd control, silence, death, buff removal or channel expiry",
    "Preserve ordinary attack windups and yield after observed manual Q, W, E or R ownership",
    "Prefer selected enemy then orbwalker target before selector fallback",
    "Support Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic decisions",
    "Wire process-spell, do-cast, buff, attack, gapcloser, interruptable, object and missile callbacks",
    "Pin Riot 26.15 and CommunityDragon 16.15 spell geometry and true-damage research behavior",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Velkoz;
    controller.ControllerId = "champion.kuroaio.ai.velkoz.artillery";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIVelkoz.md";
    controller.ImplementationSummary =
        "Organic Deconstruction stack tracking, split Plasma Fission, two-stage Void Rift, knockup Tectonic Disruption and research-aware true-damage ray channel.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Velkoz
