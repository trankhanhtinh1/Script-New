#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AISorakaGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Soraka {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Now;
using ControllerHelpers::Ready;
using ControllerHelpers::PreserveAttack;

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
inline int EnemyThreatUntil = 0;
inline int HardCcThreatUntil = 0;
inline int RejuvenationUntil = 0;
inline int EquinoxUntil = 0;
inline int LastWishTick = 0;
inline Vector3 LastQImpact{};
inline bool QReturnPending = false;
inline int QReturnMissileId = 0;
inline Vector3 LastEZone{};

inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool ProtectedEnemy(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline AIHeroClient SelectEnemy(float range = kERange) {
    return Engine::SelectTarget(range);
}

inline AIHeroClient SelectAlly(bool includePlayer = false) {
    const auto player = GameObjects::Player();
    const auto ally = SelectProtectionAlly(kWRange);
    if (Engine::ValidAlly(ally, kWRange) &&
        (includePlayer || ally.NetworkId() != player.NetworkId())) return ally;
    if (includePlayer && player.IsValid()) return player;
    return {};
}

inline bool SafeCastPosition(const Vec3& position, int maximumEnemies = 2) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid()) return false;
    return SafeSupportZone(position, Engine::CountEnemiesAt(position, 275.0f),
                           Engine::UnderEnemyTurret(position),
                           SDK::NavMesh::IsWall(position), maximumEnemies);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || ProtectedEnemy(target) || !Ready(0, mode) ||
        !Throttle(0) || PreserveAttack(reactive)) return false;
    const Vec3 aim = PredictPosition(target, kQDelay);
    if (!QHits(player.Position(), aim, target.Position(), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQRadius * 0.25f))
        return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    LastQImpact = QImpactPoint(player.Position(), aim);
    RejuvenationUntil = Now() + static_cast<int>(kQRejuvenationSeconds * 1000.0f);
    QReturnPending = true;
    QReturnMissileId = 0;
    return true;
}

inline bool CastW(const AIHeroClient& ally, Mode mode, bool reactive = false,
                  bool emergency = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidAlly(ally, kWRange) ||
        ally.NetworkId() == player.NetworkId() || !Ready(1, mode) ||
        !Throttle(1, 85) || PreserveAttack(reactive)) return false;
    const float threshold = static_cast<float>(Slider(WMenu, "AllyHealthThreshold", 72));
    if (!emergency && ally.HealthPercent() > threshold) return false;
    const bool urgent = emergency || ally.HealthPercent() <=
        Slider(WMenu, "EmergencyHealth", 35);
    if (!CanPayWHealthCost(player.Health(), player.MaxHealth(),
                           kWHealthCostPercent, kWMinimumHealthPercent, urgent))
        return false;
    const int nearby = Engine::CountEnemiesAt(player.Position(), 420.0f);
    if (!urgent && (nearby > Slider(WMenu, "MaxSelfThreat", 2) ||
                    player.HealthPercent() <= Slider(WMenu, "SelfReserve", 18)))
        return false;
    if (!Engine::ControllerCastUnit(1, ally)) return false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool emergency = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || ProtectedEnemy(target) || !Ready(2, mode) ||
        !Throttle(2, 90) || PreserveAttack(reactive)) return false;
    const Vec3 aim = PredictPosition(target, 0.25f);
    if (!EZoneHits(aim, target.Position(), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 20.0f)) return false;
    const int maxEnemies = emergency ? 3 : Slider(EMenu, "MaxZoneEnemies", 2);
    if (!SafeCastPosition(aim, maxEnemies) && !emergency) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    EquinoxUntil = Now() + 1500 + static_cast<int>(kERootDelaySeconds * 1000.0f);
    LastEZone = aim;
    return true;
}

inline bool WishCandidate(const AIHeroClient& ally, bool& predictedLethal) {
    predictedLethal = false;
    if (!Engine::ValidAlly(ally) || ally.IsDead()) return false;
    const bool threatened = Engine::CountEnemiesAt(ally.Position(), 650.0f) > 0;
    predictedLethal = threatened && ally.HealthPercent() <=
        Slider(RMenu, "LethalHealth", 24);
    return ShouldWish(ally.HealthPercent(), predictedLethal, threatened,
                      static_cast<float>(Slider(RMenu, "SaveHealth", 34)));
}

