#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIYoneGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Yone {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* SpiritMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int QStacks = 0;
inline int QStackExpireTick = 0;
inline int EStartTick = 0;
inline int EExpireTick = 0;
inline int ETargetId = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int IncomingHardCCUntil = 0;
inline Vector3 SpiritOrigin = {};
inline Vector3 SpiritEndpoint = {};
inline Vector3 LastRTarget = {};
inline bool SpiritActive = false;

using ControllerHelpers::Now;

using ControllerHelpers::Ready;

inline bool Throttle(int index, int delay) {
    const int tick = index == 0 ? QCastTick : index == 1 ? WCastTick :
        index == 2 ? ECastTick : RCastTick;
    return Now() - tick >= delay;
}

inline bool TargetCannotBeDamaged(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BlackShield") ||
           target.HasBuff("BansheesVeil") || target.HasBuff("EdgeOfNight") ||
           target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("KayleR") || target.HasBuff("kindredrnodeathbuff");
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (QStackExpireTick > 0 && now >= QStackExpireTick) QStacks = 0;
    if (SpiritActive && EExpireTick > 0 && now >= EExpireTick) {
        SpiritActive = false;
        EStartTick = EExpireTick = ETargetId = 0;
        SpiritOrigin = SpiritEndpoint = {};
    }
    if (player.HasBuff("YoneE") || player.HasBuff("YoneEVisual")) {
        SpiritActive = true;
        if (EStartTick == 0) EStartTick = now;
        EExpireTick = std::max(EExpireTick, now + 900);
    }
    if (!SpiritActive && (player.HasBuff("YoneQ3") || player.HasBuff("YoneQ3Ready"))) {
        QStacks = 2;
        QStackExpireTick = now + 6000;
    }
}

using ControllerHelpers::TotalAttackDamage;

inline float QDamage(const AIHeroClient& target, int bodyIndex = 0) {
    return Engine::ValidEnemy(target) ?
        QRawDamage(std::clamp(Engine::RuntimeSpells[0]->Level(), 1, 5), TotalAttackDamage()) *
        (bodyIndex == 0 ? 1.0f : 0.75f) : 0.0f;
}

inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return WRawDamage(std::clamp(Engine::RuntimeSpells[1]->Level(), 1, 5),
                      TotalAttackDamage(), target.MaxHealth());
}

inline float RDamage(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) ?
        RRawDamage(std::clamp(Engine::RuntimeSpells[3]->Level(), 1, 3),
                   TotalAttackDamage(), 100.0f - target.HealthPercent()) : 0.0f;
}

using ControllerHelpers::Lethal;

