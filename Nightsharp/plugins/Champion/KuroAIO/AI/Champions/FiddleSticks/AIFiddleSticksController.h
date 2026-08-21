#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIFiddleSticksGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::FiddleSticks {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::SpellSlotOrEventNameContainsAny;
using ControllerHelpers::SpellRank;
using ControllerHelpers::TextContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* FearMenu = nullptr;
inline Menu* DrainMenu = nullptr;
inline Menu* ReapMenu = nullptr;
inline Menu* CrowstormMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline std::array<int, 8> FearExpireTick{};
inline std::array<int, 8> FearTargetId{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastCombatTick = 0;

inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int EffigyObjectId = 0;
inline Vector3 EffigyPosition{};
inline bool EffigyPresent = false;
inline bool WChannel = false;
inline bool WOwned = false;
inline bool WInterrupted = false;
inline int WStartTick = 0;
inline int WTargetId = 0;
inline bool RChannel = false;
inline bool ROwned = false;
inline bool RInterrupted = false;
inline bool RStormActive = false;
inline int RStartTick = 0;
inline int RTargetId = 0;
inline Vector3 RDestination{};
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Lethal;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Protected;
using ControllerHelpers::Ready;

inline bool Throttle(int slot, int delay = 90) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool RuntimeWActive() {
    const auto player = GameObjects::Player();
    return Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsCharging() ||
        (player.IsValid() && (player.HasBuff("FiddlesticksW") ||
                              player.HasBuff("FiddlesticksWChannel") ||
                              player.HasBuff("FiddlesticksDrain")));
}

inline bool RuntimeRChanneling() {
    const auto player = GameObjects::Player();
    return Engine::RuntimeSpells[3] && Engine::RuntimeSpells[3]->IsCharging() ||
        (player.IsValid() && (player.HasBuff("FiddlesticksRChannel") ||
                              player.HasBuff("FiddlesticksCrowstormChannel")));
}

inline bool RuntimeRStorm() {
    const auto player = GameObjects::Player();
    return player.IsValid() && (player.HasBuff("FiddlesticksR") ||
                                player.HasBuff("FiddlesticksCrowstorm"));
}

inline bool PlayerInBrush() {
    const auto player = GameObjects::Player();
    return player.IsValid() && ::CoreNavGrid::IsBrush(player.Position());
}
inline bool OutOfCombat() {
    return LastCombatTick == 0 || Now() - LastCombatTick > 900;
}

inline bool VisionAmbushReady(const Vector3& destination = {}) {
    const bool destinationBrush = destination.IsValid() &&
        ::CoreNavGrid::IsBrush(destination);
    const bool effigyAmbush = EffigyPresent && OutOfCombat();
    return PlayerInBrush() || destinationBrush || effigyAmbush;
}

inline int ExpireForTarget(int networkId) {
    for (std::size_t i = 0; i < FearTargetId.size(); ++i) {
        if (FearTargetId[i] == networkId) return FearExpireTick[i];
    }
    return 0;
}

inline void RememberFear(int networkId, int lifetimeMs = 4500) {
    if (networkId == 0) return;
    for (std::size_t i = 0; i < FearTargetId.size(); ++i) {
        if (FearTargetId[i] == networkId || FearTargetId[i] == 0 ||
            FearExpireTick[i] <= Now()) {
            FearTargetId[i] = networkId;
            FearExpireTick[i] = Now() + lifetimeMs;
            return;
        }
    }
}

inline bool RecentlyFeared(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) && ExpireForTarget(static_cast<int>(target.NetworkId())) > Now();
}

inline AIHeroClient SelectTarget(const AIHeroClient& selected, float range = kQRange) {
    return PreferredEnemyTarget(selected, range);
}

inline bool TargetProtected(const AIHeroClient& target) {
    return Protected(target) || target.HasBuff("FiddlesticksQFear") ||
        target.HasBuff("FiddlesticksQFearImmunity") ||
        target.HasBuff("kindredrnodeathbuff");
}

