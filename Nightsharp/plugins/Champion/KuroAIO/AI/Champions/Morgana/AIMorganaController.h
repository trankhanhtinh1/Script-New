#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIMorganaGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Morgana {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureBeforeAttackTargetEvent;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::RawAllyHeroByNetworkId;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

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
inline int ManualOwnershipUntil = 0;
inline int ThreatAllyId = 0;
inline int ThreatUntil = 0;
inline int HardCcUntil = 0;
inline int RStartedTick = 0;
inline int RTetherUntil = 0;
inline int RStunReadyTick = 0;
inline int LastRTargetId = 0;
inline bool RActive = false;
inline bool RRecastPending = false;
inline bool BlackShieldActive = false;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
using ControllerHelpers::PreserveAttack;

inline bool Throttle(int slot, int delay = 52) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool ProtectedEnemy(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline bool ProtectedAlly(const AIHeroClient& ally) {
    return !Engine::ValidAlly(ally) || ally.IsDead();
}

inline bool Rooted(const AIHeroClient& target) {
    return target.HasBuff("MorganaQ") || target.HasBuff("morganaq") ||
        target.HasBuff("Root") || target.HasBuff("Stun");
}

inline bool LethalWith(const AIHeroClient& target, int slot,
                       float multiplier = 1.0f) {
    if (!Engine::ValidEnemy(target) || slot < 0 || slot > 3 ||
        !Engine::RuntimeSpells[slot]) return false;
    return Engine::RuntimeSpells[slot]->GetDamage(target) * multiplier >=
        target.Health() + target.AllShield();
}

inline AIHeroClient SelectEnemy(const AIHeroClient& selected,
                                float range = kQRange) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orb = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range)) return orb;
    return Engine::SelectTarget(range);
}

inline AIHeroClient SelectThreatAlly(bool includeSelf = true) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    if (ThreatAllyId != 0 && Now() <= ThreatUntil) {
        const auto tracked = RawAllyHeroByNetworkId(ThreatAllyId);
        if (Engine::ValidAlly(tracked, kERange)) return tracked;
    }
    if (includeSelf && Now() <= ThreatUntil &&
        static_cast<int>(player.NetworkId()) == ThreatAllyId)
        return player;
    const auto best = SelectProtectionAlly(kERange, ThreatAllyId, ThreatUntil,
                                           260.0f, 720.0f);
    return Engine::ValidAlly(best, kERange) ? best : AIHeroClient{};
}

inline bool SafeRPosition(const AIHeroClient& target, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const int allies = ControllerHelpers::CountAlliedFollowup(
        player.Position(), kRRadius, false);
    if (!SafeUltimatePosition(player.Position(), SDK::NavMesh::IsWall(player.Position()),
                              Engine::UnderEnemyTurret(player.Position()), enemies,
                              allies, Slider(RMenu, "MaximumEnemies", 3), defensive))
        return false;
    return defensive || Engine::ValidEnemy(target, kRRadius + target.BoundingRadius());
}

inline Vector3 QAim(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kQRange)) return {};
    Vector3 aim = PredictPosition(target, kQDelay);
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.Hitchance >= SDK::HitChance::High &&
            prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero())
            aim = prediction.GetCastPosition();
    }
    return aim;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange) ||
        ProtectedEnemy(target) || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = QAim(target);
    if (!aim.IsValid() || aim.IsZero() ||
        !QLineHits(player.Position(), aim, target.Position(), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f))
        return false;
    const float targetDistance = player.Position().Distance2D(target.Position());
    if (!QCollisionFree(targetDistance, -1.0f, target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool farm = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange) ||
        ProtectedEnemy(target) || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, 0.22f);
    if (!aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > kWRange ||
        !ZoneContains(aim, target.Position(), target.BoundingRadius())) return false;
    const float multiplier = WMissingHealthMultiplier(target.HealthPercent());
    const bool killable = LethalWith(target, 1, multiplier);
    const int nearbyTargets = Engine::CountEnemiesAt(aim, kWRadius);
    if (!farm && !WZoneWorthwhile(target.HealthPercent(), Rooted(target), killable,
                                  nearbyTargets, Slider(WMenu, "MinimumTargets", 1)))
        return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    return true;
}
inline bool CastWAtPosition(const Vector3& position, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid() || position.IsZero() ||
        player.Position().Distance2D(position) > kWRange ||
        PreserveAttack(false)) return false;
    if (!Engine::ControllerCastPosition(1, position)) return false;
    LastCastTick[1] = Now();
    return true;
}


inline ThreatKind ActiveThreat() {
    if (HardCcUntil > Now()) return ThreatKind::HardCrowdControl;
    if (ThreatUntil > Now()) return ThreatKind::Damage;
    return ThreatKind::None;
}

