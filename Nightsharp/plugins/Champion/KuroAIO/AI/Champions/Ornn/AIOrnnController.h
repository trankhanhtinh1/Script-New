#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIOrnnGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Ornn {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
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

inline PillarState Pillar{};
inline UltimateStage RStage = UltimateStage::Idle;
inline Vector3 RamPosition{};
inline Vector3 LastQAim{};
inline Vector3 LastEAim{};
inline int LastCastTick[4]{};
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int BrittleTargetId = 0;
inline int BrittleExpireTick = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 90) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::BonusAttackDamage;
using ControllerHelpers::AP;
inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target,
            QRawDamage(SpellRank(0), BonusAttackDamage())) : 0.0f;
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            WRawDamage(SpellRank(1), target.MaxHealth())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, ERawDamage(SpellRank(2),
            player.BonusArmor(), player.BonusSpellBlock())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            RRawDamagePerPass(SpellRank(3), AP())) : 0.0f;
}
using ControllerHelpers::Lethal;
inline bool TargetHasBrittle(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
        (target.HasBuff("OrnnBrittle") || target.HasBuff("OrnnBrittleDebuff") ||
         target.HasBuff("ornnbrittle"));
}
inline bool SafeEndpoint(const Vector3& endpoint, bool defensive = false) {
    if (!endpoint.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint))
        return false;
    if (!defensive && Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(GameObjects::Player().Position())) return false;
    return Engine::CountEnemiesAt(endpoint, 250.0f) <=
        Slider(EMenu, "MaxEndpointEnemies", 2);
}
inline Vector3 AimFor(const AIHeroClient& target, float delay) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, delay);
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.Hitchance >= SDK::HitChance::High &&
            prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero()) aim = prediction.GetCastPosition();
    }
    return aim;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode) || !Throttle(0) || Protected(target) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = AimFor(target, kQDelay);
    if (!aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > kQRange + target.BoundingRadius() ||
        SDK::NavMesh::IsWallBetween(player.Position(), aim, kQWidth * 0.5f) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastQAim = aim;
    LastCastTick[0] = Now();
    RecordPillar(Pillar, aim, LastCastTick[0]);
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) || Protected(target) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = Engine::ValidEnemy(target)
        ? PredictPosition(target, 0.25f) : player.Position() +
          SharedGeometry::Direction2D(player.Position(), Game::CursorPos()) *
              kWFinalLength;
    if (!aim.IsValid() || aim.IsZero() || !BreathHits(player.Position(), aim,
        target.Position(), target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    if (Engine::ValidEnemy(target)) {
        BrittleTargetId = static_cast<int>(target.NetworkId());
        BrittleExpireTick = Now() + 3000;
    }
    return true;
}
inline bool PillarCreatesImpact(const Vector3& origin, const Vector3& endpoint,
                                const AIHeroClient& target) {
    if (!PillarActive(Pillar, Now())) return false;
    const Vector3 direction = Direction2D(origin, endpoint);
    if (direction.IsZero()) return false;
    const Vector3 dashEnd = origin + direction * std::min(kERange,
        origin.Distance2D(endpoint));
    return SegmentHits(origin, dashEnd, Pillar.Position, kEDashRadius) &&
        WallImpactCanHit(origin, Pillar.Position, target.Position(),
                         target.BoundingRadius());
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2) || Protected(target) ||
        PreserveAttack(reactive)) return false;
    Vector3 aim = PredictPosition(target, 0.20f);
    if (!aim.IsValid() || aim.IsZero()) return false;
    const bool impact = PillarCreatesImpact(player.Position(), aim, target);
    if (!impact) return false;
    const DashContext context{true, SafeEndpoint(aim, reactive), impact,
        target.Position().Distance2D(Pillar.Position) <=
            kEShockwaveRadius + target.BoundingRadius(),
        !reactive && Engine::UnderEnemyTurret(aim) &&
            !Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(aim, 250.0f),
        Slider(EMenu, "MaxEndpointEnemies", 2), reactive, lethal};
    if (!ShouldSearingCharge(context)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastEAim = aim;
    LastCastTick[2] = Now();
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(3, mode) || !Throttle(3, 120) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, 0.55f);
    const Vector3 projectileOrigin = RStage == UltimateStage::RecastReady &&
        RamPosition.IsValid() && !RamPosition.IsZero() ? RamPosition : player.Position();
    if (!aim.IsValid() || aim.IsZero() ||
        projectileOrigin.Distance2D(aim) > kRRange + target.BoundingRadius() ||
        ControllerHelpers::ProjectileWallBlocks(projectileOrigin, aim, kRWidth * 0.5f))
        return false;
    int hitCount = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && SegmentHits(player.Position(), aim,
            PredictPosition(enemy, 0.55f), kRWidth, enemy.BoundingRadius())) ++hitCount;
    }
    const UltimateContext context{true, Engine::ValidEnemy(target), true, false,
        Orbwalker::IsWindingUp(), Lethal(target, RDamage(target)), reactive, manual,
        hitCount, Slider(RMenu, "MinimumTargets", 2)};
    if (RStage == UltimateStage::RecastReady) {
        if (!ShouldHeadbuttRam(context, true,
            !SharedGeometry::Direction2D(player.Position(), Game::CursorPos()).IsZero())) return false;
        if (!Engine::ControllerCastPosition(3, aim)) return false;
        LastCastTick[3] = Now();
        RStage = UltimateStage::Idle;
        return true;
    }
    if (!ShouldCallRam(context) || !Engine::ControllerCastPosition(3, aim)) return false;
    LastCastTick[3] = Now();
    RStage = UltimateStage::Calling;
    RamPosition = aim;
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, false, true)) return true;
    if (Lethal(target, WDamage(target)) && CastW(target, mode)) return true;
    if (Lethal(target, QDamage(target)) && CastQ(target, mode)) return true;
    return Lethal(target, RDamage(target)) && CastR(target, mode);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (TargetHasBrittle(target) && CastE(target, Mode::Combo, false,
        Lethal(target, EDamage(target)))) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 48)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastW(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastR(target, Mode::Flee, true);
}
inline void ReconcileState() {
    const int now = Now();
    if (Pillar.ExpireTick > 0 && now > Pillar.ExpireTick) Pillar = {};
    if (BrittleExpireTick <= now) BrittleTargetId = 0;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("OrnnRCharge") || player.HasBuff("OrnnR2")) RStage = UltimateStage::RecastReady;
    if (player.HasBuff("OrnnR")) RStage = UltimateStage::Calling;
    if (RStage != UltimateStage::Idle && !player.HasBuff("OrnnR") &&
        !player.HasBuff("OrnnRCharge") && !player.HasBuff("OrnnR2") &&
        now - LastCastTick[3] > 2500) RStage = UltimateStage::Idle;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f : kRRange);
    if (PlayerOverrideUntil > Now()) return true;
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
        if (AutomaticAllowed({IncomingThreatUntil > Now(),
            IncomingHardCCUntil > Now(), Engine::ValidEnemy(target) &&
            Lethal(target, RDamage(target)), false, PlayerOverrideUntil > Now()}))
            (void)CastR(target, mode, true);
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
        if (!Engine::WasControllerCast(slot)) PlayerOverrideUntil = now +
            Slider(TacticsMenu, "ManualOwnershipMs", 560);
        LastCastTick[slot] = now;
        if (slot == 3) {
            if (RStage == UltimateStage::Calling) RStage = UltimateStage::RecastReady;
            else RStage = UltimateStage::Calling;
        }
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
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "OrnnRCharge") ||
        Engine::TextContains(args.BuffName, "OrnnR2")) RStage = UltimateStage::RecastReady;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "OrnnR")) RStage = UltimateStage::Idle;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    const int id = static_cast<int>(args.Target.NetworkId());
    BrittleTargetId = id;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFCC8844u, 1.5f, 40);
    if (PillarActive(Pillar, Now())) Drawing::DrawCircle(Pillar.Position,
        kEShockwaveRadius, 0xFFFFAA55u, 1.5f, 36);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("OrnnOneTrick", "Ornn forge tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Rupture"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Brittle breath"));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Terrain charge"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Forge God"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum nonlethal targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("OrnnFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("OrnnCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and pillar zones", false));
}
inline void OnLoad() {
    Pillar = {};
    RStage = UltimateStage::Idle;
    RamPosition = LastQAim = LastEAim = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    PlayerOverrideUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    LastAutoTargetId = LastAutoTick = BrittleTargetId = BrittleExpireTick = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Pillar = {};
}
inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Track Ornn Q pillar creation and expiry with event and polling reconciliation",
    "Use live Q 800 range, fissure width, delay, slow and pillar duration",
    "Use live W breath width, final gout and max-health damage",
    "Apply Brittle duration and level-scaled max-health bonus damage",
    "Use E only when an observed pillar or terrain impact is reachable",
    "Reject E endpoints through walls, enemy turrets or excessive enemy count",
    "Use live E dash speed, shockwave radius and armor/MR scaling",
    "Track R call and recast stages from spell and buff events",
    "Use live 2500 R reach, projectile collision and recast direction",
    "Predict Ram contact before spending the ultimate",
    "Reserve nonlethal Ram for configured multi-target value",
    "Allow lethal, defensive and interrupt Ram casts",
    "Preserve AA windup unless brittle or reactive commitment justifies a cast",
    "Preserve selected target before orbwalker and selector fallback",
    "Combo builds Q/Brittle before safe E impact and Ram follow-up",
    "Harass preserves mana and never starts an unsolicited ultimate engage",
    "LaneClear Jungle and LastHit delegate to shared farm policy",
    "Flee uses W peel, safe E impact and manual-assist Ram",
    "Automatic mode permits defense, interrupt or kill secure only",
    "Yield after observed manual Q W E or R ownership",
    "Reject protected, invulnerable and spell-shielded targets",
    "Never automate items, summoner spells or movement ownership",
    "Keep profile metadata separate from the owned decision loop",
    "Draw ranges and pillar state without changing gameplay decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Ornn;
    controller.ControllerId = "champion.kuroaio.ai.ornn.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIOrnn.md";
    controller.ImplementationSummary =
        "Pillar-reconciled terrain charge, brittle sequencing, Ram stage tracking and conservative vanguard safety.";
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
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Ornn
