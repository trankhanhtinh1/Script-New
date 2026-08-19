#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AISeraphine.h"
#include "AISeraphineGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Seraphine {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocks;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Now;

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
inline int ManualOwnershipUntil = 0;
inline int EnemyThreatUntil = 0;
inline int HardCcThreatUntil = 0;
inline int Notes = 0;
inline int EchoUntil = 0;
inline int RLastCastTick = 0;
inline Vector3 LastRAim{};

inline bool Throttle(int slot, int delay = 44) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline AIHeroClient SelectEnemy(const AIHeroClient& selected, float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orb = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range)) return orb;
    return Engine::SelectTarget(range);
}
inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() || HasSpellShieldOrImmunity(target);
}
inline int NearbyAllies(float range) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int count = 0;
    for (const auto& ally : GameObjects::AllyHeroes())
        if (Engine::ValidAlly(ally, range) && ally.Position().Distance2D(player.Position()) <= range) ++count;
    return count;
}
inline float LowestAllyHealth() {
    float lowest = 100.0f;
    for (const auto& ally : GameObjects::AllyHeroes())
        if (Engine::ValidAlly(ally, 800.0f)) lowest = std::min(lowest, ally.HealthPercent());
    return lowest;
}
inline bool SafeCommit(bool defensive = false) {
    const auto player = GameObjects::Player();
    return player.IsValid() && (!Engine::UnderEnemyTurret(player.Position()) || defensive ||
        Engine::CountEnemiesAt(player.Position(), 600.0f) <= Slider(TacticsMenu, "MaxTurretEnemies", 2));
}
inline bool AttackWindingUp() { return Orbwalker::IsWindingUp(); }

inline float QRawDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    const int rank = std::clamp(SpellRank(0), 1, 5);
    return player.CalculateMagicDamage(target, 35.0f + 25.0f * static_cast<float>(rank - 1) + player.AP() * 0.4f);
}
inline float ERawDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    const int rank = std::clamp(SpellRank(2), 1, 5);
    return player.CalculateMagicDamage(target, 40.0f + 30.0f * static_cast<float>(rank - 1) + player.AP() * 0.5f);
}
inline float RRawDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    const int rank = std::clamp(SpellRank(3), 1, 3);
    return player.CalculateMagicDamage(target, 100.0f + 100.0f * static_cast<float>(rank - 1) + player.AP() * 0.4f);
}

inline bool CastQ(const AIHeroClient& selected, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    const auto target = SelectEnemy(selected, kQRange);
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + kQRadius) || Protected(target) ||
        !Ready(0, mode) || !Throttle(0) || (!reactive && AttackWindingUp()) ||
        player.ManaPercent() < Slider(QMenu, "MinimumMana", 42) || !SafeCommit(reactive)) return false;
    const Vector3 predicted = PredictPosition(target, TravelSeconds(player.Position().Distance2D(target.Position())));
    if (!TargetReachable(player.Position(), predicted, kQRange, target.BoundingRadius()) ||
        ProjectileWallBlocks(player.Position(), predicted, 25.0f)) return false;
    const float damage = QRawDamage(target);
    const bool lethal = QExecuteWindow(target.HealthPercent(), damage, target.Health(), target.AllShield());
    if (!reactive && !lethal && target.HealthPercent() > Slider(QMenu, "ExecuteHealth", 25)) return false;
    if (!Engine::ControllerCastPosition(0, predicted)) return false;
    LastCastTick[0] = Now();
    Notes = AdvanceNotes(Notes, NearbyAllies(800.0f));
    EchoUntil = Notes >= 3 ? Now() + 2600 : EchoUntil;
    return true;
}

inline bool CastW(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) || !SafeCommit(reactive) ||
        (!reactive && AttackWindingUp()) || player.ManaPercent() < Slider(WMenu, "MinimumMana", 50)) return false;
    const float lowest = LowestAllyHealth();
    const bool threatened = Engine::CountEnemiesAt(player.Position(), 800.0f) > 0 ||
        EnemyThreatUntil > Now() || HardCcThreatUntil > Now();
    if (!ShouldCastW(player.HealthPercent(), lowest,
                     Engine::CountEnemiesAt(player.Position(), 600.0f),
                     Engine::UnderEnemyTurret(player.Position()),
                     Slider(TacticsMenu, "MaxTurretEnemies", 2), reactive || threatened)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    Notes = AdvanceNotes(Notes, NearbyAllies(800.0f));
    EchoUntil = Notes >= 3 ? Now() + 2600 : EchoUntil;
    return true;
}

