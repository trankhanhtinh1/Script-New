#pragma once

#include "../../Core/Objects.h"
#include "../../Core/CoreBuffs.h"
#include "../../Core/Game.h"
#include "../Enumerations/DamageType.h"
#include "../Enumerations/MinionTypes.h"
#include "../Enumerations/SpellSlot.h"
#include "../../Core/Hud.h"

#include <algorithm>
#include <cmath>

// ============================================================================
// DamageReductionMod — ported from EnsoulSharp.SDK.Damage.DamageReductionMod
// Applies buff/debuff-based damage modifiers (champion passives, jungle
// dragon kills, Baron buff, Exhaust, Sona passive, etc.)
// ============================================================================

namespace SDK::DamageMod {

    // DLL: private static readonly double[] arrays
    static constexpr double AlistarR[]  = { 0.55, 0.65, 0.75 };
    static constexpr double AmumuE[]    = { 5.0, 7.0, 9.0, 11.0, 13.0 };
    static constexpr double BraumE[]    = { 0.30, 0.325, 0.35, 0.375, 0.40 };
    static constexpr double GalioW[]    = { 0.20, 0.25, 0.30, 0.35, 0.40 };
    static constexpr double GragasW[]   = { 0.10, 0.12, 0.14, 0.16, 0.18 };
    static constexpr double MasterYiW[] = { 0.60, 0.625, 0.65, 0.675, 0.70 };
    static constexpr double WarwickE[]  = { 0.35, 0.40, 0.45, 0.50, 0.55 };

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
            const int enemyDragonKills = dragon.EnemyDragonKills;
            if (enemyDragonKills > 0) {
                amount *= static_cast<float>(1.0 + 0.2 * enemyDragonKills);
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
                    const int allyDragonKills = dragon.AllyDragonKills;
                    if (allyDragonKills > 0) {
                        amount *= static_cast<float>(1.0 - 0.07 * allyDragonKills);
                    }
                }
                // Shyvana passive: +20% damage to legendary jungle
                if (source.CharacterName() == "Shyvana") {
                    const AIMinionClient minionTarget(target.Address());
                    if (HasFlag(minionTarget.GetJungleType(), JungleType::Legendary)) {
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

        // Sona passive debuff on source
        // DLL: buff.Caster.TotalMagicalDamage
        if (source.HasBuff("SonaEDebuff")) {
            const uintptr_t casterAddr = source.GetBuffCaster("SonaEDebuff");
            if (Globals::IsValidPtr(casterAddr)) {
                const AIBaseClient caster(casterAddr);
                const float bonusMag = caster.TotalMagicalDamage();
                amount -= bonusMag * 0.15f;
                if (amount < 0.0f) amount = 0.0f;
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

        // Siege minion with Baron buff vs turret: 2x damage
        if (sourceIsMinion &&
            target.Type() == ::Core::Objects::ObjectType::AITurretClient) {
            const AIMinionClient minionSource(source.Address());
            if (HasFlag(minionSource.GetMinionType(), MinionTypes::Siege) &&
                minionSource.HasBuff("exaltedwithbaronnashorminion")) {
                amount *= 2.0f;
            }
        }

        // Champion-specific damage reduction passives
        if (targetIsHero) {
            const auto heroTarget = AIHeroClient(target.Address());
            const std::string targetName = heroTarget.CharacterName();

            if (targetName == "Alistar" && heroTarget.HasBuff("AlisterTrample")) {
                const int level = GetSpellLevel(target, SpellSlot::R);
                if (level >= 1 && level <= 3) {
                    amount *= static_cast<float>(AlistarR[level - 1]);
                }
            }

            if (targetName == "Amumu" && heroTarget.HasBuff("AmumuESpell")) {
                const int level = GetSpellLevel(target, SpellSlot::E);
                if (level >= 1 && level <= 5) {
                    amount -= static_cast<float>(AmumuE[level - 1]);
                }
            }

            if (targetName == "Braum" && heroTarget.HasBuff("BraumEShield")) {
                const int level = GetSpellLevel(target, SpellSlot::E);
                if (level >= 1 && level <= 5) {
                    amount *= static_cast<float>(BraumE[level - 1]);
                }
            }

            if (targetName == "Galio" && heroTarget.HasBuff("GalioW")) {
                const int level = GetSpellLevel(target, SpellSlot::W);
                if (level >= 1 && level <= 5) {
                    amount *= static_cast<float>(GalioW[level - 1]);
                }
            }

            if (targetName == "Gragas" && heroTarget.HasBuff("GragasW")) {
                const int level = GetSpellLevel(target, SpellSlot::W);
                if (level >= 1 && level <= 5) {
                    amount *= static_cast<float>(GragasW[level - 1]);
                }
            }

            if (targetName == "MasterYi" && heroTarget.HasBuff("Meditate")) {
                const int level = GetSpellLevel(target, SpellSlot::W);
                if (level >= 1 && level <= 5) {
                    amount *= static_cast<float>(MasterYiW[level - 1]);
                }
            }

            if (targetName == "Warwick" && heroTarget.HasBuff("WarwickE")) {
                const int level = GetSpellLevel(target, SpellSlot::E);
                if (level >= 1 && level <= 5) {
                    amount *= static_cast<float>(WarwickE[level - 1]);
                }
            }
        }

        if (amount < 0.0f) amount = 0.0f;
        return amount;
    }
} // namespace SDK::DamageMod