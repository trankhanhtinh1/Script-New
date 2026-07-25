#pragma once

#include "../../Core/Objects.h"
#include "../../../Core/CoreBuffs.h"
#include "../../Core/Game.h"
#include "../../Enumerations/DamageType.h"
#include "../../Enumerations/MinionTypes.h"
#include "../../Enumerations/SpellSlot.h"
#include "../../Core/Hud.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

// ============================================================================
// DamageReductionMod — ported from EnsoulSharp.SDK.Damage.DamageReductionMod
// Applies buff/debuff-based damage modifiers (champion passives, jungle
// dragon kills, Baron buff, Exhaust, Sona passive, etc.)
// ============================================================================

namespace SDK::DamageMod {

    // CommunityDragon latest (16.13.7906961) SpellDataValue arrays keep
    // index 0 as the unlearned rank for most spells; runtime SpellLevel is
    // therefore used directly and clamped to the learned rank range.
    static constexpr float AlistarRDamageReduction[]     = { 0.45f, 0.55f, 0.65f, 0.75f };
    static constexpr float AmumuEFlatReduction[]         = { 0.0f, 5.0f, 7.0f, 9.0f, 11.0f, 13.0f };
    static constexpr float BraumEDamageReduction[]       = { 0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.55f };
    static constexpr float GalioWBaseReduction[]         = { 0.20f, 0.25f, 0.30f, 0.35f, 0.40f, 0.45f };
    static constexpr float GarenWReduction[]             = { 0.21f, 0.25f, 0.29f, 0.33f, 0.37f, 0.41f };
    static constexpr float GragasWBaseReductionPercent[] = { 6.0f, 10.0f, 14.0f, 18.0f, 22.0f, 26.0f };
    static constexpr float LeonaWFlatReduction[]         = { 4.0f, 8.0f, 12.0f, 16.0f, 20.0f, 24.0f };
    static constexpr float MasterYiWReduction[]          = { 0.425f, 0.45f, 0.475f, 0.50f, 0.525f, 0.55f };
    static constexpr float MasterYiWInitialExtra[]       = { 0.275f, 0.25f, 0.225f, 0.20f, 0.175f, 0.15f };
    static constexpr float WarwickEReduction[]           = { 0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.55f };
    static constexpr float BelvethEReduction[]           = { 0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.55f };

    template <std::size_t N>
    inline float SpellRankValue(const float (&values)[N], int spellLevel) {
        static_assert(N > 1, "SpellRankValue requires an unlearned entry plus at least one learned rank");
        const int index = std::clamp(spellLevel, 1, static_cast<int>(N) - 1);
        return values[index];
    }

    inline float PercentToMultiplier(float reduction) {
        return 1.0f - std::clamp(reduction, 0.0f, 0.99f);
    }

    inline void ApplyFlatReduction(float& amount, float reduction, float maxFraction = 0.5f) {
        const float capped = std::min(reduction, amount * maxFraction);
        amount = std::max(amount - capped, 0.0f);
    }

    // DLL: private static int GetSpellLevel(AIBaseClient source, SpellSlot slot)
    inline int GetSpellLevel(const AIBaseClient& source, SpellSlot slot) {
        auto spell = source.GetSpell(slot);
        int level = spell.Level();
        return (level == 0) ? 1 : level;
    }

    // DLL: source.IsDragon() — checks if source is a dragon minion
    inline bool IsDragon(const AIBaseClient& source) {
        const std::string name = source.CharacterName();
        return name.find("Dragon") != std::string::npos ||
               name.find("dragon") != std::string::npos;
    }

