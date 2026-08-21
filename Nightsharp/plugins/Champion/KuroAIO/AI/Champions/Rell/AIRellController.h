#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIRellGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Rell {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;

enum class Posture : std::uint8_t {
    Neutral,
    MountedEngage,
    DismountedPeel,
    TetherGuard,
    Teamfight,
    Flee,
    LaneFarm,
    JungleFarm,
};

enum class Sequence : std::uint8_t {
    None,
    CrashEntry,
    MountEscape,
    TetherStun,
    PullChannel,
    QHeal,
};

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline MountState CurrentMount = MountState::Unknown;
inline Posture CurrentPosture = Posture::Neutral;
inline Sequence ActiveSequence = Sequence::None;
inline bool ETethered = false;
inline bool RChannelActive = false;
inline bool WCrashPending = false;
inline int TetheredAllyId = 0;
inline int LastQTargetId = 0;
inline int LastWTargetId = 0;
inline int LastETargetId = 0;
inline int LastRTargetId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline float LastQHealEstimate = 0.0f;
inline float LastShatterPercent = 0.0f;
inline int RChannelEndTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;

inline constexpr int kTetherGraceMs = 650;
inline constexpr int kRChannelMs = 2000;

inline bool IsQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::Q, {"ShatteringStrike", "RellQ", "RellQAttack"});
}
inline bool IsWEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::W, {"Ferromancy", "RellW", "RellW_Mounted", "RellW_Dismounted"});
}
inline bool IsEEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::E, {"AttractAndRepel", "RellE"});
}
inline bool IsREvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::R, {"MagnetStorm", "RellR"});
}

inline bool MountedBuff(const AIBaseClient& unit) {
    return unit.IsValid() && (unit.HasBuff("RellW_Mounted") ||
                              unit.HasBuff("RellW_MountedArmor") ||
                              unit.HasBuff("RellW"));
}

inline bool DismountedBuff(const AIBaseClient& unit) {
    return unit.IsValid() && (unit.HasBuff("RellW_Dismounted") ||
                              unit.HasBuff("RellW_Dismount") ||
                              unit.HasBuff("RellW2"));
}

inline void ReconcileMountState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        CurrentMount = MountState::Unknown;
        return;
    }
    if (MountedBuff(player)) CurrentMount = MountState::Mounted;
    else if (DismountedBuff(player)) CurrentMount = MountState::Dismounted;
    else if (CurrentMount == MountState::Unknown) CurrentMount = MountState::Mounted;
    if (TetheredAllyId != 0) {
        const auto ally = ControllerHelpers::RawAllyHeroByNetworkId(TetheredAllyId);
        if (ally.IsValid() && (ally.HasBuff("RellE") ||
            ally.HasBuff("RellE_Link") || ally.HasBuff("RellEShield"))) {
            ETethered = true;
        }
    }
}

inline bool TetherBuff(const AIBaseClient& ally) {
    return ally.IsValid() && (ally.HasBuff("RellE") || ally.HasBuff("RellE_Link") ||
                              ally.HasBuff("RellEShield"));
}

inline AIHeroClient TetheredAlly() {
    if (TetheredAllyId != 0) {
        const auto remembered =
            ControllerHelpers::RawAllyHeroByNetworkId(TetheredAllyId);
        if (remembered.IsValid()) return remembered;
    }
    const auto selected = SelectProtectionAlly(1050.0f);
    return selected.IsValid() ? selected : AIHeroClient{};
}

inline AIHeroClient SelectEnemy(float range) {
    return Engine::SelectTarget(range);
}

inline bool AllySafe(const AIHeroClient& ally, bool urgent = false) {
    if (!Engine::ValidAlly(ally, kERange + 100.0f)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int enemies = Engine::CountEnemiesAt(ally.Position(), 650.0f);
    const int allies = Engine::CountAlliesAt(ally.Position(), 750.0f);
    if (Engine::UnderEnemyTurret(ally.Position()) && !urgent) return false;
    return urgent || enemies <= allies + 1 || ally.HealthPercent() <= 38.0f;
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target,
        QRawDamage(SpellRank(0), player.AP()));
}

