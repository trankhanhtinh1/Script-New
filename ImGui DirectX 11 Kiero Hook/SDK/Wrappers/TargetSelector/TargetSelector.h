#pragma once
#include "GameObject.h"
#include "GameObjects.h"
#include "BuffManager.h"
#include "DamageCalc.h"
#include "Game.h"
#include <vector>
#include <algorithm>
#include <string>
#include <memory>
#include <functional>
#include <cmath>
#include <unordered_map>

#undef min
#undef max

// ============================================================================
// TargetSelector — Select best target for combat
// Reference: EnsoulSharp.SDK/Core/Wrappers/TargetSelector/*
//
// Includes:
//   - Multiple selection modes (LowestHP, Closest, Priority, etc.)
//   - Weight system (EnsoulSharp Weights/*.cs port)
//   - Forced target support
// ============================================================================

namespace SDK {

    // ========================================================================
    // Weight System — Individual weight items for scoring
    // ========================================================================
    // Source: EnsoulSharp.SDK/Core/Wrappers/TargetSelector/Weights/*.cs
    //
    // Each IWeightItem computes a normalized score (0-1) for a target.
    // The final weight = sum of (item.GetWeight * item.DefaultWeight).
    // Higher weight = higher priority target.
    // ========================================================================

    /// Base interface for weight items
    class IWeightItem {
    public:
        std::string Name;
        std::string DisplayName;
        float DefaultWeight;    // Multiplier for this weight (configurable)
        bool Inverted;          // If true, lower raw value = higher priority
        bool Enabled;

        IWeightItem(const std::string& name, const std::string& display,
                    float weight = 1.0f, bool inverted = false)
            : Name(name), DisplayName(display), DefaultWeight(weight),
              Inverted(inverted), Enabled(true) {}

        virtual ~IWeightItem() = default;

        /// Compute raw weight for target (0-1 range normalized)
        virtual float GetWeight(const GameObject& hero) const = 0;
    };

    // ---- Individual Weight Items ----

    /// Attack damage weight — prioritize high AD targets (ADCs)
    /// Source: Weights/AttackDamageWeight.cs
    class AttackDamageWeight : public IWeightItem {
    public:
        AttackDamageWeight() : IWeightItem("ad", "Attack Damage", 1.0f) {}
        float GetWeight(const GameObject& hero) const override {
            return hero.GetTotalAD() / 300.0f; // Normalize: ~300 AD = 1.0
        }
    };

    /// Ability power weight — prioritize high AP targets (Mages)
    /// Source: Weights/AbilityPowerWeight.cs
    class AbilityPowerWeight : public IWeightItem {
    public:
        AbilityPowerWeight() : IWeightItem("ap", "Ability Power", 1.0f) {}
        float GetWeight(const GameObject& hero) const override {
            return hero.GetAP() / 500.0f; // Normalize: ~500 AP = 1.0
        }
    };

    /// Health weight — prioritize LOW health targets (inverted)
    /// Source: Weights/HealthWeight.cs
    class HealthWeight : public IWeightItem {
    public:
        HealthWeight() : IWeightItem("hp", "Health", 1.5f, true) {}
        float GetWeight(const GameObject& hero) const override {
            return hero.GetHealth() / hero.GetMaxHealth(); // Lower HP% = higher weight (inverted)
        }
    };

    /// Distance weight — prioritize CLOSER targets (inverted)
    /// Source: Weights/DistanceWeight.cs
    class DistanceWeight : public IWeightItem {
    public:
        DistanceWeight() : IWeightItem("dist", "Distance", 1.2f, true) {}
        float GetWeight(const GameObject& hero) const override {
            float dist = GameObjects::Player.DistanceTo(hero);
            return std::min(1.0f, dist / 2000.0f); // 2000 range = 1.0
        }
    };

    /// Focus weight — prioritize targets with lowest armor/MR
    /// Source: Weights/ResistWeight.cs
    class ResistWeight : public IWeightItem {
    public:
        ResistWeight() : IWeightItem("resist", "Low Resist", 1.0f, true) {}
        float GetWeight(const GameObject& hero) const override {
            float armor = hero.GetArmor();
            float mr = hero.GetMR();
            float avg = (armor + mr) / 2.0f;
            return std::min(1.0f, avg / 200.0f); // 200 avg = 1.0
        }
    };

    /// Kill pressure — attacks needed to kill (fewer = higher priority)
    /// Source: Weights/KillableWeight.cs
    class KillableWeight : public IWeightItem {
    public:
        KillableWeight() : IWeightItem("killable", "Killable", 2.0f, true) {}
        float GetWeight(const GameObject& hero) const override {
            int aas = DamageCalc::GetAutoAttacksToKill(GameObjects::Player, hero, true);
            return std::min(1.0f, (float)aas / 15.0f); // 15+ AA = 1.0 (tank)
        }
    };

