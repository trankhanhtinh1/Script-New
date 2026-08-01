#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIDianaGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Diana {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline constexpr std::size_t kTrackedMarks = 16;
inline std::array<MoonlightMark, kTrackedMarks> Marks{};
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PassiveAttacks = 0;
inline int LastQTargetId = 0;
inline int LastETargetId = 0;
inline int EResetUntil = 0;
inline int WExpireTick = 0;
inline int RCastTick = 0;
inline int RResetUntil = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline bool RActive = false;
inline OrbState WState{};

inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline int MarkIndex(int id, bool create) {
    if (id == 0) return -1;
    int freeIndex = -1;
    for (std::size_t i = 0; i < Marks.size(); ++i) {
        if (Marks[i].TargetId == id) return static_cast<int>(i);
        if (freeIndex < 0 && Marks[i].TargetId == 0) freeIndex = static_cast<int>(i);
    }
    return create ? freeIndex : -1;
}

inline bool HasMoonlight(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const int id = static_cast<int>(target.NetworkId());
    const int index = MarkIndex(id, false);
    return target.HasBuff("DianaMoonlight") || target.HasBuff("DianaMoonlightMark") ||
           (index >= 0 && MarkActive(Marks[static_cast<std::size_t>(index)], id, Now()));
}

inline void SetMoonlight(int id, int now) {
    const int index = MarkIndex(id, true);
    if (index >= 0) Marks[static_cast<std::size_t>(index)] = ApplyMoonlightMark(id, now);
}

inline void ClearMoonlight(int id) {
    const int index = MarkIndex(id, false);
    if (index >= 0) Marks[static_cast<std::size_t>(index)] = {};
}

inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target) || target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzEIcon") || target.HasBuff("KayleR") ||
           target.HasBuff("kindredrnodeathbuff");
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, QRawDamage(SpellRank(0), player.AP())) : 0.0f;
}

inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target,
        WRawDamagePerOrb(SpellRank(1), player.AP()) * 3.0f);
}

inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, ERawDamage(SpellRank(2), player.AP())) : 0.0f;
}

inline float RDamage(const AIHeroClient& target, int enemyCount, bool marked) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            RRawDamage(SpellRank(3), player.AP(), enemyCount, marked)) : 0.0f;
}

inline bool AimQ(const AIHeroClient& target, Vector3& aim) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kQRange)) return false;
    const float travel = QTravelSeconds(player.Position().Distance2D(target.Position()));
    const Vector3 predicted = PredictPosition(target, travel);
    aim = CrescentAim(player.Position(), predicted);
    if (aim.IsZero() || !CrescentStrikeHits(player.Position(), aim, predicted,
                                             target.BoundingRadius())) return false;
    return !ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f);
}

