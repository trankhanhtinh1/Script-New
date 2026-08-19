#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AILeblancGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>

namespace Plugins::KuroAIO::AI::Controllers::Leblanc {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::ObjectEventIsAllied;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using MarksmanControllerHelpers::RedirectBeforeAttackToFocus;

inline Menu* TacticsMenu = nullptr;
inline Menu* DistortionMenu = nullptr;
inline Menu* ChainMenu = nullptr;
inline Menu* MimicMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline MimicKind AvailableMimic = MimicKind::None;
inline MarkState SigilMark = {};
inline int BasicTetherTargetId = 0;
inline int BasicTetherExpireTick = 0;
inline int MimicTetherTargetId = 0;
inline int MimicTetherExpireTick = 0;
inline bool WReturnActive = false;
inline bool RReturnActive = false;
inline Vector3 WReturnOrigin = {};
inline Vector3 RReturnOrigin = {};
inline int WReturnExpireTick = 0;
inline int RReturnExpireTick = 0;
inline CloneState PassiveClone = {};
inline Vector3 PassiveClonePosition = {};
inline int PassiveCooldownUntil = 0;
inline int PassiveTriggerUntil = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PlayerOverrideUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline bool QWasManual = false;
inline bool WWasManual = false;
inline bool EWasManual = false;
inline bool RWasManual = false;

using ControllerHelpers::Now;

using ControllerHelpers::Ready;

inline bool Throttle(int index, int delay) {
    const int tick = index == 0 ? QCastTick : index == 1 ? WCastTick :
        index == 2 ? ECastTick : RCastTick;
    return Now() - tick >= delay;
}

inline bool CanAfford(int index, float reserve = 0.0f) {
    return CurrentResource() + 0.5f >= SpellCost(index) + std::max(0.0f, reserve);
}

inline bool TargetCannotBeDamaged(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BlackShield") ||
           target.HasBuff("BansheesVeil") || target.HasBuff("EdgeOfNight") ||
           target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("KayleR") || target.HasBuff("kindredrnodeathbuff");
}

inline int SpellRank(int index) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || index < 0 || index >= 4) return 0;
    const auto spell = player.Spellbook().GetSpell(static_cast<SDK::SpellSlot>(index));
    return spell.IsValid() ? spell.Level() : 0;
}

inline float MarkDetonationRaw(const AIHeroClient& target) {
    if (!MarkActive(SigilMark, static_cast<int>(target.NetworkId()), Now())) return 0.0f;
    const auto player = GameObjects::Player();
    return SigilMark.Mimic
        ? RQMarkRawDamage(SpellRank(3), player.AP())
        : QMarkRawDamage(SpellRank(0), player.AP());
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(
        target, QInitialRawDamage(SpellRank(0), player.AP()) + MarkDetonationRaw(target));
}

inline float WDamage(const AIHeroClient& target, bool mimic = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = mimic ? RWRawDamage(SpellRank(3), player.AP())
                            : WRawDamage(SpellRank(1), player.AP());
    return player.CalculateMagicDamage(target, raw + MarkDetonationRaw(target));
}

inline float EDamage(const AIHeroClient& target, bool mimic = false,
                     bool includeDelayed = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    float raw = mimic ? REInitialRawDamage(SpellRank(3), player.AP())
                      : EInitialRawDamage(SpellRank(2), player.AP());
    if (includeDelayed) {
        raw += mimic ? REDelayedRawDamage(SpellRank(3), player.AP())
                     : EDelayedRawDamage(SpellRank(2), player.AP());
    }
    return player.CalculateMagicDamage(target, raw + MarkDetonationRaw(target));
}

using ControllerHelpers::Lethal;

inline bool IsReturnName(const std::string& name) {
    return Engine::TextContains(name.c_str(), "return") || Engine::TextContains(name.c_str(), "returnm");
}

inline MimicKind MimicFromName(const std::string& name, MimicKind fallback) {
    if (Engine::TextContains(name.c_str(), "rw") || Engine::TextContains(name.c_str(), "slidem")) return MimicKind::W;
    if (Engine::TextContains(name.c_str(), "re") || Engine::TextContains(name.c_str(), "soulshacklem")) return MimicKind::E;
    if (Engine::TextContains(name.c_str(), "rq") || Engine::TextContains(name.c_str(), "chaosorbm")) return MimicKind::Q;
    return fallback;
}

