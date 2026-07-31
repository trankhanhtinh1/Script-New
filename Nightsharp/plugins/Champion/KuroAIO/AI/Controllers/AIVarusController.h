#pragma once

#include "../AIChampionEngine.h"
#include "../AIMarksmanControllerHelpers.h"
#include "AIVarusGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Varus {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::Now;
using ControllerHelpers::SpellEventNameContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* BlightMenu = nullptr;
inline Menu* ChargeMenu = nullptr;
inline Menu* ChainMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int QChargeStartTick = 0;
inline int QTargetId = 0;
inline int PendingQTargetId = 0;
inline int PendingQUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

inline int BlightStacks(const AIBaseClient& target) {
    return target.IsValid()
        ? std::clamp(target.GetBuffCount("varuswdebuff"), 0, 3)
        : 0;
}

inline bool QCharging() {
    const auto player = GameObjects::Player();
    return Engine::RuntimeSpells[0] &&
        (Engine::RuntimeSpells[0]->IsCharging() ||
         (player.IsValid() && player.HasBuff("VarusQLaunch")));
}

inline void RefreshChargeState() {
    if (QCharging()) {
        if (QChargeStartTick == 0) QChargeStartTick = Now();
        return;
    }
    if (QChargeStartTick > 0 && Now() - QChargeStartTick > 180) {
        QChargeStartTick = 0;
        QTargetId = 0;
    }
}

inline float ChargeElapsedSeconds() {
    return QChargeStartTick > 0
        ? std::max(0, Now() - QChargeStartTick) / 1000.0f
        : 0.0f;
}

inline float CurrentQRange() {
    return ChargedQRange(ChargeElapsedSeconds());
}

inline bool SafeAdditionalAuto(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && OrbwalkerAttackRoute(target) &&
        Orbwalker::CanAttack() && !IsEscaping(target) &&
        Engine::CountEnemiesAt(player.Position(), 650.0f) <= 1;
}

inline bool QPrediction(const AIHeroClient& target,
                        float range,
                        SDK::PredictionOutput* output = nullptr) {
    if (!Engine::RuntimeSpells[0] || !target.IsValid()) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(
        target, false, range);
    if (output) *output = prediction;
    return ControllerHelpers::PredictionAtLeast(
               prediction, SDK::HitChance::High) &&
           !PredictionProjectileWall(0, prediction, 70.0f);
}

inline bool EPrediction(const AIHeroClient& target,
                        SDK::PredictionOutput* output = nullptr) {
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(
        2, target, SDK::HitChance::High, false, &prediction) &&
        !PredictionProjectileWall(2, prediction, 250.0f);
    if (output) *output = prediction;
    return hit;
}

inline bool RPrediction(const AIHeroClient& target,
                        SDK::PredictionOutput* output = nullptr) {
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(
        3, target, SDK::HitChance::VeryHigh, false, &prediction) &&
        !PredictionProjectileWall(3, prediction, 120.0f);
    if (output) *output = prediction;
    return hit;
}