inline bool CastE(const AIHeroClient& ally, Mode mode, bool reactive = false,
                  bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || ProtectedAlly(ally) ||
        !AllyInShieldRange(player.Position(), ally.Position(), ally.BoundingRadius()) ||
        !Ready(2, mode) || !Throttle(2, 85) || PreserveAttack(reactive))
        return false;
    const ThreatKind threat = ActiveThreat();
    const bool allyThreatened = static_cast<int>(ally.NetworkId()) == ThreatAllyId;
    const ShieldThreatContext context{
        allyThreatened ? threat : ThreatKind::None,
        true,
        true,
        ally.HasBuff("MorganaE") || ally.HasBuff("BlackShield"),
        ally.HealthPercent() <= Slider(EMenu, "LowHealthThreshold", 48),
        allyThreatened && ally.HealthPercent() <= 30.0f,
        manual,
        Engine::CountEnemiesAt(ally.Position(), 700.0f)};
    if (!ShouldBlackShield(context)) return false;
    if (!Engine::ControllerCastUnit(2, ally)) return false;
    LastCastTick[2] = Now();
    BlackShieldActive = true;
    ThreatAllyId = static_cast<int>(ally.NetworkId());
    return true;
}

inline int NearbyTetherableEnemies() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy, kRRadius + enemy.BoundingRadius()) &&
            !ProtectedEnemy(enemy)) ++count;
    }
    return count;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 130) ||
        PreserveAttack(reactive) || RActive || !SafeRPosition(target, defensive))
        return false;
    const int nearby = NearbyTetherableEnemies();
    if (!defensive && nearby < Slider(RMenu, "MinimumTargets", 2)) return false;
    if (!defensive && (!Engine::ValidEnemy(target, kRRadius) ||
                       !RInsideTether(player.Position(), target.Position(),
                                      target.BoundingRadius()))) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    RStartedTick = Now();
    RTetherUntil = RStartedTick + static_cast<int>(kRTetherDuration * 1000.0f);
    RStunReadyTick = RStartedTick + static_cast<int>(kRStunDelay * 1000.0f);
    LastRTargetId = Engine::ValidEnemy(target) ? static_cast<int>(target.NetworkId()) : 0;
    RActive = true;
    RRecastPending = true;
    return true;
}

inline bool RecastSoulShackles(const AIHeroClient& target) {
    // Soul Shackles has an automatic delayed stun rather than a second user
    // button.  RecastPending models that branch so polling cannot issue a
    // fresh R while chains are still active; it clears after the stun window.
    const auto player = GameObjects::Player();
    if (!RActive || !player.IsValid() || Now() < RStunReadyTick ||
        Now() > RTetherUntil || !Engine::ValidEnemy(target)) return false;
    if (!RStunWillLand(player.Position(), target.Position(),
                       (RTetherUntil - Now()) / 1000.0f,
                       target.IsDashing(), ProtectedEnemy(target))) return false;
    RRecastPending = false;
    return true;
}

