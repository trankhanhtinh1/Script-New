#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIAatroxGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Aatrox {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::ChampionIs;
using ControllerHelpers::EnemySpellReady;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;

// The Q hitboxes are intentionally modeled per cast. They are not ordinary
// line skillshots: Q1 is a long rectangle with a far-edge sweetspot, Q2 is a
// widening trapezoid, and Q3 is a circle centered in front of Aatrox.
enum class Sequence : int {
    None,
    Q1WaitW,
    ChainWaitQ2,
    ChainWaitPullQ3,
};

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* ChainMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline QStage AvailableQStage = QStage::First;
inline QStage CastingQStage = QStage::First;
inline Sequence ActiveSequence = Sequence::None;

inline int QCastStartTick = 0;
inline int QWindupEndTick = 0;
inline int QRecastExpireTick = 0;
inline int LastQCastTick = 0;
inline int LastQSweetPlanTick = 0;
inline int PlannedQTargetId = 0;
inline bool EUsedDuringQ = false;
inline Vector3 QCastOrigin = {};
inline Vector3 QCastDirection = {};
inline Vector3 LastDashCorrection = {};

inline int WTargetId = 0;
inline int WCastTick = 0;
inline int WExpectedPullTick = 0;
inline int WStateExpireTick = 0;
inline bool WConfirmed = false;
inline Vector3 WCenter = {};

inline int SequenceTargetId = 0;
inline int SequenceExpireTick = 0;
inline int PendingAutoResetTick = 0;
inline int LastAutoTargetId = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserTick = 0;
inline bool RActive = false;

inline constexpr float kQCastSeconds = 0.60f;
inline constexpr int kQStaticLockoutMs = 1000;
inline constexpr int kQRecastWindowMs = 4000;
inline constexpr float kEDistance = 300.0f;

inline constexpr int kQCorrectionMinDelayMs = 225;
inline constexpr int kQCorrectionMaxDelayMs = 400;
inline QStage StageFromName(const char* name, QStage fallback) {
    if (!name || !name[0]) return fallback;
    if (Engine::TextContains(name, "aatroxq3") ||
        Engine::TextContains(name, "third")) {
        return QStage::Third;
    }
    if (Engine::TextContains(name, "aatroxq2") ||
        Engine::TextContains(name, "second")) {
        return QStage::Second;
    }
    if (Engine::TextContains(name, "aatroxq") ||
        Engine::TextContains(name, "first") ||
        Engine::TextContains(name, "thedarkinblade") ||
        Engine::TextContains(name, "darkinblade")) {
        return QStage::First;
    }
    return fallback;
}

inline QStage StageFromBuff() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return AvailableQStage;
    if (player.HasBuff("aatroxq3ready") || player.HasBuff("aatroxq3")) {
        return QStage::Third;
    }
    if (player.HasBuff("aatroxq2ready") || player.HasBuff("aatroxq2")) {
        return QStage::Second;
    }
    return AvailableQStage;
}

inline QStage RuntimeQStage() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        return AvailableQStage;
    }
    const auto instance = player.Spellbook().GetSpell(SDK::SpellSlot::Q);
    if (instance.IsValid()) {
        const std::string name = instance.Name();
        if (!name.empty()) {
            const QStage fromName = StageFromName(name.c_str(), QStage::First);
            if (Engine::TextContains(name.c_str(), "aatrox")) {
                return fromName;
            }
        }
    }
    const QStage fromBuff = StageFromBuff();
    if (fromBuff != AvailableQStage) {
        return fromBuff;
    }
    return AvailableQStage;
}

inline bool TargetIsMitigatingQ(const AIHeroClient& target) {
    return target.HasBuff("FioraW") ||
           target.HasBuff("IreliaWDefense") ||
           target.HasBuff("ireliawdefense") ||
           target.HasBuff("KSanteW") ||
           target.HasBuff("KSanteW_AllOut") ||
           target.HasBuff("PantheonE") ||
           target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzEIcon");
}

inline bool FioraRiposteReady(const AIHeroClient& target) {
    return ChampionIs(target, SDK::ChampionId::Fiora) &&
           EnemySpellReady(target, SDK::SpellSlot::W);
}

inline float MinimumSweetspotScore(bool comboMode = false) {
    const int configured = Slider(QMenu, "Sweetspot", 56);
    const int effective = comboMode ? std::max(28, configured - 18) : configured;
    return static_cast<float>(effective) / 100.0f;
}

inline bool InAutoRange(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
           InAutoAttackRange(target, 15.0f);
}

inline bool IsQWindup() {
    const int now = SDK::Variables::TickCount();
    return QCastStartTick > 0 && now >= QCastStartTick && now < QWindupEndTick;
}

inline bool WActiveFor(const AIHeroClient& target) {
    const int now = SDK::Variables::TickCount();
    return Engine::ValidEnemy(target) && WTargetId != 0 &&
           WTargetId == static_cast<int>(target.NetworkId()) &&
           now <= WStateExpireTick;
}

inline bool CastThrottleReady(int index, bool qDashCorrection = false) {
    if (qDashCorrection) {
        if (index < 0 || index >= 4 || !Engine::RuntimeSpells[index] ||
            !Engine::RuntimeSpells[index]->IsReady()) {
            return false;
        }
        const int now = SDK::Variables::TickCount();
        return index == 2 && IsQWindup() && !EUsedDuringQ &&
               now < QWindupEndTick - std::max(55, Game::Ping() / 2);
    }
    return ControllerHelpers::CastThrottleReady(index, 55);
}

