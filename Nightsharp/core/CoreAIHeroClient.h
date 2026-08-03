#pragma once

#include "CoreAttackableUnit.h"
#include "Globals.h"
#include "offset.h"

#include <cstdint>

namespace CoreAIHeroClient {

struct Snapshot {
    uintptr_t address = 0;
    CoreAttackableUnit::Snapshot attackable = {};
    float mana = 0.0f;
    float maxMana = 0.0f;
    float par = 0.0f;
    float maxPar = 0.0f;
    float sar = 0.0f;
    float maxSar = 0.0f;
    float physicalDamagePercent = 0.0f;
    float magicDamagePercent = 0.0f;
    float abilityHaste = 0.0f;
    float flatPhysicalDamageMod = 0.0f;
    float attackSpeedMod = 0.0f;
    float percentAttackSpeedMod = 0.0f;
    float baseAttackDamage = 0.0f;
    float baseAttackDamageSansScale = 0.0f;
    float flatBaseAttackDamageMod = 0.0f;
    float percentBaseAttackDamageMod = 0.0f;
    float baseAbilityDamage = 0.0f;
    float critDamageMultiplier = 0.0f;
    float dodge = 0.0f;
    float crit = 0.0f;
    float armor = 0.0f;
    float bonusArmor = 0.0f;
    float spellBlock = 0.0f;
    float bonusSpellBlock = 0.0f;
    float healthRegenRate = 0.0f;
    float baseHealthRegenRate = 0.0f;
    float moveSpeed = 0.0f;
    float attackRange = 0.0f;
    float flatArmorPen = 0.0f;
    float physicalLethality = 0.0f;
    float percentArmorPen = 0.0f;
    float percentBonusArmorPen = 0.0f;
    float flatMagicPen = 0.0f;
    float magicLethality = 0.0f;
    float percentMagicPen = 0.0f;
    float percentBonusMagicPen = 0.0f;
    float percentLifeSteal = 0.0f;
    float percentSpellVamp = 0.0f;
    float percentOmnivamp = 0.0f;
    float percentCcReduction = 0.0f;
    float flatBaseAttackSpeedMod = 0.0f;
    float gold = 0.0f;
    float goldTotal = 0.0f;
    float experience = 0.0f;
    int level = 0;
    int levelUpPoints = 0;
    uintptr_t runeManager = 0;
    float visionScore = 0.0f;
    float shutdownValue = 0.0f;
    float baseGoldOnDeath = 0.0f;
    float neutralMinionsKilled = 0.0f;

    bool IsValid() const {
        return Globals::IsValidPtr(address);
    }

    float TotalAttackDamage() const {
        return baseAttackDamage + flatPhysicalDamageMod;
    }

    float TotalArmor() const {
        return armor + bonusArmor;
    }

    float TotalSpellBlock() const {
        return spellBlock + bonusSpellBlock;
    }
};

template <typename T>
inline T ReadField(uintptr_t object, uintptr_t offset) {
    if (!Globals::IsValidPtr(object)) {
        return T{};
    }
    return Globals::Read<T>(object + offset);
}

inline float Mana(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::MP);
}

inline float MaxMana(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::MaxMP);
}

inline float Par(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PAR);
}

inline float MaxPar(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::MaxPAR);
}

inline float Sar(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::SAR);
}

inline float MaxSar(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::MaxSAR);
}

inline float PhysicalDamagePercent(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PhysDmgPercent);
}

inline float MagicDamagePercent(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::MagicDmgPercent);
}

inline float AbilityHaste(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::AbilityHaste);
}

inline float FlatPhysicalDamageMod(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::FlatPhysicalDmgMod);
}

inline float AttackSpeedMod(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::AttackSpeedMod);
}

inline float PercentAttackSpeedMod(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PercentAttackSpeedMod);
}

inline float BaseAttackDamage(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::BaseAttackDamage);
}

inline float BaseAttackDamageSansScale(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::BaseAtkDmgSansScale);
}

inline float FlatBaseAttackDamageMod(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::FlatBaseAtkDmgMod);
}

inline float PercentBaseAttackDamageMod(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PercentBaseAtkDmgMod);
}

inline float BaseAbilityDamage(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::BaseAbilityDamage);
}

inline float CritDamageMultiplier(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::CritDamageMultiplier);
}

inline float Dodge(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::Dodge);
}

inline float Crit(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::Crit);
}

inline float Armor(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::Armor);
}

inline float BonusArmor(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::BonusArmor);
}

inline float SpellBlock(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::SpellBlock);
}

inline float BonusSpellBlock(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::BonusSpellBlock);
}

inline float HealthRegenRate(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::HPRegenRate);
}

inline float BaseHealthRegenRate(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::BaseHPRegenRate);
}

inline float MoveSpeed(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::MoveSpeed);
}

inline float AttackRange(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::AttackRange);
}

inline float FlatArmorPen(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::FlatArmorPen);
}

inline float PhysicalLethality(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PhysicalLethality);
}

inline float PercentArmorPen(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PercentArmorPen);
}

inline float PercentBonusArmorPen(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PercentBonusArmorPen);
}

inline float FlatMagicPen(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::FlatMagicPen);
}

inline float MagicLethality(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::MagicLethality);
}

inline float PercentMagicPen(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PercentMagicPen);
}

inline float PercentBonusMagicPen(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PercentBonusMagicPen);
}

inline float PercentLifeSteal(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PercentLifeSteal);
}

inline float PercentSpellVamp(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PercentSpellVamp);
}

inline float PercentOmnivamp(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PercentOmnivamp);
}

inline float PercentCcReduction(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::PercentCCReduction);
}

