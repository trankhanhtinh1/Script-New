#pragma once

#include "Database/SpellDatabase.h"

#include "../../Core/Game.h"
#include "../../Core/Objects.h"
#include "../../Core/Variables.h"
#include "../../Enumerations/CastStates.h"
#include "../../Enumerations/CollisionableObjects.h"
#include "../../Enumerations/DamageType.h"
#include "../../Enumerations/HitChance.h"
#include "../../Enumerations/SkillshotType.h"
#include "../../Extensions/Extensions.h"
#include "../../GameObjects/GameObjects.h"
#include "../../Utils/Minion.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace SDK {

#ifndef NIGHTSHARP_SDK_DAMAGE_STAGE_DEFINED
#define NIGHTSHARP_SDK_DAMAGE_STAGE_DEFINED
enum class DamageStage : std::int32_t {
    Default,
    WayBack,
    Detonation,
    DamagePerTick,
    DamagePerTime,
    DamagePerHalfSecond,
    DamagePerQuarterSecond,
    DamagePerSecond,
    SingleTotal,
    SecondForm,
    ThirdForm,
    SecondCast,
    ThirdCast,
    Buff,
    Empowered,
    EmpoweredDamagePerSecond,
    EmpoweredDamagePerHalfSecond,
    EmpoweredDamagePerQuarterSecond
};
#endif

struct PredictionInput {
    AIBaseClient Unit;
    float Delay = 0.0f;
    float Radius = 0.0f;
    float Speed = FLT_MAX;
    Vector3 From = {};
    float Range = FLT_MAX;
    bool Collision = false;
    SkillshotType Type = SkillshotType::SkillshotLine;
    Vector3 RangeCheckFrom = {};
    bool AoE = false;
    CollisionableObjects CollisionObjects =
        CollisionableObjects::Heroes | CollisionableObjects::Minions;
};

struct PredictionOutput {
    Vector3 CastPosition = {};
    Vector3 UnitPosition = {};
    HitChance Hitchance = HitChance::None;
    int AoeTargetsHitCount = 0;
    std::vector<GameObject> CollisionObjects;
};

class Spell {
public:
    using CastConditionDelegate = std::function<bool()>;

    explicit Spell(SpellSlot slot, bool loadFromGame, HitChance hitChance = HitChance::Medium)
        : Slot(slot) {
        if (!loadFromGame) {
            return;
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return;
        }

        const auto spellData = player.Spellbook().GetSpell(slot);
        const auto isSanePositive = [](float value, float maxValue) {
            return std::isfinite(value) && value > 0.0f && value < maxValue;
        };

        const std::string spellName = spellData.Name();
        const auto* databaseEntry = SpellDatabase::GetByName(spellName);

        const float nativeRange = spellData.CastRange();
        if (isSanePositive(nativeRange, 100000.0f)) {
            Range = nativeRange;
        } else if (databaseEntry && databaseEntry->Range > 0 &&
                   databaseEntry->Range < std::numeric_limits<int>::max()) {
            Range = static_cast<float>(databaseEntry->Range);
        }

        const float nativeLineWidth = spellData.LineWidth();
        const float nativeCastRadius = spellData.CastRadius();
        if (isSanePositive(nativeLineWidth, 100000.0f)) {
            Width = nativeLineWidth;
        } else if (isSanePositive(nativeCastRadius, 100000.0f)) {
            Width = nativeCastRadius;
        } else if (databaseEntry) {
            const int databaseWidth = databaseEntry->Width > 0
                ? databaseEntry->Width
                : databaseEntry->Radius;
            if (databaseWidth > 0) {
                Width = static_cast<float>(databaseWidth);
            }
        }

        const float nativeSpeed = spellData.MissileSpeed();
        if (isSanePositive(nativeSpeed, 1000000.0f)) {
            Speed = nativeSpeed;
        } else if (databaseEntry && databaseEntry->MissileSpeed > 0) {
            Speed = static_cast<float>(databaseEntry->MissileSpeed);
        }

        Delay = 0.25f;
        MinHitChance = hitChance;
    }