inline float RDamage(const AIHeroClient& target, int ticks = 8) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target,
        Geometry::RRawDamage(SpellRank(3), player.AP(), ticks));
}

inline bool CanSpend(int index, Mode mode, bool reactive = false) {
    return Ready(index) && SpellEnabled(index, mode) &&
           HasCurrentResource(SpellCost(index)) &&
           ControllerHelpers::CastThrottleReady(index, 38, reactive ? 0 : -1) &&
           (reactive || !ControllerHelpers::PreserveAttack(false));
}

inline bool QTargetPlan(const AIHeroClient& target, Vector3& aim,
                        bool defensive, bool& lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 70.0f)) return false;
    const float travel = QTravelSeconds(player.Position().Distance2D(target.Position()));
    aim = PredictPosition(target, travel);
    if (!aim.IsValid() || aim.IsZero()) return false;
    if (ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    lethal = ControllerHelpers::Lethal(target, QDamage(target));
    QContext context{};
    context.Ready = Ready(0);
    context.TargetValid = true;
    context.CollisionFree = QLineHits(
        player.Position(), aim, aim, target.BoundingRadius());
    context.ProjectileWall = false;
    context.TargetSpellShielded = HasSpellShieldOrImmunity(target);
    context.UnderEnemyTurret = Engine::UnderEnemyTurret(aim);
    context.Defensive = defensive;
    context.Lethal = lethal;
    context.NearbyEnemies = Engine::CountEnemiesAt(aim, 500.0f);
    context.MaximumEnemies = Slider(QMenu, "MaxQEnemies", 3);
    return QSafe(context);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool defensive = false) {
    if (!CanSpend(0, mode, defensive) || PlayerMobilityLocked()) return false;
    Vector3 aim{};
    bool lethal = false;
    if (!QTargetPlan(target, aim, defensive, lethal)) return false;
    if (Engine::ControllerCastPosition(0, aim)) {
        LastQCastTick = Now();
        LastQTargetId = static_cast<int>(target.NetworkId());
        const auto ally = TetheredAlly();
        const float missing = ally.IsValid()
            ? std::max(0.0f, ally.MaxHealth() - ally.Health()) : 0.0f;
        LastQHealEstimate = QHeal(SpellRank(0), missing);
        LastShatterPercent = PassiveShred(SpellRank(0));
        ActiveSequence = defensive ? Sequence::QHeal : Sequence::TetherStun;
        return true;
    }
    return false;
}

inline bool CastCrash(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (CurrentMount != MountState::Mounted || !CanSpend(1, mode, reactive) ||
        PlayerMobilityLocked() || !Engine::ValidEnemy(target, kWCrashRange + 80.0f)) return false;
    const auto player = GameObjects::Player();
    const Vector3 endpoint = CrashEndpoint(player.Position(), PredictPosition(target, 0.25f));
    if (!CrashEndpointSafe(endpoint, true, Engine::UnderEnemyTurret(endpoint),
                           reactive, ControllerHelpers::Lethal(target, QDamage(target)),
                           ProjectileWallBlocksFromPlayer(endpoint, 40.0f),
                           HasReadyDashHazardAt(endpoint),
                           Engine::CountEnemiesAt(endpoint, 500.0f),
                           Slider(WMenu, "MaxCrashEnemies", 2))) return false;
    if (Engine::ControllerCastPosition(1, endpoint)) {
        LastWCastTick = Now();
        LastWTargetId = static_cast<int>(target.NetworkId());
        WCrashPending = true;
        ActiveSequence = Sequence::CrashEntry;
        return true;
    }
    return false;
}

