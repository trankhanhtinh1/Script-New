#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIViegoGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Viego {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::OrbwalkerHeroTarget;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::UnitByNetworkId;

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* MawMenu = nullptr;
inline Menu* MistMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline bool Possessed = false;
inline bool PossessionCasting = false;
inline bool Untargetable = false;
inline bool InMist = false;
inline bool WChargeOwned = false;
inline bool ForcedOrbwalkerTarget = false;
inline int PossessionStartTick = 0;
inline int PossessionExpireTick = 0;
inline int UntargetableUntil = 0;
inline int WChargeStartTick = 0;
inline int WTargetId = 0;
inline int QMarkTargetId = 0;
inline int QMarkExpireTick = 0;
inline int SoulNetworkId = 0;
inline int SoulExpireTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int PlayerOverrideUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline Vector3 SoulPosition = {};
inline Vector3 MistCastPosition = {};
inline Vector3 LastRLanding = {};

using ControllerHelpers::Now;
inline Vec2 ToVec2(const Vector3& value) { return { value.x, value.z }; }

inline bool RuntimeNameContains(int index, const char* token) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || index < 0 || index >= 4) return false;
    const auto spell = player.Spellbook().GetSpell(static_cast<SDK::SpellSlot>(index));
    return spell.IsValid() && ControllerHelpers::AnyTextContains(
        { spell.Name().c_str(), spell.ScriptName().c_str(), spell.IconName().c_str(),
          Engine::RuntimeSpellNames[index].c_str() }, { token });
}

inline bool NativeSpellNames() {
    return RuntimeNameContains(0, "ViegoQ") && RuntimeNameContains(1, "ViegoW") &&
           RuntimeNameContains(2, "ViegoE");
}
inline bool NativeForm() { return !Possessed && NativeSpellNames(); }

inline bool Ready(int index, Mode mode) {
    if (index < 0 || index >= 4 || !Engine::RuntimeSpells[index] ||
        !Engine::RuntimeSpells[index]->IsReady() || !SpellEnabled(index, mode)) return false;
    if (index < 3) return NativeForm();
    return NativeForm() || (Possessed && RuntimeNameContains(3, "ViegoR"));
}

inline bool Throttle(int index, int delay) {
    const int tick = index == 0 ? QCastTick : index == 1 ? WCastTick :
        index == 2 ? ECastTick : RCastTick;
    return Now() - tick >= delay;
}

inline PassiveState CurrentPassiveState() {
    return { PossessionCasting, Untargetable, Possessed, NativeSpellNames() };
}

inline bool TargetDamageable(const AIHeroClient& target, bool projectile = false) {
    if (!Engine::ValidEnemy(target) || IsCommonUntargetableOrImmune(target)) return false;
    if (projectile && ControllerHelpers::HasSpellShieldOrImmunity(target)) return false;
    return !target.HasBuff("JaxCounterStrike") && !target.HasBuff("kindredrnodeathbuff") &&
           !target.HasBuff("TryndamereUndyingRage");
}

inline float SpellDamage(int index, const AIHeroClient& target) {
    return index >= 0 && index < 4 && Engine::RuntimeSpells[index] &&
           Engine::ValidEnemy(target) ? Engine::RuntimeSpells[index]->GetDamage(target) : 0.0f;
}
inline bool Lethal(int index, const AIHeroClient& target) {
    return TargetDamageable(target) && SpellDamage(index, target) >=
           target.Health() + target.AllShield();
}

inline bool SafePosition(const Vector3& position, int maximumEnemies, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid() || position.IsZero() ||
        SDK::NavMesh::IsWall(position)) return false;
    if (Engine::UnderEnemyTurret(position) && !Engine::UnderEnemyTurret(player.Position()))
        return false;
    const int enemies = Engine::CountEnemiesAt(position, 575.0f);
    const int allies = Engine::CountAlliesAt(position, 650.0f);
    return enemies <= maximumEnemies + (defensive ? 1 : 0) &&
           (defensive || enemies <= allies + 1);
}

inline bool DashPathBlocked(const Vector3& origin, const Vector3& endpoint) {
    const float distance = origin.Distance2D(endpoint);
    if (distance <= 1.0f) return true;
    const Vector3 direction = SharedGeometry::Direction2D(origin, endpoint);
    for (float offset = 24.0f; offset <= distance; offset += 24.0f)
        if (SDK::NavMesh::IsWall(origin + direction * offset)) return true;
    return false;
}