    /// Crowd control weight — prioritize targets currently CC'd
    /// Source: Weights/CrowdControlWeight.cs
    class CrowdControlWeight : public IWeightItem {
    public:
        CrowdControlWeight() : IWeightItem("cc", "CC'd Target", 1.5f) {}
        float GetWeight(const GameObject& hero) const override {
            BuffManager buffs(hero.address);
            float score = 0.0f;
            if (buffs.HasBuffOfType(BuffType::Stun))       score += 0.4f;
            if (buffs.HasBuffOfType(BuffType::Snare))      score += 0.3f;
            if (buffs.HasBuffOfType(BuffType::Suppression)) score += 0.4f;
            if (buffs.HasBuffOfType(BuffType::Taunt))      score += 0.2f;
            if (buffs.HasBuffOfType(BuffType::Charm))      score += 0.3f;
            if (buffs.HasBuffOfType(BuffType::Fear))       score += 0.3f;
            if (buffs.HasBuffOfType(BuffType::Asleep))     score += 0.4f;
            if (buffs.HasBuffOfType(BuffType::Slow))       score += 0.1f;
            return std::min(1.0f, score);
        }
    };

    /// Team focus weight — prioritize targets that allies are attacking
    /// Source: Weights/TeamFocusWeight.cs
    class TeamFocusWeight : public IWeightItem {
    public:
        TeamFocusWeight() : IWeightItem("teamfocus", "Team Focus", 1.0f) {}
        float GetWeight(const GameObject& hero) const override {
            // Count ally heroes near target (within 1000 range)
            int allyCount = 0;
            for (auto& ally : GameObjects::AllyHeroes) {
                if (!ally.IsValid() || !ally.IsAlive()) continue;
                if (ally.address == GameObjects::Player.address) continue;
                float dist = ally.DistanceTo(hero);
                if (dist <= 1000.0f) allyCount++;
            }
            return std::min(1.0f, (float)allyCount / 4.0f);
        }
    };

    /// Gold value weight — prioritize targets with high gold income (high CS/kills)
    /// Source: Weights/GoldWeight.cs (approximated)
    class GoldWeight : public IWeightItem {
    public:
        GoldWeight() : IWeightItem("gold", "Gold Value", 0.8f) {}
        float GetWeight(const GameObject& hero) const override {
            // Approximate gold value from total AD + AP (items indicate gold spent)
            float totalStats = hero.GetTotalAD() + hero.GetAP() + hero.GetArmor() + hero.GetMR();
            return std::min(1.0f, totalStats / 800.0f);
        }
    };

    /// Near mouse weight — prioritize targets close to mouse cursor
    /// Source: Weights/NearMouseWeight.cs
    class NearMouseWeight : public IWeightItem {
    public:
        NearMouseWeight() : IWeightItem("mouse", "Near Mouse", 0.5f, true) {}
        float GetWeight(const GameObject& hero) const override {
            Vec3 mousePos = Game::GetMouseWorldPos();
            float dist = hero.GetPosition().Distance2D(mousePos);
            return std::min(1.0f, dist / 1500.0f);
        }
    };

    /// Max HP weight — prioritize squishy targets (low max HP)
    /// Source: Weights/MaxHealthWeight.cs
    class MaxHealthWeight : public IWeightItem {
    public:
        MaxHealthWeight() : IWeightItem("maxhp", "Max Health", 0.7f, true) {}
        float GetWeight(const GameObject& hero) const override {
            return std::min(1.0f, hero.GetMaxHealth() / 4000.0f);
        }
    };

    /// Level weight — prioritize higher level targets (more threat/reward)
    /// Source: Weights/LevelWeight.cs
    class LevelWeight : public IWeightItem {
    public:
        LevelWeight() : IWeightItem("level", "Level", 0.3f) {}
        float GetWeight(const GameObject& hero) const override {
            return (float)hero.GetLevel() / 18.0f;
        }
    };


    // ========================================================================
    // WeightedTargetSelector — Aggregate all weight items
    // ========================================================================
    // Source: EnsoulSharp.SDK/Core/Wrappers/TargetSelector/Modes/Weight.cs
    // ========================================================================
    class WeightedTargetSelector {
    public:
        static inline std::vector<std::shared_ptr<IWeightItem>> Weights;
        static inline bool Initialized = false;

        // Per-hero weight percentage (champName → 0.0-2.0, default 1.0)
        // Allows increasing/decreasing a specific hero's total weight
        static inline std::unordered_map<std::string, float> HeroPercentage;

        // Last calculated scores for drawing (champNetId → score)
        static inline std::unordered_map<int, float> LastScores;
        static inline int BestTargetNetId = 0;