inline int PredictedStormVictims(const Vector3& destination) {
    if (!destination.IsValid()) return 0;
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) &&
            destination.Distance2D(PredictPosition(enemy, kRDelay)) <=
                kRStormRadius + enemy.BoundingRadius()) ++count;
    }
    return count;
}

inline int NearbyDrainVictims() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && InDrainRadius(player.Position(), enemy.Position(), enemy.BoundingRadius())) ++count;
    }
    return count;
}

inline int NearbyLaneVictims() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int count = 0;
    for (const auto& minion : GameObjects::EnemyLaneMinions()) {
        if (minion.IsValid() && !minion.IsDead() && minion.IsTargetable() &&
            InDrainRadius(player.Position(), minion.Position(), minion.BoundingRadius())) ++count;
    }
    return count;
}

inline int NearbyJungleVictims() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int count = 0;
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() && monster.IsTargetable() &&
            InDrainRadius(player.Position(), monster.Position(), monster.BoundingRadius())) ++count;
    }
    return count;
}

inline DrainGate BuildDrainGate(Mode mode, bool owned, bool interrupted,
                                bool targetInRadius, bool lethal = false,
                                bool emergency = false) {
    const int nearby = mode == Mode::Jungle ? NearbyJungleVictims() :
                       mode == Mode::LaneClear || mode == Mode::LastHit ? NearbyLaneVictims() :
                       NearbyDrainVictims();
    const int minimum = mode == Mode::LaneClear ? Slider(FarmMenu, "MinimumWUnits", 3) :
                        mode == Mode::Jungle || mode == Mode::Flee ? 1 :
                        Slider(DrainMenu, "MinimumTargets", 1);
    const bool brush = PlayerInBrush();
    return {Ready(1, mode), owned, owned && RuntimeWActive(), interrupted,
            !brush && !EffigyPresent, brush, VisionAmbushReady(), targetInRadius,
            lethal, emergency, nearby, minimum};
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RChannel || WChannel || !Engine::ValidEnemy(target, kQRange + 30.0f) ||
        TargetProtected(target) || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive)) return false;
    const FearGate gate{true, true, true, RecentlyFeared(target),
                        HasSpellShieldOrImmunity(target), Lethal(target, Engine::RuntimeSpells[0]->GetDamage(target))};
    if (!CanFear(gate)) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    LastCastTick[0] = LastCombatTick = Now();
    RememberFear(static_cast<int>(target.NetworkId()));
    return true;
}

