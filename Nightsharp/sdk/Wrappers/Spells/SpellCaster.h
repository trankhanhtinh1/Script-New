#pragma once
#include "GameObject.h"
#include "SpellBook.h"
#include "GameObjects.h"
#include "Prediction.h"
#include "Game.h"
#include "Enums.h"
#include "sdk/Wrappers/Orbwalking/HealthPrediction.h"
#include "sdk/Utils/Bypass.h"
#include "spoof/spoofcall.h"
#include <Psapi.h>
#include <array>
#include <string>

// ============================================================================
// SpellCaster — High-level spell casting with prediction
// Reference: EnsoulSharp.SDK/Core/Wrappers/Spells/Spell.cs
// ============================================================================

namespace SDK {

    // Spell caster type — simplified version for SpellCaster factory
    // (SpellType in Enums.h is the comprehensive EnsoulSharp version)
    enum class SpellCasterType {
        Targeted,
        Line,
        Circle,
        Cone,
        None
    };

    class SpellCaster {
    public:
        // Spell properties
        SpellSlotId Slot;
        SpellCasterType Type;
        float Range;
        float Speed;            // 0 = instant
        float Delay;            // Cast delay (seconds)
        float Width;            // Skillshot width
        int Collision;          // CollisionFlags
        bool IsCharged;
        float ChargeTime;
        float MinRange;

        // EnsoulSharp-compatible: additional properties
        Vec3 From;                  // Custom origin position (default: player pos)
        Vec3 RangeCheckFrom;        // Custom range-check origin (default: player pos)
        HitChance MinHitChance;     // Minimum HitChance for auto CastWithPrediction
        bool Aoe;                   // AoE spell flag (for cluster prediction)
        float RangeSqr;             // Cached Range^2 for fast checks
        float WidthSqr;             // Cached Width^2 for fast checks

        // Charged spell state
        float ChargeStartTime;      // When charge started (game time)
        bool  IsCharging_;          // Currently charging?

        // EnsoulSharp-compatible: additional
        float MinManaPercent;       // Min mana % to cast (0 = disabled)
        std::function<bool()> CastCondition; // Custom condition delegate

        // Constructors
        SpellCaster()
            : Slot(SpellSlotId::Q), Type(SpellCasterType::Targeted), Range(0),
              Speed(0), Delay(0.25f), Width(0), Collision(CollisionNone),
              IsCharged(false), ChargeTime(0), MinRange(0),
              MinHitChance(HitChance::High), Aoe(false), RangeSqr(0), WidthSqr(0),
              ChargeStartTime(0), IsCharging_(false), MinManaPercent(0) {}

        SpellCaster(SpellSlotId slot, float range)
            : Slot(slot), Type(SpellCasterType::Targeted), Range(range),
              Speed(0), Delay(0.25f), Width(0), Collision(CollisionNone),
              IsCharged(false), ChargeTime(0), MinRange(0),
              MinHitChance(HitChance::High), Aoe(false),
              RangeSqr(range * range), WidthSqr(0),
              ChargeStartTime(0), IsCharging_(false), MinManaPercent(0) {}

        // Constructor from game data (EnsoulSharp: Spell(slot, loadFromGame))
        SpellCaster(SpellSlotId slot, bool loadFromGame, HitChance hitChance = HitChance::Medium)
            : Slot(slot), Type(SpellCasterType::Targeted), Range(0),
              Speed(0), Delay(0.25f), Width(0), Collision(CollisionNone),
              IsCharged(false), ChargeTime(0), MinRange(0),
              MinHitChance(hitChance), Aoe(false), RangeSqr(0), WidthSqr(0),
              ChargeStartTime(0), IsCharging_(false), MinManaPercent(0)
        {
            if (loadFromGame && GameObjects::Player.IsValid()) {
                SpellBook sb(GameObjects::Player.address);
                auto spell = sb.GetSpell(slot);
                if (spell.IsValid()) {
                    auto sdata = spell.GetSpellInfo().GetSpellData();
                    Range = sdata.GetCastRange();
                    float lineWidth = sdata.GetLineWidth();
                    Width = (lineWidth > 0.0f) ? lineWidth : 0.0f;
                    Speed = sdata.GetMissileSpeed();
                    RangeSqr = Range * Range;
                    WidthSqr = Width * Width;
                }
            }
        }

        // ====================================================================
        // Factory Methods
        // ====================================================================

        static SpellCaster Targeted(SpellSlotId slot, float range) {
            SpellCaster s;
            s.Slot = slot; s.Range = range; s.Type = SpellCasterType::Targeted;
            return s;
        }

        static SpellCaster Line(SpellSlotId slot, float range, float speed,
                                float width, float delay = 0.25f) {
            SpellCaster s;
            s.Slot = slot; s.Type = SpellCasterType::Line;
            s.Range = range; s.Speed = speed; s.Width = width; s.Delay = delay;
            return s;
        }

