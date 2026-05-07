#pragma once

#include "../Damages/Damage.h"
#include "../../Enumerations/CollisionObjects.h"
#include "../../Enumerations/DamageType.h"
#include "../../Enumerations/HitChance.h"
#include "../../Enumerations/SpellSlot.h"
#include "../../Enumerations/SpellType.h"
#include "../../Core/Game.h"
#include "../../Math/HealthPrediction.h"
#include "LastCast.h"
#include "../../Core/Objects.h"
#include "../../Math/Prediction.h"

#include <algorithm>
#include <cfloat>
#include <initializer_list>

namespace SDK {

class Spell {
public:
    Spell() = default;
    Spell(SpellSlot slot, float range = FLT_MAX)
        : Slot(slot), Range(range) {}

    SpellSlot Slot = SpellSlot::Unknown;
    float Range = FLT_MAX;
    float Width = 0.0f;
    float Speed = FLT_MAX;
    float Delay = 0.25f;
    bool Collision = false;
    bool IsChargedSpell = false;
    bool IsTargetedSpell = false;
    DamageType DamageType = DamageType::True;
    SpellType Type = SpellType::Unknown;
    Vector3 From = {};
    Vector3 RangeCheckFrom = {};
    int LastCastAttemptTime = 0;

    // ── Charged Spell Properties (matching EnsoulSharp C# Spell) ──
    char ChargedBuffName[64] = {};       // Buff name to check IsCharging (e.g. "XerathArcanopulseChargeUp")
    char ChargedSpellName[64] = {};      // Spell cast name for OnDoCast detection
    float ChargedMinRange = 0.0f;
    float ChargedMaxRange = 0.0f;
    int ChargeDuration = 0;              // ms — time from min to max range

    // ════════════════════════════════════════════════
    // Setup Methods
    // ════════════════════════════════════════════════

    void SetSkillshot(float delay, float width, float speed, bool collision, SpellType type) {
        Delay = delay;
        Width = width;
        Speed = speed;
        Collision = collision;
        Type = type;
        IsTargetedSpell = false;
    }

    void SetTargetted(float delay, float speed) {
        Delay = delay;
        Speed = speed;
        Width = 0.0f;
        Collision = false;
        Type = SpellType::Line;
        IsTargetedSpell = true;
    }

    /// Set up this spell as a charged spell (Xerath Q, Varus Q, Vi Q, etc.)
    /// @param spellName  Internal spell cast name (e.g. "XerathArcanopulseChargeUp")
    /// @param buffName   Buff name to check if currently charging
    /// @param minRange   Minimum range when charge starts
    /// @param maxRange   Maximum range when fully charged
    /// @param durationMs Charge duration in milliseconds
    void SetCharged(const char* spellName, const char* buffName,
                    float minRange, float maxRange, int durationMs) {
        IsChargedSpell = true;
        ChargedMinRange = minRange;
        ChargedMaxRange = maxRange;
        ChargeDuration = durationMs;
        ChargedBuffName[0] = 0;
        ChargedSpellName[0] = 0;
        if (buffName && buffName[0]) {
            lstrcpynA(ChargedBuffName, buffName, sizeof(ChargedBuffName));
        }
        if (spellName && spellName[0]) {
            lstrcpynA(ChargedSpellName, spellName, sizeof(ChargedSpellName));
        }
    }

    // ════════════════════════════════════════════════
    // State Queries
    // ════════════════════════════════════════════════

    bool IsReady() const {
        const auto player = ObjectManager::Player();
        return player.IsValid() && player.GetSpellBook().GetSpell(Slot).IsReady(Game::Time());
    }

    /// Get effective range, accounting for charged spell interpolation
    float GetRange() const {
        if (IsChargedSpell) {
            if (IsCharging()) {
                // Interpolate range based on charge time
                const int elapsed = Game::TickCount() - m_chargedCastedT;
                const float progress = (ChargeDuration > 0)
                    ? static_cast<float>(elapsed) / static_cast<float>(ChargeDuration)
                    : 1.0f;
                const float clamped = (std::min)(1.0f, (std::max)(0.0f, progress));
                return ChargedMinRange + (ChargedMaxRange - ChargedMinRange) * clamped;
            }
            return ChargedMaxRange;
        }

        if (Range < FLT_MAX) {
            return Range;
        }

        const auto player = ObjectManager::Player();
        return player.GetSpellBook().GetSpell(Slot).CastRange();
    }