        /// Initialize all default weight items
        static void Init() {
            if (Initialized) return;
            Weights.clear();

            Weights.push_back(std::make_shared<HealthWeight>());        // 1.5x — HP %
            Weights.push_back(std::make_shared<KillableWeight>());      // 2.0x — AA to kill
            Weights.push_back(std::make_shared<CrowdControlWeight>());  // 1.5x — CC'd
            Weights.push_back(std::make_shared<DistanceWeight>());      // 1.2x — Distance
            Weights.push_back(std::make_shared<AttackDamageWeight>());  // 1.0x — AD
            Weights.push_back(std::make_shared<AbilityPowerWeight>());  // 1.0x — AP
            Weights.push_back(std::make_shared<ResistWeight>());        // 1.0x — Low resists
            Weights.push_back(std::make_shared<TeamFocusWeight>());     // 1.0x — Team focus
            Weights.push_back(std::make_shared<GoldWeight>());          // 0.8x — Gold value
            Weights.push_back(std::make_shared<MaxHealthWeight>());     // 0.7x — Squishy
            Weights.push_back(std::make_shared<NearMouseWeight>());     // 0.5x — Near mouse
            Weights.push_back(std::make_shared<LevelWeight>());         // 0.3x — Level

            Initialized = true;
        }

        /// Get weight item by name (for menu configuration)
        static IWeightItem* GetByName(const std::string& name) {
            for (auto& w : Weights) {
                if (w->Name == name) return w.get();
            }
            return nullptr;
        }

        /// Set per-hero percentage (1.0 = default, 2.0 = double weight, 0.5 = half)
        static void SetHeroPercentage(const std::string& champName, float pct) {
            HeroPercentage[champName] = std::max(0.0f, std::min(2.0f, pct));
        }

        static float GetHeroPercentage(const std::string& champName) {
            auto it = HeroPercentage.find(champName);
            return (it != HeroPercentage.end()) ? it->second : 1.0f;
        }

        /// Calculate normalized weighted score using min-max normalization across targets
        /// (EnsoulSharp Weight.cs pattern: normalize each weight item across all targets)
        static float CalculateScore(const GameObject& target) {
            if (!Initialized) Init();

            float totalScore = 0.0f;
            float totalWeight = 0.0f;

            for (auto& item : Weights) {
                if (!item || !item->Enabled) continue;

                float rawWeight = item->GetWeight(target);
                rawWeight = std::max(0.0f, std::min(1.0f, rawWeight));
                if (item->Inverted) rawWeight = 1.0f - rawWeight;

                totalScore += rawWeight * item->DefaultWeight;
                totalWeight += item->DefaultWeight;
            }

            if (totalWeight > 0) totalScore /= totalWeight;

            // Apply per-hero percentage multiplier
            float heroPct = GetHeroPercentage(target.GetChampionName());
            totalScore *= heroPct;

            return totalScore;
        }

        /// Calculate scores for ALL targets with min-max normalization
        /// This is more accurate than individual CalculateScore calls because
        /// it normalizes each weight item across the full candidate set
        static std::unordered_map<int, float> CalculateNormalizedScores(
            const std::vector<GameObject>& targets)
        {
            if (!Initialized) Init();

            std::unordered_map<int, float> scores;
            if (targets.empty()) return scores;

            // Step 1: Collect raw values for each weight item per target
            struct TargetRaw {
                int netId;
                std::string champName;
                std::vector<float> rawValues;
            };

            std::vector<TargetRaw> rawData;
            for (auto& t : targets) {
                TargetRaw tr;
                tr.netId = t.GetNetId();
                tr.champName = t.GetChampionName();
                for (auto& item : Weights) {
                    if (!item || !item->Enabled) {
                        tr.rawValues.push_back(0.0f);
                        continue;
                    }
                    float raw = item->GetWeight(t);
                    raw = std::max(0.0f, std::min(1.0f, raw));
                    if (item->Inverted) raw = 1.0f - raw;
                    tr.rawValues.push_back(raw);
                }
                rawData.push_back(tr);
            }

            // Step 2: Min-max normalize each weight dimension across all targets
            size_t numWeights = Weights.size();
            for (size_t w = 0; w < numWeights; w++) {
                float minVal = FLT_MAX, maxVal = -FLT_MAX;
                for (auto& tr : rawData) {
                    if (w < tr.rawValues.size()) {
                        minVal = std::min(minVal, tr.rawValues[w]);
                        maxVal = std::max(maxVal, tr.rawValues[w]);
                    }
                }
                float range = maxVal - minVal;
                if (range < 0.001f) range = 1.0f; // Avoid division by zero
                for (auto& tr : rawData) {
                    if (w < tr.rawValues.size()) {
                        tr.rawValues[w] = (tr.rawValues[w] - minVal) / range;
                    }
                }
            }

            // Step 3: Calculate weighted sum
            for (auto& tr : rawData) {
                float totalScore = 0.0f;
                float totalWeight = 0.0f;
                for (size_t w = 0; w < numWeights; w++) {
                    if (w >= Weights.size() || !Weights[w] || !Weights[w]->Enabled) continue;
                    totalScore += tr.rawValues[w] * Weights[w]->DefaultWeight;
                    totalWeight += Weights[w]->DefaultWeight;
                }
                if (totalWeight > 0) totalScore /= totalWeight;

                // Apply per-hero percentage
                totalScore *= GetHeroPercentage(tr.champName);

                scores[tr.netId] = totalScore;
            }

            return scores;
        }

