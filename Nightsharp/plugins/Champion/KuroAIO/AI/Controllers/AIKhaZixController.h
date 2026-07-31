#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIKhaZixGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::KhaZix {

using namespace Geometry;
using ControllerHelpers::CaptureBeforeAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Protected;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline int ManualOverrideUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int QTargetId = 0;
inline int WTargetId = 0;
inline int ETargetId = 0;
inline int RTargetId = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int RStealthExpireTick = 0;
inline int RCastCount = 0;
inline int EvolutionMask = 0;
inline bool QWasIsolated = false;
inline bool RStealthed = false;
inline bool RControllerOwned = false;
inline Mode LastMode = Mode::None;

inline bool Buff(const AIBaseClient& unit, const char* token) {
    return unit.IsValid() && token && unit.HasBuff(token);
}

inline bool HasAnyBuff(const AIBaseClient& unit, const char* a, const char* b,
                       const char* c = nullptr) {
    return Buff(unit, a) || Buff(unit, b) || (c && Buff(unit, c));
}

inline bool IsEvolved(const AIHeroClient& player, int bit, const char* token,
                     const char* alternate) {
    return player.IsValid() && ((EvolutionMask & bit) != 0 ||
        HasAnyBuff(player, token, alternate));
}

inline bool TargetProtected(const AIHeroClient& target) {
    return Protected(target) || HasSpellShieldOrImmunity(target) ||
           ControllerHelpers::IsCommonUntargetableOrImmune(target);
}

inline bool CastReady(int slot, Mode mode, int delay = 55, bool reactive = false) {
    return Ready(slot, mode) && CastThrottleReady(LastCastTick, slot, delay) &&
        (reactive || !PreserveAttack(false));
}

inline bool IsTargetIsolated(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    return Isolated(Engine::CountEnemiesAt(target.Position(),
                                           kIsolationRadius));
}

inline bool SafeLeapEndpoint(const AIHeroClient& target, const Vector3& endpoint,
                             bool reactive, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || endpoint.IsZero() || !endpoint.IsValid()) return false;
    const bool endpointTurret = Engine::UnderEnemyTurret(endpoint);
    const bool originTurret = Engine::UnderEnemyTurret(player.Position());
    return LeapEndpointValid(player.Position(), endpoint,
        SDK::NavMesh::IsWall(endpoint), endpointTurret, originTurret,
        Engine::CountEnemiesAt(endpoint, 325.0f),
        reactive ? Slider(EMenu, "MaxEscapeEnemies", 1) :
                   Slider(EMenu, "MaxLeapEnemies", 2), lethal);
}

inline float QDamage(const AIHeroClient& target, bool isolated) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = QRawDamage(ControllerHelpers::SpellRank(0),
                                 player.TotalAttackDamage(), isolated);
    return player.CalculatePhysicalDamage(target, raw);
}

