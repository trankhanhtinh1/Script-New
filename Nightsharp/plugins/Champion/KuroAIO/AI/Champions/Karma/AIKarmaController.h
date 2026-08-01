#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIKarmaGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Karma {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
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
inline int LastAllyId = 0;
inline int WTargetId = 0;
inline int WStartTick = 0;
inline int WTetherExpireTick = 0;
inline int MantraArmedUntil = 0;
inline int MantraCooldownUntil = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline int LastQImpactTick = 0;
inline int LastShieldTick = 0;
inline bool TetherActive = false;
inline bool MantraReadyState = false;
inline bool PendingMantra = false;
inline Geometry::MantraPosture PendingPosture = Geometry::MantraPosture::RE;
inline Vector3 LastQAim{};

inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline bool IsThreatened(const AIHeroClient& ally, float healthThreshold = 72.0f) {
    return Engine::ValidAlly(ally) &&
        ally.HealthPercent() <= healthThreshold &&
        Engine::CountEnemiesAt(ally.Position(), 700.0f) > 0;
}

inline AIHeroClient SelectAlly(bool threatenedOnly = false) {
    const auto candidate = SelectProtectionAlly(
        kERange, LastAllyId, LastAllyId == 0 ? 0 : Now() + 300,
        threatenedOnly ? 360.0f : 210.0f, 440.0f);
    if (Engine::ValidAlly(candidate, kERange)) {
        if (!threatenedOnly || IsThreatened(candidate)) {
            LastAllyId = static_cast<int>(candidate.NetworkId());
            return candidate;
        }
    }
    const auto player = GameObjects::Player();
    if (player.IsValid() && (!threatenedOnly ||
        (player.HealthPercent() <= 72.0f &&
         Engine::CountEnemiesAt(player.Position(), 700.0f) > 0))) return player;
    return {};
}

inline float QDamage(const AIHeroClient& target, bool mantra) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const int rank = SpellRank(0);
    const float base = 10.0f + 50.0f * static_cast<float>(std::clamp(rank, 1, 5));
    const float raw = base + player.AP() * 0.70f +
        (mantra ? 40.0f + player.AP() * 0.30f : 0.0f);
    return player.CalculateMagicDamage(target, raw);
}

inline float WDamage(const AIHeroClient& target, bool mantra) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const int rank = SpellRank(1);
    const float raw = 15.0f + 25.0f * static_cast<float>(std::clamp(rank, 1, 5)) +
        player.AP() * 0.45f + (mantra ? 17.0f + player.MaxHealth() * 0.01f : 0.0f);
    return player.CalculateMagicDamage(target, raw);
}

inline bool AimQ(const AIHeroClient& target, bool mantra, Vector3& aim) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange)) return false;
    aim = PredictPosition(target, kQDelay);
    if (!aim.IsValid() || aim.IsZero() ||
        !WithinReach(player.Position(), aim, kQRange, target.BoundingRadius()) ||
        !QHits(player.Position(), aim, target.Position(), target.BoundingRadius(), mantra) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim,
            (mantra ? kQMantraWidth : kQWidth) * 0.5f)) return false;
    return true;
}

