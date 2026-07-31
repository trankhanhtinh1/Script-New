#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIJannaGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Janna {

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
inline int SelectedAllyId = 0;
inline int LastEnemyThreatUntil = 0;
inline int LastHardCcThreatUntil = 0;
inline int ManualOwnershipUntil = 0;
inline int QChargeStartedTick = 0;
inline int QLastReleaseTick = 0;
inline int RActiveUntil = 0;
inline Vector3 QDirection{};
inline Vector3 LastQEndpoint{};
inline Vector3 LastRPeelDirection{};
inline bool QCharging = false;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint{};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
using ControllerHelpers::PreserveAttack;

inline bool Throttle(int slot, int delay = 45) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool ProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline AIHeroClient SelectEnemy(const AIHeroClient& selected,
                                float range = kQMaxRange) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orb = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range)) return orb;
    return Engine::SelectTarget(range);
}

inline AIHeroClient SelectAlly(bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, kERange) ||
            ally.NetworkId() == player.NetworkId()) continue;
        const bool selected = static_cast<int>(ally.NetworkId()) == SelectedAllyId;
        const int nearby = Engine::CountEnemiesAt(ally.Position(), 700.0f);
        const float score = AllyPriority(ally.HealthPercent(),
                                         ally.TotalAttackDamage(), ally.AP(),
                                         nearby, selected);
        if (score > bestScore) {
            best = ally;
            bestScore = score;
        }
    }
    if (!best.IsValid() && defensive) return player;
    if (best.IsValid()) SelectedAllyId = static_cast<int>(best.NetworkId());
    return best;
}

inline bool Threatened(const AIHeroClient& unit) {
    return Engine::ValidAlly(unit) &&
        (Engine::CountEnemiesAt(unit.Position(), 700.0f) > 0 ||
         LastEnemyThreatUntil > Now());
}

inline bool QWallBlocked(const Vector3& endpoint) {
    return ControllerHelpers::ProjectileWallBlocksFromPlayer(
        endpoint, kQWidth * 0.5f);
}

inline bool CastQ(const AIHeroClient& target, Mode mode,
                  bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive)) return false;
    const int now = Now();
    const bool runtimeCharging = Engine::RuntimeSpells[0] &&
        Engine::RuntimeSpells[0]->IsCharging();
    if (runtimeCharging && !QCharging) {
        QCharging = true;
        QChargeStartedTick = now;
    }
    if (!QCharging) {
        if (!Engine::ValidEnemy(target, kQMaxRange) || ProtectedTarget(target)) return false;
        const Vector3 predicted = PredictPosition(target, 0.25f);
        const Vector3 direction = Direction2D(player.Position(), predicted);
        const Vector3 endpoint = QEndpoint(player.Position(), predicted, 0.0f);
        if (direction.IsZero() || endpoint.IsZero() || QWallBlocked(endpoint)) return false;
        if (!Engine::ControllerCastPosition(0, endpoint)) return false;
        QDirection = direction;
        QChargeStartedTick = now;
        QCharging = true;
        LastCastTick[0] = now;
        return true;
    }

    const float elapsed = std::max(0.0f,
        static_cast<float>(now - QChargeStartedTick) / 1000.0f);
    const bool hasTarget = Engine::ValidEnemy(target, kQMaxRange) &&
        !ProtectedTarget(target);
    const Vector3 predicted = hasTarget
        ? PredictPosition(target, 0.18f) : player.Position() + QDirection * kQMaxRange;
    const float distance = player.Position().Distance2D(predicted);
    const bool urgent = reactive || LastHardCcThreatUntil > now ||
        LastEnemyThreatUntil > now || InterruptExpireTick > now ||
        GapcloserExpireTick > now;
    if (!QShouldRelease(elapsed, distance, urgent)) return false;
    const Vector3 direction = QDirection.IsZero()
        ? Direction2D(player.Position(), predicted) : QDirection;
    const Vector3 endpoint = QEndpoint(player.Position(), player.Position() + direction,
                                       elapsed);
    if (endpoint.IsZero() || QWallBlocked(endpoint)) return false;
    if (hasTarget && !QHits(player.Position(), endpoint, predicted,
                            target.BoundingRadius())) return false;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    QLastReleaseTick = now;
    LastQEndpoint = endpoint;
    LastCastTick[0] = now;
    QCharging = false;
    QChargeStartedTick = 0;
    QDirection = {};
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode,
                  bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange) ||
        ProtectedTarget(target) || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive)) return false;
    const bool threat = LastEnemyThreatUntil > Now() ||
        Engine::CountEnemiesAt(player.Position(), 700.0f) > 0;
    if (!reactive && mode != Mode::Combo &&
        !WSlowWorthwhile(target.HealthPercent(), threat)) return false;
    if (!Engine::ControllerCastUnit(1, target)) return false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& ally, Mode mode,
                  bool reactive = false, bool offensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ally.IsValid() ||
        player.Position().Distance2D(ally.Position()) > kERange ||
        !Ready(2, mode) || !Throttle(2, 75) || PreserveAttack(reactive)) return false;
    const int nearby = Engine::CountEnemiesAt(ally.Position(), 700.0f);
    if (!ShieldWorthwhile(ally.HealthPercent(), nearby, offensive,
                          static_cast<float>(Slider(EMenu, "ShieldHealth", 92)))) return false;
    if (!Engine::ControllerCastUnit(2, ally)) return false;
    SelectedAllyId = static_cast<int>(ally.NetworkId());
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& threatTarget, Mode mode,
                  bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 120) ||
        PreserveAttack(reactive)) return false;
    const AIHeroClient ally = SelectAlly(true);
    const float allyHealth = ally.IsValid() ? ally.HealthPercent() : 100.0f;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRange);
    const int allies = Engine::CountAlliesAt(player.Position(), kRRange) + 1;
    const bool hardThreat = reactive || LastHardCcThreatUntil > Now();
    const bool defensive = mode == Mode::Automatic || mode == Mode::Flee || reactive;
    if (!MonsoonWorthwhile(player.HealthPercent(), allyHealth, enemies,
                           hardThreat, static_cast<float>(Slider(RMenu, "PlayerHealth", 40)),
                           static_cast<float>(Slider(RMenu, "AllyHealth", 55)))) return false;
    if (!MonsoonSafe(player.Position(), enemies, allies,
                     Engine::UnderEnemyTurret(player.Position()),
                     SDK::NavMesh::IsWall(player.Position()),
                     Slider(RMenu, "MaximumEnemies", 3))) return false;
    if (defensive && Engine::ValidEnemy(threatTarget) &&
        !CursorPeelSafe(player.Position(), Game::CursorPos(),
                        threatTarget.Position(),
                        static_cast<float>(Slider(RMenu, "CursorDot", 0)))) return false;
    if (!defensive && enemies < Slider(RMenu, "MinimumTargets", 2)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    RActiveUntil = Now() + static_cast<int>(kRDurationSeconds * 1000.0f);
    LastRPeelDirection = PeelDirection(player.Position(), Game::CursorPos());
    return true;
}