    /// Check if the charged spell is currently being charged
    /// Matches EnsoulSharp: HasBuff(ChargedBuffName) || TickCount - chargedCastedT < 300 + Ping
    bool IsCharging() const {
        if (!IsChargedSpell || !IsReady()) {
            return false;
        }

        // Buff-based check
        if (ChargedBuffName[0]) {
            const auto player = ObjectManager::Player();
            if (player.IsValid() && player.HasBuff(ChargedBuffName)) {
                return true;
            }
        }

        // Timing guard: recently started charging (covers buff propagation delay)
        const int elapsed = Game::TickCount() - m_chargedCastedT;
        return m_chargedCastedT > 0 && elapsed < 300;
    }

    SpellDataInstClient Instance() const {
        const auto player = ObjectManager::Player();
        return player.IsValid() ? player.GetSpellBook().GetSpell(Slot) : SpellDataInstClient();
    }

    Vector3 GetSource() const {
        return From.IsZero() ? ObjectManager::Player().Position() : From;
    }

    // ════════════════════════════════════════════════
    // Prediction
    // ════════════════════════════════════════════════

    PredictionInput GetPredictionInput() const {
        PredictionInput input = {};
        input.From = GetSource();
        input.RangeCheckFrom = RangeCheckFrom.IsZero() ? input.From : RangeCheckFrom;
        input.Range = GetRange();
        input.Speed = Speed;
        input.Delay = Delay;
        input.Radius = Width;
        input.Type = Type;
        input.Collision = Collision;
        input.UseBoundingRadius = true;
        input.CollisionObjects = CollisionObjects::Minions | CollisionObjects::YasuoWall;
        return input;
    }

    PredictionOutput GetPrediction(const AIBaseClient& target) const {
        if (IsTargetedSpell) {
            PredictionOutput out = {};
            out.UnitPosition = target.ServerPosition().IsZero() ? target.Position() : target.ServerPosition();
            out.CastPosition = out.UnitPosition;
            out.Hitchance = target.IsValidTarget(GetRange(), GetSource()) ? HitChance::VeryHigh : HitChance::OutOfRange;
            return out;
        }
        return Prediction::GetPrediction(target, GetPredictionInput());
    }

    PredictionOutput GetPrediction(const AIBaseClient& target, bool collisionCheck, float rangeOverride = -1.0f) const {
        if (IsTargetedSpell) {
            return GetPrediction(target);
        }
        auto input = GetPredictionInput();
        input.Collision = collisionCheck;
        if (rangeOverride >= 0.0f) {
            input.Range = rangeOverride;
        }
        return Prediction::GetPrediction(target, input);
    }

    PredictionOutput GetPrediction(const AIBaseClient& target,
                                   bool collisionCheck,
                                   float rangeOverride,
                                   std::initializer_list<CollisionObjects> collisions) const {
        auto input = GetPredictionInput();
        input.Collision = collisionCheck;
        if (rangeOverride >= 0.0f) {
            input.Range = rangeOverride;
        }

        bool wantsCollision = false;
        CollisionObjects requestedMask = CollisionObjects::None;
        for (const auto collision : collisions) {
            if (collision == CollisionObjects::Minions ||
                collision == CollisionObjects::Heroes ||
                collision == CollisionObjects::YasuoWall ||
                collision == CollisionObjects::BraumShield ||
                collision == CollisionObjects::Walls) {
                wantsCollision = true;
            }
            requestedMask = requestedMask | collision;
        }
        input.Collision = input.Collision || wantsCollision;
        if (requestedMask != CollisionObjects::None) {
            input.CollisionObjects = requestedMask;
        }
        return Prediction::GetPrediction(target, input);
    }

    // ════════════════════════════════════════════════
    // CanCast
    // ════════════════════════════════════════════════

    bool CanCast() const {
        return IsReady();
    }

    bool CanCast(const Vector3& position) const {
        return IsReady() && GetSource().Distance(position) <= GetRange();
    }