inline std::string RuntimeSpellName(SDK::SpellSlot slot) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    const auto spell = player.Spellbook().GetSpell(slot);
    if (!spell.IsValid()) return {};
    const std::string script = spell.ScriptName();
    return !script.empty() ? script : spell.Name();
}

inline bool ReturnOriginSafe(const Vector3& origin) {
    return origin.IsValid() && !origin.IsZero() && !SDK::NavMesh::IsWall(origin) &&
           !Engine::UnderEnemyTurret(origin) &&
           Engine::CountEnemiesAt(origin, 475.0f) <= Slider(DistortionMenu, "MaxReturnEnemies", 1);
}

inline bool CurrentPositionSafe() {
    const auto player = GameObjects::Player();
    return player.IsValid() && !Engine::UnderEnemyTurret(player.Position()) &&
           Engine::CountEnemiesAt(player.Position(), 475.0f) <= Slider(DistortionMenu, "MaxDashEnemies", 2);
}

inline bool HasActiveTetherTo(int targetId) {
    const int now = Now();
    return targetId != 0 &&
        ((BasicTetherTargetId == targetId && BasicTetherExpireTick > now) ||
         (MimicTetherTargetId == targetId && MimicTetherExpireTick > now));
}

inline AIHeroClient TetherTarget() {
    const int now = Now();
    if (MimicTetherExpireTick > now) {
        const auto target = HeroByNetworkId(MimicTetherTargetId);
        if (Engine::ValidEnemy(target)) return target;
    }
    if (BasicTetherExpireTick > now) {
        const auto target = HeroByNetworkId(BasicTetherTargetId);
        if (Engine::ValidEnemy(target)) return target;
    }
    return {};
}

inline void ClearExpiredState() {
    const int now = Now();
    if (SigilMark.ExpiresAt <= now) SigilMark = {};
    if (BasicTetherExpireTick <= now) BasicTetherTargetId = BasicTetherExpireTick = 0;
    if (MimicTetherExpireTick <= now) MimicTetherTargetId = MimicTetherExpireTick = 0;
    if (WReturnExpireTick <= now) {
        WReturnActive = false;
        WReturnExpireTick = 0;
        WReturnOrigin = {};
    }
    if (RReturnExpireTick <= now) {
        RReturnActive = false;
        RReturnExpireTick = 0;
        RReturnOrigin = {};
    }
    if (!CloneActive(PassiveClone, now)) {
        PassiveClone = {};
        PassiveClonePosition = {};
    }
}

inline void PollEnemyEffects() {
    const int now = Now();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const int id = static_cast<int>(enemy.NetworkId());
        if (enemy.HasBuff("LeblancRQMark") || enemy.HasBuff("LeblancRQ")) {
            SigilMark = { id, now + static_cast<int>(kQMarkDurationSeconds * 1000.0f), true };
        } else if (enemy.HasBuff("LeblancQMark") || enemy.HasBuff("LeblancQ")) {
            SigilMark = { id, now + static_cast<int>(kQMarkDurationSeconds * 1000.0f), false };
        }
        if (enemy.HasBuff("LeblancRE") || enemy.HasBuff("LeblancRERoot")) {
            MimicTetherTargetId = id;
            MimicTetherExpireTick = std::max(MimicTetherExpireTick,
                now + static_cast<int>(kETetherSeconds * 1000.0f));
        } else if (enemy.HasBuff("LeblancE") || enemy.HasBuff("LeblancERoot")) {
            BasicTetherTargetId = id;
            BasicTetherExpireTick = std::max(BasicTetherExpireTick,
                now + static_cast<int>(kETetherSeconds * 1000.0f));
        }
    }
}

