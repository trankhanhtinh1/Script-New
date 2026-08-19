#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIZoeGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Zoe {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttackEvent;
using ControllerHelpers::CaptureGapcloserEvent;
using ControllerHelpers::CaptureInterruptableEvent;
using ControllerHelpers::CaptureLocalAutoAttackEvent;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;

inline Menu* TacticsMenu = nullptr;
inline Menu* BubbleMenu = nullptr;
inline Menu* PaddleMenu = nullptr;
inline Menu* StolenMenu = nullptr;
inline Menu* PortalMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline bool QActive = false;
inline bool QReturning = false;
inline int QCastTick = 0;
inline int QTargetId = 0;
inline int QMissileNetworkId = 0;
inline int QLastSeenTick = 0;
inline Vector3 QOutboundEnd{};
inline Vector3 QCastOrigin{};
inline Vector3 QMissilePosition{};
inline float QReturnScore = 0.0f;
inline bool WStolenReady = false;
inline std::string WStolenSpell{};
inline int WCastTick = 0;
inline bool RActive = false;
inline int RCastTick = 0;
inline int RReturnTick = 0;
inline Vector3 RPortalEndpoint{};
inline Vector3 RReturnOrigin{};
inline int SleepTargetId = 0;
inline int SleepExpireTick = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd{};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int IncomingThreatUntil = 0;
inline int ManualOverrideUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastCastTick[4]{};

inline constexpr int kManualOwnershipMs = 650;
inline constexpr float kQRange = kQBaseRange;
inline constexpr float kQMaxRange = kQMaximumRange;
inline constexpr float kZoeRRange = Geometry::kRRange;

using ControllerHelpers::Now;

using ControllerHelpers::Ready;

inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline bool ProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target) || target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzE") || target.HasBuff("zhonyasringshield");
}

using ControllerHelpers::PreserveAttack;

inline bool RuntimeWStolen() {
    if (!Engine::RuntimeSpells[1]) return false;
    const std::string& name = Engine::RuntimeSpellNames[1];
    if (name.empty() || Engine::TextContains(name.c_str(), "zoew") ||
        Engine::TextContains(name.c_str(), "spellthief")) return false;
    return true;
}

inline bool BubbleBuffName(const char* name) {
    return name && (Engine::TextContains(name, "zoesleep") ||
                    Engine::TextContains(name, "sleepytroublebubble") ||
                    Engine::TextContains(name, "sleep"));
}

inline bool IsQMissileName(const char* spell, const char* missile) {
    return (spell && (Engine::TextContains(spell, "zoeq") ||
                      Engine::TextContains(spell, "paddlestar"))) ||
           (missile && (Engine::TextContains(missile, "zoeq") ||
                        Engine::TextContains(missile, "paddlestar")));
}

inline bool IsQReturnName(const char* spell, const char* missile) {
    return (spell && (Engine::TextContains(spell, "return") ||
                      Engine::TextContains(spell, "zoeq2"))) ||
           (missile && (Engine::TextContains(missile, "return") ||
                        Engine::TextContains(missile, "zoeq2")));
}

inline Vector3 AimFor(int slot, const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return {};
    const Vector3 fallback = PredictPosition(target, slot == 2 ? 0.30f : 0.25f);
    if (slot < 0 || slot >= 4 || !Engine::RuntimeSpells[slot]) return fallback;
    const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
    if (prediction.Hitchance >= (slot == 2 ? SDK::HitChance::VeryHigh : SDK::HitChance::High) &&
        prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero())
        return prediction.GetCastPosition();
    return fallback;
}

