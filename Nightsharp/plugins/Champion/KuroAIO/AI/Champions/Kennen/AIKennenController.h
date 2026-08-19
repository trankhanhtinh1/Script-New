#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIKennenGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Kennen {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::OrbwalkerHeroTarget;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

struct MarkRecord {
    int NetworkId = 0;
    MarkState State{};
    int LastObservedTick = 0;
};
inline std::array<MarkRecord, 16> Marks{};
inline std::array<int, 4> LastCastTick{};
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline bool LightningRushActive = false;
inline int LightningRushUntil = 0;
inline bool StormActive = false;
inline int StormUntil = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        Engine::ModeEnabled(Engine::ResolvedSpecs[slot], mode) &&
        HasCurrentResource(SpellCost(slot)) &&
        (reactive || !Orbwalker::IsWindingUp() ||
         !Bool(Engine::HumanMenu, "PreserveAttacks", true));
}

inline bool Throttle(int slot, int delay = 80) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline MarkRecord* FindMark(int id, bool create = false) {
    if (id == 0) return nullptr;
    for (auto& mark : Marks) if (mark.NetworkId == id) return &mark;
    if (!create) return nullptr;
    for (auto& mark : Marks) {
        if (mark.NetworkId == 0 || mark.LastObservedTick + kMarkDurationMs < Now()) {
            mark = {};
            mark.NetworkId = id;
            return &mark;
        }
    }
    return nullptr;
}

inline MarkState MarkFor(const AIHeroClient& target) {
    const MarkRecord* mark = FindMark(static_cast<int>(target.NetworkId()));
    return mark ? ExpireMark(mark->State, Now()) : MarkState{};
}

inline bool Marked(const AIHeroClient& target) {
    return HasMark(MarkFor(target));
}

using ControllerHelpers::Protected;