inline bool SafeEndpoint(const AIHeroClient& target, bool lethal, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange)) return false;
    const bool marked = HasMoonlight(target);
    const Vector3 endpoint = EDashEndpoint(player.Position(), target.Position(),
                                            player.BoundingRadius(), target.BoundingRadius());
    const int nearby = Engine::CountEnemiesAt(endpoint, 525.0f);
    const bool turret = Engine::UnderEnemyTurret(endpoint);
    if (!EEndpointSafe(endpoint, SDK::NavMesh::IsWall(endpoint), turret, nearby,
                       defensive ? 3 : Slider(EMenu, "MaxEnemies", 2), lethal)) return false;
    if (!marked && !Ready(2)) return false;
    return EReachable(player.Position(), target.Position(), player.BoundingRadius(),
                      target.BoundingRadius(), marked);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !Ready(0, mode) ||
        !Throttle(0) || PreserveAttack(reactive)) return false;
    Vector3 aim{};
    if (!AimQ(target, aim) || !Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    LastQTargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    const bool nearby = Engine::ValidEnemy(target, kWRange);
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive) || (!nearby && !reactive)) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 450.0f);
    if (!reactive && enemies == 0 && player.HealthPercent() > 55.0f) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    WState = BeginPaleCascade(LastCastTick[1]);
    WExpireTick = LastCastTick[1] + static_cast<int>(kWShieldSeconds * 1000.0f);
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !Ready(2, mode) ||
        !Throttle(2) || PreserveAttack(reactive)) return false;
    const bool marked = HasMoonlight(target);
    const bool lethal = ControllerHelpers::Lethal(target, EDamage(target));
    if (!SafeEndpoint(target, lethal, reactive)) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastCastTick[2] = Now();
    LastETargetId = static_cast<int>(target.NetworkId());
    if (marked) {
        ClearMoonlight(LastETargetId);
        EResetUntil = LastCastTick[2] + 850;
    }
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) || !Ready(3, mode) ||
        !Throttle(3) || PreserveAttack(reactive)) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const int minimum = reactive ? 1 : Slider(RMenu, "MinimumEnemies", 2);
    const bool marked = HasMoonlight(target);
    const bool lethal = ControllerHelpers::Lethal(target, RDamage(target, enemies, marked));
    const Vector3 targetFuture = PredictPosition(target, 0.25f);
    if (!MoonfallPulls(player.Position(), targetFuture, kRRadius,
                       target.BoundingRadius())) return false;
    MoonfallContext context{
        enemies, Engine::CountAlliesAt(player.Position(), kRRadius),
        Engine::CountEnemiesAt(player.Position(), 800.0f) - enemies,
        Engine::UnderEnemyTurret(player.Position()), SDK::NavMesh::IsWall(player.Position()),
        marked, lethal, player.HealthPercent()};
    if (!MoonfallSafe(context, reactive ? -200.0f :
                      static_cast<float>(Slider(RMenu, "MinimumSafety", 160)), minimum)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = RCastTick = Now();
    RResetUntil = RCastTick + static_cast<int>(kRResetSeconds * 1000.0f);
    RActive = true;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    const bool marked = HasMoonlight(target);
    if (!marked && CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 50)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (HasMoonlight(target)) (void)CastE(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& pursuer) {
    if (Engine::ValidEnemy(pursuer)) {
        if (CastW(pursuer, Mode::Flee, true)) return;
        (void)CastE(pursuer, Mode::Flee, true);
    }
}

inline bool Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (Engine::ValidEnemy(target) && IncomingHardCcUntil > Now() &&
        CastW(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && HasMoonlight(target) &&
        player.HealthPercent() < 45.0f && CastE(target, Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) &&
        (ControllerHelpers::Lethal(target, QDamage(target) + EDamage(target)) ||
         Engine::CountEnemiesAt(player.Position(), kRRadius) >= 2) &&
        CastR(target, Mode::Automatic, true)) return true;
    return false;
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    for (auto& mark : Marks) {
        if (mark.TargetId == 0) continue;
        const auto target = HeroByNetworkId(mark.TargetId);
        const bool buff = Engine::ValidEnemy(target) &&
                          (target.HasBuff("DianaMoonlight") ||
                           target.HasBuff("DianaMoonlightMark"));
        mark = ReconcileMark(mark, buff, mark.TargetId, now);
    }
    if (WState.CastTick > 0) {
        WState = ReconcileOrbs(WState, now,
            player.IsValid() && (player.HasBuff("DianaW") ||
                                 player.HasBuff("DianaWSshield")));
        if (now > WExpireTick + 150) WState = {};
    }
    if (RActive && (!player.IsValid() || !player.HasBuff("DianaMoonfall") &&
                    now > RResetUntil + 250)) RActive = false;
    if (EResetUntil > 0 && now > EResetUntil + 250) EResetUntil = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const auto target = PreferredEnemyTarget(selected,
        mode == Mode::Flee ? 1000.0f : kQRange);
    if (ManualOwnershipUntil > Now()) return true;
    if (mode == Mode::Automatic && Automatic(target)) return true;
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
        if (args.IsAutoAttack) {
            LastAutoTargetId = static_cast<int>(args.TargetNetworkId != 0
                ? args.TargetNetworkId : args.Target.NetworkId);
            LastAutoTick = now;
            return;
        }
        if (slot >= 0 && slot <= 3 && !Engine::WasControllerCast(slot))
            ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 650);
        if (slot == 2 && LastETargetId != 0) ClearMoonlight(LastETargetId);
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCcUntil = std::max(
        IncomingHardCcUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) LastAutoTick = Now();
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    const int now = Now();
    if (Engine::TextContains(args.BuffName, "DianaMoonlight") ||
        Engine::TextContains(args.BuffName, "DianaMoonlightMark")) SetMoonlight(id, now);
    if (IsLocalPlayer(args.Sender) &&
        (Engine::TextContains(args.BuffName, "DianaW") ||
         Engine::TextContains(args.BuffName, "DianaWSshield"))) {
        WState.ShieldActive = true;
        WExpireTick = std::max(WExpireTick, now + static_cast<int>(kWShieldSeconds * 1000.0f));
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (Engine::TextContains(args.BuffName, "DianaMoonlight") ||
        Engine::TextContains(args.BuffName, "DianaMoonlightMark")) ClearMoonlight(id);
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "DianaW")) {
        WState = {};
        WExpireTick = 0;
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) { OnBuffAdd(args); }

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs&) {}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
    if (LastAutoTargetId != 0) RecordBasicAttack(PassiveAttacks);
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (args.NetworkId != 0) IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 650);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    if (args.NetworkId != 0) IncomingHardCcUntil = std::max(IncomingHardCcUntil, Now() + 650);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && ControllerHelpers::AnyTextContains(
            {args.Sender.Name, args.Sender.CharacterName, args.SpellName},
            {"dianaworb", "dianaw"})) {
        WState.Active = true;
    }
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && ControllerHelpers::AnyTextContains(
            {args.Sender.Name, args.Sender.CharacterName, args.SpellName},
            {"dianaworb", "dianaw"})) {
        WState = {};
    }
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("DianaMoonlight", "Diana Moonlight tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 650, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Crescent Strike"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 50, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Pale Cascade"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Lunar Rush"));
    EMenu->Add(new MenuSlider("MaxEnemies", "Maximum enemies at dash endpoint", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Moonfall"));
    RMenu->Add(new MenuSlider("MinimumEnemies", "Minimum enemies in pull", 2, 1, 5));
    RMenu->Add(new MenuSlider("MinimumSafety", "Minimum Moonfall safety score", 160, -500, 1000));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("DianaFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 42, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("DianaCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/E/R ranges", false));
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF9B5DE5u, 1.4f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF4CC9F0u, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFFFAA40u, 1.2f, 32);
}

inline void ResetState() {
    Marks = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = PassiveAttacks = LastQTargetId = 0;
    LastETargetId = EResetUntil = WExpireTick = RCastTick = RResetUntil = 0;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingHardCcUntil = 0;
    RActive = false;
    WState = {};
}
inline void OnLoad() { ResetState(); }
inline void OnUnload() {
    ResetState();
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Pin Diana spell values and Moonlight identities to Riot 26.15 / CommunityDragon 16.15",
    "Track third-hit Moonsilver Blade cadence from attacks and reconcile polling state",
    "Predict 825-range Crescent Strike with cast delay, missile travel and crescent hitbox",
    "Reject Q projectile paths blocked by terrain and ignore protected targets",
    "Track DianaMoonlight marks by network ID, three-second expiry and E consumption",
    "Create Pale Cascade's timed shield and reconcile all three orbiting orb contacts",
    "Use W defensively under threat without hiding an unsafe dash or ultimate commit",
    "Compute radius-adjusted Lunar Rush endpoint and require marked reset or ready E",
    "Reject E terrain crossings, hostile turrets and excessive endpoint enemy density",
    "Use real Q/W/E/R magic damage and current target shields for lethal decisions",
    "Evaluate Moonfall pull radius, marks, allies, outside threats and player health",
    "Reject R terrain and unsafe turret/density commits except verified lethal exceptions",
    "Give selected target precedence before orbwalker fallback in every combat mode",
    "Preserve AA windup and yield to manual spell ownership before automated casts",
    "Reconcile marks, shield/orbs, reset windows, R state and cooldown assumptions by polling",
    "Automatic mode handles hard-CC pressure, defensive marked dash and lethal/density R",
    "Combo follows Q mark, W shield/orbs, marked E and safe Moonfall burst",
    "Harass uses mana-reserved predicted Q and marked E without wasteful R",
    "Flee shields against a pursuer and dashes only to a marked safe endpoint",
    "LaneClear, Jungle and LastHit defer to shared farm policy above Diana mana reserve",
    "Draw Q/E/R geometry without taking movement, summoner or item ownership",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Diana;
    controller.ControllerId = "champion.kuroaio.ai.diana.moonlight";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIDiana.md";
    controller.ImplementationSummary =
        "Moonlight mark ledger with predicted Crescent Strike, timed Pale Cascade shield/orbs, safe marked Lunar Rush and density-checked Moonfall.";
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
    controller.OnBuffUpdate = &OnBuffUpdate;
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

} // namespace Plugins::KuroAIO::AI::Controllers::Diana