inline bool WCharging() {
    const auto player = GameObjects::Player();
    return Engine::RuntimeSpells[1] &&
        (Engine::RuntimeSpells[1]->IsCharging() ||
         (player.IsValid() && (player.HasBuff("ViegoW") || player.HasBuff("ViegoWCharge"))));
}
inline float WChargeSeconds() {
    return WChargeStartTick > 0 ? static_cast<float>(std::max(0, Now() - WChargeStartTick)) /
        1000.0f : 0.0f;
}
inline void ClearWCharge() { WChargeOwned = false; WChargeStartTick = WTargetId = 0; }
inline void ClearForcedTarget() {
    if (ForcedOrbwalkerTarget) Orbwalker::ForceTarget(AttackableUnit());
    ForcedOrbwalkerTarget = false;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    const bool casting = player.HasBuff("ViegoPassiveCasting");
    const bool transformed = player.HasBuff("ViegoPassiveTransform");
    PossessionCasting = casting || (PossessionCasting && now < UntargetableUntil);
    Untargetable = casting || now < UntargetableUntil || player.IsInvulnerable() ||
                    !player.IsTargetable();
    if (transformed || (!NativeSpellNames() && !casting && PossessionStartTick > 0)) {
        if (!Possessed) {
            PossessionStartTick = now;
            PossessionExpireTick = now + kPossessionDurationMs;
        }
        Possessed = true;
    } else if (Possessed && NativeSpellNames() && now > UntargetableUntil + 120) {
        Possessed = false;
        PossessionStartTick = PossessionExpireTick = 0;
    }
    InMist = player.HasBuff("ViegoEMist") || player.HasBuff("ViegoEHaste");
    if (QMarkExpireTick > 0 && now >= QMarkExpireTick)
        QMarkTargetId = QMarkExpireTick = 0;
    if (SoulExpireTick > 0 && now >= SoulExpireTick) {
        SoulNetworkId = SoulExpireTick = 0;
        SoulPosition = {};
    }
    if (WCharging()) {
        if (WChargeStartTick == 0) {
            WChargeStartTick = now;
            WChargeOwned = false;
            PlayerOverrideUntil = std::max(PlayerOverrideUntil, now + 500);
        }
    } else if (WChargeStartTick > 0 && now - WChargeStartTick > 180) ClearWCharge();
}

inline AIHeroClient CooperatingTarget(const AIHeroClient& selected, float range = 1250.0f) {
    if (Engine::ValidEnemy(selected, range) && TargetDamageable(selected)) return selected;
    const auto orb = OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range) && TargetDamageable(orb)) return orb;
    if (QMarkTargetId != 0) {
        const auto marked = HeroByNetworkId(QMarkTargetId);
        if (Engine::ValidEnemy(marked, range) && TargetDamageable(marked)) return marked;
    }
    return Engine::SelectTarget(range);
}

inline void CooperateWithMarkedAuto(const AIHeroClient& preferred) {
    AIHeroClient marked = HeroByNetworkId(QMarkTargetId);
    if (!Engine::ValidEnemy(marked, 650.0f) || !marked.HasBuff("ViegoQMark") ||
        !TargetDamageable(marked)) {
        if (Engine::ValidEnemy(preferred, 650.0f) && preferred.HasBuff("ViegoQMark"))
            marked = preferred;
        else { ClearForcedTarget(); return; }
    }
    Orbwalker::ForceTarget(AttackableUnit(marked.Handle()));
    ForcedOrbwalkerTarget = true;
    QMarkTargetId = static_cast<int>(marked.NetworkId());
    QMarkExpireTick = std::max(QMarkExpireTick, Now() + 250);
}

