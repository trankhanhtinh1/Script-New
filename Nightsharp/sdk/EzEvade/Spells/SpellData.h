#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// ============================================================================
// EzEvade spell data models
// Sources:
//   EzEvade/Spells/SpellDatabase.cs
//   EzEvade/EvadeSpells/EvadeSpellDatabase.cs
//   SpellDatabase.lua
// ============================================================================

namespace EzEvade {

    // Skill slots used by champion spells and summoner spells.
    enum class SpellSlotId : int {
        Q = 0,
        W = 1,
        E = 2,
        R = 3,
        F = 4,   // Summoner slot 1
        T = 5,   // Summoner slot 2
        None = -1,
    };

    // Basic spell geometry used by evade and drawing.
    enum class SpellType {
        None,
        Line,
        Circular,
        Cone,
        Arc,
        Ring,
        MissileLinear,
        MissileArc,
    };

    enum class CollisionObjectType {
        None,
        EnemyChampions,
        EnemyMinions,
        AllyMinions,
        YasuoWall,
        Terrain,
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

    enum class DetectionType {
        Missile,
        CastSpell,
        Buff,
        EffectEmitter,
        Spell,
        LogicKing,
        HaveOtherState,
    };

    enum class CCType {
        None,
        Soft,
        Hard,
    };

    struct EvadeSpellData;
    using UseSpellFunc = std::function<bool(const EvadeSpellData&, bool)>;

    // Enemy spell entry used by evade detection.
    struct SpellData {
        // Identity
        std::string charName;       // Champion name. "AllChampions" = global entry.
        std::string name;           // Readable display name.
        std::string spellName;      // Internal cast name.
        std::string missileName;    // Internal missile name.
        std::vector<std::string> extraSpellNames;
        std::vector<std::string> extraMissileNames;
        SpellSlotId spellKey = SpellSlotId::Q;

        // Geometry
        SpellType   spellType       = SpellType::Line;
        float       radius          = 50.0f;
        float       range           = 1000.0f;
        float       angle           = 0.0f;
        float       secondaryRadius = 0.0f;
        float       sideRadius      = 0.0f;

        // Timing
        float       spellDelay      = 250.0f;
        float       projectileSpeed = 0.0f;
        float       extraEndTime    = 0.0f;
        float       extraDelay      = 0.0f;
        float       extraDistance   = 0.0f;

        // Drawing
        float       extraDrawHeight = 0.0f;

        // Behavior
        bool        fixedRange      = false;
        bool        hasTrap         = false;
        bool        hasEndExplosion = false;
        bool        isThreeWay      = false;
        bool        isWall          = false;
        bool        isPerpendicular = false;
        bool        isSpecial       = false;
        bool        noProcess       = false;
        bool        usePackets      = false;
        bool        updatePosition  = true;
        bool        invert          = false;
        bool        defaultOff      = false;
        bool        useEndPosition  = false;
        bool        isTeleport      = false;

        // Collision and spawned objects
        std::vector<CollisionObjectType> collisionObjects;
        std::string trapBaseName;
        std::string trapTroyName;

        // Threat and detection metadata
        int           dangerlevel   = 1;
        DetectionType detectionType = DetectionType::CastSpell;
        CCType        ccType        = CCType::None;
        std::vector<uint32_t> hashes;
        bool          isAutoAttack  = false;

        SpellData Clone() const {
            return *this;
        }
    };

    // Defensive spell entry used by evade responses.
    struct EvadeSpellData {
        // Identity
        std::string charName;
        std::string name;
        std::string spellName;
        SpellSlotId spellKey = SpellSlotId::Q;

        // Geometry
        float       range      = 0.0f;

        // Timing
        float       spellDelay = 250.0f;
        float       speed      = 0.0f;

        // Movement speed buff per spell level
        std::vector<float> speedArray;
        bool        fixedRange = false;

        // Valid spell targets
        std::vector<SpellTargets> spellTargets;

        // Evade behavior
        EvadeType   evadeType  = EvadeType::None;
        CastType    castType   = CastType::Self;

        // Behavior flags
        bool        isReversed      = false;
        bool        behindTarget    = false;
        bool        infrontTarget   = false;
        bool        untargetable    = false;
        bool        isSpecial       = false;
        bool        checkSpellName  = false;
        bool        isSummonerSpell = false;
        bool        isItem          = false;
        int         itemID          = 0;
        UseSpellFunc useSpellFunc   = nullptr;

        // Minimum threat level before using this evade tool
        int         dangerlevel = 1;
    };

} // namespace EzEvade
