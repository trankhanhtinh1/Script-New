#pragma once

#include "../../Enumerations/DamageType.h"
#include "../../Core/Objects.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace SDK::DamagePassives {

struct PassiveDamageResult {
    float Physical = 0.0f;
    float Magical  = 0.0f;
    float True_    = 0.0f;
    bool  Override = false; // if true, replaces base AA damage

    float Total() const { return Physical + Magical + True_; }
};

// ── Helpers ──

inline int Lvl(const AIHeroClient& h) { return std::max(1, std::min(18, h.Level())); }
inline int Idx(const AIHeroClient& h) { return Lvl(h) - 1; }
inline float Lerp18(float min, float max, const AIHeroClient& h) { return min + (max - min) / 17.0f * static_cast<float>(Idx(h)); }

// ── Crit multiplier (matching GetCritMultiplier in C#) ──
inline float GetCritMultiplier(const AIHeroClient& hero, bool checkCrit = false) {
    const bool hasIE = hero.HasBuff("InfinityEdge") || hero.HasItem(3031); // Infinity Edge
    const float critBonus = hasIE ? 1.25f : 1.0f;
    if (!checkCrit) return critBonus;
    return (std::abs(hero.Crit() - 1.0f) < 0.001f) ? (1.0f + critBonus) : 1.0f;
}

namespace detail {
    constexpr uint32_t ConstExprHashFNV1a(const char* str, size_t len) {
        uint32_t hash = 0x811C9DC5;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint32_t>(str[i]);
            hash *= 0x1000193u;
        }
        return hash;
    }
}

constexpr uint32_t operator"" _h(const char* str, size_t len) {
    return detail::ConstExprHashFNV1a(str, len);
}

inline uint32_t HashStringFNV1a(const std::string& str) {
    uint32_t hash = 0x811C9DC5;
    for (char c : str) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 0x1000193u;
    }
    return hash;
}