inline bool CastMantra(Mode mode, Geometry::MantraPosture posture,
                       bool reactive = false) {
    if (!Ready(3, mode) || !Throttle(3, 120) ||
        (!reactive && PreserveAttack(false))) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    MantraReadyState = false;
    PendingMantra = true;
    PendingPosture = posture;
    MantraArmedUntil = Now() + 1200;
    MantraCooldownUntil = Now() + 1200;
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool forceMantra = false) {
    const auto player = GameObjects::Player();
    const bool mantra = forceMantra;
    if (!player.IsValid() || Protected(target) || !Ready(0, mode) ||
        !Throttle(0) || PreserveAttack(reactive, reactive)) return false;
    Vector3 aim{};
    if (!AimQ(target, mantra, aim)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = LastQImpactTick = Now();
    LastQAim = aim;
    PendingMantra = false;
    PendingPosture = Geometry::MantraPosture::RE;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool forceMantra = false) {
    const auto player = GameObjects::Player();
    const bool mantra = forceMantra;
    if (!player.IsValid() || Protected(target) || !Engine::ValidEnemy(target, kWRange) ||
        !Ready(1, mode) || !Throttle(1, mantra ? 70 : 90) ||
        PreserveAttack(reactive, TetherActive && TetherRootWindow(
            player.Position(), target.Position(),
            static_cast<float>(Now() - WStartTick) / 1000.0f))) return false;
    if (TetherActive && WTargetId == static_cast<int>(target.NetworkId())) {
        const float elapsed = static_cast<float>(Now() - WStartTick) / 1000.0f;
        if (!TetherCanHold(player.Position(), PredictPosition(target, 0.1f), elapsed)) {
            TetherActive = false;
            WTargetId = WStartTick = WTetherExpireTick = 0;
            return false;
        }
        if (!TetherRootWindow(player.Position(), PredictPosition(target, 0.1f), elapsed) &&
            !reactive) return false;
        return false;
    }
    if (!Engine::ControllerCastUnit(1, target)) return false;
    LastCastTick[1] = WStartTick = Now();
    WTetherExpireTick = WStartTick + static_cast<int>(kWTetherSeconds * 1000.0f);
    WTargetId = static_cast<int>(target.NetworkId());
    TetherActive = true;
    PendingMantra = false;
    PendingPosture = Geometry::MantraPosture::RE;
    return true;
}

inline bool CastE(const AIHeroClient& ally, Mode mode, bool reactive = false,
                  bool forceMantra = false) {
    const auto player = GameObjects::Player();
    const bool mantra = forceMantra;
    if (!player.IsValid() || !Engine::ValidAlly(ally, kERange) ||
        !Ready(2, mode) || !Throttle(2, 80) || PreserveAttack(reactive, reactive)) return false;
    if (!WithinReach(player.Position(), ally.Position(), kERange, ally.BoundingRadius())) return false;
    const bool threatened = IsThreatened(ally);
    const int enemies = Engine::CountEnemiesAt(ally.Position(), kEAllyRadius);
    const int allies = Engine::CountAlliesAt(ally.Position(), kEAllyRadius);
    if (!SafeShieldDestination(ally.Position(), SDK::NavMesh::IsWall(ally.Position()),
            Engine::UnderEnemyTurret(ally.Position()) &&
                !Engine::UnderEnemyTurret(player.Position()), enemies, allies,
            reactive ? 4 : Slider(EMenu, "MaxShieldEnemies", 3))) return false;
    if (!reactive && !threatened && mode == Mode::Combo && !PendingMantra &&
        !ShieldSpeedWorthwhile(ally.HealthPercent(), true, enemies, 70.0f)) return false;
    if (mantra && !PendingMantra) return false;
    if (!Engine::ControllerCastUnit(2, ally)) return false;
    LastCastTick[2] = LastShieldTick = Now();
    LastAllyId = static_cast<int>(ally.NetworkId());
    PendingMantra = false;
    PendingPosture = Geometry::MantraPosture::RE;
    return true;
}

inline bool TryMantraRoute(const AIHeroClient& target, Mode mode,
                           bool reactive, bool allyThreatened) {
    const auto player = GameObjects::Player();
    const auto ally = SelectAlly(allyThreatened);
    const bool canQ = Engine::ValidEnemy(target, kQRange);
    const bool canW = Engine::ValidEnemy(target, kWRange);
    const bool tetherHeld = TetherActive && WTargetId == static_cast<int>(target.NetworkId());
    const MantraChoiceContext context{
        canQ && ControllerHelpers::Lethal(target, QDamage(target, true)),
        canW && (target.HealthPercent() <= 45.0f ||
                 ControllerHelpers::Lethal(target, WDamage(target, true))),
        allyThreatened, player.IsValid() && player.HealthPercent() <= 42.0f,
        Engine::ValidEnemy(target) && Engine::CountEnemiesAt(target.Position(), kQExplosionRadius) >= 2,
        tetherHeld, canQ, canW};
    const auto posture = PendingMantra ? PendingPosture : ChooseMantraPosture(context);
    if ((allyThreatened || PendingMantra) && Engine::ValidAlly(ally) &&
        (posture == MantraPosture::RE || reactive)) {
        if (PendingMantra && CastE(ally, mode, reactive, true)) return true;
        if (!PendingMantra && CastMantra(mode, MantraPosture::RE, reactive)) return true;
    }
    if (posture == MantraPosture::RQ && canQ) {
        if (PendingMantra && CastQ(target, mode, reactive, true)) return true;
        if (!PendingMantra && CastMantra(mode, MantraPosture::RQ, reactive)) return true;
    }
    if (posture == MantraPosture::RW && canW) {
        if (PendingMantra && CastW(target, mode, reactive, true)) return true;
        if (!PendingMantra && CastMantra(mode, MantraPosture::RW, reactive)) return true;
    }
    return false;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    const auto ally = SelectAlly(false);
    if (TryMantraRoute(target, Mode::Combo, false, false)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (Engine::ValidAlly(ally) && CastE(ally, Mode::Combo)) return;
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 52)) return;
    if (TryMantraRoute(target, Mode::Harass, false, false)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& pursuer) {
    const auto ally = SelectAlly(true);
    if (Engine::ValidAlly(ally) &&
        (TryMantraRoute(pursuer, Mode::Flee, true, true) || CastE(ally, Mode::Flee, true))) return;
    if (Engine::ValidEnemy(pursuer)) (void)CastW(pursuer, Mode::Flee, true);
}

inline bool AutomaticPeel(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    const auto ally = SelectAlly(true);
    const bool threatenedAlly = Engine::ValidAlly(ally) && IsThreatened(ally);
    const bool threatenedSelf = player.IsValid() && player.HealthPercent() <= 42.0f &&
        Engine::CountEnemiesAt(player.Position(), 650.0f) > 0;
    if (threatenedAlly || threatenedSelf) {
        if (TryMantraRoute(target, Mode::Automatic, true, true)) return true;
        if (Engine::ValidAlly(ally) && CastE(ally, Mode::Automatic, true)) return true;
    }
    if (Engine::ValidEnemy(target) && IncomingHardCcUntil > Now() &&
        CastW(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && ControllerHelpers::Lethal(target, QDamage(target, false)) &&
        CastQ(target, Mode::Automatic, true)) return true;
    return false;
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    MantraReadyState = Ready(3);
    if (PendingMantra && now > MantraArmedUntil + 250) {
        PendingMantra = false;
        PendingPosture = MantraPosture::RE;
    }
    if (TetherActive) {
        const auto target = HeroByNetworkId(WTargetId);
        const float elapsed = static_cast<float>(now - WStartTick) / 1000.0f;
        if (!Engine::ValidEnemy(target) || now > WTetherExpireTick + 180 ||
            !TetherCanHold(player.Position(), PredictPosition(target, 0.0f), elapsed)) {
            TetherActive = false;
            WTargetId = WStartTick = WTetherExpireTick = 0;
        }
    }
    if (player.HasBuff("KarmaMantra") || player.HasBuff("KarmaMantraReady")) {
        MantraReadyState = true;
        MantraCooldownUntil = 0;
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const auto target = ControllerHelpers::PreferredEnemyTarget(
        selected, mode == Mode::Flee ? 1000.0f : kQRange);
    if (ManualOwnershipUntil > Now()) return true;
    if (mode == Mode::Automatic && AutomaticPeel(target)) return true;
    if (mode == Mode::Combo) Combo(target);
    else if (mode == Mode::Harass) Harass(target);
    else if (mode == Mode::Flee) Flee(NearestEnemyToPlayer(target, 1000.0f));
    else if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        if (GameObjects::Player().IsValid() &&
            GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "Mana", 42)) {
            (void)Engine::TryFarm(mode);
        }
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot <= 3 && !Engine::WasControllerCast(slot)) {
            ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 600);
        }
        if (slot == 1 && !Engine::WasControllerCast(1)) {
            const auto target = HeroByNetworkId(static_cast<int>(args.TargetNetworkId));
            if (target.IsValid()) {
                WTargetId = static_cast<int>(target.NetworkId());
                WStartTick = now;
                WTetherExpireTick = now + 2000;
                TetherActive = true;
            }
        }
        if (slot == 3 && !Engine::WasControllerCast(3)) {
            PendingMantra = false;
            MantraReadyState = false;
            MantraCooldownUntil = now + 1200;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCcUntil = std::max(
        IncomingHardCcUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "KarmaMantra")) MantraReadyState = true;
    if (Engine::TextContains(args.BuffName, "KarmaSpiritBind")) TetherActive = true;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "KarmaSpiritBind")) {
        TetherActive = false;
        WTargetId = WStartTick = WTetherExpireTick = 0;
    }
    if (Engine::TextContains(args.BuffName, "KarmaMantra")) MantraReadyState = false;
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (args.NetworkId != 0) IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 550);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    if (args.NetworkId != 0) IncomingHardCcUntil = std::max(IncomingHardCcUntil, Now() + 650);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (args.Sender.IsValid() && IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTick = Now();
    }
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF9C40FFu, 1.4f, 40);
    Drawing::DrawCircle(player.Position(), kWRange, 0xFFFFAA40u, 1.2f, 32);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF66DDFFu, 1.2f, 32);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("KarmaOneTrick", "Karma Mantra tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 600, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Inner Flame"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 52, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Focused Resolve"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Inspire"));
    EMenu->Add(new MenuSlider("MaxShieldEnemies", "Maximum enemies at shield ally", 3, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Mantra posture"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("KarmaFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 42, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("KarmaCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/E ranges", false));
}

