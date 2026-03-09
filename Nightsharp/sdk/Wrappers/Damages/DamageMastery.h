#pragma once
#include "GameObject.h"
#include "BuffManager.h"
#include "DamageCalc.h"
#include "Enums.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>

// ============================================================================
// DamageMastery — Rune/Keystone On-Hit & Proc Damage (Season 2026)
// Reference: EnsoulSharp.SDK/Core/Wrappers/Damages/DamageMastery.cs
// Updated: 2026-03-06 — Patch 26.S1
//
// Covers all keystones and damage-dealing minor runes:
//   Precision:  Press the Attack, Lethal Tempo, Conqueror, Fleet Footwork
//   Domination: Electrocute, Dark Harvest, Hail of Blades
//   Sorcery:    Summon Aery, Arcane Comet, Phase Rush
//   Resolve:    Grasp of the Undying, Aftershock, Guardian
//   Inspiration: First Strike, Glacial Augment
//
//   Minor runes: Cheap Shot, Sudden Impact, Scorch, Bone Plating, etc.
//
// Usage:
//   float runeDmg = SDK::DamageMastery::GetRuneDamage(source, target);
// ============================================================================

namespace SDK {

    // ========================================================================
    // Rune IDs (Riot perk IDs)
    // ========================================================================
    enum class RuneId : int {
        // --- Precision ---
        PressTheAttack   = 8005,
        LethalTempo      = 8008,
        FleetFootwork    = 8021,
        Conqueror        = 8010,
        Overheal         = 9101,
        Triumph          = 9111,
        PresenceOfMind   = 8009,
        LegendAlacrity   = 9104,
        LegendBloodline  = 9103,
        LegendHaste      = 9105,
        CoupDeGrace      = 8014,
        CutDown           = 8017,
        LastStand        = 8299,

        // --- Domination ---
        Electrocute      = 8112,
        DarkHarvest      = 8128,
        HailOfBlades     = 9923,
        CheapShot        = 8126,
        TasteOfBlood     = 8139,
        SuddenImpact     = 8143,
        EyeballCollection = 8138,
        GhostPoro       = 8120,
        ZombieWard      = 8136,
        TreasureHunter  = 8135,
        RelentlessHunter = 8105,
        UltimateHunter  = 8106,

        // --- Sorcery ---
        SummonAery       = 8214,
        ArcaneComet      = 8229,
        PhaseRush        = 8230,
        NullifyingOrb    = 8224,
        ManaflowBand     = 8226,
        NimbusCloak      = 8275,
        Transcendence    = 8210,
        Celerity         = 8234,
        AbsoluteFocus    = 8233,
        Scorch           = 8237,
        Waterwalking     = 8232,
        GatheringStorm   = 8236,

        // --- Resolve ---
        GraspOfTheUndying = 8437,
        Aftershock       = 8439,
        Guardian         = 8465,
        Demolish         = 8446,
        FontOfLife       = 8463,
        ShieldBash       = 8401,
        Conditioning     = 8429,
        SecondWind       = 8444,
        BonePlating      = 8473,
        Overgrowth       = 8451,
        Revitalize       = 8453,
        Unflinching      = 8242,

        // --- Inspiration ---
        FirstStrike      = 8369,
        GlacialAugment   = 8351,
        UnsealedSpellbook = 8360,
        HextechFlashtraption = 8306,
        MagicalFootwear  = 8304,
        PerfectTiming     = 8313,
        FuturesMarket    = 8321,
        MinionDematerializer = 8316,
        BiscuitDelivery  = 8345,
        CosmicInsight     = 8347,
        ApproachVelocity  = 8410,
        TimeWarpTonic    = 8352,
    };

    // ========================================================================
    // DamageMastery
    // ========================================================================
    class DamageMastery {
    public:
        // ====================================================================
        // RuneEntry — One rune damage entry
        // ====================================================================
        struct RuneEntry {
            RuneId      Id;
            std::string BuffName;       // In-game buff name to detect activation
            DamageType  Type;
            bool        IgnoreCalc;     // true = raw/true damage

            std::function<bool(const GameObject&, const GameObject&, const BuffManager&, const BuffManager&)> Condition;
            std::function<float(const GameObject&, const GameObject&, const BuffManager&, const BuffManager&)> DamageFunc;
        };

        // ====================================================================
        // Init
        // ====================================================================
        static void Init() {
            if (s_initialized) return;
            s_initialized = true;
            CreateRuneEntries();
        }

