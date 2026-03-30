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

    bool IsReady() const {
        const auto player = ObjectManager::Player();
        return player.IsValid() && player.GetSpellBook().GetSpell(Slot).IsReady(Game::Time());
    }

    float GetRange() const {
        if (Range < FLT_MAX) {
            return Range;
        }

        const auto player = ObjectManager::Player();
        return player.GetSpellBook().GetSpell(Slot).CastRange();
    }

    SpellDataInstClient Instance() const {
        const auto player = ObjectManager::Player();
        return player.IsValid() ? player.GetSpellBook().GetSpell(Slot) : SpellDataInstClient();
    }

    Vector3 GetSource() const {
        return From.IsZero() ? ObjectManager::Player().Position() : From;
    }

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

    bool CanCast() const {
        return IsReady();
    }

    bool CanCast(const Vector3& position) const {
        return IsReady() && GetSource().Distance(position) <= GetRange();
    }

    bool CanCast(const GameObject& target) const {
        return target.IsValid() && target.IsValidTarget(GetRange(), GetSource()) && IsReady();
    }

    bool Cast() const {
        const auto player = ObjectManager::Player();
        if (!player.IsValid()) {
            return false;
        }

        const Vector3 self = player.Position();
        const bool ok = CoreAPI::Control::CastSpell(static_cast<int>(Slot), self, self, 0);
        if (ok) {
            const_cast<Spell*>(this)->LastCastAttemptTime = Game::TickCount();
            LastCast::NotifyLocalSpellCast(Slot, self, self, 0, Instance().Name());
        }
        return ok;
    }

    bool Cast(const Vector3& position) const {
        if (!IsReady()) {
            return false;
        }

        const Vector3 source = GetSource();
        const bool ok = CoreAPI::Control::CastSpell(static_cast<int>(Slot), source, position, 0);
        if (ok) {
            const_cast<Spell*>(this)->LastCastAttemptTime = Game::TickCount();
            LastCast::NotifyLocalSpellCast(Slot, source, position, 0, Instance().Name());
        }
        return ok;
    }

    bool Cast(const GameObject& target) const {
        if (!target.IsValid() || !IsReady()) {
            return false;
        }

        const Vector3 source = GetSource();
        const bool ok = CoreAPI::Control::CastSpell(static_cast<int>(Slot), source, target.Position(), static_cast<uint32_t>(target.NetworkId()));
        if (ok) {
            const_cast<Spell*>(this)->LastCastAttemptTime = Game::TickCount();
            LastCast::NotifyLocalSpellCast(Slot, source, target.Position(), target.NetworkId(), Instance().Name());
        }
        return ok;
    }

    bool CastOnUnit(const GameObject& target) const {
        return Cast(target);
    }

    bool CastPredicted(const AIBaseClient& target, HitChance minimum = HitChance::High) const {
        const auto prediction = GetPrediction(target);
        return static_cast<int>(prediction.Hitchance) >= static_cast<int>(minimum) && Cast(prediction.CastPosition);
    }

    bool CastIfHitchanceEquals(const AIBaseClient& target, HitChance minimum = HitChance::High) const {
        return CastPredicted(target, minimum);
    }

    bool CastMinimumHitchance(const AIBaseClient& target, HitChance minimum = HitChance::High) const {
        return CastPredicted(target, minimum);
    }

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
};

} // namespace SDK