inline bool SafePortalEndpoint(const Vector3& endpoint, const AIHeroClient& target,
                               bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint) ||
        player.Position().Distance2D(endpoint) > kZoeRRange + 20.0f) return false;
    if (!lethal && Engine::UnderEnemyTurret(endpoint)) return false;
    if (!lethal && Engine::CountEnemiesAt(endpoint, 500.0f) >
        Slider(PortalMenu, "MaximumEnemies", 1)) return false;
    if (!lethal && Engine::PositionDangerScore(
            endpoint, target, Engine::ResolvedSpecs[3]) <= -10000.0f) return false;
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || ProtectedTarget(target) || !Ready(0, mode) ||
        !Throttle(0) || PreserveAttack(reactive, lethal)) return false;
    const bool recast = QActive && !QReturning;
    Vector3 aim = AimFor(0, target);
    if (recast) {
        const Vector3 predicted = AimFor(0, target);
        aim = QRecastEndpoint(player.Position(), predicted, true,
                              std::min(kQMaxRange, player.Position().Distance2D(predicted) + 70.0f));
    } else {
        aim = QRecastEndpoint(player.Position(), aim, false,
                              std::min(kQRange, player.Position().Distance2D(aim)));
    }
    if (!aim.IsValid() || aim.IsZero() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    const int now = Now();
    LastCastTick[0] = now;
    QTargetId = static_cast<int>(target.NetworkId());
    if (!recast) {
        QActive = true;
        QReturning = false;
        QCastOrigin = player.Position();
        QOutboundEnd = aim;
        QCastTick = now;
        QLastSeenTick = now;
    } else {
        QReturning = true;
        QLastSeenTick = now;
        QOutboundEnd = aim;
    }
    return true;
}

inline std::vector<CollisionUnit> BubbleCollisionUnits() {
    std::vector<CollisionUnit> units;
    for (const auto& unit : Engine::ClearUnits(false)) {
        if (!unit.IsValid() || unit.IsDead()) continue;
        units.push_back({unit.Position(), unit.BoundingRadius(), true});
    }
    return units;
}

inline bool CastBubble(const AIHeroClient& target, Mode mode, bool reactive = false,
                       bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || ProtectedTarget(target) || !Ready(2, mode) ||
        !Throttle(2) || PreserveAttack(reactive, lethal)) return false;
    const Vector3 aim = AimFor(2, target);
    if (!aim.IsValid() || aim.IsZero()) return false;
    const bool wallOnPath = ControllerHelpers::ProjectileWallBlocksFromPlayer(
        aim, kEWidth * 0.5f);
    if (wallOnPath) return false;
    const bool throughWall = false;
    if (!Bool(BubbleMenu, "RejectCollision", true)) {
        const std::vector<CollisionUnit> noUnits;
        if (!BubblePathSafe(player.Position(), aim, noUnits.data(), 0,
                            wallOnPath, throughWall)) return false;
    } else {
        const auto units = BubbleCollisionUnits();
        if (!BubblePathSafe(player.Position(), aim, units.data(), units.size(),
                            wallOnPath, throughWall)) return false;
    }
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastStolenSpell(const AIHeroClient& target, Mode mode,
                            bool reactive = false, bool lethal = false) {
    if (!Bool(StolenMenu, "UseStolenSpell", true) || !RuntimeWStolen() ||
        !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive, lethal) || !Engine::ValidEnemy(target)) return false;
    if (Engine::ControllerCastSelf(1)) {
        WStolenReady = true;
        WStolenSpell = Engine::RuntimeSpellNames[1];
        WCastTick = Now();
        LastCastTick[1] = WCastTick;
        return true;
    }
    return false;
}

inline bool CastPortal(const AIHeroClient& target, Mode mode, bool flee = false,
                       bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 110) ||
        PreserveAttack(false, lethal)) return false;
    Vector3 desired;
    if (flee) desired = Game::CursorPos();
    else desired = AimFor(0, target);
    const Vector3 endpoint = PortalEndpoint(player.Position(), desired,
                                             false, kZoeRRange);
    if (!SafePortalEndpoint(endpoint, target, lethal)) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    RActive = true;
    RCastTick = Now();
    RReturnTick = PortalReturnTick(RCastTick);
    RPortalEndpoint = endpoint;
    RReturnOrigin = player.Position();
    LastCastTick[3] = RCastTick;
    return true;
}