        static SpellCaster Circle(SpellSlotId slot, float range, float radius,
                                  float speed = 0, float delay = 0.25f) {
            SpellCaster s;
            s.Slot = slot; s.Type = SpellCasterType::Circle;
            s.Range = range; s.Speed = speed; s.Width = radius * 2; s.Delay = delay;
            return s;
        }

        static SpellCaster Cone(SpellSlotId slot, float range, float angle,
                                float delay = 0.25f) {
            SpellCaster s;
            s.Slot = slot; s.Type = SpellCasterType::Cone;
            s.Range = range; s.Width = angle; s.Delay = delay;
            return s;
        }

        // ====================================================================
        // Builder (fluent API — EnsoulSharp Spell.cs compatible)
        // ====================================================================
        SpellCaster& SetCollision(int flags) { Collision = flags; return *this; }
        SpellCaster& SetCharged(float time, float minR) {
            IsCharged = true; ChargeTime = time; MinRange = minR; return *this;
        }
        SpellCaster& SetFrom(const Vec3& pos) { From = pos; return *this; }
        SpellCaster& SetRangeCheckFrom(const Vec3& pos) { RangeCheckFrom = pos; return *this; }
        SpellCaster& SetMinHitChance(HitChance hc) { MinHitChance = hc; return *this; }
        SpellCaster& SetAoe(bool aoe = true) { Aoe = aoe; return *this; }

        // SetSkillshot — configure all skillshot params at once (EnsoulSharp: Spell.SetSkillshot)
        SpellCaster& SetSkillshot(float delay, float width, float speed,
                                  int collision = CollisionNone, SpellCasterType type = SpellCasterType::Line) {
            Delay = delay; Width = width; Speed = speed; Collision = collision; Type = type;
            WidthSqr = width * width;
            return *this;
        }

        // SetSkillshot (overload — collision + type only, delay/width/speed from game data)
        // Reference: EnsoulSharp Spell.SetSkillshot(bool, SkillshotType, ...)
        SpellCaster& SetSkillshot(int collision, SpellCasterType type) {
            Collision = collision; Type = type;
            return *this;
        }

        // SetTargetted — configure as targeted spell (EnsoulSharp: Spell.SetTargetted)
        SpellCaster& SetTargetted(float delay = 0.0f, float speed = 0.0f,
                                  const Vec3& fromPos = Vec3(), const Vec3& rangeCheckPos = Vec3()) {
            Delay = delay; Speed = speed; Type = SpellCasterType::Targeted;
            if (!fromPos.IsZero()) From = fromPos;
            if (!rangeCheckPos.IsZero()) RangeCheckFrom = rangeCheckPos;
            return *this;
        }

        // SetMinimumManaPercentage — minimum mana % required to cast
        // Reference: EnsoulSharp Spell.SetMinimumManaPercentage
        SpellCaster& SetMinimumManaPercentage(float percent) { MinManaPercent = percent; return *this; }

        // CastCondition — custom delegate that must return true before casting
        // Reference: EnsoulSharp Spell.CastCondition
        SpellCaster& SetCastCondition(std::function<bool()> fn) { CastCondition = fn; return *this; }

        // UpdateSourcePosition — update From and RangeCheckFrom each frame
        void UpdateSourcePosition(const Vec3& from = Vec3(), const Vec3& rangeCheck = Vec3()) {
            if (!from.IsZero()) From = from;
            if (!rangeCheck.IsZero()) RangeCheckFrom = rangeCheck;
        }

        // ====================================================================
        // State
        // ====================================================================

        bool IsReady() const {
            SpellBook sb(GameObjects::Player.address);
            return sb.IsReady(Slot);
        }

        int GetLevel() const {
            SpellBook sb(GameObjects::Player.address);
            return sb.GetSpell(Slot).GetLevel();
        }

        float GetRemainingCD() const {
            SpellBook sb(GameObjects::Player.address);
            return sb.GetSpell(Slot).GetRemainingCooldown();
        }

        bool IsSkillshot() const {
            return Type != SpellCasterType::Targeted && Type != SpellCasterType::None;
        }

        bool InRange(const GameObject& target) const {
            Vec3 fromPos = RangeCheckFrom.IsZero() ?
                           (From.IsZero() ? GameObjects::Player.GetPosition() : From) :
                           RangeCheckFrom;
            return target.GetPosition().Distance2D(fromPos) <= Range + target.GetBoundingRadius();
        }

        // Get effective source position
        Vec3 GetSourcePosition() const {
            if (!From.IsZero()) return From;
            return GameObjects::Player.GetPosition();
        }

