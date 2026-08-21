#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AITahmKenchGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::TahmKench {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::RawAllyHeroByNetworkId;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PassiveTargetId = 0;
inline int PassiveStacks = 0;
inline int PassiveLastTick = 0;
inline float GreyHealthObserved = 0.0f;
inline int WChannelUntil = 0;
inline int RChannelUntil = 0;
inline int RTargetId = 0;
inline bool RDevouringAlly = false;

inline bool Throttle(int slot, int delay = 70) {
    return slot >= 0 && slot < 4 && LastCastTick[static_cast<std::size_t>(slot)] + delay <= Now();
}

inline bool ProtectedEnemy(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() || HasSpellShieldOrImmunity(target);
}

inline int ObservedStacks(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0;
    return std::max({target.GetBuffCount("TahmKenchPassive"),
        target.GetBuffCount("TahmKenchP"), target.GetBuffCount("tahmkenchpassive")});
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, QRawDamage(SpellRank(0), player.AP())) : 0.0f;
}

inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, WRawDamage(SpellRank(1), player.AP())) : 0.0f;
}

inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, REnemyRawDamage(SpellRank(3), target.MaxHealth(), player.AP())) : 0.0f;
}

inline AIHeroClient ProtectedAlly() {
    const auto ally = SelectProtectionAlly(1300.0f);
    return Engine::ValidAlly(ally, 300.0f) ? ally : AIHeroClient{};
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || ProtectedEnemy(target) || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, kQDelay);
    if (!QReachable(player.Position(), aim) || !QLineHits(player.Position(), aim,
        target.Position(), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    if (!Engine::RuntimeSpells[0] || Engine::RuntimeSpells[0]->GetPrediction(target).Hitchance < SDK::HitChance::High) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    PassiveTargetId = static_cast<int>(target.NetworkId());
    PassiveStacks = std::max(PassiveStacks, ObservedStacks(target));
    PassiveLastTick = LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 100) || WChannelUntil > Now() ||
        PreserveAttack(reactive)) return false;
    const Vector3 endpoint = Engine::ValidEnemy(target) ? PredictPosition(target, kWChannelSeconds) : Game::CursorPos();
    const float range = WRange(SpellRank(1));
    const int enemies = Engine::CountEnemiesAt(endpoint, kWRadius);
    const bool lethal = Engine::ValidEnemy(target) && ControllerHelpers::Lethal(target, WDamage(target));
    if (!WEndpointSafe(endpoint, false, false, enemies, Slider(WMenu, "MaxEndpointEnemies", 2), lethal)) return false;
    if (!Engine::ControllerCastPosition(1, endpoint)) return false;
    WChannelUntil = LastCastTick[1] = Now() + static_cast<int>(kWChannelSeconds * 1000.0f);
    if (Engine::ValidEnemy(target)) {
        PassiveTargetId = static_cast<int>(target.NetworkId());
        PassiveStacks = std::max(PassiveStacks, ObservedStacks(target));
    }
    return true;
}

inline bool CastE(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2, 100) ||
        PreserveAttack(reactive)) return false;
    const float missing = std::max(0.0f, player.MaxHealth() - player.Health());
    const float grey = std::min(missing, std::max(GreyHealthObserved, missing * 0.55f));
    const bool threatened = IncomingThreatUntil > Now() || Engine::CountEnemiesAt(player.Position(), 700.0f) > 0;
    if (!GreyHealthUsable(grey, player.Health(), player.MaxHealth()) || (!reactive && !threatened && player.HealthPercent() > Slider(EMenu, "HealthThreshold", 72))) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    GreyHealthObserved = 0.0f;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastRAlly(const AIHeroClient& ally, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidAlly(ally, 300.0f) ||
        ally.NetworkId() == player.NetworkId() || !Ready(3, mode) || !Throttle(3, 120) ||
        RChannelUntil > Now() || PreserveAttack(reactive)) return false;
    const int nearbyEnemies = Engine::CountEnemiesAt(ally.Position(), 700.0f);
    const int nearbyAllies = Engine::CountAlliesAt(ally.Position(), 760.0f);
    const bool valid = RAllyEligible(ally.HealthPercent(),
        static_cast<float>(Slider(RMenu, "AllyHealthThreshold", 42)),
        nearbyEnemies > 0, Engine::UnderEnemyTurret(ally.Position()), nearbyEnemies, nearbyAllies);
    if (!valid || !Engine::ControllerCastUnit(3, ally)) return false;
    RTargetId = static_cast<int>(ally.NetworkId());
    RDevouringAlly = true;
    RChannelUntil = LastCastTick[3] = Now() + static_cast<int>(kRChannelSeconds * 1000.0f);
    return true;
}

