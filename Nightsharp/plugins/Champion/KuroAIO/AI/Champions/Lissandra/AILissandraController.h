#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AILissandraGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Lissandra {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CaptureGapcloserEvent;
using ControllerHelpers::CaptureInterruptableEvent;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::OrbwalkerHeroTarget;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int LastCastTick[4]{};
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int EReturnReadyTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline bool EActive = false;
inline int ECastTick = 0;
inline int EExpireTick = 0;
inline Vector3 EOrigin = {};
inline Vector3 EClawEnd = {};
inline int ETargetId = 0;
inline bool PassiveReady = false;
inline std::array<ThrallState, 16> Thralls{};

using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || IsCommonUntargetableOrImmune(target) ||
        target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
        target.HasBuff("BansheesVeil") || target.HasBuff("MorganaE");
}
inline AIHeroClient CooperativeTarget(const AIHeroClient& selected) {
    if (Engine::ValidEnemy(selected, 1200.0f)) return selected;
    const auto orb = OrbwalkerHeroTarget(1200.0f);
    if (Engine::ValidEnemy(orb)) return orb;
    return Engine::SelectTarget(1200.0f);
}
using ControllerHelpers::AP;
inline bool Lethal(const AIHeroClient& target, float rawDamage) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) &&
        player.CalculateMagicDamage(target, rawDamage) >= target.Health() + target.AllShield();
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(0, mode) || !Throttle(0) ||
        player.Position().Distance2D(target.Position()) > kQRange + 150.0f) return false;
    if (!defensive && Orbwalker::IsWindingUp() && !Lethal(target, QRawDamage(SpellRank(0), AP()))) return false;
    Vector3 aim = PredictPosition(target, 0.25f);
    if (!aim.IsValid() || aim.IsZero() || !QShardHits(player.Position(), aim, aim, target.BoundingRadius())) return false;
    aim = ClampDestination(player.Position(), aim, kQRange);
    if (SDK::NavMesh::IsWall(aim) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f) ||
        !Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(1, mode) || !Throttle(1)) return false;
    const Vector3 predicted = PredictPosition(target, 0.08f);
    if (!RootContains(player.Position(), predicted, target.BoundingRadius())) return false;
    if (!defensive && Orbwalker::IsWindingUp() && target.HealthPercent() > 25.0f) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2)) return false;
    if (EActive) {
        if (Now() < EReturnReadyTick) return false;
        const bool underTurret = Engine::UnderEnemyTurret(EOrigin);
        const bool hardCc = IncomingHardCcUntil > Now();
        if (!SafeReturn(EOrigin, EClawEnd, !SDK::NavMesh::IsWall(EOrigin), underTurret, hardCc)) return false;
        if (!Engine::ControllerCastSelf(2)) return false;
        EActive = false;
        LastCastTick[2] = Now();
        return true;
    }
    if (!defensive && (!Engine::ValidEnemy(target) || Protected(target))) return false;
    Vector3 aim = defensive ? Game::CursorPos() : PredictPosition(target, TravelSeconds(player.Position(), target.Position(), kESpeed));
    if (!aim.IsValid() || aim.IsZero()) return false;
    aim = ClampDestination(player.Position(), aim, kERange);
    if (SDK::NavMesh::IsWall(aim) || !Engine::ControllerCastPosition(2, aim)) return false;
    EActive = true;
    ECastTick = Now();
    EExpireTick = ECastTick + 3800;
    EOrigin = player.Position();
    EClawEnd = aim;
    EReturnReadyTick = ECastTick + static_cast<int>(std::ceil(
        TravelSeconds(EOrigin, EClawEnd, kESpeed) * 1000.0f));
    ETargetId = Engine::ValidEnemy(target) ? static_cast<int>(target.NetworkId()) : 0;
    LastCastTick[2] = ECastTick;
    return true;
}
inline bool CastRTarget(const AIHeroClient& target, Mode mode, bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(3, mode) || !Throttle(3, 100)) return false;
    const RPolicyContext context{
        true, true, Protected(target),
        player.Position().Distance2D(target.Position()) <= kRRange + target.BoundingRadius(),
        Lethal(target, RRawDamage(SpellRank(3), AP())),
        player.HealthPercent() <= Slider(RMenu, "SelfHp", 30),
        IncomingThreatUntil > Now(),
        Engine::CountEnemiesAt(target.Position(), kRRadius) >= Slider(RMenu, "MinimumTargets", 2),
        Orbwalker::IsWindingUp(), manual};
    if (!ShouldCastTargetR(context) || !Engine::ControllerCastUnit(3, target)) return false;
    LastCastTick[3] = Now();
    return true;
}
inline bool CastRSelf(Mode mode, bool emergency = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 100)) return false;
    const RPolicyContext context{
        true, false, false, false, false,
        player.HealthPercent() <= Slider(RMenu, "SelfHp", 30),
        IncomingThreatUntil > Now(),
        Engine::CountEnemiesAt(player.Position(), kRRadius) >= 1,
        Orbwalker::IsWindingUp(), emergency};
    if (!ShouldCastSelfR(context) || !Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    return true;
}
inline bool TryReactive(const AIHeroClient& target, Mode mode) {
    if (IncomingHardCcUntil > Now() && CastRSelf(mode, true)) return true;
    const auto gap = HeroByNetworkId(GapcloserTargetId);
    if (GapcloserExpireTick > Now() && Engine::ValidEnemy(gap)) {
        if (CastW(gap, mode, true)) return true;
        if (CastRTarget(gap, mode)) return true;
    }
    const auto interrupt = HeroByNetworkId(InterruptTargetId);
    if (InterruptExpireTick > Now() && Engine::ValidEnemy(interrupt) && CastW(interrupt, mode, true)) return true;
    return IncomingThreatUntil > Now() && Engine::ValidEnemy(target) && CastRSelf(mode, true);
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, QRawDamage(SpellRank(0), AP())) && CastQ(target, mode)) return true;
    if (Lethal(target, RRawDamage(SpellRank(3), AP())) && CastRTarget(target, mode)) return true;
    return false;
}
inline bool Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastRTarget(target, Mode::Combo)) return true;
    if (CastW(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    return CastE(target, Mode::Combo);
}
inline bool Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 38)) return false;
    if (CastQ(target, Mode::Harass)) return true;
    return CastW(target, Mode::Harass);
}
inline bool Flee(const AIHeroClient& target) {
    if (EActive && CastE(target, Mode::Flee, true)) return true;
    if (CastE(target, Mode::Flee, true)) return true;
    if (CastW(target, Mode::Flee, true)) return true;
    return CastRSelf(Mode::Flee, true);
}
inline void PruneThralls() {
    const int now = Now();
    for (auto& thrall : Thralls) if (!ThrallActive(thrall, now)) thrall = {};
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    PruneThralls();
    if (EActive && EExpireTick <= Now()) EActive = false;
    const AIHeroClient target = CooperativeTarget(selected);
    if (ManualOwnershipUntil > Now()) return true;
    if (TryReactive(target, mode) || TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)Combo(target); break;
    case Mode::Harass: (void)Harass(target); break;
    case Mode::Flee: (void)Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear: (void)Engine::TryFarm(Mode::LaneClear); break;
    case Mode::Jungle: (void)Engine::TryFarm(Mode::Jungle); break;
    case Mode::LastHit: (void)Engine::TryFarm(Mode::LastHit); break;
    case Mode::Automatic: {
        const auto player = GameObjects::Player();
        const bool low = player.IsValid() && player.HealthPercent() <= Slider(RMenu, "SelfHp", 30);
        const bool kill = Engine::ValidEnemy(target) && Lethal(target, RRawDamage(SpellRank(3), AP()));
        if (AutomaticAllowed({IncomingThreatUntil > Now(), IncomingHardCcUntil > Now(), low, kill, false})) {
            (void)TryReactive(target, Mode::Automatic);
            (void)TryKillSecure(target, Mode::Automatic);
        }
        break;
    }
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot <= 3) LastCastTick[slot] = now;
        if (slot >= 0 && slot <= 3 && !Engine::WasControllerCast(slot)) ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        if (args.Slot == 2 && !EActive) {
            const auto player = GameObjects::Player();
            EOrigin = player.Position();
            EClawEnd = args.EndPosition.IsValid() ? args.EndPosition : args.CastPosition;
            EActive = EClawEnd.IsValid() && !EClawEnd.IsZero();
            EExpireTick = now + 3800;
            EReturnReadyTick = now + static_cast<int>(std::ceil(
                TravelSeconds(EOrigin, EClawEnd, kESpeed) * 1000.0f));
        }
        return;
    }
    const auto threat = AnalyzeEnemyCast(args);
    if (!threat.Valid || (!threat.TargetsPlayer && !threat.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil, std::max(threat.CommitmentUntilTick, threat.LineThreatUntilTick));
    if (threat.LikelyHardCrowdControl) IncomingHardCcUntil = std::max(IncomingHardCcUntil, IncomingThreatUntil);
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "LissandraPassive")) PassiveReady = true;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "LissandraPassive")) PassiveReady = false;
}
inline bool IsThrallName(const char* name) {
    return name && (Engine::TextContains(name, "IcyGrave") || Engine::TextContains(name, "LissandraPassive"));
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!Bool(PassiveMenu, "TrackObjects", true) ||
        !args.Sender.IsValid() || !IsThrallName(args.Sender.Name)) return;
    ThrallState* slot = nullptr;
    for (auto& thrall : Thralls) if (!thrall.Active) { slot = &thrall; break; }
    if (!slot) slot = &Thralls.front();
    *slot = {args.Sender.Position, static_cast<int>(args.Sender.NetworkId), Now(), true};
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    for (auto& thrall : Thralls) if (thrall.NetworkId == id) thrall = {};
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF9FD6FFu, 1.4f, 36);
    Drawing::DrawCircle(player.Position(), kWRadius, 0xFFB5E6FFu, 1.4f, 36);
    for (const auto& thrall : Thralls) if (ThrallActive(thrall, Now())) Drawing::DrawCircle(thrall.Position, 140.0f, 0xFF7FA8FFu, 1.2f, 24);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("LissandraOneTrick", "Lissandra stasis and claw tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("IceShard", "Q spread and harass"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 38, 10, 90));
    RMenu = TacticsMenu->AddSubMenu(new Menu("FrozenTomb", "R stasis policy"));
    RMenu->Add(new MenuSlider("SelfHp", "Self stasis below HP percent", 30, 10, 70));
    RMenu->Add(new MenuSlider("MinimumTargets", "Target R minimum nearby enemies", 2, 1, 5));
    PassiveMenu = TacticsMenu->AddSubMenu(new Menu("IcyThralls", "Passive reconciliation"));
    PassiveMenu->Add(new MenuBool("TrackObjects", "Track icy thralls", true));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("LissandraCoach", "Geometry visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W and thrall ranges", false));
}
inline void OnLoad() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    ManualOwnershipUntil = IncomingThreatUntil = IncomingHardCcUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    EActive = PassiveReady = false; ECastTick = EExpireTick = EReturnReadyTick = ETargetId = 0; EOrigin = EClawEnd = {};
    Thralls = {};
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = PassiveMenu = CoachMenu = nullptr;
    Thralls = {}; EActive = false;
}
inline constexpr const char* Scenarios[] = {
    "Pin spell and buff semantics to Riot 26.15 and CommunityDragon 16.15",
    "Preserve selected target before orbwalker and selector fallback",
    "Model Ice Shard central line and both spread shards with collision safety",
    "Reject Q through invalid terrain and preserve AA windup unless lethal",
    "Root only targets inside the live Ring of Frost radius",
    "Track Glacial Path origin, endpoint, travel and recast return",
    "Never return E into an invalid or unsafe turret endpoint",
    "Choose enemy Frozen Tomb for lethal or multi-target value",
    "Choose self Frozen Tomb for low health, hard crowd control or burst",
    "Track Icy Thralls as passive objects without inventing a passive cast",
    "Reconcile passive buffs and thrall object deletion through polling",
    "React to gapclosers and interruptible casts with root or stasis",
    "Preserve manual spell ownership for a bounded handoff window",
    "Combo uses Q spread, W root, E claw and policy-gated R",
    "Harass uses Q and W while respecting mana and AA windup",
    "LaneClear Jungle and LastHit use shared farm routing only",
    "Flee sends E toward cursor, returns only safely, then peels with W/R",
    "Automatic mode permits defensive reaction or kill secure only",
    "Never automate summoners, items, movement or cursor ownership",
    "Expose pure Q spread, E travel/return and R safety geometry",
    "Keep controller metadata and research artifact nonempty",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Lissandra;
    controller.ControllerId = "champion.kuroaio.ai.lissandra.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AILissandra.md";
    controller.ImplementationSummary = "Event-reconciled Q shard spread, W root, E claw travel/return, passive icy thrall tracking and defensive/self-or-target Frozen Tomb policy.";
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
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEnd, &GapcloserExpireTick, 720, 900>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1100, 250, 5000>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Lissandra