inline AIHeroClient SelectTarget(const AIHeroClient& selected, float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orb = OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range)) return orb;
    return Engine::SelectTarget(range);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(0, mode, reactive) ||
        !Throttle(0) || player.Position().Distance2D(target.Position()) >
            kQRange + target.BoundingRadius()) return false;
    const Vector3 aim = PredictPosition(target, kQDelay);
    if (aim.IsZero() || !aim.IsValid() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline int MarkedNearby(float range = kWRange) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range) || !Marked(enemy)) continue;
        ++count;
    }
    return count;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) || !Throttle(1) ||
        !HasCurrentResource(SpellCost(1)) || MarkedNearby() <= 0) return false;
    if (Engine::ValidEnemy(target) && player.Position().Distance2D(target.Position()) >
        kWRange + target.BoundingRadius()) return false;
    const int required = Slider(WMenu, "MinimumMarks", 1);
    if (MarkedNearby() < std::max(1, required) && !reactive) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool defensive = false,
                  bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode, reactive) || !Throttle(2, 120) ||
        PlayerMobilityLocked()) return false;
    Vector3 requested = Game::CursorPos();
    if (defensive && Engine::ValidEnemy(target)) {
        const Vector3 away = SharedGeometry::Direction2D(target.Position(), player.Position());
        if (!away.IsZero()) requested = player.Position() + away * kERange;
    }
    const Vector3 endpoint = ClampRush(player.Position(), requested);
    if (endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    const int enemies = Engine::CountEnemiesAt(endpoint, kERadius);
    const RushContext context{true, true, defensive,
        Engine::ValidEnemy(target) && target.Position().Distance2D(player.Position()) < 550.0f,
        !defensive && Engine::UnderEnemyTurret(endpoint) &&
            !Engine::UnderEnemyTurret(player.Position()), SDK::NavMesh::IsWall(endpoint),
        enemies, Slider(EMenu, "MaxEndpointEnemies", 1)};
    if (!ShouldLightningRush(context)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastCastTick[2] = Now();
    LightningRushActive = true;
    LightningRushUntil = Now() + Slider(EMenu, "RushWindowMs", 1150);
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive = false,
                  bool reactive = false, bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode, reactive) || !Throttle(3, 140)) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const bool lethal = Engine::ValidEnemy(target) &&
        target.HealthPercent() <= Slider(RMenu, "LethalTargetHp", 22);
    const UltimateContext context{true, enemies > 0, lethal, defensive, manual,
        Orbwalker::IsWindingUp(),
        Engine::UnderEnemyTurret(player.Position()) && !defensive && !lethal,
        enemies, Slider(RMenu, "MinimumTargets", 2)};
    if (!ShouldCastUltimate(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    StormActive = true;
    StormUntil = Now() + Slider(RMenu, "StormWindowMs", 6000);
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (target.HealthPercent() <= 12.0f && CastQ(target, mode)) return true;
    if (Marked(target) && CastW(target, mode)) return true;
    return false;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (Marked(target) && CastW(target, Mode::Combo)) return;
    const auto player = GameObjects::Player();
    const int nearby = player.IsValid() ? Engine::CountEnemiesAt(player.Position(), kRRadius) : 0;
    if (nearby >= Slider(RMenu, "MinimumTargets", 2) && CastR(target, Mode::Combo)) return;
    if (Engine::ValidEnemy(target) && player.IsValid() &&
        player.Position().Distance2D(target.Position()) <= kERadius + 120.0f) {
        (void)CastE(target, Mode::Combo, false);
    }
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentResource() < Slider(WMenu, "HarassEnergy", 45)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CurrentResource() < Slider(FarmMenu, "EnergyReserve", 35)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& threat) {
    if (CastE(threat, Mode::Flee, true, true)) return;
    if (Engine::ValidEnemy(threat)) (void)CastR(threat, Mode::Flee, true, true);
}

inline void ReconcileState() {
    const int now = Now();
    for (auto& mark : Marks) {
        if (mark.NetworkId != 0) mark.State = ExpireMark(mark.State, now);
        if (mark.LastObservedTick + kMarkDurationMs < now) mark = {};
    }
    if (LightningRushActive && now > LightningRushUntil) LightningRushActive = false;
    if (StormActive && now > StormUntil) StormActive = false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    LightningRushActive = LightningRushActive || player.HasBuff("KennenLightningRush");
    StormActive = StormActive || player.HasBuff("KennenShurikenStorm");
    if (!player.HasBuff("KennenLightningRush") && now > LightningRushUntil) LightningRushActive = false;
    if (!player.HasBuff("KennenShurikenStorm") && now > StormUntil) StormActive = false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const AIHeroClient target = SelectTarget(selected, mode == Mode::Flee ? 1000.0f : kQRange);
    if (PlayerOverrideUntil > Now()) return true;
    if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(target)) {
        if (CastR(target, mode, true, true)) return true;
        if (CastE(target, mode, true, true)) return true;
    }
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) &&
            (IncomingThreatUntil > Now() || Engine::CountEnemiesAt(
                GameObjects::Player().Position(), kRRadius) >= Slider(RMenu, "MinimumTargets", 2))) {
            (void)CastR(target, Mode::Automatic, true, true);
        }
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
            if (!Engine::WasControllerCast(slot))
                PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatUntil = std::max(IncomingThreatUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
        if (analysis.LikelyHardCrowdControl)
            IncomingHardCCUntil = std::max(IncomingHardCCUntil,
                std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "KennenMark")) {
        auto* record = FindMark(static_cast<int>(args.Sender.NetworkId), true);
        if (record) {
            const int observed = std::clamp(args.Count, 1, kMarkMaximumStacks);
            record->State = ReconcileMark(record->State, observed, Now());
            record->LastObservedTick = Now();
        }
    }
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "KennenLightningRush")) LightningRushActive = true;
        if (Engine::TextContains(args.BuffName, "KennenShurikenStorm")) StormActive = true;
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "KennenMark")) {
        if (auto* record = FindMark(static_cast<int>(args.Sender.NetworkId))) *record = {};
    }
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "KennenLightningRush")) LightningRushActive = false;
        if (Engine::TextContains(args.BuffName, "KennenShurikenStorm")) StormActive = false;
    }
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF66CCFFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFFF8844u, 1.5f, 32);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("KennenOneTrick", "Kennen storm tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    PassiveMenu = TacticsMenu->AddSubMenu(new Menu("Passive", "Mark of the Storm"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Thundering Shuriken"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Electrical Surge"));
    WMenu->Add(new MenuSlider("MinimumMarks", "Minimum marked enemies", 1, 1, 4));
    WMenu->Add(new MenuSlider("HarassEnergy", "Harass energy percent", 45, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Lightning Rush"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 1, 0, 4));
    EMenu->Add(new MenuSlider("RushWindowMs", "Rush posture window (ms)", 1150, 500, 1800));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Slicing Maelstrom"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum storm targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("LethalTargetHp", "Single-target lethal HP percent", 22, 1, 50));
    RMenu->Add(new MenuSlider("StormWindowMs", "Storm reconciliation window (ms)", 6000, 2000, 8000));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("KennenFarm", "Energy reserves"));
    FarmMenu->Add(new MenuSlider("EnergyReserve", "Energy reserve percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("KennenCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}

inline void OnLoad() {
    Marks = {};
    LastCastTick = {};
    PlayerOverrideUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    LastAutoTargetId = LastAutoTick = 0;
    LightningRushActive = StormActive = false;
    LightningRushUntil = StormUntil = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = PassiveMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Marks = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin all values and spell semantics to Riot 26.15 / CommunityDragon 16.15",
    "Track Mark of the Storm stacks from buff add, update, remove and polling expiry",
    "Treat three marks as a stun-ready boundary and never spend an unobserved mark",
    "Use Q 1050 reach, 50 width, 0.18 second delay and 1700 speed",
    "Use W marked-target range and 300-unit safety radius",
    "Only cast W when at least the configured number of marks are observed",
    "Preserve energy for a follow-up Q, W or R rather than draining blindly",
    "Use E Lightning Rush as a posture with cursor direction and a safe endpoint",
    "Reject E endpoints through walls, turrets or excessive enemy density",
    "Permit defensive E toward a threat when cursor telemetry is unreliable",
    "Use R 550 radius and require configured multi-target value by default",
    "Allow single-target R only for explicit lethal, defensive or manual intent",
    "Reject R beneath a new turret unless lethal or defensive evidence exists",
    "Preserve attack windup unless a reactive storm or escape is required",
    "Yield after observed manual Q W E or R ownership",
    "Preserve selected target before orbwalker and engine selector fallback",
    "Reconcile energy readiness and cooldown timestamps from events and polling",
    "Combo builds marks with Q, converts with W, then enters E or R safely",
    "Harass uses Q and marked W without unsolicited Lightning Rush or ultimate",
    "LaneClear Jungle and LastHit delegate to shared farm policy with energy reserve",
    "Flee uses cursor-safe Lightning Rush and storm peel only under threat",
    "Automatic mode permits only evidenced defense, interrupt or multi-target storm",
    "Never automate items, summoners, movement clicks or manual channels",
    "Keep pure mark, area, rush and ultimate safety rules SDK-independent",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Kennen;
    controller.ControllerId = "champion.kuroaio.ai.kennen.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIKennen.md";
    controller.ImplementationSummary =
        "Mark-aware energy reconciliation, cursor-directed Lightning Rush posture and conservative Slicing Maelstrom multi-target safety.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Kennen