inline void PollClone() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || CloneActive(PassiveClone, Now())) return;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!ally.IsValid() || ally.NetworkId() == player.NetworkId()) continue;
        if (!ControllerHelpers::ChampionIs(ally, SDK::ChampionId::Leblanc)) continue;
        PassiveClone = { static_cast<int>(ally.NetworkId()), Now(),
            Now() + static_cast<int>(kPassiveCloneSeconds * 1000.0f), true };
        PassiveClonePosition = ally.Position();
        break;
    }
}

inline void ReconcileState() {
    ClearExpiredState();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const std::string wName = RuntimeSpellName(SDK::SpellSlot::W);
    if (IsReturnName(wName)) {
        WReturnActive = true;
        WReturnExpireTick = std::max(WReturnExpireTick, Now() + 300);
    }
    const std::string rName = RuntimeSpellName(SDK::SpellSlot::R);
    AvailableMimic = MimicFromName(rName, AvailableMimic);
    if (AvailableMimic == MimicKind::W && IsReturnName(rName)) {
        RReturnActive = true;
        RReturnExpireTick = std::max(RReturnExpireTick, Now() + 300);
    }
    if (player.HasBuff("LeblancP") || player.HasBuff("LeblancPMark")) {
        PassiveTriggerUntil = std::max(PassiveTriggerUntil, Now() + 500);
    }
    PollEnemyEffects();
    PollClone();
}

inline DashContext BuildDashContext(const Vector3& endpoint, const Vector3& origin,
                                    bool lethal, bool fleeing) {
    DashContext context{};
    context.EndpointValid = endpoint.IsValid() && !endpoint.IsZero();
    context.EndpointWall = !context.EndpointValid || SDK::NavMesh::IsWall(endpoint);
    context.EndpointTurret = context.EndpointValid && Engine::UnderEnemyTurret(endpoint);
    context.OriginTurret = Engine::UnderEnemyTurret(origin);
    context.ReturnAvailable = true;
    context.OriginSafe = ReturnOriginSafe(origin);
    context.Lethal = lethal;
    context.Fleeing = fleeing;
    context.EndpointEnemies = context.EndpointValid ? Engine::CountEnemiesAt(endpoint, 475.0f) : 99;
    context.MaximumEnemies = Slider(DistortionMenu, "MaxDashEnemies", 2);
    return context;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool force = false) {
    if (!Engine::ValidEnemy(target, kQRange + 35.0f) || TargetCannotBeDamaged(target) ||
        !Ready(0, mode) || !Throttle(0, 55) || !CanAfford(0)) return false;
    if (Orbwalker::IsWindingUp() && !force && !Lethal(target, QDamage(target))) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    QCastTick = Now();
    QWasManual = false;
    AvailableMimic = MimicKind::Q;
    SigilMark = { static_cast<int>(target.NetworkId()),
        Now() + static_cast<int>(kQMarkDurationSeconds * 1000.0f), false };
    return true;
}