inline float SpellDamage(int slot, const AIHeroClient& target) {
    if (slot < 0 || slot >= 4 || !Engine::RuntimeSpells[slot] ||
        !Engine::ValidEnemy(target)) return 0.0f;
    return Engine::RuntimeSpells[slot]->GetDamage(target);
}

inline bool Lethal(int slot, const AIHeroClient& target) {
    return SpellDamage(slot, target) >= target.Health() + target.AllShield();
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target) || ProtectedTarget(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool qReachable = player.Position().Distance2D(target.Position()) <= kQMaxRange;
    const bool bubbleReachable = player.Position().Distance2D(target.Position()) <= kEWallRange;
    const auto posture = ChooseKillSecurePosture(
        target.Health() + target.AllShield(), SpellDamage(0, target),
        SpellDamage(2, target), SpellDamage(1, target), qReachable,
        bubbleReachable, RuntimeWStolen(), Orbwalker::IsWindingUp(), false, true);
    switch (posture) {
    case KillSecurePosture::Bubble:
        return Lethal(2, target) && CastBubble(target, mode, true, true);
    case KillSecurePosture::Q:
        return Lethal(0, target) && CastQ(target, mode, true, true);
    case KillSecurePosture::StolenSpell:
        return Lethal(1, target) && CastStolenSpell(target, mode, true, true);
    default:
        return false;
    }
}

inline bool TryReturnQ(const AIHeroClient& target, Mode mode) {
    if (!QReturning || !QMissilePosition.IsValid() || !Engine::ValidEnemy(target)) return false;
    QReturnScore = QReturnHitScore(QMissilePosition, QCastOrigin,
                                   PredictPosition(target, QReturnTravelSeconds(
                                       QMissilePosition, QCastOrigin, target.Position())),
                                   target.BoundingRadius());
    return QReturnScore >= Slider(PaddleMenu, "MinimumReturnScore", 42);
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (TryReturnQ(target, Mode::Combo)) return true;
    if (SleepTargetId == static_cast<int>(target.NetworkId()) && Now() < SleepExpireTick) {
        if (CastQ(target, Mode::Combo, false, Lethal(0, target))) return true;
        if (CastStolenSpell(target, Mode::Combo)) return true;
    }
    if (CastBubble(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    if (RuntimeWStolen() && CastStolenSpell(target, Mode::Combo)) return true;
    if (Bool(PortalMenu, "UsePortalSetup", true) &&
        player.Position().Distance2D(target.Position()) > 700.0f)
        return CastPortal(target, Mode::Combo);
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() <
        Slider(TacticsMenu, "HarassMana", 42)) return false;
    if (SleepTargetId == static_cast<int>(target.NetworkId()) &&
        Now() < SleepExpireTick && CastQ(target, Mode::Harass)) return true;
    if (CastBubble(target, Mode::Harass)) return true;
    if (CastQ(target, Mode::Harass)) return true;
    return RuntimeWStolen() && CastStolenSpell(target, Mode::Harass);
}

inline bool TryFlee(const AIHeroClient& selected) {
    AIHeroClient threat = selected;
    if (!Engine::ValidEnemy(threat, 1000.0f)) threat = Engine::SelectTarget(1000.0f);
    if (Engine::ValidEnemy(threat) && CastBubble(threat, Mode::Flee, true)) return true;
    return CastPortal(threat, Mode::Flee, true);
}

inline bool TryAutomatic(const AIHeroClient& target) {
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target) &&
        CastBubble(target, Mode::Automatic, true)) return true;
    if (GapcloserTargetId != 0 && GapcloserExpireTick > Now()) {
        const auto enemy = Engine::EnemyByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(enemy) && CastBubble(enemy, Mode::Automatic, true)) return true;
    }
    if (InterruptTargetId != 0 && InterruptExpireTick > Now()) {
        const auto enemy = Engine::EnemyByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(enemy) && CastBubble(enemy, Mode::Automatic, true)) return true;
    }
    return Engine::ValidEnemy(target) && TryKillSecure(target, Mode::Automatic);
}

