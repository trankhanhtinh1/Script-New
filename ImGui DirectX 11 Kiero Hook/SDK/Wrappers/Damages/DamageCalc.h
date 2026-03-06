#pragma once
#include "GameObject.h"
#include "BuffManager.h"
#include "SpellBook.h"
#include "Enums.h"
#include "core/Offsets.h"
#include "core/Globals.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <functional>
#include <string>

// ============================================================================
// DamageCalc — Damage calculation library
// Updated: 2026-03-05 — Patch 26.5 (Season 2026)
//
// References:
//   https://mobalytics.gg/lol/guides/new-lol-items-2026
//   https://www.leagueoflegends.com/en-us/news/tags/patch-notes/
//   https://leagueoflegends.fandom.com/wiki
//
// CHANGELOG vs EnsoulSharp (old S13 items):
//   [REMOVED S14 — Patch 14.1]
//     - Divine Sunderer (6632)
//     - Iceborn Gauntlet (6662)
//     - Guinsoo's Rageblade (3124)
//     - Kraken Slayer on-hit (6672 — reworked, no longer on-hit)
//
//   [UPDATED S14]
//     - BOTRK: 10%/8% → 9%/6% current HP, added min damage 15
//     - Titanic Hydra: corrected to 5 + 1.5% bonus HP
//     - Smite: 600-900 scaling, no longer targets champions
//
//   [ADDED S14 — Patch 14.1]
//     - Sundered Sky (6698) — Spellblade + % max HP
//     - Statikk Shiv (3087) — re-added, Energized on-hit
//     - Terminus (3302) — 30 mixed on-hit
//
//   [ADDED S16 — Season 2026, Patch 26.1]
//     - Dusk and Dawn — Spellblade AP bruiser (build from Sheen)
//     - Stormrazor (3095) — Energized on-hit, returned
//     - Hextech Gunblade (3146) — returned, active + spell vamp
//     - Fiendhunter Bolts — empowers 3 AAs after ult
//     - Bastionbreaker — ability true damage + Sabotage
//     - Actualizer, Bandlepipes, Protoplasm Harness, etc. (no on-hit)
//
// NOTE: Item IDs marked with TODO_ID need verification from Data Dragon or
//       game memory. Use LOLDumper or read item slot data in-game to confirm.
// ============================================================================

namespace SDK {

    class DamageCalc {
    public:
        // ====================================================================
        // Physical Damage
        // Formula: rawDamage * (100 / (100 + effectiveArmor))
        // effectiveArmor = armor * (1 - armorPenPercent) - lethality
        // ====================================================================
        static float CalcPhysicalDamage(const GameObject& source, const GameObject& target, float rawDamage) {
            if (rawDamage <= 0) return 0.0f;

            float armor = target.GetArmor();
            float armorPenPercent = source.GetArmorPenPercent();
            float armorPenFlat = source.GetArmorPenFlat();
            float lethality = source.GetLethality();

            // Lethality scales with target level
            int targetLevel = target.GetLevel();
            if (targetLevel <= 0) targetLevel = 1;
            float flatPen = armorPenFlat + lethality * (0.6f + 0.4f * (float)targetLevel / 18.0f);

            float damageMultiplier;
            if (armor < 0) {
                damageMultiplier = 2.0f - (100.0f / (100.0f - armor));
            } else {
                float effectiveArmor = armor * (1.0f - armorPenPercent) - flatPen;
                if (effectiveArmor < 0) effectiveArmor = 0;
                damageMultiplier = 100.0f / (100.0f + effectiveArmor);
            }

            return std::floor(rawDamage * damageMultiplier);
        }

        // ====================================================================
        // Magic Damage
        // Formula: rawDamage * (100 / (100 + effectiveMR))
        // ====================================================================
        static float CalcMagicDamage(const GameObject& source, const GameObject& target, float rawDamage) {
            if (rawDamage <= 0) return 0.0f;

            float mr = target.GetMR();
            float magicPenPercent = source.GetMagicPenPercent();
            float magicPenFlat = source.GetMagicPenFlat();

            float damageMultiplier;
            if (mr < 0) {
                damageMultiplier = 2.0f - (100.0f / (100.0f - mr));
            } else {
                float effectiveMR = mr * (1.0f - magicPenPercent) - magicPenFlat;
                if (effectiveMR < 0) effectiveMR = 0;
                damageMultiplier = 100.0f / (100.0f + effectiveMR);
            }

            return std::floor(rawDamage * damageMultiplier);
        }

        // ====================================================================
        // True Damage (no reduction)
        // ====================================================================
        static float CalcTrueDamage(float rawDamage) {
            return std::floor(rawDamage);
        }

        // ====================================================================
        // Mixed Damage (half physical, half magical)
        // ====================================================================
        static float CalcMixedDamage(const GameObject& source, const GameObject& target, float rawDamage) {
            float half = rawDamage / 2.0f;
            return CalcPhysicalDamage(source, target, half) + CalcMagicDamage(source, target, half);
        }

        // ====================================================================
        // Generic calculator
        // ====================================================================
        static float CalcDamage(const GameObject& source, const GameObject& target,
                                DamageType type, float rawDamage) {
            float dmg = 0.0f;
            switch (type) {
            case DamageType::Physical: dmg = CalcPhysicalDamage(source, target, rawDamage); break;
            case DamageType::Magical:  dmg = CalcMagicDamage(source, target, rawDamage); break;
            case DamageType::True:     dmg = CalcTrueDamage(rawDamage); break;
            case DamageType::Mixed:    dmg = CalcMixedDamage(source, target, rawDamage); break;
            default:                   dmg = rawDamage; break;
            }
            // Apply damage reduction passives (Alistar R, Garen W, Exhaust, etc.)
            if (type != DamageType::True) {
                dmg = DamageReductionMod(source, target, dmg, type);
            }
            return std::max(dmg, 0.0f);
        }

