#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AITalonGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Talon {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::ChampionIs;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::TotalAttackDamage;
using ControllerHelpers::Now;
using ControllerHelpers::Ready;

inline Menu* TacticsMenu = nullptr;
inline Menu* BladeMenu = nullptr;
inline Menu* MobilityMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline PassiveState Passive{};
inline TerrainState Terrain{};
inline RState ShadowAssault{};
inline int WCastTick = 0;
inline int QCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int IncomingHardCCUntil = 0;
inline int PlayerOverrideUntil = 0;
inline int WTargetId = 0;
inline int RTargetId = 0;
inline bool WReturning = false;
inline Vector3 LastWOutbound = {};
inline Vector3 LastWReturn = {};
inline Vector3 LastEEndpoint = {};
inline Vector3 LastREndpoint = {};
inline bool QWasManual = false;
inline bool WWasManual = false;
inline bool EWasManual = false;
inline bool RWasManual = false;

inline bool Throttle(int slot, int delay) {
    const int tick = slot == 0 ? QCastTick : slot == 1 ? WCastTick : slot == 2 ? ECastTick : RCastTick;
    return Now() - tick >= delay;
}

inline bool TargetCannotBeDamaged(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BlackShield") ||
           target.HasBuff("BansheesVeil") || target.HasBuff("EdgeOfNight") ||
           target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("KayleR") || target.HasBuff("kindredrnodeathbuff") ||
           target.HasBuff("ChronoShift");
}


inline int SpellRank(int slot) {
    if (slot < 0 || slot >= 4 || !Engine::RuntimeSpells[slot]) return 1;
    return std::clamp(Engine::RuntimeSpells[slot]->Level(), 1, slot == 3 ? 3 : 5);
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return QRawDamage(SpellRank(0), TotalAttackDamage(),
                      IsMeleeQ(player.Position(), target.Position()));
}

inline float WDamage(const AIHeroClient& target, bool returning = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return WRawDamage(SpellRank(1), player.BonusAttackDamage(), returning);
}

inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? ERawDamage(SpellRank(2), player.BonusAttackDamage()) : 0.0f;
}

inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? RRawDamage(SpellRank(3), player.BonusAttackDamage()) : 0.0f;
}

using ControllerHelpers::Lethal;

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (Passive.TargetId != 0 && now >= Passive.ExpireTick) Passive = {};
    if (Terrain.Traversing && now >= Terrain.ExpireTick) Terrain = {};
    if (ShadowAssault.Active && now >= ShadowAssault.ExpireTick) ShadowAssault = {};

    for (const char* token : {"TalonPassiveStack", "TalonPassiveBleed", "TalonPassiveDamage"}) {
        if (player.HasBuff(token) && Passive.TargetId != 0) {
            Passive.Stacks = std::max(Passive.Stacks, 3);
            Passive.ExpireTick = std::max(Passive.ExpireTick, now + kPassiveDurationMs);
        }
    }
    if (player.HasBuff("TalonRStealth") || player.HasBuff("TalonRInvis")) {
        ShadowAssault.Active = true;
        ShadowAssault.Stealthed = true;
        ShadowAssault.ExpireTick = std::max(ShadowAssault.ExpireTick, now + kRStealthMs);
    }
    if (WCastTick > 0 && now - WCastTick >= 260) {
        WReturning = true;
        WTargetId = WTargetId == 0 ? LastAutoTargetId : WTargetId;
        if (now - WCastTick >= kWReturnWindowMs) LastWReturn = {};
    }
}

inline AIHeroClient ChooseTarget(const AIHeroClient& selected, float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    return Engine::SelectTarget(range);
}

