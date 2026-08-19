#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIRekSaiGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::RekSai {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
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
inline bool Burrowed = false;
inline bool PassiveHealing = false;
inline std::array<TunnelState, 16> Tunnels{};
inline int MarkedTargetId = 0;
inline int MarkExpireTick = 0;
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int PendingTunnelTick = 0;
inline Vector3 PendingTunnelStart{};
inline Vector3 PendingTunnelEnd{};

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 80) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target) || target.HasBuff("kindredrnodeathbuff") ||
           target.HasBuff("ChronoShift");
}
using ControllerHelpers::PreserveAttack;
inline float Fury() { return std::clamp(CurrentResource(), 0.0f, 100.0f); }
inline bool IsMarked(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
           (target.HasBuff("RekSaiPreySeeker") || target.HasBuff("RekSaiPrey") ||
            static_cast<int>(target.NetworkId()) == MarkedTargetId) &&
           MarkExpireTick > Now();
}
inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, QRawDamage(SpellRank(0), player.TotalAttackDamage())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = FuriousBiteRawDamage(SpellRank(2), player.BonusAttackDamage(), Fury());
    return IsFuryTrueDamage(Fury()) ? raw : player.CalculatePhysicalDamage(target, raw);
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, VoidRushRawDamage(SpellRank(3),
            player.BonusAttackDamage(), 100.0f - target.HealthPercent())) : 0.0f;
}
using ControllerHelpers::Lethal;
inline bool SafeEndpoint(const Vector3& endpoint, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    if (!defensive && Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position())) return false;
    const int maxEnemies = Slider(RMenu, "MaxLandingEnemies", 2);
    return Engine::CountEnemiesAt(endpoint, 450.0f) <= maxEnemies || defensive;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!Ready(1, mode) || !Throttle(1) || PreserveAttack(reactive)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kWKnockupRadius + 30.0f);
    if (Burrowed) {
        if (!reactive && enemies == 0 && mode != Mode::Flee &&
            player.HealthPercent() > Slider(WMenu, "UnburrowHP", 65)) return false;
        if (!Engine::ControllerCastSelf(1)) return false;
        LastCastTick[1] = Now();
        Burrowed = false;
        return true;
    }
    if (mode != Mode::Flee && Engine::ValidEnemy(target) &&
        !KnockupHits(player.Position(), target.Position(), target.BoundingRadius()) &&
        !reactive) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    Burrowed = true;
    return true;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!Ready(0, mode) || !Throttle(0) || Protected(target) || PreserveAttack(reactive))
        return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (!Burrowed) {
        if (player.Position().Distance2D(target.Position()) >
            kQUnburrowedRange + target.BoundingRadius()) return false;
        if (!Engine::ControllerCastSelf(0)) return false;
    } else {
        const Vector3 aim = PredictPosition(target, kQDelay);
        if (!aim.IsValid() || aim.IsZero() ||
            player.Position().Distance2D(aim) > kQBurrowedRange + target.BoundingRadius() ||
            SDK::NavMesh::IsWallBetween(player.Position(), aim, kQWidth * 0.5f) ||
            ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f) ||
            !Engine::ControllerCastPosition(0, aim)) return false;
        MarkedTargetId = static_cast<int>(target.NetworkId());
        MarkExpireTick = Now() + 5000;
    }
    LastCastTick[0] = Now();
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    if (!Ready(2, mode) || !Throttle(2) || PreserveAttack(reactive)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (!Burrowed) {
        if (Protected(target) || player.Position().Distance2D(target.Position()) >
            kERange + target.BoundingRadius()) return false;
        if (!lethal && Fury() < Slider(EMenu, "MinimumFury", 45)) return false;
        if (!Engine::ControllerCastUnit(2, target)) return false;
        LastCastTick[2] = Now();
        return true;
    }
    const Vector3 cursor = Game::CursorPos();
    const Vector3 endpoint = ClampTunnelEndpoint(player.Position(), cursor);
    if (!TunnelPlacementSafe(player.Position(), endpoint, SDK::NavMesh::IsWall(endpoint),
        !reactive && Engine::UnderEnemyTurret(endpoint), Engine::CountEnemiesAt(endpoint, 450.0f),
        Slider(EMenu, "MaxTunnelEnemies", 2))) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    PendingTunnelTick = Now();
    PendingTunnelStart = player.Position();
    PendingTunnelEnd = endpoint;
    LastCastTick[2] = Now();
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 140) || Protected(target) ||
        !IsMarked(target) || player.Position().Distance2D(target.Position()) >
            kRRange + target.BoundingRadius() || PreserveAttack(reactive)) return false;
    const bool lethal = Lethal(target, RDamage(target));
    const bool safe = MarkedTargetRAllowed(true, true, !SDK::NavMesh::IsWall(target.Position()),
        !reactive && Engine::UnderEnemyTurret(target.Position()) &&
            !Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(target.Position(), kRLandingRadius),
        Slider(RMenu, "MaxLandingEnemies", 2), lethal);
    if (!safe || (!lethal && target.HealthPercent() > Slider(RMenu, "TargetHP", 48))) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastCastTick[3] = Now();
    MarkedTargetId = MarkExpireTick = 0;
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (!Burrowed && Lethal(target, EDamage(target)) && CastE(target, mode, true, true)) return true;
    if (IsMarked(target) && Lethal(target, RDamage(target)) && CastR(target, mode, true)) return true;
    if (Lethal(target, QDamage(target)) && CastQ(target, mode, true)) return true;
    return false;
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (TryKillSecure(target, Mode::Combo)) return;
    if (Burrowed && CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (!Burrowed && CastE(target, Mode::Combo)) return;
    if (IsMarked(target)) (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (Burrowed && CastQ(target, Mode::Harass)) return;
    if (!Burrowed && CastQ(target, Mode::Harass)) return;
    if (!Burrowed && Fury() >= Slider(EMenu, "HarassFury", 70)) (void)CastE(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (!Burrowed && CastW(target, Mode::Flee, true)) return;
    if (Burrowed) (void)CastE(target, Mode::Flee, true);
}
inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    Burrowed = player.HasBuff("RekSaiBurrowed") || player.HasBuff("RekSaiW") ||
               player.HasBuff("RekSaiBurrow");
    PassiveHealing = Burrowed && Fury() > 0.0f && player.HealthPercent() <
        Slider(WMenu, "PassiveHealHP", 72);
    if (MarkExpireTick > 0 && now >= MarkExpireTick) MarkedTargetId = MarkExpireTick = 0;
    for (auto& tunnel : Tunnels) if (tunnel.ExpireTick <= now) tunnel = {};
    if (PendingTunnelTick && now - PendingTunnelTick > 1000) {
        PendingTunnelTick = 0; PendingTunnelStart = PendingTunnelEnd = {};
    }
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f : kRRange);
    if (ManualOverrideUntil > Now()) return true;
    if (PassiveHealing && mode != Mode::Flee && Engine::CountEnemiesAt(
            GameObjects::Player().Position(), 500.0f) == 0) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (!Burrowed && Fury() >= Slider(FarmMenu, "FuryForBite", 55) &&
            Engine::ValidEnemy(target, kERange + 80.0f)) (void)CastE(target, mode);
        else if (Burrowed && Engine::ValidEnemy(target, kQBurrowedRange)) (void)CastQ(target, mode);
        else (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (IncomingThreatUntil > Now() && !Burrowed) (void)CastW(target, mode, true);
        else if (IsMarked(target) && Lethal(target, RDamage(target))) (void)CastR(target, mode, true);
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
        if (slot < 0 || slot >= 4) return;
        if (!Engine::WasControllerCast(slot)) ManualOverrideUntil = now +
            Slider(TacticsMenu, "ManualOwnershipMs", 560);
        LastCastTick[slot] = now;
        if (slot == 2 && Burrowed && args.CastPosition.IsValid()) {
            PendingTunnelTick = now;
            PendingTunnelStart = args.Sender.Position;
            PendingTunnelEnd = args.CastPosition;
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatUntil = std::max(IncomingThreatUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
        if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
            IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender) && (Engine::TextContains(args.BuffName, "Burrow") ||
        Engine::TextContains(args.BuffName, "RekSaiW"))) Burrowed = true;
    if (!IsLocalPlayer(args.Sender) && (Engine::TextContains(args.BuffName, "PreySeeker") ||
        Engine::TextContains(args.BuffName, "RekSaiPrey"))) {
        MarkedTargetId = static_cast<int>(args.Sender.NetworkId);
        MarkExpireTick = Now() + 5000;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "Burrow")) Burrowed = false;
    if (!IsLocalPlayer(args.Sender) && static_cast<int>(args.Sender.NetworkId) == MarkedTargetId &&
        Engine::TextContains(args.BuffName, "Prey")) MarkedTargetId = MarkExpireTick = 0;
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !ControllerHelpers::ObjectEventIsAllied(args) ||
        (!Engine::TextContains(args.Sender.Name, "RekSaiTunnel") &&
         !Engine::TextContains(args.Sender.CharacterName, "RekSaiTunnel") &&
         !Engine::TextContains(args.Sender.Name, "Tunnel"))) return;
    for (auto& tunnel : Tunnels) {
        if (tunnel.NetworkId == 0) {
            const Vector3 end = args.Sender.Position;
            const Vector3 start = PendingTunnelTick > 0 ? PendingTunnelStart : end;
            RecordTunnel(tunnel, static_cast<int>(args.Sender.NetworkId), start, end, Now(), true);
            PendingTunnelTick = 0;
            return;
        }
    }
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.Sender.IsValid() ? static_cast<int>(args.Sender.NetworkId) : 0;
    for (auto& tunnel : Tunnels) if (id != 0 && tunnel.NetworkId == id) tunnel = {};
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), Burrowed ? kQBurrowedRange : kERange,
                        Burrowed ? 0xFF55CCFFu : 0xFFFF8844u, 1.5f, 40);
    if (Bool(CoachMenu, "DrawTunnels", true)) for (const auto& tunnel : Tunnels)
        if (TunnelActive(tunnel, Now())) Drawing::DrawLine(tunnel.Start, tunnel.End, 0xFF66DD88u, 2.0f);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("RekSaiOneTrick", "Rek'Sai posture and tunnel tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Queen's Wrath / Prey Seeker"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Burrow and unburrow"));
    WMenu->Add(new MenuSlider("UnburrowHP", "Unburrow pressure HP", 65, 10, 100));
    WMenu->Add(new MenuSlider("PassiveHealHP", "Hold burrow for passive heal below HP", 72, 10, 95));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Furious Bite / Tunnel"));
    EMenu->Add(new MenuSlider("MinimumFury", "Minimum fury for bite", 45, 0, 100));
    EMenu->Add(new MenuSlider("HarassFury", "Harass fury", 70, 0, 100));
    EMenu->Add(new MenuSlider("MaxTunnelEnemies", "Maximum enemies at tunnel endpoint", 2, 0, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Marked-target Void Rush"));
    RMenu->Add(new MenuSlider("TargetHP", "Nonlethal marked target HP", 48, 5, 90));
    RMenu->Add(new MenuSlider("MaxLandingEnemies", "Maximum enemies at R landing", 2, 0, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("RekSaiFarm", "Fury and farm policy"));
    FarmMenu->Add(new MenuSlider("FuryForBite", "Fury for farm bite", 55, 0, 100));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("RekSaiCoach", "Posture and tunnel coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw posture range", false));
    CoachMenu->Add(new MenuBool("DrawTunnels", "Draw active tunnels", true));
}
inline void OnLoad() {
    Burrowed = PassiveHealing = false; Tunnels = {};
    MarkedTargetId = MarkExpireTick = ManualOverrideUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    PendingTunnelTick = LastAutoTargetId = LastAutoTick = 0;
    PendingTunnelStart = PendingTunnelEnd = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Tunnels = {}; Burrowed = PassiveHealing = false;
}
inline constexpr const char* Scenarios[] = {
    "Pin spell and object decisions to Riot 26.15 / CommunityDragon 16.15",
    "Reconcile burrowed and unburrowed posture from buffs and polling",
    "Use burrowed Prey Seeker range, delay, width and prey mark expiry",
    "Use unburrowed Queen's Wrath as an attack-reset weave",
    "Use W knock-up only when the target is inside the observed radius",
    "Track allied tunnel objects with creation, deletion and expiry reconciliation",
    "Reject tunnel endpoints in walls, turrets, short self loops or outnumbered landings",
    "Preserve selected target before orbwalker and selector fallbacks",
    "Preserve AA windup unless reactive or lethal",
    "Hold burrowed passive healing when fury and low-health safety justify it",
    "Require fury threshold before ordinary Furious Bite",
    "Treat full-fury Furious Bite as true damage and lower-fury bite as physical",
    "Use E bite only in unburrowed melee range",
    "Use E tunnel only while burrowed and with safe cursor intent",
    "Require a live prey mark before Void Rush",
    "Reject Void Rush through walls, enemy turret landings or excessive landing pressure",
    "Reserve nonlethal Void Rush for configured low-health marked prey",
    "Use lethal bite and marked-target R as kill-secure branches",
    "Combo owns W knock-up, Q weave, fury bite and marked R sequence",
    "Harass uses ranged burrow Q and high-fury bite without unsolicited dives",
    "LaneClear and Jungle preserve farm policy while LastHit avoids reckless tunnels",
    "Flee burrows under threat and tunnels only toward a safe cursor endpoint",
    "Automatic mode defends against observed threat or secures lethal marked prey",
    "Yield after manually observed Q, W, E or R casts",
    "Never automate items, summoners or movement outside the tunnel spell",
    "Keep profile metadata, geometry and controller responsibilities separated",
};
inline constexpr ChampionController Controller = [] {
    ChampionController c{};
    c.ChampionId = SDK::ChampionId::RekSai;
    c.ControllerId = "champion.kuroaio.ai.reksai.onetrick";
    c.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    c.ResearchArtifact = "AI/Research/AIRekSai.md";
    c.ImplementationSummary = "Burrow posture and tunnel reconciliation with fury-aware bite, prey-marked Void Rush and conservative diver safety.";
    c.Scenarios = Scenarios;
    c.ScenarioCount = std::size(Scenarios);
    c.OwnsDecisionLoop = true;
    c.OnLoad = &OnLoad;
    c.OnUnload = &OnUnload;
    c.BuildMenu = &BuildMenu;
    c.OnUpdate = &OnUpdate;
    c.OnDraw = &OnDraw;
    c.OnProcessSpell = &OnProcessSpell;
    c.OnBuffAdd = &OnBuffAdd;
    c.OnBuffRemove = &OnBuffRemove;

    c.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    c.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    c.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    c.OnObjectCreate = &OnObjectCreate;
    c.OnObjectDelete = &OnObjectDelete;
    return c;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::RekSai