        // ====================================================================
        // DamageReductionMod — Buff-based damage reduction
        // Port of EnsoulSharp Damage.cs::DamageReductionMod()
        // Covers: champion defensive abilities, summoner spells, debuffs
        // ====================================================================
        static float DamageReductionMod(const GameObject& source, const GameObject& target,
                                        float amount, DamageType damageType) {
            try {
                BuffManager targetBuffs(target.address);
                BuffManager sourceBuffs(source.address);

                // ----------------------------------------------------------------
                // Exhaust (SummonerExhaust) — Source deals 40% less damage
                // ----------------------------------------------------------------
                if (source.IsHero() && sourceBuffs.HasBuff("SummonerExhaust")) {
                    amount *= 0.6f;
                }

                // ----------------------------------------------------------------
                // Vladimir R — Target takes 10% more damage
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("vladimirhemoplaguedamageamp")) {
                    amount *= 1.1f;
                }

                // Only apply hero-specific reductions if target is a hero
                if (!target.IsHero()) return amount;

                int targetLevel = target.GetLevel();
                if (targetLevel < 1) targetLevel = 1;

                // ----------------------------------------------------------------
                // Alistar R — Unbreakable Will: 55/65/75% damage reduction
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("FerociousHowl")) {
                    float reductions[] = { 0.55f, 0.65f, 0.75f };
                    SpellBook sb(target.address);
                    if (sb.IsValid()) {
                        auto sp = sb.GetSpell(SpellSlotId::R);
                        int lvl = sp.IsValid() ? sp.GetLevel() : 1;
                        if (lvl >= 1 && lvl <= 3)
                            amount *= (1.0f - reductions[lvl - 1]);
                    }
                }

                // ----------------------------------------------------------------
                // Amumu E — Tantrum: reduces physical damage taken
                // 2/4/6/8/10 + 3% bonus armor + 3% bonus MR
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("Tantrum") && damageType == DamageType::Physical) {
                    SpellBook sb(target.address);
                    if (sb.IsValid()) {
                        auto sp = sb.GetSpell(SpellSlotId::E);
                        int lvl = sp.IsValid() ? sp.GetLevel() : 1;
                        if (lvl >= 1 && lvl <= 5) {
                            float flat[] = { 2.0f, 4.0f, 6.0f, 8.0f, 10.0f };
                            amount -= flat[lvl - 1]
                                + 0.03f * target.GetBonusArmor()
                                + 0.03f * target.GetBonusMR();
                        }
                    }
                }