inline bool SafeEndpoint(const Vector3& endpoint, const AIHeroClient& target, bool lethal, bool fleeing) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int maxEnemies = Slider(MobilityMenu, "MaxEndpointEnemies", 2);
    const int enemies = Engine::CountEnemiesAt(endpoint, 625.0f);
    const bool exitAvailable = fleeing || enemies <= maxEnemies || Engine::CountAlliesAt(endpoint, 700.0f) > 0;
    return REndpointSafe(endpoint, endpoint.IsValid() && !endpoint.IsZero(), SDK::NavMesh::IsWall(endpoint),
                         Engine::UnderEnemyTurret(endpoint) && !Engine::UnderEnemyTurret(player.Position()),
                         lethal, exitAvailable, enemies, maxEnemies) &&
           (lethal || target.IsValid() || fleeing);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 35.0f) || !Ready(0, mode) ||
        !Throttle(0, 42) || TargetCannotBeDamaged(target)) return false;
    const bool melee = IsMeleeQ(player.Position(), target.Position());
    const bool lethal = Lethal(target, QDamage(target) +
        (PassiveReady(Passive, static_cast<int>(target.NetworkId()), Now()) ?
         PassiveRawDamage(player.Level(), TotalAttackDamage()) : 0.0f));
    if (Orbwalker::IsWindingUp() && Bool(TacticsMenu, "PreserveAttacks", true) && !lethal && !defensive) return false;
    const ModeContext context{ true, true, Orbwalker::IsWindingUp(), lethal, false };
    if (!MayUseAbility(context) && !defensive) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    QCastTick = Now();
    QWasManual = false;
    Passive = AddPassiveStack(Passive, static_cast<int>(target.NetworkId()), Now());
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWOutboundRange + 35.0f) || !Ready(1, mode) ||
        !Throttle(1, 65) || TargetCannotBeDamaged(target)) return false;
    const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
    const Vector3 endpoint = prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, 0.25f);
    if (endpoint.IsZero() || player.Position().Distance2D(endpoint) > kWOutboundRange + target.BoundingRadius() ||
        prediction.Hitchance < SDK::HitChance::High ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kWWidth * 0.5f)) return false;
    const bool lethal = Lethal(target, WDamage(target, false) + WDamage(target, true));
    if (Orbwalker::IsWindingUp() && Bool(TacticsMenu, "PreserveAttacks", true) && !lethal && !defensive) return false;
    if (!MayUseAbility({ true, true, Orbwalker::IsWindingUp(), lethal, false }) && !defensive) return false;
    if (!Engine::ControllerCastPosition(1, endpoint)) return false;
    WCastTick = Now();
    WTargetId = static_cast<int>(target.NetworkId());
    LastWOutbound = endpoint;
    WReturning = false;
    LastWReturn = WReturnEndpoint(player.Position(), endpoint, player.Position());
    WWasManual = false;
    Passive = AddPassiveStack(Passive, WTargetId, Now());
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2, 80)) return false;
    const Vector3 cursor = Game::CursorPos();
    const Vector3 toward = target.IsValid() ? PredictPosition(target, 0.20f) : cursor;
    const Vector3 direction = Direction2D(player.Position(), toward.IsZero() ? cursor : toward);
    if (direction.IsZero()) return false;
    Vector3 endpoint = player.Position() + direction * std::min(kERange, player.Position().Distance2D(toward));
    if (endpoint.IsZero() || !SDK::NavMesh::IsWall(endpoint)) {
        for (float distance = 120.0f; distance <= kERange; distance += 80.0f) {
            const Vector3 sample = player.Position() + direction * distance;
            if (SDK::NavMesh::IsWall(sample)) { endpoint = sample; break; }
        }
    }
    if (endpoint.IsZero() || !SDK::NavMesh::IsWall(endpoint)) return false;
    const bool lethal = target.IsValid() && Lethal(target, QDamage(target));
    if (!SafeEndpoint(endpoint, target, lethal, fleeing)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    Terrain = { true, false, player.Position(), endpoint, Now(), Now() + 1200 };
    ECastTick = Now();
    LastEEndpoint = endpoint;
    EWasManual = false;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!ShadowAssault.Active && !Ready(3, mode)) return false;
    if (ShadowAssault.Active) {
        const bool lethal = Engine::ValidEnemy(target) && Lethal(target, RDamage(target) + WDamage(target, true));
        const Vector3 endpoint = target.IsValid() ? PredictPosition(target, 0.20f) : player.Position();
        const bool lineHit = target.IsValid() && RLineHits(player.Position(), endpoint, endpoint, target.BoundingRadius());
        if (!RReturnAllowed(ShadowAssault, Now(), SafeEndpoint(player.Position(), target, lethal, mode == Mode::Flee), lethal, mode == Mode::Flee) &&
            !defensive && !lineHit) return false;
        if (!Engine::ControllerCastSelf(3)) return false;
        ShadowAssault.Returning = true;
        ShadowAssault.Active = false;
        RCastTick = Now();
        RWasManual = false;
        LastREndpoint = endpoint;
        return true;
    }
    if (!Engine::ValidEnemy(target, kRRange + 80.0f) || TargetCannotBeDamaged(target)) return false;
    const bool lethal = Lethal(target, RDamage(target) + WDamage(target, true));
    if (Engine::CountEnemiesAt(player.Position(), 625.0f) > Slider(UltimateMenu, "MaxCommitEnemies", 2) && !lethal && !defensive) return false;
    if (!MayUseAbility({ true, true, Orbwalker::IsWindingUp(), lethal, false }) && !defensive) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RCastTick = Now();
    RTargetId = static_cast<int>(target.NetworkId());
    ShadowAssault = { true, true, false, player.Position(), {}, Now(), Now() + kRStealthMs + kRReturnWindowMs };
    RWasManual = false;
    Passive = AddPassiveStack(Passive, RTargetId, Now());
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, QDamage(target)) && CastQ(target, mode, true)) return true;
    if (Lethal(target, WDamage(target, true)) && CastW(target, mode, true)) return true;
    if (Lethal(target, RDamage(target) + WDamage(target, true)) && CastR(target, mode)) return true;
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastW(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    if (!Terrain.Traversing && GameObjects::Player().Position().Distance2D(target.Position()) > 475.0f && CastE(target, Mode::Combo)) return true;
    if (CastR(target, Mode::Combo)) return true;
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastW(target, Mode::Harass)) return true;
    return CastQ(target, Mode::Harass);
}

