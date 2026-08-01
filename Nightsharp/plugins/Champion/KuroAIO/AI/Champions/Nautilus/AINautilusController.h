#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AINautilusGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Nautilus {

using namespace Geometry;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PassiveTargetId = 0;
inline int PassiveExpireTick = 0;
inline int QDashExpireTick = 0;
inline int QTargetId = 0;
inline int RTargetId = 0;
inline int ManualOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline bool WActive = false;
inline bool RChannelActive = false;
inline DepthChargeTrack DepthCharge{};
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::SpellRank;

inline bool Throttle(int slot, int delay = 70) {
    return slot >= 0 && slot < 4 &&
           Now() - LastCastTick[static_cast<std::size_t>(slot)] >= delay;
}

inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target);
}

inline AIHeroClient SelectTarget(const AIHeroClient& selected, float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orb = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range)) return orb;
    return Engine::SelectTarget(range);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode) || !Throttle(0, 90)) return false;
    const bool validTarget = Engine::ValidEnemy(target, kQRange);
    const Vector3 aim = validTarget ? PredictPosition(target, kQCastSeconds) : Game::CursorPos();
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > kQRange) return false;
    if (validTarget) {
        if (TargetBlocked(target) || !SegmentContact(player.Position(), aim,
                target.Position(), target.BoundingRadius())) return false;
    } else if (!reactive) {
        return false;
    }
    if (ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQHalfWidth)) return false;
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), aim);
    const bool safe = DashEndpointSafe(endpoint,
        SDK::NavMesh::IsWall(endpoint),
        Engine::UnderEnemyTurret(endpoint), Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(endpoint, 450.0f), Slider(QMenu, "MaxDashEnemies", 2));
    if (!safe && !validTarget) return false;
    if (PreserveAttack(reactive)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    QTargetId = validTarget ? static_cast<int>(target.NetworkId()) : 0;
    QDashExpireTick = Now() + 900;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 100)) return false;
    const bool threat = Engine::ValidEnemy(target, 500.0f);
    if (!reactive && !threat && player.HealthPercent() > Slider(WMenu, "Health", 72)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    WActive = true;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2, 100)) return false;
    const bool targetInRange = Engine::ValidEnemy(target, kEWaveRange + kEWaveRadius);
    if (!targetInRange && !reactive) return false;
    const int nearby = Engine::CountEnemiesAt(player.Position(), kEWaveRadius);
    if (!reactive && nearby <= 0) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !Ready(3, mode) ||
        !Throttle(3, 130) || player.Position().Distance2D(target.Position()) >
            kRRange + target.BoundingRadius()) return false;
    const float damage = EDamage(SpellRank(2), player.AP());
    const bool lethal = ControllerHelpers::Lethal(target, player.CalculateMagicDamage(target, damage));
    const bool alliedFollowup = Engine::CountAlliesAt(target.Position(), 650.0f) > 0;
    if (!ChannelSafe(true, target.HasBuff("BansheeVeil") || target.HasBuff("SivirE"),
                     false, Engine::UnderEnemyTurret(player.Position()),
                     IncomingHardCCUntil > Now(), alliedFollowup) && !reactive) return false;
    if (!lethal && !reactive && Engine::CountEnemiesAt(target.Position(), 350.0f) <
        Slider(RMenu, "MinimumTargets", 2)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastCastTick[3] = Now();
    RTargetId = static_cast<int>(target.NetworkId());
    RChannelActive = BeginDepthCharge(DepthCharge, RTargetId, target.Position(), Now());
    return true;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (now >= PassiveExpireTick) PassiveTargetId = 0;
    if (now >= QDashExpireTick) QTargetId = 0;
    if (!player.HasBuff("NautilusW")) WActive = false;
    if (RChannelActive && RTargetId > 0) {
        const auto target = ControllerHelpers::HeroByNetworkId(RTargetId);
        if (!Engine::ValidEnemy(target)) {
            RChannelActive = false;
            RTargetId = 0;
        } else {
            RChannelActive = UpdateDepthCharge(DepthCharge, RTargetId, target.Position(), now);
        }
    }
    if (now >= ManualOverrideUntil) ManualOverrideUntil = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    if (ManualOverrideUntil > Now()) return true;
    const AIHeroClient target = SelectTarget(selected, mode == Mode::Flee ? 900.0f : kQRange);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 900.0f);
    if (mode == Mode::Flee) {
        if (Engine::ValidEnemy(threat)) (void)CastQ(threat, mode, true);
        (void)CastW(threat, mode, true);
        (void)CastE(threat, mode, true);
        return true;
    }
    if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(threat)) {
        if (CastE(threat, mode, true)) return true;
        (void)CastW(threat, mode, true);
    }
    switch (mode) {
    case Mode::Combo:
        if (CastR(target, mode)) return true;
        if (CastQ(target, mode)) return true;
        if (CastW(target, mode)) return true;
        (void)CastE(target, mode);
        break;
    case Mode::Harass:
        if (CastQ(target, mode)) return true;
        (void)CastE(target, mode);
        break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        (void)Engine::TryFarm(mode);
        (void)CastE(target, mode);
        break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && (IncomingThreatUntil > Now() ||
            target.HealthPercent() <= Slider(RMenu, "TargetHP", 72)))
            (void)CastR(target, mode, true);
        break;
    default:
        break;
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
            if (!Engine::WasControllerCast(slot)) ManualOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
            if (slot == 3) RChannelActive = true;
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
        IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    const int remaining = ControllerHelpers::RemainingMilliseconds(args.EndTime, 6000, 250, 10000);
    if (Engine::TextContains(args.BuffName, "NautilusPassive")) {
        PassiveTargetId = id;
        PassiveExpireTick = Now() + remaining;
    }
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "NautilusW")) WActive = true;
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "NautilusR")) RChannelActive = true;
    if (Engine::TextContains(args.BuffName, "NautilusR")) {
        RTargetId = id;
        RChannelActive = true;
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    const int id = args.Sender.IsValid() ? static_cast<int>(args.Sender.NetworkId) : 0;
    if (Engine::TextContains(args.BuffName, "NautilusPassive") && id == PassiveTargetId) PassiveTargetId = 0;
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "NautilusW")) WActive = false;
    if (Engine::TextContains(args.BuffName, "NautilusR")) {
        RChannelActive = false;
        if (id == RTargetId) RTargetId = 0;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (PassiveTargetId > 0 && args.Target.IsValid() &&
        PassiveTargetId == static_cast<int>(args.Target.NetworkId())) args.Process = true;
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF3366CCu, 1.3f, 48);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFCC3355u, 1.3f, 48);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("NautilusOneTrick", "Nautilus vanguard"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("NautilusQ", "Dredge Line"));
    QMenu->Add(new MenuSlider("MaxDashEnemies", "Maximum dash endpoint enemies", 2, 1, 5));
    WMenu = TacticsMenu->AddSubMenu(new Menu("NautilusW", "Titan's Wrath"));
    WMenu->Add(new MenuSlider("Health", "Use shield below health percent", 72, 20, 95));
    EMenu = TacticsMenu->AddSubMenu(new Menu("NautilusE", "Riptide"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("NautilusR", "Depth Charge"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum nearby enemies", 2, 1, 5));
    RMenu->Add(new MenuSlider("TargetHP", "Automatic target health", 72, 10, 95));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("NautilusFarm", "Wave and jungle policy"));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("NautilusCoach", "Decision visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}

inline void OnLoad() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = PassiveTargetId = PassiveExpireTick = 0;
    QDashExpireTick = QTargetId = RTargetId = ManualOverrideUntil = 0;
    IncomingThreatUntil = IncomingHardCCUntil = 0;
    WActive = RChannelActive = false;
    DepthCharge = {};
    LastMode = Mode::None;
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    WActive = RChannelActive = false;
    DepthCharge = {};
}

inline constexpr const char* Scenarios[] = {
    "Use Riot 26.15 and CommunityDragon 16.15 Summoner's Rift values",
    "Preserve selected target precedence before orbwalker and selector fallback",
    "Predict Dredge Line and reject collision, projectile-wall and uncertain endpoints",
    "Track anchor dash endpoint, terrain, turret and enemy-density safety",
    "Track first-hit passive root by buff events and polling expiry",
    "Use Titan's Wrath shield when low or under a verified close threat",
    "Use Riptide only when a live target is inside the wave radius or for reactive peel",
    "Track Depth Charge target, travel position, channel and expiry by event plus polling",
    "Reject R against spell shields, invalid targets and unsafe turret channels",
    "Preserve AA windup and passive root ownership around manual or controller casts",
    "Yield after observed manual Q, W, E or R ownership",
    "Respect mana, cooldown, reach, collision, prediction and enemy-count gates",
    "Cover Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes",
    "Automatic mode is restricted to defensive R or observed incoming threat",
    "Keep Q collision, dash safety, W shield, E waves and R tracking independently testable",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Nautilus;
    controller.OwnsDecisionLoop = true;
    controller.ControllerId = "champion.kuroaio.ai.nautilus.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AINautilus.md";
    controller.ImplementationSummary =
        "Collision-safe anchor engage, passive-root tracking, shield timing, Riptide peel and Depth Charge channel safety.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Nautilus
