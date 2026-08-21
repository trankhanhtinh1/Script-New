#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIKassadinGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Kassadin {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Protected;
using ControllerHelpers::Lethal;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::CurrentResource;

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
inline int ForcePulseCharges = 0;
inline int RiftwalkStacks = 0;
inline int RiftwalkLastCastTick = 0;
inline int NetherBladeArmedUntil = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 70) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool HasManaFor(int slot, float reservePercent = 0.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    return CurrentResource() + 0.5f >= SpellCost(slot) +
        player.MaxMana() * std::clamp(reservePercent, 0.0f, 100.0f) / 100.0f;
}
inline void ReconcileState() {
    const int now = Now();
    if (RiftwalkStacks > 0 && !RiftwalkStacksActive(RiftwalkStacks, RiftwalkLastCastTick, now)) {
        RiftwalkStacks = 0;
        RiftwalkLastCastTick = 0;
    }
    if (NetherBladeArmedUntil <= now) NetherBladeArmedUntil = 0;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("ForcePulseAvailable") || player.HasBuff("forcepulseavailable"))
        ForcePulseCharges = kForcePulseRequiredCharges;
    if (player.HasBuff("NetherBladeBuff") || player.HasBuff("netherbladebuff"))
        NetherBladeArmedUntil = std::max(NetherBladeArmedUntil, now + 650);
}
inline bool SafeEndpoint(const Vec3& endpoint, bool defensive, bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int enemies = Engine::CountEnemiesAt(endpoint, 280.0f);
    BlinkSafetyContext context{
        endpoint.IsValid() && !endpoint.IsZero(),
        endpoint.IsValid() && SDK::NavMesh::IsWall(endpoint),
        endpoint.IsValid() && Engine::UnderEnemyTurret(endpoint),
        Engine::UnderEnemyTurret(player.Position()), lethal, defensive,
        enemies, Slider(RMenu, "MaximumEnemies", 2), RiftwalkStacks,
        Slider(RMenu, "ReserveStacks", 1)};
    return SafeBlinkEndpoint(context);
}
inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, QRawDamage(SpellRank(0), player.AP())) : 0.0f;
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, WActiveDamage(SpellRank(1), player.AP()) +
            WOnHitDamage(SpellRank(1), player.AP())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, ERawDamage(SpellRank(2), player.AP())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target, int stacks = RiftwalkStacks) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, RiftwalkDamage(SpellRank(3), player.AP(),
            player.MaxMana(), stacks)) : 0.0f;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive, lethal) || !HasManaFor(0, reactive ? 0.0f : 12.0f)) return false;
    const Vector3 aim = PredictPosition(target, kQDelay);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) >
        kQRange + target.BoundingRadius() || ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 30.0f)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    if (prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero() &&
        static_cast<int>(prediction.Hitchance) >= static_cast<int>(SDK::HitChance::High)) {
        if (player.Position().Distance2D(prediction.GetCastPosition()) <= kQRange + target.BoundingRadius()) {
            if (!Engine::ControllerCastPosition(0, prediction.GetCastPosition())) return false;
        } else return false;
    } else if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || Protected(target) ||
        !Ready(1, mode) || !Throttle(1) || PreserveAttack(reactive, lethal) ||
        !HasManaFor(1, 8.0f) || !ControllerHelpers::InAutoAttackRange(target, 12.0f)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    NetherBladeArmedUntil = Now() + 650;
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(2, mode) || !Throttle(2) ||
        PreserveAttack(reactive, lethal) || !HasForcePulseCharges(ForcePulseCharges) ||
        !HasManaFor(2, 15.0f)) return false;
    const Vector3 aim = PredictPosition(target, kEDelay);
    if (!ConeHits(player.Position(), aim, PredictPosition(target, kEDelay), kERange,
                  kEConeAngle, target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 30.0f)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    ForcePulseCharges = ConsumeForcePulseCharges(ForcePulseCharges);
    LastCastTick[2] = Now();
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 110) ||
        PlayerMobilityLocked() || PreserveAttack(reactive, lethal) ||
        !HasManaFor(3, defensive ? 0.0f : 18.0f)) return false;
    const bool validTarget = Engine::ValidEnemy(target) && !Protected(target);
    const Vector3 requested = defensive && validTarget
        ? player.Position() + Direction2D(target.Position(), player.Position()) * kRRange
        : validTarget ? PredictPosition(target, kRDelay) : player.Position() + Direction2D(
            player.Position(), player.Position() + Vector3{1.0f, 0.0f, 0.0f}) * kRRange;
    const Vector3 endpoint = ClampBlinkEndpoint(player.Position(), requested);
    if (!SafeEndpoint(endpoint, defensive, lethal) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, 40.0f)) return false;
    const float requiredMana = RiftwalkCost(player.MaxMana(), RiftwalkStacks);
    if (CurrentResource() + 0.5f < requiredMana + (lethal || defensive ? 0.0f :
        player.MaxMana() * Slider(RMenu, "ManaReserve", 20) / 100.0f)) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    LastCastTick[3] = Now();
    RiftwalkStacks = NextRiftwalkStacks(RiftwalkStacks);
    RiftwalkLastCastTick = Now();
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target) || Protected(target)) return false;
    if (Lethal(target, RDamage(target)) && CastR(target, mode, true, false, true)) return true;
    if (Lethal(target, WDamage(target)) && CastW(target, mode, true, true)) return true;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, true, true)) return true;
    return Lethal(target, QDamage(target)) && CastQ(target, mode, true, true);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (HasForcePulseCharges(ForcePulseCharges) && CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo, false, false, false);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || PlayerManaPercent() < Slider(TacticsMenu, "HarassMana", 55)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (HasForcePulseCharges(ForcePulseCharges)) (void)CastE(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target)) (void)CastQ(target, Mode::Flee, true, false);
    if (Engine::ValidEnemy(target)) (void)CastR(target, Mode::Flee, true, true, false);
}
inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    LastMode = mode;
    ReconcileState();
    const AIHeroClient target = Engine::SelectTarget(
        mode == Mode::Flee ? 900.0f : kRRange);
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target) &&
        CastQ(target, mode, true, false)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 900.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (PlayerManaPercent() >= Slider(FarmMenu, "Mana", 40)) (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(target)) (void)CastQ(target, mode, true, false);
        break;
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    const int slot = static_cast<int>(args.Slot);
    if (slot < 0 || slot > 3) return;
    ForcePulseCharges = AddForcePulseCast(ForcePulseCharges);
    if (IsLocalPlayer(args.Sender)) {
        LastCastTick[slot] = now;
        if (slot == 2) ForcePulseCharges = 0;
        if (slot == 3) {
            RiftwalkStacks = NextRiftwalkStacks(RiftwalkStacks);
            RiftwalkLastCastTick = now;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(IncomingHardCCUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    const auto name = args.BuffName;
    if (Engine::TextContains(name, "ForcePulseAvailable") || Engine::TextContains(name, "forcepulseavailable"))
        ForcePulseCharges = kForcePulseRequiredCharges;
    if (Engine::TextContains(name, "NetherBladeBuff") || Engine::TextContains(name, "netherbladebuff"))
        NetherBladeArmedUntil = Now() + 650;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "ForcePulseAvailable") || Engine::TextContains(args.BuffName, "forcepulseavailable"))
        ForcePulseCharges = 0;
    if (Engine::TextContains(args.BuffName, "NetherBladeBuff") || Engine::TextContains(args.BuffName, "netherbladebuff"))
        NetherBladeArmedUntil = 0;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || (LastMode != Mode::Combo && LastMode != Mode::Harass)) return;
    const AIHeroClient target(args.Target.Handle());
    if (Engine::ValidEnemy(target) && NetherBladeArmedUntil <= Now())
        (void)CastW(target, LastMode, true, false);
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFAA66FFu, 1.4f, 40);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFF8844CCu, 1.0f, 36);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("KassadinTactics", "Kassadin stack tactics"));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 55, 10, 90));
    QMenu = TacticsMenu->AddSubMenu(new Menu("NullSphere", "Null Sphere shield and projectile"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("NetherBlade", "Nether Blade reset"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("ForcePulse", "Force Pulse charges"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("Riftwalk", "Riftwalk blink safety"));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at endpoint", 2, 0, 5));
    RMenu->Add(new MenuSlider("ReserveStacks", "Reserve Riftwalk stacks", 1, 0, 2));
    RMenu->Add(new MenuSlider("ManaReserve", "Mana reserve percent", 20, 0, 60));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("KassadinFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 40, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("KassadinCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}
inline void OnLoad() {
    LastCastTick = {};
    LastAutoTargetId = LastAutoTick = IncomingThreatUntil = IncomingHardCCUntil = 0;
    ForcePulseCharges = RiftwalkStacks = RiftwalkLastCastTick = NetherBladeArmedUntil = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    ForcePulseCharges = RiftwalkStacks = 0;
}
inline constexpr const char* Scenarios[] = {
    "Pin all arithmetic to Riot 26.15 and CommunityDragon 16.15",
    "Reconcile Force Pulse six-cast charges from spell events and ForcePulseAvailable polling",
    "Consume all six Force Pulse charges only on a valid predicted cone hit",
    "Use Null Sphere's 650 range, 1400 projectile and wall collision checks",
    "Use Null Sphere magic shield reactively against an observed incoming threat",
    "Reset Nether Blade through an in-range empowered attack and preserve AA windup",
    "Track Nether Blade buff and empowered reset state from events plus polling",
    "Track Riftwalk's 15-second stack window and clamp at four stacks",
    "Include maximum-mana Riftwalk cost and damage scaling in every gate",
    "Reserve configurable mana and Riftwalk stacks before aggressive blinks",
    "Clamp Riftwalk to the 500 range endpoint and reject wall endpoints",
    "Reject unsafe enemy turret, excessive enemy-count and mobility-locked endpoints",
    "Allow lethal and defensive exceptions to endpoint density rules",
    "Use autonomous target selection before orbwalker and selector fallback",
    "Reject invalid, invulnerable and spell-shielded targets",
    "Preserve AA windup except reactive or lethal casts",
    "Resume after each observed Q W E or R event",
    "Combo sequences shielded Q, Nether Blade reset, charged E and Riftwalk",
    "Harass uses Q and charged E while respecting mana floor",
    "LaneClear Jungle and LastHit delegate to shared farm policy",
    "Flee uses defensive Q then safe Riftwalk escape",
    "Automatic mode permits reactive Q interruption and no fresh blink engage",
    "Never automate movement, items or summoner spells",
    "Draw observed Q and R ranges without changing decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Kassadin;
    controller.ControllerId = "champion.kuroaio.ai.kassadin.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIKassadin.md";
    controller.ImplementationSummary =
        "Distinct six-cast Force Pulse state, Nether Blade reset protection, Null Sphere shield gate and mana/stack-aware Riftwalk endpoint safety.";
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

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Kassadin
