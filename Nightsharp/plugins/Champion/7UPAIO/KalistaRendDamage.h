#pragma once

#include <algorithm>

namespace Plugins::AIO7UP::Kalista::RendDamage {

inline double RawDamage(int spellLevel,
                        int spearCount,
                        float totalAttackDamage,
                        float abilityPower,
                        bool isEpicMonster) {
    if (spellLevel <= 0 || spearCount <= 0) {
        return 0.0;
    }

    // CommunityDragon latest KalistaExpungeWrapper DataValues and
    // mSpellCalculations. Rank 0 is the unlearned sentinel.
    static constexpr double kBaseDamage[] = { -5.0, 5.0, 15.0, 25.0, 35.0, 45.0, 55.0 };
    static constexpr double kAdditionalBaseDamage[] = { 0.0, 7.0, 14.0, 21.0, 28.0, 35.0, 42.0 };
    static constexpr double kAdditionalAdRatio[] = {
        0.125,
        0.20000000298023224,
        0.2750000059604645,
        0.3499999940395355,
        0.42500001192092896,
        0.5,
        0.574999988079071
    };

    const int level = std::clamp(spellLevel, 1, 5);
    const double normal =
        kBaseDamage[level] +
        0.699999988079071 * static_cast<double>(totalAttackDamage) +
        0.6499999761581421 * static_cast<double>(abilityPower);
    const double additional =
        kAdditionalBaseDamage[level] +
        kAdditionalAdRatio[level] * static_cast<double>(totalAttackDamage) +
        0.5 * static_cast<double>(abilityPower);
    const double raw = normal + additional * static_cast<double>(spearCount - 1);

    // CommunityDragon latest: EpicMonsterDamageMod = 0.5 at every rank.
    return std::max(0.0, raw * (isEpicMonster ? 0.5 : 1.0));
}

inline double SecureJungleDamage(double finalDamage, bool isLargeOrEpic) {
    // Original 7UPAIO Clear() deliberately uses half of calculated damage for
    // large/legendary jungle executes so latency and objective modifiers cannot
    // cause an early Rend that leaves the objective alive.
    return std::max(0.0, finalDamage * (isLargeOrEpic ? 0.5 : 1.0));
}

} // namespace Plugins::AIO7UP::Kalista::RendDamage