inline bool CastQ(const AIHeroClient& target, Mode mode, bool defensive = false) {
    if (!Engine::ValidEnemy(target, kQRange + 40.0f) || !Ready(0, mode) ||
        !Throttle(0, 45) || TargetCannotBeDamaged(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool tornado = CanWhirlwind(QStacks, true);
    const float range = tornado ? kQWhirlwindRange : kQRange;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 predicted = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero() ? prediction.GetCastPosition() :
        PredictPosition(target, tornado ? 0.35f : 0.25f);
    if (!predicted.IsValid() || predicted.IsZero() ||
        player.Position().Distance2D(predicted) > range + target.BoundingRadius() ||
        prediction.Hitchance < SDK::HitChance::High ||
        (tornado && ControllerHelpers::ProjectileWallBlocksFromPlayer(
            predicted, kQWidth * 0.5f))) {
        return false;
    }
    const bool lethal = Lethal(target, QDamage(target));
    const ModeContext context{ Orbwalker::IsWindingUp(), lethal };
    if (!MayUseAbility(context) && !defensive) return false;
    if (!Engine::ControllerCastPosition(0, predicted)) return false;
    QCastTick = Now();
    if (tornado) QStacks = 0;
    else {
        QStacks = std::min(2, QStacks + 1);
        QStackExpireTick = Now() + 6000;
    }
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool defensive = false) {
    if (!Engine::ValidEnemy(target, kWRange + 30.0f) || !Ready(1, mode) ||
        !Throttle(1, 70) || TargetCannotBeDamaged(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vector3 predicted = PredictPosition(target, 0.22f);
    if (!predicted.IsValid() || predicted.IsZero() ||
        player.Position().Distance2D(predicted) > kWRange + target.BoundingRadius()) return false;
    const bool lethal = Lethal(target, WDamage(target));
    const ModeContext context{ Orbwalker::IsWindingUp(), lethal };
    if (!MayUseAbility(context) && !defensive) return false;
    if (!Engine::ControllerCastPosition(1, predicted)) return false;
    WCastTick = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool fleeing = false) {
    if (!Ready(2, mode) || !Throttle(2, 80)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (SpiritActive) {
        if (!Engine::ControllerCastSelf(2)) return false;
        ECastTick = Now();
        return true;
    }
    if (!Engine::ValidEnemy(target, kERange + 30.0f) || TargetCannotBeDamaged(target)) return false;
    const Vector3 predicted = PredictPosition(target, 0.20f);
    const Vector3 endpoint = ClampSpiritEndpoint(player.Position(), predicted);
    if (endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    SpiritContext safety{};
    safety.EndpointValid = true;
    safety.EndpointSafe = !Engine::UnderEnemyTurret(endpoint) || Engine::UnderEnemyTurret(player.Position());
    safety.ReturnAvailable = true;
    safety.TargetMarked = false;
    safety.Lethal = Lethal(target, QDamage(target) + WDamage(target));
    safety.Fleeing = fleeing;
    safety.NearbyEnemies = Engine::CountEnemiesAt(endpoint, 450.0f);
    safety.MaximumEnemies = Slider(SpiritMenu, "MaxSpiritEnemies", 2);
    if (!SpiritSafe(safety)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    SpiritActive = true;
    SpiritOrigin = player.Position();
    SpiritEndpoint = endpoint;
    EStartTick = Now();
    EExpireTick = EStartTick + Slider(SpiritMenu, "SpiritDurationMs", kEDurationMs);
    ETargetId = static_cast<int>(target.NetworkId());
    ECastTick = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive = false) {
    if (!Ready(3, mode) || !Throttle(3, 120) ||
        !Engine::ValidEnemy(target, kRRange + 35.0f) || TargetCannotBeDamaged(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vector3 predicted = PredictPosition(target, 0.45f);
    const Vector3 direction = Direction2D(player.Position(), predicted);
    if (direction.IsZero()) return false;
    const Vector3 endpoint = player.Position() + direction * kRRange;
    std::vector<Body> bodies;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (enemy.IsValid()) bodies.push_back({ enemy.Position(), enemy.BoundingRadius(),
                                                static_cast<int>(enemy.NetworkId()),
                                                static_cast<int>(enemy.NetworkId()) == static_cast<int>(target.NetworkId()), true });
    }
    const LineHit line = EvaluateLine(player.Position(), endpoint, bodies,
                                       static_cast<int>(target.NetworkId()), kRWidth);
    const bool lethal = Lethal(target, RDamage(target));
    const RContext context{ true, true, line.Hit, false,
                            lethal, line.OrderedIds.size() >= 2, defensive,
                            Engine::UnderEnemyTurret(endpoint) && !lethal,
                            static_cast<int>(line.OrderedIds.size()) };
    if (!MayCastR(context)) return false;
    if (!Engine::ControllerCastPosition(3, predicted)) return false;
    RCastTick = Now();
    LastRTarget = predicted;
    return true;
}

inline bool TryDefensive(const AIHeroClient& threat) {
    if (!Engine::ValidEnemy(threat, 900.0f)) return false;
    if (IncomingHardCCUntil > Now() || GameObjects::Player().HealthPercent() <=
        Slider(TacticsMenu, "EmergencyHP", 30)) {
        if (SpiritActive && CastE(threat, Mode::Flee, true)) return true;
        if (CastE(threat, Mode::Flee, true)) return true;
        if (CastR(threat, Mode::Flee, true)) return true;
    }
    return false;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, QDamage(target)) && CastQ(target, mode, true)) return true;
    if (Lethal(target, WDamage(target)) && CastW(target, mode, true)) return true;
    if (Lethal(target, RDamage(target)) && CastR(target, mode)) return true;
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (!SpiritActive && GameObjects::Player().Position().Distance2D(target.Position()) > 475.0f &&
        CastE(target, Mode::Combo)) return true;
    if (CanWhirlwind(QStacks, true) && CastQ(target, Mode::Combo)) return true;
    if (CastW(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    if (SpiritActive && CastE(target, Mode::Combo)) return true;
    if (target.HealthPercent() <= Slider(UltimateMenu, "RTargetHP", 55) &&
        CastR(target, Mode::Combo)) return true;
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastQ(target, Mode::Harass)) return true;
    return GameObjects::Player().HealthPercent() > 55.0f && CastW(target, Mode::Harass);
}

inline bool TryFarm(Mode mode) {
    if (!Ready(0, mode) || CurrentResource() < SpellCost(0) +
        Slider(FarmMenu, "ManaReserve", 65)) return false;
    return Engine::TryFarm(mode);
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (SpiritActive && CastE(threat, Mode::Flee, true)) return true;
    if (Engine::ValidEnemy(threat) && CastE(threat, Mode::Flee, true)) return true;
    if (Engine::ValidEnemy(threat) && CastQ(threat, Mode::Flee, true)) return true;
    return Engine::ValidEnemy(threat) && CastR(threat, Mode::Flee, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    ReconcileState();
    AIHeroClient target = Engine::SelectTarget(kRRange + 60.0f);
    const AIHeroClient threat = NearestEnemyToPlayer({}, 1000.0f);
    if (mode == Mode::Flee) {
        (void)TryFlee(threat);
        return true;
    }
    if (TryDefensive(threat)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && AutomaticAllowed({ false, IncomingHardCCUntil > Now(),
                                                              Lethal(target, QDamage(target)), false })) {
            (void)TryKillSecure(target, Mode::Automatic);
        }
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(args, 220.0f, 110.0f,
                                                                 250, 280, 260, 1500, 450);
        if (threat.Valid && threat.CrossesPlayer && threat.LikelyHardCrowdControl) {
            IncomingHardCCUntil = now + 650;
        }
        return;
    }
    const int slot = args.Slot;
    if (slot == 0) {
        QCastTick = now;
    } else if (slot == 1) {
        WCastTick = now;
    } else if (slot == 2) {
        ECastTick = now;
        if (Engine::TextContains(args.SpellName, "Return") ||
            Engine::TextContains(args.SpellName, "YoneE2")) SpiritActive = false;
    } else if (slot == 3) {
        RCastTick = now;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (Engine::TextContains(args.BuffName, "YoneQ3") ||
        Engine::TextContains(args.BuffName, "YoneQ3Ready")) {
        QStacks = 2;
        QStackExpireTick = now + 6000;
    } else if (Engine::TextContains(args.BuffName, "YoneQStack")) {
        QStacks = std::min(2, QStacks + 1);
        QStackExpireTick = now + 6000;
    } else if (Engine::TextContains(args.BuffName, "YoneE")) {
        SpiritActive = true;
        EStartTick = now;
        EExpireTick = now + ControllerHelpers::RemainingMilliseconds(args.EndTime, kEDurationMs, 300, 6000);
        SpiritOrigin = GameObjects::Player().Position();
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "YoneQ")) QStacks = 0;
    if (Engine::TextContains(args.BuffName, "YoneE")) {
        SpiritActive = false;
        EStartTick = EExpireTick = ETargetId = 0;
        SpiritOrigin = SpiritEndpoint = {};
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || !SpiritActive) return;
    const auto target = HeroByNetworkId(static_cast<int>(args.Target.NetworkId()));
    if (!Engine::ValidEnemy(target)) return;
    if (target.HealthPercent() > 70.0f && Orbwalker::IsWindingUp()) args.Process = true;
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), SpiritActive ? kERange : kQRange,
                         SpiritActive ? 0xFFB278FFu : 0xFF45D5FFu, 1.8f, 40);
    if (SpiritActive && !SpiritOrigin.IsZero()) {
        Drawing::DrawLine(player.Position(), SpiritOrigin, 0xFFB278FFu, 2.0f);
        Drawing::DrawCircle(SpiritOrigin, 55.0f, 0xFFB278FFu, 2.0f, 28);
    }
    if (!LastRTarget.IsZero()) Drawing::DrawLine(player.Position(), LastRTarget, 0xFF65B6FFu, 2.0f);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("YoneOneTrick", "Yone skirmisher mechanics"));
    TacticsMenu->Add(new MenuSlider("EmergencyHP", "Defensive threshold", 30, 10, 70));
    SpiritMenu = TacticsMenu->AddSubMenu(new Menu("SpiritForm", "Soul Unbound safety"));
    SpiritMenu->Add(new MenuSlider("SpiritDurationMs", "Spirit duration fallback", kEDurationMs, 2500, 6000));
    SpiritMenu->Add(new MenuSlider("MaxSpiritEnemies", "Max enemies at spirit endpoint", 2, 1, 5));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu("FateSealed", "Ultimate policy"));
    UltimateMenu->Add(new MenuSlider("RTargetHP", "R target HP threshold", 55, 15, 100));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("YoneFarm", "Conservative farm"));
    FarmMenu->Add(new MenuSlider("ManaReserve", "Mana reserve", 65, 0, 180));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("YoneCoach", "Route visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/E/R ranges", false));
}

inline void OnLoad() {
    QStacks = 0;
    QStackExpireTick = EStartTick = EExpireTick = ETargetId = 0;
    QCastTick = WCastTick = ECastTick = RCastTick = 0;
    LastAutoTargetId = LastAutoTick = 0;
    IncomingHardCCUntil = 0;
    SpiritOrigin = SpiritEndpoint = LastRTarget = {};
    SpiritActive = false;
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = SpiritMenu = UltimateMenu = FarmMenu = CoachMenu = nullptr;
    SpiritActive = false;
    QStacks = 0;
}

inline constexpr const char* Scenarios[] = {
    "Read Riot 26.15 and CommunityDragon 16.15 as the pinned Summoner's Rift baseline",
    "Keep Yone's passive mixed physical and magical damage split explicit",
    "Track Q stack count from events and polling reconciliation",
    "Expire Q stacks after the observed live stack window",
    "Use 475 range for ordinary Mortal Steel",
    "Use 650 range only for the confirmed third-Q whirlwind",
    "Preserve first-body Q damage and reduce continuation bodies",
    "Reject Q through projectile-wall or collision uncertainty",
    "Preserve AA windup during nonlethal Q casts",
    "Use W cone reach and current-target maximum-health scaling",
    "Preserve W shield and attack reset economy in combo planning",
    "Avoid W when its cone prediction is below the configured hitchance",
    "Track Soul Unbound start, expiry and return state",
    "Allow E return before offensive re-entry when the spirit route is unsafe",
    "Reject E endpoints through walls or under a new turret dive",
    "Reject offensive spirit endpoints surrounded by excessive enemies",
    "Permit lethal or flee spirit routes with an available return",
    "Use the engine-selected target while it remains reachable during Spirit Form",
    "Use return recast only when the player route is unsafe or lethal",
    "Track E mark target and stored damage state conservatively",
    "Use R 1000 range, 120 width and 1500 projectile speed",
    "Require R initial line intersection before hit credit",
    "Reject R through a real projectile wall",
    "Reject ordinary single-target nonlethal R",
    "Allow R for multi-target or verified defensive peel",
    "Allow R for lethal target damage",
    "Reject R endpoint under an enemy turret unless lethal",
    "Preserve R channel state through the engine lock",
    "Use the engine-selected target for continuity",
    "Combo starts Spirit Form only when the entry route is reachable",
    "Combo prioritizes third-Q whirlwind before ordinary Q",
    "Combo weaves Q and W around attack timing",
    "Combo returns from Spirit Form after safety or kill evaluation",
    "Harass uses Q and W without unsolicited Spirit Form commit",
    "LaneClear delegates ordinary farm only through the shared farm path",
    "Jungle mode preserves mana reserve and attack timing",
    "LastHit avoids spending the Spirit route on routine minions",
    "Flee returns from active Spirit Form before offensive casts",
    "Flee permits Q only as a defensive spacing action",
    "Automatic mode never starts an unsolicited engage",
    "Automatic mode may react to hard crowd control",
    "Automatic mode may execute verified lethal damage",
    "Track enemy hard crowd-control threat windows",
    "Track Q/W/E/R casts from events and re-plan without clearing state",
    "Never automate Flash, Ignite, Smite or item actives",
    "Keep profile metadata separate from the owned decision loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Yone;
    controller.ControllerId = "champion.kuroaio.ai.yone.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIYone.md";
    controller.ImplementationSummary =
        "Q-stack and W cone sequencing, collision-aware whirlwind, Spirit Form "
        "return safety, mixed damage estimates and conservative multi-target R planning.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Yone