    explicit Spell(SpellSlot slot, float range = FLT_MAX)
        : Range(range),
          Slot(slot) {
    }

    CastConditionDelegate CastCondition;
    std::string ChargedBuffName;
    int ChargedMaxRange = 0;
    int ChargedMinRange = 0;
    std::string ChargedSpellName;
    int ChargeDuration = 0;
    bool Collision = false;
    DamageType DamageType = DamageType::Physical;
    float Delay = 0.0f;
    Vector3 From = {};
    bool IsChargedSpell = false;
    bool IsSkillshot = false;
    int LastCastAttemptT = 0;
    HitChance MinHitChance = HitChance::Medium;
    float Range = FLT_MAX;
    Vector3 RangeCheckFrom = {};
    SpellSlot Slot = SpellSlot::Unknown;
    float Speed = FLT_MAX;
    SkillshotType Type = SkillshotType::SkillshotLine;
    float Width = 0.0f;

    SpellDataInstClient Instance() const {
        const auto player = GameObjects::Player();
        return player.IsValid() ? player.Spellbook().GetSpell(Slot) : SpellDataInstClient();
    }

    bool IsCharging() const {
        if (!IsReady()) {
            return false;
        }

        const auto player = GameObjects::Player();
        return (player.IsValid() && !ChargedBuffName.empty() && player.HasBuff(ChargedBuffName.c_str())) ||
               Variables::TickCount() - chargedCastedT_ < 300 + Game::Ping();
    }

    int Level() const {
        const auto instance = Instance();
        return instance.IsValid() ? instance.Level() : 0;
    }

    float CurrentRange() const {
        if (!IsChargedSpell) {
            return Range;
        }

        if (IsCharging()) {
            return static_cast<float>(ChargedMinRange) +
                   std::min(
                       static_cast<float>(ChargedMaxRange - ChargedMinRange),
                       ((Variables::TickCount() - chargedCastedT_) *
                        static_cast<float>(ChargedMaxRange - ChargedMinRange) /
                        std::max(1, ChargeDuration)) - 150.0f);
        }

        return static_cast<float>(ChargedMaxRange);
    }

    float RangeSqr() const {
        const float range = CurrentRange();
        return range * range;
    }

    float WidthSqr() const {
        return Width * Width;
    }

    Vector3 SourcePosition() const {
        const auto player = GameObjects::Player();
        return From.IsValid() && !From.IsZero()
            ? From
            : (player.IsValid() ? player.Position() : Vector3());
    }

    Vector3 RangeCheckSource() const {
        const auto player = GameObjects::Player();
        return RangeCheckFrom.IsValid() && !RangeCheckFrom.IsZero()
            ? RangeCheckFrom
            : (player.IsValid() ? player.Position() : Vector3());
    }

    bool CanCast(const AIBaseClient& unit) const {
        return IsReady() && Extensions::IsValidTarget(unit, CurrentRange());
    }

    bool CanKill(const AIBaseClient& unit, DamageStage stage = DamageStage::Default) const {
        return Extensions::IsValidTarget(unit) && GetDamage(unit, stage) > unit.Health();
    }

    CastStates Cast(const AIBaseClient& unit,
                    bool exactHitChance = false,
                    bool areaOfEffect = false,
                    int minTargets = -1,
                    HitChance tempHitChance = HitChance::None) {
        if (!unit.IsValid()) {
            return CastStates::InvalidTarget;
        }

        if (!IsReady()) {
            return CastStates::NotReady;
        }

        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return CastStates::NotCasted;
        }

        if (minManaPercent_ != 0.0f) {
            const float maxMana = std::max(1.0f, player.MaxMana());
            if ((player.Mana() * 100.0f / maxMana) < minManaPercent_) {
                return CastStates::LowMana;
            }
        }

        if (CastCondition && !CastCondition()) {
            return CastStates::FailedCondition;
        }