inline AIHeroClient LowestSaveAlly(bool& predictedLethal) {
    const auto player = GameObjects::Player();
    AIHeroClient best{};
    float score = -1.0f;
    bool bestLethal = false;
    auto consider = [&](const AIHeroClient& ally) {
        bool lethal = false;
        if (!WishCandidate(ally, lethal)) return;
        const float urgency = (100.0f - ally.HealthPercent()) +
            (lethal ? 150.0f : 0.0f) +
            static_cast<float>(Engine::CountEnemiesAt(ally.Position(), 650.0f)) * 18.0f;
        if (!best.IsValid() || urgency > score) {
            best = ally;
            score = urgency;
            bestLethal = lethal;
        }
    };
    if (player.IsValid()) consider(player);
    for (const auto& ally : GameObjects::AllyHeroes()) consider(ally);
    predictedLethal = bestLethal;
    return best;
}

inline bool CastR(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 140) ||
        PreserveAttack(reactive)) return false;
    bool lethal = false;
    const auto ally = LowestSaveAlly(lethal);
    if (!Engine::ValidAlly(ally) || !ShouldWish(ally.HealthPercent(), lethal,
            Engine::CountEnemiesAt(ally.Position(), 650.0f) > 0,
            static_cast<float>(Slider(RMenu, "SaveHealth", 34)))) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = LastWishTick = Now();
    return true;
}

inline bool DefensiveAutomatic(const AIHeroClient& selected) {
    const auto ally = SelectAlly(true);
    if (Engine::ValidAlly(ally) && ally.NetworkId() !=
            GameObjects::Player().NetworkId() && CastW(ally, Mode::Automatic, true, true))
        return true;
    if (CastR(Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(selected) && HardCcThreatUntil >= Now() &&
        CastE(selected, Mode::Automatic, true, true)) return true;
    if (Engine::ValidEnemy(selected) && EnemyThreatUntil >= Now() &&
        CastQ(selected, Mode::Automatic, true)) return true;
    return false;
}

inline void Combo(const AIHeroClient& target) {
    const auto ally = SelectAlly(false);
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Combo)) return;
    if (Engine::ValidEnemy(target) && CastQ(target, Mode::Combo)) return;
    if (Engine::ValidAlly(ally) && CastW(ally, Mode::Combo)) return;
    (void)CastR(Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() <
        Slider(QMenu, "HarassMana", 52)) return;
    if (Engine::ValidEnemy(target) && CastQ(target, Mode::Harass)) return;
    if (Engine::ValidEnemy(target)) (void)CastE(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& pursuer) {
    if (CastR(Mode::Flee, true)) return;
    const auto ally = SelectAlly(false);
    if (Engine::ValidAlly(ally) && CastW(ally, Mode::Flee, true, true)) return;
    if (Engine::ValidEnemy(pursuer)) (void)CastE(pursuer, Mode::Flee, true, true);
}