        // ====================================================================
        // Charged Spell Support (EnsoulSharp: Spell.IsCharging, Spell.Range)
        // ====================================================================

        /// Start charging the spell
        void StartCharging() {
            if (!IsCharged) return;
            IsCharging_ = true;
            ChargeStartTime = Game::GetTime();
        }

        /// Stop charging
        void StopCharging() { IsCharging_ = false; }

        /// Is currently charging?
        bool IsCurrentlyCharging() const { return IsCharging_ && IsCharged; }

        /// Get charge percent (0.0 - 1.0)
        float GetChargePercent() const {
            if (!IsCharged || !IsCharging_) return 0.0f;
            float elapsed = Game::GetTime() - ChargeStartTime;
            if (elapsed <= 0.0f) return 0.0f;
            if (ChargeTime <= 0.0f) return 1.0f;
            return std::min(1.0f, elapsed / ChargeTime);
        }

        /// Get current charged range (interpolated between MinRange and Range)
        float GetChargedRange() const {
            if (!IsCharged) return Range;
            float pct = GetChargePercent();
            return MinRange + (Range - MinRange) * pct;
        }

        // ====================================================================
        // Damage Integration (EnsoulSharp: Spell.GetSpellDamage)
        // ====================================================================

        /// Get spell damage to target (uses DamageLibrary if available)
        float GetSpellDamage(const GameObject& target, int stage = 0) const {
            // DamageLibrary is declared in DamageLibrary.h — use callback
            if (s_getDamageFunc)
                return s_getDamageFunc(GameObjects::Player, target, Slot, stage);
            return 0.0f;
        }

        // EnsoulSharp-compatible alias (Spell.GetDamage)
        float GetDamage(const GameObject& target, int stage = 0) const {
            return GetSpellDamage(target, stage);
        }

        /// Set damage calculation callback (called from main.cpp after DamageLibrary::Init)
        static void SetDamageCallback(
            std::function<float(const GameObject&, const GameObject&, SpellSlotId, int)> fn) {
            s_getDamageFunc = fn;
        }

        // ====================================================================
        // AoE helpers (EnsoulSharp: Spell.CountHitsInArea)
        // ====================================================================

        /// Count enemy heroes that would be hit by this spell at a position
        int CountHitsInArea(const Vec3& position, float overrideWidth = 0.0f) const {
            float effectiveWidth = overrideWidth > 0 ? overrideWidth : Width;
            int count = 0;
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
                float dist = hero.GetPosition().Distance2D(position);
                if (dist <= effectiveWidth + hero.GetBoundingRadius()) count++;
            }
            return count;
        }

