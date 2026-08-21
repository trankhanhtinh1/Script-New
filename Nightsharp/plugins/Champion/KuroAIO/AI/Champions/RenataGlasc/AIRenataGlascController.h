#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIRenataGlascGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::RenataGlasc {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
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

inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastEnemyThreatUntil = 0;
inline int LastHardCcThreatUntil = 0;
inline int QGrabbedTargetId = 0;
inline int QRecastExpireTick = 0;
inline int WTargetId = 0;
inline int WActiveUntil = 0;
inline int WDeathTick = 0;
inline int ELastDirectionTick = 0;
inline int RActiveUntil = 0;
inline Vector3 LastQEndpoint{};
inline Vector3 LastREndpoint{};
inline bool QRecastReady = false;
inline bool WReviveObserved = false;
inline int PassiveMarkedTargetId = 0;
inline int PassiveMarkExpireTick = 0;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::PlayerMobilityLocked;

inline bool Throttle(int slot, int delay = 45) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool ProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline AIHeroClient SelectEnemy(float range = kRRange) {
    return Engine::SelectTarget(range);
}

inline AIHeroClient AllyById(int id) {
    if (id == 0) return {};
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ally.IsValid() && static_cast<int>(ally.NetworkId()) == id) return ally;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (enemy.IsValid() && static_cast<int>(enemy.NetworkId()) == id) return enemy;
    }
    const auto player = GameObjects::Player();
    if (player.IsValid() && static_cast<int>(player.NetworkId()) == id) return player;
    return {};
}

inline AIHeroClient SelectAlly(bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, 800.0f) || ally.NetworkId() == player.NetworkId()) continue;
        const int nearby = Engine::CountEnemiesAt(ally.Position(), 700.0f);
        const float score = (100.0f - std::clamp(ally.HealthPercent(), 0.0f, 100.0f)) * 1.7f +
            std::max(0.0f, ally.TotalAttackDamage()) * 0.75f +
            std::max(0.0f, ally.AP()) * 0.45f +
            static_cast<float>(std::max(0, nearby)) * 240.0f;
        if (score > bestScore) { best = ally; bestScore = score; }
    }
    if (!best.IsValid() && defensive) best = player;
    return best;
}

