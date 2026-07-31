#pragma once

#include "../AIChampionEngine.h"
#include "../AIMarksmanControllerHelpers.h"
#include "AISivirGeometry.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Sivir {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* BoomerangMenu = nullptr;
inline Menu* RicochetMenu = nullptr;
inline Menu* ShieldMenu = nullptr;
inline Menu* HuntMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline int ShieldActiveUntil = 0;
inline int QMissileId = 0;
inline int QLastSeenTick = 0;
inline int QCastTick = 0;
inline bool QActive = false;
inline bool QReturning = false;
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

using ControllerHelpers::Ready;

inline bool CanCast(int index, Mode mode, bool reactive = false) {
    if (!Ready(index, mode) || !HasCurrentResource(SpellCost(index)) ||
        !Engine::CanAct(reactive) || !CastThrottlePassed(
            index == 0 ? LastQCastTick : index == 1 ? LastWCastTick :
            index == 2 ? LastECastTick : LastRCastTick,
            reactive ? 15 : 55)) return false;
    if (!reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true) && index != 1) {
        return false;
    }
    return true;
}

inline bool ShieldBuffActive() {
    const auto player = GameObjects::Player();
    return player.IsValid() && (player.HasBuff("SivirE") ||
        ShieldActiveUntil > Now());
}

inline bool QName(const char* spell, const char* missile) {
    return Engine::TextContains(spell, "sivirq") ||
           Engine::TextContains(missile, "sivirq") ||
           Engine::TextContains(missile, "boomerang");
}

inline bool QReturnName(const char* spell, const char* missile) {
    return Engine::TextContains(spell, "return") ||
           Engine::TextContains(missile, "return") ||
           Engine::TextContains(missile, "sivirqmissileback");
}

inline void ReconcileQState() {
    if (!QActive) return;
    if (Now() - QLastSeenTick > 1800 ||
        (QReturning && Now() - QLastSeenTick > 750)) {
        QActive = false;
        QReturning = false;
        QMissileId = 0;
        QCastTick = 0;
    }
}

inline void RefreshThreatState() {
    if (IncomingThreatUntil < Now()) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
    }
    if (GapcloserExpireTick < Now()) {
        GapcloserTargetId = 0;
        GapcloserEndpoint = {};
    }
    if (ShieldActiveUntil < Now()) ShieldActiveUntil = 0;
}

inline AIHeroClient SelectTarget(const AIHeroClient& preferred,
                                 float range = kQMaximumRange) {
    if (Engine::ValidEnemy(preferred, range)) return preferred;
    const auto selected = ControllerHelpers::PlayerSelectedEnemy(range);
    if (Engine::ValidEnemy(selected, range)) return selected;
    return NearestEnemyToPlayer(preferred, range);
}