inline bool CastWReturn(const AIHeroClient& target, Mode mode, bool force = false) {
    if (!WReturnActive || !Ready(1, mode) || !Throttle(1, 80)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool tetherNeedsCurrent = Engine::ValidEnemy(target) &&
        TetherIntact(player.Position(), target.Position(), target.BoundingRadius()) &&
        !TetherIntact(WReturnOrigin, target.Position(), target.BoundingRadius());
    ReturnContext context{ true, WReturnOrigin.IsValid() && !WReturnOrigin.IsZero(),
        ReturnOriginSafe(WReturnOrigin), CurrentPositionSafe(),
        Engine::ValidEnemy(target, kQRange + 80.0f),
        player.HealthPercent() <= Slider(DistortionMenu, "ReturnHP", 32),
        IncomingHardCCUntil > Now(), tetherNeedsCurrent,
        Engine::ValidEnemy(target) && Lethal(target, QDamage(target) + EDamage(target)) };
    if (!force && !ShouldReturn(context)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WCastTick = Now();
    WWasManual = false;
    WReturnActive = false;
    WReturnExpireTick = 0;
    WReturnOrigin = {};
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool fleeing = false,
                  bool force = false) {
    if (WReturnActive) return CastWReturn(target, mode, force || fleeing);
    if (!Ready(1, mode) || !Throttle(1, 90) || !CanAfford(1)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    Vector3 requested = fleeing ? Game::CursorPos() : PredictPosition(target, 0.25f);
    if (!requested.IsValid() || requested.IsZero()) return false;
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), requested);
    const bool targetHit = fleeing || (Engine::ValidEnemy(target) &&
        endpoint.Distance2D(PredictPosition(target, 0.25f)) <= kWRadius + target.BoundingRadius());
    if (!targetHit) return false;
    const bool lethal = Engine::ValidEnemy(target) && Lethal(target, WDamage(target));
    if (!MayStartDash(BuildDashContext(endpoint, player.Position(), lethal, fleeing))) return false;
    const Vector3 origin = player.Position();
    if (!Engine::ControllerCastPosition(1, endpoint)) return false;
    WCastTick = Now();
    WWasManual = false;
    WReturnActive = true;
    WReturnOrigin = origin;
    WReturnExpireTick = Now() + static_cast<int>(kWReturnSeconds * 1000.0f);
    AvailableMimic = MimicKind::W;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool mimic = false,
                  bool force = false) {
    const int index = mimic ? 3 : 2;
    if (!Engine::ValidEnemy(target, kERange + 35.0f) || TargetCannotBeDamaged(target) ||
        !Ready(index, mode) || !Throttle(index, 70) || !CanAfford(index)) return false;
    const auto prediction = Engine::RuntimeSpells[index]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, 0.25f);
    if (!aim.IsValid() || aim.IsZero() ||
        prediction.Hitchance < (force ? SDK::HitChance::Medium : SDK::HitChance::High) ||
        !prediction.CollisionObjects.empty() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 55.0f)) return false;
    if (Orbwalker::IsWindingUp() && !force && !Lethal(target, EDamage(target, mimic))) return false;
    if (!Engine::ControllerCastPosition(index, aim)) return false;
    if (mimic) {
        RCastTick = Now();
        RWasManual = false;
    } else {
        ECastTick = Now();
        EWasManual = false;
        AvailableMimic = MimicKind::E;
    }
    return true;
}