inline bool Threatened(const AIHeroClient& ally) {
    return ally.IsValid() &&
        (Engine::CountEnemiesAt(ally.Position(), 700.0f) > 0 ||
         LastEnemyThreatUntil > Now());
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || (!reactive && PlayerMobilityLocked()) ||
        !Ready(0, mode) || !Throttle(0) || PreserveAttack(reactive)) return false;
    const int now = Now();
    if (QRecastReady && QGrabbedTargetId != 0 && now <= QRecastExpireTick) {
        const auto grabbed = AllyById(QGrabbedTargetId);
        if (!grabbed.IsValid() || !Engine::ValidEnemy(grabbed)) return false;
        const Vector3 desired = PredictPosition(grabbed, 0.20f);
        const Vector3 endpoint = QRecastEndpoint(grabbed.Position(), desired);
        if (!QRecastReachable(grabbed.Position(), endpoint) ||
            ControllerHelpers::ProjectileWallBlocks(
                grabbed.Position(), endpoint, kQWidth * 0.5f)) return false;
        if (!Engine::ControllerCastPosition(0, endpoint)) return false;
        LastQEndpoint = endpoint;
        LastCastTick[0] = now;
        QRecastReady = false;
        QGrabbedTargetId = 0;
        QRecastExpireTick = 0;
        return true;
    }
    if (!Engine::ValidEnemy(target, kQRange) || ProtectedTarget(target)) return false;
    const Vector3 predicted = PredictPosition(target, reactive ? 0.14f : 0.22f);
    const Vector3 endpoint = QEndpoint(player.Position(), predicted);
    if (endpoint.IsZero() || !QLineHits(player.Position(), endpoint, predicted, target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kQWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    LastQEndpoint = endpoint;
    LastCastTick[0] = now;
    QGrabbedTargetId = static_cast<int>(target.NetworkId());
    QRecastReady = true;
    QRecastExpireTick = now + 1500;
    return true;
}

inline bool CastW(const AIHeroClient& requested, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 75) || PreserveAttack(reactive)) return false;
    AIHeroClient ally = requested;
    if (!Engine::ValidAlly(ally, 800.0f)) ally = SelectAlly(true);
    if (!Engine::ValidAlly(ally, 800.0f)) return false;
    const bool threatened = Threatened(ally);
    const bool low = ally.HealthPercent() <= Slider(WMenu, "AllyHealth", 72);
    if (!reactive && !threatened && !low && mode != Mode::Combo) return false;
    if (ally.HasBuff("RenataW") || ally.HasBuff("RenataWSelf")) return false;
    if (!Engine::ControllerCastUnit(1, ally)) return false;
    WTargetId = static_cast<int>(ally.NetworkId());
    WActiveUntil = Now() + static_cast<int>(kWDurationSeconds * 1000.0f);
    WDeathTick = 0;
    WReviveObserved = false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || (!reactive && PlayerMobilityLocked()) ||
        !Ready(2, mode) || !Throttle(2) || PreserveAttack(reactive)) return false;
    AIHeroClient ally = SelectAlly(reactive || mode == Mode::Flee);
    const bool hasEnemy = Engine::ValidEnemy(target, kERange);
    if (!ally.IsValid() && !hasEnemy) return false;
    Vector3 aimTarget = ally.IsValid() ? ally.Position() : target.Position();
    if (hasEnemy && ally.IsValid()) {
        const Vector3 towardEnemy = PredictPosition(target, 0.18f);
        if (player.Position().Distance2D(towardEnemy) < player.Position().Distance2D(ally.Position())) aimTarget = towardEnemy;
    }
    if (ally.IsValid() && EShieldRaw(SpellRank(2), player.AP()) <= 0.0f) return false;
    const Vector3 endpoint = QEndpoint(player.Position(), aimTarget, kERange);
    if (endpoint.IsZero() || ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kEWidth * 0.5f)) return false;
    if (!reactive && ally.IsValid() && ally.HealthPercent() > Slider(EMenu, "ShieldHealth", 90) && !hasEnemy) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    ELastDirectionTick = Now();
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || (!reactive && PlayerMobilityLocked()) ||
        !Ready(3, mode) || !Throttle(3, 120) || PreserveAttack(reactive) ||
        !RHostileTargetAllowed(target.IsEnemy(), target.IsValid(),
                                target.IsInvulnerable(),
                                HasSpellShieldOrImmunity(target)) ||
        !Engine::ValidEnemy(target, kRRange)) return false;
    const float runtimeDamage = Engine::RuntimeSpells[3]
        ? Engine::RuntimeSpells[3]->GetDamage(target) : 0.0f;
    const bool lethal = runtimeDamage >= target.Health() + target.AllShield();
    const int enemies = Engine::CountEnemiesAt(player.Position(), 750.0f);
    const int allies = Engine::CountAlliesAt(player.Position(), 750.0f) + 1;
    const bool defensive = reactive || mode == Mode::Flee || mode == Mode::Automatic;
    if (!defensive && enemies < Slider(RMenu, "MinimumTargets", 2) && !lethal) return false;
    if (!RCommitSafe(std::max(1, enemies), allies, Engine::UnderEnemyTurret(player.Position()),
                     SDK::NavMesh::IsWall(player.Position()), Slider(RMenu, "MaximumEnemies", 3)) && !defensive) return false;
    const Vector3 predicted = PredictPosition(target, reactive ? 0.35f : 0.55f);
    const Vector3 endpoint = QEndpoint(player.Position(), predicted, kRRange);
    if (endpoint.IsZero() || !RLineHits(player.Position(), endpoint, predicted, target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kRWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    LastREndpoint = endpoint;
    LastCastTick[3] = Now();
    RActiveUntil = Now() + 2500;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    const auto ally = SelectAlly(false);
    if (ally.IsValid() && Threatened(ally) && CastW(ally, Mode::Combo, false)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (ally.IsValid() && CastW(ally, Mode::Combo, false)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 52)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& threat) {
    const auto player = GameObjects::Player();
    const auto ally = SelectAlly(true);
    if (ally.IsValid() && CastW(ally, Mode::Flee, true)) return;
    if (CastE(threat, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(threat) && CastQ(threat, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(threat)) (void)CastR(threat, Mode::Flee, true);
    (void)player;
}

inline void DefensiveAutomatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    const auto ally = SelectAlly(true);
    if (ally.IsValid() && (Threatened(ally) || ally.HealthPercent() <= Slider(TacticsMenu, "AllyHealth", 65)) &&
        CastW(ally, Mode::Automatic, true)) return;
    if (player.IsValid() && player.HealthPercent() <= Slider(TacticsMenu, "PlayerHealth", 42) &&
        CastW(player, Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(target) && LastHardCcThreatUntil > Now() && CastQ(target, Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(target) && LastEnemyThreatUntil > Now()) (void)CastR(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const int now = Now();
    if (WDeathTick > 0 &&
        !ReviveWindowOpen(static_cast<float>(now - WDeathTick) / 1000.0f)) {
        WReviveObserved = false;
    }
    if (QRecastReady && now > QRecastExpireTick) {
        QRecastReady = false;
        QGrabbedTargetId = 0;
    }
    if (PassiveMarkExpireTick > 0 && now > PassiveMarkExpireTick) {
        PassiveMarkedTargetId = 0;
        PassiveMarkExpireTick = 0;
    }
    if (WActiveUntil <= now) {
        WActiveUntil = 0;
        WTargetId = 0;
    }
    if (RActiveUntil <= now) RActiveUntil = 0;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("RenataR") || player.HasBuff("RenataRChannel"))
        RActiveUntil = std::max(RActiveUntil, now + 250);
    const auto bailout = AllyById(WTargetId);
    if (bailout.IsValid()) {
        if (bailout.HasBuff("RenataW") || bailout.HasBuff("RenataWSelf"))
            WActiveUntil = std::max(WActiveUntil, now + 250);
        if (bailout.IsDead() && WDeathTick == 0) {
            WDeathTick = now;
            WReviveObserved = ReviveWindowOpen(0.0f);
        }
        if (WDeathTick > 0 && !bailout.IsDead()) {
            WDeathTick = 0;
            WReviveObserved = false;
        }
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    (void)selected;
    ReconcileState();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    if (RActiveUntil > Now()) return true;
    const AIHeroClient target = SelectEnemy(kRRange);
    if (mode == Mode::Automatic) { DefensiveAutomatic(target); return true; }
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, kQRange)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (player.ManaPercent() >= Slider(FarmMenu, "Mana", 35)) (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
    case Mode::None:
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
            const int previousCastTick = LastCastTick[static_cast<std::size_t>(slot)];
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (slot == 0) {
                if (QRecastReady && now - previousCastTick > 250) {
                    QRecastReady = false;
                    QGrabbedTargetId = 0;
                } else {
                    QRecastReady = true;
                    QRecastExpireTick = now + 1500;
                }
            } else if (slot == 1) {
                WActiveUntil = now + static_cast<int>(kWDurationSeconds * 1000.0f);
            } else if (slot == 3) {
                RActiveUntil = now + 2500;
            }
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    LastEnemyThreatUntil = std::max(LastEnemyThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        LastHardCcThreatUntil = std::max(LastHardCcThreatUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>(args);
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    const int targetId = args.TargetNetworkId != 0
        ? static_cast<int>(args.TargetNetworkId)
        : static_cast<int>(args.Target.NetworkId);
    if (targetId != 0 && targetId == PassiveMarkedTargetId) {
        PassiveMarkedTargetId = 0;
        PassiveMarkExpireTick = 0;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = Now();
    if (Engine::TextContains(args.BuffName, "RenataPassiveDebuff")) {
        PassiveMarkedTargetId = static_cast<int>(args.Sender.NetworkId);
        PassiveMarkExpireTick = now + 4000;
    }
    if (Engine::TextContains(args.BuffName, "RenataQ") || Engine::TextContains(args.BuffName, "RenataQRoot")) {
        QRecastReady = true;
        QRecastExpireTick = now + 1500;
    }
    if (Engine::TextContains(args.BuffName, "RenataW")) WActiveUntil = std::max(WActiveUntil, now + 250);
    if (Engine::TextContains(args.BuffName, "RenataR")) RActiveUntil = std::max(RActiveUntil, now + 250);
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "RenataPassiveDebuff")) {
        PassiveMarkedTargetId = 0;
        PassiveMarkExpireTick = 0;
    }
    if (Engine::TextContains(args.BuffName, "RenataQ")) {
        QRecastReady = false;
        QGrabbedTargetId = 0;
    }
    if (Engine::TextContains(args.BuffName, "RenataR")) RActiveUntil = 0;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs&) {}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs&) {}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF7FBFFFu, 1.25f, 36);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF7FFFBBu, 1.25f, 36);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFB07FFFu, 1.5f, 36);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("RenataTactics", "Renata support tactics"));
    TacticsMenu->Add(new MenuSlider("AllyHealth", "Automatic ally health threshold", 65, 10, 95));
    TacticsMenu->Add(new MenuSlider("PlayerHealth", "Automatic player health threshold", 42, 10, 95));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Handshake and throw"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 52, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Bailout revive"));
    WMenu->Add(new MenuSlider("AllyHealth", "Bailout ally health threshold", 72, 20, 95));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Loyalty Program shield"));
    EMenu->Add(new MenuSlider("ShieldHealth", "Shield ally below health %", 90, 40, 100));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Hostile Takeover"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum combo enemies", 2, 1, 5));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at player", 3, 0, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("RenataFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("RenataCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E/R ranges", false));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = 0;
    LastEnemyThreatUntil = LastHardCcThreatUntil = 0;
    QGrabbedTargetId = QRecastExpireTick = WTargetId = WActiveUntil = WDeathTick = 0;
    ELastDirectionTick = RActiveUntil = PassiveMarkedTargetId = PassiveMarkExpireTick = 0;
    LastQEndpoint = LastREndpoint = {};
    QRecastReady = WReviveObserved = false;
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    LastQEndpoint = LastREndpoint = {};
    QRecastReady = WReviveObserved = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15 Renata metadata",
    "Apply Leverage mark damage to first champion attack and reconcile mark consumption",
    "Handshake uses predicted collision, line width, projectile wall and first-blocker gates",
    "Q recast throws only a confirmed grabbed target toward a reachable safe endpoint",
    "Bailout selects a threatened carry, tracks five-second steroid and three-second revive window",
    "Revive policy requires a takedown before the observed Bailout death timer expires",
    "Loyalty Program shields a concrete ally while its missile damages enemies on the line",
    "E rejects empty shielding, invalid targets, walls and unsafe turret positioning",
    "Hostile Takeover predicts a wide projectile and affects hostile targets only",
    "R requires bounded enemy density, ally follow-up and turret/wall safety",
    "R never treats allies as valid hostile takeover victims",
    "Automatic mode is defensive-only and reacts to observed ally/enemy threats",
    "Combo prioritizes bailout, handshake, loyalty shielding and safe multi-target R",
    "Harass preserves mana while using handshake and loyalty projectile selectively",
    "LaneClear Jungle and LastHit delegate only after Renata mana floor is met",
    "Flee bails out an ally, shields through E, handshakes pursuer and peels with R",
    "Reconcile Q recast, W bailout, R channel and threat windows through polling and events",
    "Expose complete load menu update draw spell buff attack object and missile callbacks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::RenataGlasc;
    controller.ControllerId = "champion.kuroaio.ai.renataglasc.support";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIRenataGlasc.md";
    controller.ImplementationSummary =
        "Leverage mark-aware support routing, Q handshake/recast displacement, Bailout revive-window tracking, "
        "line-shared E shield/damage and hostile-only R safety policy.";
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

    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<
        &QGrabbedTargetId, &LastQEndpoint, &QRecastExpireTick, 800, 900>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<
        &QGrabbedTargetId, &QRecastExpireTick, 900, 250, 5000>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::RenataGlasc