inline bool SafeDash(const Vector3& position,
                     const AIHeroClient& target,
                     bool aggressive) {
    if (!position.IsValid() || position.IsZero() || SDK::NavMesh::IsWall(position)) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        return false;
    }
    const SpellSpec& eSpec = Engine::ResolvedSpecs[2];
    if (Engine::PositionDangerScore(position, target, eSpec) <= -10000.0f) {
        return false;
    }
    if (aggressive && Engine::UnderEnemyTurret(position) &&
        !Bool(Engine::ComboMenu, "AllowTurretDive", false)) {
        return false;
    }
    if (Engine::CountEnemiesAt(position, 650.0f) >
        Slider(Engine::ComboMenu, "MaxCommitEnemies", 2)) {
        return false;
    }
    return true;
}

inline bool FindDashCorrection(QStage stage,
                               const AIHeroClient& target,
                               const Vector3& direction,
                               const Vector3& predicted,
                               Vector3& result,
                               float& improvement,
                               bool aggressive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || direction.IsZero()) {
        return false;
    }
    const Vector3 source = player.Position();
    Vector3 idealSource = predicted - direction * QIdealForward(stage);
    idealSource.y = source.y;
    Vector3 delta = idealSource - source;
    delta.y = 0.0f;
    const float length = delta.Length2D();
    if (length < static_cast<float>(Slider(QMenu, "MinEDistance", 45))) {
        return false;
    }
    const float current = SweetspotScore(
        stage, source, direction, predicted, target.BoundingRadius());
    const float dashLength = std::min(kEDistance, length);
    const Vector3 direct = source + delta / length * dashLength;
    const Vector3 perpendicular = { -direction.z, 0.0f, direction.x };
    const bool angleRiposte = Bool(QMenu, "AngleFiora", true) &&
                              FioraRiposteReady(target);
    std::array<Vector3, 3> candidates = {
        direct,
        direct + perpendicular * (angleRiposte ? 115.0f : 55.0f),
        direct - perpendicular * (angleRiposte ? 115.0f : 55.0f),
    };

    float bestScore = -FLT_MAX;
    Vector3 best = {};
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        Vector3 candidate = candidates[index];
        candidate.y = source.y;
        Vector3 travel = candidate - source;
        travel.y = 0.0f;
        if (travel.Length2D() > kEDistance) {
            candidate = source + travel / travel.Length2D() * kEDistance;
            candidate.y = source.y;
        }
        if (!SafeDash(candidate, target, aggressive)) continue;
        const float sweet = SweetspotScore(
            stage, candidate, direction, predicted, target.BoundingRadius());
        if (sweet < MinimumSweetspotScore(aggressive)) continue;
        float candidateScore = sweet;
        if (angleRiposte) {
            // Prefer a lateral body displacement while the fixed Q blade still
            // travels into Fiora. This mirrors the one-trick Q2-E parry dodge.
            candidateScore += index == 0 ? -0.30f : 0.20f;
        }
        if (candidateScore > bestScore) {
            bestScore = candidateScore;
            best = candidate;
        }
    }
    if (best.IsZero()) return false;
    result = best;
    const float corrected = SweetspotScore(
        stage, result, direction, predicted, target.BoundingRadius());
    improvement = corrected - current;
    return improvement >= 0.14f;
}

inline bool TryCorrectQWithE() {
    if (!IsQWindup() || EUsedDuringQ || !Bool(QMenu, "UseEForQ", true) ||
        !CastThrottleReady(2, true)) {
        return false;
    }
    auto target = Engine::EnemyByNetworkId(PlannedQTargetId);
    if (!Engine::ValidEnemy(target, 1200.0f)) {
        target = ControllerHelpers::NearestEnemyToPlayer({}, 1000.0f);
    }
    if (!Engine::ValidEnemy(target, 1200.0f)) {
        return false;
    }
    const int now = SDK::Variables::TickCount();
    const int elapsedMs = now - QCastStartTick;
    const int correctionDelay = std::clamp(
        Slider(QMenu, "ECommitDelay", 300),
        kQCorrectionMinDelayMs,
        kQCorrectionMaxDelayMs);
    if (elapsedMs < correctionDelay || elapsedMs > kQCorrectionMaxDelayMs) {
        return false;
    }
    const float remaining = static_cast<float>(std::max(0, QWindupEndTick - now)) /
                            1000.0f;
    const Vector3 predicted = PredictPosition(target, remaining);
    const float currentScore = SweetspotScore(
        CastingQStage, GameObjects::Player().Position(), QCastDirection,
        predicted, target.BoundingRadius());
    const bool aggressive = Engine::CurrentMode() == Mode::Combo ||
                            Engine::CurrentMode() == Mode::Harass;
    if (currentScore >= MinimumSweetspotScore(aggressive) + 0.08f) {
        return false;
    }

    Vector3 correction = {};
    float improvement = 0.0f;
    if (!FindDashCorrection(CastingQStage, target, QCastDirection, predicted,
                            correction, improvement, aggressive)) {
        return false;
    }
    if (Engine::ControllerCastPosition(2, correction)) {
        EUsedDuringQ = true;
        LastDashCorrection = correction;
        LastQSweetPlanTick = now;
        return true;
    }
    return false;
}

inline bool PredictionAcceptable(const AIHeroClient& target, SDK::HitChance wanted) {
    if (!Engine::RuntimeSpells[0] || !Engine::ValidEnemy(target)) {
        return false;
    }
    if (Engine::IsHardCrowdControlled(target) || target.IsDashing()) {
        return true;
    }
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    return static_cast<int>(prediction.Hitchance) >= static_cast<int>(wanted);
}