inline void RefreshTrackedQ() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    bool found = false;
    for (const auto& missile : GameObjects::Missiles()) {
        if (!missile.IsValid() || missile.CasterNetworkId() != player.NetworkId())
            continue;
        const std::string spellName = missile.SpellName();
        const std::string missileName = missile.MissileName();
        if (!IsQMissileName(spellName.c_str(), missileName.c_str())) continue;
        found = true;
        QActive = true;
        QReturning = IsQReturnName(spellName.c_str(), missileName.c_str());
        QMissileNetworkId = missile.NetworkId();
        QMissilePosition = missile.Position();
        QLastSeenTick = Now();
        if (QCastOrigin.IsZero()) QCastOrigin = missile.StartPosition();
        if (QOutboundEnd.IsZero()) QOutboundEnd = missile.EndPosition();
    }
    if (found || !QActive || QLastSeenTick <= 0) return;
    if (Now() - QLastSeenTick > 240) {
        QReturning = true;
        QMissilePosition = QOutboundEnd;
    }
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    RefreshTrackedQ();
    WStolenReady = RuntimeWStolen();
    if (SleepTargetId != 0 && now > SleepExpireTick) SleepTargetId = 0;
    if (QActive && now - QCastTick > 2600 && !QReturning) QActive = false;
    if (QReturning && now - QCastTick > 1800) QActive = QReturning = false;
    if (RActive && now >= RReturnTick) {
        RActive = false;
        RPortalEndpoint = {};
        RReturnOrigin = {};
    }
    if (IncomingThreatUntil < now) IncomingThreatUntil = 0;
    if (GapcloserTargetId != 0 && GapcloserExpireTick <= now) GapcloserTargetId = 0;
    if (InterruptTargetId != 0 && InterruptExpireTick <= now) InterruptTargetId = 0;
    if (player.IsValid() && !player.HasBuff("ZoePortalJump") &&
        RActive && now - RCastTick > 1300) RActive = false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, kQMaxRange);
    if (ManualOverrideUntil > Now()) return true;
    if (mode == Mode::Automatic) {
        (void)TryAutomatic(target);
        return true;
    }
    if (TryKillSecure(target, mode)) return true;
    if (mode == Mode::Flee) {
        (void)TryFlee(selected);
        return true;
    }
    if (mode == Mode::Combo) {
        (void)TryCombo(target);
        return true;
    }
    if (mode == Mode::Harass) {
        (void)TryHarass(target);
        return true;
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        (void)Engine::TryFarm(mode);
        return true;
    }
    if (Key(Engine::AutomaticMenu, "ManualR", false) && Engine::ValidEnemy(target))
        (void)CastPortal(target, Mode::Automatic);
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        if (!Engine::WasControllerCast(slot)) ManualOverrideUntil = now + kManualOwnershipMs;
        LastCastTick[slot] = now;
        if (slot == 0) {
            if (QActive) QReturning = true;
            else {
                QActive = true;
                QReturning = false;
                QLastSeenTick = now;
                QCastOrigin = GameObjects::Player().Position();
                QCastTick = now;
            }
        } else if (slot == 1) {
            WCastTick = now;
            WStolenReady = RuntimeWStolen();
        } else if (slot == 3) {
            RActive = true;
            RCastTick = now;
            RReturnTick = PortalReturnTick(now);
            RReturnOrigin = GameObjects::Player().Position();
        }
        return;
    }
    const auto threat = ControllerHelpers::AnalyzeEnemyCast(args,
        230.0f, 100.0f, 260, 250, 240, 1200, 350);
    if (threat.Valid && (threat.TargetsPlayer || threat.CrossesPlayer))
        IncomingThreatUntil = std::max(IncomingThreatUntil,
            std::max(threat.CommitmentUntilTick, threat.LineThreatUntilTick));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "ZoePortalJump")) {
            RActive = true;
            RReturnTick = PortalReturnTick(now);
        }
        return;
    }
    const auto enemy = Engine::EnemyByNetworkId(static_cast<int>(args.Sender.NetworkId));
    if (Engine::ValidEnemy(enemy) && BubbleBuffName(args.BuffName)) {
        SleepTargetId = static_cast<int>(enemy.NetworkId());
        SleepExpireTick = now + ControllerHelpers::RemainingMilliseconds(
            args.EndTime, 2500, 400, 6000);
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (static_cast<int>(args.Sender.NetworkId) == SleepTargetId &&
        BubbleBuffName(args.BuffName)) {
        SleepTargetId = 0;
        SleepExpireTick = 0;
    }
    if (IsLocalPlayer(args.Sender) &&
        Engine::TextContains(args.BuffName, "ZoePortalJump")) RActive = false;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || SleepTargetId == 0 || Now() >= SleepExpireTick) return;
    if (static_cast<int>(args.Target.NetworkId()) == SleepTargetId &&
        Ready(0, Mode::Combo) && !QActive &&
        SleepExpireTick - Now() < 300) args.Process = false;
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) || !IsQMissileName(args.SpellName, args.MissileName)) return;
    QActive = true;
    QReturning = IsQReturnName(args.SpellName, args.MissileName);
    QMissileNetworkId = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    QLastSeenTick = Now();
    QMissilePosition = args.Sender.Position.IsValid()
        ? args.Sender.Position : args.StartPosition;
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) || !IsQMissileName(args.SpellName, args.MissileName)) return;
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (QReturning || IsQReturnName(args.SpellName, args.MissileName) ||
        id == QMissileNetworkId) {
        QActive = false;
        QReturning = false;
        QMissileNetworkId = 0;
        QLastSeenTick = 0;
        QMissilePosition = {};
    }
}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawState", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFB078FFu, 1.4f, 40);
    if (QActive && QMissilePosition.IsValid())
        Drawing::DrawCircle(QMissilePosition, 42.0f,
                            QReturning ? 0xFFFF7AE8u : 0xFF74D7FFu, 1.8f, 32);
    if (RActive && RPortalEndpoint.IsValid())
        Drawing::DrawCircle(RPortalEndpoint, 48.0f, 0xFF9D62FFu, 1.8f, 32);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("ZoeOneTrick", "Zoe one-trick tactics"));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Minimum harass mana", 42, 10, 90));
    BubbleMenu = TacticsMenu->AddSubMenu(new Menu("Bubble", "Sleepy Trouble Bubble"));
    BubbleMenu->Add(new MenuBool("RejectCollision", "Reject minion collision", true));
    PaddleMenu = TacticsMenu->AddSubMenu(new Menu("Paddle", "Paddle Star return geometry"));
    PaddleMenu->Add(new MenuSlider("MinimumReturnScore", "Minimum return hit score", 42, 15, 90));
    PaddleMenu->Add(new MenuBool("PreferRecast", "Prefer long recast after sleep", true));
    StolenMenu = TacticsMenu->AddSubMenu(new Menu("SpellThief", "Stolen spell state"));
    StolenMenu->Add(new MenuBool("UseStolenSpell", "Use observed stolen spell", true));
    PortalMenu = TacticsMenu->AddSubMenu(new Menu("Portal", "Portal endpoint safety"));
    PortalMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at endpoint", 1, 0, 4));
    PortalMenu->Add(new MenuBool("UsePortalSetup", "Use portal to set Paddle angle", true));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("ZoeFarm", "Conservative farming"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum farming mana", 48, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("ZoeCoach", "Visual mechanics coaching"));
    CoachMenu->Add(new MenuBool("DrawState", "Draw Q and Portal state", false));
}