inline bool CastREnemy(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || ProtectedEnemy(target) || !Engine::ValidEnemy(target, kRRange) ||
        !Ready(3, mode) || !Throttle(3, 120) || RChannelUntil > Now() || PreserveAttack(reactive)) return false;
    const int stacks = std::max(PassiveStacks, ObservedStacks(target));
    const bool lethal = ControllerHelpers::Lethal(target, RDamage(target));
    const int enemies = Engine::CountEnemiesAt(player.Position(), 600.0f);
    if (!REnemyEligible(stacks, lethal, enemies <= 1, Engine::UnderEnemyTurret(player.Position()),
        enemies, Slider(RMenu, "MaxEnemyCount", 2))) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    RTargetId = static_cast<int>(target.NetworkId());
    RDevouringAlly = false;
    RChannelUntil = LastCastTick[3] = Now() + static_cast<int>(kRChannelSeconds * 1000.0f);
    PassiveStacks = 0;
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (ProtectedEnemy(target)) return false;
    if (ControllerHelpers::Lethal(target, RDamage(target)) && CastREnemy(target, mode)) return true;
    if (ControllerHelpers::Lethal(target, QDamage(target)) && CastQ(target, mode, true)) return true;
    return ControllerHelpers::Lethal(target, WDamage(target)) && CastW(target, mode, true);
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (TryKillSecure(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (PassiveReady(std::max(PassiveStacks, ObservedStacks(target))) && CastREnemy(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastE(Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 45)) return;
    if (Engine::ValidEnemy(target)) (void)CastQ(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& pursuer) {
    if (CastE(Mode::Flee, true)) return;
    if (Engine::ValidEnemy(pursuer)) (void)CastW(pursuer, Mode::Flee, true);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 35)) return;
    if (mode == Mode::LastHit) return;
    (void)Engine::TryFarm(mode);
}

inline void Automatic() {
    const auto ally = ProtectedAlly();
    if (Engine::ValidAlly(ally) && CastRAlly(ally, Mode::Automatic, true)) return;
    const auto target = Engine::SelectTarget(kQRange);
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target)) {
        if (CastQ(target, Mode::Automatic, true)) return;
        (void)CastE(Mode::Automatic, true);
        return;
    }
    if (Engine::ValidEnemy(target) && ControllerHelpers::Lethal(target, QDamage(target)))
        (void)CastQ(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    if (!player.IsValid()) return;
    if (WChannelUntil <= now) WChannelUntil = 0;
    if (RChannelUntil <= now) {
        RChannelUntil = 0;
        RTargetId = 0;
        RDevouringAlly = false;
    }
    if (PassiveTargetId != 0) {
        const auto target = HeroByNetworkId(PassiveTargetId);
        if (Engine::ValidEnemy(target)) PassiveStacks = ObservedStacks(target);
    }
    if (PassiveDecayDue(now, PassiveLastTick) && PassiveStacks > 0) {
        --PassiveStacks;
        PassiveLastTick = now;
    }
    const float missing = std::max(0.0f, player.MaxHealth() - player.Health());
    if (missing <= 0.0f) GreyHealthObserved = 0.0f;
    else GreyHealthObserved = std::min(player.MaxHealth() * 0.55f, std::max(GreyHealthObserved, missing * 0.55f));
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    ReconcileState();
    const auto target = Engine::SelectTarget(mode == Mode::Flee ? 1300.0f : kQRange);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1300.0f)); break;
    case Mode::Automatic: Automatic(); break;
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
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (slot == 1) WChannelUntil = now + static_cast<int>(kWChannelSeconds * 1000.0f);
            if (slot == 3) RChannelUntil = now + static_cast<int>(kRChannelSeconds * 1000.0f);
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max({IncomingThreatUntil, analysis.CommitmentUntilTick, analysis.LineThreatUntilTick});
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "TahmKenchE")) GreyHealthObserved = std::max(GreyHealthObserved, 1.0f);
        if (Engine::TextContains(args.BuffName, "TahmKenchW")) WChannelUntil = std::max(WChannelUntil, Now() + 350);
    } else {
        const auto enemy = HeroByNetworkId(static_cast<int>(args.Sender.NetworkId));
        if (!Engine::ValidEnemy(enemy)) return;
        const int stacks = ObservedStacks(enemy);
        if (stacks > 0) {
            PassiveTargetId = static_cast<int>(args.Sender.NetworkId);
            PassiveStacks = stacks;
            PassiveLastTick = Now();
        }
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "TahmKenchR")) {
        RChannelUntil = 0;
        RTargetId = 0;
        RDevouringAlly = false;
    }
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    IncomingThreatTargetId = static_cast<int>(args.NetworkId);
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 700);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    IncomingThreatTargetId = static_cast<int>(args.NetworkId);
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 700);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
        LastAutoTick = Now();
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
    const auto target = HeroByNetworkId(LastAutoTargetId);
    if (Engine::ValidEnemy(target)) {
        PassiveTargetId = LastAutoTargetId;
        PassiveStacks = AddPassiveStack(std::max(PassiveStacks, ObservedStacks(target)));
        PassiveLastTick = Now();
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    const auto target = HeroByNetworkId(static_cast<int>(args.TargetNetworkId));
    if (Engine::ValidEnemy(target)) {
        PassiveTargetId = static_cast<int>(target.NetworkId());
        PassiveStacks = AddPassiveStack(std::max(PassiveStacks, ObservedStacks(target)));
        PassiveLastTick = Now();
    }
}