inline bool StartW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!MayIssueNativeKitCast(CurrentPassiveState()) || WCharging() || !Ready(1, mode) ||
        !Throttle(1, 80) || !TargetDamageable(target, true) ||
        !Engine::ValidEnemy(target, kWMaxRange + 50.0f)) return false;
    const auto player = GameObjects::Player();
    const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target, false, kWMaxRange);
    Vector3 cast = prediction.GetCastPosition();
    if (!cast.IsValid() || cast.IsZero()) cast = PredictPosition(target, 0.20f);
    if (!cast.IsValid() || cast.IsZero()) return false;
    const Vector3 dashEnd = player.Position().Extend(cast, kWDashDistance);
    if (DashPathBlocked(player.Position(), dashEnd) ||
        !SafePosition(dashEnd, Slider(MawMenu, "MaxDashEnemies", 2), reactive)) return false;
    Engine::ArmControllerCast(1);
    if (!Engine::RuntimeSpells[1]->StartCharging(cast)) {
        Engine::CancelControllerCast(1);
        return false;
    }
    Engine::MarkSuccessfulCast(1);
    WChargeOwned = true;
    WChargeStartTick = WCastTick = Now();
    WTargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline bool ReleaseW(const AIHeroClient& fallback, bool reactive = false) {
    if (!WCharging() || !WChargeOwned || !Engine::RuntimeSpells[1] || Untargetable || Possessed)
        return false;
    AIHeroClient target = HeroByNetworkId(WTargetId);
    if (!TargetDamageable(target, true)) target = fallback;
    if (!TargetDamageable(target, true)) return false;
    const auto player = GameObjects::Player();
    const float range = WRange(WChargeSeconds());
    const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target, false, range);
    Vector3 cast = prediction.GetCastPosition();
    if (!cast.IsValid() || cast.IsZero()) cast = PredictPosition(target, 0.20f);
    const Vector3 dashEnd = player.Position().Extend(cast, kWDashDistance);
    WReleaseContext context{};
    context.Charging = true;
    context.TargetDamageable = TargetDamageable(target, true);
    context.PredictionHits = cast.IsValid() && !cast.IsZero() &&
        player.Position().Distance2D(cast) <= range + target.BoundingRadius() &&
        prediction.Hitchance >= SDK::HitChance::High;
    context.Collision = !prediction.CollisionObjects.empty();
    context.DashPathBlocked = DashPathBlocked(player.Position(), dashEnd);
    context.DashEndpointSafe = SafePosition(dashEnd, Slider(MawMenu, "MaxDashEnemies", 2), reactive);
    context.TargetInCurrentRange = player.Position().Distance2D(target.Position()) <=
        range + target.BoundingRadius();
    context.TargetInAttackRange = player.Position().Distance2D(target.Position()) <=
        player.AttackRange() + target.BoundingRadius() + 35.0f;
    context.Lethal = Lethal(1, target);
    context.Peel = reactive;
    context.ChargeExpiring = WChargeSeconds() >= 0.96f;
    if (!ShouldReleaseW(context)) return false;
    Engine::ArmControllerCast(1);
    if (!Engine::RuntimeSpells[1]->ShootChargedSpell(cast)) {
        Engine::CancelControllerCast(1);
        return false;
    }
    Engine::MarkSuccessfulCast(1);
    WCastTick = Now();
    ClearWCharge();
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool execute = false) {
    if (!MayIssueNativeKitCast(CurrentPassiveState()) || !Ready(0, mode) ||
        !Throttle(0, 45) || !TargetDamageable(target) ||
        !Engine::ValidEnemy(target, kQRange + 35.0f)) return false;
    const auto player = GameObjects::Player();
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    Vector3 cast = prediction.GetCastPosition();
    if (!cast.IsValid() || cast.IsZero()) cast = PredictPosition(target, 0.25f);
    if (!cast.IsValid() || cast.IsZero() ||
        player.Position().Distance2D(cast) > kQRange + target.BoundingRadius() ||
        prediction.Hitchance < SDK::HitChance::High) return false;
    const bool marked = target.HasBuff("ViegoQMark");
    const bool attackReady = player.Position().Distance2D(target.Position()) <=
        player.AttackRange() + target.BoundingRadius() + 25.0f && Orbwalker::CanAttack();
    if (!execute && (Orbwalker::IsWindingUp() || (marked && attackReady))) return false;
    if (!Engine::ControllerCastPosition(0, cast)) return false;
    QCastTick = Now();
    QMarkTargetId = static_cast<int>(target.NetworkId());
    QMarkExpireTick = Now() + kQMarkDurationMs;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool retreating = false) {
    if (!MayIssueNativeKitCast(CurrentPassiveState()) || !Ready(2, mode) || !Throttle(2, 80))
        return false;
    const auto player = GameObjects::Player();
    Vector3 desired = retreating ? Game::CursorPos() :
        (Engine::ValidEnemy(target) ? PredictPosition(target, 0.20f) : Game::CursorPos());
    desired = player.Position().Extend(desired, std::min(kERange,
        player.Position().Distance2D(desired)));
    MistContext context{};
    context.NativeForm = NativeForm();
    context.Ready = true;
    context.NearTerrain = ControllerHelpers::NearTerrain(desired, 190.0f, 20) ||
                          ControllerHelpers::NearTerrain(player.Position(), 230.0f, 24);
    context.CastPointValid = desired.IsValid() && !desired.IsZero();
    context.AlreadyInMist = InMist;
    context.Pursuing = Engine::ValidEnemy(target) &&
        player.Position().Distance2D(target.Position()) > player.AttackRange();
    context.Retreating = retreating;
    context.Outnumbered = Engine::CountEnemiesAt(player.Position(), 700.0f) >
                          Engine::CountAlliesAt(player.Position(), 750.0f) + 1;
    if (!ShouldCastMist(context) || !Engine::ControllerCastPosition(2, desired)) return false;
    ECastTick = Now();
    MistCastPosition = desired;
    return true;
}