inline bool Automatic(const AIHeroClient& target) {
    const auto ally = SelectThreatAlly(true);
    if (Engine::ValidAlly(ally, kERange) &&
        (HardCcUntil > Now() || ThreatUntil > Now()) &&
        CastE(ally, Mode::Automatic, true, true)) return true;
    if (Engine::ValidEnemy(target) && HardCcUntil > Now() &&
        CastQ(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && ThreatUntil > Now() &&
        CastW(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && LethalWith(target, 0) &&
        CastQ(target, Mode::Automatic, true)) return true;
    if (CastR(target, Mode::Automatic, true, true)) return true;
    return false;
}

inline void Combo(const AIHeroClient& target) {
    const auto ally = SelectThreatAlly(false);
    if (Engine::ValidAlly(ally, kERange) &&
        ActiveThreat() != ThreatKind::None && CastE(ally, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo, false, false);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 55)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& pursuer) {
    const auto player = GameObjects::Player();
    if (player.IsValid() && player.HealthPercent() <= 55.0f) {
        ThreatAllyId = static_cast<int>(player.NetworkId());
        ThreatUntil = Now() + 250;
        HardCcUntil = Now() + 250;
        if (CastE(player, Mode::Flee, true, true)) return;
    }
    if (CastQ(pursuer, Mode::Flee, true)) return;
    (void)CastR(pursuer, Mode::Flee, true, true);
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 42)) return;
    if (mode == Mode::Jungle) {
        const auto monster = ControllerHelpers::SelectJungleTarget(kWRange);
        if (monster.IsValid()) {
            (void)CastWAtPosition(monster.Position(), mode);
        }
    }
    (void)Engine::TryFarm(mode);
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (!RActive || now > RTetherUntil + 450) {
        RActive = false;
        RRecastPending = false;
        RStartedTick = RTetherUntil = RStunReadyTick = 0;
    }
    if (RActive && (player.HasBuff("MorganaR") || player.HasBuff("morganar"))) {
        RTetherUntil = std::max(RTetherUntil, now + 250);
        RStunReadyTick = std::max(RStunReadyTick, now + 100);
    }
    if (!player.HasBuff("MorganaE") && now - LastCastTick[2] > 1300)
        BlackShieldActive = false;
    if (ThreatUntil < now) ThreatAllyId = 0;
    if (HardCcUntil < now) HardCcUntil = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = SelectEnemy(selected, mode == Mode::Flee ? 900.0f : kQRange);
    if (RRecastPending && Engine::ValidEnemy(target)) (void)RecastSoulShackles(target);
    if (mode == Mode::Automatic) { (void)Automatic(target); return true; }
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 900.0f)); break;
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
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
            LastCastTick[slot] = now;
            if (slot == 3) {
                RActive = true; RRecastPending = true; RStartedTick = now;
                RTetherUntil = now + static_cast<int>(kRTetherDuration * 1000.0f);
                RStunReadyTick = now + static_cast<int>(kRStunDelay * 1000.0f);
            }
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid) return;
    int targetId = static_cast<int>(args.TargetNetworkId);
    if (targetId == 0) targetId = static_cast<int>(args.Target.NetworkId);
    const auto ally = RawAllyHeroByNetworkId(targetId);
    if (Engine::ValidAlly(ally, kERange) || analysis.TargetsPlayer) {
        const auto player = GameObjects::Player();
        ThreatAllyId = analysis.TargetsPlayer && player.IsValid()
            ? static_cast<int>(player.NetworkId()) : targetId;
        ThreatUntil = std::max(ThreatUntil, now + 650);
        if (analysis.LikelyHardCrowdControl)
            HardCcUntil = std::max(HardCcUntil, now + 950);
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "MorganaR") ||
        Engine::TextContains(args.BuffName, "morganar")) {
        RActive = true; RRecastPending = true;
        RStartedTick = Now();
        RTetherUntil = Now() + static_cast<int>(kRTetherDuration * 1000.0f);
        RStunReadyTick = Now() + static_cast<int>(kRStunDelay * 1000.0f);
    }
    if (Engine::TextContains(args.BuffName, "MorganaE") ||
        Engine::TextContains(args.BuffName, "BlackShield")) BlackShieldActive = true;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "MorganaR") ||
        Engine::TextContains(args.BuffName, "morganar")) RActive = false;
    if (Engine::TextContains(args.BuffName, "MorganaE") ||
        Engine::TextContains(args.BuffName, "BlackShield")) BlackShieldActive = false;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF8155B8u, 1.5f, 42);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFB14B9Bu, 1.5f, 36);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs&) {}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("MorganaTactics", "Morgana binding and protection"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Dark Binding"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 55, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Tormented Shadow"));
    WMenu->Add(new MenuSlider("MinimumTargets", "Minimum W zone enemies", 1, 1, 5));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Black Shield"));
    EMenu->Add(new MenuSlider("LowHealthThreshold", "Protect low-health ally below %", 48, 15, 90));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Soul Shackles"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum tether enemies", 2, 1, 5));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies in R commit", 3, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("MorganaFarm", "Morgana farming"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum farm mana percent", 42, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("MorganaCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}

inline void OnLoad() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = 0;
    ThreatAllyId = ThreatUntil = HardCcUntil = 0;
    RStartedTick = RTetherUntil = RStunReadyTick = LastRTargetId = 0;
    RActive = RRecastPending = BlackShieldActive = false;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    RActive = RRecastPending = BlackShieldActive = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin Morgana mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Preserve selected enemy before orbwalker and selector fallback",
    "Aim Dark Binding with prediction, 70-width contact and projectile-wall rejection",
    "Stop Q at the first collision instead of treating it as a free line",
    "Never replace an ordinary attack windup with nonreactive Q or W",
    "Place Tormented Shadow inside the 325-radius zone and respect 900 range",
    "Increase W value as target missing health rises and gate empty zones",
    "Use W on rooted targets, lethal targets and jungle bodies without generic Q-W-E-R fallback",
    "Match Black Shield to a concrete ally target and incoming disabling threat",
    "Shield self or an ally only inside the 800 range and never overwrite an active shield",
    "Track enemy casts targeting allies and reconcile hard-CC threat polling",
    "Start Soul Shackles only with tetherable nearby enemies and a safe non-turret position",
    "Keep R tether and delayed stun state separate from fresh-cast readiness",
    "Do not issue a fresh R while the current tether or automatic stun window is active",
    "Automatic mode is defensive: anti-CC shield, peel binding, zone denial, then defensive R",
    "Combo prioritizes Q binding, missing-health W, ally shield and multi-target R",
    "Harass preserves mana and refuses low-value W zones",
    "Flee protects self, binds the pursuer and uses defensive shackles without turret diving",
    "LaneClear, Jungle and LastHit use W-specific objective placement before farm policy",
    "Respect real reach, target validity, spell shields, invulnerability and enemy count gates",
    "Yield after observed manual Q, W, E or R ownership",
    "Reconcile R, Black Shield and threat state from buffs, process-spell events and polling",
    "Expose Q and R geometry without changing gameplay decisions",
    "Assign every ChampionController callback, including object and missile lifecycle hooks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Morgana;
    controller.ControllerId = "champion.kuroaio.ai.morgana.catcher";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIMorgana.md";
    controller.ImplementationSummary =
        "Collision-aware Dark Binding, missing-health Tormented Shadow, threat-matched Black Shield and tether-safe Soul Shackles controller.";
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
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Morgana