inline void OnDraw() {
    if (!CoachMenu || !ControllerHelpers::Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF66CCAAu, 1.4f, 36);
    Drawing::DrawCircle(player.Position(), WRange(SpellRank(1)), 0xFFAA66CCu, 1.2f, 36);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Tongue Lash"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Abyssal Dive"));
    WMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 0, 5));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Thick Skin"));
    EMenu->Add(new MenuSlider("HealthThreshold", "Health threshold", 72, 20, 95));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Devour"));
    RMenu->Add(new MenuSlider("AllyHealthThreshold", "Ally rescue health percent", 42, 10, 80));
    RMenu->Add(new MenuSlider("MaxEnemyCount", "Maximum nearby enemies", 2, 0, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("TahmKenchFarm", "Farm policy"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("TahmKenchCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and W ranges", false));
}

inline void ResetState() {
    LastAutoTargetId = LastAutoTick = PassiveTargetId = PassiveStacks = PassiveLastTick = 0;
    GreyHealthObserved = 0.0f;
    WChannelUntil = RChannelUntil = RTargetId = 0;
    RDevouringAlly = false;
}

inline void OnLoad() { ResetState(); }
inline void OnUnload() {
    ResetState();
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Track Acquired Taste stacks on the active enemy from auto attacks, Q and buff polling",
    "Decay passive stacks on the live one-second cadence and clear stale target state",
    "Predict Tongue Lash at 900 range with missile collision and projectile-wall rejection",
    "Apply Q slow and require three observed stacks before enemy Devour",
    "Use Abyssal Dive channel, radius, knock-up and rank-scaled reach",
    "Reject W walls, projectile barriers, turrets and over-committed landing endpoints",
    "Track Thick Skin grey health and spend only observed missing-health value on E shield",
    "Use autonomous Engine target selection with orbwalker attack safety",
    "Devour threatened allies only when rescue safety and channel range are valid",
    "Devour enemies only with three stacks and lethal or isolated safe policy",
    "Keep R channel ownership bounded and reconcile ally or enemy channel buffs",
    "Automatic mode answers only incoming threats, ally rescue or verified Q lethal",
    "Combo sequences Q stacks, enemy Devour, safe W and grey-health E",
    "Harass spends Q under mana policy without initiating W or R",
    "LaneClear, Jungle and LastHit use shared farm policy without inventing stacks",
    "Flee converts grey health first and uses safe W peel second",
    "Track gapcloser and interrupt windows for bounded reactive casts",
    "Expose complete event, polling, object and missile callback ABI",
    "Never automate items, summoners, unsafe turret dives or movement outside W policy",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::TahmKench;
    controller.ControllerId = "champion.kuroaio.ai.tahmkench.devour";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AITahmKench.md";
    controller.ImplementationSummary =
        "Passive-stack tracking with collision-safe Tongue Lash, bounded Abyssal Dive, grey-health Thick Skin and ally/enemy Devour safety policy.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::TahmKench
