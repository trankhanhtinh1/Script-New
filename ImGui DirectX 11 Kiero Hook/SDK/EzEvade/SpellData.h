#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ============================================================================
// SpellData / EvadeSpellData — EzEvade Spell & Evade Spell Definitions
//
// Sources:
//   EzEvade/Spells/SpellDatabase.cs  (Hellsing — skillshot defs)
//   EzEvade/EvadeSpells/EvadeSpellDatabase.cs (Hellsing — evade skill defs)
//   SpellDatabase.lua                (Hanbot   — targeted + hash data)
// ============================================================================

namespace EzEvade {

    // =========================================================================
    // Spell slot
    // =========================================================================
    enum class SpellSlotId : int {
        Q    = 0,
        W    = 1,
        E    = 2,
        R    = 3,
        F    = 4,   // Summoner Spell 1 (thường là Flash)
        T    = 5,   // Summoner Spell 2
        None = -1,
    };

    // =========================================================================
    // Enums — Spell types
    // =========================================================================
    enum class SpellType {
        None,
        Line,           // Đường thẳng (Lucian Q, Lux Q...)
        Circular,       // Vòng tròn (Amumu R, Brand W...)
        Cone,           // Hình nón (Cassio R, Annie W...)
        Arc,            // Cung (Diana Q...)
        Ring,           // Vòng ring (Zac...)
        MissileLinear,  // Linear missile (theo dõi qua object)
        MissileArc,     // Arc missile
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
        Blink,              // Tức thời (Flash, Ezreal E)
        Dash,               // Lướt (Lucian E, Gragas E)
        MovementSpeedBuff,  // Tăng tốc (Draven W, Ghost)
        SpellShield,        // Chắn 1 skill (Sivir E, Nocturne W)
        WindWall,           // Chắn projectile (Yasuo W)
        Shield,             // Giáp máu (Lee Sin W)
        Stasis,             // Bất tử (Zhonya, Kayle R)
        Untargetable,       // Không thể bị nhắm (Elise E)
        Recall,             // Về nhà
    };

    enum class CastType {
        None,
        Self,       // Dùng lên bản thân
        Position,   // Dùng về phía / tại vị trí
        Target,     // Dùng lên target cụ thể
    };

    enum class SpellTargets {
        None,
        AllyChampions,
        EnemyChampions,
        AllyMinions,
        EnemyMinions,
        Targetables,    // Bất kỳ object có thể nhắm
    };

    enum class DetectionType {
        Missile,        // Phát hiện qua missile object (projectile)
        CastSpell,      // Phát hiện qua OnProcessSpellCast
        Buff,           // Phát hiện qua buff được thêm
        EffectEmitter,  // Phát hiện qua particle / emitter
        Spell,          // Phát hiện qua tên spell cast
        LogicKing,      // Custom logic
        HaveOtherState, // Phát hiện khi sender có buff/state khác
    };

    enum class CCType {
        None,
        Soft,   // Slow, silence, ground
        Hard,   // Stun, root, knock
    };

    // =========================================================================
    // SpellData — Định nghĩa skillshot cần né
    // Dựa trên EzEvade SpellDatabase.cs::SpellData
    // =========================================================================
    struct SpellData {
        // --- Identification ---
        std::string charName;       // Tên champion, "AllChampions" cho global
        std::string name;           // Tên skill đọc được
        std::string spellName;      // Internal spell name (OnProcessSpellCast)
        std::string missileName;    // Missile name (nếu là projectile)
        std::vector<std::string> extraSpellNames;   // Tên spell phụ
        std::vector<std::string> extraMissileNames; // Tên missile phụ
        SpellSlotId spellKey = SpellSlotId::Q;

        // --- Geometry ---
        SpellType   spellType       = SpellType::Line;
        float       radius          = 50.0f;    // Bán kính / nửa chiều rộng
        float       range           = 1000.0f;  // Tầm bắn tối đa
        float       angle           = 0.0f;     // Góc hình nón (độ)
        float       secondaryRadius = 0.0f;     // Bán kính nổ ở cuối (Arc)