    bool CanCast(const GameObject& target) const {
        return target.IsValid() && target.IsValidTarget(GetRange(), GetSource()) && IsReady();
    }

    // ════════════════════════════════════════════════
    // Charged Spell: StartCharging / ShootChargedSpell
    // ════════════════════════════════════════════════

    /// Begin charging the spell (first press). Does nothing if already charging.
    /// Matches EnsoulSharp: Spell.StartCharging()
    bool StartCharging() {
        if (!IsChargedSpell || !IsReady()) {
            return false;
        }

        if (IsCharging()) {
            return false;
        }

        // Throttle: don't re-send charge request within 400ms
        const int now = Game::TickCount();
        if (m_chargedReqSentT > 0 && (now - m_chargedReqSentT) <= 400) {
            return false;
        }

        // Cast the spell slot to start the charge channel
        const Vector3 self = ObjectManager::Player().Position();
        const bool ok = CoreAPI::Control::CastSpell(static_cast<int>(Slot), self, self, 0);
        if (ok) {
            m_chargedReqSentT = now;
            m_chargedCastedT = now;
        }
        return ok;
    }

    /// Begin charging toward a specific direction/position.
    /// Matches EnsoulSharp: Spell.StartCharging(Vector3 position)
    bool StartCharging(const Vector3& position) {
        if (!IsChargedSpell || !IsReady()) {
            return false;
        }

        if (IsCharging()) {
            return false;
        }

        const int now = Game::TickCount();
        if (m_chargedReqSentT > 0 && (now - m_chargedReqSentT) <= 400) {
            return false;
        }

        const Vector3 source = GetSource();
        const bool ok = CoreAPI::Control::CastSpell(static_cast<int>(Slot), source, position, 0);
        if (ok) {
            m_chargedReqSentT = now;
            m_chargedCastedT = now;
        }
        return ok;
    }

    /// Release the charged spell at a position.
    /// Matches EnsoulSharp: ShootChargedSpell(slot, position, releaseCast=true)
    ///   → UpdateChargeableSpell(slot, position, true)
    ///   → CastSpell(slot, position)
    bool ShootChargedSpell(const Vector3& position) {
        if (!IsChargedSpell) {
            return false;
        }

        // UpdateChargedSpell (releaseCast = true) to signal release
        CoreAPI::Control::UpdateChargedSpell(static_cast<int>(Slot), position, true);

        // Follow up with CastSpell to finalize
        const Vector3 source = GetSource();
        return CoreAPI::Control::CastSpell(static_cast<int>(Slot), source, position, 0);
    }

    /// Notify that the charged spell was actually cast (call from OnDoCast / SpellCastTracker)
    /// This updates internal timing for IsCharging() detection.
    void NotifyChargedCast() {
        m_chargedCastedT = Game::TickCount();
    }

    // ════════════════════════════════════════════════
    // Cast Methods (with charged spell auto-routing)
    // ════════════════════════════════════════════════

    bool Cast() const {
        const auto player = ObjectManager::Player();
        if (!player.IsValid()) {
            return false;
        }

        // Cast throttle: 200ms between attempts per slot (matches old NightSharp)
        const int now = Game::TickCount();
        if (LastCastAttemptTime > 0 && (now - LastCastAttemptTime) < 200) {
            return false;
        }

        // Charged spell: start charging on bare Cast()
        if (IsChargedSpell) {
            const_cast<Spell*>(this)->LastCastAttemptTime = now;
            return const_cast<Spell*>(this)->StartCharging();
        }

        const Vector3 self = player.Position();
        const bool ok = CoreAPI::Control::CastSpell(static_cast<int>(Slot), self, self, 0);
        if (ok) {
            const_cast<Spell*>(this)->LastCastAttemptTime = now;
            LastCast::NotifyLocalSpellCast(Slot, self, self, 0, Instance().Name());
        }
        return ok;
    }