inline bool CastQ(const AIHeroClient& target,
                  Mode mode,
                  bool allowBody,
                  bool allowDashCorrection,
                  bool reactive = false) {
    if (!Engine::ValidEnemy(target, 1050.0f) || !SpellEnabled(0, mode) ||
        !CastThrottleReady(0) || IsQWindup()) {
        return false;
    }
    if (TargetIsMitigatingQ(target)) {
        return false;
    }
    const int now = SDK::Variables::TickCount();
    if (LastQCastTick > 0 && now - LastQCastTick < kQStaticLockoutMs - Game::Ping()) {
        return false;
    }

    AvailableQStage = RuntimeQStage();
    const QStage stage = AvailableQStage;
    const Vector3 source = GameObjects::Player().Position();
    const Vector3 predicted = PredictPosition(target, kQCastSeconds);
    const Vector3 direction = Direction2D(source, predicted);
    if (direction.IsZero()) {
        return false;
    }

    const bool targetIsImmobile = Engine::IsHardCrowdControlled(target);
    const SDK::HitChance wanted = reactive
        ? SDK::HitChance::Medium
        : (Slider(QMenu, "Prediction", 1) == 0
               ? SDK::HitChance::Medium
               : SDK::HitChance::High);
    if (!targetIsImmobile && !PredictionAcceptable(target, wanted) && !WActiveFor(target)) {
        return false;
    }

    const bool comboMode = (mode == Mode::Combo);
    const float score = SweetspotScore(
        stage, source, direction, predicted, target.BoundingRadius());
    const bool bodyHit = BodyCanHit(
        stage, source, direction, predicted, target.BoundingRadius());
    Vector3 correction = {};
    float improvement = 0.0f;
    const bool canCorrect = allowDashCorrection && Engine::RuntimeSpells[2] &&
        Engine::RuntimeSpells[2]->IsReady() && Bool(QMenu, "UseEForQ", true) &&
        FindDashCorrection(stage, target, direction, predicted, correction,
                           improvement, mode == Mode::Combo || mode == Mode::Harass);
    const float minScore = MinimumSweetspotScore(comboMode);
    if (score < minScore && !canCorrect && !(allowBody && bodyHit)) {
        return false;
    }

    PlannedQTargetId = static_cast<int>(target.NetworkId());
    CastingQStage = stage;
    QCastDirection = direction;
    QCastOrigin = source;
    EUsedDuringQ = false;
    LastDashCorrection = canCorrect ? correction : Vector3{};

    const Vector3 castPosition = source + direction * QRange(stage);
    if (Engine::ControllerCastPosition(0, castPosition)) {
        LastQSweetPlanTick = score >= MinimumSweetspotScore(comboMode) || canCorrect ? now : 0;
        if (stage == QStage::First && Bool(ChainMenu, "Q1W", true)) {
            ActiveSequence = Sequence::Q1WaitW;
            SequenceTargetId = PlannedQTargetId;
            SequenceExpireTick = now + 1800;
        } else if (stage == QStage::Second && WActiveFor(target)) {
            ActiveSequence = Sequence::ChainWaitPullQ3;
            SequenceTargetId = PlannedQTargetId;
            SequenceExpireTick = now + 1800;
        }
        return true;
    }
    return false;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!Engine::ValidEnemy(target, 850.0f) || !SpellEnabled(1, mode) ||
        !CastThrottleReady(1) || IsQWindup()) {
        return false;
    }
    const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
    const SDK::HitChance needed = (reactive || Engine::IsHardCrowdControlled(target))
        ? SDK::HitChance::Medium
        : SDK::HitChance::High;
    if (!prediction.CollisionObjects.empty() ||
        static_cast<int>(prediction.Hitchance) < static_cast<int>(needed) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(
            prediction.GetCastPosition(), 40.0f)) {
        return false;
    }
    if (Engine::ControllerCastPosition(1, prediction.GetCastPosition())) {
        const int now = SDK::Variables::TickCount();
        WTargetId = static_cast<int>(target.NetworkId());
        WCastTick = now;
        WExpectedPullTick = now + 1550;
        WStateExpireTick = now + 2400;
        WCenter = prediction.GetUnitPosition();
        WConfirmed = false;
        return true;
    }
    return false;
}

inline bool CastE(const Vector3& destination,
                  const AIHeroClient& target,
                  bool aggressive) {
    if (!CastThrottleReady(2) || IsQWindup() ||
        !SafeDash(destination, target, aggressive)) {
        return false;
    }
    return Engine::ControllerCastPosition(2, destination);
}

inline float QDamage(const AIHeroClient& target, QStage stage, bool sweetspot) {
    if (!Engine::RuntimeSpells[0] || !target.IsValid()) {
        return 0.0f;
    }
    SDK::DamageStage damageStage = SDK::DamageStage::Default;
    if (stage == QStage::Second) damageStage = SDK::DamageStage::SecondCast;
    if (stage == QStage::Third) damageStage = SDK::DamageStage::ThirdCast;
    float damage = Engine::RuntimeSpells[0]->GetDamage(target, damageStage);
    if (sweetspot) {
        damage *= 1.70f;
    }
    return std::max(0.0f, damage);
}