        /// Get best target using normalized weight system
        static GameObject GetTarget(const std::vector<GameObject>& targets) {
            if (targets.empty()) return GameObject();

            // Use normalized scoring
            auto scores = CalculateNormalizedScores(targets);

            // Update last scores for drawing
            LastScores = scores;

            GameObject best;
            float bestScore = -1.0f;

            for (auto& target : targets) {
                float score = 0.0f;
                auto it = scores.find(target.GetNetId());
                if (it != scores.end()) score = it->second;

                if (score > bestScore) {
                    bestScore = score;
                    best = target;
                }
            }

            BestTargetNetId = best.IsValid() ? best.GetNetId() : 0;
            return best;
        }

        /// Get all targets sorted by weight (highest first)
        static std::vector<GameObject> GetSorted(std::vector<GameObject> targets) {
            auto scores = CalculateNormalizedScores(targets);
            LastScores = scores;

            std::sort(targets.begin(), targets.end(),
                [&scores](const GameObject& a, const GameObject& b) {
                    float sa = 0.0f, sb = 0.0f;
                    auto itA = scores.find(a.GetNetId());
                    auto itB = scores.find(b.GetNetId());
                    if (itA != scores.end()) sa = itA->second;
                    if (itB != scores.end()) sb = itB->second;
                    return sa > sb;
                });

            if (!targets.empty())
                BestTargetNetId = targets[0].GetNetId();

            return targets;
        }
    };


    // ========================================================================
    // ChampionPriority — Role-based champion threat database
    // ========================================================================
    // Source: EnsoulSharp.SDK/Core/Wrappers/TargetSelector/Priorities.cs
    //
    // Priority 5: ADC (highest threat / squishiest)
    // Priority 4: Assassin / Burst Mage
    // Priority 3: Mage / Fighter-Mage
    // Priority 2: Fighter / Bruiser
    // Priority 1: Support / Tank (lowest priority)
    // ========================================================================
    class ChampionPriority {
    public:
        static int GetPriority(const std::string& champName) {
            auto it = s_priorities.find(champName);
            if (it != s_priorities.end()) return it->second;
            return 3; // Default: mid-priority
        }

        static void SetPriority(const std::string& champName, int priority) {
            s_priorities[champName] = std::max(1, std::min(5, priority));
        }

        static void Init() {
            if (!s_priorities.empty()) return;

            // ---- Priority 5: ADC ----
            for (auto& n : { "Jinx", "Vayne", "Twitch", "KogMaw", "Tristana", "Caitlyn",
                             "Draven", "Ashe", "MissFortune", "Kalista", "Lucian", "Jhin",
                             "Xayah", "Aphelios", "Samira", "Zeri", "Smolder", "Varus",
                             "Sivir", "Ezreal", "Kaisa", "Kindred" })
                s_priorities[n] = 5;

            // ---- Priority 4: Assassin / Burst Mage ----
            for (auto& n : { "Akali", "Zed", "Talon", "Katarina", "Fizz", "LeBlanc",
                             "Evelynn", "Khazix", "Rengar", "Shaco", "Pyke", "Qiyana",
                             "Kayn", "Naafiri", "Yone", "Yasuo", "Irelia", "Fiora",
                             "Riven", "Viego", "Ahri", "Syndra", "Veigar", "Annie",
                             "Brand", "Lux", "Xerath", "Ziggs", "Velkoz", "Zyra",
                             "Viktor", "Cassiopeia", "Azir", "Orianna", "TwistedFate",
                             "Hwei", "Aurora", "Ambessa", "Mel" })
                s_priorities[n] = 4;

            // ---- Priority 3: Mage / Fighter ----
            for (auto& n : { "Ryze", "Anivia", "Taliyah", "Malzahar", "Swain", "Vladimir",
                             "Kennen", "Rumble", "Teemo", "Heimerdinger", "Zilean",
                             "Karma", "Morgana", "Lissandra", "Neeko", "Zoe",
                             "Gnar", "Jayce", "Gangplank", "Jax", "Camille", "Wukong",
                             "Warwick", "Hecarim", "Olaf", "Trundle", "Udyr", "XinZhao",
                             "Vex", "Lillia", "Diana", "Mordekaiser", "Sylas" })
                s_priorities[n] = 3;

            // ---- Priority 2: Bruiser / Off-Tank ----
            for (auto& n : { "Darius", "Garen", "Renekton", "Nasus", "Illaoi", "Urgot",
                             "Sett", "Aatrox", "Kled", "Yorick", "Tryndamere", "Volibear",
                             "Pantheon", "LeeSin", "JarvanIV", "Vi", "RekSai", "Nocturne",
                             "Skarner", "Amumu", "Sejuani", "Zac", "Gragas", "Elise",
                             "Nidalee", "Belveth", "Briar", "KSante" })
                s_priorities[n] = 2;

            // ---- Priority 1: Tank / Support ----
            for (auto& n : { "Malphite", "Ornn", "Maokai", "ChoGath", "Sion", "Poppy",
                             "TahmKench", "DrMundo", "Rammus", "Singed", "Nautilus",
                             "Leona", "Braum", "Alistar", "Thresh", "Blitzcrank",
                             "Taric", "Rakan", "Bard", "Janna", "Lulu", "Nami",
                             "Sona", "Soraka", "Yuumi", "Renata", "Milio", "Ivern",
                             "Galio", "Rell", "Shen" })
                s_priorities[n] = 1;
        }