inline bool CastE(const AIHeroClient& selected, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    const auto target = SelectEnemy(selected, kERange);
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange) || Protected(target) ||
        !Ready(2, mode) || !Throttle(2) || (!reactive && AttackWindingUp())) return false;
    const Vector3 predicted = PredictPosition(target, TravelSeconds(player.Position().Distance2D(target.Position())));
    if (!TargetReachable(player.Position(), predicted, kERange, target.BoundingRadius()) ||
        ProjectileWallBlocks(player.Position(), predicted, kEWidth * 0.5f)) return false;
    const bool slowed = target.HasBuff("slow") || target.HasBuff("SeraphineESlow");
    const bool rooted = target.HasBuff("SeraphineERoot") || target.HasBuff("root");
    const auto control = ControlForTarget(slowed, rooted, EchoReady(Notes));
    if (!reactive && control == EControl::Slow && mode == Mode::Harass &&
        player.ManaPercent() < Slider(EMenu, "HarassMana", 55)) return false;
    if (!Engine::ControllerCastPosition(2, predicted)) return false;
    LastCastTick[2] = Now();
    Notes = AdvanceNotes(Notes, NearbyAllies(800.0f));
    EchoUntil = Notes >= 3 ? Now() + 2600 : EchoUntil;
    return true;
}