inline bool ShouldUseR(const AIHeroClient& target) {
    if (!Engine::RuntimeSpells[3] || !Engine::RuntimeSpells[3]->IsReady() ||
        RActive || !Engine::ValidEnemy(target, 900.0f) || IsQWindup()) {
        return false;
    }
    if (!SpellEnabled(3, Mode::Combo)) {
        return false;
    }
    const auto player = GameObjects::Player();
    const int now = SDK::Variables::TickCount();
    const int enemies = Engine::CountEnemiesAt(player.Position(), 750.0f);
    const bool engageConnected = WActiveFor(target) ||
        (LastQSweetPlanTick > 0 && now - LastQSweetPlanTick <= 1050) ||
        AvailableQStage != QStage::First;
    if (!engageConnected && enemies < Slider(UltimateMenu, "RMinEnemies", 2)) {
        return false;
    }

    const float aa = SDK::Damage::GetAutoAttackDamage(player, target, true);
    const float cheapLethal = QDamage(target, AvailableQStage, true) + aa;
    if (Bool(UltimateMenu, "SaveIfLethal", true) && enemies < 2 &&
        cheapLethal >= target.Health() + target.AllShield()) {
        return false;
    }
    return target.HealthPercent() <=
               static_cast<float>(Slider(UltimateMenu, "RTargetHp", 72)) ||
           enemies >= Slider(UltimateMenu, "RMinEnemies", 2) ||
           (player.HealthPercent() <=
                static_cast<float>(Slider(UltimateMenu, "RPlayerHp", 32)) &&
            (Engine::RuntimeSpells[0]->IsReady() ||
             Engine::RuntimeSpells[2]->IsReady()));
}

inline bool TryR(const AIHeroClient& target) {
    if (!ShouldUseR(target) || !CastThrottleReady(3)) {
        return false;
    }
    return Engine::ControllerCastSelf(3);
}


inline bool ShouldWaitForAuto(const AIHeroClient& target, QStage stage) {
    if (!Bool(ChainMenu, "WeaveAutos", true) || !InAutoRange(target) ||
        !Orbwalker::CanAttack()) {
        return false;
    }
    if (target.HasBuff("JaxCounterStrike") || target.HasBuff("FioraW")) {
        return false;
    }
    const auto player = GameObjects::Player();
    const int now = SDK::Variables::TickCount();
    const bool passive = player.HasBuff("aatroxpassiveready");
    if (passive) {
        return QRecastExpireTick <= 0 || QRecastExpireTick - now > 720;
    }
    if (stage == QStage::Second && now - LastQCastTick < 1320) {
        return true;
    }
    if (stage == QStage::Third && WActiveFor(target) &&
        WExpectedPullTick - now > 720) {
        return true;
    }
    return false;
}

inline bool ShouldHoldQ3(const AIHeroClient& target, Mode mode);

inline bool TrySequence(const AIHeroClient& fallbackTarget, Mode mode) {
    const int now = SDK::Variables::TickCount();
    if (ActiveSequence == Sequence::None || now > SequenceExpireTick) {
        ActiveSequence = Sequence::None;
        return false;
    }
    auto target = Engine::EnemyByNetworkId(SequenceTargetId);
    if (!Engine::ValidEnemy(target, 1150.0f)) {
        target = fallbackTarget;
    }
    if (!Engine::ValidEnemy(target, 1150.0f)) {
        ActiveSequence = Sequence::None;
        return false;
    }

    if (ActiveSequence == Sequence::Q1WaitW) {
        if (WActiveFor(target)) {
            ActiveSequence = Sequence::ChainWaitQ2;
            SequenceExpireTick = now + 1800;
        } else if (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsReady() &&
                   CastW(target, mode)) {
            ActiveSequence = Sequence::ChainWaitQ2;
            SequenceExpireTick = now + 1800;
            return true;
        } else if (now - LastQCastTick > 750) {
            ActiveSequence = Sequence::ChainWaitQ2;
        }
        return false;
    }
    if (ActiveSequence == Sequence::ChainWaitQ2) {
        if (!WActiveFor(target) && ShouldWaitForAuto(target, QStage::Second)) {
            return false;
        }
        if (CastQ(target, mode, false, true) ||
            CastQ(target, mode, true, true)) {
            ActiveSequence = Sequence::ChainWaitPullQ3;
            SequenceExpireTick = now + 1900;
            return true;
        }
        return false;
    }
    if (ActiveSequence == Sequence::ChainWaitPullQ3) {
        if (ShouldHoldQ3(target, mode)) {
            return false;
        }
        if (ShouldWaitForAuto(target, QStage::Third)) {
            return false;
        }
        if (WActiveFor(target) && Bool(ChainMenu, "TimeQ3Pull", true) &&
            now < WExpectedPullTick - 660) {
            return false;
        }
        if (CastQ(target, mode, false, true) ||
            CastQ(target, mode, true, true)) {
            ActiveSequence = Sequence::None;
            return true;
        }
    }
    return false;
}

inline bool ShouldHoldQ3(const AIHeroClient& target, Mode mode) {
    if (AvailableQStage != QStage::Third || !Engine::ValidEnemy(target)) {
        return false;
    }
    if (TargetIsMitigatingQ(target) || FioraRiposteReady(target) ||
        target.HasBuff("UndyingRage")) {
        return true;
    }
    if (mode == Mode::Harass && Bool(ChainMenu, "HoldQ3Harass", true) &&
        !WActiveFor(target) && target.HealthPercent() > 32.0f) {
        return true;
    }
    return false;
}

inline bool TryKillSecure() {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true) || IsQWindup()) {
        return false;
    }
    AvailableQStage = RuntimeQStage();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1000.0f)) {
            continue;
        }
        const float shielded = enemy.Health() + enemy.AllShield();
        if (Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsReady() &&
            QDamage(enemy, AvailableQStage, true) * 0.94f >= shielded &&
            CastQ(enemy, Mode::Automatic, false, true, true)) {
            return true;
        }
        if (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsReady()) {
            const float damage = Engine::RuntimeSpells[1]->GetDamage(enemy);
            if (damage > 0.0f && damage * 0.94f >= shielded &&
                CastW(enemy, Mode::Automatic, true)) {
                return true;
            }
        }
    }
    return false;
}