inline bool QLethal(const AIHeroClient& target, bool isolated) {
    return QDamageLethal(QDamage(target, isolated), target.Health(),
                         target.AllShield());
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 45.0f) ||
        TargetProtected(target) || !CastReady(0, mode, 45, reactive)) return false;
    const Vector3 predicted = PredictPosition(target, 0.08f);
    if (!InRange(player.Position(), predicted, kQRange, target.BoundingRadius())) return false;
    const bool isolated = IsTargetIsolated(target);
    const bool lethal = QLethal(target, isolated);
    if (!isolated && Bool(QMenu, "RequireIsolation", false) && !lethal) return false;
    if (Engine::UnderEnemyTurret(target.Position()) &&
        !Engine::UnderEnemyTurret(player.Position()) && !lethal) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    QTargetId = static_cast<int>(target.NetworkId());
    QWasIsolated = isolated;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange) ||
        TargetProtected(target) || !CastReady(1, mode, 80, reactive)) return false;
    const Vector3 predicted = PredictPosition(target, 0.28f);
    if (!predicted.IsValid() || predicted.IsZero() ||
        !InRange(player.Position(), predicted, kWRange, target.BoundingRadius()) ||
        !WLineHits(player.Position(), predicted, target.Position(),
                   target.BoundingRadius())) return false;
    if (Engine::RuntimeSpells[1]) {
        const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
        if (!ControllerHelpers::PredictionAtLeast(prediction, SDK::HitChance::High) ||
            prediction.GetCastPosition().IsZero() || !prediction.CollisionObjects.empty())
            return false;
    }
    if (ControllerHelpers::ProjectileWallBlocksFromPlayer(predicted, kWWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(1, predicted)) return false;
    WTargetId = static_cast<int>(target.NetworkId());
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange + 75.0f) ||
        TargetProtected(target) || !CastReady(2, mode, 70, reactive)) return false;
    const Vector3 predicted = PredictPosition(target, 0.25f);
    const Vector3 endpoint = LeapEndpoint(player.Position(), predicted,
                                           IsEvolved(player, 4, "KhazixEEvo", "KhaZixEEvo")
                                               ? kERange + 200.0f : kERange);
    if (!SafeLeapEndpoint(target, endpoint, reactive,
                          QLethal(target, IsTargetIsolated(target)))) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    ETargetId = static_cast<int>(target.NetworkId());
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) ||
        !CastThrottleReady(LastCastTick, 3, 120) ||
        (!reactive && PreserveAttack(false))) return false;
    const bool isolated = Engine::ValidEnemy(target) && IsTargetIsolated(target);
    const bool lethal = Engine::ValidEnemy(target) && QLethal(target, isolated);
    const bool targetTurret = Engine::ValidEnemy(target) && Engine::UnderEnemyTurret(target.Position());
    if (!reactive && Engine::ValidEnemy(target) &&
        !RTargetSafe(isolated, targetTurret,
                     Engine::UnderEnemyTurret(player.Position()), lethal)) return false;
    if (!reactive && Engine::UnderEnemyTurret(player.Position()) &&
        Engine::CountEnemiesAt(player.Position(), 500.0f) > Slider(RMenu, "MaxREnemies", 2)) return false;
    if (!reactive && !isolated && Bool(RMenu, "RequireIsolation", true)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    ++RCastCount;
    RTargetId = Engine::ValidEnemy(target) ? static_cast<int>(target.NetworkId()) : 0;
    RControllerOwned = true;
    RStealthed = true;
    RStealthExpireTick = Now() + static_cast<int>(kRStealthSeconds * 1000.0f);
    LastCastTick[3] = Now();
    return true;
}

inline bool CastREcast(const AIHeroClient& target, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !IsEvolved(player, 8, "KhazixREvo", "KhaZixREvo") ||
        !RRecastAvailable(true, RStealthed, RCastCount, Now(), RStealthExpireTick)) return false;
    if (Engine::ValidEnemy(target) && !IsTargetIsolated(target) &&
        Bool(RMenu, "RequireIsolation", true)) return false;
    return CastR(target, mode, true);
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (RStealthed && CastREcast(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 45) ||
        !Engine::ValidEnemy(target)) return;
    if (CastW(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) &&
        (GameObjects::Player().HealthPercent() <= Slider(RMenu, "EscapeHP", 40) ||
         IncomingHardCCUntil > Now())) {
        if (RStealthed && CastREcast(target, Mode::Flee)) return;
        if (CastR(target, Mode::Flee, true)) return;
    }
    if (Engine::ValidEnemy(target)) (void)CastE(target, Mode::Flee, true);
}