inline bool StartW(Mode mode, bool reactive = false, int targetId = 0) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || WChannel || RChannel || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive)) return false;
    const AIHeroClient target = targetId != 0 ? ControllerHelpers::HeroByNetworkId(targetId) : AIHeroClient{};
    const bool lethal = Engine::ValidEnemy(target) && Engine::RuntimeSpells[1] &&
        Lethal(target, Engine::RuntimeSpells[1]->GetDamage(target));
    const bool emergency = mode == Mode::Flee && player.HealthPercent() <= Slider(DrainMenu, "EmergencyHp", 42);
    const DrainGate gate = BuildDrainGate(mode, false, false,
        mode == Mode::Jungle || mode == Mode::LaneClear || mode == Mode::LastHit
            ? (NearbyJungleVictims() > 0 || NearbyLaneVictims() > 0)
            : NearbyDrainVictims() > 0,
        lethal, emergency);
    if (!CanStartDrain(gate)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WChannel = true;
    WOwned = true;
    WInterrupted = false;
    WStartTick = LastCastTick[1] = LastCombatTick = Now();
    WTargetId = targetId;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || WChannel || RChannel || !Engine::ValidEnemy(target, kEDistance + 50.0f) ||
        TargetProtected(target) || !Ready(2, mode) || !Throttle(2) || PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = PredictPosition(target, kEDelay);
    const ReapAim evaluated = EvaluateReap(player.Position(), aim, PredictPosition(target, kEDelay),
        target.BoundingRadius(), SDK::NavMesh::IsWallBetween(player.Position(), aim, kEHalfWidth * 0.35f),
        prediction.Hitchance >= SDK::HitChance::High);
    if (!evaluated.Valid || evaluated.WallBlocked || !evaluated.PredictionAccepted ||
        !evaluated.CenterHit && !Bool(ReapMenu, "AllowOuterSlow", true)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = LastCombatTick = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool flee = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || WChannel || RChannel || RStormActive || !Engine::ValidEnemy(target, kRRange + 80.0f) ||
        TargetProtected(target) || !Ready(3, mode) || !Throttle(3, 170) ||
        PreserveAttack(flee)) return false;
    const Vector3 requested = PredictPosition(target, kRDelay);
    const Vector3 destination = ClampRDestination(player.Position(), requested);
    if (!destination.IsValid() || destination.IsZero() || player.Position().Distance2D(destination) > kRRange + target.BoundingRadius() ||
        SDK::NavMesh::IsWall(destination)) return false;
    const int victims = PredictedStormVictims(destination);
    const bool turret = Engine::UnderEnemyTurret(destination) && !flee;
    const bool unsafe = ControllerHelpers::PlayerMobilityLocked();
    const bool ambush = VisionAmbushReady(destination);
    const TeleportGate gate{true, false, false, false, true, !ambush,
        ambush, SDK::NavMesh::IsWall(destination), turret, unsafe,
        victims > 0, Lethal(target, Engine::RuntimeSpells[3]->GetDamage(target)), flee,
        victims, Slider(CrowstormMenu, "MinimumVictims", 2), Slider(CrowstormMenu, "MaxLandingEnemies", 3)};
    if (!CanStartCrowstorm(gate) || (!flee && Engine::CountEnemiesAt(destination, kRStormRadius) > gate.MaximumEnemies)) return false;
    if (!Engine::ControllerCastPosition(3, destination)) return false;
    RChannel = true;
    ROwned = true;
    RInterrupted = false;
    RStartTick = LastCastTick[3] = LastCombatTick = Now();
    RTargetId = static_cast<int>(target.NetworkId());
    RDestination = destination;
    return true;
}

inline bool Farm(Mode mode) {
    if (mode == Mode::Jungle) return StartW(mode, false);
    if (mode == Mode::LaneClear) {
        if (NearbyLaneVictims() >= Slider(FarmMenu, "MinimumWUnits", 3)) return StartW(mode, false);
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable()) continue;
            if (minion.Position().Distance2D(GameObjects::Player().Position()) <= kEDistance &&
                Ready(2, mode) && Throttle(2) && Engine::ControllerCastPosition(2, minion.Position())) {
                LastCastTick[2] = LastCombatTick = Now();
                return true;
            }
        }
    }
    if (mode == Mode::LastHit && Ready(2, mode) && Throttle(2)) {
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable()) continue;
            if (Engine::RuntimeSpells[2]->GetDamage(minion) >= minion.Health() &&
                minion.Position().Distance2D(GameObjects::Player().Position()) <= kEDistance &&
                Engine::ControllerCastPosition(2, minion.Position())) {
                LastCastTick[2] = LastCombatTick = Now();
                return true;
            }
        }
    }
    return false;
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool runtimeW = RuntimeWActive();
    const bool runtimeRChannel = RuntimeRChanneling();
    const bool runtimeStorm = RuntimeRStorm();
    if (runtimeW) {
        if (!WChannel) WStartTick = now;
        WChannel = true;
    } else if (WChannel && now - WStartTick > 280) {
        if (now - WStartTick < static_cast<int>(kWChannelSeconds * 1000.0f) - 100) WInterrupted = true;
        WChannel = WOwned = false;
        WTargetId = 0;
    }
    if (runtimeRChannel) {
        if (!RChannel) RStartTick = now;
        RChannel = true;
    } else if (RChannel && now - RStartTick > 350) {
        if (now - RStartTick < static_cast<int>(kRChannelSeconds * 1000.0f) - 100) RInterrupted = true;
        RChannel = ROwned = false;
    }
    RStormActive = runtimeStorm || (RStormActive && now - RStartTick < 6000);
    if (!runtimeStorm && RStormActive && now - RStartTick >= 6000) RStormActive = false;
    for (std::size_t i = 0; i < FearExpireTick.size(); ++i) {
        if (FearExpireTick[i] <= now) FearTargetId[i] = FearExpireTick[i] = 0;
    }
    if (InterruptExpireTick <= now) InterruptTargetId = 0;
    if (IncomingThreatUntil <= now) IncomingThreatUntil = 0;
    if (IncomingHardCCUntil <= now) IncomingHardCCUntil = 0;
}