inline bool CastMount(const AIHeroClient& ally, Mode mode, bool urgent = false) {
    if (CurrentMount != MountState::Dismounted || !CanSpend(1, mode, urgent) ||
        PlayerMobilityLocked() || !AllySafe(ally, urgent)) return false;
    const auto player = GameObjects::Player();
    if (!MountUpReachable(player.Position(), ally.Position(), true, false,
                          AllySafe(ally, urgent))) return false;
    if (Engine::ControllerCastUnit(1, ally)) {
        LastWCastTick = Now();
        LastWTargetId = static_cast<int>(ally.NetworkId());
        ActiveSequence = Sequence::MountEscape;
        return true;
    }
    return false;
}

inline bool CastTether(const AIHeroClient& ally, const AIHeroClient& enemy,
                       Mode mode, bool urgent = false) {
    if (!Engine::ValidAlly(ally, kERange + 100.0f) || !AllySafe(ally, urgent) ||
        !CanSpend(2, mode, urgent)) return false;
    const auto player = GameObjects::Player();
    if (!TetherValid(player.Position(), ally.Position(), true,
                     AllySafe(ally, urgent))) return false;
    if (enemy.IsValid() && !TetherStunHits(player.Position(), ally.Position(),
                                           PredictPosition(enemy, 0.25f),
                                           enemy.BoundingRadius()) && !urgent) {
        return false;
    }
    if (Engine::ControllerCastUnit(2, ally)) {
        LastECastTick = Now();
        LastETargetId = enemy.IsValid() ? static_cast<int>(enemy.NetworkId()) : 0;
        TetheredAllyId = static_cast<int>(ally.NetworkId());
        ETethered = true;
        ActiveSequence = Sequence::TetherStun;
        return true;
    }
    return false;
}

inline bool CastMagnet(Mode mode, bool defensive = false,
                       bool urgent = false) {
    if (!CanSpend(3, mode, urgent) || PlayerMobilityLocked()) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius + 70.0f);
    const int allies = Engine::CountAlliesAt(player.Position(), 750.0f);
    const RContext context{
        true, RChannelActive, Engine::UnderEnemyTurret(player.Position()),
        urgent, defensive, defensive, enemies, allies,
        Slider(RMenu, "MinimumTargets", 2), Slider(RMenu, "MaxEnemyAdvantage", 1)};
    if (!MagneticPullSafe(context) || !AllyChannelSafe(enemies, allies,
                                                        context.UnderEnemyTurret,
                                                        urgent)) return false;
    if (Engine::ControllerCastSelf(3)) {
        LastRCastTick = Now();
        RChannelEndTick = LastRCastTick + kRChannelMs;
        RChannelActive = true;
        LastRTargetId = enemies > 0 ? static_cast<int>(Engine::SelectTarget(500.0f).NetworkId()) : 0;
        ActiveSequence = Sequence::PullChannel;
        return true;
    }
    return false;
}

inline bool TryPeel(const AIHeroClient& threat, Mode mode) {
    const auto ally = TetheredAlly();
    if (!ally.IsValid()) return false;
    const bool urgent = ally.HealthPercent() <= Slider(EMenu, "EmergencyAllyHp", 38) ||
                        IncomingHardCcUntil >= Now();
    if (Engine::ValidEnemy(threat, kQRange + 50.0f) &&
        CastTether(ally, threat, mode, urgent)) return true;
    if (Engine::ValidEnemy(threat, kQRange + 50.0f) &&
        CastQ(threat, mode, true)) return true;
    if (CurrentMount == MountState::Dismounted && CastMount(ally, mode, true)) return true;
    return Engine::ValidEnemy(threat, kRRadius + 80.0f) && CastMagnet(mode, true, true);
}

inline bool TryCombo(const AIHeroClient& target, Mode mode) {
    const auto ally = TetheredAlly();
    if (ally.IsValid() && ETethered &&
        CastTether(ally, target, mode)) return true;
    if (ally.IsValid() && !ETethered && CastTether(ally, target, mode)) return true;
    if (CurrentMount == MountState::Mounted && CastCrash(target, mode)) return true;
    if (CastQ(target, mode)) return true;
    return CastMagnet(mode, false, false);
}