    // DLL: DamageReductionMod — full port
    inline float DamageReductionMod(const AIBaseClient& source,
                                    const AIBaseClient& target,
                                    float amount,
                                    DamageType damageType) {
        if (!source.IsValid() || !target.IsValid()) return 0.0f;

        const std::string characterName = target.CharacterName();
        const bool targetIsHero = (target.Type() == ::Core::Objects::ObjectType::AIHeroClient);
        const bool sourceIsHero = (source.Type() == ::Core::Objects::ObjectType::AIHeroClient);
        const bool sourceIsMinion = (source.Type() == ::Core::Objects::ObjectType::AIMinionClient);
        const bool targetIsMinion = (target.Type() == ::Core::Objects::ObjectType::AIMinionClient);

        // Dragon vs hero: dragon damage scales with dragon kills
        // DLL: aIHeroClient != null && aIMinionClient != null && source.Team == Neutral && source.IsDragon()
        if (targetIsHero && sourceIsMinion &&
            source.Team() == GameObjectTeam::Neutral && IsDragon(source)) {
            const auto dragon = SDK::Hud::DragonSRX();
            const int dragonKills = target.IsAlly()
                ? dragon.AllyDragonKills
                : dragon.EnemyDragonKills;
            if (dragonKills > 0) {
                amount *= static_cast<float>(1.0 + 0.2 * dragonKills);
            }
        }

        // Hero source vs neutral minion
        if (sourceIsHero) {
            if (targetIsMinion && target.Team() == GameObjectTeam::Neutral) {
                // Baron target buff
                if (source.HasBuff("barontarget") && characterName == "SRU_Baron") {
                    amount *= 0.5f;
                }
                // Dragon damage reduction from dragon kills
                if (IsDragon(target)) {
                    const auto dragon = SDK::Hud::DragonSRX();
                    const int dragonKills = source.IsAlly()
                        ? dragon.AllyDragonKills
                        : dragon.EnemyDragonKills;
                    if (dragonKills > 0) {
                        amount *= static_cast<float>(1.0 - 0.07 * dragonKills);
                    }
                }
                // Shyvana passive: +20% damage to legendary jungle
                if (source.CharacterName() == "Shyvana") {
                    const AIMinionClient minionTarget(target.Address());
                    if (minionTarget.GetJungleType() == JungleType::Legendary) {
                        amount *= 1.2f;
                    }
                }
            }
            // Exhaust: -40% damage
            if (source.HasBuff("SummonerExhaust")) {
                amount *= 0.6f;
            }
        }

        // Vladimir hemoplague: +10% damage amp on target
        if (target.HasBuff("vladimirhemoplaguedamageamp")) {
            amount *= 1.1f;
        }

        // Sona W Power Chord - Diminuendo: target deals 25% + 4%/100 AP
        // less physical/magic damage. CDragon object is SonaWPassiveDebuff;
        // keep the legacy lowercase SDK name as a fallback for older dumps.
        const char* sonaDebuffName = nullptr;
        if (source.HasBuff("SonaWPassiveDebuff")) {
            sonaDebuffName = "SonaWPassiveDebuff";
        } else if (source.HasBuff("sonapassivedebuff")) {
            sonaDebuffName = "sonapassivedebuff";
        }
        if (sonaDebuffName) {
            const uintptr_t casterAddr = source.GetBuffCaster(sonaDebuffName);
            if (Globals::IsValidPtr(casterAddr)) {
                const AIBaseClient caster(casterAddr);
                const float weaken = 0.25f + 0.0004f * caster.TotalMagicalDamage();
                amount *= PercentToMultiplier(weaken);
            }
        }

        // Baron buff on minions (exaltedwithbaronnashorminion)
        if (targetIsMinion && target.HasBuff("exaltedwithbaronnashorminion")) {
            const AIMinionClient minionTarget(target.Address());
            MinionTypes minionType = minionTarget.GetMinionType();
            if (HasFlag(minionType, MinionTypes::Ranged)) {
                if (sourceIsHero) {
                    amount *= 0.3f;
                }
            } else if (HasFlag(minionType, MinionTypes::Melee)) {
                if (sourceIsHero) {
                    amount *= 0.3f;
                } else if (sourceIsMinion && source.Team() != GameObjectTeam::Neutral) {
                    amount *= 0.25f;
                }
            }
        }

        // REMOVED: Turret/Inhibitor/Nexus disabled by user request
        // Siege minion with Baron buff vs turret: 2x damage
        /*if (sourceIsMinion &&
            target.Type() == ::Core::Objects::ObjectType::AITurretClient) {
            const AIMinionClient minionSource(source.Address());
            if (HasFlag(minionSource.GetMinionType(), MinionTypes::Siege) &&
                minionSource.HasBuff("exaltedwithbaronnashorminion")) {
                amount *= 2.0f;
            }
        }*/

        // Champion-specific damage reduction from CDragon latest.
        if (targetIsHero) {
            const AIHeroClient heroTarget(target.Address());
            const std::string targetName = heroTarget.CharacterName();
            const int championLevel = std::clamp(heroTarget.Level(), 1, 18);

            if (targetName == "Alistar" && heroTarget.HasBuff("FerociousHowl")) {
                const float reduction = SpellRankValue(
                    AlistarRDamageReduction,
                    GetSpellLevel(heroTarget, SpellSlot::R));
                amount *= PercentToMultiplier(reduction);
            }

            if (targetName == "Amumu" && heroTarget.HasBuff("Tantrum") &&
                damageType == DamageType::Physical) {
                const float reduction = SpellRankValue(
                    AmumuEFlatReduction,
                    GetSpellLevel(heroTarget, SpellSlot::E)) +
                    0.03f * heroTarget.BonusArmor() +
                    0.03f * heroTarget.BonusSpellBlock();
                ApplyFlatReduction(amount, reduction);
            }

            if (targetName == "Braum" &&
                (heroTarget.HasBuff("BraumEShieldBuff") ||
                 heroTarget.HasBuff("braumeshieldbuff"))) {
                const float reduction = SpellRankValue(
                    BraumEDamageReduction,
                    GetSpellLevel(heroTarget, SpellSlot::E));
                amount *= PercentToMultiplier(reduction);
            }

            if (targetName == "Galio" &&
                (heroTarget.HasBuff("GalioWBuff") ||
                 heroTarget.HasBuff("galiowbuff"))) {
                const float magicReduction =
                    SpellRankValue(GalioWBaseReduction, GetSpellLevel(heroTarget, SpellSlot::W)) +
                    0.0004f * heroTarget.TotalMagicalDamage() +
                    0.0008f * heroTarget.BonusSpellBlock() +
                    0.0001f * heroTarget.MaxHealth();

                if (damageType == DamageType::Physical) {
                    amount *= PercentToMultiplier(magicReduction * 0.5f);
                } else if (damageType == DamageType::Magical) {
                    amount *= PercentToMultiplier(magicReduction);
                }
            }

            if (targetName == "Garen" && heroTarget.HasBuff("GarenW")) {
                const float reduction = SpellRankValue(
                    GarenWReduction,
                    GetSpellLevel(heroTarget, SpellSlot::W));
                amount *= PercentToMultiplier(reduction);
            }

            if (targetName == "Gragas" &&
                (heroTarget.HasBuff("GragasW") ||
                 heroTarget.HasBuff("gragaswself"))) {
                const float reduction =
                    (SpellRankValue(GragasWBaseReductionPercent, GetSpellLevel(heroTarget, SpellSlot::W)) +
                     0.04f * heroTarget.TotalMagicalDamage()) *
                    0.01f;
                amount *= PercentToMultiplier(reduction);
            }

            if (targetName == "Irelia" &&
                (heroTarget.HasBuff("IreliaWDefense") ||
                 heroTarget.HasBuff("ireliawdefense"))) {
                const float physicalReduction =
                    0.40f +
                    (0.30f / 17.0f) * static_cast<float>(championLevel - 1) +
                    0.0007f * heroTarget.TotalMagicalDamage();

                if (damageType == DamageType::Physical) {
                    amount *= PercentToMultiplier(physicalReduction);
                } else if (damageType == DamageType::Magical) {
                    amount *= PercentToMultiplier(physicalReduction * 0.5f);
                }
            }

            if (targetName == "Kassadin" && damageType == DamageType::Magical) {
                amount *= 0.90f;
            }

            if (targetName == "Leona" &&
                (heroTarget.HasBuff("LeonaSolarBarrier") ||
                 heroTarget.HasBuff("leonasolarbarrier"))) {
                const float reduction = SpellRankValue(
                    LeonaWFlatReduction,
                    GetSpellLevel(heroTarget, SpellSlot::W));
                ApplyFlatReduction(amount, reduction);
            }

            if (targetName == "Fizz") {
                const bool neutralMonsterSource =
                    sourceIsMinion && source.Team() == GameObjectTeam::Neutral;
                const float reduction =
                    (neutralMonsterSource ? 14.0f : 4.0f) +
                    0.01f * heroTarget.TotalMagicalDamage();
                ApplyFlatReduction(amount, reduction);
            }

            if (targetName == "Malzahar" &&
                (heroTarget.HasBuff("MalzaharPassiveShield") ||
                 heroTarget.HasBuff("MalzaharPassive"))) {
                amount *= 0.10f;
            }

            if (targetName == "MasterYi" && heroTarget.HasBuff("Meditate")) {
                const int spellLevel = GetSpellLevel(heroTarget, SpellSlot::W);
                float reduction = SpellRankValue(MasterYiWReduction, spellLevel);

                const auto meditate = CoreBuffs::FindByName(heroTarget.Address(), "Meditate");
                if (meditate.IsValid()) {
                    const float elapsed = ::SDK::Game::Time() - meditate.GetStartTime();
                    if (elapsed >= 0.0f && elapsed < 0.5f) {
                        reduction += SpellRankValue(MasterYiWInitialExtra, spellLevel);
                    }
                }

                // REMOVED: Turret/Inhibitor/Nexus disabled by user request
                /*if (source.Type() == ::Core::Objects::ObjectType::AITurretClient) {
                    reduction *= 0.5f;
                }*/
                amount *= PercentToMultiplier(reduction);
            }

            if (targetName == "Warwick" && heroTarget.HasBuff("WarwickE")) {
                const float reduction = SpellRankValue(
                    WarwickEReduction,
                    GetSpellLevel(heroTarget, SpellSlot::E));
                amount *= PercentToMultiplier(reduction);
            }

            if (targetName == "Belveth" && heroTarget.HasBuff("BelvethE")) {
                const float reduction = SpellRankValue(
                    BelvethEReduction,
                    GetSpellLevel(heroTarget, SpellSlot::E));
                amount *= PercentToMultiplier(reduction);
            }

            if (targetName == "Briar" && heroTarget.HasBuff("BriarEDR")) {
                amount *= PercentToMultiplier(0.35f);
            }

            const bool targetIsKSante = targetName == "KSante" || targetName == "K'Sante";
            if (targetIsKSante) {
                if (heroTarget.HasBuff("KSanteW_AllOut")) {
                    amount *= PercentToMultiplier(0.75f);
                } else if (heroTarget.HasBuff("KSanteW")) {
                    amount *= PercentToMultiplier(0.30f);
                }
            }

            if (targetName == "Nilah" && damageType == DamageType::Magical &&
                (heroTarget.HasBuff("NilahWBuff") ||
                 heroTarget.HasBuff("NilahWAllyBuff"))) {
                amount *= PercentToMultiplier(0.25f);
            }

            if (targetName == "Zaahen" && heroTarget.HasBuff("ZaahenR")) {
                amount *= PercentToMultiplier(0.50f);
            }
        }

        if (amount < 0.0f) amount = 0.0f;
        return amount;
    }
} // namespace SDK::DamageMod