inline bool CastRQ(const AIHeroClient& target, Mode mode, bool force = false) {
    if (AvailableMimic != MimicKind::Q || !Engine::ValidEnemy(target, kQRange + 35.0f) ||
        TargetCannotBeDamaged(target) || !Ready(3, mode) || !Throttle(3, 80)) return false;
    const auto player = GameObjects::Player();
    const float damage = player.CalculateMagicDamage(target,
        RQInitialRawDamage(SpellRank(3), player.AP()) + MarkDetonationRaw(target));
    if (Orbwalker::IsWindingUp() && !force && !Lethal(target, damage)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    RCastTick = Now();
    RWasManual = false;
    SigilMark = { static_cast<int>(target.NetworkId()),
        Now() + static_cast<int>(kQMarkDurationSeconds * 1000.0f), true };
    return true;
}

inline bool CastRWReturn(const AIHeroClient& target, Mode mode, bool force = false) {
    if (!RReturnActive || !Ready(3, mode) || !Throttle(3, 80)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool tetherNeedsCurrent = Engine::ValidEnemy(target) &&
        TetherIntact(player.Position(), target.Position(), target.BoundingRadius()) &&
        !TetherIntact(RReturnOrigin, target.Position(), target.BoundingRadius());
    ReturnContext context{ true, RReturnOrigin.IsValid() && !RReturnOrigin.IsZero(),
        ReturnOriginSafe(RReturnOrigin), CurrentPositionSafe(),
        Engine::ValidEnemy(target, kQRange + 80.0f),
        player.HealthPercent() <= Slider(DistortionMenu, "ReturnHP", 32),
        IncomingHardCCUntil > Now(), tetherNeedsCurrent,
        Engine::ValidEnemy(target) && Lethal(target, QDamage(target) + EDamage(target)) };
    if (!force && !ShouldReturn(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RCastTick = Now();
    RWasManual = false;
    RReturnActive = false;
    RReturnExpireTick = 0;
    RReturnOrigin = {};
    return true;
}

inline bool CastRW(const AIHeroClient& target, Mode mode, bool fleeing = false,
                   bool force = false) {
    if (RReturnActive) return CastRWReturn(target, mode, force || fleeing);
    if (AvailableMimic != MimicKind::W || !Ready(3, mode) || !Throttle(3, 100)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    Vector3 requested = fleeing ? Game::CursorPos() : PredictPosition(target, 0.25f);
    if (!requested.IsValid() || requested.IsZero()) return false;
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), requested);
    const bool targetHit = fleeing || (Engine::ValidEnemy(target) &&
        endpoint.Distance2D(PredictPosition(target, 0.25f)) <= kWRadius + target.BoundingRadius());
    if (!targetHit) return false;
    const bool lethal = Engine::ValidEnemy(target) && Lethal(target, WDamage(target, true));
    if (!MayStartDash(BuildDashContext(endpoint, player.Position(), lethal, fleeing))) return false;
    const Vector3 origin = player.Position();
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    RCastTick = Now();
    RWasManual = false;
    RReturnActive = true;
    RReturnOrigin = origin;
    RReturnExpireTick = Now() + static_cast<int>(kWReturnSeconds * 1000.0f);
    return true;
}

inline bool CastMimic(const AIHeroClient& target, Mode mode, bool force = false,
                      bool fleeing = false) {
    if (!Ready(3, mode)) return false;
    switch (AvailableMimic) {
    case MimicKind::Q: return CastRQ(target, mode, force);
    case MimicKind::W: return CastRW(target, mode, fleeing, force);
    case MimicKind::E: return CastE(target, mode, true, force);
    default: return false;
    }
}

inline bool TryReturns(const AIHeroClient& target, Mode mode, bool force = false) {
    if (WReturnActive && CastWReturn(target, mode, force)) return true;
    return RReturnActive && CastRWReturn(target, mode, force);
}

inline bool TryReactive() {
    if (InterruptExpireTick > Now()) {
        const auto target = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(target) && CastE(target, Mode::Automatic, false, true)) return true;
        if (Engine::ValidEnemy(target) && AvailableMimic == MimicKind::E &&
            CastE(target, Mode::Automatic, true, true)) return true;
    }
    if (GapcloserExpireTick > Now()) {
        const auto target = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(target) && CastE(target, Mode::Automatic, false, true)) return true;
    }
    return false;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, QDamage(target)) && CastQ(target, mode, true)) return true;
    if (Lethal(target, EDamage(target, false, true)) && CastE(target, mode, false, true)) return true;
    if (AvailableMimic == MimicKind::Q && CastRQ(target, mode, true)) return true;
    if (AvailableMimic == MimicKind::W && Lethal(target, WDamage(target, true)) &&
        CastRW(target, mode, false, true)) return true;
    if (AvailableMimic == MimicKind::E && Lethal(target, EDamage(target, true, true)) &&
        CastE(target, mode, true, true)) return true;
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return TryReturns(target, Mode::Combo);
    const int targetId = static_cast<int>(target.NetworkId());
    if (TryReturns(target, Mode::Combo)) return true;
    const bool marked = MarkActive(SigilMark, targetId, Now());
    if (!marked && CastQ(target, Mode::Combo)) return true;
    if (marked && GameObjects::Player().Position().Distance2D(target.Position()) <=
            kWRange + kWRadius && CastW(target, Mode::Combo)) return true;
    if (!HasActiveTetherTo(targetId) && CastE(target, Mode::Combo)) return true;
    if (AvailableMimic == MimicKind::Q && marked && CastRQ(target, Mode::Combo)) return true;
    if (AvailableMimic == MimicKind::E && HasActiveTetherTo(targetId) && CastMimic(target, Mode::Combo)) return true;
    if (AvailableMimic == MimicKind::W && marked && CastMimic(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    return CastMimic(target, Mode::Combo);
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return TryReturns(target, Mode::Harass);
    if (CurrentResource() < Slider(TacticsMenu, "HarassMana", 40)) return false;
    if (TryReturns(target, Mode::Harass)) return true;
    const bool marked = MarkActive(SigilMark, static_cast<int>(target.NetworkId()), Now());
    if (!marked && CastQ(target, Mode::Harass)) return true;
    if (marked && Bool(DistortionMenu, "HarassW", true) && CastW(target, Mode::Harass)) return true;
    return CastE(target, Mode::Harass);
}

inline bool TryFarm(Mode mode) {
    if (WReturnActive || RReturnActive) return false;
    if (CurrentResource() < Slider(FarmMenu, "ManaReserve", 100)) return false;
    return Engine::TryFarm(mode);
}

inline bool TryFlee(const AIHeroClient& threat) {
    if (TryReturns(threat, Mode::Flee, true)) return true;
    if (CastW(threat, Mode::Flee, true, true)) return true;
    if (AvailableMimic == MimicKind::W && CastRW(threat, Mode::Flee, true, true)) return true;
    return Engine::ValidEnemy(threat) && CastE(threat, Mode::Flee, false, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    AIHeroClient target = selected;
    const auto tether = TetherTarget();
    if (Engine::ValidEnemy(tether, kERange + 160.0f)) target = tether;
    if (!Engine::ValidEnemy(target)) target = Engine::SelectTarget(kERange + 120.0f);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 1050.0f);
    if (mode == Mode::Flee) {
        (void)TryFlee(threat);
        return true;
    }
    if (PlayerOverrideUntil > Now()) return true;
    if (TryReturns(target, mode)) return true;
    if (TryReactive()) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && AutomaticAllowed({ false, true,
            HasActiveTetherTo(static_cast<int>(target.NetworkId())), false })) {
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
        if (threat.Valid && threat.CrossesPlayer && threat.LikelyHardCrowdControl) IncomingHardCCUntil = now + 700;
        return;
    }
    const int slot = args.Slot;
    const bool owned = slot >= 0 && slot < 4 && Engine::WasControllerCast(slot);
    if (!owned) PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
    const std::string name = args.SpellName;
    const Vector3 origin = args.StartPosition.IsValid() && !args.StartPosition.IsZero()
        ? args.StartPosition : player.Position();
    const int targetId = static_cast<int>(args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId);
    if (slot == 0) {
        QCastTick = now;
        QWasManual = !owned;
        AvailableMimic = MimicKind::Q;
        if (targetId != 0) SigilMark = { targetId,
            now + static_cast<int>(kQMarkDurationSeconds * 1000.0f), false };
    } else if (slot == 1) {
        WCastTick = now;
        WWasManual = !owned;
        if (IsReturnName(name)) {
            WReturnActive = false;
            WReturnOrigin = {};
            WReturnExpireTick = 0;
        } else {
            WReturnActive = true;
            WReturnOrigin = origin;
            WReturnExpireTick = now + static_cast<int>(kWReturnSeconds * 1000.0f);
            AvailableMimic = MimicKind::W;
        }
    } else if (slot == 2) {
        ECastTick = now;
        EWasManual = !owned;
        AvailableMimic = MimicKind::E;
    } else if (slot == 3) {
        RCastTick = now;
        RWasManual = !owned;
        const MimicKind observed = MimicFromName(name, AvailableMimic);
        if (observed == MimicKind::W) {
            if (IsReturnName(name)) {
                RReturnActive = false;
                RReturnOrigin = {};
                RReturnExpireTick = 0;
            } else {
                RReturnActive = true;
                RReturnOrigin = origin;
                RReturnExpireTick = now + static_cast<int>(kWReturnSeconds * 1000.0f);
            }
        } else if (observed == MimicKind::Q && targetId != 0) {
            SigilMark = { targetId, now + static_cast<int>(kQMarkDurationSeconds * 1000.0f), true };
        }
    }
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid()) return;
    const int senderId = static_cast<int>(args.Sender.NetworkId);
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "LeblancP")) {
            if (added) {
                PassiveTriggerUntil = now + 1200;
                PassiveCooldownUntil = now + static_cast<int>(kPassiveCooldownSeconds * 1000.0f);
            } else PassiveTriggerUntil = 0;
        }
        return;
    }
    if (Engine::TextContains(args.BuffName, "LeblancRQMark")) {
        if (added) SigilMark = { senderId,
            now + ControllerHelpers::RemainingMilliseconds(args.EndTime,
                static_cast<int>(kQMarkDurationSeconds * 1000.0f), 200, 5000), true };
        else if (SigilMark.TargetId == senderId && SigilMark.Mimic) SigilMark = {};
    } else if (Engine::TextContains(args.BuffName, "LeblancQMark")) {
        if (added) SigilMark = { senderId,
            now + ControllerHelpers::RemainingMilliseconds(args.EndTime,
                static_cast<int>(kQMarkDurationSeconds * 1000.0f), 200, 5000), false };
        else if (SigilMark.TargetId == senderId && !SigilMark.Mimic) SigilMark = {};
    }
    if (Engine::TextContains(args.BuffName, "LeblancRE") && !Engine::TextContains(args.BuffName, "Mark")) {
        if (added) {
            MimicTetherTargetId = senderId;
            MimicTetherExpireTick = now + ControllerHelpers::RemainingMilliseconds(args.EndTime,
                static_cast<int>(kETetherSeconds * 1000.0f), 200, 2500);
        } else if (MimicTetherTargetId == senderId) MimicTetherTargetId = MimicTetherExpireTick = 0;
    } else if (Engine::TextContains(args.BuffName, "LeblancE") && !Engine::TextContains(args.BuffName, "Mark")) {
        if (added) {
            BasicTetherTargetId = senderId;
            BasicTetherExpireTick = now + ControllerHelpers::RemainingMilliseconds(args.EndTime,
                static_cast<int>(kETetherSeconds * 1000.0f), 200, 2500);
        } else if (BasicTetherTargetId == senderId) BasicTetherTargetId = BasicTetherExpireTick = 0;
    }
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !ObjectEventIsAllied(args)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || static_cast<int>(args.Sender.NetworkId) == player.NetworkId()) return;
    if (!Engine::TextContains(args.Sender.CharacterName, "Leblanc")) return;
    PassiveClone = { static_cast<int>(args.Sender.NetworkId), Now(),
        Now() + static_cast<int>(kPassiveCloneSeconds * 1000.0f), true };
    PassiveClonePosition = args.Sender.Position;
    PassiveCooldownUntil = std::max(PassiveCooldownUntil,
        Now() + static_cast<int>(kPassiveCooldownSeconds * 1000.0f));
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && static_cast<int>(args.Sender.NetworkId) == PassiveClone.NetworkId) {
        PassiveClone = {};
        PassiveClonePosition = {};
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    const auto tether = TetherTarget();
    if (Engine::ValidEnemy(tether)) (void)RedirectBeforeAttackToFocus(args, tether);
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (ControllerHelpers::CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint,
            GapcloserExpireTick, 760.0f, 900)) {
        IncomingHardCCUntil = std::max(IncomingHardCCUntil, Now() + 650);
    }
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawState", false)) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFA938FFu, 1.5f, 40);
    if (WReturnActive && !WReturnOrigin.IsZero()) {
        Drawing::DrawCircle(WReturnOrigin, 65.0f, 0xFF7D49FFu, 2.0f, 28);
        Drawing::DrawLine(player.Position(), WReturnOrigin, 0xFF7D49FFu, 2.0f);
    }
    if (RReturnActive && !RReturnOrigin.IsZero()) {
        Drawing::DrawCircle(RReturnOrigin, 65.0f, 0xFFD64CFFu, 2.0f, 28);
        Drawing::DrawLine(player.Position(), RReturnOrigin, 0xFFD64CFFu, 2.0f);
    }
    const auto tether = TetherTarget();
    if (Engine::ValidEnemy(tether)) Drawing::DrawLine(player.Position(), tether.Position(), 0xFFB875FFu, 2.2f);
    if (CloneActive(PassiveClone, Now()) && !PassiveClonePosition.IsZero())
        Drawing::DrawCircle(PassiveClonePosition, 55.0f, 0xFF8A66D9u, 1.6f, 24);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("LeblancOneTrick", "LeBlanc mimic mechanics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana reserve", 40, 0, 200));
    DistortionMenu = TacticsMenu->AddSubMenu(new Menu("DistortionSafety", "Distortion return safety"));
    DistortionMenu->Add(new MenuSlider("MaxDashEnemies", "Maximum enemies at landing", 2, 1, 5));
    DistortionMenu->Add(new MenuSlider("MaxReturnEnemies", "Maximum enemies at return pad", 1, 0, 5));
    DistortionMenu->Add(new MenuSlider("ReturnHP", "Return below health percent", 32, 10, 70));
    DistortionMenu->Add(new MenuBool("HarassW", "Use safe W to detonate Sigil", true));
    ChainMenu = TacticsMenu->AddSubMenu(new Menu("ChainTether", "Ethereal Chains tether"));
    ChainMenu->Add(new MenuSeparator("PreserveTether", "Keep current position when return would break tether"));
    MimicMenu = TacticsMenu->AddSubMenu(new Menu("MimicState", "Q/W/E Mimic routing"));
    MimicMenu->Add(new MenuSeparator("ObservedMimic", "Mimic follows spell events and runtime polling"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("LeblancFarm", "Conservative farm"));
    FarmMenu->Add(new MenuSlider("ManaReserve", "Mana reserve", 100, 0, 300));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("LeblancCoach", "State visualization"));
    CoachMenu->Add(new MenuBool("DrawState", "Draw returns, chain and clone", false));
}