inline bool DefensiveAutomatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const auto ally = SelectAlly(true);
    const bool allyThreat = ally.IsValid() && Threatened(ally) &&
        ally.HealthPercent() <= Slider(TacticsMenu, "AllyHealth", 60);
    const bool playerThreat = player.HealthPercent() <=
        Slider(TacticsMenu, "PlayerHealth", 40) &&
        Engine::CountEnemiesAt(player.Position(), 700.0f) > 0;
    if (allyThreat && CastE(ally, Mode::Automatic, true, false)) return true;
    if ((LastHardCcThreatUntil > Now() || playerThreat) &&
        CastQ(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && (allyThreat || playerThreat) &&
        CastW(target, Mode::Automatic, true)) return true;
    if ((allyThreat || playerThreat || LastEnemyThreatUntil > Now()) &&
        CastR(target, Mode::Automatic, true)) return true;
    return false;
}

inline void Combo(const AIHeroClient& target) {
    const auto ally = SelectAlly(false);
    if (ally.IsValid() && Threatened(ally) && CastE(ally, Mode::Combo, false, false)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (ally.IsValid() && CastE(ally, Mode::Combo, false, true)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 52)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (CastW(target, Mode::Harass)) return;
    const auto ally = SelectAlly(false);
    if (ally.IsValid()) (void)CastE(ally, Mode::Harass, false, true);
}

inline void Flee(const AIHeroClient& threat) {
    const auto ally = SelectAlly(true);
    if (ally.IsValid() && CastE(ally, Mode::Flee, true, false)) return;
    if (Engine::ValidEnemy(threat) && CastQ(threat, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(threat) && CastW(threat, Mode::Flee, true)) return;
    (void)CastR(threat, Mode::Flee, true);
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (RActiveUntil <= now) RActiveUntil = 0;
    if (player.HasBuff("JannaR") || player.HasBuff("JannaRChannel"))
        RActiveUntil = std::max(RActiveUntil, now + 250);
    else if (RActiveUntil > now && now - LastCastTick[3] > 500)
        RActiveUntil = 0;
    const bool observedCharging = Engine::RuntimeSpells[0] &&
        Engine::RuntimeSpells[0]->IsCharging();
    if (observedCharging) {
        if (!QCharging) {
            QCharging = true;
            QChargeStartedTick = now;
        }
    } else if (QCharging && now - QLastReleaseTick > 350) {
        QCharging = false;
        QChargeStartedTick = 0;
        QDirection = {};
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    if (RActiveUntil > Now()) return true;
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = SelectEnemy(selected,
        mode == Mode::Flee ? kQMaxRange : kQMaxRange);
    if (mode == Mode::Automatic && DefensiveAutomatic(target)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, kWRange + 150.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (player.ManaPercent() >= Slider(FarmMenu, "Mana", 35))
            (void)Engine::TryFarm(mode);
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
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (slot == 0) {
                const auto player = GameObjects::Player();
                const Vector3 end = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
                    ? args.EndPosition : args.CastPosition;
                const Vector3 direction = Direction2D(player.Position(), end);
                if (!direction.IsZero()) QDirection = direction;
                if (Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsCharging()) {
                    QCharging = true;
                    QChargeStartedTick = now;
                } else if (QCharging) {
                    QCharging = false;
                    QLastReleaseTick = now;
                }
            } else if (slot == 3) {
                RActiveUntil = std::max(RActiveUntil,
                    now + static_cast<int>(kRDurationSeconds * 1000.0f));
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

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "JannaQ")) {
        QCharging = true;
        if (QChargeStartedTick <= 0) QChargeStartedTick = Now();
    }
    if (Engine::TextContains(args.BuffName, "JannaR"))
        RActiveUntil = std::max(RActiveUntil, Now() + 250);
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "JannaQ")) {
        QCharging = false;
        QChargeStartedTick = 0;
    }
    if (Engine::TextContains(args.BuffName, "JannaR")) RActiveUntil = 0;
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQMaxRange, 0xFF7FBFFFu, 1.25f, 36);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF7FFFBBu, 1.25f, 36);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFB07FFFu, 1.5f, 36);
    if (LastQEndpoint.IsValid())
        Drawing::DrawCircle(LastQEndpoint, kQWidth * 0.5f, 0xFF7FBFFFu, 1.0f, 20);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("JannaTactics", "Janna support tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("AllyHealth", "Automatic ally health threshold", 60, 10, 95));
    TacticsMenu->Add(new MenuSlider("PlayerHealth", "Automatic player health threshold", 40, 10, 95));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Howling Gale charge"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 52, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Zephyr slow"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Eye of the Storm shield/AD"));
    EMenu->Add(new MenuSlider("ShieldHealth", "Shield when ally health below %", 92, 40, 100));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Monsoon peel"));
    RMenu->Add(new MenuSlider("PlayerHealth", "Monsoon player health threshold", 40, 10, 90));
    RMenu->Add(new MenuSlider("AllyHealth", "Monsoon ally health threshold", 55, 10, 95));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies in zone", 3, 0, 5));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum combo enemies", 2, 1, 5));
    RMenu->Add(new MenuSlider("CursorDot", "Cursor peel alignment", 0, -100, 100));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("JannaFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("JannaCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E/R ranges", false));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = SelectedAllyId = 0;
    LastEnemyThreatUntil = LastHardCcThreatUntil = ManualOwnershipUntil = 0;
    QChargeStartedTick = QLastReleaseTick = RActiveUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    GapcloserEndpoint = {};
    QDirection = LastQEndpoint = LastRPeelDirection = {};
    QCharging = false;
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    QDirection = LastQEndpoint = LastRPeelDirection = {};
    QCharging = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15 Janna metadata",
    "Select explicit enemy before orbwalker fallback and preserve selected target",
    "Track Howling Gale initial cast, charge duration, direction and recast release",
    "Release Q at target reach or urgent interrupt timing with predicted position",
    "Reject Q through projectile walls and protected spell-shield targets",
    "Use Zephyr as a targeted slow only when poke, threat or disengage is worthwhile",
    "Score Eye of the Storm allies by missing health, AD/AP, threat and selection",
    "Shield defensively or grant offensive AD without wasting a full-health cast",
    "Track Monsoon channel state and stop issuing casts while its heal zone is active",
    "Require Monsoon zone safety for enemy density, wall and turret conditions",
    "Use cursor-aligned peel direction for defensive Monsoon knockback intent",
    "Automatic mode shields threatened allies and reacts to observed enemy CC",
    "Combo prioritizes ally shield, charged tornado, Zephyr slow and defensive Monsoon",
    "Harass preserves mana floor while charging Q and using Zephyr selectively",
    "Flee shields an ally, releases tornado, slows pursuer and peels with Monsoon",
    "LaneClear Jungle and LastHit delegate only after Janna mana floor is met",
    "Preserve auto-attack windup and yield ownership after manual spell casts",
    "Reconcile Q/R buffs and runtime charging state through polling and events",
    "Expose complete load menu update draw spell buff attack and cast callbacks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Janna";
    controller.ControllerId = "champion.kuroaio.ai.janna.support";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIJanna.md";
    controller.ImplementationSummary =
        "Charged Howling Gale reach control, threat-weighted Eye of the Storm ally scoring, "
        "targeted Zephyr slow and cursor-directed Monsoon peel/healing safety.";
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
    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnGapcloser =
        &ControllerHelpers::CaptureGapcloserEvent<
            &GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick,
            800, 900>;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick, 900, 250, 5000>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Janna