        /// Count enemy minions in area
        int CountMinionsInArea(const Vec3& position, float overrideWidth = 0.0f) const {
            float effectiveWidth = overrideWidth > 0 ? overrideWidth : Width;
            int count = 0;
            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsValid() || !minion.IsAlive()) continue;
                float dist = minion.GetPosition().Distance2D(position);
                if (dist <= effectiveWidth + minion.GetBoundingRadius()) count++;
            }
            return count;
        }

        /// Check if will hit at least N targets (EnsoulSharp: IsInRange + count)
        bool WillHit(const Vec3& position, int minHits = 1) const {
            return CountHitsInArea(position) >= minHits;
        }

        // ====================================================================
        // CanCast / CanKill (EnsoulSharp: Spell.CanCast, Spell.CanKill)
        // ====================================================================

        /// Returns if spell is ready and target is in range
        bool CanCast(const GameObject& target) const {
            return IsReady() && target.IsValid() && target.IsAlive() && InRange(target);
        }

        /// Returns if spell can kill the target (damage > health)
        bool CanKill(const GameObject& target, int stage = 0) const {
            return target.IsValid() && target.IsAlive() && GetSpellDamage(target, stage) > target.GetHealth();
        }

        // ====================================================================
        // GetHitCount — Count enemies that would be hit with high hitchance
        // Reference: EnsoulSharp Spell.GetHitCount
        // ====================================================================
        int GetHitCount(HitChance minHC = HitChance::High) const {
            int count = 0;
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
                if (!InRange(hero)) continue;
                PredictionInput input = BuildPredictionInput();
                auto pred = Prediction::GetPrediction(hero, input);
                if ((int)pred.Hitchance >= (int)minHC) count++;
            }
            return count;
        }

        // ====================================================================
        // GetUnitsByHitChance — Get all enemies that can be hit
        // Reference: EnsoulSharp Spell.GetUnitsByHitChance
        // ====================================================================
        std::vector<GameObject> GetUnitsByHitChance(HitChance minHC = HitChance::High) const {
            std::vector<GameObject> result;
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
                if (!InRange(hero)) continue;
                PredictionInput input = BuildPredictionInput();
                auto pred = Prediction::GetPrediction(hero, input);
                if ((int)pred.Hitchance >= (int)minHC)
                    result.push_back(hero);
            }
            return result;
        }

        // ====================================================================
        // FarmLocation — Find best position to hit most minions
        // Reference: EnsoulSharp Spell.GetCircularFarmLocation / GetLineFarmLocation
        // ====================================================================

        struct FarmLocation {
            Vec3 Position;
            int MinionsHit = 0;
        };

        /// Get best circular farm location (for circle/AoE spells)
        FarmLocation GetCircularFarmLocation(float overrideWidth = -1) const {
            float effectiveWidth = overrideWidth >= 0 ? overrideWidth : Width;
            FarmLocation best;

            // Collect enemy minion positions in range
            std::vector<Vec3> minionPos;
            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsValid() || !minion.IsAlive()) continue;
                Vec3 pos = minion.GetPosition();
                if (pos.Distance2D(GetSourcePosition()) <= Range + effectiveWidth)
                    minionPos.push_back(pos);
            }
            if (minionPos.empty()) return best;

            // Test each minion position as center
            for (auto& center : minionPos) {
                int hits = 0;
                for (auto& pos : minionPos) {
                    if (center.Distance2D(pos) <= effectiveWidth)
                        hits++;
                }
                if (hits > best.MinionsHit) {
                    best.MinionsHit = hits;
                    best.Position = center;
                }
            }
            return best;
        }

        /// Get best line farm location (for line spells)
        FarmLocation GetLineFarmLocation(float overrideWidth = -1) const {
            float effectiveWidth = overrideWidth >= 0 ? overrideWidth : Width;
            FarmLocation best;
            Vec3 from = GetSourcePosition();

            // Collect enemy minion positions in range
            std::vector<Vec3> minionPos;
            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsValid() || !minion.IsAlive()) continue;
                Vec3 pos = minion.GetPosition();
                if (pos.Distance2D(from) <= Range + 200.0f)
                    minionPos.push_back(pos);
            }
            if (minionPos.empty()) return best;

            // For each minion, cast line towards it and count hits
            for (auto& target : minionPos) {
                Vec2 dir2d = (target.To2D() - from.To2D());
                float len = dir2d.Length();
                if (len <= 0) continue;
                dir2d = Vec2(dir2d.x / len, dir2d.y / len);

                Vec3 endPos = Vec3(from.x + dir2d.x * Range, 0, from.z + dir2d.y * Range);

                int hits = 0;
                for (auto& pos : minionPos) {
                    // Distance from point to line segment
                    Vec2 ap = pos.To2D() - from.To2D();
                    Vec2 ab = endPos.To2D() - from.To2D();
                    float abLenSq = ab.x * ab.x + ab.y * ab.y;
                    if (abLenSq <= 0) continue;
                    float t = std::clamp((ap.x * ab.x + ap.y * ab.y) / abLenSq, 0.0f, 1.0f);
                    Vec2 closest(from.x + ab.x * t, from.z + ab.y * t);
                    float dist = (pos.To2D() - closest).Length();
                    if (dist <= effectiveWidth / 2.0f + 50.0f) // +50 for minion radius
                        hits++;
                }
                if (hits > best.MinionsHit) {
                    best.MinionsHit = hits;
                    best.Position = endPos;
                }
            }
            return best;
        }

        // ====================================================================
        // Health Prediction helpers (EnsoulSharp: Spell.GetHealthPrediction)
        // ====================================================================

        /// Get predicted health of target after spell hits
        float GetHealthPrediction(const GameObject& target) const {
            if (!target.IsValid() || !target.IsAlive()) {
                return 0.0f;
            }

            const float travelTimeSec = GetTravelTime(target);
            const float delayMs = (travelTimeSec > 0.0f) ? (travelTimeSec * 1000.0f) : 0.0f;
            return HealthPrediction::GetPrediction(target, delayMs);
        }

        // ====================================================================
        // Convenience: CastIfHitchanceAbove
        // ====================================================================

        /// Cast with the spell's configured MinHitChance
        bool CastOnBestTarget(float overrideRange = 0.0f) {
            float r = overrideRange > 0 ? overrideRange : Range;
            auto target = GetBestTarget(r);
            if (!target.IsValid()) return false;
            return Cast(target);
        }

        /// Get best target in range (uses TargetSelector)
        GameObject GetBestTarget(float overrideRange = 0.0f) const;  // Defined below class

        // ====================================================================
        // Cast Methods
        // All Cast methods check: IsReady, MinManaPercent, CastCondition
        // Reference: EnsoulSharp Spell.cs Cast() checks
        // ====================================================================

    private:
        /// Internal pre-cast checks (mana, condition, ready)
        bool PreCastCheck() const {
            if (!IsReady()) return false;
            if (IsCastThrottled()) return false;
            // MinManaPercent check
            if (MinManaPercent > 0.0f) {
                float maxMana = GameObjects::Player.GetMaxMana();
                if (maxMana > 0.0f) {
                    float manaPercent = (GameObjects::Player.GetMana() / maxMana) * 100.0f;
                    if (manaPercent < MinManaPercent) return false;
                }
            }
            // CastCondition delegate
            if (CastCondition && !CastCondition()) return false;
            return true;
        }

    public:
        // Self cast (no target)
        bool Cast() {
            if (!PreCastCheck()) return false;
            GameObject self = GameObjects::Player;
            CastSpellInternal(self.GetPosition());
            return true;
        }

        // Cast on target (targeted spell)
        bool Cast(const GameObject& target) {
            if (!PreCastCheck() || !target.IsValid()) return false;
            if (!InRange(target)) return false;

            if (IsSkillshot()) {
                return CastWithPrediction(target, MinHitChance);
            }

            // Targeted: cast at target position
            CastSpellInternal(target.GetPosition());
            return true;
        }

        // Cast at position
        bool Cast(const Vec3& pos) {
            if (!PreCastCheck()) return false;
            CastSpellInternal(pos);
            return true;
        }

        // Cast from one position to another (EnsoulSharp: Spell.Cast(from, to))
        bool Cast(const Vec3& fromPos, const Vec3& toPos) {
            if (!PreCastCheck()) return false;
            // Temporarily override From position for this cast
            Vec3 savedFrom = From;
            From = fromPos;
            CastSpellInternal(toPos);
            From = savedFrom;
            return true;
        }

        // Cast with prediction + hit chance requirement
        bool CastWithPrediction(const GameObject& target,
                                HitChance minChance = HitChance::High) {
            if (!PreCastCheck() || !target.IsValid()) return false;

            PredictionInput input = BuildPredictionInput();

            auto pred = Prediction::GetPrediction(target, input);

            // Use parameter or configured MinHitChance (whichever is higher)
            HitChance effectiveMinHC = ((int)minChance > (int)MinHitChance) ? minChance : MinHitChance;
            if ((int)pred.Hitchance < (int)effectiveMinHC)
                return false;

            CastSpellInternal(pred.CastPosition);
            return true;
        }

        // EnsoulSharp-compatible helper: require exact hitchance match
        bool CastIfHitchanceEquals(const GameObject& target, HitChance hitChance) {
            if (!PreCastCheck() || !target.IsValid()) return false;

            PredictionInput input = BuildPredictionInput();
            auto pred = Prediction::GetPrediction(target, input);
            if (pred.Hitchance != hitChance) return false;

            CastSpellInternal(pred.CastPosition);
            return true;
        }

        // EnsoulSharp-compatible helper: require hitchance >= threshold
        bool CastIfHitchanceMinimum(const GameObject& target, HitChance hitChance) {
            return CastWithPrediction(target, hitChance);
        }

        // Cast AoE with cluster prediction (EnsoulSharp: CastIfWillHit)
        bool CastIfWillHit(const GameObject& mainTarget, int minTargets = 2,
                           HitChance minChance = HitChance::High) {
            if (!PreCastCheck() || !mainTarget.IsValid() || !Aoe) return false;

            PredictionInput input = BuildPredictionInput();

            auto pred = Prediction::GetPrediction(mainTarget, input);
            if ((int)pred.Hitchance < (int)minChance) return false;

            // Count how many enemies are near the cast position
            int hits = CountHitsInArea(pred.CastPosition);
            if (hits < minTargets) return false;

            CastSpellInternal(pred.CastPosition);
            return true;
        }

        // ====================================================================
        // Prediction helpers
        // ====================================================================

        /// Build prediction input from spell properties
        PredictionInput BuildPredictionInput() const {
            PredictionInput input;
            input.Range = IsCharged ? GetChargedRange() : Range;
            input.Speed = Speed;
            input.Delay = Delay;
            input.Width = Width;
            input.Aoe = Aoe;
            if (!From.IsZero()) input.From = From;
            if (!RangeCheckFrom.IsZero()) input.RangeCheckFrom = RangeCheckFrom;
            switch (Type) {
            case SpellCasterType::Line:   input.Type = SkillshotType::Line; break;
            case SpellCasterType::Circle: input.Type = SkillshotType::Circle; break;
            case SpellCasterType::Cone:   input.Type = SkillshotType::Cone; break;
            default: break;
            }
            // Set collision flags
            input.CollisionFlags = Collision;
            input.CollisionCheck = (Collision != 0);
            return input;
        }

        PredictionResult GetPrediction(const GameObject& target) const {
            return Prediction::GetPrediction(target, BuildPredictionInput());
        }

        // EnsoulSharp-style overload:
        // GetPrediction(target, aoe, overrideRange, collisionFlags)
        PredictionResult GetPrediction(const GameObject& target,
                                       bool aoe,
                                       float overrideRange,
                                       int collisionFlags) const {
            PredictionInput input = BuildPredictionInput();
            input.Aoe = aoe;
            if (overrideRange > 0.0f) {
                input.Range = overrideRange;
            }
            if (collisionFlags >= 0) {
                input.CollisionFlags = collisionFlags;
                input.CollisionCheck = (collisionFlags != CollisionNone);
            }
            return Prediction::GetPrediction(target, input);
        }

        // Get travel time to target
        float GetTravelTime(const GameObject& target) const {
            Vec3 from = GetSourcePosition();
            float dist = target.GetPosition().Distance2D(from);
            if (Speed <= 0) return Delay;
            return Delay + dist / Speed;
        }

        // ====================================================================
        // Cast at mouse position (convenience)
        // ====================================================================
        bool CastAtMouse() {
            LastCastTime = Game::GetTime();
            if (!IsReady()) {
                LastCastResult = -10; LastCastError = "Spell not ready";
                return false;
            }
            Vec3 mousePos = Game::GetMouseWorldPos();
            if (mousePos.IsZero()) {
                LastCastResult = -11; LastCastError = "Mouse pos zero";
                return false;
            }
            CastSpellInternal(mousePos);
            return true;
        }

        // ====================================================================
        // Cast via keypress simulation (Method 3 - most reliable for testing)
        // Reference: leagueoflegends-master guide castspell.md Method 3
        // Writes target to HUD mouse → simulates key press
        // ====================================================================
        bool CastAtMouseViaKey() {
            LastCastTime = Game::GetTime();
            if (!IsReady()) {
                LastCastResult = -10; LastCastError = "Spell not ready";
                return false;
            }

            Vec3 mousePos = Game::GetMouseWorldPos();
            if (mousePos.IsZero()) {
                LastCastResult = -11; LastCastError = "Mouse pos zero";
                return false;
            }

            // Get key for slot
            BYTE key = 0;
            switch (Slot) {
                case SpellSlotId::Q: key = 'Q'; break;
                case SpellSlotId::W: key = 'W'; break;
                case SpellSlotId::E: key = 'E'; break;
                case SpellSlotId::R: key = 'R'; break;
                default: {
                    LastCastResult = -12; LastCastError = "Invalid slot for key";
                    return false;
                }
            }

            // Find game window
            HWND gameWnd = FindWindowA("RiotWindowClass", nullptr);
            if (!gameWnd) gameWnd = FindWindowA(nullptr, "League of Legends (TM) Client");
            if (!gameWnd) {
                LastCastResult = -13; LastCastError = "Game window not found";
                return false;
            }

            // Simulate keypress → game casts spell at current mouse position
            PostMessageA(gameWnd, WM_KEYDOWN, key, 0);
            PostMessageA(gameWnd, WM_KEYUP, key, 0);

            LastCastResult = 1;
            LastCastError = "OK (KeySim)";
            return true;
        }

        // ====================================================================
        // Debug: Last cast result (for overlay)
        // ====================================================================
        static inline int    LastCastResult  = 0;   // 0=none, 1=success, -1..-9=error
        static inline float  LastCastTime    = 0.0f;
        static inline const char* LastCastError = "";
        static inline std::array<float, 14> s_lastSlotCastAttemptTimes = {};

        // Damage callback (set via SetDamageCallback, avoids circular dependency)
        static inline std::function<float(const GameObject&, const GameObject&, SpellSlotId, int)>
            s_getDamageFunc;

    private:
        static float GetCastThrottleWindow(SpellSlotId slot) {
            switch (slot) {
                case SpellSlotId::Q:
                case SpellSlotId::W:
                case SpellSlotId::E:
                case SpellSlotId::R:
                    return 0.20f;
                default:
                    return 0.10f;
            }
        }

        bool IsCastThrottled() const {
            const int slotIndex = static_cast<int>(Slot);
            if (slotIndex < 0 || slotIndex >= static_cast<int>(s_lastSlotCastAttemptTimes.size())) {
                return false;
            }

            const float now = Game::GetTime();
            if (now <= 0.0f) {
                return false;
            }

            const float lastAttempt = s_lastSlotCastAttemptTimes[slotIndex];
            const float throttleWindow = GetCastThrottleWindow(Slot);
            if (lastAttempt > 0.0f && now - lastAttempt >= 0.0f && now - lastAttempt < throttleWindow) {
                LastCastResult = -14;
                LastCastError = "Spell throttled";
                return true;
            }

            return false;
        }

        void MarkCastAttempt() const {
            const int slotIndex = static_cast<int>(Slot);
            if (slotIndex < 0 || slotIndex >= static_cast<int>(s_lastSlotCastAttemptTimes.size())) {
                return;
            }

            s_lastSlotCastAttemptTimes[slotIndex] = Game::GetTime();
        }

        // ====================================================================
        // Find trampoline gadget for spoof_call (cached, FF 23 = jmp [rbx])
        // ====================================================================
        static void* GetTrampoline() {
            static void* trampoline = nullptr;
            if (!trampoline) {
                MODULEINFO modInfo{};
                GetModuleInformation(GetCurrentProcess(),
                    (HMODULE)GetModuleHandleA(nullptr), &modInfo, sizeof(modInfo));
                char* base = (char*)GetModuleHandleA(nullptr);
                for (size_t i = 0; i < modInfo.SizeOfImage - 2; i++) {
                    if (base[i] == '\xFF' && base[i + 1] == '\x23') {
                        trampoline = base + i;
                        break;
                    }
                }
            }
            return trampoline;
        }

        void CastSpellInternal(const Vec3& pos) {
            MarkCastAttempt();

            void* trampoline = GetTrampoline();
            if (!trampoline) {
                LastCastResult = -1; LastCastError = "No trampoline (FF 23)";
                return;
            }

            auto& player = GameObjects::Player;
            if (!player.IsValid()) {
                LastCastResult = -2; LastCastError = "Player invalid";
                return;
            }

            // Get SpellBook → SpellSlot
            uintptr_t spellBookAddr = player.address + Offset::SpellBook::Offset;
            uintptr_t spellSlotAddr = Globals::Read<uintptr_t>(
                spellBookAddr + Offset::SpellBook::SpellSlotArray + (int)Slot * 8);
            if (!Globals::IsValidPtr(spellSlotAddr)) {
                LastCastResult = -3; LastCastError = "SpellSlot invalid";
                return;
            }

            // Get SpellInfo + SpellInput pointers
            // Confirmed: SpellInfo=0x128 (SlotSpellInfo), SpellInput=0x120 (SlotSpellInput)
            uintptr_t spellInfoPtr = Globals::Read<uintptr_t>(
                spellSlotAddr + Offset::SpellBook::SlotSpellInfo);
            if (!Globals::IsValidPtr(spellInfoPtr)) {
                LastCastResult = -4; LastCastError = "SpellInfo invalid";
                return;
            }

            uintptr_t spellInput = Globals::Read<uintptr_t>(
                spellSlotAddr + Offset::SpellBook::SlotSpellInput);
            if (!Globals::IsValidPtr(spellInput)) {
                LastCastResult = -5; LastCastError = "SpellInput invalid";
                return;
            }

            // Get HudInstance → HudSpellInfo (RCX param for CastSpellSafe)
            uintptr_t hudInstance = Globals::Read<uintptr_t>(
                Globals::base + Offset::Global::HudInstance);
            if (!Globals::IsValidPtr(hudInstance)) {
                LastCastResult = -6; LastCastError = "HudInstance invalid";
                return;
            }

            uintptr_t hudSpellInfo = Globals::Read<uintptr_t>(
                hudInstance + Offset::Hud::SpellInfo);
            if (!Globals::IsValidPtr(hudSpellInfo)) {
                LastCastResult = -7; LastCastError = "HudSpellInfo invalid";
                return;
            }

            Vec3 playerPos = player.GetPosition();

            // Save original SpellInput values (will restore after cast)
            Vec3 origStartPos = Globals::Read<Vec3>(
                spellInput + Offset::SpellBook::InputStartPos);
            Vec3 origEndPos = Globals::Read<Vec3>(
                spellInput + Offset::SpellBook::InputEndPos);
            Vec3 origEndPos2 = Globals::Read<Vec3>(
                spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3));
            Vec3 origEndPos3 = Globals::Read<Vec3>(
                spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2);

            // Write target position to SpellInput
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputStartPos, playerPos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos, pos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3), pos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2, pos);

            // Also write to HUD mouse position (game also reads from here)
            uintptr_t hudInput = Globals::Read<uintptr_t>(hudInstance + Offset::Hud::Input);
            Vec3 origMouse;
            bool savedMouse = false;
            if (Globals::IsValidPtr(hudInput)) {
                origMouse = Globals::Read<Vec3>(hudInput + Offset::Hud::MouseWorldPos);
                Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, pos);
                savedMouse = true;
            }

            // ============================================================
            // Chimera pattern: set CastSpellFlag before the HUD -> packet path.
            // Current patch flows through:
            //   CastSpellSafe -> CastSpellPacketA/B/Charged -> PacketSendCommon
            // ============================================================
            Bypass::PrepareCastSpell();

            // ============================================================
            // Call CastSpellSafe(hudSpellInfo, spellInfoPtr) via spoof_call
            // Reference: leagueoflegends-master/global/functions.cpp line 229
            //   param1 = *(HudInstance + 0x68) = hudSpellInfo
            //   param2 = *(SpellSlot + SlotSpellInfo) = spellInfoPtr
            // ============================================================
            using fnCastSpell = void(__fastcall*)(uintptr_t, uintptr_t);
            fnCastSpell fn = reinterpret_cast<fnCastSpell>(
                Globals::base + Offset::Function::CastSpellSafe);

            bool castCallSucceeded = false;
            __try {
                spoof_call(trampoline, fn, hudSpellInfo, spellInfoPtr);
                castCallSucceeded = true;
                LastCastResult = 1;
                LastCastError = "OK (Chimera/CastSpellSafe)";
            } __except(1) {
                LastCastResult = -8; LastCastError = "CastSpellSafe CRASHED";
            }

            // Chimera leaves flag cleanup to the internal cast pipeline.
            // If we fault before reaching that code path, clear it defensively here.
            if (!castCallSucceeded) {
                Globals::Write<uint8_t>(Globals::base + Offset::Flag::CastSpellFlag, 0);
            }

            // Restore original SpellInput values
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputStartPos, origStartPos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos, origEndPos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3), origEndPos2);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2, origEndPos3);

            // Restore HUD mouse position
            if (savedMouse && Globals::IsValidPtr(hudInput)) {
                Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, origMouse);
            }

            LastCastTime = Game::GetTime();
        }
    };

    // ========================================================================
    // SpellCaster::GetBestTarget — out-of-class definition
    // (Cannot be inline because TargetSelector may not be fully defined yet.
    //  Uses basic enemy hero iteration as fallback.)
    // ========================================================================
    inline GameObject SpellCaster::GetBestTarget(float overrideRange) const {
        float r = overrideRange > 0 ? overrideRange : Range;
        Vec3 fromPos = GetSourcePosition();

        // Find closest valid enemy hero in range (simple fallback)
        GameObject best;
        float bestDist = 99999.0f;
        for (auto& hero : GameObjects::EnemyHeroes) {
            if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
            if (!hero.IsTargetable()) continue;
            float dist = hero.GetPosition().Distance2D(fromPos);
            if (dist > r + hero.GetBoundingRadius()) continue;
            if (dist < bestDist) {
                bestDist = dist;
                best = hero;
            }
        }
        return best;
    }

    // ========================================================================
    // SpellFactory — Pre-defined spells for popular champions
    // ========================================================================
    namespace SpellFactory {
        // Ezreal
        inline SpellCaster EzrealQ() {
            return SpellCaster::Line(SpellSlotId::Q, 1150, 2000, 120, 0.25f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }
        inline SpellCaster EzrealW() {
            return SpellCaster::Line(SpellSlotId::W, 1150, 1700, 160, 0.25f);
        }
        inline SpellCaster EzrealR() {
            return SpellCaster::Line(SpellSlotId::R, 20000, 2000, 320, 1.0f);
        }

        // Lux
        inline SpellCaster LuxQ() {
            return SpellCaster::Line(SpellSlotId::Q, 1175, 1200, 70, 0.25f)
                .SetCollision(CollisionHeroes);
        }
        inline SpellCaster LuxE() {
            return SpellCaster::Circle(SpellSlotId::E, 1100, 310, 1200, 0.25f);
        }
        inline SpellCaster LuxR() {
            return SpellCaster::Line(SpellSlotId::R, 3340, 0, 110, 1.0f);
        }

        // Morgana
        inline SpellCaster MorganaQ() {
            return SpellCaster::Line(SpellSlotId::Q, 1175, 1200, 70, 0.25f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }

        // Jinx
        inline SpellCaster JinxW() {
            return SpellCaster::Line(SpellSlotId::W, 1450, 3300, 60, 0.6f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }
        inline SpellCaster JinxR() {
            return SpellCaster::Line(SpellSlotId::R, 25000, 1700, 140, 0.6f)
                .SetCollision(CollisionHeroes);
        }

        // Blitzcrank
        inline SpellCaster BlitzcrankQ() {
            return SpellCaster::Line(SpellSlotId::Q, 1150, 1800, 70, 0.25f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }

        // Thresh
        inline SpellCaster ThreshQ() {
            return SpellCaster::Line(SpellSlotId::Q, 1100, 1900, 70, 0.5f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }

        // Ahri
        inline SpellCaster AhriE() {
            return SpellCaster::Line(SpellSlotId::E, 975, 1550, 60, 0.25f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }

        // Brand
        inline SpellCaster BrandW() {
            return SpellCaster::Circle(SpellSlotId::W, 900, 250, 0, 0.85f);
        }
    }

} // namespace SDK