inline bool TryInterrupt() {
    const int now = SDK::Variables::TickCount();
    if (!Bool(Engine::AutomaticMenu, "Interrupt", true) ||
        InterruptTargetId == 0 || now > InterruptExpireTick || IsQWindup()) {
        return false;
    }
    const auto target = Engine::EnemyByNetworkId(InterruptTargetId);
    if (!Engine::ValidEnemy(target, 950.0f) ||
        InterruptExpireTick - now <= 560 + Game::Ping()) {
        return false;
    }
    if (CastQ(target, Mode::Automatic, false, true, true)) {
        InterruptTargetId = 0;
        return true;
    }
    return false;
}

inline bool TryAntiGapcloser() {
    const int now = SDK::Variables::TickCount();
    if (!Bool(Engine::AutomaticMenu, "AntiGapcloser", true) ||
        GapcloserTargetId == 0 || now - GapcloserTick > 700 || IsQWindup()) {
        return false;
    }
    const auto target = Engine::EnemyByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(target, 850.0f)) {
        return false;
    }
    AvailableQStage = RuntimeQStage();
    if (AvailableQStage == QStage::Third &&
        CastQ(target, Mode::Automatic, false, true, true)) {
        GapcloserTargetId = 0;
        return true;
    }
    if (CastW(target, Mode::Automatic, true)) {
        GapcloserTargetId = 0;
        return true;
    }
    if (CastQ(target, Mode::Automatic, true, true, true)) {
        GapcloserTargetId = 0;
        return true;
    }
    return false;
}

inline bool TryAutoReset(const AIHeroClient& target) {
    const int now = SDK::Variables::TickCount();
    if (!Bool(ChainMenu, "EAutoReset", true) || PendingAutoResetTick <= 0 ||
        now - PendingAutoResetTick > 260 || !Engine::ValidEnemy(target) ||
        !InAutoRange(target) || AvailableQStage != QStage::First ||
        (Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsReady()) ||
        !Engine::RuntimeSpells[2] || !Engine::RuntimeSpells[2]->IsReady()) {
        return false;
    }
    const auto player = GameObjects::Player();
    Vector3 destination = player.Position().Extend(target.Position(), 85.0f);
    if (!SafeDash(destination, target, false)) {
        destination = player.Position().Extend(target.Position(), 70.0f);
    }
    if (CastE(destination, target, false)) {
        PendingAutoResetTick = 0;
        return true;
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target, 1100.0f)) {
        return false;
    }
    AvailableQStage = RuntimeQStage();
    if (Orbwalker::IsWindingUp()) {
        return false;
    }
    if (TrySequence(target, mode)) {
        return true;
    }

    if (AvailableQStage == QStage::First) {
        const float distance = GameObjects::Player().Distance(target);
        const bool allowBodyFallback =
            distance > 335.0f || Bool(ChainMenu, "MeleeBranch", true);
        if (CastQ(target, mode, false, true)) {
            return true;
        }
        if (allowBodyFallback && CastQ(target, mode, true, true)) {
            return true;
        }
        if (TryR(target) || TryAutoReset(target)) {
            return true;
        }
        return false;
    }

    if (ShouldWaitForAuto(target, AvailableQStage) ||
        ShouldHoldQ3(target, mode)) {
        return false;
    }
    if (CastQ(target, mode, false, true) ||
        CastQ(target, mode, true, true)) {
        return true;
    }
    if (TryR(target) || TryAutoReset(target)) {
        return true;
    }
    return false;
}

inline bool TryFlee() {
    AIHeroClient pursuer{};
    float bestDistance = FLT_MAX;
    const auto player = GameObjects::Player();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 950.0f)) continue;
        const float distance = player.Distance(enemy);
        if (distance < bestDistance) {
            bestDistance = distance;
            pursuer = enemy;
        }
    }
    if (Engine::ValidEnemy(pursuer, 850.0f) && CastW(pursuer, Mode::Flee, true)) {
        return true;
    }
    AvailableQStage = RuntimeQStage();
    if (Engine::ValidEnemy(pursuer, 700.0f) &&
        CastQ(pursuer, Mode::Flee, true, true, true)) {
        return true;
    }
    if (Engine::RuntimeSpells[2] && Engine::RuntimeSpells[2]->IsReady() &&
        SpellEnabled(2, Mode::Flee)) {
        SpellSpec spec = Engine::ResolvedSpecs[2];
        spec.Aim = AimPolicy::SafeCursor;
        const Vector3 destination = Engine::BestSafePosition(spec, pursuer, AimPolicy::SafeCursor);
        if (destination.IsValid() && CastE(destination, pursuer, false)) {
            return true;
        }
    }
    if (Bool(UltimateMenu, "RFlee", true) && player.HealthPercent() <= 24.0f &&
        Engine::ValidEnemy(pursuer, 650.0f) && CastThrottleReady(3) && !RActive) {
        return Engine::ControllerCastSelf(3);
    }
    return false;
}


struct FarmDirection {
    Vector3 Direction = {};
    int Hits = 0;
    int SweetHits = 0;
    float Score = -FLT_MAX;
    AIBaseClient Primary = {};
};