inline void RefreshOrbwalkerFocus(Mode mode,
                                  const AIHeroClient& preferred) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    auto owned = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 850.0f);
    const int ownedStacks = owned.IsValid() ? BlightStacks(owned) : 0;
    const bool pendingFirstStack = owned.IsValid() && ownedStacks == 0 &&
        LastAfterAttackTargetId == OwnedFocusTargetId &&
        Now() - LastAfterAttackTick <= 360;
    if (!combat || !owned.IsValid() || !InAutoAttackRange(owned) ||
        (ownedStacks == 0 && !pendingFirstStack)) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        owned = {};
    }
    if (owned.IsValid()) return;

    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const int stacks = BlightStacks(enemy);
        if (!Engine::ValidEnemy(enemy, 850.0f) ||
            !InAutoAttackRange(enemy) || stacks <= 0) continue;
        float score = static_cast<float>(stacks) * 120.0f +
                      (100.0f - enemy.HealthPercent());
        if (preferred.IsValid() &&
            preferred.NetworkId() == enemy.NetworkId()) score += 150.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    if (best.IsValid()) {
        (void)SetTemporaryOrbwalkerFocus(
            best, 850.0f, 1000,
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target,
    Mode mode,
    bool allowR) {
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool attack = OrbwalkerAttackRoute(target);
    const bool safeAuto = SafeAdditionalAuto(target);
    const int stacks = BlightStacks(target);
    const bool qPrediction = QPrediction(target,
        QCharging() ? CurrentQRange() : kMaximumQRange);
    bool q = false;
    if (CanUse(0, mode, QCharging()) && qPrediction) {
        if (QCharging()) {
            QReleaseContext release{};
            release.BlightStacks = stacks;
            release.Charging = true;
            release.PredictionHits = true;
            release.InCurrentRange = distance <=
                CurrentQRange() + target.BoundingRadius();
            release.SafeAdditionalAuto = safeAuto;
            release.TargetEscaping = IsEscaping(target);
            release.Lethal = SpellDamage(0, target) >=
                target.Health() + target.AllShield();
            release.ChargeExpiring = ChargeElapsedSeconds() >= 1.18f;
            q = ShouldReleaseQ(release);
        } else {
            QStartContext start{};
            start.BlightStacks = stacks;
            start.PredictionHits = true;
            start.InMaximumRange = distance <=
                kMaximumQRange + target.BoundingRadius();
            start.SafeAdditionalAuto = safeAuto;
            start.TargetEscaping = IsEscaping(target);
            start.OutsideAttackRange = !attack;
            start.Lethal = SpellDamage(0, target) >=
                target.Health() + target.AllShield();
            q = ShouldStartQ(start);
        }
    }
    const bool e = !QCharging() && CanUse(2, mode) &&
        distance <= 950.0f && EPrediction(target) &&
        (ShouldDetonateBlight(stacks, safeAuto, IsEscaping(target),
             SpellDamage(2, target) >= target.Health() + target.AllShield()) ||
         !Engine::RuntimeSpells[0] || !Engine::RuntimeSpells[0]->IsReady());
    const bool r = allowR && !QCharging() && Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->IsReady() && distance <= 1220.0f &&
        RPrediction(target);

    const std::array<bool, 4> reachable = {q, false, e, r};
    auto context = BaseTargetContext(
        target, EstimatedDamage(target, reachable, attack ? 2 : 0));
    context.AutoReachable = attack;
    context.DirectSpellReachable = q || e;
    context.SetupReachable = r;
    context.ExecuteReachable = q && context.Killable;
    context.ProjectileBlocked = !attack && !q && !e && !r;
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred,
                                      Mode mode,
                                      bool allowR = false) {
    const auto target = ControllerHelpers::SelectReachableEnemy(
        preferred, 1700.0f,
        [mode, allowR](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode, allowR);
        });
    LastSmartTarget = target;
    return target;
}

inline AIHeroClient SelectRTarget(const AIHeroClient& preferred) {
    return ControllerHelpers::SelectReachableEnemy(
        preferred, 1220.0f,
        [](const AIHeroClient& enemy) {
            const bool r = RPrediction(enemy);
            auto context = BaseTargetContext(
                enemy, r ? SpellDamage(3, enemy) : 0.0f);
            context.DirectSpellReachable = r;
            return context;
        });
}

inline bool CastWForQ(const AIHeroClient& target, Mode mode) {
    if (!CanUse(1, mode) || QCharging() ||
        !CastThrottlePassed(LastWCastTick, 30)) return false;
    const float qDamage = SpellDamage(0, target);
    const bool lowOrLethal = target.HealthPercent() <=
        static_cast<float>(Slider(BlightMenu, "EmpowerHp", 42)) ||
        qDamage >= target.Health() + target.AllShield();
    const bool alreadyEmpowered = LastWCastTick > 0 &&
        Now() - LastWCastTick < 4200;
    if (!ShouldEmpowerQ(true, alreadyEmpowered, true, lowOrLethal)) {
        return false;
    }
    if (Engine::ControllerCastSelf(1)) {
        LastWCastTick = Now();
        PendingQTargetId = static_cast<int>(target.NetworkId());
        PendingQUntil = Now() + 700;
        return true;
    }
    return false;
}