        // ====================================================================
        // GetRuneDamage — Total rune proc damage for source on target
        // ====================================================================
        static float GetRuneDamage(const GameObject& source, const GameObject& target) {
            if (!s_initialized) Init();

            BuffManager srcBuffs(source.address);
            BuffManager tgtBuffs(target.address);
            float total = 0.0f;

            for (auto& entry : s_entries) {
                try {
                    if (entry.Condition && entry.Condition(source, target, srcBuffs, tgtBuffs)) {
                        float raw = entry.DamageFunc ? entry.DamageFunc(source, target, srcBuffs, tgtBuffs) : 0.0f;
                        if (entry.IgnoreCalc || entry.Type == DamageType::True)
                            total += raw;
                        else
                            total += DamageCalc::CalcDamage(source, target, entry.Type, raw);
                    }
                } catch (...) {}
            }

            return total;
        }

        // ====================================================================
        // GetRuneDamageByType — Get damage from a specific rune
        // ====================================================================
        static float GetRuneDamageByType(const GameObject& source, const GameObject& target, RuneId runeId) {
            if (!s_initialized) Init();

            BuffManager srcBuffs(source.address);
            BuffManager tgtBuffs(target.address);

            for (auto& entry : s_entries) {
                if (entry.Id != runeId) continue;
                try {
                    if (entry.Condition && entry.Condition(source, target, srcBuffs, tgtBuffs)) {
                        float raw = entry.DamageFunc ? entry.DamageFunc(source, target, srcBuffs, tgtBuffs) : 0.0f;
                        if (entry.IgnoreCalc || entry.Type == DamageType::True)
                            return raw;
                        else
                            return DamageCalc::CalcDamage(source, target, entry.Type, raw);
                    }
                } catch (...) {}
            }
            return 0.0f;
        }

        // ====================================================================
        // HasRune — Check if hero has a specific rune active (by buff)
        // ====================================================================
        static bool HasRune(const GameObject& hero, RuneId runeId) {
            BuffManager buffs(hero.address);
            for (auto& entry : s_entries) {
                if (entry.Id == runeId && !entry.BuffName.empty()) {
                    return buffs.HasBuff(entry.BuffName.c_str());
                }
            }
            return false;
        }

    private:
        static inline bool s_initialized = false;
        static inline std::vector<RuneEntry> s_entries;

        // Helpers
        static int Clamp(int v, int lo, int hi) { return (std::max)(lo, (std::min)(hi, v)); }
        static float Lerp(float a, float b, int level) { return a + (b - a) * (float)(Clamp(level, 1, 18) - 1) / 17.0f; }

        // ====================================================================
        // CreateRuneEntries
        // ====================================================================
        static void CreateRuneEntries() {
            s_entries.clear();

            // ================================================================
            // PRECISION KEYSTONES
            // ================================================================

            // --- Press the Attack ---
            // After 3 AAs on same target: 40-180 adaptive damage (based on level)
            // + target takes 8-12% increased damage for 6s
            s_entries.push_back({
                RuneId::PressTheAttack, "PressTheAttack", DamageType::Mixed, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("PressTheAttackDamage"); // proc'd
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(40.0f, 180.0f, h.GetLevel());
                }
            });

