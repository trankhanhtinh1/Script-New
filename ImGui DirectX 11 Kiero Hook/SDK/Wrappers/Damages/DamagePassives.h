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
// DamagePassives — Champion Passive On-Hit Damage Database (60+ champions)
// Reference: EnsoulSharp.SDK/Core/Wrappers/Damages/DamagePassives.cs
// Updated: 2026-03-06 — Patch 26.S1 (Season 2026)
//
// Sources:
//   https://leagueoflegends.fandom.com/wiki/
//   https://www.leagueoflegends.com/en-us/news/tags/patch-notes/
//
// Usage:
//   float passiveDmg = SDK::DamagePassives::GetPassiveDamage(source, target);
//   bool overrides   = SDK::DamagePassives::DoesOverrideAA(source, target);
//
// NOTE: Buff names may change between patches. Verify in-game with buff viewer.
// ============================================================================

namespace SDK {

    class DamagePassives {
    public:
        // ====================================================================
        // PassiveEntry — One on-hit passive damage entry
        // ====================================================================
        struct PassiveEntry {
            std::string ChampionName;    // Empty = global (any champion)
            DamageType  Type;
            bool        IgnoreCalc;      // true = raw damage, skip CalcDamage
            bool        Override;        // true = replaces normal AA damage

            // Condition: returns true if this passive is active
            std::function<bool(const GameObject&, const GameObject&, const BuffManager&, const BuffManager&)> Condition;

            // DamageFunc: returns raw damage amount
            std::function<float(const GameObject&, const GameObject&, const BuffManager&, const BuffManager&)> DamageFunc;
        };

        // ====================================================================
        // Init — Build the passive database
        // ====================================================================
        static void Init() {
            if (s_initialized) return;
            s_initialized = true;
            CreatePassives();
        }

        // ====================================================================
        // GetPassiveDamage — Total passive on-hit damage for a source vs target
        // Returns post-mitigation damage
        // ====================================================================
        static float GetPassiveDamage(const GameObject& source, const GameObject& target) {
            if (!s_initialized) Init();

            std::string champName = source.GetChampionName();
            BuffManager srcBuffs(source.address);
            BuffManager tgtBuffs(target.address);
            float totalDmg = 0.0f;

            // Check global passives (empty champion name)
            auto git = s_entries.find("");
            if (git != s_entries.end()) {
                for (auto& e : git->second) {
                    try {
                        if (e.Condition && e.Condition(source, target, srcBuffs, tgtBuffs)) {
                            float raw = e.DamageFunc ? e.DamageFunc(source, target, srcBuffs, tgtBuffs) : 0.0f;
                            if (e.IgnoreCalc)
                                totalDmg += raw;
                            else
                                totalDmg += DamageCalc::CalcDamage(source, target, e.Type, raw);
                        }
                    } catch (...) {}
                }
            }

            // Check champion-specific passives
            if (!champName.empty()) {
                auto it = s_entries.find(champName);
                if (it != s_entries.end()) {
                    for (auto& e : it->second) {
                        try {
                            if (e.Condition && e.Condition(source, target, srcBuffs, tgtBuffs)) {
                                float raw = e.DamageFunc ? e.DamageFunc(source, target, srcBuffs, tgtBuffs) : 0.0f;
                                if (e.IgnoreCalc)
                                    totalDmg += raw;
                                else
                                    totalDmg += DamageCalc::CalcDamage(source, target, e.Type, raw);
                            }
                        } catch (...) {}
                    }
                }
            }

            return totalDmg;
        }

        // ====================================================================
        // DoesOverrideAA — Check if any active passive overrides normal AA damage
        // (e.g., Ashe Q, TF W/E picks, Blitz E, etc.)
        // ====================================================================
        static bool DoesOverrideAA(const GameObject& source, const GameObject& target) {
            if (!s_initialized) Init();

            std::string champName = source.GetChampionName();
            BuffManager srcBuffs(source.address);
            BuffManager tgtBuffs(target.address);

            auto checkList = [&](const std::string& key) -> bool {
                auto it = s_entries.find(key);
                if (it == s_entries.end()) return false;
                for (auto& e : it->second) {
                    if (!e.Override) continue;
                    try {
                        if (e.Condition && e.Condition(source, target, srcBuffs, tgtBuffs))
                            return true;
                    } catch (...) {}
                }
                return false;
            };

            return checkList("") || (!champName.empty() && checkList(champName));
        }

    private:
        static inline bool s_initialized = false;
        static inline std::unordered_map<std::string, std::vector<PassiveEntry>> s_entries;