inline FarmDirection BestFarmDirection(const std::vector<AIBaseClient>& units,
                                       QStage stage,
                                       bool lastHitOnly) {
    FarmDirection best{};
    const auto player = GameObjects::Player();
    for (const auto& candidate : units) {
        if (!candidate.IsValid() || candidate.IsDead()) continue;
        const Vector3 predictedCandidate = PredictPosition(candidate, kQCastSeconds);
        const Vector3 direction = Direction2D(player.Position(), predictedCandidate);
        if (direction.IsZero()) continue;
        int hits = 0;
        int sweet = 0;
        float killValue = 0.0f;
        for (const auto& unit : units) {
            if (!unit.IsValid() || unit.IsDead()) continue;
            const Vector3 predicted = PredictPosition(unit, kQCastSeconds);
            const float sweetScore = SweetspotScore(
                stage, player.Position(), direction, predicted, unit.BoundingRadius());
            const bool body = BodyCanHit(
                stage, player.Position(), direction, predicted, unit.BoundingRadius());
            if (body) ++hits;
            if (sweetScore >= 0.30f) ++sweet;
            if (lastHitOnly && body && Engine::RuntimeSpells[0]) {
                SDK::DamageStage damageStage = SDK::DamageStage::Default;
                if (stage == QStage::Second) damageStage = SDK::DamageStage::SecondCast;
                if (stage == QStage::Third) damageStage = SDK::DamageStage::ThirdCast;
                const float damage = Engine::RuntimeSpells[0]->GetDamage(unit, damageStage) *
                                     (sweetScore >= 0.30f ? 1.70f : 1.0f);
                const float health = SDK::HealthPrediction::GetPrediction(unit, 650);
                if (health > 0.0f && damage >= health) killValue += 4.0f;
            }
        }
        const float score = static_cast<float>(hits) +
                            static_cast<float>(sweet) * 2.25f + killValue;
        if (score > best.Score) {
            best.Direction = direction;
            best.Hits = hits;
            best.SweetHits = sweet;
            best.Score = score;
            best.Primary = candidate;
        }
    }
    return best;
}

inline bool TryFarm(bool lastHitOnly) {
    if (!SpellEnabled(0, lastHitOnly ? Mode::LastHit : Mode::LaneClear) ||
        !Engine::RuntimeSpells[0] || !Engine::RuntimeSpells[0]->IsReady() ||
        !CastThrottleReady(0) || IsQWindup()) {
        return false;
    }
    auto lane = Engine::ClearUnits(false);
    auto jungle = Engine::ClearUnits(true);
    const bool useJungle = lane.empty() && !jungle.empty();
    auto& units = useJungle ? jungle : lane;
    if (units.empty()) return false;

    AvailableQStage = RuntimeQStage();
    const FarmDirection best = BestFarmDirection(units, AvailableQStage, lastHitOnly);
    const int minimum = useJungle
        ? 1
        : (lastHitOnly ? 1 : Slider(FarmMenu, "QMinions", 3));
    if (best.Hits < minimum || best.Direction.IsZero()) {
        return false;
    }
    if (lastHitOnly && best.Score < 4.0f) {
        return false;
    }
    if (!useJungle && best.SweetHits == 0 && Bool(FarmMenu, "SweetOnly", true)) {
        return false;
    }
    PlannedQTargetId = 0;
    CastingQStage = AvailableQStage;
    QCastOrigin = GameObjects::Player().Position();
    QCastDirection = best.Direction;
    EUsedDuringQ = true; // Never spend E to correct a lane-clear Q.
    const Vector3 castPosition = QCastOrigin + best.Direction * QRange(AvailableQStage);
    return Engine::ControllerCastPosition(0, castPosition);
}

inline void RefreshState() {
    const int now = SDK::Variables::TickCount();
    if (QRecastExpireTick > 0 && now > QRecastExpireTick) {
        AvailableQStage = QStage::First;
        QRecastExpireTick = 0;
        ActiveSequence = Sequence::None;
    }
    if (WStateExpireTick > 0 && now > WStateExpireTick) {
        WTargetId = 0;
        WConfirmed = false;
        WCenter = {};
    }
    const auto player = GameObjects::Player();
    RActive = player.IsValid() && player.HasBuff("aatroxr");
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    RefreshState();
    if (IsQWindup()) {
        (void)TryCorrectQWithE();
        return true;
    }
    if (TryInterrupt() || TryAntiGapcloser() || TryKillSecure()) {
        return true;
    }
    if (mode == Mode::Flee) {
        (void)TryFlee();
        return true;
    }
    if (mode == Mode::Combo || mode == Mode::Harass) {
        AIHeroClient target = Engine::SelectTarget(1150.0f);
        if (!Engine::ValidEnemy(target, 1150.0f)) {
            target = ControllerHelpers::NearestEnemyToPlayer({}, 1150.0f);
        }
        (void)TryCombo(target, mode);
        return true;
    }
    if (mode == Mode::LaneClear) {
        (void)TryFarm(false);
        return true;
    }
    if (mode == Mode::LastHit) {
        (void)TryFarm(true);
        return true;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || args.Slot < 0 || args.Slot > 3) {
        return;
    }
    const int now = SDK::Variables::TickCount();
    if (args.Slot == 0) {
        const QStage stage = StageFromName(args.SpellName, AvailableQStage);
        CastingQStage = stage;
        AvailableQStage = stage == QStage::First
            ? QStage::Second
            : (stage == QStage::Second ? QStage::Third : QStage::First);
        LastQCastTick = now;
        QCastStartTick = now;
        const int castMs = args.CastDelay > 0.10f
            ? static_cast<int>(args.CastDelay * 1000.0f)
            : 600;
        QWindupEndTick = now + std::clamp(castMs, 480, 720);
        QRecastExpireTick = stage == QStage::Third
            ? 0
            : QWindupEndTick + kQRecastWindowMs;
        QCastOrigin = args.StartPosition.IsValid() && !args.StartPosition.IsZero()
            ? args.StartPosition
            : GameObjects::Player().Position();
        Vector3 castEnd = args.CastPosition;
        if (!castEnd.IsValid() || castEnd.IsZero()) castEnd = args.EndPosition;
        const Vector3 eventDirection = Direction2D(QCastOrigin, castEnd);
        if (!eventDirection.IsZero()) QCastDirection = eventDirection;
        EUsedDuringQ = false;
    } else if (args.Slot == 1) {
        WCastTick = now;
    } else if (args.Slot == 2) {
        if (IsQWindup()) EUsedDuringQ = true;
    } else if (args.Slot == 3) {
        RActive = true;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = SDK::Variables::TickCount();
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "aatroxr")) {
        RActive = true;
        return;
    }
    const auto enemy = Engine::EnemyByNetworkId(static_cast<int>(args.Sender.NetworkId));
    if (Engine::ValidEnemy(enemy) && Engine::TextContains(args.BuffName, "aatroxw")) {
        WTargetId = static_cast<int>(enemy.NetworkId());
        WConfirmed = true;
        const int remaining = ControllerHelpers::RemainingMilliseconds(
            args.EndTime, 1500, 350, 2200);
        WExpectedPullTick = now + remaining;
        WStateExpireTick = WExpectedPullTick + 400;
        if (WCenter.IsZero()) WCenter = enemy.Position();
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "aatroxr")) {
        RActive = false;
        return;
    }
    if (static_cast<int>(args.Sender.NetworkId) == WTargetId &&
        Engine::TextContains(args.BuffName, "aatroxw")) {
        WStateExpireTick = SDK::Variables::TickCount() + 180;
        WConfirmed = false;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    const int now = SDK::Variables::TickCount();
    if (AvailableQStage == QStage::Third && WTargetId != 0 &&
        Bool(ChainMenu, "TimeQ3Pull", true) &&
        WExpectedPullTick - now <= 690 && WExpectedPullTick - now > 180 &&
        Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsReady()) {
        // A late AA would miss the guaranteed Q3-on-pull timing.
        args.Process = false;
    }
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (!args.IsDirectedToPlayer) return;
    GapcloserTargetId = static_cast<int>(args.NetworkId);
    GapcloserTick = SDK::Variables::TickCount();
}