inline void ReconcileState() {
    const int now = Now();
    if (RejuvenationUntil <= now) RejuvenationUntil = 0;
    if (EquinoxUntil <= now) EquinoxUntil = 0;
    if (QReturnPending && LastCastTick[0] > 0 &&
        now - LastCastTick[0] > 2800) {
        QReturnPending = false;
        QReturnMissileId = 0;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("SorakaQRegen")) RejuvenationUntil = std::max(RejuvenationUntil, now + 150);
    if (player.HasBuff("SorakaE")) EquinoxUntil = std::max(EquinoxUntil, now + 150);
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    ReconcileState();
    const auto target = SelectEnemy(mode == Mode::Flee ? 950.0f : kERange);
    if (mode == Mode::Automatic && DefensiveAutomatic(target)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 950.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: {
        const auto player = GameObjects::Player();
        if (player.IsValid() && player.ManaPercent() >= Slider(FarmMenu, "Mana", 40))
            (void)Engine::TryFarm(mode);
        break;
    }
    case Mode::Automatic: break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) LastCastTick[slot] = now;
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    EnemyThreatUntil = std::max(EnemyThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        HardCcThreatUntil = std::max(HardCcThreatUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot == 0) RejuvenationUntil = Now() + 2500;
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "SorakaQRegen"))
        RejuvenationUntil = std::max(RejuvenationUntil, Now() + 150);
    if (Engine::TextContains(args.BuffName, "SorakaE"))
        EquinoxUntil = std::max(EquinoxUntil, Now() + 150);
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "SorakaQRegen")) RejuvenationUntil = 0;
    if (Engine::TextContains(args.BuffName, "SorakaE")) EquinoxUntil = 0;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline bool IsSorakaQMissile(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::MissileEventIsLocal(args) &&
        ControllerHelpers::AnyTextContains(
            {args.SpellName, args.MissileName, args.Sender.Name},
            {"SorakaQ", "Soraka_Q", "QReturnMissile"});
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!IsSorakaQMissile(args)) return;
    QReturnPending = true;
    QReturnMissileId = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (id == QReturnMissileId) {
        QReturnPending = false;
        QReturnMissileId = 0;
    }
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF9BE7FFu, 1.5f, 36);
    Drawing::DrawCircle(player.Position(), kERange, 0xFFB879FFu, 1.0f, 36);
    if (LastEZone.IsValid() && !LastEZone.IsZero())
        Drawing::DrawCircle(LastEZone, kERadius, 0xFFCC66CCu, 1.0f, 30);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("SorakaTactics", "Soraka support tactics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Starcall rejuvenation"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 52, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Astral Infusion health gate"));
    WMenu->Add(new MenuSlider("AllyHealthThreshold", "Heal ally below health %", 72, 20, 95));
    WMenu->Add(new MenuSlider("EmergencyHealth", "Emergency heal below health %", 35, 10, 70));
    WMenu->Add(new MenuSlider("SelfReserve", "Self health reserve %", 18, 5, 50));
    WMenu->Add(new MenuSlider("MaxSelfThreat", "Max enemies for normal W", 2, 0, 5));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Equinox silence and root"));
    EMenu->Add(new MenuSlider("MaxZoneEnemies", "Maximum enemies at zone", 2, 0, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Wish global ally save"));
    RMenu->Add(new MenuSlider("SaveHealth", "Save ally below health %", 34, 10, 70));
    RMenu->Add(new MenuSlider("LethalHealth", "Lethal threat health %", 24, 5, 50));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("SorakaFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 40, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("SorakaCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E ranges", false));
}

inline void OnLoad() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = 0;
    EnemyThreatUntil = HardCcThreatUntil = 0;
    QReturnPending = false;
    QReturnMissileId = 0;
    LastQImpact = LastEZone = {};
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    LastQImpact = LastEZone = {};
    QReturnPending = false;
    QReturnMissileId = 0;
}

inline constexpr const char* Scenarios[] = {
    "Use the autonomous engine-selected enemy and select a vulnerable non-self ally",
    "Predict Starcall impact, check circle contact and reject projectile-wall shots",
    "Track Q return missile and keep rejuvenation state through buff and polling reconciliation",
    "Use Astral Infusion only on an ally below threshold with a real self-health reserve",
    "Permit emergency W only when the health cost cannot kill Soraka and avoid self-harm",
    "Place Equinox on predicted committed enemies for silence then delayed root",
    "Reject Equinox through walls, under unsafe turrets or over configured enemy density",
    "Evaluate Wish globally across every ally and amplify the low-health save window",
    "Use enemy cast analysis to distinguish lethal and hard-CC automatic saves",
    "Automatic mode is defensive and never starts a fresh poke or engage",
    "Combo prioritizes Equinox peel, Starcall rejuvenation, ally infusion and Wish save",
    "Harass spends Starcall before Equinox and never burns W as damage logic",
    "Flee prioritizes global Wish, then safe ally infusion and Equinox pursuer zone",
    "LaneClear Jungle and LastHit delegate to farm policy after a support mana reserve",
    "Preserve autonomous routing and basic-attack windup safety",
    "Use the autonomous engine-selected enemy and preserve basic-attack windup",
    "Reconcile observed Q W E R casts and support state through polling",
    "Draw Q/E ranges and last Equinox zone without changing decisions",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Soraka;
    controller.ControllerId = "champion.kuroaio.ai.soraka.support";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISoraka.md";
    controller.ImplementationSummary =
        "Q rejuvenation return tracking, health-cost-gated ally W, delayed Equinox root and global Wish saves.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Soraka