inline int RChampionContacts(const Vec3& origin, const Vec3& endpoint) {
    int contacts = 0;
    for (const auto& ally : GameObjects::AllyHeroes())
        if (Engine::ValidAlly(ally, ExtendedRRange(4)) && LineContacts(origin, endpoint, ally.Position(), kRWidth, ally.BoundingRadius())) ++contacts;
    for (const auto& enemy : GameObjects::EnemyHeroes())
        if (Engine::ValidEnemy(enemy, ExtendedRRange(4)) && LineContacts(origin, endpoint, enemy.Position(), kRWidth, enemy.BoundingRadius())) ++contacts;
    return contacts;
}
inline bool CastR(const AIHeroClient& selected, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    const auto target = SelectEnemy(selected, kRRange);
    if (!player.IsValid() || !Engine::ValidEnemy(target, ExtendedRRange(4)) || Protected(target) ||
        !Ready(3, mode) || !Throttle(3, 140) || (!reactive && AttackWindingUp())) return false;
    const Vector3 predicted = PredictPosition(target, TravelSeconds(player.Position().Distance2D(target.Position()), kCastDelay, kMissileSpeed));
    const int contacts = RChampionContacts(player.Position(), predicted);
    const Vector3 endpoint = ExtendedEndpoint(player.Position(), predicted, contacts);
    const bool defensive = reactive || mode == Mode::Flee || HardCcThreatUntil > Now();
    if (!TargetReachable(player.Position(), predicted, ExtendedRRange(contacts), target.BoundingRadius()) ||
        !LineContacts(player.Position(), endpoint, predicted, kRWidth, target.BoundingRadius()) ||
        !WallSafe(player.Position(), endpoint, ProjectileWallBlocks(player.Position(), endpoint, kRWidth * 0.5f), ExtendedRRange(contacts))) return false;
    const bool lethal = RRawDamage(target) >= target.Health() + target.AllShield();
    if (!defensive && Engine::CountEnemiesAt(player.Position(), 850.0f) < Slider(RMenu, "MinimumTargets", 2) &&
        !lethal && target.HealthPercent() > Slider(RMenu, "ExecuteHealth", 30)) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    LastCastTick[3] = RLastCastTick = Now();
    LastRAim = endpoint;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (CastR(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    (void)CastW(Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (CastE(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (CastW(Mode::Flee, true)) return;
    if (CastE(target, Mode::Flee, true)) return;
    (void)CastR(target, Mode::Flee, true);
}
inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 38)) return;
    if (mode == Mode::Jungle && ControllerHelpers::HasNearbyJungleTarget(kQRange)) {
        const auto monster = ControllerHelpers::SelectJungleTarget(kQRange);
        if (monster.IsValid() && Ready(0, mode) && Throttle(0)) (void)Engine::ControllerCastPosition(0, monster.Position());
    } else if (mode == Mode::LaneClear || mode == Mode::LastHit) (void)Engine::TryFarm(mode);
}
inline void Automatic(const AIHeroClient& target) {
    if (CastW(Mode::Automatic, true)) return;
    if (EnemyThreatUntil > Now() || HardCcThreatUntil > Now()) {
        if (CastE(target, Mode::Automatic, true)) return;
        (void)CastR(target, Mode::Automatic, true);
    }
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (player.HasBuff("SeraphinePassiveNotes") || player.HasBuff("SeraphinePassiveNotesBuff"))
        Notes = std::clamp(player.GetBuffCount("SeraphinePassiveNotes"), 0, 4);
    if (EchoUntil <= now) EchoUntil = 0;
    if (ManualOwnershipUntil <= now) ManualOwnershipUntil = 0;
    if (EnemyThreatUntil <= now) EnemyThreatUntil = 0;
    if (HardCcThreatUntil <= now) HardCcThreatUntil = 0;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const auto target = SelectEnemy(selected, mode == Mode::Flee ? 1400.0f : ExtendedRRange(4));
    if (mode == Mode::Automatic) Automatic(target);
    else if (mode == Mode::Combo) Combo(target);
    else if (mode == Mode::Harass) Harass(target);
    else if (mode == Mode::Flee) Flee(NearestEnemyToPlayer(target, 1400.0f));
    else if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) Farm(mode);
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack) return;
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (slot < 3) {
                Notes = AdvanceNotes(Notes, NearbyAllies(800.0f));
                EchoUntil = Notes >= 3 ? now + 2600 : EchoUntil;
            }
            if (!Engine::WasControllerCast(slot)) ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 600);
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    EnemyThreatUntil = std::max(EnemyThreatUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) HardCcThreatUntil = std::max(HardCcThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick)) return;
    if (EchoReady(Notes)) Notes = ConsumeEcho(Notes);
}
inline void OnBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "SeraphinePassiveNotes")) {
        if (added) Notes = std::clamp(args.Count, 0, 4);
        else Notes = 0;
    } else if (Engine::TextContains(args.BuffName, "SeraphineW")) {
        if (added) EchoUntil = std::max(EchoUntil, Now() + 2500);
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { OnBuffState(args, true); }
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { OnBuffState(args, false); }
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    const auto player = GameObjects::Player();
    if (player.IsValid() && (args.IsDirectedToPlayer || (args.End.IsValid() && args.End.Distance2D(player.Position()) < 500.0f))) {
        EnemyThreatUntil = std::max(EnemyThreatUntil, Now() + 750);
        HardCcThreatUntil = std::max(HardCcThreatUntil, Now() + 500);
    }
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    const int remaining = args.EndTime > Game::Time() ? static_cast<int>((args.EndTime - Game::Time()) * 1000.0f) : 900;
    HardCcThreatUntil = std::max(HardCcThreatUntil, Now() + std::clamp(remaining, 250, 5000));
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) { if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); }
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) { (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick); }
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF62D5E8u, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF8C6BFFu, 1.0f, 36);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFE76BFFu, 1.0f, 36);
    if (LastRAim.IsValid() && !LastRAim.IsZero()) Drawing::DrawLine(player.Position(), LastRAim, 0xFFE76BFFu, 1.0f);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("SeraphineTactics", "Seraphine Echo and team tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 600, 180, 1400));
    TacticsMenu->Add(new MenuSlider("MaxTurretEnemies", "Max enemies under turret", 2, 0, 5));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "High Note execute"));
    QMenu->Add(new MenuSlider("MinimumMana", "Minimum Q mana %", 42, 0, 90));
    QMenu->Add(new MenuSlider("ExecuteHealth", "Q execute window %", 25, 5, 50));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Surround Sound support"));
    WMenu->Add(new MenuSlider("MinimumMana", "Minimum W mana %", 50, 0, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Beat Drop control"));
    EMenu->Add(new MenuSlider("HarassMana", "Harass E mana %", 55, 10, 90));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Encore charm extension"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum normal targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("ExecuteHealth", "Single target health %", 30, 5, 75));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("SeraphineFarm", "Resource-safe farm"));
    FarmMenu->Add(new MenuSlider("Mana", "Farm mana floor %", 38, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("SeraphineCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E/R ranges", false));
}
inline void OnLoad() {
    LastCastTick.fill(0); LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = 0;
    EnemyThreatUntil = HardCcThreatUntil = Notes = EchoUntil = RLastCastTick = 0; LastRAim = {};
}
inline void OnUnload() { TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr; LastRAim = {}; }

inline constexpr const char* Scenarios[] = {
    "Reconcile Notes from passive buffs and spell events, including manual ownership windows",
    "Advance Notes for Q W E and consume exactly three on a basic attack Echo",
    "Respect selected target, then orbwalker target, then engine fallback",
    "Predict Q area contact and apply the 25 percent execute amplification",
    "Use Surround Sound for allied shield, missing-health heal, speed, and peel windows",
    "Escalate Beat Drop from slow to root or echoed stun using target crowd-control state",
    "Reject E paths blocked by projectile walls and targets outside 1300 range",
    "Extend Encore through allied and enemy champion contacts without treating allies as charm targets",
    "Reject unsafe turret commitments and dense enemy dives unless a defensive reaction is urgent",
    "Gate casts on mana, cooldown, damage, shield, and health thresholds",
    "Preserve auto-attack windup except for reactive W or crowd-control peel",
    "Combo uses Encore, Beat Drop, execute-weighted High Note, then Surround Sound",
    "Harass uses Beat Drop and High Note while preserving W and R resources",
    "LaneClear Jungle and LastHit retain resource-safe farming without random Encore",
    "Flee uses Surround Sound, Beat Drop peel, and defensive Encore",
    "Automatic responds to low allies, gapclosers, hard crowd control, and interrupt windows",
    "Draw Q E R reach and last Encore aim without changing decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Seraphine;
    controller.ControllerId = "champion.kuroaio.ai.seraphine.catcher";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISeraphine.md";
    controller.ImplementationSummary = "Echo/Notes state machine with execute Q, ally W utility, escalating E control, and champion-extending R charm policy.";
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
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Seraphine