inline bool StartQ(const AIHeroClient& target, Mode mode) {
    if (QCharging() || !CanUse(0, mode) ||
        !Engine::ValidEnemy(target, 1700.0f) ||
        !CastThrottlePassed(LastQCastTick, 24)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = QPrediction(target, kMaximumQRange, &prediction);
    QStartContext context{};
    context.BlightStacks = BlightStacks(target);
    context.PredictionHits = hit;
    context.InMaximumRange = GameObjects::Player().Position().Distance2D(
        target.Position()) <= kMaximumQRange + target.BoundingRadius();
    context.SafeAdditionalAuto = SafeAdditionalAuto(target);
    context.TargetEscaping = IsEscaping(target);
    context.OutsideAttackRange = !OrbwalkerAttackRoute(target);
    context.Lethal = SpellDamage(0, target) >=
        target.Health() + target.AllShield();
    context.ProjectileWall =
        PredictionProjectileWall(0, prediction, 70.0f);
    if (!ShouldStartQ(context)) return false;

    if (Bool(BlightMenu, "UseWExecute", true) &&
        CastWForQ(target, mode)) return true;

    Engine::ArmControllerCast(0);
    if (Engine::RuntimeSpells[0]->StartCharging(
            prediction.GetCastPosition())) {
        Engine::MarkSuccessfulCast(0);
        LastQCastTick = Now();
        QChargeStartTick = Now();
        QTargetId = static_cast<int>(target.NetworkId());
        PendingQTargetId = 0;
        PendingQUntil = 0;
        return true;
    }
    Engine::CancelControllerCast(0);
    return false;
}

inline bool ReleaseQ(const AIHeroClient& fallback) {
    RefreshChargeState();
    if (!QCharging() || !Engine::RuntimeSpells[0] ||
        !CastThrottlePassed(LastQCastTick, 20)) return false;
    auto target = ControllerHelpers::HeroByNetworkId(QTargetId);
    if (!Engine::ValidEnemy(target, 1700.0f)) target = fallback;
    if (!Engine::ValidEnemy(target, 1700.0f)) return false;

    const float range = CurrentQRange();
    SDK::PredictionOutput prediction{};
    const bool hit = QPrediction(target, range, &prediction);
    QReleaseContext context{};
    context.BlightStacks = BlightStacks(target);
    context.Charging = true;
    context.PredictionHits = hit;
    context.InCurrentRange = GameObjects::Player().Position().Distance2D(
        target.Position()) <= range + target.BoundingRadius();
    context.SafeAdditionalAuto = SafeAdditionalAuto(target);
    context.TargetEscaping = IsEscaping(target);
    context.Lethal = SpellDamage(0, target) >=
        target.Health() + target.AllShield();
    context.ProjectileWall =
        PredictionProjectileWall(0, prediction, 70.0f);
    context.ChargeExpiring = ChargeElapsedSeconds() >= 1.18f;
    if (!ShouldReleaseQ(context)) return false;

    Engine::ArmControllerCast(0);
    if (Engine::RuntimeSpells[0]->ShootChargedSpell(
            prediction.GetCastPosition())) {
        Engine::MarkSuccessfulCast(0);
        LastQCastTick = Now();
        QChargeStartTick = 0;
        QTargetId = 0;
        if (OwnedFocusTargetId == static_cast<int>(target.NetworkId())) {
            ClearTemporaryOrbwalkerFocus(
                OwnedFocusTargetId, OwnedFocusUntil);
        }
        return true;
    }
    Engine::CancelControllerCast(0);
    return false;
}

inline bool ReleaseQForReactive(const AIHeroClient& threat) {
    if (!QCharging() || !Engine::RuntimeSpells[0] ||
        !Engine::ValidEnemy(threat, 1700.0f)) return false;
    const float overrideRange = std::max(
        CurrentQRange(),
        GameObjects::Player().Position().Distance2D(threat.Position()) +
            threat.BoundingRadius());
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(
        threat, false, std::min(kMaximumQRange, overrideRange));
    Vector3 cast = prediction.GetCastPosition();
    if (!cast.IsValid() || cast.IsZero()) {
        cast = ControllerHelpers::PredictPosition(threat, 0.20f);
    }
    if (!cast.IsValid() || cast.IsZero()) return false;
    Engine::ArmControllerCast(0);
    if (Engine::RuntimeSpells[0]->ShootChargedSpell(cast)) {
        Engine::MarkSuccessfulCast(0);
        LastQCastTick = Now();
        QChargeStartTick = 0;
        QTargetId = 0;
        ClearTemporaryOrbwalkerFocus(
            OwnedFocusTargetId, OwnedFocusUntil);
        return true;
    }
    Engine::CancelControllerCast(0);
    return false;
}

inline bool CastE(const AIHeroClient& target,
                  Mode mode,
                  bool defensive = false) {
    if (QCharging() || !CanUse(2, mode, defensive) ||
        !Engine::ValidEnemy(target, 950.0f) ||
        !CastThrottlePassed(LastECastTick, 28)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = EPrediction(target, &prediction);
    const int stacks = BlightStacks(target);
    const bool lethal = SpellDamage(2, target) >=
        target.Health() + target.AllShield();
    const bool fallback = !Engine::RuntimeSpells[0] ||
        !Engine::RuntimeSpells[0]->IsReady();
    if (!hit || (!defensive && !fallback &&
        !ShouldDetonateBlight(
            stacks, SafeAdditionalAuto(target),
            IsEscaping(target), lethal))) {
        return false;
    }
    if (Engine::ControllerCastPosition(2, prediction.GetCastPosition())) {
        LastECastTick = Now();
        if (OwnedFocusTargetId == static_cast<int>(target.NetworkId())) {
            ClearTemporaryOrbwalkerFocus(
                OwnedFocusTargetId, OwnedFocusUntil);
        }
        return true;
    }
    return false;
}

inline bool CastR(const AIHeroClient& target,
                  bool manual,
                  bool interrupt,
                  bool selfPeel) {
    if (QCharging() || !Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !Engine::ValidEnemy(target, 1220.0f) ||
        !CastThrottlePassed(LastRCastTick, 80)) return false;
    SDK::PredictionOutput prediction{};
    ChainContext context{};
    context.Manual = manual;
    context.PredictionVeryHigh = RPrediction(target, &prediction);
    context.InRange = true;
    context.ProjectileWall =
        PredictionProjectileWall(3, prediction, 120.0f);
    context.Interrupt = interrupt;
    context.SelfPeel = selfPeel;
    context.AlliedFollowup = CountAlliedFollowup(
        target.Position(), 750.0f, true) > 0;
    context.TargetLethal = EstimatedDamage(
        target, {true, false, true, true}, 2) >=
        target.Health() + target.AllShield();
    if (!ShouldCastChain(context)) return false;
    if (Engine::ControllerCastPosition(3, prediction.GetCastPosition())) {
        LastRCastTick = Now();
        return true;
    }
    return false;
}

inline bool TryManualR(const AIHeroClient& preferred) {
    if (!ManualUltimatePressed()) return false;
    const auto target = SelectRTarget(preferred);
    return Engine::ValidEnemy(target) && CastR(target, true, false, false);
}

inline bool TryInterrupt() {
    if (!Bool(Engine::AutomaticMenu, "Interrupt", true) ||
        InterruptExpireTick < Now()) return false;
    const auto target = ControllerHelpers::HeroByNetworkId(InterruptTargetId);
    return Engine::ValidEnemy(target, 1220.0f) &&
           CastR(target, false, true, false);
}

inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(target, 950.0f)) return false;
    if (CastR(target, false, false, true)) return true;
    return CastE(target, Mode::Automatic, true);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = SelectSmartTarget(preferred, Mode::Automatic, false);
    if (!Engine::ValidEnemy(target)) return false;
    if (SpellDamage(0, target) >= target.Health() + target.AllShield()) {
        if (QCharging()) return ReleaseQ(target);
        if (StartQ(target, Mode::Automatic)) return true;
    }
    return SpellDamage(2, target) >= target.Health() + target.AllShield() &&
           CastE(target, Mode::Automatic);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (QCharging()) return ReleaseQ(target);
    AIHeroClient resolved = target;
    if (PendingQTargetId != 0 && Now() <= PendingQUntil) {
        const auto pending = ControllerHelpers::HeroByNetworkId(
            PendingQTargetId);
        if (Engine::ValidEnemy(pending, 1700.0f)) resolved = pending;
    } else {
        PendingQTargetId = 0;
        PendingQUntil = 0;
    }
    if (!Engine::ValidEnemy(resolved)) return false;
    if (StartQ(resolved, mode)) return true;
    return CastE(resolved, mode);
}