        if (!areaOfEffect && minTargets != -1) {
            areaOfEffect = true;
        }

        if (!IsSkillshot) {
            if (RangeCheckSource().DistanceSqr2D(unit.Position()) > RangeSqr()) {
                return CastStates::OutOfRange;
            }

            LastCastAttemptT = Variables::TickCount();
            return player.Spellbook().CastSpell(Slot, unit)
                ? CastStates::SuccessfullyCasted
                : CastStates::NotCasted;
        }

        const auto prediction = GetPrediction(unit, areaOfEffect);
        if (minTargets != -1 && prediction.AoeTargetsHitCount <= minTargets) {
            return CastStates::NotEnoughTargets;
        }

        if (!prediction.CollisionObjects.empty()) {
            return CastStates::Collision;
        }

        if (RangeCheckSource().DistanceSqr2D(prediction.CastPosition) > RangeSqr()) {
            return CastStates::OutOfRange;
        }

        const HitChance needed = tempHitChance == HitChance::None ? MinHitChance : tempHitChance;
        if (ChanceValue(prediction.Hitchance) < ChanceValue(needed) ||
            (exactHitChance && prediction.Hitchance != needed)) {
            return CastStates::LowHitChance;
        }

        LastCastAttemptT = Variables::TickCount();
        if (IsChargedSpell) {
            if (IsCharging()) {
                ShootChargedSpell(Slot, prediction.CastPosition);
            } else {
                StartCharging();
            }
        } else if (!player.Spellbook().CastSpell(Slot, prediction.CastPosition)) {
            return CastStates::NotCasted;
        }