inline std::vector<RBody> RTargets() {
    std::vector<RBody> result;
    result.reserve(GameObjects::EnemyHeroes().size());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.IsDead()) continue;
        result.push_back({ static_cast<int>(enemy.NetworkId()), ToVec2(enemy.Position()),
            enemy.BoundingRadius(), std::clamp(enemy.HealthPercent() / 100.0f, 0.0f, 1.0f),
            true, TargetDamageable(enemy) });
    }
    return result;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const PassiveState state = CurrentPassiveState();
    if ((!MayIssueNativeKitCast(state) && !MayIssuePossessionRecast(state)) ||
        !Ready(3, mode) || !Throttle(3, 100) || !TargetDamageable(target)) return false;
    const auto player = GameObjects::Player();
    Vector3 desired = defensive ? Game::CursorPos() : PredictPosition(target, 0.50f);
    if (!desired.IsValid() || desired.IsZero()) desired = target.Position();
    const Vector3 landing = player.Position().Extend(desired,
        std::min(kRRange, player.Position().Distance2D(desired)));
    const auto bodies = RTargets();
    const bool lethal = Lethal(3, target) || target.HealthPercent() <=
        Slider(UltimateMenu, "RExecuteHP", 32);
    RContext context{};
    context.Ready = true;
    context.TargetDamageable = TargetDamageable(target);
    context.LandingValid = landing.IsValid() && !landing.IsZero() && !SDK::NavMesh::IsWall(landing);
    context.LandingSafe = SafePosition(landing, Slider(UltimateMenu, "MaxREnemies", 2),
                                       defensive || lethal);
    context.IntendedTargetIsPrimary = RPrimaryTarget(ToVec2(landing), bodies) ==
                                      static_cast<int>(target.NetworkId());
    context.Lethal = lethal;
    context.Defensive = defensive;
    context.Possessed = Possessed;
    context.PossessionExpiring = Possessed && PossessionExpireTick > 0 &&
        PossessionExpireTick - Now() <= Slider(PassiveMenu, "ExitBeforeMs", 900);
    context.PossessionDangerous = Possessed &&
        (player.HealthPercent() <= Slider(PassiveMenu, "DangerHP", 28) ||
         Engine::CountEnemiesAt(player.Position(), 650.0f) >
         Engine::CountAlliesAt(player.Position(), 700.0f) + 1);
    context.EnemyHits = RHitCount(ToVec2(landing), bodies);
    if (!ShouldCastR(context) || !Engine::ControllerCastPosition(3, landing)) return false;
    RCastTick = Now();
    LastRLanding = landing;
    if (Possessed) { Possessed = false; PossessionStartTick = PossessionExpireTick = 0; }
    return true;
}

inline bool TrySoul() {
    if (Possessed || PossessionCasting || SoulNetworkId == 0 || SoulExpireTick <= Now()) return false;
    const auto player = GameObjects::Player();
    const AIBaseClient soul = UnitByNetworkId(SoulNetworkId);
    const bool attackable = soul.IsValid() && !soul.IsDead() && soul.IsTargetable();
    const Vector3 position = attackable ? soul.Position() : SoulPosition;
    SoulContext context{};
    context.Exists = position.IsValid() && !position.IsZero();
    context.Attackable = attackable;
    context.InRange = player.Position().Distance2D(position) <=
        player.AttackRange() + (attackable ? soul.BoundingRadius() : 65.0f) + 35.0f;
    context.Safe = SafePosition(position, Slider(PassiveMenu, "MaxSoulEnemies", 2), true);
    context.PlayerLow = player.HealthPercent() <= Slider(PassiveMenu, "TakeSoulHP", 72);
    context.CurrentFightWonWithoutSoul = Engine::CountEnemiesAt(player.Position(), 700.0f) == 0;
    context.SoulExpiring = SoulExpireTick - Now() <= 900;
    if (!ShouldTakeSoul(context)) return false;
    Orbwalker::ForceTarget(AttackableUnit(soul.Handle()));
    ForcedOrbwalkerTarget = true;
    return true;
}

