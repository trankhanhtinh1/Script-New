#pragma once

#include "Database/SpellDatabase.h"
#include "../../../Core/CoreEvadeState.h"
#include "../../../Core/CoreNewCastSpell.h"

#include "../../Core/Game.h"
#include "../../Core/Objects.h"
#include "../../Core/Variables.h"
#include "../../Enumerations/CastStates.h"
#include "../../Enumerations/CollisionableObjects.h"
#include "../../Enumerations/DamageType.h"
#include "../../Enumerations/HitChance.h"
#include "../../Enumerations/SkillshotType.h"
#include "../../Events/Events.h"
#include "../../Extensions/Extensions.h"
#include "../../GameObjects/GameObjects.h"
#include "../../Utils/Minion.h"
#include "../../Math/Collision.h"
#include "../../Math/Prediction.h"
#include "../../Math/HealthPrediction.h"
#include "../Damages/Damage.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

namespace SDK {

// PredictionInput and PredictionOutput are now defined in Math/Prediction.h
// (ported from Movement.cs) and included above.

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
    bool AddHitBox = true;
    float MaxCollisionCount = 0.0f;
    bool PredictionCloserPosition = false;
    // Guide default: Minions | Heroes | YasuoWall. YasuoWall expands to Samira's
    // and Mel's walls too, so the three no longer need spelling out here.
    CollisionObjectsBridge CollisionObjects =
        CollisionableObjects::Minions |
        CollisionableObjects::Heroes |
        CollisionableObjects::YasuoWall;
    bool Collision = false;
    DamageType DamageType = DamageType::Physical;
    float Delay = 0.0f;
    Vector3 From = {};
    bool IsChargedSpell = false;
    bool IsSkillshot = false;
    int LastCastAttemptT = 0;
    HitChance MinHitChance = HitChance::Medium;
    mutable float Range = FLT_MAX;
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
        if (!IsChargedSpell) {
            return false;
        }