// ── Main function ──
inline PassiveDamageResult GetPassiveDamageDetails(const AIHeroClient& source,
                                                   const AIBaseClient& target) {
    PassiveDamageResult out = {};
    if (!source.IsValid() || !target.IsValid()) return out;

    const std::string name = source.CharacterName();
    const int level = Lvl(source);
    const int idx = Idx(source);

    // ════════════════════════════════════════════════
    // ITEM PASSIVES — Season 14+ (Patch 26.6)
    // ════════════════════════════════════════════════

    const bool isMelee = source.IsMelee();

    // ── Crit damage (generic, excludes special champs) ──
    if (name != "Ashe" && name != "Corki" && name != "Fiora" && name != "Galio" &&
        name != "Graves" && name != "Jayce" && name != "Jhin" && name != "Kayle" &&
        name != "Kled" && name != "Pantheon" && name != "Shaco" && name != "Urgot" &&
        name != "Yasuo" && name != "Yone" && name != "Zac" && name != "Zeri") {
        if (std::abs(source.Crit() - 1.0f) < 0.001f) {
            const float multiplier = (name == "Kalista") ? 0.9f : 1.0f;
            out.Physical += source.TotalAttackDamage() * multiplier * GetCritMultiplier(source);
        }
    }

    // ───────────────────────────────────────────────
    // GROUP A: SPELLBLADE (only highest applies)
    // ───────────────────────────────────────────────
    {
        float spellbladeDmgPhys = 0.0f;
        float spellbladeDmgMagic = 0.0f;
        bool hasSpellblade = false;

        // Sheen (3057) — 100% Base AD physical
        if (source.HasBuff("sheen") && source.HasItem(3057)) {
            spellbladeDmgPhys = std::max(spellbladeDmgPhys, source.BaseAttackDamage());
            hasSpellblade = true;
        }
        // Trinity Force (3078) — 200% Base AD physical
        if ((source.HasBuff("sheen") || source.HasBuff("TrinityForce")) && source.HasItem(3078)) {
            spellbladeDmgPhys = std::max(spellbladeDmgPhys, 2.0f * source.BaseAttackDamage());
            hasSpellblade = true;
        }
        // Iceborn Gauntlet (3025) — 100% Base AD physical
        if (source.HasBuff("itemfrozenfist") && source.HasItem(3025)) {
            spellbladeDmgPhys = std::max(spellbladeDmgPhys, source.BaseAttackDamage());
            hasSpellblade = true;
        }
        // Lich Bane (3100) — 100% Base AD + 50% AP magic
        if (source.HasBuff("lichbane") && source.HasItem(3100)) {
            spellbladeDmgMagic = std::max(spellbladeDmgMagic,
                1.0f * source.BaseAttackDamage() + 0.5f * source.TotalMagicalDamage());
            hasSpellblade = true;
        }
        // Bloodsong (3869) — 150% Base AD physical (support spellblade)
        if (source.HasBuff("sheen") && source.HasItem(3869)) {
            spellbladeDmgPhys = std::max(spellbladeDmgPhys, 1.5f * source.BaseAttackDamage());
            hasSpellblade = true;
        }

        if (hasSpellblade) {
            out.Physical += spellbladeDmgPhys;
            out.Magical += spellbladeDmgMagic;
        }
    }

    // ───────────────────────────────────────────────
    // GROUP B: ON-HIT EFFECTS (all stack)
    // ───────────────────────────────────────────────

    // Nashor's Tooth (3115) — 15 + 20% AP magic on-hit
    if (source.HasItem(3115)) {
        out.Magical += 15.0f + 0.20f * source.TotalMagicalDamage();
    }

    // Wit's End (3091) — 15-80 magic on-hit (scales with level)
    if (source.HasItem(3091)) {
        constexpr float witDmg[18] = {15,15,15,15,15,25,35,45,55,65,70,72,74,76,78,79,80,80};
        out.Magical += witDmg[idx];
    }

    // Blade of the Ruined King (3153) — 9% melee / 6% ranged current HP physical
    if (source.HasItem(3153)) {
        const float ratio = isMelee ? 0.09f : 0.06f;
        const float cap = target.IsMinion() ? 60.0f : 999999.0f;
        out.Physical += std::max(15.0f, std::min(cap, ratio * target.Health()));
    }

    // Recurve Bow (1043) — 15 physical on-hit
    if (source.HasItem(1043)) {
        out.Physical += 15.0f;
    }

    // Guinsoo's Rageblade (3124) — 30 magic on-hit (converts crit to on-hit)
    if (source.HasItem(3124)) {
        out.Magical += 30.0f;
    }

    // Terminus (3302) — 30 magic on-hit
    if (source.HasItem(3302)) {
        out.Magical += 30.0f;
    }

    // Kraken Slayer (6672) — every 3rd hit bonus physical
    if (source.HasItem(6672) && source.GetBuffCount("intothefray") >= 2) {
        // Base 35-85 + 60% bonus AD physical
        out.Physical += Lerp18(35.0f, 85.0f, source) + 0.60f * source.BonusAttackDamage();
    }

    // Titanic Hydra (3748) — on-hit: 1.5% maxHP melee / 0.75% maxHP ranged
    if (source.HasItem(3748)) {
        if (source.HasBuff("itemtitanichydracleavebuff")) {
            // Active reset: 5% maxHP (melee) / 2.5% maxHP (ranged)
            out.Physical += (isMelee ? 0.05f : 0.025f) * source.MaxHealth();
        } else {
            // Passive on-hit
            out.Physical += (isMelee ? 0.015f : 0.0075f) * source.MaxHealth();
        }
    }

    // Profane Hydra (3303) — Cleave passive: ~on-hit portion
    if (source.HasItem(3303)) {
        if (source.HasBuff("hydaborusactiveattack")) {
            out.Physical += 1.3f * source.TotalAttackDamage(); // Active: 130% tAD
        }
    }

    // ───────────────────────────────────────────────
    // GROUP C: ENERGIZED ATTACKS
    // ───────────────────────────────────────────────

    // Voltaic Cyclosword (6698) — Energized: 100 physical
    if (source.HasItem(6698) && source.GetBuffCount("itemstatikshankcharge") >= 100) {
        out.Physical += 100.0f;
    }

    // Rapid Firecannon (3094) — Energized: 60 magic
    if (source.HasItem(3094) && source.GetBuffCount("itemstatikshankcharge") >= 100) {
        out.Magical += 60.0f;
    }

    // Statikk Shiv (3087) — Chain lightning: 100-180 magic (scales with level)
    if (source.HasItem(3087) && source.GetBuffCount("itemstatikshankcharge") >= 100) {
        out.Magical += Lerp18(100.0f, 180.0f, source);
    }

    // ───────────────────────────────────────────────
    // GROUP D: PROC / CONDITIONAL PASSIVES
    // ───────────────────────────────────────────────

    // Eclipse (6692) — 2-hit proc: 8% melee / 4% ranged max HP physical
    if (source.HasItem(6692) && target.GetBuffCount("eclipsetargetdebuff") >= 1) {
        out.Physical += (isMelee ? 0.08f : 0.04f) * target.MaxHealth();
    }

    // Sundered Sky (6610) — First hit guaranteed crit (extra damage)
    if (source.HasItem(6610) && !target.HasBuff("6610debuff")) {
        // Guarantees 175% crit + heals. Extra physical ~ 75% base AD
        out.Physical += 0.75f * source.BaseAttackDamage();
    }

    // Heartsteel (3084) — Colossal Consumption charge attack
    if (source.HasItem(3084) && source.HasBuff("intothefray")) {
        // Note: BonusHealth not available, using MaxHealth as approximation
        out.Physical += 100.0f + 0.10f * source.MaxHealth();
    }

    // Hullbreaker (3181) — Enhanced 5th attack: 140% base AD physical
    if (source.HasItem(3181) && source.GetBuffCount("6035counter") >= 4) {
        out.Physical += 1.4f * source.BaseAttackDamage();
    }

    // Dead Man's Plate (3742) — Momentum at 100 stacks: 40 + 100% base AD physical
    if (source.HasBuff("dreadnoughtmomentumbuff")) {
        const int stacks = source.GetBuffCount("dreadnoughtmomentumbuff");
        if (stacks >= 100) {
            out.Physical += 40.0f + source.BaseAttackDamage();
        }
    }

    // Muramana (3042) — on-hit: 1.5% max mana physical
    if (source.HasBuff("Muramana") && source.ManaPercent() > 20.0f) {
        out.Physical += 0.015f * source.MaxMana();
    }

    // ───────────────────────────────────────────────
    // GROUP E: STARTER / MISC ITEMS
    // ───────────────────────────────────────────────

    // Doran's Ring (1056) — +5 damage vs minions
    if (source.HasItem(1056) && target.IsMinion() && target.IsEnemy()) {
        out.Physical += 5.0f;
    }

    // Doran's Shield (1054) — +5 damage vs minions
    if (source.HasItem(1054) && target.IsMinion() && target.IsEnemy()) {
        out.Physical += 5.0f;
    }

    // ════════════════════════════════════════════════
    // CHAMPION PASSIVES
    // ════════════════════════════════════════════════

    switch (HashStringFNV1a(name)) {
        case "Aatrox"_h:
            if (source.HasBuff("aatroxpassiveready")) {
                const float cap = (target.IsMinion() && !target.IsEnemy()) ? 400.0f : 999999.0f;
                out.Physical += std::min(cap, target.MaxHealth() * (0.04f + 0.08f * static_cast<float>(level) / 18.0f));
            }
            break;
        case "Akali"_h:
            if (source.HasBuff("akalipweapon")) {
                constexpr float dmg[18] = {35,38,41,44,47,50,53,56,65,74,83,92,101,116,131,146,162,182};
                out.Magical += dmg[idx] + 0.60f * source.BonusAttackDamage() + 0.55f * source.TotalMagicalDamage();
            }
            break;
        case "Alistar"_h:
            if (source.HasBuff("alistareattack"))
                out.Magical += 35.0f + 15.0f * idx;
            break;
        case "Aphelios"_h:
            if (source.HasBuff("apheliosseverumq")) out.Physical += 0.25f * source.BonusAttackDamage();
            break;
        case "Ashe"_h:
            if (target.HasBuff("ashepassiveslow"))
                out.Physical += (1.2f + source.Crit() * GetCritMultiplier(source)) * source.TotalAttackDamage() - source.TotalAttackDamage();
            break;
        case "Blitzcrank"_h:
            if (source.HasBuff("PowerFist"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Braum"_h:
            if (target.GetBuffCount("BraumMark") == 3)
                out.Magical += 26.0f + 10.0f * idx;
            break;
        case "Caitlyn"_h:
            if (source.HasBuff("caitlynheadshot") || target.HasBuff("caitlynyordletrapinternal")) {
                float headshotRatio = (level < 7) ? 0.5f : ((level < 13) ? 0.75f : 1.0f);
                float dmg = headshotRatio * source.TotalAttackDamage() + (1.3125f * source.Crit()) * source.TotalAttackDamage();
                out.Physical += dmg;
            }
            break;
        case "Camille"_h: {
            const int qLvl = std::max(0, std::min(4, source.GetSpell(SpellSlot::Q).Level() - 1));
            constexpr float qRatios[5] = {0.20f, 0.25f, 0.30f, 0.35f, 0.40f};
            if (source.HasBuff("CamilleQ"))
                out.Physical += qRatios[qLvl] * source.TotalAttackDamage();
            if (source.HasBuff("CamilleQ2")) {
                const float trueConv = std::min(1.0f, 0.4f + 0.04f * idx);
                const float totalDmg = 2.0f * qRatios[qLvl] * source.TotalAttackDamage();
                out.Physical += (1.0f - trueConv) * totalDmg;
                out.True_ += trueConv * totalDmg;
            }
            break;
        }
        case "Chogath"_h:
            if (source.HasBuff("VorpalSpikes"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Corki"_h:
            out.Magical += 0.8f * GetCritMultiplier(source) * source.TotalAttackDamage();
            break;
        case "Darius"_h:
            if (source.HasBuff("DariusNoxianTacticsONH"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Diana"_h:
            if (source.HasBuff("dianaarcready")) {
                constexpr float dmg[18] = {20,25,30,35,40,50,60,70,80,90,105,120,135,155,175,200,225,250};
                out.Magical += dmg[idx] + 0.50f * source.TotalMagicalDamage();
            }
            break;
        case "DrMundo"_h:
            if (source.HasBuff("Masochism"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Draven"_h:
            if (source.HasBuff("DravenSpinning"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Ekko"_h:
            if (target.GetBuffCount("ekkostacks") == 2)
                out.Magical += Lerp18(30.0f, 140.0f, source) + 0.8f * source.TotalMagicalDamage();
            if (source.HasBuff("ekkoeattackbuff"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Elise"_h:
            if (source.HasBuff("EliseR"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::R);
            break;
        case "Ezreal"_h:
            if (target.HasBuff("ezrealwattach"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Fiora"_h:
            if (std::abs(source.Crit() - 1.0f) < 0.001f && !source.HasBuff("FioraE") && !source.HasBuff("fiorae2"))
                out.Physical += static_cast<float>(GetCritMultiplier(source)) * source.TotalAttackDamage();
            if (source.HasBuff("fiorae2"))
                out.Physical += static_cast<float>(GetCritMultiplier(source)) * source.TotalAttackDamage();
            break;
        case "Fizz"_h:
            if (source.GetSpell(SpellSlot::W).Level() > 0)
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            if (source.HasBuff("FizzW"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Galio"_h:
            if (source.HasBuff("galiopassivebuff"))
                out.Magical += 12.0f + 4.0f * idx + source.TotalAttackDamage() + 0.5f * source.TotalMagicalDamage() + 0.4f * source.BonusSpellBlock();
            break;
        case "Gangplank"_h:
            if (source.HasBuff("gangplankpassiveattack")) {
                const float gpDmg = (55.0f + 10.0f * idx + source.BonusAttackDamage()) * (target.IsTurret() ? 0.5f : 1.0f);
                out.True_ += gpDmg;
            }
            break;
        case "Garen"_h:
            if (source.HasBuff("GarenQ"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            if (target.HasBuff("garenpassiveenemytarget"))
                out.True_ += 0.01f * target.MaxHealth();
            break;
        case "Gnar"_h:
            if (target.GetBuffCount("gnarwproc") == 2)
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Gragas"_h:
            if (source.HasBuff("gragaswattackbuff"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Gwen"_h:
            out.Magical += (0.01f + 0.008f * (source.TotalMagicalDamage() / 100.0f)) * target.MaxHealth();
            break;
        case "Hecarim"_h:
            if (source.HasBuff("hecarimrampspeed"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Illaoi"_h:
            if (source.HasBuff("IllaoiW"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Irelia"_h:
            if (source.GetBuffCount("ireliapassivestacks") >= 4)
                out.Magical += Lerp18(10.0f, 61.0f, source) + 0.25f * source.BonusAttackDamage();
            break;
        case "Ivern"_h:
            if (source.HasBuff("ivernwpassive"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "JarvanIV"_h:
            if (!target.HasBuff("jarvanivmartialcadencecheck"))
                out.Physical += std::min(400.0f, std::max(20.0f, 0.08f * target.Health()));
            break;
        case "Jax"_h:
            if (source.HasBuff("JaxEmpowerTwo"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Jayce"_h:
            if (source.HasBuff("JaycePassiveMeleeAttack")) {
                float jDmg = 0.25f * source.BonusAttackDamage();
                if (level >= 16) jDmg += 145.0f;
                else if (level >= 11) jDmg += 105.0f;
                else if (level >= 6) jDmg += 65.0f;
                else jDmg += 25.0f;
                out.Magical += jDmg;
            }
            break;
        case "Jhin"_h:
            if (source.HasBuff("jhinpassiveattackbuff")) {
                const float missingHpRatio = level >= 11 ? 0.25f : (level >= 6 ? 0.2f : 0.15f);
                out.Physical += missingHpRatio * (target.MaxHealth() - target.Health());
            }
            break;
        case "Jinx"_h:
            if (source.HasBuff("JinxQ"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q) * GetCritMultiplier(source, true);
            break;
        case "Kaisa"_h: {
            constexpr float baseDmg[18] = {4,4,4,5,5,6,6,6,7,7,7,8,8,8,9,9,10,10};
            float kDmg = baseDmg[idx];
            const int passiveCnt = target.GetBuffCount("kaisapassivemarker");
            if (passiveCnt > 0) {
                constexpr float stackDmg[18] = {1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5};
                kDmg += stackDmg[idx] * passiveCnt;
            }
            constexpr float apRatios[5] = {0.10f, 0.125f, 0.15f, 0.175f, 0.20f};
            kDmg += apRatios[std::min(4, std::max(0, passiveCnt))] * source.TotalMagicalDamage();
            out.Magical += kDmg;
            if (passiveCnt >= 3) {
                const float missingHp = std::max(0.0f, target.MaxHealth() - target.Health());
                const float capKai = target.IsMinion() ? 400.0f : 999999.0f;
                out.Magical += std::min(capKai, (0.15f + source.TotalMagicalDamage() / 100.0f * 0.025f) * missingHp);
            }
            break;
        }
        case "Kalista"_h:
            if (target.HasBuff("kalistacoopstrikemarkally"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Kassadin"_h:
            if (source.GetSpell(SpellSlot::W).Level() > 0)
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Kayle"_h:
            if (!source.HasBuff("KayleE") && std::abs(source.Crit() - 1.0f) < 0.001f)
                out.Physical += source.TotalAttackDamage() * GetCritMultiplier(source);
            if (source.GetSpell(SpellSlot::E).Level() > 0)
                out.Magical += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Kennen"_h:
            if (source.HasBuff("kennendoublestrikelive"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Khazix"_h:
            if (source.HasBuff("KhazixPDamage") && target.IsHero())
                out.Magical += Lerp18(14.0f, 150.0f, source) + 0.4f * source.BonusAttackDamage();
            break;
        case "Kled"_h:
            if (source.HasBuff("kledwactive"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "KogMaw"_h:
            if (source.HasBuff("KogMawBioArcaneBarrage"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "KSante"_h:
            if (target.HasBuff("ksantepassivemark"))
                out.Physical += 5.0f + 0.01f * target.MaxHealth();
            break;
        case "Leona"_h:
            if (target.HasBuff("LeonaSunlight"))
                out.Magical += 25.0f + 7.0f * idx;
            if (source.HasBuff("LeonaShieldOfDaybreak"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Lucian"_h:
            if (source.HasBuff("LucianPassiveBuff")) {
                float baseAdM = 0.5f + 0.05f * (idx / 4);
                out.Physical += baseAdM * source.TotalAttackDamage();
            }
            break;
        case "Lux"_h:
            if (target.HasBuff("LuxIlluminatingFraulein"))
                out.Magical += 20.0f + 10.0f * idx + 0.2f * source.TotalMagicalDamage();
            break;
        case "Malphite"_h:
            if (source.HasBuff("MalphiteCleave"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "MasterYi"_h:
            if (source.HasBuff("doublestrike"))
                out.Physical += 0.5f * source.TotalAttackDamage() * GetCritMultiplier(source, true);
            if (source.HasBuff("wujustylesuperchargedvisual"))
                out.True_ += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "MissFortune"_h:
            out.Physical += Lerp18(0.5f, 1.0f, source) * source.TotalAttackDamage();
            break;
        case "MonkeyKing"_h:
            if (source.HasBuff("MonkeyKingDoubleAttack"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Mordekaiser"_h:
            if (source.HasBuff("mordekaisermaceofspades") || source.HasBuff("mordekaisermaceofspades2"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Nasus"_h:
            if (source.HasBuff("NasusQ")) out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Nautilus"_h:
            if (!target.HasBuff("nautiluspassivecheck")) out.Physical += 8.0f + 6.0f * idx;
            if (source.HasBuff("nautiluspiercinggazeshield"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::W) / (target.IsHero() ? 1.0f : 2.0f);
            break;
        case "Neeko"_h:
            if (source.GetBuffCount("neekowpassivestack") == 2)
                out.Magical += Lerp18(50.0f, 170.0f, source) + 0.6f * source.TotalMagicalDamage();
            break;
        case "Nidalee"_h:
            if (source.HasBuff("Takedown"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Nilah"_h:
            if (source.Crit() > 0.0f)
                out.Physical += source.Crit() * 0.33f * source.TotalAttackDamage();
            break;
        case "Nocturne"_h:
            if (source.HasBuff("nocturneumbrablades"))
                out.Physical += 0.2f * source.TotalAttackDamage();
            break;
        case "Orianna"_h:
            out.Magical += Lerp18(10.0f, 50.0f, source) + 0.15f * source.TotalMagicalDamage();
            break;
        case "Ornn"_h:
            if (target.HasBuff("OrnnVulnerableDebuff"))
                out.Magical += Lerp18(0.12f, 0.205f, source) * target.MaxHealth();
            break;
        case "Pantheon"_h:
            if (target.HealthPercent() < 15.0f && source.GetSpell(SpellSlot::E).Level() > 0)
                out.Physical += GetCritMultiplier(source) * source.TotalAttackDamage();
            break;
        case "Poppy"_h:
            if (source.HasBuff("poppypassivebuff"))
                out.Magical += Lerp18(20.0f, 180.0f, source);
            break;
        case "Quinn"_h:
            if (target.HasBuff("QuinnW"))
                out.Physical += 10.0f + 5.0f * idx + (0.16f + 0.02f * idx) * source.TotalAttackDamage();
            break;
        case "Rammus"_h: {
            float rDmg = (std::min(20.0f, 8.0f + static_cast<float>(idx)) + 0.1f * source.Armor());
            if (source.HasBuff("DefensiveBallCurl")) rDmg *= 1.5f;
            out.Magical += rDmg;
            break;
        }
        case "RekSai"_h:
            if (source.HasBuff("RekSaiQ"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Renekton"_h:
            if (source.HasBuff("RenektonPreExecute"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Rengar"_h:
            if (source.HasBuff("RengarQ")) out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            if (source.HasBuff("RengarQEmp")) out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Riven"_h:
            if (source.HasBuff("RivenPassiveAABoost"))
                out.Physical += Lerp18(0.25f, 0.5f, source) * source.TotalAttackDamage();
            break;
        case "Rumble"_h:
            if (source.HasBuff("rumbleoverheat")) out.Magical += 25.0f + 5.0f * idx + 0.3f * source.TotalMagicalDamage();
            break;
        case "Samira"_h:
            if (source.DistanceSquared(target) <= 300 * 300)
                out.Magical += Lerp18(2.0f, 19.0f, source) + 0.04f * source.BonusAttackDamage();
            break;
        case "Sejuani"_h:
            if (target.HasBuff("sejuanistun")) {
                const float sejRatio = level >= 14 ? 0.20f : (level >= 7 ? 0.15f : 0.10f);
                out.Magical += sejRatio * target.MaxHealth();
            }
            break;
        case "Shen"_h:
            if (source.HasBuff("shenqbuffweak"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::Q);
            if (source.HasBuff("shenqbuffstrong"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Shyvana"_h:
            if (source.HasBuff("ShyvanaDoubleAttack") || source.HasBuff("ShyvanaDoubleAttackDragon"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            if (source.HasBuff("ShyvanaImmolationAura") || source.HasBuff("ShyvanaImmolateDragon"))
                out.Magical += 0.25f * source.GetSpellDamage(target, SpellSlot::W);
            if (target.HasBuff("ShyvanaFireballMissile"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Sion"_h:
            if (source.HasBuff("sionpassivezombie"))
                out.Physical += std::min(target.IsMinion() ? 75.0f : 999999.0f, 0.1f * target.MaxHealth());
            break;
        case "Skarner"_h:
            if (target.HasBuff("skarnerpassive3"))
                out.Magical += 0.05f * target.MaxHealth();
            break;
        case "Smolder"_h:
            if (source.GetBuffCount("smolderdragonpractice") > 0)
                out.True_ += 2.0f;
            break;
        case "Sona"_h:
            if (source.HasBuff("sonapassiveattack"))
                out.Magical += Lerp18(20.0f, 240.0f, source) + 0.2f * source.TotalMagicalDamage();
            break;
        case "Sylas"_h:
            if (source.HasBuff("SylasPassiveAttack")) {
                out.Magical += Lerp18(5.0f, 48.0f, source) + 1.0f * source.TotalAttackDamage() + 0.2f * source.TotalMagicalDamage();
                out.Override = true;
            }
            break;
        case "TahmKench"_h: {
            const int stacks = std::max(1, target.GetBuffCount("tahmkenchpdebuffcounter"));
            const float hpRatio = level >= 13 ? 0.0175f : (level >= 7 ? 0.015f : 0.0125f);
            out.Magical += stacks * hpRatio * source.MaxHealth();
            break;
        }
        case "Talon"_h:
            if (target.GetBuffCount("TalonPassiveStack") >= 3)
                out.Physical += 75.0f + 10.0f * idx + 2.0f * source.BonusAttackDamage();
            break;
        case "Taric"_h:
            if (source.HasBuff("TaricPassiveAttack"))
                out.Magical += 25.0f + 4.0f * idx + 0.15f * source.BonusArmor();
            break;
        case "Teemo"_h:
            out.Magical += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Tristana"_h:
            if (target.GetBuffCount("tristanaecharge") >= 3) out.Physical += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Trundle"_h:
            if (source.HasBuff("TrundleTrollSmash"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "TwistedFate"_h:
            if (source.HasBuff("BlueCardPreAttack") || source.HasBuff("RedCardPreAttack") || source.HasBuff("GoldCardPreAttack")) {
                out.Magical += source.GetSpellDamage(target, SpellSlot::W);
                out.Override = true;
            }
            if (source.HasBuff("cardmasterstackparticle"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Twitch"_h:
            out.True_ += static_cast<float>(std::min(std::max(target.GetBuffCount("TwitchDeadlyVenom"), 0) + 1, 6)) * Lerp18(1.0f, 5.0f, source);
            break;
        case "Udyr"_h:
            if (source.GetBuffCount("UdyrTigerStance") >= 3)
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Varus"_h:
            out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Vayne"_h:
            if (source.HasBuff("vaynetumblebonus"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            if (target.GetBuffCount("VayneSilveredDebuff") == 2)
                out.True_ += source.GetSpellDamage(target, SpellSlot::W);
            break;
        case "Vi"_h:
            if (target.GetBuffCount("viwproc") == 2) out.Physical += source.GetSpellDamage(target, SpellSlot::W);
            if (source.HasBuff("ViE")) out.Physical += GetCritMultiplier(source, true) * source.GetSpellDamage(target, SpellSlot::E);
            break;
        case "Viego"_h:
            if (target.HasBuff("viegoqmark")) out.Physical += 0.20f * source.TotalAttackDamage();
            out.Physical += 0.02f * target.Health();
            break;
        case "Viktor"_h:
            if (source.HasBuff("ViktorPowerTransferReturn"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Volibear"_h:
            if (source.HasBuff("VolibearQ"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            if (source.HasBuff("volibearrapplicator"))
                out.Magical += source.GetSpellDamage(target, SpellSlot::R);
            break;
        case "Warwick"_h:
            out.Magical += 10.0f + 2.0f * idx;
            break;
        case "Xayah"_h:
            if (source.HasBuff("XayahW"))
                out.Physical += 0.2f * source.TotalAttackDamage();
            break;
        case "XinZhao"_h:
            if (source.GetBuffCount("XinZhaoPTracker") >= 3) out.Physical += Lerp18(0.15f, 0.45f, source) * source.TotalAttackDamage();
            if (source.HasBuff("XinZhaoQ")) out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Yasuo"_h:
            if (std::abs(source.Crit() - 1.0f) < 0.001f)
                out.Physical += 0.9f * GetCritMultiplier(source) * source.TotalAttackDamage();
            break;
        case "Yone"_h:
            if (source.HasBuff("yonepassivedoublehit")) {
                out.Physical *= 0.5f;
                out.Magical += out.Physical;
            }
            break;
        case "Yorick"_h:
            if (source.HasBuff("yorickqbuff"))
                out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
            break;
        case "Zed"_h:
            if (target.HealthPercent() < 50.0f && !target.HasBuff("zedpassivecd")) out.Magical += Lerp18(0.06f, 0.10f, source) * target.MaxHealth();
            break;
        case "Zeri"_h:
            out.Magical += Lerp18(90.0f, 200.0f, source);
            break;
        case "Ziggs"_h:
            if (source.HasBuff("ZiggsShortFuse")) out.Magical += Lerp18(20.0f, 160.0f, source) + 0.4f * source.TotalMagicalDamage();
            break;
        case "Zoe"_h:
            if (source.HasBuff("zoepassivesheenbuff")) out.Magical += Lerp18(10.0f, 124.0f, source) + 0.2f * source.TotalMagicalDamage();
            break;
    }

    return out;
}

inline float GetPassiveDamage(const AIHeroClient& source, const AIBaseClient& target) {
    const auto detail = GetPassiveDamageDetails(source, target);
    return source.CalculatePhysicalDamage(target, detail.Physical)
         + source.CalculateMagicDamage(target, detail.Magical)
         + detail.True_;
}

} // namespace SDK::DamagePassives