inline bool TryReactive(const AIHeroClient& fallback) {
    const bool gap = GapcloserExpireTick >= Now();
    const bool interrupt = InterruptExpireTick >= Now();
    AIHeroClient target = gap ? HeroByNetworkId(GapcloserTargetId) :
        interrupt ? HeroByNetworkId(InterruptTargetId) : fallback;
    if (!Engine::ValidEnemy(target, kWMaxRange + 80.0f)) return false;
    if (WCharging()) return ReleaseW(target, true);
    if (StartW(target, Mode::Automatic, true)) return true;
    return gap && CastR(target, Mode::Automatic, true);
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!TargetDamageable(target)) return false;
    if (NativeForm()) {
        if (Lethal(0, target) && CastQ(target, mode, true)) return true;
        if (Lethal(1, target)) {
            if (WCharging()) return ReleaseW(target);
            if (StartW(target, mode)) return true;
        }
    }
    return Lethal(3, target) && CastR(target, mode);
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!TargetDamageable(target)) return false;
    if (WCharging()) return ReleaseW(target);
    if (target.HasBuff("ViegoQMark") && Orbwalker::CanAttack()) {
        CooperateWithMarkedAuto(target);
        return false;
    }
    if (CastE(target, Mode::Combo)) return true;
    if (StartW(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    return target.HealthPercent() <= Slider(UltimateMenu, "RExecuteHP", 32) &&
           CastR(target, Mode::Combo);
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!TargetDamageable(target) || Possessed) return false;
    if (WCharging()) return ReleaseW(target);
    if (target.HasBuff("ViegoQMark") && Orbwalker::CanAttack()) {
        CooperateWithMarkedAuto(target);
        return false;
    }
    if (CastQ(target, Mode::Harass)) return true;
    return Bool(MawMenu, "HarassW", false) && StartW(target, Mode::Harass);
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (Possessed) return TargetDamageable(threat) && CastR(threat, Mode::Flee, true);
    if (WCharging()) return ReleaseW(threat, true);
    if (CastE(threat, Mode::Flee, true)) return true;
    if (TargetDamageable(threat) && StartW(threat, Mode::Flee, true)) return true;
    return TargetDamageable(threat) && CastR(threat, Mode::Flee, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const AIHeroClient target = CooperatingTarget(selected);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 1000.0f);
    if (PossessionCasting || Untargetable) return true;
    if (WCharging() && !WChargeOwned) return true;
    if (TrySoul()) return true;
    if (PlayerOverrideUntil > Now()) return true;
    CooperateWithMarkedAuto(target);
    if (TryReactive(threat)) return true;
    if (mode == Mode::Flee) { (void)TryFlee(threat); return true; }
    if (Possessed) { (void)CastR(target, mode, false); return true; }
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (!WCharging() && NativeForm()) (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (TargetDamageable(target) && Lethal(0, target))
            (void)CastQ(target, Mode::Automatic, true);
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (ControllerHelpers::AnyTextContains(
        { args.SpellName, args.ScriptName }, { "ViegoPassiveAttack", "ViegoPassive" })) {
        PossessionCasting = Untargetable = true;
        UntargetableUntil = now + kPossessionInvulnerableMs;
        PossessionStartTick = now;
        PossessionExpireTick = now + kPossessionDurationMs;
        SoulNetworkId = SoulExpireTick = 0;
        SoulPosition = {};
        ClearForcedTarget();
        return;
    }
    const int slot = args.Slot;
    if (slot < 0 || slot >= 4) return;
    const bool owned = Engine::WasControllerCast(slot);
    if (!owned) PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
    if (slot == 0) QCastTick = now;
    else if (slot == 1) {
        WCastTick = now;
        if (!owned && WCharging()) { WChargeOwned = false; if (WChargeStartTick == 0) WChargeStartTick = now; }
    } else if (slot == 2) {
        ECastTick = now;
        MistCastPosition = args.EndPosition.IsValid() ? args.EndPosition : MistCastPosition;
    } else {
        RCastTick = now;
        if (Possessed && RuntimeNameContains(3, "ViegoR")) {
            Possessed = false;
            PossessionStartTick = PossessionExpireTick = 0;
        }
    }
}

inline void UpdateBuff(const SDK::Events::BuffEventArgs& args, bool added) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "ViegoPassiveCasting")) {
            PossessionCasting = Untargetable = added;
            if (added) UntargetableUntil = now + ControllerHelpers::RemainingMilliseconds(
                args.EndTime, kPossessionInvulnerableMs, 200, 1800);
        } else if (Engine::TextContains(args.BuffName, "ViegoPassiveTransform")) {
            Possessed = added;
            if (added) {
                PossessionCasting = false;
                PossessionStartTick = now;
                PossessionExpireTick = now + ControllerHelpers::RemainingMilliseconds(
                    args.EndTime, kPossessionDurationMs, 500, 12000);
            } else PossessionStartTick = PossessionExpireTick = 0;
        } else if (Engine::TextContains(args.BuffName, "ViegoEMist") ||
                   Engine::TextContains(args.BuffName, "ViegoEHaste")) InMist = added;
        else if (Engine::TextContains(args.BuffName, "ViegoW")) {
            if (added && WChargeStartTick == 0) WChargeStartTick = now;
            if (!added && !WCharging()) ClearWCharge();
        }
        return;
    }
    if (Engine::TextContains(args.BuffName, "ViegoQMark")) {
        if (added) {
            QMarkTargetId = static_cast<int>(args.Sender.NetworkId);
            QMarkExpireTick = now + ControllerHelpers::RemainingMilliseconds(
                args.EndTime, kQMarkDurationMs, 150, 5000);
        } else if (QMarkTargetId == static_cast<int>(args.Sender.NetworkId)) {
            QMarkTargetId = QMarkExpireTick = 0;
            ClearForcedTarget();
        }
    }
}