inline void ReconcileChannels() {
    ReconcileState();
    if (WChannel) {
        const int targetCount = NearbyDrainVictims();
        const bool targetInRadius = targetCount > 0 || NearbyJungleVictims() > 0 || NearbyLaneVictims() > 0;
        const DrainGate gate = BuildDrainGate(LastMode, WOwned, WInterrupted, targetInRadius,
            false, GameObjects::Player().HealthPercent() <= Slider(DrainMenu, "EmergencyHp", 42));
        const float elapsed = static_cast<float>(Now() - WStartTick) / 1000.0f;
        if (!ShouldContinueDrain(gate) || ShouldFinishDrain(gate, elapsed)) {
            if (!ShouldFinishDrain(gate, elapsed)) WInterrupted = true;
            WChannel = WOwned = false;
        }
    }
    if (RChannel) {
        const auto player = GameObjects::Player();
        const bool ambush = VisionAmbushReady(RDestination);
        const bool safe = ambush && !ControllerHelpers::PlayerMobilityLocked();
        const TeleportGate gate{true, ROwned, true, RInterrupted, RDestination.IsValid(),
            !ambush, ambush, SDK::NavMesh::IsWall(RDestination),
            Engine::UnderEnemyTurret(RDestination), !safe,
            PredictedStormVictims(RDestination) > 0, false, false,
            PredictedStormVictims(RDestination), Slider(CrowstormMenu, "MinimumVictims", 2),
            Slider(CrowstormMenu, "MaxLandingEnemies", 3)};
        if (!CanTeleportStorm(gate)) RInterrupted = true;
        if (Now() - RStartTick > static_cast<int>((kRChannelSeconds + 0.25f) * 1000.0f)) {
            RChannel = ROwned = false;
            RStormActive = !RInterrupted;
        }
        (void)player;
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileChannels();
    if (WChannel || RChannel) return true;
    const AIHeroClient target = SelectTarget(selected, std::max(kRRange, kQRange));
    const bool threatened = IncomingThreatUntil > Now() || IncomingHardCCUntil > Now();
    if (RStormActive && Engine::ValidEnemy(target)) {
        if (CastQ(target, mode, true)) return true;
        if (CastE(target, mode, true)) return true;
        return StartW(mode, true, static_cast<int>(target.NetworkId()));
    }
    switch (mode) {
    case Mode::Combo:
        if (Engine::ValidEnemy(target) && CastR(target, mode)) return true;
        if (Engine::ValidEnemy(target) && CastQ(target, mode)) return true;
        if (Engine::ValidEnemy(target) && CastE(target, mode)) return true;
        if (Engine::ValidEnemy(target) && StartW(mode, false, static_cast<int>(target.NetworkId()))) return true;
        return false;
    case Mode::Harass:
        return Engine::ValidEnemy(target) && (CastQ(target, mode) || CastE(target, mode));
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        return Farm(mode);
    case Mode::Flee: {
        const AIHeroClient threat = NearestEnemyToPlayer(target, 900.0f);
        if (Engine::ValidEnemy(threat) && CastQ(threat, mode, true)) return true;
        if (Engine::ValidEnemy(threat) && CastE(threat, mode, true)) return true;
        if (Engine::ValidEnemy(threat) && CastR(threat, mode, true)) return true;
        return StartW(mode, true);
    }
    case Mode::Automatic:
        if (threatened && Engine::ValidEnemy(target) && CastQ(target, mode, true)) return true;
        if (Engine::ValidEnemy(target) && Lethal(target, Engine::RuntimeSpells[0] ? Engine::RuntimeSpells[0]->GetDamage(target) : 0.0f) && CastQ(target, mode, true)) return true;
        if (Engine::ValidEnemy(target) && CastR(target, mode)) return true;
        return Engine::ValidEnemy(target) && CastE(target, mode, true);
    default: return false;
    }
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        LastCombatTick = now;
        const int slot = args.Slot;
        if (slot >= 0 && slot < 4) {
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (!Engine::WasControllerCast(slot)) {
                if (slot == static_cast<int>(SDK::SpellSlot::W)) WInterrupted = true;
                if (slot == static_cast<int>(SDK::SpellSlot::R)) RInterrupted = true;
            }
        }
        if (SpellSlotOrEventNameContainsAny(args, SDK::SpellSlot::W, {"FiddlesticksW", "Drain"})) WChannel = true;
        if (SpellSlotOrEventNameContainsAny(args, SDK::SpellSlot::R, {"Crowstorm", "FiddlesticksR"})) RChannel = true;
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatUntil = std::max(IncomingThreatUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
        if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
        if (WChannel || RChannel) { WInterrupted = WChannel; RInterrupted = RChannel; }
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId != 0 ? args.TargetNetworkId : args.Target.NetworkId);
        LastAutoTick = LastCombatTick = Now();
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) {
        if (TextContainsAny(args.BuffName, {"FiddlesticksW", "FiddlesticksDrain"})) WChannel = true;
        if (TextContainsAny(args.BuffName, {"FiddlesticksR", "Crowstorm"})) RStormActive = true;
        return;
    }
    if (TextContainsAny(args.BuffName, {"FiddlesticksQFear", "FiddleSticksQFear", "Fear"})) RememberFear(id, std::max(800, ControllerHelpers::RemainingMilliseconds(args.EndTime, 1200, 800, 6000)));
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (TextContainsAny(args.BuffName, {"FiddlesticksW", "FiddlesticksDrain"}) && WChannel) WInterrupted = true;
        if (TextContainsAny(args.BuffName, {"FiddlesticksRChannel"}) && RChannel) RInterrupted = true;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (WChannel || RChannel) { args.Process = false; return; }
    if (!args.Target.IsValid()) return;
    const auto target = ControllerHelpers::HeroByNetworkId(static_cast<int>(args.Target.NetworkId()));
    if (Engine::ValidEnemy(target) && TargetProtected(target)) args.Process = false;
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) LastCombatTick = Now();
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)args;
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 800);
    if (WChannel || RChannel) { WInterrupted = WChannel; RInterrupted = RChannel; }
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptExpireTick, 900, 250, 5000);
    IncomingHardCCUntil = std::max(IncomingHardCCUntil, InterruptExpireTick);
    if (WChannel || RChannel) { WInterrupted = WChannel; RInterrupted = RChannel; }
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (TextContainsAny(args.Sender.Name, {"FiddlesticksEffigy", "FiddleSticksEffigy", "Scarecrow"})) {
        EffigyPresent = true;
        EffigyObjectId = static_cast<int>(args.Sender.NetworkId);
        EffigyPosition = args.Sender.Position;
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (EffigyObjectId != 0 && static_cast<int>(args.Sender.NetworkId) == EffigyObjectId) {
        EffigyPresent = false;
        EffigyObjectId = 0;
        EffigyPosition = {};
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) { (void)args; }

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFAA66FFu, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kWDrainRadius, 0xFF66CCAAu, 1.2f, 48);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFCC66FFu, 1.2f, 40);
    if (RDestination.IsValid()) Drawing::DrawCircle(RDestination, kRStormRadius, 0xFFFFAA66u, 1.5f, 48);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("FiddleSticksOneTrick", "Fiddlesticks tactics"));
    FearMenu = TacticsMenu->AddSubMenu(new Menu("Terrify", "Fear and ambush gating"));
    FearMenu->Add(new MenuBool("RequireAmbushForPassive", "Require brush/effigy for fear", true));
    DrainMenu = TacticsMenu->AddSubMenu(new Menu("BountifulHarvest", "Drain channel safety"));
    DrainMenu->Add(new MenuSlider("MinimumTargets", "Minimum champion drain targets", 1, 1, 4));
    DrainMenu->Add(new MenuSlider("EmergencyHp", "Emergency drain HP", 42, 10, 80));
    ReapMenu = TacticsMenu->AddSubMenu(new Menu("Reap", "Silence cone"));
    ReapMenu->Add(new MenuBool("AllowOuterSlow", "Allow outer slow when center misses", true));
    CrowstormMenu = TacticsMenu->AddSubMenu(new Menu("Crowstorm", "Teleport channel safety"));
    CrowstormMenu->Add(new MenuSlider("MinimumVictims", "Minimum predicted storm victims", 2, 1, 5));
    CrowstormMenu->Add(new MenuSlider("MaxLandingEnemies", "Maximum enemies at landing", 3, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("FiddlesticksFarm", "Distinct lane and jungle drain"));
    FarmMenu->Add(new MenuSlider("MinimumWUnits", "Minimum lane units for W", 3, 1, 8));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("FiddlesticksCoach", "Vision and range telemetry"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/R ranges and storm landing", false));
}

inline void OnLoad() {
    std::fill(LastCastTick.begin(), LastCastTick.end(), 0);
    std::fill(FearExpireTick.begin(), FearExpireTick.end(), 0);
    std::fill(FearTargetId.begin(), FearTargetId.end(), 0);
    LastAutoTargetId = LastAutoTick = LastCombatTick = 0;
    IncomingThreatUntil = IncomingHardCCUntil = InterruptTargetId = InterruptExpireTick = 0;
    EffigyObjectId = 0;
    EffigyPosition = {};
    EffigyPresent = false;
    WChannel = WOwned = WInterrupted = false;
    RChannel = ROwned = RInterrupted = RStormActive = false;
    RDestination = {};
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = FearMenu = DrainMenu = ReapMenu = CrowstormMenu = FarmMenu = CoachMenu = nullptr;
    EffigyPresent = false;
    EffigyObjectId = 0;
    WChannel = WOwned = WInterrupted = false;
    RChannel = ROwned = RInterrupted = RStormActive = false;
    RDestination = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin Riot 26.15 and CommunityDragon 16.15 Fiddlesticks values",
    "Reconcile effigy object lifecycle and brush/fog vision state",
    "Prefer selected/orbwalker target only when real Q/E/W/R reach exists",
    "Use Q activation fear while retaining passive unseen/effigy ambush state",
    "Reject protected, spell-shielded and recently feared targets",
    "Start W only with an owned channel, brush/vision safety and a reachable drain victim",
    "Reconcile W completion versus interruption and preserve health execute timing",
    "Aim E from prediction, check center silence cone and wall-safe reach",
    "Require unseen brush/effigy/fog before Crowstorm channel and teleport destination",
    "Reject visible, interrupted, turret, wall and unsafe mobility Crowstorm commits",
    "Count predicted storm victims and cap enemy pressure at landing",
    "Handle distinct Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes",
    "Use W for jungle objectives and lane clusters rather than generic farm ordering",
    "Preserve auto-attack windup and reconcile spell state/",
    "Track enemy fear, interruptable casts, gapclosers and threat windows by events and polling",
    "Complete every ChampionController callback without claiming item or summoner ownership",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Fiddlesticks;
    controller.ControllerId = "champion.kuroaio.ai.fiddlesticks.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIFiddleSticks.md";
    controller.ImplementationSummary =
        "Effigy and brush vision state, owned W drain reconciliation, Q fear and E center silence, "
        "plus unseen Crowstorm channel/teleport safety with distinct lane, jungle and flee policies.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::FiddleSticks