                // ----------------------------------------------------------------
                // Braum E — Unbreakable: 30/32.5/35/37.5/40% damage reduction
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("braumeshieldbuff")) {
                    SpellBook sb(target.address);
                    if (sb.IsValid()) {
                        auto sp = sb.GetSpell(SpellSlotId::E);
                        int lvl = sp.IsValid() ? sp.GetLevel() : 1;
                        if (lvl >= 1 && lvl <= 5) {
                            float pct[] = { 0.30f, 0.325f, 0.35f, 0.375f, 0.40f };
                            amount *= (1.0f - pct[lvl - 1]);
                        }
                    }
                }

                // ----------------------------------------------------------------
                // Galio W — Shield of Durand: 20-40% damage reduction
                //   Magic: full reduction; Physical: half reduction
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("galiowbuff")) {
                    SpellBook sb(target.address);
                    if (sb.IsValid()) {
                        auto sp = sb.GetSpell(SpellSlotId::W);
                        int lvl = sp.IsValid() ? sp.GetLevel() : 1;
                        if (lvl >= 1 && lvl <= 5) {
                            float basePct[] = { 0.20f, 0.25f, 0.30f, 0.35f, 0.40f };
                            float pct = basePct[lvl - 1];
                            if (damageType == DamageType::Magical)
                                amount *= (1.0f - pct);
                            else if (damageType == DamageType::Physical)
                                amount *= (1.0f - pct * 0.5f);
                        }
                    }
                }

                // ----------------------------------------------------------------
                // Garen W — Courage: 60% first 0.75s, then 30%
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("GarenW")) {
                    // Simplified: average ~30% reduction (can't easily check start time)
                    amount *= 0.7f;
                }

                // ----------------------------------------------------------------
                // Gragas W — Drunken Rage: 10-18% + 4% per 100 AP
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("gragaswself")) {
                    SpellBook sb(target.address);
                    if (sb.IsValid()) {
                        auto sp = sb.GetSpell(SpellSlotId::W);
                        int lvl = sp.IsValid() ? sp.GetLevel() : 1;
                        if (lvl >= 1 && lvl <= 5) {
                            float pct[] = { 0.10f, 0.12f, 0.14f, 0.16f, 0.18f };
                            float totalPct = pct[lvl - 1] + 0.04f * target.GetAP() / 100.0f;
                            amount *= (1.0f - totalPct);
                        }
                    }
                }

                // ----------------------------------------------------------------
                // Irelia W — Defiant Dance: 50% + 7% per 100 AP physical reduction
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("ireliawdefense") && damageType == DamageType::Physical) {
                    float pct = 0.50f + 0.07f * target.GetAP() / 100.0f;
                    amount *= (1.0f - pct);
                }

                // ----------------------------------------------------------------
                // Kassadin P — Void Stone: 15% magic damage reduction
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("voidstone") && damageType == DamageType::Magical) {
                    amount *= 0.85f;
                }

                // ----------------------------------------------------------------
                // Master Yi W — Meditate: 60-70% damage reduction (50% for turrets)
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("Meditate")) {
                    SpellBook sb(target.address);
                    if (sb.IsValid()) {
                        auto sp = sb.GetSpell(SpellSlotId::W);
                        int lvl = sp.IsValid() ? sp.GetLevel() : 1;
                        if (lvl >= 1 && lvl <= 5) {
                            float pct[] = { 0.60f, 0.625f, 0.65f, 0.675f, 0.70f };
                            float mult = source.IsTurret() ? 0.5f : 1.0f;
                            amount *= (1.0f - pct[lvl - 1] * mult);
                        }
                    }
                }

                // ----------------------------------------------------------------
                // Warwick E — Primal Howl: 35-55% damage reduction
                // ----------------------------------------------------------------
                if (targetBuffs.HasBuff("WarwickE")) {
                    SpellBook sb(target.address);
                    if (sb.IsValid()) {
                        auto sp = sb.GetSpell(SpellSlotId::E);
                        int lvl = sp.IsValid() ? sp.GetLevel() : 1;
                        if (lvl >= 1 && lvl <= 5) {
                            float pct[] = { 0.35f, 0.40f, 0.45f, 0.50f, 0.55f };
                            amount *= (1.0f - pct[lvl - 1]);
                        }
                    }
                }

            } catch (...) {}

            return amount;
        }

        // ====================================================================
        // AutoAttackDamageOverrideMod — Turret vs minion damage
        // Port of EnsoulSharp Damage.cs::AutoAttackDamageOverrideMod()
        // Returns {true, overriddenDamage} if turret damage is overridden
        // ====================================================================
        struct AAOverrideResult {
            bool Override = false;
            float Damage = 0.0f;
        };

        static AAOverrideResult AutoAttackDamageOverrideMod(
            const GameObject& source, const GameObject& target, float amount)
        {
            AAOverrideResult result;

            if (!source.IsTurret()) return result;
            if (!target.IsMinion()) return result;

            try {
                float maxHP = target.GetMaxHealth();
                std::string minionName = target.GetName();

                // Melee minion: turret deals 45% of minion max HP
                if (minionName.find("Melee") != std::string::npos ||
                    minionName.find("MeleeMinion") != std::string::npos) {
                    result.Override = true;
                    result.Damage = 0.45f * maxHP;
                }
                // Ranged minion: turret deals 70% of minion max HP
                else if (minionName.find("Ranged") != std::string::npos ||
                         minionName.find("Wizard") != std::string::npos ||
                         minionName.find("CasterMinion") != std::string::npos) {
                    result.Override = true;
                    result.Damage = 0.70f * maxHP;
                }
                // Siege minion (Cannon): turret deals 14%/11%/8% depending on tier
                else if (minionName.find("Siege") != std::string::npos ||
                         minionName.find("Cannon") != std::string::npos) {
                    result.Override = true;
                    result.Damage = 0.14f * maxHP; // Tier 1 default
                }
                // Super minion: turret deals 5% of max HP
                else if (minionName.find("Super") != std::string::npos) {
                    result.Override = true;
                    result.Damage = 0.05f * maxHP;
                }
            } catch (...) {}

            return result;
        }

        // ====================================================================
        // Item IDs — Updated for Patch 26.5 (Season 2026)
        //
        // Sources:
        //   - Riot Data Dragon (stable IDs)
        //   - https://mobalytics.gg/lol/guides/new-lol-items-2026
        //   - Items marked TODO_ID: verify from Data Dragon / game memory
        // ====================================================================
        enum class ItemId : int {
            None = 0,

            // ============================================================
            // Sheen / Spellblade family
            // ============================================================
            Sheen               = 3057,  // Component: 100% base AD physical
            TrinityForce        = 3078,  // 200% base AD physical
            LichBane            = 3100,  // 75% base AD + 50% AP magic
            EssenceReaver       = 3508,  // 100% base AD + 40% bonus AD physical
            SunderedSky         = 6698,  // S14: 120% base AD + 6-10% target maxHP physical

            // NEW Season 2026 Spellblade:
            DuskAndDawn         = 8001,  // TODO_ID: verify — Built from Sheen, empowers AA after ability
                                         // 300 HP, 70 AP, 20 AH, 25% AS
                                         // Good on: Gwen, Diana, Ekko

            // ============================================================
            // On-Hit items (basic attacks deal bonus damage)
            // ============================================================
            BOTRK               = 3153,  // Blade of the Ruined King: 9%/6% current HP phys
            WitsEnd             = 3091,  // Fray: 15-80 magic on-hit (scales with level)
            NashorsTooth        = 3115,  // Icathian Bite: 15 + 20% AP magic on-hit
            RecurveBow          = 1043,  // Component: 15 physical on-hit
            Terminus            = 3302,  // Juxtaposition: 30 mixed on-hit (15p + 15m)
            TitanicHydra        = 3748,  // Colossus: 5 + 1.5% bonus HP physical on-hit

            // ============================================================
            // Energized items (charge from moving/attacking, proc on next AA)
            // ============================================================
            StatikkShiv         = 3087,  // S14: Electroshock 100-180 magic AoE
            Stormrazor          = 3095,  // S26 returned: Energized AA bonus damage + slow
                                         // 50 AD, 20% AS, 25% crit
                                         // Good on: Jhin, Vayne, Aphelios

            // ============================================================
            // Season 2026 NEW items (from mobalytics.gg)
            // ============================================================
            HextechGunblade     = 3146,  // S26 returned: 80 AP, 40 AD, 10% lifesteal
                                         // Passive: spell vamp
                                         // Active: deal damage + slow target
                                         // Good on: Katarina, Akali, Kayle

            FiendhunterBolts    = 8002,  // TODO_ID: verify — 40% AS, 25% crit, 4% MS
                                         // 30 ult AH, empowers next 3 AAs after ult
                                         // Good on: Yunara, Zeri, Twitch

            Bastionbreaker      = 8003,  // TODO_ID: verify — 55 AD, 22 lethality, 15 AH
                                         // Abilities deal extra true damage
                                         // Sabotage: takedown empowers AA vs turret/epic monster
                                         // Good on: Zed, Kha'Zix, Qiyana

            Actualizer          = 8004,  // TODO_ID: 90 AP, 300 mana, 10 AH (no on-hit)
            Bandlepipes         = 8005,  // TODO_ID: 200 HP, 15 AH, 20 armor/MR (no on-hit)
            EndlessHunger       = 8006,  // TODO_ID: 60 AD, 5% omnivamp, 20% tenacity (no on-hit)
            HexopticsC44        = 8007,  // TODO_ID: 50 AD, 25% crit (AA-focused)
            ProtoplasmaHarness  = 8008,  // TODO_ID: 600 HP, 15 AH (no on-hit)

            // ============================================================
            // Hydra family (cleave, not on-hit)
            // ============================================================
            RavenousHydra       = 3074,  // Cleave + omnivamp, no on-hit

            // ============================================================
            // REMOVED ITEMS — kept as comments for reference
            // ============================================================
            // DivineSunderer    = 6632,  // REMOVED Patch 14.1
            // IcebornGauntlet   = 6662,  // REMOVED Patch 14.1
            // GuinsoosRageblade = 3124,  // REMOVED Patch 14.1
            // KrakenSlayer      = 6672,  // Reworked S14, no longer on-hit
        };

        // ====================================================================
        // Get items equipped by a unit -> vector of item IDs
        // ====================================================================
        static std::vector<int> GetItems(const GameObject& unit) {
            std::vector<int> result;
            uintptr_t addr = unit.address;
            if (!Globals::IsValidPtr(addr)) return result;

            uintptr_t itemListBase = addr + Offset::GameObject::ItemList;

            for (int i = 0; i < 7; i++) {
                uintptr_t slotPtr = Globals::Read<uintptr_t>(itemListBase + i * 8);
                if (!Globals::IsValidPtr(slotPtr)) continue;

                uintptr_t infoPtr = Globals::Read<uintptr_t>(slotPtr + Offset::ItemSystem::SlotInfo);
                if (!Globals::IsValidPtr(infoPtr)) continue;

                uintptr_t dataPtr = Globals::Read<uintptr_t>(infoPtr + Offset::ItemSystem::InfoData);
                if (!Globals::IsValidPtr(dataPtr)) continue;

                int itemId = Globals::Read<int>(dataPtr + Offset::ItemSystem::DataItemId);
                if (itemId > 0)
                    result.push_back(itemId);
            }
            return result;
        }

        // Check if unit has specific item
        static bool HasItem(const GameObject& unit, int itemId) {
            auto items = GetItems(unit);
            for (int id : items)
                if (id == itemId) return true;
            return false;
        }

        static bool HasItem(const GameObject& unit, ItemId itemId) {
            return HasItem(unit, (int)itemId);
        }

        // ====================================================================
        // On-Hit Item Damage Calculation
        // Updated: Patch 26.5 — Season 2026
        //
        // Includes: S14 items + S26 new items (Stormrazor, Fiendhunter Bolts)
        // Reference: https://mobalytics.gg/lol/guides/new-lol-items-2026
        // ====================================================================
        static float GetItemOnHitDamage(const GameObject& source, const GameObject& target) {
            float totalDmg = 0.0f;
            auto items = GetItems(source);
            int sourceLevel = source.GetLevel();
            if (sourceLevel < 1) sourceLevel = 1;
            if (sourceLevel > 18) sourceLevel = 18;

            for (int itemId : items) {
                switch (itemId) {

                // ================================================================
                // BOTRK — Blade of the Ruined King (ID: 3153)
                // Passive: Mist's Edge
                //   Melee: 9% target's current HP as bonus physical damage
                //   Ranged: 6% target's current HP as bonus physical damage
                //   Minimum damage: 15
                //   Maximum vs monsters: 60
                // Status: Still in game Patch 26.5
                // ================================================================
                case (int)ItemId::BOTRK: {
                    float percent = source.IsMelee() ? 0.09f : 0.06f;
                    float bonusDmg = target.GetHealth() * percent;
                    if (bonusDmg < 15.0f) bonusDmg = 15.0f;
                    if (!target.IsHero() && bonusDmg > 60.0f) bonusDmg = 60.0f;
                    totalDmg += CalcPhysicalDamage(source, target, bonusDmg);
                    break;
                }

                // ================================================================
                // Wit's End (ID: 3091)
                // Passive: Fray
                //   15-80 bonus magic damage on-hit (scales with level)
                //   Formula: 15 + 65 * (level - 1) / 17
                // Status: Still in game Patch 26.5
                // ================================================================
                case (int)ItemId::WitsEnd: {
                    float baseDmg = 15.0f + 65.0f * ((float)(sourceLevel - 1) / 17.0f);
                    totalDmg += CalcMagicDamage(source, target, baseDmg);
                    break;
                }

                // ================================================================
                // Nashor's Tooth (ID: 3115)
                // Passive: Icathian Bite
                //   15 (+20% AP) bonus magic damage on-hit
                // Status: Still in game Patch 26.5
                // ================================================================
                case (int)ItemId::NashorsTooth: {
                    float bonusDmg = 15.0f + 0.20f * source.GetAP();
                    totalDmg += CalcMagicDamage(source, target, bonusDmg);
                    break;
                }

                // ================================================================
                // Recurve Bow (ID: 1043)
                // Passive: 15 bonus physical damage on-hit
                // Status: Still in game (component item)
                // ================================================================
                case (int)ItemId::RecurveBow: {
                    totalDmg += CalcPhysicalDamage(source, target, 15.0f);
                    break;
                }

                // ================================================================
                // Titanic Hydra (ID: 3748)
                // Passive: Colossus
                //   5 + 1.5% bonus HP as bonus physical damage on-hit
                // Status: Still in game Patch 26.5
                // ================================================================
                case (int)ItemId::TitanicHydra: {
                    // Bonus HP = MaxHP - BaseHP (base HP at level, not total)
                    // Approximate base HP: use level-based estimate
                    float maxHP = source.GetMaxHealth();
                    float baseHP = 600.0f + 95.0f * (float)(source.GetLevel() - 1); // Rough average
                    float bonusHP = maxHP - baseHP;
                    if (bonusHP < 0.0f) bonusHP = 0.0f;
                    float bonusDmg = 5.0f + bonusHP * 0.015f;
                    totalDmg += CalcPhysicalDamage(source, target, bonusDmg);
                    break;
                }

                // ================================================================
                // Terminus (ID: 3302)
                // Passive: Juxtaposition
                //   30 bonus damage on-hit (15 physical + 15 magic)
                //   Grants alternating stacks of armor/MR
                // Status: Still in game Patch 26.5
                // ================================================================
                case (int)ItemId::Terminus: {
                    totalDmg += CalcPhysicalDamage(source, target, 15.0f);
                    totalDmg += CalcMagicDamage(source, target, 15.0f);
                    break;
                }

                // ================================================================
                // Statikk Shiv (ID: 3087) — Energized
                // Passive: Electroshock
                //   When Energized: 100-180 bonus magic damage to target + 7 nearby
                //   Formula: 100 + 80 * (level - 1) / 17
                // NOTE: Only procs when fully charged. ~50% uptime estimate.
                // Status: Still in game Patch 26.5
                // ================================================================
                case (int)ItemId::StatikkShiv: {
                    float baseDmg = 100.0f + 80.0f * ((float)(sourceLevel - 1) / 17.0f);
                    totalDmg += CalcMagicDamage(source, target, baseDmg * 0.5f);
                    break;
                }

                // ================================================================
                // Stormrazor (ID: 3095) — NEW Season 2026 (returned)
                // Stats: 50 AD, 20% AS, 25% crit
                // Passive: Energized
                //   Moving generates stacks, fully charged next AA deals
                //   additional damage + bonus movement speed
                //   Estimated: 80-160 bonus physical damage (scales with level)
                //   Formula: 80 + 80 * (level - 1) / 17
                // NOTE: ~50% uptime. Values need verification from patch notes.
                // Source: https://mobalytics.gg/lol/guides/new-lol-items-2026
                // Good on: Jhin, Vayne, Aphelios
                // ================================================================
                case (int)ItemId::Stormrazor: {
                    float baseDmg = 80.0f + 80.0f * ((float)(sourceLevel - 1) / 17.0f);
                    totalDmg += CalcPhysicalDamage(source, target, baseDmg * 0.5f);
                    break;
                }

                // ================================================================
                // Hextech Gunblade (ID: 3146) — NEW Season 2026 (returned)
                // Stats: 80 AP, 40 AD, 10% lifesteal
                // Passive: Spell vamp on abilities
                // Active: Deal magic damage + slow target (point-and-click)
                //   Estimated: 175-250 magic damage active (scales with level)
                // NOTE: Active damage is NOT on-hit, but spell vamp IS passive
                //       We don't include active here — only passive lifesteal matters
                // Source: https://mobalytics.gg/lol/guides/new-lol-items-2026
                // Good on: Katarina, Akali, Kayle
                // ================================================================
                // Hextech Gunblade: no on-hit component, active is separate
                // Lifesteal (10%) already factored into sustain, not on-hit damage

                // ================================================================
                // Fiendhunter Bolts (ID: 8002 TODO_ID) — NEW Season 2026
                // Stats: 40% AS, 25% crit, 4% MS, 30 ult AH
                // Passive: Empowers next 3 basic attacks after casting ultimate
                //   Estimated: 30-60 bonus physical damage per empowered AA
                // NOTE: Conditional — only after using ult. Hard to factor in
                //       general on-hit calc. Include as ~33% average uptime.
                // Source: https://mobalytics.gg/lol/guides/new-lol-items-2026
                // Good on: Yunara, Zeri, Twitch
                // ================================================================
                case (int)ItemId::FiendhunterBolts: {
                    // ~33% uptime for empowered AAs (3 out of ~9 AAs per ult cycle)
                    float empoweredDmg = 40.0f + 20.0f * ((float)(sourceLevel - 1) / 17.0f);
                    totalDmg += CalcPhysicalDamage(source, target, empoweredDmg * 0.33f);
                    break;
                }

                default:
                    break;
                }
            }
            return totalDmg;
        }

        // ====================================================================
        // Sheen Proc Damage (Spellblade passive)
        // Only ONE Spellblade can be active at a time
        // Returns the HIGHEST priority Spellblade damage
        //
        // Updated: Patch 26.5
        //   Removed: Iceborn Gauntlet (6662), Divine Sunderer (6632)
        //   Added S14: Sundered Sky (6698)
        //   Added S26: Dusk and Dawn (Sheen AP bruiser)
        // ====================================================================
        static float GetSheenProcDamage(const GameObject& source, const GameObject& target) {
            auto items = GetItems(source);
            float baseAD = source.GetBaseAD();
            float bonusAD = source.GetTotalAD() - baseAD;

            // Priority: completed items first, then Sheen component
            for (int itemId : items) {
                switch (itemId) {

                // ================================================================
                // Trinity Force (ID: 3078)
                // Spellblade: 200% base AD bonus physical damage
                // CD: 1.5s
                // ================================================================
                case (int)ItemId::TrinityForce:
                    return CalcPhysicalDamage(source, target, baseAD * 2.0f);

                // ================================================================
                // Lich Bane (ID: 3100)
                // Spellblade: 75% base AD + 50% AP bonus magic damage
                // CD: 2.5s
                // ================================================================
                case (int)ItemId::LichBane: {
                    float dmg = baseAD * 0.75f + source.GetAP() * 0.50f;
                    return CalcMagicDamage(source, target, dmg);
                }

                // ================================================================
                // Dusk and Dawn (ID: 8001 TODO_ID) — NEW Season 2026
                // Built from: Sheen, Blasting Wand, Kindlegem, Dagger + 300g
                // Stats: 300 HP, 70 AP, 20 AH, 25% AS
                // Passive: Empowers next basic attack after using an ability
                //   Estimated: 100% base AD + 40% AP as mixed damage
                //   (AP bruiser Spellblade — like hybrid Lich Bane + Trinity)
                // Source: https://mobalytics.gg/lol/guides/new-lol-items-2026
                // Good on: Gwen, Diana, Ekko
                // ================================================================
                case (int)ItemId::DuskAndDawn: {
                    // AP bruiser Spellblade — estimated ratios
                    float dmg = baseAD + source.GetAP() * 0.40f;
                    return CalcMixedDamage(source, target, dmg);
                }

                // ================================================================
                // Essence Reaver (ID: 3508)
                // Spellblade: 100% base AD + 40% bonus AD bonus physical damage
                // CD: 1.5s
                // ================================================================
                case (int)ItemId::EssenceReaver: {
                    float dmg = baseAD + bonusAD * 0.40f;
                    return CalcPhysicalDamage(source, target, dmg);
                }

                // ================================================================
                // Sundered Sky (ID: 6698) — S14
                // Lightshield Strike: First attack vs champion
                //   120% base AD + (6-10% target max HP) physical damage
                // CD: Per-target, 8s
                // ================================================================
                case (int)ItemId::SunderedSky: {
                    if (target.IsHero()) {
                        float hpPercent = 0.06f + 0.04f * ((float)(source.GetLevel() - 1) / 17.0f);
                        float dmg = baseAD * 1.20f + target.GetMaxHealth() * hpPercent;
                        return CalcPhysicalDamage(source, target, dmg);
                    }
                    return CalcPhysicalDamage(source, target, baseAD * 1.20f);
                }

                // ================================================================
                // Sheen (ID: 3057) — Component
                // Spellblade: 100% base AD bonus physical damage
                // CD: 1.5s
                // ================================================================
                case (int)ItemId::Sheen:
                    return CalcPhysicalDamage(source, target, baseAD);

                default: break;
                }
            }
            return 0.0f;
        }

        // ====================================================================
        // Check if unit has any Spellblade item
        // ====================================================================
        static bool HasSheenItem(const GameObject& source) {
            auto items = GetItems(source);
            for (int itemId : items) {
                if (itemId == (int)ItemId::TrinityForce ||
                    itemId == (int)ItemId::LichBane ||
                    itemId == (int)ItemId::DuskAndDawn ||
                    itemId == (int)ItemId::EssenceReaver ||
                    itemId == (int)ItemId::SunderedSky ||
                    itemId == (int)ItemId::Sheen) {
                    return true;
                }
            }
            return false;
        }

        // ====================================================================
        // Hextech Gunblade Active Damage (point-and-click)
        // 80 AP, 40 AD, 10% lifesteal + spell vamp
        // Active: Deal magic damage + slow target
        // Estimated: 175-250 magic damage (scales with level + AP)
        // Source: https://mobalytics.gg/lol/guides/new-lol-items-2026
        // ====================================================================
        static float GetGunbladeActiveDamage(const GameObject& source, const GameObject& target) {
            if (!HasItem(source, ItemId::HextechGunblade)) return 0.0f;

            int sourceLevel = source.GetLevel();
            if (sourceLevel < 1) sourceLevel = 1;
            if (sourceLevel > 18) sourceLevel = 18;

            // Estimated: 175 + 75 * (level-1)/17 + 30% AP
            float baseDmg = 175.0f + 75.0f * ((float)(sourceLevel - 1) / 17.0f);
            float apScaling = source.GetAP() * 0.30f;
            return CalcMagicDamage(source, target, baseDmg + apScaling);
        }

        // ====================================================================
        // Bastionbreaker Sabotage Damage (on takedown, vs turret/epic monster)
        // 55 AD, 22 lethality, 15 AH
        // After takedown: empowered AA vs turret/epic monster deals bonus damage
        // Estimated: 200 + 50% AD true damage
        // Source: https://mobalytics.gg/lol/guides/new-lol-items-2026
        // ====================================================================
        static float GetBastionbreakerSabotageDamage(const GameObject& source) {
            if (!HasItem(source, ItemId::Bastionbreaker)) return 0.0f;
            return 200.0f + source.GetTotalAD() * 0.50f; // True damage
        }

        // ====================================================================
        // Auto Attack Damage (with optional item damage)
        // Includes: turret override, Ninja Tabi reduction, Fizz P
        // ====================================================================
        static float GetAutoAttackDamage(const GameObject& source, const GameObject& target,
                                         bool includeCrit = false, bool includeItems = false) {
            float totalAD = source.GetTotalAD();
            float damage = totalAD;

            // Turret vs minion override
            auto overrideResult = AutoAttackDamageOverrideMod(source, target, damage);
            if (overrideResult.Override) {
                return overrideResult.Damage;
            }

            if (includeCrit) {
                float critChance = source.GetCrit();
                float critMulti = source.GetCritMultiplier();
                if (critMulti <= 0.0f) critMulti = 1.75f;
                damage = totalAD * (1.0f + critChance * (critMulti - 1.0f));
            }

            float result = CalcPhysicalDamage(source, target, damage);

            // Ninja Tabi / Plated Steelcaps: 12% AA damage reduction
            if (target.IsHero()) {
                try {
                    if (HasItem(target, 3047)) { // Plated Steelcaps (formerly Ninja Tabi)
                        result *= 0.88f;
                    }
                } catch (...) {}
            }

            // Fizz P: reduce AA damage by 4 + 2 * floor((level-1)/3)
            if (target.IsHero()) {
                try {
                    std::string champName = target.GetChampionName();
                    if (_stricmp(champName.c_str(), "Fizz") == 0) {
                        int lvl = target.GetLevel();
                        result -= 4.0f + 2.0f * std::floor((float)(lvl - 1) / 3.0f);
                    }
                } catch (...) {}
            }

            if (includeItems) {
                result += GetItemOnHitDamage(source, target);
            }

            return std::max(result, 0.0f);
        }

        // Guaranteed crit damage
        static float GetCritDamage(const GameObject& source, const GameObject& target) {
            float critMulti = source.GetCritMultiplier();
            if (critMulti <= 0.0f) critMulti = 1.75f;
            return CalcPhysicalDamage(source, target, source.GetTotalAD() * critMulti);
        }

        // ====================================================================
        // Hits to kill (with items)
        // ====================================================================
        static int GetAutoAttacksToKill(const GameObject& source, const GameObject& target,
                                        bool includeItems = false) {
            float aaDmg = GetAutoAttackDamage(source, target, false, includeItems);
            if (aaDmg <= 0) return 999;
            return (int)std::ceil(target.GetHealth() / aaDmg);
        }

        // ====================================================================
        // Effective Health (raw damage needed to kill)
        // ====================================================================
        static float GetEffectiveHealth(const GameObject& source, const GameObject& target,
                                        DamageType type) {
            float testDmg = 100.0f;
            float actual = CalcDamage(source, target, type, testDmg);
            if (actual <= 0) return 999999.0f;
            float multiplier = actual / testDmg;
            return target.GetHealth() / multiplier;
        }

        // ====================================================================
        // Target Score (lower = easier to kill = better target)
        // ====================================================================
        static float GetTargetScore(const GameObject& source, const GameObject& target, DamageType type) {
            float effectiveHP = GetEffectiveHealth(source, target, type);
            float distance = source.DistanceTo(target);
            return effectiveHP + (distance / 1000.0f) * 50.0f;
        }

        // ====================================================================
        // Summoner Spell Damage — Updated Patch 26.5
        // ====================================================================

        // ----------------------------------------------------------------
        // Ignite (SummonerDot)
        // True damage over 5 seconds: 70 + 20 * level
        // Applies 25% Grievous Wounds
        // Source: wiki — formula unchanged
        // ----------------------------------------------------------------
        static float GetIgniteDamage(int sourceLevel) {
            if (sourceLevel < 1) sourceLevel = 1;
            if (sourceLevel > 18) sourceLevel = 18;
            return (float)(70 + 20 * sourceLevel);
        }

        // Overload for compat
        static float GetIgniteDamage(const GameObject& target, int sourceLevel) {
            return GetIgniteDamage(sourceLevel);
        }

        // ----------------------------------------------------------------
        // Smite (SummonerSmite)
        // True damage to MONSTERS ONLY (cannot target champions since S14)
        //   Level 1: 600 → Level 18: 900
        //   Formula: 600 + 300 * (level - 1) / 17
        // Primal Smite (upgraded): 1200 vs epic monsters
        // ----------------------------------------------------------------
        static float GetSmiteDamage(int sourceLevel, bool isEpicMonster = false) {
            if (sourceLevel < 1) sourceLevel = 1;
            if (sourceLevel > 18) sourceLevel = 18;

            if (isEpicMonster) return 1200.0f;
            return 600.0f + 300.0f * ((float)(sourceLevel - 1) / 17.0f);
        }

        // Overload for compat — uses local player level
        static float GetSmiteDamage(const GameObject& target, bool isChampion = false) {
            if (isChampion) return 0.0f;
            bool isEpic = false;
            try {
                std::string name = target.GetChampionName();
                isEpic = (name.find("Baron") != std::string::npos ||
                          name.find("Dragon") != std::string::npos ||
                          name.find("Herald") != std::string::npos ||
                          name.find("Grubs") != std::string::npos);
            } catch (...) {}
            return GetSmiteDamage(GameObjects::Player.GetLevel(), isEpic);
        }

        // ====================================================================
        // 7.3 Champion Passive On-Hit Damage
        // Calculates bonus damage from champion passives/abilities that
        // enhance auto-attacks.
        //
        // NOTE: This does NOT include ability-triggered procs (like Vayne W 3rd hit)
        // that require stack tracking. Only includes flat/scaling on-hit bonuses.
        //
        // Source: League of Legends wiki
        // ====================================================================
        static float GetChampionPassiveDamage(const GameObject& source, const GameObject& target) {
            std::string champName = source.GetChampionName();
            if (champName.empty()) return 0.0f;

            float totalDmg = 0.0f;
            int level = source.GetLevel();
            float totalAD = source.GetTotalAD();
            float bonusAD = source.GetBonusAD();
            float ap = source.GetAP();
            BuffManager buffs(source.address);
            BuffManager targetBuffs(target.address);

            // ----------------------------------------------------------------
            // Ashe — Passive: Frost Shot
            // After first AA on target, subsequent AAs deal 10 + (crit% * (1+bonus AS%)) bonus damage
            // Simplified: if target has FrostShot debuff, extra physical damage
            // ----------------------------------------------------------------
            if (_stricmp(champName.c_str(), "Ashe") == 0) {
                if (targetBuffs.HasBuff("ashepassiveslow")) {
                    float critChance = source.GetCritChance();
                    float bonusDmg = totalAD * (1.0f + critChance) * 0.1f;
                    totalDmg += CalcPhysicalDamage(source, target, bonusDmg);
                }
            }

            // ----------------------------------------------------------------
            // Braum — Passive: Concussive Blows
            // First mark AA deals 16 + 10 * level magic damage
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Braum") == 0) {
                if (targetBuffs.HasBuff("BraumMark")) {
                    int stacks = targetBuffs.HasBuff("BraumMarkCount") ? 3 : 0; // Simplified
                    if (stacks >= 3)
                        totalDmg += CalcMagicDamage(source, target, 16.0f + 10.0f * (float)level);
                }
            }

            // ----------------------------------------------------------------
            // Caitlyn — Passive: Headshot
            // Every 6th (ranged) / every other (from brush) AA deals bonus damage
            // Bonus: 60-160% AD (scales with level)
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Caitlyn") == 0) {
                if (buffs.HasBuff("caitlynheadshot")) {
                    float ratio = 0.6f + 0.059f * (float)(level - 1); // 60% at lv1, ~160% at lv18
                    totalDmg += CalcPhysicalDamage(source, target, totalAD * ratio);
                }
            }

            // ----------------------------------------------------------------
            // Diana — Passive: Moonsilver Blade
            // Every 3rd AA deals 20-250 + 50% AP magic damage
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Diana") == 0) {
                if (buffs.HasBuff("DianaPassiveMarker")) {
                    float baseDmg = 20.0f + (230.0f / 17.0f) * (float)(level - 1);
                    totalDmg += CalcMagicDamage(source, target, baseDmg + ap * 0.5f);
                }
            }

            // ----------------------------------------------------------------
            // Draven — Q: Spinning Axe (enhanced AA)
            // Bonus physical damage: 40/45/50/55/60 + 75-100% bonus AD
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Draven") == 0) {
                if (buffs.HasBuff("DravenSpinningAttack")) {
                    // Approximate: 50 + 80% bonus AD at mid rank
                    totalDmg += CalcPhysicalDamage(source, target, 50.0f + bonusAD * 0.85f);
                }
            }

            // ----------------------------------------------------------------
            // Ekko — Passive: Z-Drive Resonance
            // 3rd hit on same target deals 30-140 + 80% AP magic damage
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Ekko") == 0) {
                if (targetBuffs.HasBuff("eaboraliondamagetracker")) {
                    float baseDmg = 30.0f + (110.0f / 17.0f) * (float)(level - 1);
                    totalDmg += CalcMagicDamage(source, target, baseDmg + ap * 0.8f);
                }
            }

            // ----------------------------------------------------------------
            // Jax — Passive: Relentless Assault
            // Each consecutive AA on same target increases AS; not direct damage
            // W: Empower — next AA deals bonus magic damage
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Jax") == 0) {
                if (buffs.HasBuff("JaxEmpowerTwo")) {
                    // W bonus: 50/75/100/125/150 + 60% AP
                    totalDmg += CalcMagicDamage(source, target, 100.0f + ap * 0.6f);
                }
            }

            // ----------------------------------------------------------------
            // Kai'Sa — Passive: Second Skin
            // AAs apply Plasma stacks. 5th stack deals 15% + (2.5% per 100 AP) missing HP magic damage
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Kaisa") == 0) {
                if (targetBuffs.HasBuff("kaisapassivemarker")) {
                    // Simplified: per-stack damage
                    float missingHP = target.GetMaxHealth() - target.GetHealth();
                    float procDmg = missingHP * (0.15f + ap * 0.00025f);
                    totalDmg += CalcMagicDamage(source, target, procDmg);
                }
            }

            // ----------------------------------------------------------------
            // Kog'Maw — W: Bio-Arcane Barrage
            // While active, AAs deal 3/3.75/4.5/5.25/6% target max HP as magic damage
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "KogMaw") == 0) {
                if (buffs.HasBuff("KogMawBioArcaneBarrage")) {
                    float pct = 0.035f + ap * 0.0001f; // ~3.5% + 0.01% per AP
                    totalDmg += CalcMagicDamage(source, target, target.GetMaxHealth() * pct);
                }
            }

            // ----------------------------------------------------------------
            // Master Yi — E: Wuju Style
            // While active, AAs deal bonus true damage = 18-62 + 35% bonus AD
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "MasterYi") == 0) {
                if (buffs.HasBuff("wujustylesuperchargedvisual")) {
                    float baseDmg = 18.0f + (44.0f / 17.0f) * (float)(level - 1);
                    totalDmg += CalcTrueDamage(baseDmg + bonusAD * 0.35f);
                }
            }

            // ----------------------------------------------------------------
            // Nasus — Q: Siphoning Strike
            // Next AA deals bonus physical damage (enhanced by stacks)
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Nasus") == 0) {
                if (buffs.HasBuff("NasusQStacks")) {
                    // Can't easily read stacks, but Q base is 30-110
                    totalDmg += CalcPhysicalDamage(source, target, 70.0f);
                }
            }

            // ----------------------------------------------------------------
            // Teemo — E: Toxic Shot (passive)
            // AAs deal bonus magic damage: 10-50 + 30% AP on-hit
            // Plus 6-30 + 10% AP over 4 seconds (poison)
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Teemo") == 0) {
                // Teemo E passive is always active once learned
                float baseDmg = 10.0f + 10.0f * (float)((std::min)(level, 18) / 4);
                totalDmg += CalcMagicDamage(source, target, baseDmg + ap * 0.3f);
            }

            // ----------------------------------------------------------------
            // Twisted Fate — E: Stacked Deck
            // Every 4th AA deals 65-165 + 50% AP bonus magic damage
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "TwistedFate") == 0) {
                if (buffs.HasBuff("cardmaborsdamageindicator")) {
                    float baseDmg = 65.0f + (100.0f / 17.0f) * (float)(level - 1);
                    totalDmg += CalcMagicDamage(source, target, baseDmg + ap * 0.5f);
                }
            }

            // ----------------------------------------------------------------
            // Twitch — Passive: Deadly Venom
            // AAs apply poison stacks (1-5), each dealing 1/2/3/4/5 true damage per second
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Twitch") == 0) {
                // Per-hit poison (single stack damage over 6s)
                float poisonDmg = 1.0f + (float)(level / 4);
                totalDmg += CalcTrueDamage(poisonDmg * 6.0f); // Total for one stack
            }

            // ----------------------------------------------------------------
            // Vayne — W: Silver Bolts
            // Every 3rd consecutive AA on same target deals bonus true damage
            // = 4-14% target max HP (minimum 50-170 flat)
            // We average this over 3 hits for per-AA estimate
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Vayne") == 0) {
                // Averaged per-hit: procDmg / 3
                float pct = 0.04f + 0.006f * (float)((std::min)(level, 18) / 4);
                float procDmg = target.GetMaxHealth() * pct;
                float minDmg = 50.0f + 7.06f * (float)(level - 1);
                if (procDmg < minDmg) procDmg = minDmg;
                totalDmg += CalcTrueDamage(procDmg / 3.0f); // Average per-hit
            }

            // ----------------------------------------------------------------
            // Varus — W: Blighted Quiver (passive)
            // AAs deal 7-21 + 25% AP bonus magic damage on-hit
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Varus") == 0) {
                float baseDmg = 7.0f + (14.0f / 17.0f) * (float)(level - 1);
                totalDmg += CalcMagicDamage(source, target, baseDmg + ap * 0.25f);
            }

            // ----------------------------------------------------------------
            // Vi — W: Denting Blows
            // Every 3rd hit deals % max HP physical damage
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Vi") == 0) {
                // Averaged per-hit: ~4% maxHP / 3
                float procDmg = target.GetMaxHealth() * 0.04f;
                totalDmg += CalcPhysicalDamage(source, target, procDmg / 3.0f);
            }

            // ----------------------------------------------------------------
            // Zeri — Passive: Living Battery
            // AAs deal magic damage instead of physical
            // ----------------------------------------------------------------
            else if (_stricmp(champName.c_str(), "Zeri") == 0) {
                // Zeri's charged right-click: 10-25 + 3% max HP magic damage
                float baseDmg = 10.0f + (15.0f / 17.0f) * (float)(level - 1);
                totalDmg += CalcMagicDamage(source, target, baseDmg + target.GetMaxHealth() * 0.03f);
            }

            return totalDmg;
        }

        // ====================================================================
        // Callback hooks for DamagePassives and DamageMastery
        // These are set during Init() to avoid circular includes.
        // ====================================================================
        using DmgCallback = std::function<float(const GameObject&, const GameObject&)>;
        static inline DmgCallback s_passiveDmgCallback = nullptr;
        static inline DmgCallback s_runeDmgCallback    = nullptr;
        static inline DmgCallback s_runeMultCallback   = nullptr;

        static void SetPassiveDmgCallback(DmgCallback cb) { s_passiveDmgCallback = cb; }
        static void SetRuneDmgCallback(DmgCallback cb)    { s_runeDmgCallback = cb; }
        static void SetRuneMultCallback(DmgCallback cb)   { s_runeMultCallback = cb; }

        // ====================================================================
        // Full AA Damage (AA + items + champion passive + Sheen + Runes)
        // Automatically uses DamagePassives + DamageMastery callbacks if set.
        // ====================================================================
        static float GetFullAutoAttackDamage(const GameObject& source, const GameObject& target,
                                              bool includePassive = true) {
            float dmg = GetAutoAttackDamage(source, target, true, true);

            if (includePassive) {
                // Old inline passive (kept for fallback / simple champions)
                dmg += GetChampionPassiveDamage(source, target);

                // New: comprehensive 60+ champion passive system (via callback)
                if (s_passiveDmgCallback) {
                    try { dmg += s_passiveDmgCallback(source, target); } catch (...) {}
                }

                // New: rune/keystone on-hit damage (via callback)
                if (s_runeDmgCallback) {
                    try { dmg += s_runeDmgCallback(source, target); } catch (...) {}
                }
            }

            // Apply rune damage multipliers (Press the Attack, Coup de Grace, etc.)
            if (s_runeMultCallback) {
                try {
                    float mult = s_runeMultCallback(source, target);
                    if (mult > 1.0f) dmg *= mult;
                } catch (...) {}
            }

            return dmg;
        }
    };

} // namespace SDK