inline bool SoulObject(const SDK::Events::ObjectEventArgs& args) {
    return args.Sender.IsValid() && ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName, args.SpellName, args.MissileName },
        { "ViegoPassiveSoul", "ViegoSoul", "PossessionSoul" });
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!SoulObject(args)) return;
    SoulNetworkId = static_cast<int>(args.Sender.NetworkId);
    SoulPosition = args.Sender.Position;
    SoulExpireTick = Now() + kSoulDurationMs;
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || static_cast<int>(args.Sender.NetworkId) != SoulNetworkId) return;
    SoulNetworkId = SoulExpireTick = 0;
    SoulPosition = {};
    ClearForcedTarget();
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || PossessionCasting || Untargetable) return;
    const AIHeroClient target = args.Target.IsHero() ? AIHeroClient(args.Target.Handle()) : AIHeroClient{};
    if (Engine::ValidEnemy(target) && (target.HasBuff("JaxCounterStrike") ||
        target.HasBuff("FioraW") || target.IsInvulnerable())) args.Process = false;
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) return;
    const auto target = HeroByNetworkId(LastAutoTargetId);
    if (Engine::ValidEnemy(target) && target.HasBuff("ViegoQMark")) {
        QMarkTargetId = LastAutoTargetId;
        QMarkExpireTick = std::max(QMarkExpireTick, Now() + 180);
    }
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRoutes", false)) return;
    Drawing::DrawCircle(player.Position(), WCharging() ? WRange(WChargeSeconds()) : kQRange,
                        WCharging() ? 0xFF4B7BFFu : 0xFF42D7A0u, 1.7f, 42);
    if (!SoulPosition.IsZero() && SoulExpireTick > Now())
        Drawing::DrawCircle(SoulPosition, 70.0f, 0xFF7AEBC5u, 2.0f, 28);
    if (!MistCastPosition.IsZero())
        Drawing::DrawLine(player.Position(), MistCastPosition, 0xFF48D99Bu, 1.6f);
    if (!LastRLanding.IsZero())
        Drawing::DrawCircle(LastRLanding, kRRadius, 0xFF547DFFu, 1.8f, 40);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("ViegoOneTrick", "Viego possession mechanics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    PassiveMenu = TacticsMenu->AddSubMenu(new Menu("ViegoPassive", "Soul and possession"));
    PassiveMenu->Add(new MenuSlider("TakeSoulHP", "Take safe soul below HP", 72, 10, 100));
    PassiveMenu->Add(new MenuSlider("MaxSoulEnemies", "Maximum enemies at soul", 2, 1, 5));
    PassiveMenu->Add(new MenuSlider("ExitBeforeMs", "R-exit before possession ends", 900, 200, 2200));
    PassiveMenu->Add(new MenuSlider("DangerHP", "R-exit possession below HP", 28, 5, 70));
    MawMenu = TacticsMenu->AddSubMenu(new Menu("SpectralMaw", "Charged W dash"));
    MawMenu->Add(new MenuSlider("MaxDashEnemies", "Maximum enemies at W endpoint", 2, 1, 5));
    MawMenu->Add(new MenuBool("HarassW", "Use W in harass", false));
    MistMenu = TacticsMenu->AddSubMenu(new Menu("HarrowedPath", "Wall mist routing"));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu("Heartbreaker", "Execute and recast"));
    UltimateMenu->Add(new MenuSlider("RExecuteHP", "R execute HP threshold", 32, 5, 75));
    UltimateMenu->Add(new MenuSlider("MaxREnemies", "Maximum enemies at R landing", 2, 1, 5));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("ViegoCoach", "Route visualization"));
    CoachMenu->Add(new MenuBool("DrawRoutes", "Draw W, soul, mist and R routes", false));
}

