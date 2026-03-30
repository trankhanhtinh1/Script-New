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

// ── Main function ──
inline PassiveDamageResult GetPassiveDamageDetails(const AIHeroClient& source,
                                                   const AIBaseClient& target) {
    PassiveDamageResult out = {};
    if (!source.IsValid() || !target.IsValid()) return out;

    const std::string name = source.CharacterName();
    const int level = Lvl(source);
    const int idx = Idx(source);

    // ════════════════════════════════════════════════
    // ITEM PASSIVES (global, string.Empty in C#)
    // ════════════════════════════════════════════════

    // -- Crit damage (generic, excludes special champs) --
    if (name != "Ashe" && name != "Corki" && name != "Fiora" && name != "Galio" &&
        name != "Graves" && name != "Jayce" && name != "Jhin" && name != "Kayle" &&
        name != "Kled" && name != "Pantheon" && name != "Shaco" && name != "Urgot" &&
        name != "Yasuo" && name != "Zac") {
        if (std::abs(source.Crit() - 1.0f) < 0.001f) {
            const float multiplier = (name == "Kalista") ? 0.9f : 1.0f;
            out.Physical += source.TotalAttackDamage() * multiplier * GetCritMultiplier(source);
        }
    }

    // -- Sheen / Trinity / Iceborn --
    if (source.HasBuff("sheen")) {
        float d1 = source.HasItem(3057) ? source.BaseAttackDamage() : 0.0f;  // Sheen
        float d2 = source.HasItem(3078) ? 2.0f * source.BaseAttackDamage() : 0.0f; // Trinity
        out.Physical += std::max(d1, d2);
    }
    if (source.HasBuff("itemfrozenfist")) { // Iceborn Gauntlet
        out.Physical += source.BaseAttackDamage();
    }
    if (source.HasBuff("TrinityForce")) {
        out.Physical += 2.0f * source.BaseAttackDamage();
    }

    // -- Lich Bane --
    if (source.HasBuff("lichbane")) {
        out.Magical += 0.75f * source.BaseAttackDamage() + 0.5f * source.TotalMagicalDamage();
    }

    // -- Nashor's Tooth --
    if (source.HasItem(3115)) { // Nashor's Tooth
        out.Magical += 15.0f + 0.15f * source.TotalMagicalDamage();
    }

    // -- Wit's End --
    if (source.HasItem(3091)) { // Wit's End
        out.Magical += 15.0f + 65.0f / 17.0f * static_cast<float>(idx);
    }

    // -- BotRK --
    if (source.HasItem(3153) || source.HasItem(3144)) { // BotRK / Bilgewater
        const float limit = target.IsMinion() ? 60.0f : 999999.0f;
        out.Physical += std::max(15.0f, std::min(limit, 0.08f * target.Health()));
    }

    // -- Recurve Bow --
    if (source.HasItem(1043)) {
        out.Physical += 15.0f;
    }

    // -- Guinsoo's Rageblade --
    if (source.HasItem(3124)) {
        out.Magical += 15.0f;
    }

    // -- Doran's Ring (vs minions) --
    if (source.HasItem(1056) && target.IsMinion() && target.IsEnemy()) {
        out.Physical += 5.0f;
    }

    // -- Doran's Shield (vs minions) --
    if (source.HasItem(1054) && target.IsMinion() && target.IsEnemy()) {
        out.Physical += 5.0f;
    }

    // -- Titanic Hydra --
    if (source.HasItem(3748)) {
        if (source.HasBuff("itemtitanichydracleavebuff"))
            out.Physical += 40.0f + 0.1f * source.MaxHealth();
        else
            out.Physical += 5.0f + 0.01f * source.MaxHealth();
    }

    // -- Muramana --
    if (source.HasBuff("Muramana") && source.ManaPercent() > 20.0f) {
        out.Physical += 0.06f * source.Mana();
    }

    // -- Duskblade --
    if (source.HasBuff("itemdusknightstalkerdamageproc")) {
        out.Physical += Lerp18(30.0f, 150.0f, source);
    }

    // -- Statikk Shiv charge --
    if (source.GetBuffCount("itemstatikshankcharge") >= 100) {
        constexpr float shivBase[18] = {60,60,60,60,60,67,73,79,85,91,97,104,110,116,122,128,134,140};
        out.Magical += shivBase[idx] * GetCritMultiplier(source, true);
    }

    // -- Dead Man's Plate --
    if (source.HasBuff("dreadnoughtmomentumbuff")) {
        out.Magical += static_cast<float>(source.GetBuffCount("dreadnoughtmomentumbuff"));
    }

    // -- Kircheis Shard / Rapid Firecannon / Stormrazor (handled by statikk check above mostly) --

    // ════════════════════════════════════════════════
    // CHAMPION PASSIVES
    // ════════════════════════════════════════════════

    if (name == "Aatrox") {
        if (source.HasBuff("aatroxpassiveready")) {
            const float cap = (target.IsMinion() && !target.IsEnemy()) ? 400.0f : 999999.0f;
            out.Physical += std::min(cap, target.MaxHealth() * (0.08f + 0.0047f * static_cast<float>(idx)));
        }
    }
    else if (name == "Akali") {
        if (source.HasBuff("akalipweapon")) {
            constexpr float dmg[18] = {39,42,45,48,51,54,57,60,69,78,87,96,105,120,135,150,165,180};
            out.Magical += dmg[idx] + 0.9f * source.BonusAttackDamage() + 0.7f * source.TotalMagicalDamage();
        }
    }
    else if (name == "Ashe") {
        if (target.HasBuff("ashepassiveslow"))
            out.Physical += (0.1f + source.Crit()) * source.TotalAttackDamage();
    }
    else if (name == "Caitlyn") {
        if (source.HasBuff("caitlynheadshot") || target.HasBuff("caitlynyordletrapinternal")) {
            float dmg;
            if (target.IsHero()) {
                if (level >= 13) dmg = (1.0f + 1.25f * source.Crit()) * source.TotalAttackDamage();
                else if (level >= 7) dmg = (0.75f + 1.25f * source.Crit()) * source.TotalAttackDamage();
                else dmg = (0.5f + 1.25f * source.Crit()) * source.TotalAttackDamage();
            } else {
                dmg = (1.0f + 1.25f * source.Crit()) * source.TotalAttackDamage();
            }
            out.Physical += dmg;
        }
    }
    else if (name == "Corki") {
        if (std::abs(source.Crit() - 1.0f) < 0.001f) {
            out.Magical += 0.8f * GetCritMultiplier(source) * source.TotalAttackDamage();
            out.Physical += 0.2f * GetCritMultiplier(source) * source.TotalAttackDamage();
        }
    }
    else if (name == "Diana") {
        if (source.HasBuff("dianaarcready")) {
            constexpr float dmg[18] = {20,25,30,35,40,50,60,70,80,90,105,120,135,155,175,200,225,250};
            out.Magical += dmg[idx] + 0.8f * source.TotalMagicalDamage();
        }
    }
    else if (name == "Draven") {
        if (source.HasBuff("DravenSpinning"))
            out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
    }
    else if (name == "Ekko") {
        if (target.GetBuffCount("ekkostacks") == 2) {
            constexpr float base[18] = {30,40,50,60,70,80,85,90,95,100,105,110,115,120,125,130,135,140};
            float dmg = base[idx] + 0.8f * source.TotalMagicalDamage();
            if (target.IsMinion() && !target.IsEnemy()) dmg = std::min(600.0f, dmg * 2.0f);
            out.Magical += dmg;
        }
    }
    else if (name == "Fiora") {
        if (std::abs(source.Crit() - 1.0f) < 0.001f && !source.HasBuff("FioraE") && !source.HasBuff("fiorae2"))
            out.Physical += GetCritMultiplier(source) * source.TotalAttackDamage();
        if (source.HasBuff("fiorae2"))
            out.Physical += (GetCritMultiplier(source)) * source.TotalAttackDamage();
    }
    else if (name == "Gangplank") {
        if (source.HasBuff("gangplankpassiveattack")) {
            float dmg = (55.0f + 10.0f * static_cast<float>(idx) + source.BonusAttackDamage());
            if (target.IsTurret()) dmg *= 0.5f;
            out.True_ += dmg;
        }
    }
    else if (name == "Gnar") {
        if (target.GetBuffCount("gnarwproc") == 2)
            out.Magical += source.GetSpellDamage(target, SpellSlot::W);
    }
    else if (name == "Irelia") {
        if (source.GetBuffCount("ireliapassivestacks") >= 5)
            out.Magical += Lerp18(15.0f, 66.0f, source) + 0.25f * source.BonusAttackDamage();
    }
    else if (name == "Jax") {
        if (source.HasBuff("JaxEmpowerTwo"))
            out.Magical += source.GetSpellDamage(target, SpellSlot::W);
    }
    else if (name == "Jhin") {
        if (source.HasBuff("jhinpassiveattackbuff")) {
            const float missingHpRatio = level >= 11 ? 0.25f : (level >= 6 ? 0.2f : 0.15f);
            out.Physical += missingHpRatio * (target.MaxHealth() - target.Health());
        }
        if (std::abs(source.Crit() - 1.0f) < 0.001f || source.HasBuff("jhinpassiveattackbuff")) {
            const float mult = source.HasBuff("jhinpassiveattackbuff") ? 1.0f : GetCritMultiplier(source);
            out.Physical += 0.75f * mult * source.TotalAttackDamage();
        }
    }
    else if (name == "Kalista") {
        if (target.HasBuff("kalistacoopstrikemarkally"))
            out.Magical += source.GetSpellDamage(target, SpellSlot::W);
    }
    else if (name == "Kayle") {
        if (!source.HasBuff("KayleE") && std::abs(source.Crit() - 1.0f) < 0.001f)
            out.Physical += source.TotalAttackDamage() * GetCritMultiplier(source);
        out.Magical += source.GetSpellDamage(target, SpellSlot::E);
    }
    else if (name == "Khazix") {
        if (source.HasBuff("KhazixPDamage") && target.IsHero())
            out.Magical += Lerp18(14.0f, 150.0f, source) + 0.4f * source.BonusAttackDamage();
    }
    else if (name == "Lucian") {
        if (source.HasBuff("LucianPassiveBuff")) {
            const bool isMinion = target.IsMinion() && target.IsEnemy();
            float ratio = isMinion ? 1.0f : (level >= 13 ? 0.6f : (level >= 7 ? 0.55f : 0.5f));
            out.Physical += ratio * source.TotalAttackDamage();
        }
    }
    else if (name == "Lux") {
        if (target.HasBuff("LuxIlluminatingFraulein"))
            out.Magical += 20.0f + 10.0f * static_cast<float>(idx) + 0.2f * source.TotalMagicalDamage();
    }
    else if (name == "MasterYi") {
        if (source.HasBuff("doublestrike"))
            out.Physical += 0.5f * source.TotalAttackDamage() * GetCritMultiplier(source, true);
        if (source.HasBuff("wujustylesuperchargedvisual"))
            out.True_ += source.GetSpellDamage(target, SpellSlot::E);
    }
    else if (name == "MissFortune") {
        float ratio;
        if (level >= 13) ratio = 1.0f;
        else if (level >= 11) ratio = 0.9f;
        else if (level >= 9) ratio = 0.8f;
        else if (level >= 7) ratio = 0.7f;
        else if (level >= 4) ratio = 0.6f;
        else ratio = 0.5f;
        float dmg = ratio * source.TotalAttackDamage();
        if (target.IsMinion() && target.IsEnemy()) dmg *= 0.5f;
        out.Physical += dmg;
    }
    else if (name == "Nasus") {
        if (source.HasBuff("NasusQ"))
            out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
    }
    else if (name == "Nautilus") {
        if (!target.HasBuff("nautiluspassivecheck"))
            out.Physical += 8.0f + 6.0f * static_cast<float>(idx);
    }
    else if (name == "Orianna") {
        float base;
        if (level >= 16) base = 50.0f;
        else if (level >= 13) base = 42.0f;
        else if (level >= 10) base = 34.0f;
        else if (level >= 7) base = 26.0f;
        else if (level >= 4) base = 18.0f;
        else base = 10.0f;
        base += 0.15f * source.TotalMagicalDamage();
        int cnt = source.GetBuffCount("orianapowerdaggerdisplay");
        if (cnt > 0) base *= (1.0f + 0.2f * std::min(2, cnt));
        out.Magical += base;
    }
    else if (name == "Quinn") {
        if (target.HasBuff("QuinnW"))
            out.Physical += 10.0f + 5.0f * static_cast<float>(idx) + (0.16f + 0.02f * static_cast<float>(idx)) * source.TotalAttackDamage();
    }
    else if (name == "Rengar") {
        if (source.HasBuff("RengarQ"))
            out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
        if (source.HasBuff("RengarQEmp"))
            out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
    }
    else if (name == "Riven") {
        if (source.HasBuff("RivenPassiveAABoost")) {
            float ratio;
            if (level >= 18) ratio = 0.5f;
            else if (level >= 15) ratio = 0.45f;
            else if (level >= 12) ratio = 0.4f;
            else if (level >= 9) ratio = 0.35f;
            else if (level >= 6) ratio = 0.3f;
            else ratio = 0.25f;
            out.Physical += ratio * source.TotalAttackDamage();
        }
    }
    else if (name == "Sona") {
        if (source.HasBuff("sonapassiveattack")) {
            constexpr float dmg[18] = {20,30,40,50,60,70,80,90,105,120,135,150,165,180,195,210,225,240};
            out.Magical += dmg[idx] + 0.2f * source.TotalMagicalDamage();
        }
    }
    else if (name == "Talon") {
        if (target.GetBuffCount("TalonPassiveStack") >= 3)
            out.Physical += 75.0f + 10.0f * static_cast<float>(idx) + 2.0f * source.BonusAttackDamage();
    }
    else if (name == "Teemo") {
        out.Magical += source.GetSpellDamage(target, SpellSlot::E);
    }
    else if (name == "Tristana") {
        if (target.GetBuffCount("tristanaecharge") >= 3)
            out.Physical += source.GetSpellDamage(target, SpellSlot::E);
    }
    else if (name == "TwistedFate") {
        if (source.HasBuff("BlueCardPreAttack")) {
            out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            out.Override = true;
        }
        else if (source.HasBuff("RedCardPreAttack")) {
            out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            out.Override = true;
        }
        else if (source.HasBuff("GoldCardPreAttack")) {
            out.Magical += source.GetSpellDamage(target, SpellSlot::W);
            out.Override = true;
        }
        if (source.HasBuff("cardmasterstackparticle"))
            out.Magical += source.GetSpellDamage(target, SpellSlot::E);
    }
    else if (name == "Twitch") {
        float perStack = level >= 17 ? 5.0f : (level >= 13 ? 4.0f : (level >= 9 ? 3.0f : (level >= 5 ? 2.0f : 1.0f)));
        int stacks = std::min(std::max(target.GetBuffCount("TwitchDeadlyVenom"), 0) + 1, 6);
        float multiplier = target.IsHero() ? 6.0f : 1.0f;
        out.True_ += perStack * static_cast<float>(stacks) * multiplier;
    }
    else if (name == "Varus") {
        out.Magical += source.GetSpellDamage(target, SpellSlot::W);
    }
    else if (name == "Vayne") {
        if (source.HasBuff("vaynetumblebonus"))
            out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
        if (target.GetBuffCount("VayneSilveredDebuff") == 2)
            out.True_ += source.GetSpellDamage(target, SpellSlot::W);
    }
    else if (name == "Vi") {
        if (target.GetBuffCount("viwproc") == 2)
            out.Physical += source.GetSpellDamage(target, SpellSlot::W);
        if (source.HasBuff("ViE"))
            out.Physical += GetCritMultiplier(source, true) * source.GetSpellDamage(target, SpellSlot::E);
    }
    else if (name == "Warwick") {
        out.Magical += 10.0f + 2.0f * static_cast<float>(idx);
    }
    else if (name == "XinZhao") {
        if (source.GetBuffCount("XinZhaoPTracker") >= 3) {
            float ratio = level >= 16 ? 0.45f : (level >= 11 ? 0.35f : (level >= 6 ? 0.25f : 0.15f));
            out.Physical += ratio * source.TotalAttackDamage();
        }
        if (source.HasBuff("XinZhaoQ"))
            out.Physical += source.GetSpellDamage(target, SpellSlot::Q);
    }
    else if (name == "Yasuo") {
        if (std::abs(source.Crit() - 1.0f) < 0.001f)
            out.Physical += 0.9f * GetCritMultiplier(source) * source.TotalAttackDamage();
    }
    else if (name == "Zed") {
        if (target.HealthPercent() < 50.0f && !target.HasBuff("zedpassivecd")) {
            float ratio = level >= 17 ? 0.1f : (level >= 7 ? 0.08f : 0.06f);
            out.Magical += ratio * target.MaxHealth();
        }
    }
    else if (name == "Ziggs") {
        if (source.HasBuff("ZiggsShortFuse")) {
            constexpr float base[18] = {20,24,28,32,36,40,48,56,64,72,80,88,100,112,124,136,148,160};
            float apRatio = level >= 13 ? 0.5f : (level >= 7 ? 0.4f : 0.3f);
            float dmg = base[idx] + apRatio * source.TotalMagicalDamage();
            if (target.IsTurret()) dmg *= 2.0f;
            out.Magical += dmg;
        }
    }
    else if (name == "Zoe") {
        if (source.HasBuff("zoepassivesheenbuff")) {
            constexpr float base[18] = {10,12,16,20,24,28,34,40,46,52,60,68,76,84,94,104,114,124};
            out.Magical += base[idx] + 0.2f * source.TotalMagicalDamage();
        }
    }

    // -- Rammus (always on) --
    if (name == "Rammus") {
        float base = std::min(20.0f, 8.0f + static_cast<float>(idx)) + 0.1f * source.Armor();
        if (source.HasBuff("DefensiveBallCurl")) base *= 1.5f;
        out.Magical += base;
    }

    // -- Rumble overheat --
    if (name == "Rumble" && source.HasBuff("rumbleoverheat")) {
        out.Magical += 25.0f + 5.0f * static_cast<float>(idx) + 0.3f * source.TotalMagicalDamage();
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