inline bool TryFarm(Mode mode) {
    if (CurrentResource() < SpellCost(0) + Slider(FarmMenu, "ManaReserve", 65)) return false;
    return Engine::TryFarm(mode);
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (ShadowAssault.Active && CastR(threat, Mode::Flee, true)) return true;
    if (CastE(threat, Mode::Flee)) return true;
    return Engine::ValidEnemy(threat) && CastR(threat, Mode::Flee, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (PlayerOverrideUntil > Now()) return true;
    const AIHeroClient target = ChooseTarget(selected, kWOutboundRange + 80.0f);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 1000.0f);
    if (mode == Mode::Flee) { (void)TryFlee(threat); return true; }
    if (Engine::ValidEnemy(target) && TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && (IncomingHardCCUntil > Now() ||
            Lethal(target, QDamage(target) + WDamage(target, true)))) (void)TryKillSecure(target, Mode::Automatic);
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(args, 220.0f, 110.0f, 250, 280, 260, 1500, 450);
        if (threat.Valid && threat.CrossesPlayer && threat.LikelyHardCrowdControl) IncomingHardCCUntil = now + 650;
        return;
    }
    const bool owned = args.Slot >= 0 && args.Slot < 4 && Engine::WasControllerCast(args.Slot);
    if (!owned) PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 520);
    if (args.Slot == 0) { QCastTick = now; QWasManual = !owned; }
    else if (args.Slot == 1) {
        WCastTick = now; WWasManual = !owned;
        WReturning = Engine::TextContains(args.SpellName, "Return") ||
                     Engine::TextContains(args.SpellName, "TalonW2");
    }
    else if (args.Slot == 2) {
        ECastTick = now; EWasManual = !owned;
        Terrain.Traversing = true; Terrain.StartTick = now; Terrain.ExpireTick = now + 1200;
        Terrain.Start = player.Position(); Terrain.End = args.EndPosition;
    } else if (args.Slot == 3) {
        RCastTick = now; RWasManual = !owned;
        const bool returnCast = Engine::TextContains(args.SpellName, "Return") || ShadowAssault.Active;
        if (returnCast) { ShadowAssault.Returning = true; ShadowAssault.Active = false; }
        else ShadowAssault = { true, true, false, player.Position(), {}, now, now + kRStealthMs + kRReturnWindowMs };
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = Now();
    const auto markedEnemy = HeroByNetworkId(static_cast<int>(args.Sender.NetworkId));
    if (Engine::ValidEnemy(markedEnemy) &&
        (Engine::TextContains(args.BuffName, "TalonPassiveStack") ||
         Engine::TextContains(args.BuffName, "TalonPassiveMark") ||
         Engine::TextContains(args.BuffName, "TalonPassiveBleed"))) {
        Passive = AddPassiveStack(Passive, static_cast<int>(markedEnemy.NetworkId()), now);
        if (Engine::TextContains(args.BuffName, "Bleed") ||
            Engine::TextContains(args.BuffName, "Damage")) Passive.Stacks = 3;
        return;
    }
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "TalonRStealth") || Engine::TextContains(args.BuffName, "TalonRInvis")) {
        ShadowAssault.Active = true; ShadowAssault.Stealthed = true; ShadowAssault.ExpireTick = now + kRStealthMs + kRReturnWindowMs;
    } else if (Engine::TextContains(args.BuffName, "TalonE")) {
        Terrain.Traversing = true; Terrain.StartTick = now; Terrain.ExpireTick = now + 1200;
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    const auto markedEnemy = HeroByNetworkId(static_cast<int>(args.Sender.NetworkId));
    if (Engine::ValidEnemy(markedEnemy) &&
        (Engine::TextContains(args.BuffName, "TalonPassiveStack") ||
         Engine::TextContains(args.BuffName, "TalonPassiveMark"))) {
        if (Passive.TargetId == static_cast<int>(markedEnemy.NetworkId())) {
            Passive.ExpireTick = std::min(Passive.ExpireTick, Now() + 250);
        }
        return;
    }
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "TalonRStealth") || Engine::TextContains(args.BuffName, "TalonRInvis")) ShadowAssault.Stealthed = false;
    if (Engine::TextContains(args.BuffName, "TalonE")) Terrain = {};
}


inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || !ShadowAssault.Active) return;
    const auto target = HeroByNetworkId(static_cast<int>(args.Target.NetworkId()));
    if (Engine::ValidEnemy(target) && Orbwalker::IsWindingUp() && !Lethal(target, RDamage(target) + WDamage(target, true))) args.Process = true;
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), kWOutboundRange, 0xFF5B4BFFu, 1.8f, 40);
    Drawing::DrawCircle(player.Position(), kRRange, ShadowAssault.Active ? 0xFFE5B7FFu : 0xFF8D72FFu, 1.5f, 40);
    if (!LastEEndpoint.IsZero()) Drawing::DrawLine(player.Position(), LastEEndpoint, 0xFFB2A0FFu, 2.0f);
    if (!LastWOutbound.IsZero()) Drawing::DrawLine(player.Position(), LastWOutbound, 0xFF5B4BFFu, 1.5f);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("TalonOneTrick", "Talon blade mechanics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 520, 180, 1100));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve AA windup unless lethal", true));
    BladeMenu = TacticsMenu->AddSubMenu(new Menu("BladeReturn", "W/R blade tracking"));
    BladeMenu->Add(new MenuBool("ReturnForLethal", "Return blades for lethal damage", true));
    MobilityMenu = TacticsMenu->AddSubMenu(new Menu("TerrainMobility", "E endpoint safety"));
    MobilityMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at endpoint", 2, 1, 5));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu("ShadowAssault", "R safety"));
    UltimateMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies for R commit", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("TalonFarm", "Conservative farming"));
    FarmMenu->Add(new MenuSlider("ManaReserve", "Mana reserve", 65, 0, 180));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("TalonCoach", "Route visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/E/R ranges", false));
}

inline void OnLoad() {
    Passive = {}; Terrain = {}; ShadowAssault = {};
    WCastTick = QCastTick = ECastTick = RCastTick = 0;
    LastAutoTargetId = LastAutoTick = WTargetId = RTargetId = 0;
    IncomingHardCCUntil = PlayerOverrideUntil = 0;
    LastWOutbound = LastWReturn = LastEEndpoint = LastREndpoint = {};
    QWasManual = WWasManual = EWasManual = RWasManual = false;
    WReturning = false;
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = BladeMenu = MobilityMenu = UltimateMenu = FarmMenu = CoachMenu = nullptr;
    Passive = {}; Terrain = {}; ShadowAssault = {};
}

inline constexpr const char* Scenarios[] = {
    "Use Riot 26.15 and CommunityDragon 16.15 Summoner's Rift values",
    "Track Talon passive three-stack target identity and six-second expiry",
    "Reconcile passive stacks from Talon passive buff events and polling",
    "Apply passive bleed only after the third distinct hit stack",
    "Use melee Q at 170 radius and ranged Q through the 575 range boundary",
    "Preserve Q auto-reset timing and AA windup unless the cast is lethal",
    "Predict W outbound cast with 650 range, 75 width and projectile-wall rejection",
    "Track W outbound and return blade positions and return damage separately",
    "Credit W return only during its observed return window",
    "Do not invent a W recast; blade return is game-driven and event-reconciled",
    "Use E only against an observed terrain endpoint and retain traversal state",
    "Reject E walls through invalid, blocked, turret-dangerous or overcommitted endpoints",
    "Track E traversal start/end and expire it through events or polling",
    "Start R stealth only after owned or observed Shadow Assault cast",
    "Track R stealth, blade envelope, target identity and return window",
    "Return R blades only when lethal, fleeing, or the current endpoint is unsafe",
    "Reject R commit under excessive enemy count unless lethal or defensive",
    "Reject target damage through invulnerability, spell shields and stasis buffs",
    "Respect selected target before orbwalker target fallback",
    "Use hard crowd-control threat windows for Automatic defensive decisions",
    "Manual Q/W/E/R casts suspend automation briefly without clearing state",
    "Combo sequences W, Q, safe E traversal and R only with real target safety",
    "Harass uses W and Q without unsolicited terrain commit",
    "LaneClear, Jungle and LastHit delegate only through shared farm path",
    "Flee returns active R before choosing a safe terrain traversal",
    "Automatic mode executes only verified lethal or hard-CC reactions",
    "Do not automate Flash, Ignite, Smite or item actives",
    "Keep profile metadata separate from the owned decision loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Talon;
    controller.ControllerId = "champion.kuroaio.ai.talon.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AITalon.md";
    controller.ImplementationSummary = "Passive stack tracking, W blade phases, Q range-aware reset, terrain traversal safety and R stealth/return endpoint planning.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardLocalActiveBuffEvent<&OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Talon