        return CastStates::SuccessfullyCasted;
    }

    bool Cast() {
        return CastOnUnit(GameObjects::Player());
    }

    bool Cast(const Vector2& fromPosition, const Vector2& toPosition) {
        return Cast(Vector3::From2D(fromPosition), Vector3::From2D(toPosition));
    }

    bool Cast(const Vector3& fromPosition, const Vector3& toPosition) {
        if (!IsReady()) {
            return false;
        }

        LastCastAttemptT = Variables::TickCount();
        return GameObjects::Player().Spellbook().CastSpell(Slot, fromPosition, toPosition);
    }

    bool Cast(const Vector2& position) {
        return Cast(Vector3::From2D(position));
    }

    bool Cast(const Vector3& position) {
        if (!IsReady()) {
            return false;
        }

        LastCastAttemptT = Variables::TickCount();
        if (IsChargedSpell) {
            if (IsCharging()) {
                ShootChargedSpell(Slot, position);
            } else {
                StartCharging();
            }
            return false;
        }

        return GameObjects::Player().Spellbook().CastSpell(Slot, position);
    }

    CastStates CastIfHitchanceEquals(const AIBaseClient& unit, HitChance hitChance) {
        return Cast(unit, true, false, -1, hitChance);
    }

    CastStates CastIfHitchanceMinimum(const AIBaseClient& unit, HitChance hitChance) {
        return Cast(unit, false, false, -1, hitChance);
    }

    CastStates CastIfWillHit(const AIBaseClient& unit, int minTargets = 5) {
        return Cast(unit, false, true, minTargets);
    }

    CastStates CastOnBestTarget(float extraRange = 0.0f,
                                bool areaOfEffect = false,
                                int minTargets = -1) {
        return Cast(GetTarget(extraRange), false, areaOfEffect, minTargets);
    }

    bool CastOnUnit(const AIBaseClient& unit) {
        if (!IsReady() || SourcePosition().DistanceSqr2D(unit.Position()) > RangeSqr()) {
            return false;
        }

        LastCastAttemptT = Variables::TickCount();
        return GameObjects::Player().Spellbook().CastSpell(Slot, unit);
    }

    int CountHits(const std::vector<AIBaseClient>& units, const Vector3& castPosition) {
        std::vector<Vector3> points;
        points.reserve(units.size());
        for (const auto& unit : units) {
            points.push_back(GetPrediction(unit).UnitPosition);
        }
        return CountHits(points, castPosition);
    }

    int CountHits(const std::vector<Vector3>& points, const Vector3& castPosition) const {
        int count = 0;
        for (const auto& point : points) {
            if (WillHit(point, castPosition)) {
                ++count;
            }
        }
        return count;
    }

    Utils::FarmLocation GetCircularFarmLocation(const std::vector<AIBaseClient>& minions,
                                                float overrideWidth = -1.0f) const {
        const auto positions = Utils::Minion::GetMinionsPredictedPositions(
            minions,
            Delay,
            Width,
            Speed,
            SourcePosition(),
            CurrentRange(),
            false,
            SkillshotType::SkillshotCircle);
        return GetCircularFarmLocation(positions, overrideWidth);
    }

    Utils::FarmLocation GetCircularFarmLocation(const std::vector<Vector2>& minionPositions,
                                                float overrideWidth = -1.0f) const {
        return Utils::Minion::GetBestCircularFarmLocation(
            minionPositions,
            overrideWidth >= 0.0f ? overrideWidth : Width,
            CurrentRange());
    }

    std::vector<AIBaseClient> GetCollision(const Vector2& /*fromVector2*/,
                                           const std::vector<Vector2>& /*to*/,
                                           float /*delayOverride*/ = -1.0f) const {
        // TODO(SDK parity): port SDK::Collision/Movement prediction; current
        // Collision.h references missing Prediction.h.
        return {};
    }

    float GetDamage(const AIBaseClient& /*target*/, DamageStage /*stage*/ = DamageStage::Default) const {
        // TODO(SDK parity): wire to SDK::Damage::GetSpellDamage after the
        // damage wrapper is made self-contained for AIBaseClient accessors.
        return 0.0f;
    }

    float GetHealthPrediction(const AIBaseClient& unit) const {
        // TODO(SDK parity): restore HealthPrediction after Prediction/Health.h
        // is ported into NightSharp/SDK/Math/Prediction.
        return unit.IsValid() ? unit.Health() : 0.0f;
    }

    float GetHitCount(HitChance hitChance = HitChance::High) {
        float count = 0.0f;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ChanceValue(GetPrediction(enemy).Hitchance) >= ChanceValue(hitChance)) {
                count += 1.0f;
            }
        }
        return count;
    }

    Utils::FarmLocation GetLineFarmLocation(const std::vector<AIBaseClient>& minions,
                                            float overrideWidth = -1.0f) const {
        const auto positions = Utils::Minion::GetMinionsPredictedPositions(
            minions,
            Delay,
            Width,
            Speed,
            SourcePosition(),
            CurrentRange(),
            false,
            SkillshotType::SkillshotLine);
        return GetLineFarmLocation(positions, overrideWidth >= 0.0f ? overrideWidth : Width);
    }

    Utils::FarmLocation GetLineFarmLocation(const std::vector<Vector2>& minionPositions,
                                            float overrideWidth = -1.0f) const {
        return Utils::Minion::GetBestLineFarmLocation(
            minionPositions,
            overrideWidth >= 0.0f ? overrideWidth : Width,
            CurrentRange());
    }

    PredictionOutput GetPrediction(const AIBaseClient& unit,
                                   bool aoe = false,
                                   float overrideRange = -1.0f,
                                   CollisionableObjects /*collisionable*/ =
                                       CollisionableObjects::Heroes | CollisionableObjects::Minions) const {
        // TODO(SDK parity): replace current-position fallback with
        // Movement.GetPrediction once NightSharp prediction is ported.
        PredictionOutput output;
        output.UnitPosition = unit.Position();
        output.CastPosition = unit.Position();
        output.AoeTargetsHitCount = aoe ? 1 : 0;

        const float range = overrideRange > 0.0f ? overrideRange : CurrentRange();
        output.Hitchance = RangeCheckSource().DistanceSqr2D(output.CastPosition) <= range * range
            ? HitChance::High
            : HitChance::OutOfRange;
        return output;
    }

    AIHeroClient GetTarget(float extraRange = 0.0f,
                           bool accountForCollision = false,
                           const std::vector<AIHeroClient>& champsToIgnore = {}) const {
        // TODO(SDK parity): replace nearest-target fallback with
        // Variables.TargetSelector.GetTarget/GetTargetNoCollision.
        (void)accountForCollision;

        AIHeroClient best;
        float bestDistance = FLT_MAX;
        const float range = CurrentRange() + extraRange;
        const Vector3 source = SourcePosition();
        for (const auto& hero : GameObjects::EnemyHeroes()) {
            if (std::find_if(champsToIgnore.begin(), champsToIgnore.end(), [&](const AIHeroClient& ignored) {
                    return ignored.Compare(hero);
                }) != champsToIgnore.end()) {
                continue;
            }
            if (!Extensions::IsValidTarget(hero, range, true, source)) {
                continue;
            }

            const float distance = source.DistanceSqr2D(hero.Position());
            if (distance < bestDistance) {
                bestDistance = distance;
                best = hero;
            }
        }
        return best;
    }

    std::vector<AIBaseClient> GetUnitsByHitChance(HitChance minimumHitChance = HitChance::High) {
        std::vector<AIBaseClient> result;
        for (const auto& unit : GameObjects::Enemy()) {
            if (WillHit(unit, GameObjects::Player().Position(), 0, minimumHitChance)) {
                result.push_back(unit);
            }
        }
        return result;
    }

    bool IsInRange(const GameObject& obj, float otherRange = -1.0f) const {
        return IsInRange(obj.Position().To2D(), otherRange);
    }

    bool IsInRange(const Vector3& point, float otherRange = -1.0f) const {
        return IsInRange(point.To2D(), otherRange);
    }

    bool IsInRange(const Vector2& point, float otherRange = -1.0f) const {
        const float range = otherRange < 0.0f ? CurrentRange() : otherRange;
        return RangeCheckSource().To2D().DistanceSqr(point) < range * range;
    }

    bool IsReady(int t = 0) const {
        return Extensions::IsReady(Slot, t);
    }

    Spell& SetCharged(const std::string& spellName,
                      const std::string& buffName,
                      int minRange,
                      int maxRange,
                      float deltaT) {
        IsChargedSpell = true;
        ChargedSpellName = spellName;
        ChargedBuffName = buffName;
        ChargedMinRange = minRange;
        ChargedMaxRange = maxRange;
        ChargeDuration = static_cast<int>(deltaT * 1000.0f);
        chargedCastedT_ = 0;

        // TODO(SDK parity): EventList is function-pointer based, so per-spell
        // member subscriptions for OnDoCast/OnUpdateChargeableSpell need a
        // lightweight instance registry before charged-spell suppression can
        // match EnsoulSharp exactly.
        return *this;
    }

    void SetMinimumManaPercentage(float percentage) {
        minManaPercent_ = percentage;
    }

    Spell& SetSkillshot(float delay,
                        float skillWidth,
                        float speed,
                        bool collision,
                        SkillshotType type,
                        Vector3 fromVector3 = {},
                        Vector3 rangeCheckFromVector3 = {}) {
        Delay = delay;
        Width = skillWidth;
        Speed = speed;
        From = fromVector3;
        Collision = collision;
        Type = type;
        RangeCheckFrom = rangeCheckFromVector3;
        IsSkillshot = true;
        return *this;
    }

    Spell& SetSkillshot(bool collision,
                        SkillshotType type,
                        Vector3 fromVector3 = {},
                        Vector3 rangeCheckFromVector3 = {}) {
        From = fromVector3;
        Collision = collision;
        Type = type;
        RangeCheckFrom = rangeCheckFromVector3;
        IsSkillshot = true;
        return *this;
    }

    Spell& SetTargetted(float delay,
                        float speed,
                        Vector3 fromVector3 = {},
                        Vector3 rangeCheckFromVector3 = {}) {
        Delay = delay;
        Speed = speed;
        From = fromVector3;
        RangeCheckFrom = rangeCheckFromVector3;
        IsSkillshot = false;
        return *this;
    }

    Spell& SetTargetted(Vector3 fromVector3 = {}, Vector3 rangeCheckFromVector3 = {}) {
        From = fromVector3;
        RangeCheckFrom = rangeCheckFromVector3;
        IsSkillshot = false;
        return *this;
    }

    void StartCharging() {
        if (IsCharging() || Variables::TickCount() - chargedReqSentT_ <= 400 + Game::Ping()) {
            return;
        }

        GameObjects::Player().Spellbook().CastSpell(Slot);
        chargedReqSentT_ = Variables::TickCount();
    }

    void StartCharging(const Vector3& position) {
        if (IsCharging() || Variables::TickCount() - chargedReqSentT_ <= 400 + Game::Ping()) {
            return;
        }

        GameObjects::Player().Spellbook().CastSpell(Slot, position);
        chargedReqSentT_ = Variables::TickCount();
    }

    void UpdateSourcePosition(Vector3 fromVector3 = {}, Vector3 rangeCheckFromVector3 = {}) {
        From = fromVector3;
        RangeCheckFrom = rangeCheckFromVector3;
    }

    bool WillHit(const AIBaseClient& unit,
                 const Vector3& castPosition,
                 int extraWidth = 0,
                 HitChance minHitChance = HitChance::High) {
        const auto prediction = GetPrediction(unit);
        return ChanceValue(prediction.Hitchance) >= ChanceValue(minHitChance) &&
               WillHit(prediction.UnitPosition, castPosition, extraWidth);
    }

    bool WillHit(const Vector3& point, const Vector3& castPosition, int extraWidth = 0) const {
        switch (Type) {
        case SkillshotType::SkillshotCircle:
            return point.DistanceSqr2D(castPosition) < WidthSqr();
        case SkillshotType::SkillshotLine:
            return DistancePointSegmentSqr(
                       point.To2D(),
                       SourcePosition().To2D(),
                       castPosition.To2D()) < std::pow(Width + static_cast<float>(extraWidth), 2.0f);
        case SkillshotType::SkillshotCone: {
            const Vector2 edge1 = Rotate2D((castPosition - SourcePosition()).To2D(), -Width / 2.0f);
            const Vector2 edge2 = Rotate2D(edge1, Width);
            const Vector2 v = (point - SourcePosition()).To2D();
            return point.DistanceSqr2D(SourcePosition()) < RangeSqr() &&
                   edge1.Cross(v) > 0.0f &&
                   v.Cross(edge2) > 0.0f;
        }
        default:
            return false;
        }
    }

