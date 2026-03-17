#pragma once
#include <functional>
#include <string>
#include <vector>

namespace EzEvade {

    enum class SpellSlotId : int {
        Q = 0,
        W = 1,
        E = 2,
        R = 3,
        F = 4,
        T = 5,
        None = -1,
    };

    enum class EvadeType {
        None,
        Blink,
        Dash,
        MovementSpeedBuff,
        SpellShield,
        WindWall,
        Shield,
        Stasis,
        Invulnerability,
        Untargetable,
        Recall,
    };

    enum class CastType {
        None,
        Self,
        Position,
        Target,
    };

    enum class SpellTargets {
        None,
        AllyChampions,
        EnemyChampions,
        AllyMinions,
        EnemyMinions,
        Targetables,
    };

    struct EvadeSpellData;
    using UseSpellFunc = std::function<bool(const EvadeSpellData&, bool)>;

    struct EvadeSpellData {
        std::string charName;
        std::string name;
        std::string spellName;
        SpellSlotId spellKey = SpellSlotId::Q;

        float range = 0.0f;

        float spellDelay = 250.0f;
        float speed = 0.0f;

        std::vector<float> speedArray;
        bool fixedRange = false;

        std::vector<SpellTargets> spellTargets;

        EvadeType evadeType = EvadeType::None;
        CastType castType = CastType::Self;

        bool isReversed = false;
        bool behindTarget = false;
        bool infrontTarget = false;
        bool untargetable = false;
        bool isSpecial = false;
        bool checkSpellName = false;
        bool isSummonerSpell = false;
        bool isItem = false;
        int itemID = 0;
        UseSpellFunc useSpellFunc = nullptr;

        int dangerlevel = 1;
    };

} // namespace EzEvade