        // Helper to add entry
        static void Add(const std::string& champ, DamageType type, bool ignoreCalc, bool override_,
            std::function<bool(const GameObject&, const GameObject&, const BuffManager&, const BuffManager&)> cond,
            std::function<float(const GameObject&, const GameObject&, const BuffManager&, const BuffManager&)> dmgFn) {
            PassiveEntry e;
            e.ChampionName = champ;
            e.Type = type;
            e.IgnoreCalc = ignoreCalc;
            e.Override = override_;
            e.Condition = cond;
            e.DamageFunc = dmgFn;
            s_entries[champ].push_back(e);
        }

        // Shorthand helpers
        static int Clamp(int v, int lo, int hi) { return (std::max)(lo, (std::min)(hi, v)); }
        static float Lerp(float a, float b, int level) { return a + (b - a) * (float)(Clamp(level, 1, 18) - 1) / 17.0f; }

        // ====================================================================
        // CreatePassives — Register all champion passive entries
        // Updated for Season 2026 (Patch 26.S1)
        // ====================================================================
        static void CreatePassives() {
            s_entries.clear();

            // ================================================================
            // GLOBAL PASSIVES (apply to any champion with matching items/buffs)
            // ================================================================

            // Energized: Kircheis Shard / Statikk Shiv / RFC / Stormrazor
            Add("", DamageType::Magical, false, false,
                [](const GameObject& h, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("itemstatikshankcharge"); // 100 stacks
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    int lv = Clamp(h.GetLevel(), 1, 18);
                    bool hasShiv = DamageCalc::HasItem(h, DamageCalc::ItemId::StatikkShiv);
                    bool hasRFC = DamageCalc::HasItem(h, 3094); // Rapid Firecannon
                    bool hasStorm = DamageCalc::HasItem(h, DamageCalc::ItemId::Stormrazor);
                    float d0 = DamageCalc::HasItem(h, 2015) ? 50.0f : 0.0f; // Kircheis Shard
                    float d1 = hasShiv ? Lerp(100.0f, 180.0f, lv) : 0.0f;
                    float d2 = hasRFC ? Lerp(60.0f, 140.0f, lv) : 0.0f;
                    float d3 = hasStorm ? Lerp(80.0f, 160.0f, lv) : 0.0f;
                    return (std::max)({d0, d1, d2, d3});
                });

            // Muramana: 6% current mana as bonus physical on-hit
            Add("", DamageType::Physical, false, false,
                [](const GameObject& h, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("Muramana") && h.GetManaPercent() > 20.0f;
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.06f * h.GetMana();
                });

            // Duskblade of Draktharr: 30-150 bonus physical on-hit from stealth/unseen
            Add("", DamageType::Physical, false, false,
                [](const GameObject& h, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("itemdusknightstalkerdamageproc");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(30.0f, 150.0f, h.GetLevel());
                });

            // ================================================================
            // CHAMPION-SPECIFIC PASSIVES (60+ champions)
            // ================================================================

            // --- Aatrox: Deathbringer Stance ---
            // Periodically, AA deals 4-12% target max HP bonus physical
            Add("Aatrox", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("aatroxpassiveready");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    float pct = 0.04f + 0.047f * (float)(Clamp(h.GetLevel(), 1, 18) - 1) / 17.0f;
                    float dmg = t.GetMaxHealth() * pct;
                    if (!t.IsHero()) dmg = (std::min)(dmg, 400.0f);
                    return dmg;
                });

            // --- Akali: Assassin's Mark ---
            // After ability, next AA deals bonus magic damage
            Add("Akali", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("akalipweapon");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    static const float baseDmg[] = {39,42,45,48,51,54,57,60,69,78,87,96,105,120,135,150,165,180};
                    int idx = Clamp(h.GetLevel(), 1, 18) - 1;
                    return baseDmg[idx] + 0.6f * h.GetBonusAD() + 0.55f * h.GetAP();
                });