inline void DrawQGeometry() {
    if (!IsQWindup() || QCastDirection.IsZero()) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const Vector3 source = player.Position();
    const Vector3 perpendicular = { -QCastDirection.z, 0.0f, QCastDirection.x };
    if (CastingQStage == QStage::First) {
        const Vector3 nearCenter = source + QCastDirection * 475.0f;
        const Vector3 farCenter = source + QCastDirection * 650.0f;
        Drawing::DrawLine(nearCenter - perpendicular * 90.0f,
                          nearCenter + perpendicular * 90.0f,
                          0xFFFF5555u, 2.0f);
        Drawing::DrawLine(farCenter - perpendicular * 90.0f,
                          farCenter + perpendicular * 90.0f,
                          0xFFFF5555u, 2.0f);
    } else if (CastingQStage == QStage::Second) {
        const Vector3 nearCenter = source + QCastDirection * 300.0f;
        const Vector3 farCenter = source + QCastDirection * 500.0f;
        Drawing::DrawLine(nearCenter - perpendicular * 195.0f,
                          farCenter - perpendicular * 250.0f,
                          0xFFFF8844u, 2.0f);
        Drawing::DrawLine(nearCenter + perpendicular * 195.0f,
                          farCenter + perpendicular * 250.0f,
                          0xFFFF8844u, 2.0f);
    } else {
        Drawing::DrawCircle(source + QCastDirection * 200.0f,
                            180.0f, 0xFFFFAA55u, 2.0f, 64);
    }
}