inline bool TryHarass(const AIHeroClient& target, Mode mode) {
    if (PlayerManaPercent() < Slider(QMenu, "HarassMana", 55)) return false;
    if (CurrentMount == MountState::Mounted && InAutoAttackRange(target) &&
        CastCrash(target, mode)) return true;
    return CastQ(target, mode);
}
inline bool TryFlee(const AIHeroClient& threat, Mode mode) {
    const auto ally = TetheredAlly();
    if (ally.IsValid() && CurrentMount == MountState::Dismounted &&
        CastMount(ally, mode, true)) return true;
    if (ally.IsValid() && !ETethered && CastTether(ally, threat, mode, true)) return true;
    if (Engine::ValidEnemy(threat, kRRadius + 80.0f)) return CastMagnet(mode, true, true);
    return Engine::ValidEnemy(threat, kQRange + 50.0f) && CastQ(threat, mode, true);
}

inline bool TryFarm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PlayerManaPercent() <
        Slider(FarmMenu, "FarmMana", 35)) return false;
    AIBaseClient best{};
    float bestHealth = -FLT_MAX;
    const auto& units = mode == Mode::Jungle
        ? GameObjects::Jungle()
        : GameObjects::EnemyMinions();
    for (const auto& unit : units) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) > kQRange) continue;
        if (unit.Health() > bestHealth) {
            best = unit;
            bestHealth = unit.Health();
        }
    }
    if (!best.IsValid()) return false;
    const Vector3 aim = PredictPosition(best, QTravelSeconds(
        player.Position().Distance2D(best.Position())));
    if (!aim.IsValid() || aim.IsZero() ||
        ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    return CanSpend(0, mode) && Engine::ControllerCastPosition(0, aim);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    (void)selected;
    ReconcileMountState();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    if (RChannelActive && Now() >= RChannelEndTick) RChannelActive = false;
    const auto target = SelectEnemy(std::max(kQRange, kRRadius) + 100.0f);
    const auto threat = ControllerHelpers::NearestEnemyToPlayer(target, 900.0f);
    CurrentPosture = mode == Mode::Flee ? Posture::Flee :
        (RChannelActive ? Posture::Teamfight :
         (CurrentMount == MountState::Mounted ? Posture::MountedEngage : Posture::DismountedPeel));
    if (RChannelActive) return true;
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        CurrentPosture = mode == Mode::Jungle ? Posture::JungleFarm : Posture::LaneFarm;
        (void)TryFarm(mode);
        return true;
    }
    if (mode == Mode::Flee) { (void)TryFlee(threat, mode); return true; }
    const auto ally = TetheredAlly();
    if (ally.IsValid() && (ally.HealthPercent() <= 38.0f ||
        Engine::CountEnemiesAt(ally.Position(), 550.0f) > Engine::CountAlliesAt(ally.Position(), 700.0f))) {
        if (TryPeel(threat, mode)) return true;
    }
    if (!target.IsValid()) return true;
    if (mode == Mode::Harass) { (void)TryHarass(target, mode); return true; }
    if (mode == Mode::Combo || mode == Mode::Automatic) { (void)TryCombo(target, mode); return true; }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto analysis = ControllerHelpers::AnalyzeEnemyCast(
            args, 220.0f, 100.0f, 250, 260, 240, 1500, 450);
        if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
            IncomingThreatUntil = now + 1300;
            if (analysis.LikelyHardCrowdControl) IncomingHardCcUntil = now + 900;
        }
        return;
    }
    const int slot = args.Slot;
    if (IsQEvent(args)) {
        LastQCastTick = now;
        LastQTargetId = static_cast<int>(args.TargetNetworkId ? args.TargetNetworkId : args.Target.NetworkId);
    } else if (IsWEvent(args)) {
        LastWCastTick = now;
        WCrashPending = CurrentMount == MountState::Mounted;
    } else if (IsEEvent(args)) {
        LastECastTick = now;
        ETethered = true;
        if (args.TargetNetworkId) TetheredAllyId = static_cast<int>(args.TargetNetworkId);
    } else if (IsREvent(args)) {
        LastRCastTick = now;
        RChannelActive = true;
        RChannelEndTick = now + kRChannelMs;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    int targetId = 0;
    int attackTick = 0;
    if (CaptureLocalAutoAttack(args, targetId, attackTick)) {
        LastAutoTargetId = targetId;
        LastAutoTick = attackTick;
    }
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (ControllerHelpers::TextContainsAny(args.BuffName,
                {"RellW_Mounted", "RellW_MountedArmor"})) {
            CurrentMount = added ? MountState::Mounted : MountState::Unknown;
        } else if (ControllerHelpers::TextContainsAny(args.BuffName,
                {"RellW_Dismounted", "RellW_Dismount", "RellW2"})) {
            CurrentMount = added ? MountState::Dismounted : MountState::Unknown;
        } else if (ControllerHelpers::TextContainsAny(args.BuffName,
                {"RellR", "MagnetStorm"})) {
            RChannelActive = added;
            RChannelEndTick = added ? Now() + kRChannelMs : 0;
        }
        return;
    }
    const int allyId = static_cast<int>(args.Sender.NetworkId);
    if (allyId == TetheredAllyId && ControllerHelpers::TextContainsAny(
            args.BuffName, {"RellE", "RellE_Link", "RellEShield"})) {
        ETethered = added;
        if (!added) TetheredAllyId = 0;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (RChannelActive || PlayerMobilityLocked() ||
        (CurrentMount == MountState::Mounted && WCrashPending)) {
        args.Process = false;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (ControllerHelpers::CaptureGapcloser(args, GapcloserTargetId,
                                             GapcloserEndpoint,
                                             GapcloserExpireTick, 600.0f, 900)) {
        IncomingThreatUntil = Now() + 1000;
    }
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    (void)ControllerHelpers::CaptureInterruptable(args, InterruptTargetId,
                                                   InterruptExpireTick, 900, 120, 2600);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawQ", false)) Drawing::DrawCircle(
        player.Position(), kQRange, 0x665CD6FFu, 1.4f, 64);
    if (Bool(CoachMenu, "DrawE", false)) {
        const auto ally = TetheredAlly();
        if (ally.IsValid()) {
            Drawing::DrawLine(player.Position(), ally.Position(),
                              ETethered ? 0xFF69D2FFu : 0x665A8DFFu, 2.2f);
            Drawing::DrawCircle(ally.Position(), 80.0f, 0xAA69D2FFu, 1.5f, 36);
        }
    }
    if (Bool(CoachMenu, "DrawR", false)) Drawing::DrawCircle(
        player.Position(), kRRadius, RChannelActive ? 0xAAFFD166u : 0x445C9CFFu,
        2.0f, 72);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("RellOneTrick", "Rell one-trick mechanics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("ShatteringStrike", "Q prediction, shatter and heal"));
    QMenu->Add(new MenuSlider("MaxQEnemies", "Maximum Q endpoint enemies", 3, 1, 6));
    QMenu->Add(new MenuSlider("HarassMana", "Minimum harass mana (%)", 55, 15, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("Ferromancy", "Mounted crash and dismounted mount"));
    WMenu->Add(new MenuSlider("MaxCrashEnemies", "Maximum crash endpoint enemies", 2, 1, 5));
    EMenu = TacticsMenu->AddSubMenu(new Menu("AttractRepel", "Ally tether and line stun"));
    EMenu->Add(new MenuSlider("EmergencyAllyHp", "Emergency ally health (%)", 38, 10, 75));
    RMenu = TacticsMenu->AddSubMenu(new Menu("MagnetStorm", "Pull channel and ally safety"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum R enemies", 2, 1, 5));
    RMenu->Add(new MenuSlider("MaxEnemyAdvantage", "Maximum enemy advantage", 1, 0, 3));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Conservative lane and jungle Q"));
    FarmMenu->Add(new MenuSlider("FarmMana", "Minimum farm mana (%)", 35, 0, 80));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Rell state coaching"));
    CoachMenu->Add(new MenuBool("DrawQ", "Draw Q range", false));
    CoachMenu->Add(new MenuBool("DrawE", "Draw tether and ally", false));
    CoachMenu->Add(new MenuBool("DrawR", "Draw magnetic storm", false));
}

inline void OnLoad() {
    CurrentMount = MountState::Unknown;
    CurrentPosture = Posture::Neutral;
    ActiveSequence = Sequence::None;
    ETethered = false;
    RChannelActive = false;
    WCrashPending = false;
    TetheredAllyId = LastQTargetId = LastWTargetId = LastETargetId = LastRTargetId = 0;
    LastAutoTargetId = LastAutoTick = LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastQHealEstimate = LastShatterPercent = 0.0f;
    RChannelEndTick = IncomingThreatUntil = IncomingHardCcUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = 0;
    ReconcileMountState();
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Reconcile mounted and dismounted Ferromancy from both buffs and polling",
    "Use Q predicted line impact with width, range, projectile-wall and spell-shield gates",
    "Heal missing health while shattering the target's stolen resistances on confirmed Q hit",
    "Crash W only on a reachable endpoint with turret, enemy-count and unsafe-dash gates",
    "Mount W toward a safe tethered ally rather than inventing movement",
    "Maintain E ally tether state and only seek line stun when the ally route is safe",
    "Protect the carry before using E or Q for a proactive engage",
    "Use R magnetic pull as a channel with minimum enemies and ally-safety accounting",
    "Avoid R under enemy turret unless urgent peel or verified lethal pressure justifies it",
    "Track incoming casts, gapclosers and interruptible channels for reactive peel",
    "Respect attack windup and suppress autos only during W crash or R channel ownership",
    "Reconcile local casts and buff events with bounded polling fallbacks",
    "Reserve Q and W resources for defensive ally safety when mounted state is uncertain",
    "Distinct Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic decisions",
    "Never issue movement, attack-move, Flash, ward or Smite commands",
};

inline constexpr ChampionController Controller = [] {
    ChampionController c{};
    c.ChampionId = SDK::ChampionId::Rell;
    c.ControllerId = "champion.kuroaio.ai.rell.onetrick";
    c.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    c.ResearchArtifact = "AI/Research/AIRell.md";
    c.ImplementationSummary =
        "Event and polling reconciled mount state, predicted Q shatter/heal, W crash and mount safety, E ally tether line stun, and R magnetic pull channel with ally protection.";
    c.Scenarios = Scenarios;
    c.ScenarioCount = std::size(Scenarios);
    c.OwnsDecisionLoop = true;
    c.OnLoad = &OnLoad;
    c.OnUnload = &OnUnload;
    c.BuildMenu = &BuildMenu;
    c.OnUpdate = &OnUpdate;
    c.OnDraw = &OnDraw;
    c.OnProcessSpell = &OnProcessSpell;
    c.OnDoCast = &OnDoCast;
    c.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    c.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;

    c.OnBeforeAttack = &OnBeforeAttack;
    c.OnAfterAttack = &OnAfterAttack;
    c.OnGapcloser = &OnGapcloser;
    c.OnInterruptable = &OnInterruptable;
    c.OnObjectCreate = &OnObjectCreate;
    c.OnObjectDelete = &OnObjectDelete;
    c.OnMissileCreate = &OnMissileCreate;
    c.OnMissileDelete = &OnMissileDelete;
    return c;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Rell
