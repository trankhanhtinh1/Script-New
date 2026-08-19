#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AILuluGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Lulu {

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
inline int RActiveUntil = 0;
inline int PixLinkedAllyId = 0;
inline int PixActiveUntil = 0;
inline int GapcloserTargetId = 0;
inline Vector3 GapcloserEndpoint{};
inline int GapcloserExpireTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline Vector3 PixPosition{};
inline Vector3 LastQEndpoint{};

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
using ControllerHelpers::PreserveAttack;

inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool ProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline AIHeroClient SelectEnemy(const AIHeroClient& selected,
                                float range = kQRange) {
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

inline Vector3 CurrentPixPosition(const AIHeroClient& player) {
    if (PixPosition.IsValid() && PixActiveUntil > Now()) return PixPosition;
    if (PixLinkedAllyId != 0) {
        for (const auto& ally : GameObjects::AllyHeroes()) {
            if (static_cast<int>(ally.NetworkId()) == PixLinkedAllyId &&
                Engine::ValidAlly(ally, kERange)) return ally.Position();
        }
    }
    return player.Position();
}

inline bool Threatened(const AIHeroClient& unit) {
    return Engine::ValidAlly(unit) &&
        (Engine::CountEnemiesAt(unit.Position(), 700.0f) > 0 ||
         LastEnemyThreatUntil > Now());
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange) ||
        ProtectedTarget(target) || !Ready(0, mode) || !Throttle(0) ||
        PreserveAttack(reactive)) return false;
    const Vector3 predicted = PredictPosition(target, reactive ? 0.18f : 0.25f);
    const Vector3 pix = CurrentPixPosition(player);
    const Vector3 playerEndpoint = BoltEndpoint(player.Position(), predicted);
    const Vector3 pixEndpoint = BoltEndpoint(pix, predicted);
    const bool playerHit = BoltHits(player.Position(), playerEndpoint, predicted,
                                    target.BoundingRadius());
    const bool pixHit = BoltHits(pix, pixEndpoint, predicted,
                                 target.BoundingRadius());
    if (!playerHit && !pixHit) return false;
    const Vector3 endpoint = playerHit ? playerEndpoint : pixEndpoint;
    if (endpoint.IsZero() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, kQWidth * 0.5f))
        return false;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    LastQEndpoint = endpoint;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& enemy, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive)) return false;
    const bool enemyThreat = Engine::ValidEnemy(enemy, kWRange) &&
        !ProtectedTarget(enemy);
    const auto ally = SelectAlly(true);
    const bool allyThreat = ally.IsValid() && Threatened(ally) &&
        ally.HealthPercent() <= Slider(TacticsMenu, "AllyHealth", 62);
    const WPosture posture = ChooseWPosture(
        enemyThreat, allyThreat, mode == Mode::Flee,
        LastHardCcThreatUntil > Now());
    if (posture == WPosture::Polymorph && enemyThreat) {
        if (!Engine::ControllerCastUnit(1, enemy)) return false;
        LastCastTick[1] = Now();
        return true;
    }
    if (posture != WPosture::Speed || (!allyThreat && mode != Mode::Flee)) return false;
    if (Engine::UnderEnemyTurret(ally.Position()) &&
        Engine::CountEnemiesAt(ally.Position(), 700.0f) > 0) return false;
    if (ally.IsValid() && ally.NetworkId() != player.NetworkId() &&
        Engine::ValidAlly(ally, kWRange)) {
        if (!Engine::ControllerCastUnit(1, ally)) return false;
    } else {
        if (!Engine::ControllerCastSelf(1)) return false;
    }
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& enemy, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2, 75) ||
        PreserveAttack(reactive)) return false;
    const auto ally = SelectAlly(true);
    if (ally.IsValid() && Engine::ValidAlly(ally, kERange)) {
        const int nearby = Engine::CountEnemiesAt(ally.Position(), 700.0f);
        const bool lethal = ally.HealthPercent() <= Slider(EMenu, "AllyHealth", 62) - 15.0f;
        const bool offensive = mode == Mode::Combo && !Threatened(ally);
        if (EShieldWorthwhile(ally.HealthPercent(), nearby, lethal, offensive,
                              static_cast<float>(Slider(EMenu, "ShieldHealth", 92)))) {
            if (Engine::ControllerCastUnit(2, ally)) {
                PixPosition = PixTransferPosition(player.Position(), ally.Position(), true);
                PixLinkedAllyId = static_cast<int>(ally.NetworkId());
                PixActiveUntil = Now() + 2200;
                SelectedAllyId = static_cast<int>(ally.NetworkId());
                LastCastTick[2] = Now();
                return true;
            }
        }
    }
    if (!Engine::ValidEnemy(enemy, kERange) || ProtectedTarget(enemy)) return false;
    const int nearbyAllies = Engine::CountAlliesAt(enemy.Position(), 500.0f);
    if (!EDamageWorthwhile(enemy.HealthPercent(),
                           LastEnemyThreatUntil > Now(), nearbyAllies)) return false;
    if (!Engine::ControllerCastUnit(2, enemy)) return false;
    PixPosition = enemy.Position();
    PixLinkedAllyId = 0;
    PixActiveUntil = Now() + 2200;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& threat, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 130) ||
        PreserveAttack(reactive)) return false;
    const auto ally = SelectAlly(true);
    const Vector3 center = ally.IsValid() ? ally.Position() : player.Position();
    const float allyHealth = ally.IsValid() ? ally.HealthPercent() : 100.0f;
    const int enemies = Engine::CountEnemiesAt(center, kRKnockupRadius);
    const int allies = Engine::CountAlliesAt(center, kRRange) + 1;
    const bool hardThreat = reactive || LastHardCcThreatUntil > Now();
    const bool defensive = mode == Mode::Automatic || mode == Mode::Flee || reactive;
    if (!WildGrowthWorthwhile(player.HealthPercent(), allyHealth, enemies,
                              hardThreat, static_cast<float>(Slider(RMenu, "PlayerHealth", 44)),
                              static_cast<float>(Slider(RMenu, "AllyHealth", 62)))) return false;
    if (!WildGrowthSafe(center, enemies, allies,
                        Engine::UnderEnemyTurret(center),
                        SDK::NavMesh::IsWall(center),
                        Slider(RMenu, "MaximumEnemies", 3))) return false;
    if (!defensive && enemies < Slider(RMenu, "MinimumTargets", 2)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RActiveUntil = Now() + 4200;
    LastCastTick[3] = Now();
    return true;
}