        // The client's own charge state (castContext + 0x38, which InitChargeState
        // sets and the release clears) is the authority. This used to be driven by
        // the champion buff, but the buff outlives the release by a few frames, so a
        // finished charge kept reporting as charging and the follow-up release then
        // failed against a charge state that was already gone.
        const bool charging =
            CoreNewCastSpell::IsCharging(static_cast<std::int32_t>(Slot));
        SyncChargedRange(charging);
        return charging;
    }

    int Level() const {
        const auto instance = Instance();
        return instance.IsValid() ? instance.Level() : 0;
    }

    float CurrentRange() const {
        if (!IsChargedSpell) {
            return Range;
        }

        const bool charging = IsCharging();
        const float range = ChargedRange(charging);
        Range = range;
        return range;
    }

    float RangeSqr() const {
        const float range = CurrentRange();
        return range * range;
    }

    float WidthSqr() const {
        return Width * Width;
    }

    ~Spell() {
        UnregisterChargedSpell(this);
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

        const bool charging = IsChargedSpell && IsCharging();
        if (!IsReady() && !charging) {
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

        if (!IsSkillshot && !IsChargedSpell) {
            if (RangeCheckSource().DistanceSqr2D(unit.Position()) > RangeSqr()) {
                return CastStates::OutOfRange;
            }

            if (!TryReserveCastRequest()) {
                return CastStates::NotCasted;
            }
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

        if (RangeCheckSource().DistanceSqr2D(prediction.GetCastPosition()) > RangeSqr()) {
            return CastStates::OutOfRange;
        }

        const HitChance needed = tempHitChance == HitChance::None ? MinHitChance : tempHitChance;
        if (ChanceValue(prediction.Hitchance) < ChanceValue(needed) ||
            (exactHitChance && prediction.Hitchance != needed)) {
            return CastStates::LowHitChance;
        }

        if (!TryReserveCastRequest()) {
            return CastStates::NotCasted;
        }
        if (IsChargedSpell) {
            const bool casted = IsCharging()
                ? ShootChargedSpell(prediction.GetCastPosition())
                : StartCharging(prediction.GetCastPosition());
            return casted ? CastStates::SuccessfullyCasted : CastStates::NotCasted;
        } else if (!player.Spellbook().CastSpell(Slot, prediction.GetCastPosition())) {
            return CastStates::NotCasted;
        }

        return CastStates::SuccessfullyCasted;
    }

    bool Cast() {
        const bool charging = IsChargedSpell && IsCharging();
        if (!IsReady() && !charging) {
            return false;
        }

        if (!TryReserveCastRequest()) {
            return false;
        }
        return GameObjects::Player().Spellbook().CastSpell(Slot);
    }

    bool Cast(const Vector2& fromPosition, const Vector2& toPosition) {
        return Cast(Vector3::From2D(fromPosition), Vector3::From2D(toPosition));
    }

    bool Cast(const Vector3& fromPosition, const Vector3& toPosition) {
        if (!IsReady()) {
            return false;
        }

        if (!TryReserveCastRequest()) {
            return false;
        }
        return GameObjects::Player().Spellbook().CastSpell(Slot, fromPosition, toPosition);
    }

    bool Cast(const Vector2& position) {
        return Cast(Vector3::From2D(position));
    }

    bool Cast(const Vector3& position) {
        const bool charging = IsChargedSpell && IsCharging();
        if (!IsReady() && !charging) {
            return false;
        }

        if (!TryReserveCastRequest()) {
            return false;
        }
        if (IsChargedSpell) {
            return IsCharging()
                ? ShootChargedSpell(position)
                : StartCharging(position);
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

        if (!TryReserveCastRequest()) {
            return false;
        }
        return GameObjects::Player().Spellbook().CastSpell(Slot, unit);
    }

    int CountHits(const std::vector<AIBaseClient>& units, const Vector3& castPosition) {
        std::vector<Vector3> points;
        points.reserve(units.size());
        for (const auto& unit : units) {
            points.push_back(GetPrediction(unit).GetUnitPosition());
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

    std::vector<AIBaseClient> GetCollision(const Vector2& fromVector2,
                                           const std::vector<Vector2>& to,
                                           float delayOverride = -1.0f) const {
        std::vector<Vector3> positions;
        positions.reserve(to.size());
        for (const auto& position : to) {
            positions.push_back(Vector3::From2D(position));
        }

        PredictionInput input;
        input.From = Vector3::From2D(fromVector2);
        input.SetType(Type);
        input.Radius = Width;
        input.Delay = delayOverride > 0.0f ? delayOverride : Delay;
        input.Speed = Speed;
        input.Range = CurrentRange();
        input.Collision = Collision;
        input.RangeCheckFrom = RangeCheckFrom;
        input.MaxCollisionCount = MaxCollisionCount;
        input.CollisionObjects = CollisionObjects;

        return Collision::GetCollision(positions, input);
    }

    float GetDamage(const AIBaseClient& target, DamageStage stage = DamageStage::Default) const {
        const auto player = GameObjects::Player();
        if (!player.IsValid() || !target.IsValid()) {
            return 0.0f;
        }
        return Damage::GetSpellDamage(player, target, Slot, stage);
    }

    float GetHealthPrediction(const AIBaseClient& unit) const {
        if (!unit.IsValid()) {
            return 0.0f;
        }
        float time = (Delay * 1000.0f) - 100.0f + static_cast<float>(Game::Ping()) / 2.0f;
        if (std::abs(Speed - FLT_MAX) > FLT_EPSILON && Speed > 1.0f) {
            time += 1000.0f * SourcePosition().Distance2D(unit.Position()) / Speed;
        }
        if (!std::isfinite(time)) {
            time = 0.0f;
        }
        return HealthPrediction::GetPrediction(unit, static_cast<int>(time));
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
                                   float overrideRange = -1.0f) const {
        return GetPrediction(unit, aoe, overrideRange, CollisionObjects);
    }

    PredictionOutput GetPrediction(const AIBaseClient& unit,
                                   bool aoe,
                                   float overrideRange,
                                   CollisionObjectsBridge collisionable) const {
        const float range = overrideRange > 0.0f ? overrideRange : CurrentRange();

        PredictionInput input;
        input.Unit = unit;
        input.Delay = Delay;
        input.Radius = Width;
        input.Speed = Speed;
        input.From = From;
        input.Range = range;
        input.Collision = Collision;
        input.SetType(Type);
        input.Spell = const_cast<Spell*>(this);
        input.RangeCheckFrom = RangeCheckFrom;
        input.AoE = aoe || (Width > 85.0f && !Collision);
        input.AddHitBox = AddHitBox;
        input.ChoiceCloserPosition = PredictionCloserPosition;
        input.MaxCollisionCount = MaxCollisionCount;
        input.CollisionObjects = collisionable;

        return Prediction::GetPrediction(input);
    }

    AIHeroClient GetTarget(float extraRange = 0.0f,
                           bool accountForCollision = false,
                           const std::vector<AIHeroClient>& champsToIgnore = {}) const;

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
        return Extensions::IsReady(Slot, t) ||
               (IsChargedSpell && t == 0 && IsCharging());
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
        chargedReqSentT_ = 0;
        Range = static_cast<float>(maxRange);

        CoreSpellBook::RegisterChargedBuffName(buffName.c_str());
        RegisterChargedSpell(this);
        return *this;
    }

    void SetMinimumManaPercentage(float percentage) {
        minManaPercent_ = percentage;
    }

    Spell& SetCollisionObjects(CollisionableObjects flags) {
        CollisionObjects = flags;
        return *this;
    }

    Spell& SetCollisionObjects(std::initializer_list<CollisionableObjects> objects) {
        CollisionObjects = objects;
        return *this;
    }

    Spell& SetCollisionObjects(const std::vector<CollisionableObjects>& objects) {
        CollisionObjects = objects;
        return *this;
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

    Spell& SetSkillshot(float delay,
                        float skillWidth,
                        float speed,
                        bool collision,
                        SpellType type,
                        Vector3 fromVector3 = {},
                        Vector3 rangeCheckFromVector3 = {}) {
        return SetSkillshot(
            delay,
            skillWidth,
            speed,
            collision,
            ToSkillshotType(type),
            fromVector3,
            rangeCheckFromVector3);
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

    Spell& SetSkillshot(bool collision,
                        SpellType type,
                        Vector3 fromVector3 = {},
                        Vector3 rangeCheckFromVector3 = {}) {
        return SetSkillshot(
            collision,
            ToSkillshotType(type),
            fromVector3,
            rangeCheckFromVector3);
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

    bool StartCharging() {
        return StartCharging(SourcePosition());
    }

    // Parity with EnsoulSharp.SDK.Spell, which offers StartCharging() plus both a
    // Vector2 and a Vector3 overload (ShootChargedSpell already had both).
    bool StartCharging(Vector2 position) {
        return StartCharging(Vector3::From2D(position));
    }

    bool StartCharging(const Vector3& position) {
        if (IsCharging() || Variables::TickCount() - chargedReqSentT_ <= 400 + Game::Ping()) {
            return false;
        }

        const int now = Variables::TickCount();
        const bool result =
            GameObjects::Player().Spellbook().UpdateChargedSpell(Slot, position, false);
        if (result) {
            chargedReqSentT_ = now;
            chargedCastedT_ = now;
            LastCastAttemptT = now;
            SyncChargedRange(true);
        }
        return result;
    }

    bool ShootChargedSpell(Vector2 position) {
        return ShootChargedSpell(Vector3::From2D(position));
    }

    bool ShootChargedSpell(Vector3 position) {
        LastCastAttemptT = Variables::TickCount();
        const bool result =
            GameObjects::Player().Spellbook().UpdateChargedSpell(Slot, position, true);
        if (result) {
            chargedReqSentT_ = 0;
            SyncChargedRange(false);
        }
        return result;
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
               WillHit(prediction.GetUnitPosition(), castPosition, extraWidth);
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
    static constexpr int kCastRequestThrottleMs = 120;

    float ChargedRange(bool charging) const {
        if (!IsChargedSpell) {
            return Range;
        }

        if (!charging) {
            return static_cast<float>(ChargedMaxRange);
        }

        // Prefer the moment the client recorded the charge as opening. The local
        // request ticks only know about charges this wrapper started, so they drift
        // whenever the charge came from somewhere else — the player's own key, say.
        int elapsed = 0;
        const float chargeStart =
            CoreNewCastSpell::ChargeStartTime(static_cast<std::int32_t>(Slot));
        if (chargeStart > 0.0f) {
            elapsed = static_cast<int>((Game::Time() - chargeStart) * 1000.0f);
        } else {
            const int now = Variables::TickCount();
            const int startTick =
                chargedCastedT_ > 0 ? chargedCastedT_ : chargedReqSentT_;
            elapsed = startTick > 0 ? now - startTick : 0;
        }
        elapsed = std::max(0, elapsed);

        const float delta = static_cast<float>(ChargedMaxRange - ChargedMinRange);
        return static_cast<float>(ChargedMinRange) +
               std::min(delta,
                        (static_cast<float>(elapsed) * delta /
                         static_cast<float>(std::max(1, ChargeDuration))) -
                        150.0f);
    }

    void SyncChargedRange(bool charging) const {
        if (IsChargedSpell) {
            Range = ChargedRange(charging);
        }
    }

    static int CastThrottleSlotIndex(SpellSlot slot) {
        const int value = static_cast<int>(slot);
        if (value >= 0 && value <= 13) {
            return value;
        }
        if (slot == SpellSlot::BasicAttack) {
            return 14;
        }
        return 15;
    }

    static int& LastSlotCastRequestTick(SpellSlot slot) {
        static int ticks[16] = {};
        return ticks[CastThrottleSlotIndex(slot)];
    }

    bool TryReserveCastRequest() {
        const int now = Variables::TickCount();
        if (CoreEvadeState::AreSpellCastsBlocked(
                now, static_cast<int>(Slot))) {
            return false;
        }
        if (LastCastAttemptT != 0 && now - LastCastAttemptT < kCastRequestThrottleMs) {
            return false;
        }

        int& slotTick = LastSlotCastRequestTick(Slot);
        if (slotTick != 0 && now - slotTick < kCastRequestThrottleMs) {
            return false;
        }

        LastCastAttemptT = now;
        slotTick = now;
        return true;
    }

    static std::vector<Spell*>& ChargedSpellRegistry() {
        static std::vector<Spell*> registry;
        return registry;
    }

    static void OnDoCastStatic(const Events::ProcessSpellEventArgs& args) {
        for (Spell* spell : ChargedSpellRegistry()) {
            if (spell) {
                spell->OnDoCast(args);
            }
        }
    }

    static void RegisterChargedSpell(Spell* spell) {
        if (!spell) {
            return;
        }

        auto& registry = ChargedSpellRegistry();
        if (std::find(registry.begin(), registry.end(), spell) == registry.end()) {
            registry.push_back(spell);
        }

        Events::AddOnDoCast(&OnDoCastStatic);
    }

    static void UnregisterChargedSpell(Spell* spell) {
        auto& registry = ChargedSpellRegistry();
        registry.erase(std::remove(registry.begin(), registry.end(), spell), registry.end());
        if (registry.empty()) {
            Events::RemoveOnDoCast(&OnDoCastStatic);
        }
    }

    void OnDoCast(const Events::ProcessSpellEventArgs& args) {
        const auto player = GameObjects::Player();
        if (!player.IsValid() ||
            args.Sender.NetworkId != player.NetworkId() ||
            ChargedSpellName.empty() ||
            ChargedSpellName != args.SpellName) {
            return;
        }

        chargedCastedT_ = Variables::TickCount();
        SyncChargedRange(true);
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