        // --- Timing ---
        float       spellDelay      = 250.0f;   // Delay trước khi bay (ms)
        float       projectileSpeed = 0.0f;     // Tốc độ missile (0 = instant)
        float       extraEndTime    = 0.0f;     // Thời gian tồn tại thêm (ms)
        float       extraDelay      = 0.0f;     // Delay phát hiện thêm (ms)

        // --- Visuals ---
        float       extraDrawHeight = 0.0f;     // Offset Y khi vẽ

        // --- Behaviour ---
        bool        fixedRange      = false;    // Luôn đi đủ range
        bool        hasTrap         = false;    // Tạo bẫy (trap object)
        bool        hasEndExplosion = false;    // Nổ ở điểm cuối (Arc)
        bool        isPerpendicular = false;    // Line vuông góc ở cuối
        bool        isSpecial       = false;    // Cần xử lý đặc biệt
        bool        noProcess       = false;    // Không detect qua ProcessSpell
        bool        usePackets      = false;    // Dùng packet detection
        bool        updatePosition  = true;     // Missile đuổi theo target
        bool        invert          = false;    // Ngược chiều (Yasuo Q)
        bool        defaultOff      = false;    // Tắt mặc định trong menu
        bool        useEndPosition  = false;    // Dùng EndPos làm hướng
        bool        isTeleport      = false;    // Global / teleport spell

        // --- Collision ---
        std::vector<CollisionObjectType> collisionObjects;

        // --- Trap ---
        std::string trapBaseName;   // Tên object bẫy (Caitlyn W trap)
        std::string trapTroyName;   // Emitter .troy (Zilean Q)

        // --- Danger & Detection ---
        int           dangerlevel    = 1;                       // 1(thấp)..5(chết)
        DetectionType detectionType  = DetectionType::CastSpell;
        CCType        ccType         = CCType::None;
        std::vector<uint32_t> hashes;   // Missile/spell hash (Hanbot data)
        bool          isAutoAttack   = false;
    };

    // =========================================================================
    // EvadeSpellData — Skill của champion mình dùng để né
    // Dựa trên EzEvade EvadeSpellDatabase.cs::EvadeSpellData
    // =========================================================================
    struct EvadeSpellData {
        // --- Identification ---
        std::string charName;   // Tên champion
        std::string name;       // Tên dễ đọc
        std::string spellName;  // Internal spell name
        SpellSlotId spellKey = SpellSlotId::Q;

        // --- Evade Properties ---
        EvadeType   evadeType  = EvadeType::None;
        CastType    castType   = CastType::Self;

        // --- Geometry ---
        float       range      = 0.0f;     // Tầm (Blink/Dash)
        float       speed      = 0.0f;     // Tốc độ lướt (0 = instant)
        bool        fixedRange = false;    // Luôn đi đủ range

        // --- Timing ---
        float       spellDelay = 250.0f;   // Delay cast (ms)

        // --- Speed Buff ---
        // Mảng tốc độ thêm theo level (cho MovementSpeedBuff)
        std::vector<float> speedArray;     // e.g. {40, 45, 50, 55, 60}

        // --- Targets ---
        std::vector<SpellTargets> spellTargets; // Target hợp lệ

        // --- Behaviour ---
        bool        isReversed      = false;  // Dash ngược hướng (Caitlyn E)
        bool        infrontTarget   = false;  // Dash trước mặt target
        bool        untargetable    = false;  // Không thể bị nhắm khi lướt
        bool        isSpecial       = false;  // Cần logic đặc biệt
        bool        checkSpellName  = false;  // Validate theo tên spell
        bool        isSummonerSpell = false;  // Là summoner spell
        bool        isItem          = false;  // Là item active
        int         itemID          = 0;      // Item ID

        // --- Priority ---
        int         dangerlevel = 1; // Mức nguy hiểm tối thiểu để dùng skill này
    };

} // namespace EzEvade