            // --- Press the Attack Vulnerability ---
            // Target takes 8-12% increased damage from all sources
            // (Not direct damage — this is a multiplier, tracked separately)
            s_entries.push_back({
                RuneId::PressTheAttack, "PressTheAttack", DamageType::True, true,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("PressTheAttackDamage");
                },
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.0f; // Vulnerability is a damage amp, not direct damage
                    // Used for HasRune detection; actual amp handled in CalcDamage
                }
            });

            // --- Fleet Footwork ---
            // Energized AA heals and speeds up. No damage, but useful for tracking.
            // (No damage entry needed; heal only)

            // --- Conqueror ---
            // Stacking: 2-6 AD/AP per stack (max 12 stacks).
            // At max stacks: heal for 8% post-mitigation damage.
            // No direct on-hit proc, but adaptive bonus tracked.
            s_entries.push_back({
                RuneId::Conqueror, "ConquerorStacks", DamageType::Mixed, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("ConquerorStacks");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    int lv = h.GetLevel();
                    // Per stack: 2-6 adaptive force. Max 12 stacks.
                    // This isn't damage per se — it's AD/AP bonus already reflected in stats.
                    return 0.0f; // Stats buff, not a damage proc
                }
            });

            // ================================================================
            // DOMINATION KEYSTONES
            // ================================================================

            // --- Electrocute ---
            // 3 separate attacks/abilities within 3s: 30-180 + 40% bAD + 25% AP
            s_entries.push_back({
                RuneId::Electrocute, "Electrocute", DamageType::Mixed, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("ELECTROCUTE"); // When proc'd
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    float base = Lerp(30.0f, 180.0f, h.GetLevel());
                    return base + 0.4f * h.GetBonusAD() + 0.25f * h.GetAP();
                }
            });

            // --- Dark Harvest ---
            // Below 50% HP: 20-60 + 5/soul + 25% bAD + 15% AP
            s_entries.push_back({
                RuneId::DarkHarvest, "DarkHarvest", DamageType::Mixed, false,
                [](const GameObject&, const GameObject& t, const BuffManager& sb, const BuffManager&) {
                    return t.GetHealthPercent() < 50.0f && sb.HasBuff("DarkHarvestCooldown") == false;
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    float base = Lerp(20.0f, 60.0f, h.GetLevel());
                    // Souls can't be read easily; estimate ~20 souls mid-game
                    float soulEstimate = 20.0f * 5.0f;
                    return base + soulEstimate + 0.25f * h.GetBonusAD() + 0.15f * h.GetAP();
                }
            });

            // --- Hail of Blades ---
            // No damage, attack speed burst only.

            // ================================================================
            // DOMINATION MINOR RUNES
            // ================================================================

            // --- Cheap Shot ---
            // Bonus true damage to movement-impaired targets: 10-45
            s_entries.push_back({
                RuneId::CheapShot, "CheapShot", DamageType::True, true,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    // Target is slowed, stunned, rooted, etc.
                    return tb.HasBuff("CheapShotTarget");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(10.0f, 45.0f, h.GetLevel());
                }
            });

            // --- Sudden Impact ---
            // After dash/blink/stealth: 7 Lethality + 6 Magic Pen for 5s
            // No direct damage, but stat buff tracked for detection.

            // ================================================================
            // SORCERY KEYSTONES
            // ================================================================

            // --- Summon Aery ---
            // Offensive: 10-40 + 15% bAD + 10% AP adaptive damage
            s_entries.push_back({
                RuneId::SummonAery, "SummonAery", DamageType::Mixed, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("AerySend"); // Aery flying to target
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    float base = Lerp(10.0f, 40.0f, h.GetLevel());
                    return base + 0.15f * h.GetBonusAD() + 0.1f * h.GetAP();
                }
            });

            // --- Arcane Comet ---
            // Ability hit: 30-100 + 20% bAD + 10% AP adaptive damage
            s_entries.push_back({
                RuneId::ArcaneComet, "ArcaneComet", DamageType::Mixed, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("ArcaneCometSnipe"); // Comet landing
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    float base = Lerp(30.0f, 100.0f, h.GetLevel());
                    return base + 0.2f * h.GetBonusAD() + 0.1f * h.GetAP();
                }
            });

            // --- Phase Rush ---
            // No damage, movement speed only.

            // ================================================================
            // SORCERY MINOR RUNES
            // ================================================================

            // --- Scorch ---
            // Ability damage sets target on fire: 15-35 magic damage
            s_entries.push_back({
                RuneId::Scorch, "Scorch", DamageType::Magical, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("MasteryBurnoutDebuff");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(15.0f, 35.0f, h.GetLevel());
                }
            });

            // ================================================================
            // RESOLVE KEYSTONES
            // ================================================================

            // --- Grasp of the Undying ---
            // After 4s in combat, next AA deals 3.5% max HP magic damage (melee)
            // or 2.4% for ranged
            s_entries.push_back({
                RuneId::GraspOfTheUndying, "GraspOfTheUndying", DamageType::Magical, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("GraspOfTheUndying");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    float pct = h.GetAttackRange() > 300.0f ? 0.024f : 0.035f;
                    return pct * h.GetMaxHealth();
                }
            });

            // --- Aftershock ---
            // After immobilizing: 25-120 + 8% bHP magic damage in AoE
            s_entries.push_back({
                RuneId::Aftershock, "Aftershock", DamageType::Magical, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("AftershockReadyDamage");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    float base = Lerp(25.0f, 120.0f, h.GetLevel());
                    return base + 0.08f * h.GetMaxHealth();
                }
            });

            // --- Guardian ---
            // Shield ally, no damage.

            // ================================================================
            // RESOLVE MINOR RUNES
            // ================================================================

            // --- Demolish ---
            // After 3s near turret: AA deals 100 + 35% max HP bonus physical to turret
            s_entries.push_back({
                RuneId::Demolish, "Demolish", DamageType::Physical, false,
                [](const GameObject&, const GameObject& t, const BuffManager& sb, const BuffManager&) {
                    return t.IsTurret() && sb.HasBuff("DemolishReady");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 100.0f + 0.35f * h.GetMaxHealth();
                }
            });

            // --- Shield Bash ---
            // While shielded, next AA deals 5-30 + 1.5% bHP + 8.5% shield amount
            s_entries.push_back({
                RuneId::ShieldBash, "ShieldBash", DamageType::Mixed, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("ShieldBashReady");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    float base = Lerp(5.0f, 30.0f, h.GetLevel());
                    return base + 0.015f * h.GetMaxHealth();
                }
            });

            // --- Bone Plating ---
            // No damage, damage reduction only.

            // ================================================================
            // INSPIRATION KEYSTONES
            // ================================================================

            // --- First Strike ---
            // First to deal damage: 5 gold + 9% of bonus damage dealt as true damage
            // The direct proc is a bonus true damage multiplier.
            s_entries.push_back({
                RuneId::FirstStrike, "FirstStrike", DamageType::True, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("FirstStrike");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    // 9% of damage dealt as bonus true damage (simplified)
                    // We return an estimated flat value since actual damage isn't known here
                    return Lerp(5.0f, 25.0f, h.GetLevel()); // Rough estimate
                }
            });

            // --- Glacial Augment ---
            // No damage, slow only.

            // --- Unsealed Spellbook ---
            // No damage.

            // ================================================================
            // PRECISION MINOR RUNES
            // ================================================================

            // --- Coup de Grace ---
            // 8% more damage to champions below 40% HP
            // (This is a damage amplifier, not direct damage.)

            // --- Cut Down ---
            // 5-15% more damage to champions with more max HP than you
            // (Damage amplifier)

            // --- Last Stand ---
            // 5-11% more damage while below 60% HP
            // (Damage amplifier)

            // ================================================================
            // DAMAGE AMPLIFIERS (not direct damage, but tracked for GetDamageMultiplier)
            // ================================================================

            // These are added so HasRune() can detect them,
            // but they return 0 damage (they're multipliers).
            s_entries.push_back({
                RuneId::CoupDeGrace, "CoupDeGrace", DamageType::Mixed, true,
                [](const GameObject&, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return t.GetHealthPercent() < 40.0f;
                },
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.0f; // 8% amp handled in CalcDamage wrapper
                }
            });

            s_entries.push_back({
                RuneId::CutDown, "CutDown", DamageType::Mixed, true,
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return t.GetMaxHealth() > h.GetMaxHealth();
                },
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.0f; // 5-15% amp based on HP difference
                }
            });

            s_entries.push_back({
                RuneId::LastStand, "LastStand", DamageType::Mixed, true,
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return h.GetHealthPercent() < 60.0f;
                },
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.0f; // 5-11% amp based on missing HP
                }
            });
        }

    public:

        // ====================================================================
        // GetDamageMultiplier — Returns the total damage multiplier from runes
        //
        // e.g., Press the Attack vulnerability + Coup de Grace + Cut Down
        // Returns 1.0 if no multipliers are active.
        // ====================================================================
        static float GetDamageMultiplier(const GameObject& source, const GameObject& target) {
            if (!s_initialized) Init();

            BuffManager srcBuffs(source.address);
            BuffManager tgtBuffs(target.address);
            float mult = 1.0f;

            // --- Press the Attack vulnerability (8-12%) ---
            if (tgtBuffs.HasBuff("PressTheAttackDamage")) {
                mult *= 1.08f + 0.04f * (float)(Clamp(source.GetLevel(), 1, 18) - 1) / 17.0f;
            }

            // --- Coup de Grace (8% to <40% HP) ---
            if (target.GetHealthPercent() < 40.0f && srcBuffs.HasBuff("Mastery6261")) {
                mult *= 1.08f;
            }

            // --- Cut Down (5-15% based on HP difference) ---
            if (target.GetMaxHealth() > source.GetMaxHealth()) {
                float hpDiff = target.GetMaxHealth() - source.GetMaxHealth();
                float pct = (std::min)(0.15f, (std::max)(0.05f, hpDiff / 1000.0f * 0.05f));
                mult *= (1.0f + pct);
            }

            // --- Last Stand (5-11% when <60% HP) ---
            if (source.GetHealthPercent() < 60.0f) {
                float missingPct = 1.0f - source.GetHealthPercent() / 100.0f;
                float bonus = 0.05f + (std::min)(0.06f, missingPct * 0.15f);
                mult *= (1.0f + bonus);
            }

            // --- First Strike (9% bonus as true damage, simplified) ---
            if (srcBuffs.HasBuff("FirstStrike")) {
                mult *= 1.09f;
            }

            return mult;
        }

        // ====================================================================
        // GetTotalOnHitDamage — Convenience: GetRuneDamage + multiplier applied
        // ====================================================================
        static float GetTotalOnHitDamage(const GameObject& source, const GameObject& target) {
            float runeDmg = GetRuneDamage(source, target);
            float multiplier = GetDamageMultiplier(source, target);
            return runeDmg * multiplier;
        }
    };

} // namespace SDK