    private:
        static inline std::unordered_map<std::string, int> s_priorities;
    };


    // ========================================================================
    // TargetSelector — Main class
    // ========================================================================
    // ========================================================================
    // FoW Humanizer — Adds human-like delay after target appears from fog
    // ========================================================================
    // Source: EnsoulSharp.SDK TargetSelectorHumanizer.cs
    // ========================================================================
    class TargetSelectorHumanizer {
    public:
        static inline float FowDelay = 0.0f;  // Delay in seconds (0 = disabled)

        struct VisibilityEntry {
            float lastVisibleChangeTime = 0.0f;  // Game time when visibility changed
            bool wasVisible = false;
        };

        static inline std::unordered_map<int, VisibilityEntry> VisibilityMap;

        /// Update visibility tracking for all enemy heroes (call every frame)
        static void Update() {
            float now = Game::GetTime();
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid()) continue;
                int netId = hero.GetNetId();
                bool visible = hero.IsVisible();

                auto it = VisibilityMap.find(netId);
                if (it == VisibilityMap.end()) {
                    VisibilityMap[netId] = { now, visible };
                } else {
                    if (visible != it->second.wasVisible) {
                        it->second.lastVisibleChangeTime = now;
                        it->second.wasVisible = visible;
                    }
                }
            }
        }

        /// Check if target should be filtered out (recently appeared from fog)
        static bool ShouldFilter(const GameObject& target) {
            if (FowDelay <= 0.0f) return false;
            if (!target.IsVisible()) return false;

            int netId = target.GetNetId();
            auto it = VisibilityMap.find(netId);
            if (it == VisibilityMap.end()) return false;

            float timeSinceAppear = Game::GetTime() - it->second.lastVisibleChangeTime;
            return timeSinceAppear < FowDelay;
        }

        /// Filter targets by FoW humanizer
        static std::vector<GameObject> FilterTargets(const std::vector<GameObject>& targets) {
            if (FowDelay <= 0.0f) return targets;

            std::vector<GameObject> result;
            for (auto& t : targets) {
                if (!ShouldFilter(t))
                    result.push_back(t);
            }
            // If all targets filtered, return original list (don't leave empty)
            return result.empty() ? targets : result;
        }
    };


    class TargetSelector {
    public:
        // Selection modes
        enum class Mode {
            LowestHP,       // Attack lowest health first
            Closest,        // Attack nearest enemy
            MostAD,         // Prioritize highest AD
            MostAP,         // Prioritize highest AP
            Priority,       // Champion priority list (threat score)
            NearMouse,      // Closest to mouse cursor
            LeastAttacks,   // Fewest attacks to kill
            AutoPriority,   // DamageCalc-based scoring (recommended)
            Weighted,       // Full weight system (12 weight items)
            LessAttack,     // Similar to LeastAttacks but using effective HP
            MostStacks,     // Prioritize targets with most buff stacks
        };

        // Current selection mode (settable from menu)
        static inline Mode CurrentMode = Mode::Weighted;

        // ====================================================================
        // GetRealHealth — health including shields, minus incoming damage
        // EnsoulSharp: Extensions.GetRealHealth()
        // ====================================================================
        static float GetRealHealth(const GameObject& target, DamageType dmgType = DamageType::Physical) {
            float hp = target.GetHealth();

            // Add shields based on damage type
            switch (dmgType) {
            case DamageType::Physical:
                hp += target.GetAllShield() + target.GetPhysicalShield();
                break;
            case DamageType::Magical:
                hp += target.GetAllShield() + target.GetMagicalShield();
                break;
            case DamageType::True:
                // True damage ignores shields (only raw HP)
                break;
            case DamageType::Mixed:
                hp += target.GetAllShield();
                break;
            }

            return hp;
        }

        // ====================================================================
        // IsValidTarget — Enhanced validity check
        // Checks: valid ptr, alive, visible, targetable, not invulnerable,
        //         not zombie (Sion passive, Karthus passive), in range
        // ====================================================================
        static bool IsValidTarget(const GameObject& target, float range = 25000.0f,
                                  const Vec3& from = Vec3()) {
            if (!target.IsValid()) return false;
            if (!target.IsAlive()) return false;
            if (!target.IsVisible()) return false;
            if (!target.IsTargetable()) return false;

            // Check if target is invulnerable (Kayle R, Tryndamere R, etc.)
            if (HasInvulnerableBuff(target)) return false;

            // Check if target is a zombie (Sion passive, Karthus passive, Kog'Maw passive)
            if (target.IsZombie()) return false;

            // Range check
            if (range > 0.0f) {
                Vec3 fromPos = from.IsZero() ? GameObjects::Player.GetPosition() : from;
                float dist = target.GetPosition().Distance2D(fromPos);
                if (dist > range + target.GetBoundingRadius()) return false;
            }

            return true;
        }

        // ====================================================================
        // Custom Provider Override (For Plugins like TargetSelectorPlugin)
        // ====================================================================
        static inline std::function<GameObject(float, DamageType)> CustomTargetSelector = nullptr;

        // ====================================================================
        // Main target selection
        // ====================================================================
        static GameObject GetTarget(float range, Mode mode = CurrentMode) {
            if (CustomTargetSelector) {
                // If a plugin has registered a custom selector, delegate to it
                return CustomTargetSelector(range, DamageType::Physical);
            }

            auto targets = GetValidTargets(range);
            if (targets.empty()) return GameObject();

            switch (mode) {
            case Mode::LowestHP:     return GetLowestHP(targets);
            case Mode::Closest:      return GetClosest(targets);
            case Mode::MostAD:       return GetMostAD(targets);
            case Mode::MostAP:       return GetMostAP(targets);
            case Mode::Priority:     return GetByPriority(targets);
            case Mode::NearMouse:    return GetNearMouse(targets);
            case Mode::LeastAttacks: return GetLeastAttacks(targets);
            case Mode::AutoPriority: return GetAutoPriority(targets);
            case Mode::Weighted:     return WeightedTargetSelector::GetTarget(targets);
            case Mode::LessAttack:   return GetLessAttack(targets);
            case Mode::MostStacks:   return GetMostStacks(targets);
            default:                 return GetAutoPriority(targets);
            }
        }

        // ====================================================================
        // GetTarget with DamageType — considers shields based on damage type
        // EnsoulSharp: TargetSelector.GetTarget(float range, DamageType dmgType)
        // ====================================================================
        static GameObject GetTarget(float range, DamageType dmgType, Mode mode = CurrentMode) {
            if (CustomTargetSelector) {
                return CustomTargetSelector(range, dmgType);
            }

            auto targets = GetValidTargets(range);
            if (targets.empty()) return GameObject();

            // Filter out targets that are effectively unkillable by this damage type
            // (e.g., magic shield vs physical damage doesn't matter)

            if (mode == Mode::Weighted) {
                return WeightedTargetSelector::GetTarget(targets);
            }

            // For damage-type-aware selection: sort by effective HP for this damage type
            return *std::min_element(targets.begin(), targets.end(),
                [dmgType](const GameObject& a, const GameObject& b) {
                    return GetRealHealth(a, dmgType) < GetRealHealth(b, dmgType);
                });
        }

        // Get multiple targets sorted by mode
        static std::vector<GameObject> GetTargets(float range, Mode mode = Mode::AutoPriority) {
            auto targets = GetValidTargets(range);
            SortByMode(targets, mode);
            return targets;
        }

        // ====================================================================
        // Forced target (for click-to-focus)
        // ====================================================================
        static inline GameObject ForcedTarget;

        // When true, only attack the forced/selected target. If OOR → attack nothing.
        // Set by TargetSelectorPlugin from its "Only Attack Select Target" menu option.
        static inline bool OnlyAttackSelected = false;

        static void SetForcedTarget(const GameObject& target) {
            ForcedTarget = target;
        }

        static void ClearForcedTarget() {
            ForcedTarget = GameObject();
        }

        static GameObject GetForcedTarget() {
            if (ForcedTarget.IsValid() && IsValidTarget(ForcedTarget))
                return ForcedTarget;
            return GameObject();
        }

        // ====================================================================
        // Convenience: Get selected or best target
        // ====================================================================
        static GameObject GetSelectedTarget(float range, Mode mode = Mode::AutoPriority) {
            // Priority: forced target > best target
            auto forced = GetForcedTarget();
            if (forced.IsValid()) {
                float dist = GameObjects::Player.DistanceTo(forced);
                if (dist <= range + forced.GetBoundingRadius())
                    return forced;
            }
            return GetTarget(range, mode);
        }

        // ====================================================================
        // GetTargetNoCollision — Get target without minion collision
        // Source: EnsoulSharp TargetSelector.GetTargetNoCollision(Spell)
        // Returns a target where there are no minions blocking the path
        // ====================================================================
        static GameObject GetTargetNoCollision(float range, float spellWidth = 70.0f,
                                               float spellSpeed = FLT_MAX,
                                               Mode mode = CurrentMode) {
            auto targets = GetValidTargets(range);
            if (targets.empty()) return GameObject();

            // Apply FoW humanizer
            targets = TargetSelectorHumanizer::FilterTargets(targets);

            // Filter targets by collision: remove those blocked by minions
            std::vector<GameObject> unblocked;
            Vec3 from = GameObjects::Player.GetPosition();

            for (auto& target : targets) {
                bool blocked = false;
                Vec3 to = target.GetPosition();
                float dist = from.Distance2D(to);

                // Check if any enemy minion is in the path
                for (auto& minion : GameObjects::EnemyMinions) {
                    if (!minion.IsAlive()) continue;
                    Vec3 mPos = minion.GetPosition();

                    // Point-to-line distance check
                    Vec3 dir = Vec3(to.x - from.x, 0, to.z - from.z);
                    float dirLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
                    if (dirLen < 1.0f) continue;

                    Vec3 toMinion = Vec3(mPos.x - from.x, 0, mPos.z - from.z);
                    float proj = (toMinion.x * dir.x + toMinion.z * dir.z) / dirLen;

                    if (proj < 0 || proj > dirLen) continue; // Behind or beyond

                    float perpDist = std::abs(toMinion.x * dir.z - toMinion.z * dir.x) / dirLen;
                    if (perpDist < spellWidth + minion.GetBoundingRadius()) {
                        blocked = true;
                        break;
                    }
                }

                if (!blocked)
                    unblocked.push_back(target);
            }

            if (unblocked.empty()) return GameObject(); // All blocked

            // Select best from unblocked targets using chosen mode
            switch (mode) {
            case Mode::Weighted:     return WeightedTargetSelector::GetTarget(unblocked);
            case Mode::AutoPriority: return GetAutoPriority(unblocked);
            case Mode::LowestHP:     return GetLowestHP(unblocked);
            case Mode::Closest:      return GetClosest(unblocked);
            default:                 return GetAutoPriority(unblocked);
            }
        }

        // ====================================================================
        // Update — call every frame for FoW humanizer tracking
        // ====================================================================
        static void Update() {
            TargetSelectorHumanizer::Update();
        }

    private:
        // ====================================================================
        // Invulnerable buff check — comprehensive list
        // Source: EnsoulSharp.SDK Invulnerable.cs
        // ====================================================================
        static bool HasInvulnerableBuff(const GameObject& target) {
            // Quick check: GameObject flag
            if (target.IsInvulnerable()) return true;

            BuffManager buffs(target.address);

            // Comprehensive invulnerable buff list (expanded)
            // Source: EnsoulSharp.SDK Invulnerable.cs + community additions
            static const char* invulnBuffs[] = {
                // Full invulnerability
                "KayleR",                       // Kayle R
                "UndyingRage",                  // Tryndamere R
                "kindaborroweytime",            // Kindred R
                "ChronoShift",                  // Zilean R (revive)
                "JudicatorIntervention",        // Old Kayle R
                "ZhonyasRingShield",            // Zhonya's
                "zhonyasringshield",            // Zhonya's (lowercase)
                "chronorevive",                 // Zilean R revive state
                "BardRStasis",                  // Bard R

                // Untargetable states
                "FioraW",                       // Fiora W (riposte)
                "faborroweetime",             // Fizz E
                "VladimirSanguinePool",         // Vladimir W
                "EkkoR",                        // Ekko R
                "lissaborroweetime",            // Lissandra R (self)
                "MasterYiQ",                    // Master Yi Q
                "XayahR",                       // Xayah R
                "SamiraW",                      // Samira W (partial)

                // Damage immunity / dodge
                "YiMeditate",                   // Master Yi W (damage reduction)
                "JaxCounterStrike",             // Jax E (dodge)
                "PantheonE",                    // Pantheon E (shield)
                "NilahW",                       // Nilah W
                "ShenWBuff",                    // Shen W (spirit blade)
                "GwenW",                        // Gwen W (hallowed mist)
                "TaricR",                       // Taric R (cosmic radiance)
                "MorganaE",                     // Morgana E (Black Shield, magic only)

                // Guardian Angel / revive
                "willrevive",                   // GA passive
                "AkShanEResurrect",             // Akshan passive
                "aatroxpassivedeath",           // Aatrox revive

                // Spell shields (targeted spells only)
                "bansheesveil",                 // Banshee's Veil
                "SpellShield",                  // Edge of Night / Sivir E
                "NocturneShield",               // Nocturne W
                "malaborroweeveil",             // Malzahar passive

                // Special states
                "KogMawIcathianSurprise",       // Kog'Maw passive (already dead)
                "KarthusDeathDefiedBuff",       // Karthus passive
                "SionPassiveZombie",            // Sion passive
                nullptr
            };

            for (const char** p = invulnBuffs; *p != nullptr; p++) {
                if (buffs.HasBuff(*p)) return true;
            }
            return false;
        }

        // ====================================================================
        // Get all valid targets in range (with FoW humanizer filter)
        // ====================================================================
        static std::vector<GameObject> GetValidTargets(float range) {
            std::vector<GameObject> result;
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (IsValidTarget(hero, range))
                    result.push_back(hero);
            }
            // Apply FoW humanizer
            result = TargetSelectorHumanizer::FilterTargets(result);
            return result;
        }

        // ====================================================================
        // Mode implementations
        // ====================================================================

        static GameObject GetLowestHP(std::vector<GameObject>& targets) {
            return *std::min_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    return a.GetHealth() < b.GetHealth();
                });
        }

        static GameObject GetClosest(std::vector<GameObject>& targets) {
            return *std::min_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    return GameObjects::Player.DistanceTo(a) < GameObjects::Player.DistanceTo(b);
                });
        }

        static GameObject GetMostAD(std::vector<GameObject>& targets) {
            return *std::max_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    return a.GetTotalAD() < b.GetTotalAD();
                });
        }

        static GameObject GetMostAP(std::vector<GameObject>& targets) {
            return *std::max_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    return a.GetAP() < b.GetAP();
                });
        }

        static GameObject GetNearMouse(std::vector<GameObject>& targets) {
            Vec3 mousePos = Game::GetMouseWorldPos();
            return *std::min_element(targets.begin(), targets.end(),
                [&mousePos](const GameObject& a, const GameObject& b) {
                    return a.GetPosition().Distance2D(mousePos) < b.GetPosition().Distance2D(mousePos);
                });
        }

        static GameObject GetLeastAttacks(std::vector<GameObject>& targets) {
            return *std::min_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    int aaA = DamageCalc::GetAutoAttacksToKill(GameObjects::Player, a);
                    int aaB = DamageCalc::GetAutoAttacksToKill(GameObjects::Player, b);
                    return aaA < aaB;
                });
        }

        static GameObject GetByPriority(std::vector<GameObject>& targets) {
            // Use ChampionPriority database (higher priority = more important target)
            // Tiebreaker: lower HP% = higher priority
            return *std::max_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    int priA = ChampionPriority::GetPriority(a.GetChampionName());
                    int priB = ChampionPriority::GetPriority(b.GetChampionName());
                    if (priA != priB) return priA < priB;
                    // Same priority tier: prefer lower HP%
                    float hpPctA = a.GetHealth() / std::max(1.0f, a.GetMaxHealth());
                    float hpPctB = b.GetHealth() / std::max(1.0f, b.GetMaxHealth());
                    return hpPctA > hpPctB;
                });
        }

        // AutoPriority — Uses DamageCalc::GetTargetScore (lower = better)
        static GameObject GetAutoPriority(std::vector<GameObject>& targets) {
            return *std::min_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    float scoreA = DamageCalc::GetTargetScore(GameObjects::Player, a, DamageType::Physical);
                    float scoreB = DamageCalc::GetTargetScore(GameObjects::Player, b, DamageType::Physical);
                    return scoreA < scoreB;
                });
        }

        // LessAttack — effective HP / our damage (considers shields)
        static GameObject GetLessAttack(std::vector<GameObject>& targets) {
            return *std::min_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    float ehpA = GetRealHealth(a) / std::max(1.0f,
                        DamageCalc::GetAutoAttackDamage(GameObjects::Player, a));
                    float ehpB = GetRealHealth(b) / std::max(1.0f,
                        DamageCalc::GetAutoAttackDamage(GameObjects::Player, b));
                    return ehpA < ehpB;
                });
        }

        // MostStacks — prioritize targets with most CC debuffs (proxy for stack count)
        static GameObject GetMostStacks(std::vector<GameObject>& targets) {
            return *std::max_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    // Approximate stack priority by checking CC debuffs
                    auto countDebuffs = [](const GameObject& t) -> int {
                        BuffManager buffs(t.address);
                        int count = 0;
                        if (buffs.HasBuffOfType(BuffType::Stun))       count += 3;
                        if (buffs.HasBuffOfType(BuffType::Snare))      count += 2;
                        if (buffs.HasBuffOfType(BuffType::Slow))       count += 1;
                        if (buffs.HasBuffOfType(BuffType::Suppression)) count += 3;
                        if (buffs.HasBuffOfType(BuffType::Charm))      count += 2;
                        if (buffs.HasBuffOfType(BuffType::Fear))       count += 2;
                        if (buffs.HasBuffOfType(BuffType::Taunt))      count += 2;
                        if (buffs.HasBuffOfType(BuffType::Poison))     count += 1;
                        return count;
                    };
                    return countDebuffs(a) < countDebuffs(b);
                });
        }

        // ====================================================================
        // Sorting
        // ====================================================================
        static void SortByMode(std::vector<GameObject>& targets, Mode mode) {
            switch (mode) {
            case Mode::LowestHP:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) {
                        return a.GetHealth() < b.GetHealth();
                    });
                break;
            case Mode::Closest:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) {
                        return GameObjects::Player.DistanceTo(a) < GameObjects::Player.DistanceTo(b);
                    });
                break;
            case Mode::LeastAttacks:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) {
                        int aaA = DamageCalc::GetAutoAttacksToKill(GameObjects::Player, a);
                        int aaB = DamageCalc::GetAutoAttacksToKill(GameObjects::Player, b);
                        return aaA < aaB;
                    });
                break;
            case Mode::AutoPriority:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) {
                        float scoreA = DamageCalc::GetTargetScore(GameObjects::Player, a, DamageType::Physical);
                        float scoreB = DamageCalc::GetTargetScore(GameObjects::Player, b, DamageType::Physical);
                        return scoreA < scoreB;
                    });
                break;
            case Mode::Weighted:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) {
                        return WeightedTargetSelector::CalculateScore(a) >
                               WeightedTargetSelector::CalculateScore(b);
                    });
                break;
            default:
                break;
            }
        }
    };

} // namespace SDK