inline void OnLoad() {
    AvailableMimic = MimicKind::None;
    SigilMark = {};
    BasicTetherTargetId = BasicTetherExpireTick = MimicTetherTargetId = MimicTetherExpireTick = 0;
    WReturnActive = RReturnActive = false;
    WReturnOrigin = RReturnOrigin = {};
    WReturnExpireTick = RReturnExpireTick = 0;
    PassiveClone = {};
    PassiveClonePosition = {};
    PassiveCooldownUntil = PassiveTriggerUntil = 0;
    QCastTick = WCastTick = ECastTick = RCastTick = 0;
    LastAutoTargetId = LastAutoTick = PlayerOverrideUntil = IncomingHardCCUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    GapcloserEndpoint = {};
    QWasManual = WWasManual = EWasManual = RWasManual = false;
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = DistortionMenu = ChainMenu = MimicMenu = FarmMenu = CoachMenu = nullptr;
    SigilMark = {};
    PassiveClone = {};
    WReturnActive = RReturnActive = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin Riot 26.15 and CommunityDragon 16.15 as the Summoner's Rift kit baseline",
    "Track ordinary and Mimic Sigil marks from events and target-buff polling",
    "Expire both mark forms after 3.5 seconds and preserve their distinct damage",
    "Track the last Q, W or E cast and reconcile the live Mimic runtime name",
    "Cast Mimic Q targeted, Mimic W with return safety and Mimic E with prediction",
    "Track ordinary and Mimic chain tethers independently",
    "Prefer the active tether target and keep position when return would break tether",
    "Use 925 range, 110 width, 1750 speed and prediction collision for Chains",
    "Track W and Mimic-W return origins and four-second windows independently",
    "Reject dash endpoints in walls, new turret range or excessive enemy density",
    "Return only to a currently safe exact endpoint",
    "Return on low health, crowd control, unsafe landing or lost target",
    "Preserve safe lethal continuation instead of snapping back prematurely",
    "Track Mirror Image activation, cooldown and allied clone create/delete/polling",
    "Expire clone ownership after eight seconds and never identify the player as clone",
    "Combo opens Q and detonates through a safe W/E/Mimic route",
    "Harass respects mana reserve and closes its safe Distortion trade",
    "Clear modes never spend R or strand a return pad",
    "Flee uses safe returns before creating a new W or Mimic-W route",
    "Automatic mode permits kill secure, tether preservation and defensive return only",
    "React to gapclosers and interruptible channels with collision-safe E",
    "Yield after manual Q/W/E/R while retaining observed state",
    "Never automate summoners, items or passive clone movement",
    "Keep metadata separate from the champion-owned decision loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Leblanc;
    controller.ControllerId = "champion.kuroaio.ai.leblanc.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AILeblanc.md";
    controller.ImplementationSummary =
        "Event-and-poll Sigil/Mimic state, dual chain tethers, independent Distortion "
        "returns, collision prediction and passive clone lifecycle tracking.";
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
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1800, 250, 5000>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Leblanc