inline bool DefensiveAutomatic(const AIHeroClient& target) {
    const auto ally = SelectAlly(true);
    const bool allyThreat = ally.IsValid() && Threatened(ally) &&
        ally.HealthPercent() <= Slider(TacticsMenu, "AllyHealth", 62);
    const auto player = GameObjects::Player();
    const bool playerThreat = player.IsValid() &&
        player.HealthPercent() <= Slider(TacticsMenu, "PlayerHealth", 44) &&
        Engine::CountEnemiesAt(player.Position(), 700.0f) > 0;
    if (allyThreat && CastE(target, Mode::Automatic, true)) return true;
    if ((LastHardCcThreatUntil > Now() || playerThreat) &&
        CastW(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && (allyThreat || playerThreat) &&
        CastE(target, Mode::Automatic, true)) return true;
    if (allyThreat || playerThreat || LastEnemyThreatUntil > Now())
        return CastR(target, Mode::Automatic, true);
    return false;
}

inline void Combo(const AIHeroClient& target) {
    const auto ally = SelectAlly(false);
    if (ally.IsValid() && Threatened(ally) && CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 52)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (CastW(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& threat) {
    if (CastW(threat, Mode::Flee, true)) return;
    if (CastE(threat, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(threat)) (void)CastQ(threat, Mode::Flee, true);
    (void)CastR(threat, Mode::Flee, true);
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (RActiveUntil <= now) RActiveUntil = 0;
    if (PixActiveUntil <= now) {
        PixActiveUntil = 0;
        PixLinkedAllyId = 0;
        PixPosition = {};
    }
    if (player.HasBuff("LuluR") || player.HasBuff("LuluRActive"))
        RActiveUntil = std::max(RActiveUntil, now + 350);
    else if (RActiveUntil > now && now - LastCastTick[3] > 500)
        RActiveUntil = 0;
    if (player.HasBuff("LuluW"))
        ManualOwnershipUntil = std::max(ManualOwnershipUntil, now + 250);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    if (RActiveUntil > Now() || ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = SelectEnemy(selected, kQRange);
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
            if (slot == 2 && args.CastPosition.IsValid()) {
                PixPosition = args.CastPosition;
                PixActiveUntil = now + 1800;
            }
            if (slot == 3)
                RActiveUntil = std::max(RActiveUntil, now + 4200);
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
    if (Engine::TextContains(args.BuffName, "LuluR"))
        RActiveUntil = std::max(RActiveUntil, Now() + 350);
    if (Engine::TextContains(args.BuffName, "LuluE"))
        PixActiveUntil = std::max(PixActiveUntil, Now() + 1800);
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "LuluR")) RActiveUntil = 0;
    if (Engine::TextContains(args.BuffName, "LuluE")) PixActiveUntil = 0;
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFFF77D0u, 1.25f, 36);
    Drawing::DrawCircle(player.Position(), kERange, 0xFFB86BFFu, 1.25f, 36);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFF77D0FFu, 1.5f, 36);
    if (PixPosition.IsValid()) Drawing::DrawCircle(PixPosition, 28.0f, 0xFFFF77D0u, 1.0f, 18);
    if (LastQEndpoint.IsValid()) Drawing::DrawCircle(LastQEndpoint, kQWidth * 0.5f, 0xFFFF77D0u, 1.0f, 18);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("LuluTactics", "Lulu Pix tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("AllyHealth", "Automatic ally health threshold", 62, 10, 95));
    TacticsMenu->Add(new MenuSlider("PlayerHealth", "Automatic player health threshold", 44, 10, 95));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Glitterlance dual bolts"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 52, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Whimsy polymorph or speed"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Help Pix shield or damage"));
    EMenu->Add(new MenuSlider("ShieldHealth", "Shield when ally health below %", 92, 40, 100));
    EMenu->Add(new MenuSlider("AllyHealth", "Automatic ally health threshold", 62, 10, 95));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Wild Growth safety"));
    RMenu->Add(new MenuSlider("PlayerHealth", "Player health threshold", 44, 10, 90));
    RMenu->Add(new MenuSlider("AllyHealth", "Ally health threshold", 62, 10, 95));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies in knock-up zone", 3, 0, 5));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum combo knock-up targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("LuluFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("LuluCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E/R and Pix ranges", false));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = SelectedAllyId = 0;
    LastEnemyThreatUntil = LastHardCcThreatUntil = ManualOwnershipUntil = 0;
    RActiveUntil = PixActiveUntil = PixLinkedAllyId = 0;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    GapcloserEndpoint = {};
    PixPosition = LastQEndpoint = {};
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    PixPosition = LastQEndpoint = {};
    PixActiveUntil = PixLinkedAllyId = RActiveUntil = 0;
}

inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15 Lulu metadata",
    "Prefer selected enemy target, then orbwalker target, then engine fallback",
    "Track Pix position and linked ally through Help Pix transfer events and polling",
    "Model both Lulu and Pix Glitterlance bolt origins with prediction and target radius",
    "Reject Glitterlance through projectile walls and preserve auto-attack windup",
    "Use Whimsy polymorph against enemy threats and speed for ally rescue or flee",
    "Do not speed an ally into an unsafe turret or excessive enemy cluster",
    "Score shield targets by missing health, carry value, threat and selected continuity",
    "Use Help Pix offensively only for marked, low-health or multi-ally damage value",
    "Transfer Pix to the shielded ally or damaged enemy for the next Q dual bolts",
    "Require Wild Growth health safety and a meaningful ally or player threat",
    "Count only enemies inside the Wild Growth knock-up radius",
    "Reject Wild Growth at walls, enemy cap or unsupported enemy turret positions",
    "Suppress duplicate casts while Wild Growth state is observed by buff or polling",
    "Automatic mode reacts to ally health, enemy hard crowd control and nearby threats",
    "Combo prioritizes carry shield, polymorph, dual-bolt Q, offensive E and knock-up",
    "Harass preserves mana floor and avoids low-value polymorph or E damage",
    "LaneClear Jungle and LastHit delegate only after Lulu mana floor is met",
    "Flee uses Whimsy speed posture, shield transfer, Q peel and safe Wild Growth",
    "Yield manual spell ownership for a bounded window and reconcile missed events",
    "Expose complete load menu update draw spell buff attack and cast callbacks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Lulu;
    controller.ControllerId = "champion.kuroaio.ai.lulu.support";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AILulu.md";
    controller.ImplementationSummary =
        "Pix-aware dual-bolt Glitterlance ownership, threat-sensitive Whimsy polymorph/speed posture, "
        "Help Pix shield/damage transfer and health-safe Wild Growth knock-up.";
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

    controller.OnBeforeAttack =
        &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnGapcloser =
        &ControllerHelpers::CaptureGapcloserEvent<
            &GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick,
            800, 900>;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick, 900, 250, 5000>;
    controller.OnAfterAttack =
        &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast =
        &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Lulu