inline void ResetState() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = LastAllyId = WTargetId = 0;
    WStartTick = WTetherExpireTick = MantraArmedUntil = MantraCooldownUntil = 0;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingHardCcUntil = 0;
    LastQImpactTick = LastShieldTick = 0;
    TetherActive = PendingMantra = MantraReadyState = false;
    PendingPosture = MantraPosture::RE;
    LastQAim = {};
}
inline void OnLoad() { ResetState(); }
inline void OnUnload() {
    ResetState();
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Pin Karma spell values and Mantra variants to Riot 26.15 / CommunityDragon 16.15",
    "Track Mantra readiness and cooldown from R ownership, buffs and polling reconciliation",
    "Predict Inner Flame with 950 range, 902 speed, 90 width and projectile-wall rejection",
    "Use the empowered Q impact and 275 radius detonation only when the chosen posture reaches",
    "Track Spirit Bind target, 825 leash, two-second tether and root-window hold behavior",
    "Break tether state on target loss, leash break, expiry or observed buff removal",
    "Choose RQ for lethal/grouped Q, RW for held lethal tether and RE for threatened allies",
    "Shield the selected or protection-priority ally with real 600 range and 400 area safety",
    "Use RE ally shield speed for peel and reject wall, turret and over-committed destinations",
    "Preserve AA windup except reactive peel, root timing or verified lethal response",
    "Reconcile selected target before orbwalker fallback and honor manual ownership windows",
    "Automatic mode permits defensive RE/RW, hard-CC tether and verified Q kill secure only",
    "Combo prioritizes posture, Q poke, W tether and ally shield-speed follow-up",
    "Harass spends reserved mana on Q then W and avoids wasteful shield casts",
    "Flee shields a threatened ally/self with speed before tethering a pursuer",
    "LaneClear, Jungle and LastHit use shared farm policy only above Karma mana reserve",
    "Reject invulnerable, untargetable and spell-shielded enemy targets",
    "Record enemy process-spell and gapcloser threat windows for automatic peel",
    "Draw Q/W/E ranges without changing gameplay decisions",
    "Never automate items, summoners, flash or movement ownership",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Karma;
    controller.ControllerId = "champion.kuroaio.ai.karma.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIKarma.md";
    controller.ImplementationSummary =
        "Mantra cooldown and RQ/RW/RE posture tree with prediction Q, held Spirit Bind tether and ally shield-speed peel.";
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
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Karma