    bool Cast(const Vector3& position) const {
        if (!IsReady()) {
            return false;
        }

        // Cast throttle: 200ms between attempts per slot
        const int now = Game::TickCount();
        if (LastCastAttemptTime > 0 && (now - LastCastAttemptTime) < 200) {
            return false;
        }

        // ── Charged spell routing (EnsoulSharp parity) ──
        // If charged: IsCharging → release; else → start charging
        if (IsChargedSpell) {
            const_cast<Spell*>(this)->LastCastAttemptTime = now;
            if (const_cast<Spell*>(this)->IsCharging()) {
                return const_cast<Spell*>(this)->ShootChargedSpell(position);
            } else {
                return const_cast<Spell*>(this)->StartCharging();
            }
        }

        const Vector3 source = GetSource();
        const bool ok = CoreAPI::Control::CastSpell(static_cast<int>(Slot), source, position, 0);
        if (ok) {
            const_cast<Spell*>(this)->LastCastAttemptTime = now;
            LastCast::NotifyLocalSpellCast(Slot, source, position, 0, Instance().Name());
        }
        return ok;
    }

    bool Cast(const GameObject& target, const Vector3& position) const {
        if (!target.IsValid() || !IsReady()) {
            return false;
        }

        const int now = Game::TickCount();
        if (LastCastAttemptTime > 0 && (now - LastCastAttemptTime) < 200) {
            return false;
        }

        // Charged spells ignore target NetID — route to position-based cast
        if (IsChargedSpell) {
            return Cast(position);
        }

        // Skillshots must NOT carry a targetNetId: when the engine sees a NetId
        // for a non-targeted spell, it overrides the supplied `position` with
        // `target.Position()` (auto-faces the target), nuking prediction. Only
        // true targeted spells (Annie Q, etc.) need the NetId routing.
        const Vector3 source = GetSource();
        const auto targetNetId = IsTargetedSpell
            ? static_cast<uint32_t>(target.NetworkId())
            : 0u;
        const bool ok = CoreAPI::Control::CastSpell(static_cast<int>(Slot), source, position, targetNetId);
        if (ok) {
            const_cast<Spell*>(this)->LastCastAttemptTime = now;
            LastCast::NotifyLocalSpellCast(Slot, source, position, target.NetworkId(), Instance().Name());
        }
        return ok;
    }

    bool Cast(const GameObject& target) const {
        if (!target.IsValid() || !IsReady()) {
            return false;
        }

        return Cast(target, target.Position());
    }

    bool CastOnUnit(const GameObject& target) const {
        return Cast(target);
    }

    bool CastPredicted(const AIBaseClient& target, HitChance minimum = HitChance::High) const {
        const auto prediction = GetPrediction(target);
        if (prediction.Hitchance < minimum) {
            return false;
        }

        if (IsTargetedSpell) {
            return Cast(target);
        }

        return Cast(prediction.CastPosition);
    }

    bool CastIfHitchanceEquals(const AIBaseClient& target, HitChance minimum = HitChance::High) const {
        return CastPredicted(target, minimum);
    }

    bool CastMinimumHitchance(const AIBaseClient& target, HitChance minimum = HitChance::High) const {
        return CastPredicted(target, minimum);
    }

    // ════════════════════════════════════════════════
    // Damage / HealthPrediction
    // ════════════════════════════════════════════════

    float GetDamage(const AIBaseClient& target, DamageStage stage = DamageStage::Default) const {
        return Damage::GetSpellDamage(ObjectManager::Player(), target, Slot, stage);
    }

    float GetSpellDamage(const AIBaseClient& target, DamageStage stage = DamageStage::Default) const {
        return GetDamage(target, stage);
    }

    float GetHealthPrediction(const AIBaseClient& target) const {
        const float distance = GetSource().Distance(target.Position());
        const int travelMs = Speed > 0.0f && Speed < FLT_MAX
            ? static_cast<int>((distance / Speed) * 1000.0f)
            : 0;
        return HealthPrediction::GetPrediction(target, travelMs, static_cast<int>(Delay * 1000.0f));
    }

    bool CanKill(const AIBaseClient& target) const {
        return target.IsValid() && GetDamage(target) >= target.Health();
    }

private:
    // ── Charged spell internal timing ──
    int m_chargedCastedT = 0;    // TickCount when charge started / spell was cast
    int m_chargedReqSentT = 0;   // TickCount when start-charge request was sent (throttle)
};

} // namespace SDK