inline void OnLoad() {
    Possessed = PossessionCasting = Untargetable = InMist = WChargeOwned = false;
    ForcedOrbwalkerTarget = false;
    PossessionStartTick = PossessionExpireTick = UntargetableUntil = 0;
    WChargeStartTick = WTargetId = QMarkTargetId = QMarkExpireTick = 0;
    SoulNetworkId = SoulExpireTick = LastAutoTargetId = LastAutoTick = 0;
    QCastTick = WCastTick = ECastTick = RCastTick = PlayerOverrideUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    GapcloserEndpoint = SoulPosition = MistCastPosition = LastRLanding = {};
    ReconcileState();
}
inline void OnUnload() {
    ClearForcedTarget();
    TacticsMenu = PassiveMenu = MawMenu = MistMenu = UltimateMenu = CoachMenu = nullptr;
    ClearWCharge();
    Possessed = PossessionCasting = Untargetable = InMist = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin Riot 26.15 and CommunityDragon 16.15 Summoner's Rift values",
    "Treat foreign Q/W/E spell names as possession rather than native variants",
    "Reconcile possession from transform buffs and runtime spell names",
    "Block every controller action during the one-second soul attack untargetability",
    "Yield all foreign possessed Q/W/E spells to the player",
    "Allow only Viego Heartbreaker while possession owns the spell bar",
    "Exit possession with R when its ten-second window is expiring or dangerous",
    "Track eight-second soul objects through create and delete events",
    "Take only attackable in-range souls whose position passes safety policy",
    "Track Q marks through buff events and expiry polling",
    "Cooperate with the orbwalker by forcing the marked double-strike target",
    "Do not cancel a safe marked auto with another native spell",
    "Use Q 600 range and 125 total rectangle width",
    "Start W only when its 300-unit dash path and endpoint are safe",
    "Interpolate W projectile reach from 500 to 900 over one second",
    "Require high prediction and an empty first-hit collision list before W release",
    "Release an owned W before expiry but never release a manual W",
    "Use W reactively against gapclosers and interruptible channels",
    "Cast E only toward terrain and avoid refreshing active mist",
    "Clamp R landing to 500 and model its 300-radius victim set",
    "Require the intended target to be Heartbreaker's lowest-health primary victim",
    "Allow R for execute, defensive displacement, multi-target value or possession exit",
    "Respect selected, orbwalker and marked target cooperation",
    "Cover Combo, Harass, farm, Flee and conservative Automatic modes",
    "Track runtime cooldowns, resource-free casting and manual ownership",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Viego;
    controller.ControllerId = "champion.kuroaio.ai.viego.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIViego.md";
    controller.ImplementationSummary =
        "Possession/untargetable ownership boundary, safe soul acquisition, Q-mark "
        "orbwalker cooperation, collision-aware charged W dash, terrain mist routing "
        "and primary-victim-aware Heartbreaker execute/recast control.";
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
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuff, true>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuff, false>;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 760, 900>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1400, 250, 5000>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Viego