inline int NearbyUniqueTargets(const AIHeroClient& primary, float range) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range) ||
            enemy.NetworkId() == primary.NetworkId() ||
            HasSpellShieldOrImmunity(enemy)) continue;
        ++count;
    }
    return count;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool manual = false) {
    if (!CanCast(0, mode, manual) || QActive ||
        !Engine::ValidEnemy(target, kQMaximumRange + 90.0f)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    SDK::PredictionOutput prediction{};
    if (!PredictionHits(0, target, SDK::HitChance::High, false, &prediction) ||
        !prediction.GetCastPosition().IsValid() ||
        !prediction.CollisionObjects.empty()) return false;

    const Vector3 cast = prediction.GetCastPosition();
    const Vec3 origin{player.Position().x, player.Position().y,
                      player.Position().z};
    const Vec3 endpoint{cast.x, cast.y, cast.z};
    const Vec3 targetPosition{target.Position().x, target.Position().y,
                              target.Position().z};
    BoomerangContext context{};
    context.TargetValid = true;
    context.OutgoingCollisionFree = true;
    context.ReturnPathAvailable = ReturnHits(
        origin, endpoint, targetPosition, target.BoundingRadius());
    context.TargetEscaping = target.IsDashing();
    context.OutsideAttackRange = !InAutoAttackRange(target);
    context.Lethal = SpellDamage(0, target) >=
        target.Health() + target.AllShield();
    context.Manual = manual;
    if (!ShouldThrowBoomerang(context)) return false;

    if (!Engine::ControllerCastPosition(0, cast)) return false;
    LastQCastTick = QCastTick = Now();
    QLastSeenTick = Now();
    QActive = true;
    QReturning = false;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool attackIntent) {
    if (!CanCast(1, mode, attackIntent) ||
        !Engine::ValidEnemy(target, 900.0f) ||
        !InAutoAttackRange(target) || !OrbwalkerAttackRoute(target) ||
        HasSpellShieldOrImmunity(target)) return false;
    const bool lastHit = mode == Mode::LastHit;
    const RicochetContext context{
        true, true, NearbyUniqueTargets(target, 900.0f) > 0,
        true, true, PlayerManaPercent() >=
            static_cast<float>(Slider(RicochetMenu, "ManaPercent", 42)), lastHit};
    if (!ShouldCastRicochet(context)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastWCastTick = Now();
    (void)SetTemporaryOrbwalkerFocus(
        target, 850.0f, 900, OwnedFocusTargetId, OwnedFocusUntil);
    return true;
}

inline bool CastE(Mode mode, bool reactive) {
    if (!CanCast(2, mode, reactive) || ShieldBuffActive() ||
        IncomingThreatUntil <= Now() ||
        !ShieldTimingAllowed(static_cast<float>(IncomingThreatUntil - Now()),
                             kShieldWindowMs, kShieldReactionMs, false, true)) {
        return false;
    }
    if (!Engine::ControllerCastSelf(2)) return false;
    LastECastTick = Now();
    ShieldActiveUntil = Now() + static_cast<int>(kShieldWindowMs);
    IncomingThreatUntil = 0;
    IncomingThreatTargetId = 0;
    return true;
}

inline RPosture ResolvePosture(Mode mode) {
    if (mode == Mode::Flee) return RPosture::Disengage;
    if (GapcloserExpireTick >= Now() || IncomingThreatUntil > Now()) {
        return RPosture::Peel;
    }
    return mode == Mode::Combo ? RPosture::Engage : RPosture::Hold;
}

inline bool CastR(Mode mode, bool manual = false) {
    if (!CanCast(3, mode, manual)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 900.0f);
    const int allies = CountAlliedFollowup(player.Position(), 1050.0f, true);
    const bool threatened = player.HealthPercent() <=
        Slider(HuntMenu, "ThreatHealth", 48) ||
        GapcloserExpireTick >= Now() || IncomingThreatUntil > Now();
    RContext context{};
    context.Posture = ResolvePosture(mode);
    context.Ready = true;
    context.HasMana = HasCurrentResource(SpellCost(3));
    context.PlayerThreatened = threatened;
    context.AlliesMovingWithPlayer = allies > 0 || mode == Mode::Flee;
    context.AlliedFollowup = allies > 0;
    context.EnemyCommitment = GapcloserExpireTick >= Now();
    context.Manual = manual;
    context.NearbyAllies = allies;
    context.NearbyEnemies = enemies;
    if (!ShouldCastOnTheHunt(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastRCastTick = Now();
    return true;
}

inline bool TryReactiveShield() {
    if (IncomingThreatUntil <= Now()) return false;
    return CastE(Mode::Automatic, true);
}

inline bool TryManualUltimate() {
    return ManualUltimatePressed() && CastR(Mode::Automatic, true);
}

inline bool TryCombat(const AIHeroClient& preferred, Mode mode) {
    const auto target = SelectTarget(preferred);
    if (!Engine::ValidEnemy(target, kQMaximumRange + 90.0f)) return false;
    if (CastW(target, mode, false)) return true;
    return CastQ(target, mode);
}

inline bool TryFlee(const AIHeroClient& preferred) {
    if (TryReactiveShield()) return true;
    const auto threat = NearestEnemyToPlayer(preferred, 950.0f);
    if (Engine::ValidEnemy(threat, 950.0f) && CastR(Mode::Flee)) return true;
    return false;
}

inline bool TryFarm(Mode mode) {
    const bool lastHit = mode == Mode::LastHit;
    const bool jungle = mode == Mode::Jungle;
    if (Ready(0, mode) && PlayerManaPercent() >=
        static_cast<float>(Slider(BoomerangMenu,
            jungle ? "JungleMana" : "ClearMana", 35)) &&
        Engine::TryFarmSpell(0, jungle, lastHit)) return true;
    return Engine::TryFarmSpell(1, jungle, lastHit);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    RefreshThreatState();
    ReconcileQState();
    if (TryReactiveShield()) return true;
    if (TryManualUltimate()) return true;
    if (mode == Mode::Flee) return TryFlee(preferred);
    if (mode == Mode::Combo || mode == Mode::Harass || mode == Mode::Automatic) {
        if (mode == Mode::Combo && CastR(mode)) return true;
        const auto target = SelectTarget(preferred);
        if (Engine::ValidEnemy(target) && TryCombat(target, mode)) return true;
        return mode == Mode::Automatic && CastR(mode);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return TryFarm(mode);
    return false;
}

inline void ObserveLocalSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack) return;
        if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
            SpellEventNameContainsAny(args, {"sivirq", "boomerang"})) {
            LastQCastTick = QCastTick = now;
            QLastSeenTick = now;
            QActive = true;
            QReturning = false;
        } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
                   SpellEventNameContainsAny(args, {"sivirw", "ricochet"})) {
            LastWCastTick = now;
        } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
                   SpellEventNameContainsAny(args, {"sivire", "spellshield"})) {
            LastECastTick = now;
            ShieldActiveUntil = now + static_cast<int>(kShieldWindowMs);
        } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
                   SpellEventNameContainsAny(args, {"sivirr", "onthehunt"})) {
            LastRCastTick = now;
        }
        return;
    }
    const auto player = GameObjects::Player();
    const std::uint32_t playerTeam =
        static_cast<std::uint32_t>(player.Team());
    if (!player.IsValid() || !args.Sender.IsValid() ||
        args.Sender.Team == playerTeam ||
        args.IsAutoAttack || player.Position().Distance2D(args.Sender.Position) >
            1200.0f) return;
    IncomingThreatTargetId = static_cast<int>(args.Sender.NetworkId);
    IncomingThreatUntil = std::max(IncomingThreatUntil, now + 450);
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "sivire")) {
        ShieldActiveUntil = std::max(ShieldActiveUntil,
            ControllerHelpers::BuffExpireTick(args, static_cast<int>(kShieldWindowMs)));
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "sivire")) {
        ShieldActiveUntil = 0;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || !args.Target.IsHero()) return;
    const AIHeroClient target(args.Target.Handle());
    if (LastMode == Mode::Combo || LastMode == Mode::Harass) {
        (void)CastW(target, LastMode, true);
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    const auto target = HeroByNetworkId(LastAfterAttackTargetId);
    if (Engine::ValidEnemy(target, 900.0f)) {
        (void)SetTemporaryOrbwalkerFocus(
            target, 850.0f, 850, OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint,
                           GapcloserExpireTick, 700.0f, 950);
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 400);
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) || !QName(args.SpellName, args.MissileName)) return;
    QActive = true;
    QReturning = QReturnName(args.SpellName, args.MissileName);
    QMissileId = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    QLastSeenTick = Now();
    if (QCastTick == 0) QCastTick = Now();
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) || !QName(args.SpellName, args.MissileName)) return;
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (QMissileId == 0 || id == QMissileId || QReturnName(args.SpellName, args.MissileName)) {
        if (QReturning || QReturnName(args.SpellName, args.MissileName)) {
            QActive = false;
            QReturning = false;
            QMissileId = 0;
            QCastTick = 0;
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("SivirMechanics", "Sivir Mechanics"));
    BoomerangMenu = TacticsMenu->AddSubMenu(new Menu("Boomerang", "Boomerang Blade"));
    BoomerangMenu->Add(new MenuSlider("ClearMana", "Q clear mana (%)", 35, 10, 90));
    BoomerangMenu->Add(new MenuSlider("JungleMana", "Q jungle mana (%)", 24, 5, 90));
    RicochetMenu = TacticsMenu->AddSubMenu(new Menu("Ricochet", "Ricochet target policy"));
    RicochetMenu->Add(new MenuSlider("ManaPercent", "Minimum W mana (%)", 42, 10, 90));
    ShieldMenu = TacticsMenu->AddSubMenu(new Menu("SpellShield", "Spell Shield timing"));
    ShieldMenu->Add(new MenuSeparator("ImpactWindow", "Cast E only inside a verified impact window"));
    HuntMenu = TacticsMenu->AddSubMenu(new Menu("OnTheHunt", "Ally movement and engage posture"));
    HuntMenu->Add(new MenuSlider("ThreatHealth", "R self-peel health (%)", 48, 10, 80));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    IncomingThreatUntil = IncomingThreatTargetId = 0;
    ShieldActiveUntil = 0;
    QMissileId = QLastSeenTick = QCastTick = 0;
    QActive = QReturning = false;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = BoomerangMenu = RicochetMenu = ShieldMenu = HuntMenu = nullptr;
    QActive = QReturning = false;
    QMissileId = QCastTick = QLastSeenTick = 0;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Anchor Q to the selected or orbwalker target and reject collision objects",
    "Evaluate Q outgoing and reverse return lines as independent collision passes",
    "Allow Q outside attack range, on escape, lethal damage or a verified return hit",
    "Reconcile a missing Q missile callback and expire stale boomerang state",
    "Cast W only for the actual orbwalker attack target",
    "Prefer a W primary with a nearby unique ricochet target and preserve attack timing",
    "Use W for explicit last-hit/clear paths only after the mana threshold is met",
    "Observe enemy spell casts and gapclosers as an E impact window",
    "Cast E before impact, reject negative/late windows and reconcile SivirE buff state",
    "Keep E available in mana policy rather than spending it on ordinary poke",
    "Use R for coordinated engage when allies move with the player",
    "Use R for committed peel or flee movement when the player is threatened",
    "Reject blind automatic R with no allied follow-up or enemy pressure",
    "Preserve selected target, orbwalker attacks, manual casts and AA windup",
    "Run Q, W, E, R, farm and reactive policies through the owned update loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Sivir";
    controller.ControllerId = "champion.kuroaio.ai.sivir.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISivir.md";
    controller.ImplementationSummary =
        "Two-pass Q collision state, orbwalker-owned W primary target policy, "
        "impact-window E spell shield and ally-posture R movement with mana/cooldown reconciliation.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Sivir
