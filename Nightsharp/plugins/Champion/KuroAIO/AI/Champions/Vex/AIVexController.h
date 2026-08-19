#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIVexGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Vex {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
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

inline std::array<GloomMark, 12> GloomMarks = {};
inline std::array<int, 4> LastCastTick = {};
inline int RTargetId = 0;
inline int RHitTick = 0;
inline int RWindowExpireTick = 0;
inline int RProjectileTick = 0;
inline bool RProjectileHit = false;
inline bool RResetReady = false;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 65) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
inline GloomMark* FindMark(int id, bool create = false) {
    if (id == 0) return nullptr;
    GloomMark* free = nullptr;
    for (auto& mark : GloomMarks) {
        if (mark.TargetId == id) return &mark;
        if (!free && mark.TargetId == 0) free = &mark;
    }
    return create ? free : nullptr;
}
inline bool HasGloom(const AIHeroClient& target) {
    const auto* mark = FindMark(static_cast<int>(target.NetworkId()));
    return mark && mark->Confirmed &&
        GloomActive(*mark, static_cast<int>(target.NetworkId()), Now());
}
inline void SetGloom(int id, int tick, bool confirmed) {
    if (auto* mark = FindMark(id, true))
        ApplyGloom(*mark, id, tick, kGloomDurationMs, confirmed);
}
inline void ReconcileGloom() {
    const int now = Now();
    for (auto& mark : GloomMarks) {
        if (mark.TargetId != 0 && mark.ExpireTick <= now) mark = {};
    }
}
inline bool CastPosition(int slot, const Vector3& position, Mode mode,
                         bool reactive = false, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(slot, mode) || !Throttle(slot) ||
        PreserveAttack(reactive, lethal) || !position.IsValid() || position.IsZero())
        return false;
    if (!Engine::ControllerCastPosition(slot, position)) return false;
    LastCastTick[slot] = Now();
    return true;
}
inline Vector3 Aim(const AIHeroClient& target, int slot, float fallbackDelay) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, fallbackDelay);
    if (slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot]) {
        const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
        if (static_cast<int>(prediction.Hitchance) >=
                static_cast<int>(SDK::HitChance::High) &&
            prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero()) {
            aim = prediction.GetCastPosition();
        }
    }
    return aim;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) ||
        player.Position().Distance2D(target.Position()) > kQRange + target.BoundingRadius())
        return false;
    const Vector3 aim = Aim(target, 0, kQDelay);
    if (!SegmentHits(player.Position(), aim, PredictPosition(target, kQDelay),
                     kQWidth, target.BoundingRadius())) return false;
    if (!CastPosition(0, aim, mode, reactive, lethal)) return false;
    SetGloom(static_cast<int>(target.NetworkId()),
             Now() + static_cast<int>(ProjectileTravelSeconds(
                 player.Position(), aim, kQDelay, kQSpeed, kQRange) * 1000.0f), false);
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) ||
        player.Position().Distance2D(target.Position()) >
            kWRange + target.BoundingRadius()) return false;
    const bool fear = HasGloom(target);
    if (!fear && !reactive && !lethal &&
        Engine::CountEnemiesAt(player.Position(), kWRadius) <
            Slider(WMenu, "MinimumEnemies", 1)) return false;
    if (!Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive, lethal) ||
        !Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    if (fear) {
        if (auto* mark = FindMark(static_cast<int>(target.NetworkId())))
            (void)ConsumeGloom(*mark, static_cast<int>(target.NetworkId()), Now());
    }
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) ||
        player.Position().Distance2D(target.Position()) > kEDistance + 250.0f)
        return false;
    const Vector3 aim = Aim(target, 2, kEResolveSeconds);
    if (!CircleHits(aim, PredictPosition(target, kEResolveSeconds), kERadius,
                    target.BoundingRadius())) return false;
    if (!CastPosition(2, aim, mode, reactive, lethal)) return false;
    SetGloom(static_cast<int>(target.NetworkId()), Now(), false);
    return true;
}
inline bool ShadowLandingSafe(const Vector3& endpoint, const AIHeroClient& target,
                              bool defensive, bool lethal, bool manual) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return false;
    const ShadowLandingContext context{
        (HasGloom(target) || RResetReady), endpoint.IsValid() && !endpoint.IsZero(),
        SDK::NavMesh::IsWall(endpoint), Engine::UnderEnemyTurret(endpoint),
        Engine::UnderEnemyTurret(player.Position()),
        HasReadyPointClickThreatAt(endpoint), HasReadyDashHazardAt(endpoint),
        lethal, defensive, manual, Engine::CountEnemiesAt(endpoint, 650.0f),
        Slider(RMenu, "MaxLandingEnemies", 2)};
    return SafeShadowLanding(context);
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false, bool manual = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Engine::ValidEnemy(target)) return false;
    const int targetId = static_cast<int>(target.NetworkId());
    const bool recast = RProjectileHit && (RTargetId == targetId || RResetReady);
    if (recast) {
        const Vector3 endpoint = ClampShadowLanding(player.Position(), target.Position());
        if (!ShadowLandingSafe(endpoint, target, defensive, lethal, manual) ||
            !CastPosition(3, endpoint, mode, reactive, lethal)) return false;
        RProjectileHit = false;
        RResetReady = false;
        RTargetId = targetId;
        RWindowExpireTick = Now() + kShadowResetWindowMs;
        return true;
    }
    if (!Ready(3, mode) || !Throttle(3, 120) ||
        player.Position().Distance2D(target.Position()) > kRRange + target.BoundingRadius())
        return false;
    const Vector3 aim = Aim(target, 3, kRDelay);
    if (!SegmentHits(player.Position(), aim, PredictPosition(target, kRDelay),
                     kRWidth, target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kRWidth * 0.5f))
        return false;
    if (!CastPosition(3, aim, mode, reactive, lethal)) return false;
    RTargetId = targetId;
    RProjectileTick = Now();
    RWindowExpireTick = Now() + kShadowResetWindowMs;
    RProjectileHit = false;
    RResetReady = false;
    SetGloom(targetId, Now(), false);
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastW(target, mode, true, true)) return true;
    if (CastQ(target, mode, true, true)) return true;
    if (CastE(target, mode, true, true)) return true;
    return CastR(target, mode, true, false, false, true);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    const bool marked = HasGloom(target);
    if (marked && CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (RProjectileHit && CastR(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "HarassMana", 45)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastW(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastR(target, Mode::Flee, true, true, true);
}
inline void ReconcileState() {
    ReconcileGloom();
    const int now = Now();
    if (RProjectileHit && now > RWindowExpireTick) {
        RProjectileHit = false;
        RResetReady = false;
        RTargetId = 0;
    }
    if (RProjectileHit && RTargetId != 0) {
        const auto target = Engine::EnemyByNetworkId(RTargetId);
        if (!Engine::ValidEnemy(target)) {
            RResetReady = true;
            RTargetId = 0;
        }
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("VexR") || player.HasBuff("VexRRecast")) RProjectileHit = true;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f : kRRange);
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target) &&
        CastW(target, mode, true)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "Mana", 35))
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(target))
            (void)CastW(target, mode, true);
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
        if (slot < 0 || slot > 3) return;
        LastCastTick[slot] = now;
        if (!Engine::WasControllerCast(slot))
            ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
        IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick,
                                       analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == 0) return;
    if (Engine::TextContains(args.BuffName, "VexGloom") ||
        Engine::TextContains(args.BuffName, "vexgloom") ||
        Engine::TextContains(args.BuffName, "VexGloomMarker")) {
        if (id == RTargetId && RProjectileTick > 0 &&
            Now() - RProjectileTick <= 3000) {
            RProjectileHit = true;
            RWindowExpireTick = Now() + kShadowResetWindowMs;
        }
        SetGloom(id, Now(), true);
    }
    if (IsLocalPlayer(args.Sender) &&
        (Engine::TextContains(args.BuffName, "VexR") ||
         Engine::TextContains(args.BuffName, "VexRRecast"))) {
        RProjectileHit = true;
        RWindowExpireTick = Now() + kShadowResetWindowMs;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == 0) return;
    if (Engine::TextContains(args.BuffName, "VexGloom") ||
        Engine::TextContains(args.BuffName, "vexgloom") ||
        Engine::TextContains(args.BuffName, "VexGloomMarker")) {
        if (auto* mark = FindMark(id)) *mark = {};
    }
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "VexR")) {
        RProjectileHit = false;
        RResetReady = false;
    }
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF8D6CFFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kWRadius, 0xFFB49BFFu, 1.0f, 32);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("VexOneTrick", "Vex gloom tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 10, 90));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Mistral Bolt"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Personal Space"));
    WMenu->Add(new MenuSlider("MinimumEnemies", "Minimum W enemies", 1, 1, 3));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Looming Darkness"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Shadow Surge"));
    RMenu->Add(new MenuSlider("MaxLandingEnemies", "Maximum landing enemies", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("VexFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("VexCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and W ranges", false));
}
inline void OnLoad() {
    GloomMarks = {};
    LastCastTick = {};
    RTargetId = RHitTick = RWindowExpireTick = RProjectileTick = 0;
    RProjectileHit = RResetReady = false;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    LastAutoTargetId = LastAutoTick = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    GloomMarks = {};
}
inline constexpr const char* Scenarios[] = {
    "Pin mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Track Gloom mark add, expiry and consumption from buffs plus polling",
    "Use Q Mistral Bolt projectile width, range, speed and wall collision",
    "Use W Personal Space as shield, fear consumer and anti-gapcloser",
    "Place E Looming Darkness from predicted target position and apply Gloom",
    "Track R Shadow Surge projectile hit, recast window and target identity",
    "Allow R reset retargeting only after an observed target takedown",
    "Reject Shadow landing through walls, point-click threats and dash hazards",
    "Reject aggressive landings under a new enemy turret unless lethal",
    "Limit landing enemy count and preserve defensive or manual exceptions",
    "Interrupt committed enemy casts with fear-enabled W when observable",
    "Reconcile cooldown and resource readiness from runtime spell telemetry",
    "Preserve selected target before orbwalker and selector fallback",
    "Preserve AA windup except reactive or lethal casts",
    "Yield to manual Q W E and R ownership before controller decisions",
    "Combo layers E and Q before marked W fear and safe R recast",
    "Harass uses Q/E with a configurable mana reserve",
    "LaneClear Jungle and LastHit delegate to shared farm policy",
    "Flee uses W/E peel and manual-assist Shadow escape",
    "Automatic mode permits defensive interruption and no fresh engage",
    "Reject protected, invulnerable and spell-shielded targets",
    "Never automate items, summoner spells or movement ownership",
    "Draw range and mark state without changing gameplay decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Vex;
    controller.ControllerId = "champion.kuroaio.ai.vex.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIVex.md";
    controller.ImplementationSummary =
        "Gloom mark/fear sequencing, projectile safety, Shadow reset retargeting and conservative landing reconciliation.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Vex