inline void OnLoad() {
    QActive = QReturning = false;
    QCastTick = QTargetId = QMissileNetworkId = QLastSeenTick = 0;
    QCastOrigin = QOutboundEnd = QMissilePosition = {};
    QReturnScore = 0.0f;
    WStolenReady = false;
    WStolenSpell.clear();
    WCastTick = 0;
    RActive = false;
    RCastTick = RReturnTick = 0;
    RPortalEndpoint = RReturnOrigin = {};
    SleepTargetId = SleepExpireTick = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    IncomingThreatUntil = ManualOverrideUntil = 0;
    LastAutoTargetId = LastAutoTick = 0;
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
}

inline void OnUnload() {
    TacticsMenu = BubbleMenu = PaddleMenu = StolenMenu = PortalMenu =
        FarmMenu = CoachMenu = nullptr;
    WStolenSpell.clear();
}

inline constexpr const char* Scenarios[] = {
    "Pin all spell ranges, delays and safety assumptions to Riot 26.15 / CommunityDragon 16.15",
    "Track outbound Paddle Star from process-spell and missile-create events",
    "Recognize Q recast and return missile names without losing outbound state",
    "Reconcile Q state when a missile callback is missed or a player recasts manually",
    "Aim first Q to the target's predicted base-range line",
    "Recast Q only toward a reachable extended endpoint and reject projectile walls",
    "Score Q return intersection at predicted target timing rather than current position",
    "Use Sleepy Trouble Bubble prediction with direct and wall-extended reach",
    "Reject Bubble through minion collision when collision telemetry is observed",
    "Permit wall-extension only inside the researched extended range",
    "Track Sleep buff add, remove and update for guaranteed Q follow-up",
    "Use W only when runtime spellbook exposes an observed stolen spell",
    "Preserve stolen-spell state across event and polling reconciliation",
    "Never guess the cast shape of an unknown stolen spell",
    "Portal only to a non-wall endpoint with endpoint enemy and turret checks",
    "Record the Portal return origin and one-second return endpoint deadline",
    "Use Portal as a Q setup only when it materially improves reach",
    "Do not commit Portal at a dangerous endpoint for nonlethal damage",
    "Combo opens with Bubble, then Q recast/return, then observed stolen spell",
    "Harass preserves mana and avoids unsolicited Portal commits",
    "Flee uses Bubble as peel before a cursor-directed safe Portal",
    "Automatic mode responds only to incoming threat, interrupt, gapcloser or lethal state",
    "Kill secure prioritizes lethal Bubble, Q and observed stolen spell in that order",
    "Hold casts during an AA windup unless the selected posture is lethal or Bubble peel",
    "Respect selected target before orbwalker target and generic selector fallback",
    "Yield to manual Q, W, E or R ownership for the configured reconciliation window",
    "Capture enemy process-spell threat and interruptable channel state",
    "Capture gapcloser endpoint for reactive Bubble posture",
    "Keep lane clear, jungle and last-hit behavior on shared conservative farm policy",
    "Do not use Q or Portal as a farming substitute when no target is selected",
    "Draw projectile and Portal state without changing cast decisions",
    "Never invent shape or damage for an unavailable stolen spell",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Zoe;
    controller.ControllerId = "champion.kuroaio.ai.zoe.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIZoe.md";
    controller.ImplementationSummary =
        "Collision-aware Sleepy Trouble Bubble, outbound/recast/return Paddle Star state, observed Spell Thief variants, wall-safe Portal endpoints, and posture-aware kill-secure behavior.";
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

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnDoCast = &CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    controller.OnAfterAttack = &CaptureAfterAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser = &CaptureGapcloserEvent<
        &GapcloserTargetId, &GapcloserEnd, &GapcloserExpireTick, 475, 720>;
    controller.OnInterruptable = &CaptureInterruptableEvent<
        &InterruptTargetId, &InterruptExpireTick>;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Zoe