inline bool TryFlee(const AIHeroClient& preferred) {
    const auto target = ControllerHelpers::NearestEnemyToPlayer(preferred, 950.0f);
    if (!Engine::ValidEnemy(target)) return false;
    if (CastR(target, false, false, true)) return true;
    return CastE(target, Mode::Flee, true);
}

inline bool TryFarm(Mode mode) {
    const bool lastHit = mode == Mode::LastHit;
    const bool jungle = !lastHit && !Engine::ClearUnits(true).empty() &&
        Engine::ClearUnits(false).empty();
    return Engine::TryFarmSpell(2, jungle, lastHit);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    RefreshChargeState();
    RefreshOrbwalkerFocus(mode, preferred);
    if (QCharging()) {
        if (InterruptExpireTick >= Now()) {
            const auto interrupt = ControllerHelpers::HeroByNetworkId(
                InterruptTargetId);
            if (Engine::ValidEnemy(interrupt, 1700.0f) &&
                ReleaseQForReactive(interrupt)) return true;
        }
        if (GapcloserExpireTick >= Now()) {
            const auto gapcloser = ControllerHelpers::HeroByNetworkId(
                GapcloserTargetId);
            if (Engine::ValidEnemy(gapcloser, 1700.0f) &&
                ReleaseQForReactive(gapcloser)) return true;
        }
        auto target = ControllerHelpers::HeroByNetworkId(QTargetId);
        if (!Engine::ValidEnemy(target)) {
            target = SelectSmartTarget(preferred, mode, false);
        }
        return ReleaseQ(target);
    }
    if (TryManualR(preferred)) return true;
    if (TryInterrupt()) return true;
    if (TryAntiGapcloser()) return true;
    if (TryKillSecure(preferred)) return true;
    if (mode == Mode::Flee) return TryFlee(preferred);
    if (mode == Mode::Combo || mode == Mode::Harass) {
        const auto target = SelectSmartTarget(preferred, mode, false);
        return TryCombat(target, mode);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return TryFarm(mode);
    return false;
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.IsAutoAttack) return;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        SpellEventNameContainsAny(args, {"varusq"})) {
        LastQCastTick = now;
        if (QCharging()) {
            if (QChargeStartTick == 0) QChargeStartTick = now;
        }
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               SpellEventNameContainsAny(args, {"varusw"})) {
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               SpellEventNameContainsAny(args, {"varuse"})) {
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               SpellEventNameContainsAny(args, {"varusr"})) {
        LastRCastTick = now;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(
            args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    const auto target = ControllerHelpers::HeroByNetworkId(
        LastAfterAttackTargetId);
    if (Engine::ValidEnemy(target, 850.0f)) {
        (void)SetTemporaryOrbwalkerFocus(
            target, 850.0f, 1000,
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (QCharging()) {
        args.Process = false;
        return;
    }
    const auto focus = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 850.0f);
    if (!focus.IsValid()) {
        ClearTemporaryOrbwalkerFocus(
            OwnedFocusTargetId, OwnedFocusUntil);
        return;
    }
    const int stacks = BlightStacks(focus);
    const bool awaitingFirstStack = stacks == 0 &&
        LastAfterAttackTargetId == OwnedFocusTargetId &&
        Now() - LastAfterAttackTick <= 360;
    const bool qCanConsume = CanUse(0, LastMode, true) &&
        QPrediction(focus, kMaximumQRange);
    const bool eCanConsume = CanUse(2, LastMode, true) &&
        EPrediction(focus);
    if (stacks >= 3 && (qCanConsume || eCanConsume)) {
        args.Process = false;
        return;
    }
    if ((stacks > 0 || awaitingFirstStack) &&
        RedirectBeforeAttackToFocus(args, focus)) return;
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
}



inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "VarusMechanics", "Varus Mechanics"));
    BlightMenu = TacticsMenu->AddSubMenu(new Menu(
        "BlightLogic", "Blight/Orbwalker"));
    BlightMenu->Add(new MenuSeparator(
        "WaitThird", "Wait for stack three when another AA is safe"));
    BlightMenu->Add(new MenuBool(
        "UseWExecute", "Use W before committed low-health Q", true));
    BlightMenu->Add(new MenuSlider(
        "EmpowerHp", "W-Q below target HP (%)", 42, 10, 80));
    ChargeMenu = TacticsMenu->AddSubMenu(new Menu(
        "PiercingLogic", "Piercing Arrow"));
    ChargeMenu->Add(new MenuSeparator(
        "FastRelease", "Q releases at first valid range/prediction frame"));
    ChainMenu = TacticsMenu->AddSubMenu(new Menu(
        "CorruptionLogic", "Chain of Corruption"));
    ChainMenu->Add(new MenuSeparator(
        "ReactiveOnly", "Automatic R is interrupt/self-peel only"));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    QChargeStartTick = QTargetId = PendingQTargetId = PendingQUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = 0;
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = BlightMenu = ChargeMenu = ChainMenu = nullptr;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Anchor spell selection to the orbwalker's current reachable hero target",
    "Force the same AA target while building one-to-three Blight stacks",
    "Redirect BeforeAttack to the owned Blight target when orbwalker changes",
    "Cancel one BeforeAttack at three stacks so Q/E can detonate first",
    "Release every owned forced target when state, range or lifetime ends",
    "Wait at two Blight stacks when one additional auto remains safe",
    "Detonate two stacks when the target escapes or another auto is unsafe",
    "Start Q only for a predicted target inside maximum charge reach",
    "Block attacks while Q is actively charging",
    "Release Q on the first frame current charge range reaches the target",
    "Hold Q through projectile wall and re-evaluate rather than blind fire",
    "Release a charged Q immediately when an interrupt/self-peel R is required",
    "Force release near charge expiry once a valid target line exists",
    "Cast W only immediately before a committed low-health Q",
    "Use E as fallback when Q cannot consume Blight",
    "Clear forced Blight focus as soon as Q/E consumes the stack state",
    "Use R automatically only for interrupt or immediate self peel",
    "Require very-high R prediction and a clear projectile-wall path",
    "Keep manual R under the same targetability and real-range rules",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Varus";
    controller.ControllerId = "champion.kuroaio.ai.varus.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIVarus.md";
    controller.ImplementationSummary =
        "Orbwalker-owned Blight focus with BeforeAttack redirection; safe-third-"
        "stack hold; current-range charged Q start/release state; committed W-Q "
        "execute; E fallback detonation; reactive/manual R and reach rescoring.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 720, 900>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1500, 250, 5000>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Varus