inline bool Automatic(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (IncomingHardCCUntil > Now() && CastR(target, Mode::Automatic, true)) return true;
    return QLethal(target, IsTargetIsolated(target)) && CastQ(target, Mode::Automatic, true);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 35)) return;
    (void)Engine::TryFarm(mode);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    EvolutionMask = 0;
    if (HasAnyBuff(player, "KhazixQEvo", "KhaZixQEvo")) EvolutionMask |= 1;
    if (HasAnyBuff(player, "KhazixWEvo", "KhaZixWEvo")) EvolutionMask |= 2;
    if (HasAnyBuff(player, "KhazixEEvo", "KhaZixEEvo")) EvolutionMask |= 4;
    if (HasAnyBuff(player, "KhazixREvo", "KhaZixREvo")) EvolutionMask |= 8;
    RStealthed = HasAnyBuff(player, "KhazixRStealth", "KhaZixRStealth", "KhazixR");
    if (!RStealthed && RStealthExpireTick <= now) {
        RControllerOwned = false;
        if (now - LastCastTick[3] > 1800) RCastCount = 0;
    }
    if (QTargetId != 0 && !GameObjects::GetUnitByNetworkId<AIHeroClient>(QTargetId).IsValid())
        QTargetId = 0;
    if (WTargetId != 0 && !GameObjects::GetUnitByNetworkId<AIHeroClient>(WTargetId).IsValid())
        WTargetId = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    if (ManualOverrideUntil > Now()) return true;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(
        selected, mode == Mode::Flee ? 950.0f : kWRange);
    if (mode == Mode::Automatic) { (void)Automatic(target); return true; }
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
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
            const bool manual = !Engine::WasControllerCast(slot);
            if (manual) ManualOverrideUntil = now +
                Slider(TacticsMenu, "ManualOwnershipMs", 650);
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (slot == 3) {
                if (manual) ++RCastCount;
                RStealthed = true;
                RStealthExpireTick = now + 1250;
            }
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

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "KhazixRStealth") ||
        Engine::TextContains(args.BuffName, "KhaZixRStealth")) {
        RStealthed = added;
        if (added) RStealthExpireTick = Now() + 1250;
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, true); }
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, false); }
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    UpdateBuffState(args, args.EndTime > Game::Time());
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureBeforeAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)ControllerHelpers::CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    (void)ControllerHelpers::CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0x33FF8F4Fu, 1.2f, 48);
    Drawing::DrawCircle(player.Position(), kERange, 0x339F6FFFu, 1.2f, 48);
    Drawing::DrawCircle(player.Position(), kWRange, 0x336F4FFFu, 1.2f, 64);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("KhaZixOneTrick", "Kha'Zix isolation tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 650, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Taste Their Fear"));
    QMenu->Add(new MenuBool("RequireIsolation", "Require isolation unless lethal", false));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Void Spike"));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Leap safety"));
    EMenu->Add(new MenuSlider("MaxLeapEnemies", "Maximum enemies at leap endpoint", 2, 0, 5));
    EMenu->Add(new MenuSlider("MaxEscapeEnemies", "Maximum enemies at escape endpoint", 1, 0, 3));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Void Assault"));
    RMenu->Add(new MenuBool("RequireIsolation", "Require isolated target", true));
    RMenu->Add(new MenuSlider("MaxREnemies", "Maximum enemies while stealthed", 2, 0, 5));
    RMenu->Add(new MenuSlider("EscapeHP", "Escape health percent", 40, 10, 70));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("KhaZixFarm", "Safe farm"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum farm mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("KhaZixCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q, W and E ranges", false));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    ManualOverrideUntil = LastAutoTargetId = LastAutoTick = QTargetId = WTargetId =
        ETargetId = RTargetId = IncomingThreatUntil = IncomingHardCCUntil =
        RStealthExpireTick = RCastCount = EvolutionMask = 0;
    QWasIsolated = RStealthed = RControllerOwned = false;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    QWasIsolated = RStealthed = RControllerOwned = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Reconcile Q/W/E/R evolution buffs by event and polling before route decisions",
    "Prefer selected target and use orbwalker fallback only when selection is invalid",
    "Compute target isolation from the 425-unit nearby-enemy boundary",
    "Apply the isolated Taste Their Fear multiplier to damage and lethal gates",
    "Preserve attacks and manual Q/W/E/R ownership before nonreactive casts",
    "Aim Void Spike with prediction, collision rejection, width and projectile-wall checks",
    "Use the real 325-unit Q melee envelope and reject protected targets",
    "Clamp Leap to its evolved endpoint range and reject wall, turret and crowded landings",
    "Use Void Assault only for isolated-target setup or urgent escape",
    "Track stealth expiration, evolved R recasts, charges and manual recasts",
    "Reject offensive R around unsafe turret transitions and excessive local enemies",
    "Automatic mode is restricted to lethal isolated Q or hard-CC stealth escape",
    "Combo prioritizes spike contact, isolated Q, safe leap and bounded stealth",
    "Harass preserves mana and uses only collision-safe W and Q poke",
    "LaneClear, Jungle and LastHit delegate to shared farm policy",
    "Flee spends stealth under low health or hard crowd control before safe leap",
    "Expose Q, W and E ranges without altering gameplay decisions",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Kha'Zix";
    controller.ControllerId = "champion.kuroaio.ai.khazix.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIKhaZix.md";
    controller.ImplementationSummary =
        "Isolation-aware Taste Their Fear damage, collision-safe Void Spike, safe evolved Leap endpoints and stateful Void Assault stealth/recasts.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::KhaZix