private:
    static void ShootChargedSpell(SpellSlot slot, Vector3 position, bool releaseCast = true) {
        // TODO(SDK parity): set position.y via NavMesh.GetHeightForPosition
        // after the NavMesh wrapper exposes the exact C# API.
        auto spellbook = GameObjects::Player().Spellbook();
        spellbook.UpdateChargedSpell(slot, position, releaseCast, false);
        spellbook.CastSpell(slot, position, false);
    }

    static Vector2 Rotate2D(const Vector2& value, float angle) {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        return { value.x * c - value.y * s, value.x * s + value.y * c };
    }

    static int ChanceValue(HitChance value) {
        return static_cast<int>(value);
    }

    static float DistancePointSegmentSqr(const Vector2& point, const Vector2& start, const Vector2& end) {
        const Vector2 segment = end - start;
        const float lengthSqr = segment.LengthSqr();
        if (lengthSqr <= 1e-6f) {
            return point.DistanceSqr(start);
        }

        const float t = std::clamp((point - start).Dot(segment) / lengthSqr, 0.0f, 1.0f);
        const Vector2 projection = start + segment * t;
        return point.DistanceSqr(projection);
    }

    int chargedCastedT_ = 0;
    int chargedReqSentT_ = 0;
    float minManaPercent_ = 0.0f;
};

} // namespace SDK