            // --- Alistar: Trample (E) enhanced AA ---
            Add("Alistar", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("alistareattack");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 35.0f + 15.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1);
                });

            // --- Ashe: Frost Shot passive ---
            // Subsequent AAs on slowed target deal 10% + (crit%) bonus damage
            Add("Ashe", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("ashepassiveslow");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return (0.1f + h.GetCrit()) * h.GetTotalAD();
                });

            // --- Bard: Meeps ---
            Add("Bard", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("bardpspiritammocount");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 40.0f + 0.3f * h.GetAP(); // + chime scaling (simplified)
                });

            // --- Blitzcrank: Power Fist (E) ---
            Add("Blitzcrank", DamageType::Physical, true, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("PowerFist");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return h.GetTotalAD() * 2.5f + 0.25f * h.GetAP(); // E doubles AA + AP
                });

            // --- Braum: Concussive Blows (passive stun proc) ---
            Add("Braum", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("BraumMark"); // 4th stack procs
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 26.0f + 10.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1);
                });

            // --- Caitlyn: Headshot ---
            Add("Caitlyn", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager& tb) {
                    return sb.HasBuff("caitlynheadshot") || tb.HasBuff("caitlynyordletrapinternal");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    int lv = h.GetLevel();
                    float mult = lv >= 13 ? 1.0f : (lv >= 7 ? 0.75f : 0.5f);
                    float dmg = (mult + 1.25f * h.GetCrit()) * h.GetTotalAD();
                    if (!t.IsHero()) dmg = (1.0f + 1.25f * h.GetCrit()) * h.GetTotalAD();
                    return dmg;
                });

            // --- Camille: Precision Protocol (Q1) ---
            Add("Camille", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("CamilleQ");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.3f * h.GetTotalAD(); // ~Q1 bonus (20-40% AD)
                });

            // --- Camille: Precision Protocol (Q2 — true damage portion) ---
            Add("Camille", DamageType::True, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("CamilleQ2");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    float ratio = 0.4f + 0.04f * (float)(Clamp(h.GetLevel(), 1, 18) - 1) / 17.0f;
                    return ratio * 0.6f * h.GetTotalAD(); // True damage portion of Q2
                });

            // --- Corki: Hextech Munitions (50% magic, 50% physical AA) ---
            // Corki's AA deals 80% as magic, 20% as physical
            Add("Corki", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // Always active for Corki
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.8f * h.GetTotalAD(); // 80% magic portion
                });

            // --- Darius: Crippling Strike (W) ---
            Add("Darius", DamageType::Physical, true, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("DariusNoxianTacticsONH");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return h.GetTotalAD() * 1.4f; // W: 140% total AD
                });

            // --- Diana: Moonsilver Blade (every 3rd AA) ---
            Add("Diana", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("dianaarcready");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    static const float baseDmg[] = {20,25,30,35,40,50,60,70,80,90,105,120,135,155,175,200,225,250};
                    int idx = Clamp(h.GetLevel(), 1, 18) - 1;
                    return baseDmg[idx] + 0.5f * h.GetAP();
                });

            // --- Draven: Spinning Axe (Q) ---
            Add("Draven", DamageType::Physical, true, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("DravenSpinning");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 50.0f + h.GetBonusAD() * 0.85f; // Q bonus ~45/50/55/60/65 + 75-100% bAD
                });

            // --- Ekko: Z-Drive Resonance (3rd hit) ---
            Add("Ekko", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("ekkostacks"); // 2 stacks = next hit procs
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    static const float baseDmg[] = {30,40,50,60,70,80,85,90,95,100,105,110,115,120,125,130,135,140};
                    int idx = Clamp(h.GetLevel(), 1, 18) - 1;
                    float dmg = baseDmg[idx] + 0.8f * h.GetAP();
                    if (!t.IsHero()) dmg = (std::min)(dmg, 600.0f); // Monster cap
                    return dmg;
                });

            // --- Fizz: Seastone Trident (W passive) ---
            Add("Fizz", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("FizzW") || sb.HasBuff("fizzonhitbuff");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 20.0f + 0.35f * h.GetAP(); // W on-hit
                });

            // --- Galio: Winds of War (passive AA) ---
            Add("Galio", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("galiopassivebuff");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    float base = 12.0f + 4.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1);
                    return base + h.GetTotalAD() + 0.5f * h.GetAP() + 0.4f * h.GetMR();
                });

            // --- Gangplank: Trial by Fire (passive) ---
            Add("Gangplank", DamageType::True, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("gangplankpassiveattack");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 55.0f + 10.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1) + h.GetBonusAD();
                });

            // --- Garen: Q (Decisive Strike) ---
            Add("Garen", DamageType::Physical, true, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("GarenQ");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return h.GetTotalAD() + 60.0f + 0.5f * h.GetTotalAD(); // Q: 30/60/90/120/150 + 50% AD
                });

            // --- Garen: Judgment passive (villain mark) ---
            Add("Garen", DamageType::True, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("garenpassiveenemytarget");
                },
                [](const GameObject&, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return 0.01f * t.GetMaxHealth(); // 1% max HP true damage
                });

            // --- Gnar: Hyper (W, 3rd hit) ---
            Add("Gnar", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("gnarwproc"); // 2 stacks = next procs
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return (std::max)(10.0f, t.GetMaxHealth() * 0.06f) + 0.5f * h.GetAP(); // 6-14% max HP
                });

            // --- Gragas: Drunken Rage (W) ---
            Add("Gragas", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("gragaswattackbuff");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return 20.0f + t.GetMaxHealth() * 0.07f + 0.07f * h.GetAP(); // W: 20/50/80/110/140 + 7% HP
                });

            // --- Illaoi: Harsh Lesson (W) ---
            Add("Illaoi", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("IllaoiW");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return (std::max)(10.0f, t.GetMaxHealth() * 0.04f); // 3-5% max HP
                });

            // --- Irelia: Ionian Fervor (passive, 5 stacks) ---
            Add("Irelia", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("ireliapassivestacks"); // at max stacks
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(15.0f, 66.0f, h.GetLevel()) + 0.25f * h.GetBonusAD();
                });

            // --- Jarvan IV: Martial Cadence ---
            Add("JarvanIV", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return !tb.HasBuff("jarvanivmartialcadencecheck");
                },
                [](const GameObject&, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return (std::min)(400.0f, (std::max)(20.0f, 0.08f * t.GetHealth()));
                });

            // --- Jax: Empower (W) ---
            Add("Jax", DamageType::Magical, true, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("JaxEmpowerTwo");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 75.0f + 0.6f * h.GetAP(); // W: 50/75/100/125/150 + 60% AP
                });

            // --- Jhin: 4th Shot ---
            Add("Jhin", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("jhinpassiveattackbuff");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    int lv = h.GetLevel();
                    float missingHpPct = lv >= 11 ? 0.25f : (lv >= 6 ? 0.2f : 0.15f);
                    return missingHpPct * (t.GetMaxHealth() - t.GetHealth());
                });

            // --- Jinx: Pow-Pow Rockets (Q toggle) ---
            Add("Jinx", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("JinxQ"); // Fishbones (rocket) mode
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.1f * h.GetTotalAD(); // 10% AD bonus per rocket (AoE)
                });

            // --- Kai'Sa: Second Skin (passive plasma) ---
            Add("Kaisa", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // Always has on-hit magic damage
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager& tb) {
                    int lv = Clamp(h.GetLevel(), 1, 18);
                    float base = lv >= 17 ? 10.0f : (lv >= 14 ? 9.0f : (lv >= 11 ? 8.0f :
                                 (lv >= 9 ? 7.0f : (lv >= 6 ? 6.0f : (lv >= 3 ? 5.0f : 4.0f)))));
                    base += 0.15f * h.GetAP();
                    // 5th stack proc: 15-25% missing HP
                    if (tb.HasBuff("kaisapassivemarker")) {
                        float missingHP = t.GetMaxHealth() - t.GetHealth();
                        base += (0.15f + h.GetAP() * 0.00025f) * missingHP;
                    }
                    return base;
                });

            // --- Kassadin: Nether Blade (W) ---
            Add("Kassadin", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("NetherBlade") || true; // W passive always active
                },
                [](const GameObject& h, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    if (sb.HasBuff("NetherBlade"))
                        return 50.0f + 0.8f * h.GetAP(); // W active: 50/75/100/125/150 + 80% AP
                    return 20.0f + 0.1f * h.GetAP(); // W passive: 20 + 10% AP
                });

            // --- Kayle: Starfire Spellblade (E passive) ---
            Add("Kayle", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // E passive always adds magic on-hit once learned
                },
                [](const GameObject& h, const GameObject& t, const BuffManager& sb, const BuffManager&) {
                    float base = 15.0f + 0.1f * h.GetAP() + 0.1f * h.GetBonusAD();
                    if (sb.HasBuff("KayleE")) // E active: execute missing HP
                        base += (t.GetMaxHealth() - t.GetHealth()) * 0.08f;
                    return base;
                });

            // --- Kennen: Electrical Surge (W passive, every 4th AA) ---
            Add("Kennen", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("kennendoublestrikelive");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 35.0f + 0.8f * h.GetBonusAD() + 0.35f * h.GetAP();
                });

            // --- Kha'Zix: Unseen Threat (passive, from stealth) ---
            Add("Khazix", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject& t, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("KhazixPDamage") && t.IsHero();
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 14.0f + 8.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1) + 0.4f * h.GetBonusAD();
                });

            // --- Kog'Maw: Bio-Arcane Barrage (W active) ---
            Add("KogMaw", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("KogMawBioArcaneBarrage");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    float pct = 0.035f + 0.01f * h.GetAP() / 100.0f;
                    return t.GetMaxHealth() * pct; // 3.5/4.5/5.5/6.5/7.5% + AP scaling
                });

            // --- Leona: Sunlight (passive, proc'd by allies) ---
            Add("Leona", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("LeonaSunlight");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 25.0f + 7.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1);
                });

            // --- Lucian: Lightslinger (passive, double-shot) ---
            Add("Lucian", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("LucianPassiveBuff");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    bool isMinion = !t.IsHero();
                    int lv = h.GetLevel();
                    float mult = isMinion ? 1.0f : (lv >= 13 ? 0.6f : (lv >= 7 ? 0.55f : 0.5f));
                    return h.GetTotalAD() * mult;
                });

            // --- Lux: Illumination ---
            Add("Lux", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("LuxIlluminatingFraulein");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 20.0f + 10.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1) + 0.2f * h.GetAP();
                });

            // --- Master Yi: Double Strike (passive, every other hit) ---
            Add("MasterYi", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("doublestrike");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.5f * h.GetTotalAD();
                });

            // --- Master Yi: Wuju Style (E active, true damage) ---
            Add("MasterYi", DamageType::True, true, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("wujustylesuperchargedvisual");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(18.0f, 62.0f, h.GetLevel()) + 0.35f * h.GetBonusAD();
                });

            // --- Miss Fortune: Love Tap (passive, new target bonus) ---
            Add("MissFortune", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // Approximation — fires on new target
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    int lv = h.GetLevel();
                    float mult = lv >= 13 ? 1.0f : (lv >= 11 ? 0.9f : (lv >= 9 ? 0.8f :
                                 (lv >= 7 ? 0.7f : (lv >= 4 ? 0.6f : 0.5f))));
                    float dmg = h.GetTotalAD() * mult;
                    if (!t.IsHero()) dmg *= 0.5f;
                    return dmg;
                });

            // --- Nasus: Siphoning Strike (Q, with stacks) ---
            Add("Nasus", DamageType::Physical, true, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("NasusQ");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return h.GetTotalAD() + 70.0f; // Q base + estimated stacks (simplified)
                });

            // --- Nautilus: Staggering Blow (passive, root + bonus) ---
            Add("Nautilus", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return !tb.HasBuff("nautiluspassivecheck");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 8.0f + 6.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1);
                });

            // --- Neeko: Shapesplitter (W, empowered AA every 3rd) ---
            Add("Neeko", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("neekowpassiveready");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 50.0f + 0.6f * h.GetAP(); // W: 50/70/90/110/130 + 60% AP
                });

            // --- Nocturne: Umbra Blades (passive, AoE AA) ---
            Add("Nocturne", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("nocturneumbrablades");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.2f * h.GetTotalAD();
                });

            // --- Orianna: Clockwork Windup (passive AA) ---
            Add("Orianna", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // Always active
                },
                [](const GameObject& h, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    int lv = h.GetLevel();
                    float base = lv >= 16 ? 50.0f : (lv >= 13 ? 42.0f : (lv >= 10 ? 34.0f :
                                 (lv >= 7 ? 26.0f : (lv >= 4 ? 18.0f : 10.0f))));
                    base += 0.15f * h.GetAP();
                    // Stacking: +20% per repeat on same target (max 2 stacks)
                    return base;
                });

            // --- Ornn: Brittle (passive proc on enemy with Brittle) ---
            Add("Ornn", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("OrnnVulnerableDebuff");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    float pct = Lerp(0.12f, 0.205f, h.GetLevel());
                    return pct * t.GetMaxHealth();
                });

            // --- Poppy: Iron Ambassador (passive, ranged AA) ---
            Add("Poppy", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("poppypassivebuff");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(20.0f, 180.0f, h.GetLevel());
                });

            // --- Quinn: Harrier (passive mark proc) ---
            Add("Quinn", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("QuinnW");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    int lv = h.GetLevel();
                    float base = 10.0f + 5.0f * (float)(lv - 1);
                    float adRatio = 0.16f + 0.02f * (float)(lv - 1);
                    return base + adRatio * h.GetTotalAD();
                });

            // --- Rammus: Spiked Shell (passive, based on armor) ---
            Add("Rammus", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true;
                },
                [](const GameObject& h, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    float base = (std::min)(20.0f, 8.0f + (float)(h.GetLevel() - 1)) + 0.1f * h.GetArmor();
                    if (sb.HasBuff("DefensiveBallCurl")) base *= 1.5f;
                    return base;
                });

            // --- Rek'Sai: Queen's Wrath (Q, enhanced AA) ---
            Add("RekSai", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("RekSaiQ");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 21.0f + 0.5f * h.GetBonusAD(); // Q on-hit: 21/27/33/39/45 + 50% bAD
                });

            // --- Renekton: Ruthless Predator (W) ---
            Add("Renekton", DamageType::Physical, true, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("RenektonPreExecute");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return h.GetTotalAD() * 1.5f + 15.0f; // W: 2 hits at 5/15/25/35/45 + 75% AD each
                });

            // --- Rengar: Savagery (Q) ---
            Add("Rengar", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("RengarQ") || sb.HasBuff("RengarQEmp");
                },
                [](const GameObject& h, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    float base = sb.HasBuff("RengarQEmp") ? 40.0f + 0.4f * h.GetTotalAD()
                                                           : 30.0f + 0.3f * h.GetTotalAD();
                    return base;
                });

            // --- Riven: Runic Blade (passive, charges from abilities) ---
            Add("Riven", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("RivenPassiveAABoost");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    int lv = h.GetLevel();
                    float mult = lv >= 18 ? 0.5f : (lv >= 15 ? 0.45f : (lv >= 12 ? 0.4f :
                                 (lv >= 9 ? 0.35f : (lv >= 6 ? 0.3f : 0.25f))));
                    return mult * h.GetTotalAD();
                });

            // --- Sejuani: Permafrost (E proc, frozen shatter) ---
            Add("Sejuani", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("sejuanistun");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    int lv = h.GetLevel();
                    float pct = lv >= 14 ? 0.2f : (lv >= 7 ? 0.15f : 0.1f);
                    return (std::min)(300.0f, pct * t.GetMaxHealth());
                });

            // --- Sett: Knuckle Down (passive alternating punches) ---
            Add("Sett", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // Right punch always does more
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    // Right punch bonus (averaged): ~5-30 + 10-25% bonus AD
                    return Lerp(5.0f, 30.0f, h.GetLevel()) + 0.15f * h.GetBonusAD();
                });

            // --- Shaco: Backstab (passive, from behind) ---
            Add("Shaco", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("Deceive");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 25.0f + 0.4f * h.GetBonusAD() + 0.3f * h.GetAP(); // Q from stealth
                });

            // --- Shen: Spirit Blade (Q enhanced AA) ---
            Add("Shen", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("shenqbuffweak") || sb.HasBuff("shenqbuffstrong");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager& sb, const BuffManager&) {
                    float pct = sb.HasBuff("shenqbuffstrong") ? 0.06f : 0.035f;
                    return pct * t.GetMaxHealth() + 10.0f; // Q on-hit % max HP
                });

            // --- Shyvana: Twin Bite (Q, double hit) ---
            Add("Shyvana", DamageType::Physical, true, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("ShyvanaDoubleAttack") || sb.HasBuff("ShyvanaDoubleAttackDragon");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return h.GetTotalAD() * 1.2f; // Q: second hit at 20/35/50/65/80% AD
                });

            // --- Sion: Glory in Death (zombie passive) ---
            Add("Sion", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("sionpassivezombie");
                },
                [](const GameObject&, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return (std::min)(75.0f, 0.1f * t.GetMaxHealth());
                });

            // --- Sona: Power Chord (passive, every 3rd ability) ---
            Add("Sona", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("sonapassiveattack");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    static const float baseDmg[] = {20,30,40,50,60,70,80,90,105,120,135,150,165,180,195,210,225,240};
                    return baseDmg[Clamp(h.GetLevel(), 1, 18) - 1] + 0.2f * h.GetAP();
                });

            // --- Sylas: Petricite Burst (passive AA after ability) ---
            Add("Sylas", DamageType::Magical, false, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("SylasPassiveAttack");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(5.0f, 48.0f, h.GetLevel()) + 1.0f * h.GetTotalAD() + 0.2f * h.GetAP();
                });

            // --- Tahm Kench: An Acquired Taste (passive stacking) ---
            Add("TahmKench", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true;
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    int lv = h.GetLevel();
                    float pct = lv >= 13 ? 0.0175f : (lv >= 7 ? 0.015f : 0.0125f);
                    return pct * h.GetMaxHealth();
                });

            // --- Talon: Blade's End (passive, 3 stacks wound) ---
            Add("Talon", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("TalonPassiveStack"); // 3 stacks
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 75.0f + 10.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1) + 2.0f * h.GetBonusAD();
                });

            // --- Taric: Bravado (passive, empowered AA after ability) ---
            Add("Taric", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("TaricPassiveAttack");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 25.0f + 4.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1) + 0.15f * h.GetArmor();
                });

            // --- Teemo: Toxic Shot (E passive) ---
            Add("Teemo", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // Always active once E learned
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    float base = Lerp(10.0f, 50.0f, h.GetLevel());
                    float onHit = base + 0.3f * h.GetAP();
                    float poison = (base * 0.6f + 0.1f * h.GetAP()) * (t.IsHero() ? 4.0f : 1.0f);
                    return onHit + poison; // Total: on-hit + 4s poison
                });

            // --- Tristana: Explosive Charge (E, 4 stacks detonation) ---
            Add("Tristana", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("tristanaecharge");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    // Per stack bonus (simplified): ~25% E damage per AA
                    return 25.0f + 0.25f * h.GetBonusAD();
                });

            // --- Trundle: Chomp (Q, enhanced AA) ---
            Add("Trundle", DamageType::Physical, true, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("TrundleTrollSmash");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return h.GetTotalAD() * 1.1f + 20.0f; // Q: 20/40/60/80/100 + 10-40% AD
                });

            // --- Twisted Fate: Stacked Deck (E, every 4th AA) ---
            Add("TwistedFate", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("cardmasterstackparticle");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(65.0f, 165.0f, h.GetLevel()) + 0.5f * h.GetAP();
                });

            // --- Twitch: Deadly Venom (passive, poison stacks) ---
            Add("Twitch", DamageType::True, true, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true;
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager& tb) {
                    int lv = h.GetLevel();
                    float perStack = lv >= 17 ? 5.0f : (lv >= 13 ? 4.0f : (lv >= 9 ? 3.0f : (lv >= 5 ? 2.0f : 1.0f)));
                    // Approximate 1 new stack per AA, existing stacks ticking
                    return perStack * (t.IsHero() ? 6.0f : 1.0f);
                });

            // --- Varus: Blighted Quiver (W passive) ---
            Add("Varus", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // W passive always active
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(7.0f, 21.0f, h.GetLevel()) + 0.25f * h.GetAP();
                });

            // --- Vayne: Tumble (Q) ---
            Add("Vayne", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("vaynetumblebonus");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.65f * h.GetTotalAD(); // Q: 60-100% bonus AD (estimated ~65%)
                });

            // --- Vayne: Silver Bolts (W, 3rd hit true damage) ---
            Add("Vayne", DamageType::True, true, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("VayneSilveredDebuff"); // 2 stacks = next procs
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    float pct = 0.04f + 0.006f * (float)((std::min)(h.GetLevel(), 18) / 4);
                    float dmg = t.GetMaxHealth() * pct;
                    return (std::max)(dmg, Lerp(50.0f, 170.0f, h.GetLevel()));
                });

            // --- Viego: Sovereign's Domination ---
            Add("Viego", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // Passive on-hit
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return t.GetHealth() * 0.02f; // 2% current HP on-hit
                });

            // --- Vi: Denting Blows (W, 3rd hit) ---
            Add("Vi", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("viwproc"); // 2 stacks = next procs
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return (std::max)(10.0f, t.GetMaxHealth() * 0.04f); // 4-10% max HP
                });

            // --- Viktor: Discharge (Q empowered AA) ---
            Add("Viktor", DamageType::Magical, true, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("ViktorPowerTransferReturn");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 20.0f + 0.5f * h.GetAP(); // Q empowered AA: 20/45/70/95/120 + 50% AP
                });

            // --- Volibear: Thundering Smash (Q) ---
            Add("Volibear", DamageType::Physical, true, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("VolibearQ");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return h.GetTotalAD() + 20.0f + 1.2f * h.GetBonusAD(); // Q: 20/40/60/80/100 + 120% bAD
                });

            // --- Warwick: Jaws of the Beast (passive, magic on-hit) ---
            Add("Warwick", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true;
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 10.0f + 2.0f * (float)(Clamp(h.GetLevel(), 1, 18) - 1);
                });

            // --- Xin Zhao: Three Talon Strike (Q, 3rd hit knockup) ---
            Add("XinZhao", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("XinZhaoQ");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 20.0f + 0.4f * h.GetBonusAD(); // Q: 16/25/34/43/52 + 40% bAD per hit
                });

            // --- Xin Zhao: Determination (passive, 3rd hit heal) ---
            Add("XinZhao", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("XinZhaoPTracker"); // 3rd hit
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    int lv = h.GetLevel();
                    float mult = lv >= 16 ? 0.45f : (lv >= 11 ? 0.35f : (lv >= 6 ? 0.25f : 0.15f));
                    return mult * h.GetTotalAD();
                });

            // --- Yasuo: Steel Tempest critical modifier ---
            Add("Yasuo", DamageType::Physical, false, false,
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return h.GetCrit() >= 0.99f;
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.9f * 1.75f * h.GetTotalAD() - h.GetTotalAD(); // Net bonus from crit
                });

            // --- Yone: Way of the Hunter (passive, magic damage portion) ---
            Add("Yone", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // Passive: every other AA deals bonus magic
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 0.5f * h.GetTotalAD(); // 2nd AA: 50% AD as magic (simplified average)
                });

            // --- Yorick: Last Rites (Q) ---
            Add("Yorick", DamageType::Physical, true, true,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("yorickqbuff");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return h.GetTotalAD() + 30.0f + 0.4f * h.GetTotalAD(); // Q: 30/55/80/105/130 + 40% AD
                });

            // --- Zed: Contempt for the Weak (passive, <50% HP target) ---
            Add("Zed", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject& t, const BuffManager&, const BuffManager& tb) {
                    return t.GetHealthPercent() < 50.0f && !tb.HasBuff("zedpassivecd");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    int lv = h.GetLevel();
                    float pct = lv >= 17 ? 0.1f : (lv >= 7 ? 0.08f : 0.06f);
                    return pct * t.GetMaxHealth();
                });

            // --- Zeri: Living Battery (passive, charged AA) ---
            Add("Zeri", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true;
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return Lerp(10.0f, 25.0f, h.GetLevel()) + t.GetMaxHealth() * 0.03f;
                });

            // --- Ziggs: Short Fuse (passive, empowered AA) ---
            Add("Ziggs", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("ZiggsShortFuse");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    static const float baseDmg[] = {20,24,28,32,36,40,48,56,64,72,80,88,100,112,124,136,148,160};
                    int idx = Clamp(h.GetLevel(), 1, 18) - 1;
                    float pct = h.GetLevel() >= 13 ? 0.5f : (h.GetLevel() >= 7 ? 0.4f : 0.3f);
                    float dmg = baseDmg[idx] + pct * h.GetAP();
                    if (t.IsTurret()) dmg *= 2.0f;
                    return dmg;
                });

            // --- Zoe: More Sparkles (passive, after ability) ---
            Add("Zoe", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("zoepassivesheenbuff");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    static const float baseDmg[] = {10,12,16,20,24,28,34,40,46,52,60,68,76,84,94,104,114,124};
                    return baseDmg[Clamp(h.GetLevel(), 1, 18) - 1] + 0.2f * h.GetAP();
                });

            // --- Gwen: Snip Snip! (passive, % HP magic on-hit) ---
            Add("Gwen", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true;
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    float dmg = 0.01f * t.GetMaxHealth() + 0.15f * h.GetAP(); // Thousand Cuts
                    if (!t.IsHero()) dmg = (std::min)(dmg, 10.0f + 0.25f * h.GetAP());
                    return dmg;
                });

            // --- Bel'Veth: Death in Lavender (passive, attack stacking) ---
            Add("Belveth", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true;
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    // Above average AS: extra damage scales
                    return h.GetBonusAD() * 0.1f; // Simplified extra on-hit
                });

            // --- K'Sante: Ntofo Strikes (passive mark proc) ---
            Add("KSante", DamageType::Magical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager& tb) {
                    return tb.HasBuff("ksantepassivestack"); // 3rd hit proc
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    float pct = 0.05f; // ~5% max HP magic damage
                    return pct * t.GetMaxHealth();
                });

            // --- Briar: Blood Frenzy (W, enhanced AA) ---
            Add("Briar", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("briarwself");
                },
                [](const GameObject& h, const GameObject& t, const BuffManager&, const BuffManager&) {
                    return 0.05f * (t.GetMaxHealth() - t.GetHealth()) + 0.3f * h.GetBonusAD(); // Frenzy bonus
                });

            // --- Ambessa: Drakehound's Step (passive on-hit after dash) ---
            Add("Ambessa", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager& sb, const BuffManager&) {
                    return sb.HasBuff("ambessapassiveready");
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return Lerp(20.0f, 100.0f, h.GetLevel()) + 0.6f * h.GetBonusAD();
                });

            // --- Smolder: Super Scorcher Breath (Q as AA replacement) ---
            Add("Smolder", DamageType::Physical, false, false,
                [](const GameObject&, const GameObject&, const BuffManager&, const BuffManager&) {
                    return true; // Q replaces AA, stacking passive
                },
                [](const GameObject& h, const GameObject&, const BuffManager&, const BuffManager&) {
                    return 15.0f + 0.5f * h.GetBonusAD(); // Simplified per-AA bonus from stacks
                });
        }
    };

} // namespace SDK