inline float FlatBaseAttackSpeedMod(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::FlatBaseAttackSpeedMod);
}

inline float Gold(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::Gold);
}

inline float GoldTotal(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::GoldTotal);
}

inline float Experience(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::Exp);
}

inline int Level(uintptr_t object) {
    return ReadField<int>(object, Offset::AIHeroClient::LevelRef);
}

inline int LevelUpPoints(uintptr_t object) {
    return ReadField<int>(object, Offset::AIHeroClient::LevelUpPoints);
}

inline uintptr_t RuneManager(uintptr_t object) {
    return Globals::IsValidPtr(object)
        ? object + Offset::AIHeroClient::RuneManager
        : 0;
}

inline float VisionScore(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::VisionScore);
}

inline float ShutdownValue(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::ShutdownValue);
}

inline float BaseGoldOnDeath(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::BaseGoldOnDeath);
}

inline float NeutralMinionsKilled(uintptr_t object) {
    return ReadField<float>(object, Offset::AIHeroClient::NeutralMinionsKilled);
}

inline float TotalAttackDamage(uintptr_t object) {
    return BaseAttackDamage(object) + FlatPhysicalDamageMod(object);
}

// Forward declaration — defined below. Read() delegates to ReadFromAttackable()
// so it can reuse an already-read CoreAttackableUnit snapshot and avoid
// re-reading the 15 attackable fields a second time.
Snapshot ReadFromAttackable(Snapshot snapshot, uintptr_t object);

inline Snapshot Read(uintptr_t object) {
    Snapshot snapshot{};
    snapshot.address = object;
    if (!Globals::IsValidPtr(object)) {
        return snapshot;
    }

    snapshot.attackable = CoreAttackableUnit::Read(object);
    return ReadFromAttackable(snapshot, object);
}

// Reads only the AI-hero-specific fields, reusing an already-populated
// CoreAttackableUnit snapshot instead of re-reading the 15 attackable fields.
// Called by Core::Objects::ReadSnapshot after ReadAttackable() has already run
// CoreAttackableUnit::Read — without this overload the attackable read would
// run twice per AIHero/AIMinion/AITurret snapshot (15 wasted reads each).
inline Snapshot ReadFromAttackable(Snapshot snapshot, uintptr_t object) {
    snapshot.address = object;
    if (!Globals::IsValidPtr(object)) {
        return snapshot;
    }

    snapshot.mana = Mana(object);
    snapshot.maxMana = MaxMana(object);
    snapshot.par = Par(object);
    snapshot.maxPar = MaxPar(object);
    snapshot.sar = Sar(object);
    snapshot.maxSar = MaxSar(object);
    snapshot.physicalDamagePercent = PhysicalDamagePercent(object);
    snapshot.magicDamagePercent = MagicDamagePercent(object);
    snapshot.abilityHaste = AbilityHaste(object);
    snapshot.flatPhysicalDamageMod = FlatPhysicalDamageMod(object);
    snapshot.attackSpeedMod = AttackSpeedMod(object);
    snapshot.percentAttackSpeedMod = PercentAttackSpeedMod(object);
    snapshot.baseAttackDamage = BaseAttackDamage(object);
    snapshot.baseAttackDamageSansScale = BaseAttackDamageSansScale(object);
    snapshot.flatBaseAttackDamageMod = FlatBaseAttackDamageMod(object);
    snapshot.percentBaseAttackDamageMod = PercentBaseAttackDamageMod(object);
    snapshot.baseAbilityDamage = BaseAbilityDamage(object);
    snapshot.critDamageMultiplier = CritDamageMultiplier(object);
    snapshot.dodge = Dodge(object);
    snapshot.crit = Crit(object);
    snapshot.armor = Armor(object);
    snapshot.bonusArmor = BonusArmor(object);
    snapshot.spellBlock = SpellBlock(object);
    snapshot.bonusSpellBlock = BonusSpellBlock(object);
    snapshot.healthRegenRate = HealthRegenRate(object);
    snapshot.baseHealthRegenRate = BaseHealthRegenRate(object);
    snapshot.moveSpeed = MoveSpeed(object);
    snapshot.attackRange = AttackRange(object);
    snapshot.flatArmorPen = FlatArmorPen(object);
    snapshot.physicalLethality = PhysicalLethality(object);
    snapshot.percentArmorPen = PercentArmorPen(object);
    snapshot.percentBonusArmorPen = PercentBonusArmorPen(object);
    snapshot.flatMagicPen = FlatMagicPen(object);
    snapshot.magicLethality = MagicLethality(object);
    snapshot.percentMagicPen = PercentMagicPen(object);
    snapshot.percentBonusMagicPen = PercentBonusMagicPen(object);
    snapshot.percentLifeSteal = PercentLifeSteal(object);
    snapshot.percentSpellVamp = PercentSpellVamp(object);
    snapshot.percentOmnivamp = PercentOmnivamp(object);
    snapshot.percentCcReduction = PercentCcReduction(object);
    snapshot.flatBaseAttackSpeedMod = FlatBaseAttackSpeedMod(object);
    snapshot.gold = Gold(object);
    snapshot.goldTotal = GoldTotal(object);
    snapshot.experience = Experience(object);
    snapshot.level = Level(object);
    snapshot.levelUpPoints = LevelUpPoints(object);
    snapshot.runeManager = RuneManager(object);
    snapshot.visionScore = VisionScore(object);
    snapshot.shutdownValue = ShutdownValue(object);
    snapshot.baseGoldOnDeath = BaseGoldOnDeath(object);
    snapshot.neutralMinionsKilled = NeutralMinionsKilled(object);
    return snapshot;
}

} // namespace CoreAIHeroClient