inline void OnDraw() {
    if (!CoachMenu || !GameObjects::Player().IsValid()) return;
    if (Bool(CoachMenu, "DrawSweetspot", false)) {
        DrawQGeometry();
    }
    if (Bool(CoachMenu, "DrawCorrection", false) &&
        LastDashCorrection.IsValid() && !LastDashCorrection.IsZero() && IsQWindup()) {
        Drawing::DrawCircle(LastDashCorrection, 45.0f, 0xFF55FF88u, 2.0f, 40);
        Drawing::DrawLine(GameObjects::Player().Position(), LastDashCorrection,
                          0xFF55FF88u, 2.0f);
    }
    if (Bool(CoachMenu, "DrawW", false) && WTargetId != 0 &&
        SDK::Variables::TickCount() <= WStateExpireTick && !WCenter.IsZero()) {
        Drawing::DrawCircle(WCenter, 250.0f,
                            WConfirmed ? 0xFFCC66FFu : 0x88CC66FFu, 1.5f, 56);
    }
    if (Bool(CoachMenu, "DrawState", false)) {
        Vec2 screen = {};
        if (Drawing::WorldToScreen(GameObjects::Player().Position(), screen)) {
            char state[160] = {};
            _snprintf_s(state, sizeof(state), _TRUNCATE,
                        "Aatrox one-trick | Q%d | seq %d | W %s | R %s",
                        static_cast<int>(AvailableQStage),
                        static_cast<int>(ActiveSequence),
                        WConfirmed ? "pull" : (WTargetId ? "pending" : "off"),
                        RActive ? "active" : "ready");
            Drawing::DrawText(screen.x - 105.0f, screen.y - 112.0f,
                              0xFFFFDDCCu, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AatroxMechanics", "Aatrox Mechanics"));

    QMenu = TacticsMenu->AddSubMenu(new Menu("QGeometry", "Q Sweetspots"));
    QMenu->Add(new MenuBool("UseEForQ", "Use E to improve Q sweetspot", true));
    QMenu->Add(new MenuSlider("Sweetspot", "Min sweetspot confidence (%)", 56, 25, 90));
    QMenu->Add(new MenuSlider("MinEDistance", "Min E correction distance", 45, 20, 160));
    QMenu->Add(new MenuSlider(
        "ECommitDelay", "Q-E correction delay (ms)", 300, 225, 400));
    QMenu->Add(new MenuList(
        "Prediction", "Q prediction confidence", { "Medium", "High" }, 1));
    QMenu->Add(new MenuBool(
        "AngleFiora", "Angle Q-E vs Fiora W", true));

    ChainMenu = TacticsMenu->AddSubMenu(new Menu("Chains", "Combo Chains"));
    ChainMenu->Add(new MenuBool("Q1W", "Q1 -> W -> Q2", true));
    ChainMenu->Add(new MenuBool("TimeQ3Pull", "Time Q3 to W pull", true));
    ChainMenu->Add(new MenuBool("HoldQ3Harass", "Hold Q3 in trade", true));
    ChainMenu->Add(new MenuBool(
        "MeleeBranch", "Melee dive defense spacing", true));
    ChainMenu->Add(new MenuBool("WeaveAutos", "Weave passive/auto attacks", true));
    ChainMenu->Add(new MenuBool("EAutoReset", "Use E as AA reset", true));

    UltimateMenu = TacticsMenu->AddSubMenu(new Menu("WorldEnder", "Ultimate R"));
    UltimateMenu->Add(new MenuSlider("RTargetHp", "R if target HP below (%)", 72, 20, 100));
    UltimateMenu->Add(new MenuSlider("RPlayerHp", "R if own HP below (%)", 32, 10, 70));
    UltimateMenu->Add(new MenuSlider("RMinEnemies", "R if nearby enemies count", 2, 1, 5));
    UltimateMenu->Add(new MenuBool("SaveIfLethal", "Skip R (target dies)", true));
    UltimateMenu->Add(new MenuBool("RFlee", "R to flee", true));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu("AatroxFarm", "Lane Clear"));
    FarmMenu->Add(new MenuSlider("QMinions", "Min minions hit by Q", 3, 1, 8));
    FarmMenu->Add(new MenuBool("SweetOnly", "Require sweetspot hit", true));
    FarmMenu->Add(new MenuSeparator(
        "NoFarmE", "E is saved during lane clear"));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu("AatroxCoach", "Drawings"));
    CoachMenu->Add(new MenuBool("DrawSweetspot", "Draw Q sweetspots", false));
    CoachMenu->Add(new MenuBool("DrawCorrection", "Draw planned E", false));
    CoachMenu->Add(new MenuBool("DrawW", "Draw W pull zone", false));
    CoachMenu->Add(new MenuBool("DrawState", "Draw combo state", false));
}

inline void OnLoad() {
    AvailableQStage = RuntimeQStage();
    CastingQStage = AvailableQStage;
    ActiveSequence = Sequence::None;
    QCastStartTick = 0;
    QWindupEndTick = 0;
    QRecastExpireTick = 0;
    LastQCastTick = 0;
    LastQSweetPlanTick = 0;
    PlannedQTargetId = 0;
    EUsedDuringQ = false;
    QCastOrigin = {};
    QCastDirection = {};
    LastDashCorrection = {};
    WTargetId = 0;
    WCastTick = 0;
    WExpectedPullTick = 0;
    WStateExpireTick = 0;
    WConfirmed = false;
    WCenter = {};
    SequenceTargetId = 0;
    SequenceExpireTick = 0;
    PendingAutoResetTick = 0;
    LastAutoTargetId = 0;
    InterruptTargetId = 0;
    InterruptExpireTick = 0;
    GapcloserTargetId = 0;
    GapcloserTick = 0;
    RActive = GameObjects::Player().HasBuff("aatroxr");
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    QMenu = nullptr;
    ChainMenu = nullptr;
    UltimateMenu = nullptr;
    FarmMenu = nullptr;
    CoachMenu = nullptr;
    ActiveSequence = Sequence::None;
}

inline constexpr const char* Scenarios[] = {
    "Q stage synchronization from runtime name and local cast events",
    "Resolve Q2 as a widening cone and Q3 as a circle in the runtime catalog",
    "Q1 far-edge sweetspot without spending E",
    "Q1 forward E correction for an escaping target",
    "Q1 backward E correction against a target stepping inside",
    "Q2 widening-trapezoid and corner sweetspot alignment",
    "Q2 lateral E correction during the fixed-direction windup",
    "Q3 center-circle E correction",
    "Deliberately drop Q3 during a short harass trade",
    "Reset state when the four-second Q recast window expires",
    "Q1-W-Q2 standard chain setup",
    "Time Q3 impact to the Infernal Chains pull",
    "Adapt Q2 toward the target's W escape path",
    "Weave Deathbringer Stance between Q casts",
    "Reserve E for Q before considering its auto-reset use",
    "Use spare E as an auto reset after Q becomes unavailable",
    "Activate R only after engage confirmation",
    "Activate R for a multi-target extended fight",
    "Save R when Q plus passive attack already kills",
    "Use R's movement/healing amplification in a critical retreat",
    "Q3 sweetspot peel against a directed gapcloser",
    "W peel fallback against a directed gapcloser",
    "Interrupt a long channel only when Q sweetspot lands in time",
    "Flee with safe-cursor E instead of dashing through danger",
    "Reject sweetspot corrections into walls, turrets, or excess enemies",
    "Geometric multi-minion Q lane clear without spending E",
    "Sweetspot-aware jungle Q sequencing",
    "Health-predicted Q last hits",
    "Yield spell timing for a valuable passive auto",
    "Cancel an AA only when it would miss guaranteed Q3-on-pull timing",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Aatrox;
    controller.ControllerId = "champion.kuroaio.ai.aatrox.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIAatrox.md";
    controller.ImplementationSummary =
        "Q1-W-Q2-Q3 chaining, passive weaving, smart R commitment, "
        "reactive peel, and geometric farming.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &PendingAutoResetTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &PendingAutoResetTick>;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Aatrox
