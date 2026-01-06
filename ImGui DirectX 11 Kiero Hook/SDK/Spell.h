#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
#include <vector>
#include <cmath>
#include "Offsets.h"
#include "../Vector.h"

namespace SDK
{
    // Forward declaration
    class GameObject;
    
    // ============================================================================
    // SPELL TYPE ENUM (Based on EnsoulSharp.SDK/SkillshotType)
    // ============================================================================
    enum class SpellType
    {
        Targeted,           // Point-and-click spell (no prediction needed)
        Line,               // Linear skillshot (Ezreal Q, Morgana Q)
        Circle,             // Circular AoE (Lux E, Ziggs R)
        Cone,               // Cone/Arc spell (Annie W, Karthus Q)
        Ring,               // Ring shape (Veigar E edge)
        None                // Non-damaging or passive
    };
    
    // ============================================================================
    // COLLISION FLAGS (What the spell can collide with)
    // ============================================================================
    enum CollisionFlags : int
    {
        CollisionNone          = 0,
        CollisionMinions       = (1 << 0),    // Collides with enemy minions
        CollisionChampions     = (1 << 1),    // Collides with enemy champions
        CollisionYasuoWall     = (1 << 2),    // Blocked by Yasuo W
        CollisionBraumShield   = (1 << 3),    // Blocked by Braum E
        CollisionWalls         = (1 << 4),    // Blocked by terrain walls
        CollisionAllyMinions   = (1 << 5),    // Collides with ally minions (rare)
        CollisionAllyChampions = (1 << 6),    // Collides with ally champions (rare)
    };

    // ============================================================================
    // SPELL SLOT CLASS (Memory reading from game)
    // ============================================================================
    class SpellSlot
    {
    public:
        uint64_t Address;
        
        SpellSlot(uint64_t address) : Address(address) {}
        SpellSlot() : Address(0) {}
        
        bool IsValid() const { return Address != 0; }
        
        int GetLevel() {
            if (!IsValid()) return 0;
            return *(int*)(Address + Offset::oLevelSpell);
        }
        
        float GetCooldownExpire() {
            if (!IsValid()) return 0.0f;
            return *(float*)(Address + Offset::oCooldownExpire);
        }
        
        float GetTotalCooldown() {
            if (!IsValid()) return 0.0f;
            return *(float*)(Address + Offset::oSpellTotalCooldown);
        }
        
        float GetManaCost() {
             if (!IsValid()) return 0.0f;
             uint64_t info = *(uint64_t*)(Address + Offset::oSpellInfo);
             if (!info) return 0.0f;
             
             uint64_t data = *(uint64_t*)(info + Offset::oSpellIData);
             if (!data) return 0.0f;
             
             return *(float*)(data + Offset::ManaCosSpell);
        }
        
        std::string GetName() {
             if (!IsValid()) return "";
             uint64_t info = *(uint64_t*)(Address + Offset::oSpellInfo);
             if (!info) return "";
             
             uint64_t data = *(uint64_t*)(info + Offset::oSpellIData);
             if (!data) return "";
             
             return std::string((char*)(data + Offset::NameSpell));
        }
        
        bool IsReady(float gameTime) {
            return GetLevel() > 0 && GetCooldownExpire() <= gameTime;
        }
        
        float GetRemainingCooldown(float gameTime) {
            float expire = GetCooldownExpire();
            return expire > gameTime ? expire - gameTime : 0.0f;
        }
        
        // Get spell cast range from memory
        // NOTE: CastRange requires function call (o获取技能范围), not simple offset read
        // TODO: Implement GetSpellRange function call using pattern: E8 ? ? ? ? E9 ? ? ? ? 8B 47 ? 0F 29 7c 24 ? 44 0f
        // For now, returns 0 to fallback to SpellFactory or default ranges
        float GetCastRange() {
            return 0.0f; // Needs GetSpellRange function implementation
        }
    };
    
    // ============================================================================
    // SPELL BOOK CLASS (Memory reading from game)
    // ============================================================================
    class SpellBook
    {
    public:
        uint64_t Address;
        
        SpellBook(uint64_t address) : Address(address) {}
        SpellBook() : Address(0) {}
        
        bool IsValid() const { return Address != 0; }
        
        // SlotID: 0=Q, 1=W, 2=E, 3=R, 4=Sum1, 5=Sum2, etc.
        SpellSlot GetSpell(int slotId) {
            if (!IsValid()) return SpellSlot(0);
            
            uint64_t slotArray = Address + Offset::oObjSpellBookSpellSlot;
            uint64_t slotAddr = *(uint64_t*)(slotArray + (slotId * 0x8));
            
            return SpellSlot(slotAddr);
        }
        
        // Convenience accessors
        SpellSlot Q() { return GetSpell(0); }
        SpellSlot W() { return GetSpell(1); }
        SpellSlot E() { return GetSpell(2); }
        SpellSlot R() { return GetSpell(3); }
        SpellSlot Summoner1() { return GetSpell(4); }
        SpellSlot Summoner2() { return GetSpell(5); }
    };

    // ============================================================================
    // SPELL DATA CLASS - Used for Prediction
    // Defines static properties of a spell (Range, Speed, Width, etc.)
    // Based on EnsoulSharp.SDK/Core/Wrappers/Spells/Spell.cs
    // ============================================================================
    class SpellData
    {
    public:
        // Spell identification
        int SlotId;                 // 0=Q, 1=W, 2=E, 3=R
        std::string Name;           // Spell name (e.g., "EzrealQ")
        
        // Skillshot properties
        SpellType Type;             // Line, Circle, Cone, Targeted
        float Range;                // Max cast range (units)
        float Speed;                // Missile speed (units/sec, 0 = instant)
        float CastDelay;            // Cast time/wind-up (seconds)
        float Width;                // Spell width/radius (units)
        float Radius;               // Same as Width (alias for circle spells)
        
        // Collision settings
        int CollisionMask;         // What the spell collides with
        
        // Optional parameters
        float CastRange;            // If different from Range
        float MinRange;             // Minimum cast range (e.g., Varus Q)
        bool IsChargedSpell;        // Chargeable spell (Varus Q, Xerath Q)
        float ChargeTime;           // Max charge duration
        
        // Constructors
        SpellData() :
            SlotId(-1),
            Name(""),
            Type(SpellType::None),
            Range(0),
            Speed(0),
            CastDelay(0),
            Width(0),
            Radius(0),
            CollisionMask(CollisionNone),
            CastRange(0),
            MinRange(0),
            IsChargedSpell(false),
            ChargeTime(0)
        {}
        
        SpellData(int slotId, float range, SpellType type = SpellType::Targeted) :
            SlotId(slotId),
            Name(""),
            Type(type),
            Range(range),
            Speed(0),
            CastDelay(0.25f), // Default cast delay
            Width(0),
            Radius(0),
            CollisionMask(CollisionNone),
            CastRange(range),
            MinRange(0),
            IsChargedSpell(false),
            ChargeTime(0)
        {}
        
        // Builder pattern for fluent API
        SpellData& SetRange(float range) {
            Range = range;
            CastRange = range;
            return *this;
        }
        
        SpellData& SetSpeed(float speed) {
            Speed = speed;
            return *this;
        }
        
        SpellData& SetDelay(float delay) {
            CastDelay = delay;
            return *this;
        }
        
        SpellData& SetWidth(float width) {
            Width = width;
            Radius = width;
            return *this;
        }
        
        SpellData& SetRadius(float radius) {
            Radius = radius;
            Width = radius;
            return *this;
        }
        
        SpellData& SetType(SpellType type) {
            Type = type;
            return *this;
        }
        
        SpellData& SetCollision(int flags) {
            CollisionMask = flags;
            return *this;
        }
        
        SpellData& AddCollision(CollisionFlags flag) {
            CollisionMask |= flag;
            return *this;
        }
        
        SpellData& SetName(const std::string& name) {
            Name = name;
            return *this;
        }
        
        SpellData& SetCharged(float chargeTime, float minRange) {
            IsChargedSpell = true;
            ChargeTime = chargeTime;
            MinRange = minRange;
            return *this;
        }
        
        // Utility methods
        bool IsSkillshot() const {
            return Type != SpellType::Targeted && Type != SpellType::None;
        }
        
        bool HasCollision() const {
            return CollisionMask != CollisionNone;
        }
        
        bool CollidesWithMinions() const {
            return (CollisionMask & CollisionMinions) != 0;
        }
        
        bool CollidesWithChampions() const {
            return (CollisionMask & CollisionChampions) != 0;
        }
        
        bool IsBlockedByWindWall() const {
            return (CollisionMask & CollisionYasuoWall) != 0;
        }
        
        // Calculate travel time to a position
        float GetTravelTime(float distance) const {
            if (Speed <= 0) return CastDelay;
            return CastDelay + (distance / Speed);
        }
        
        // Get full cast time (for instant spells)
        float GetCastTime() const {
            return CastDelay;
        }
    };

    // ============================================================================
    // SPELL CLASS - Combines SpellData with SpellSlot for runtime use
    // Used by champion scripts to cast spells with prediction
    // ============================================================================
    class Spell
    {
    public:
        SpellData Data;
        
        Spell() {}
        
        Spell(int slotId, float range) {
            Data.SlotId = slotId;
            Data.Range = range;
            Data.CastRange = range;
            Data.Type = SpellType::Targeted;
        }
        
        // Create a line skillshot
        static Spell CreateLine(int slotId, float range, float speed, float width, float delay = 0.25f) {
            Spell spell;
            spell.Data.SlotId = slotId;
            spell.Data.Type = SpellType::Line;
            spell.Data.Range = range;
            spell.Data.CastRange = range;
            spell.Data.Speed = speed;
            spell.Data.Width = width;
            spell.Data.Radius = width / 2.0f;
            spell.Data.CastDelay = delay;
            return spell;
        }
        
        // Create a circular AoE
        static Spell CreateCircle(int slotId, float range, float radius, float speed = 0, float delay = 0.25f) {
            Spell spell;
            spell.Data.SlotId = slotId;
            spell.Data.Type = SpellType::Circle;
            spell.Data.Range = range;
            spell.Data.CastRange = range;
            spell.Data.Speed = speed;
            spell.Data.Width = radius * 2.0f;
            spell.Data.Radius = radius;
            spell.Data.CastDelay = delay;
            return spell;
        }
        
        // Create a cone/arc spell
        static Spell CreateCone(int slotId, float range, float angle, float delay = 0.25f) {
            Spell spell;
            spell.Data.SlotId = slotId;
            spell.Data.Type = SpellType::Cone;
            spell.Data.Range = range;
            spell.Data.CastRange = range;
            spell.Data.Width = angle; // Width as angle for cone
            spell.Data.CastDelay = delay;
            return spell;
        }
        
        // Builder pattern
        Spell& SetRange(float range) { Data.SetRange(range); return *this; }
        Spell& SetSpeed(float speed) { Data.SetSpeed(speed); return *this; }
        Spell& SetDelay(float delay) { Data.SetDelay(delay); return *this; }
        Spell& SetWidth(float width) { Data.SetWidth(width); return *this; }
        Spell& SetRadius(float radius) { Data.SetRadius(radius); return *this; }
        Spell& SetCollision(int flags) { Data.SetCollision(flags); return *this; }
        Spell& AddCollision(CollisionFlags flag) { Data.AddCollision(flag); return *this; }
        Spell& SetCharged(float chargeTime, float minRange) { Data.SetCharged(chargeTime, minRange); return *this; }
        
        // Accessors
        int GetSlot() const { return Data.SlotId; }
        float GetRange() const { return Data.Range; }
        float GetSpeed() const { return Data.Speed; }
        float GetDelay() const { return Data.CastDelay; }
        float GetWidth() const { return Data.Width; }
        float GetRadius() const { return Data.Radius; }
        SpellType GetType() const { return Data.Type; }
        
        bool IsSkillshot() const { return Data.IsSkillshot(); }
        bool HasCollision() const { return Data.HasCollision(); }
        bool CollidesWithMinions() const { return Data.CollidesWithMinions(); }
        bool CollidesWithChampions() const { return Data.CollidesWithChampions(); }
        
        // Check if target is in range
        bool IsInRange(Vector3 sourcePos, Vector3 targetPos) const {
            float dx = targetPos.x - sourcePos.x;
            float dz = targetPos.z - sourcePos.z;
            float distSq = dx * dx + dz * dz;
            return distSq <= Data.Range * Data.Range;
        }
        
        // Get travel time to target
        float GetTravelTime(Vector3 sourcePos, Vector3 targetPos) const {
            float dx = targetPos.x - sourcePos.x;
            float dz = targetPos.z - sourcePos.z;
            float distance = std::sqrt(dx * dx + dz * dz);
            return Data.GetTravelTime(distance);
        }
    };
    
    // ============================================================================
    // COMMON SPELL FACTORY - Pre-defined spells for popular champions
    // Usage: auto Q = SpellFactory::EzrealQ();
    // ============================================================================
    namespace SpellFactory
    {
        // --- Example Spells (Add more as needed) ---
        
        // Ezreal Q - Mystic Shot
        inline Spell EzrealQ() {
            // Wiki: Range 1200 (Reduced to 1150 for hit accuracy)
            return Spell::CreateLine(0, 1150, 2000, 120, 0.25f)
                .SetCollision(CollisionMinions | CollisionChampions | CollisionYasuoWall);
        }
        
        // Ezreal W - Essence Flux
        inline Spell EzrealW() {
            // Wiki: Range 1200 (Reduced to 1150 for hit accuracy)
            return Spell::CreateLine(1, 1150, 1700, 160, 0.25f)
                .SetCollision(CollisionYasuoWall); // W ignores minions
        }
        
        // Ezreal E - Arcane Shift
        inline Spell EzrealE() {
            // Wiki: Blink Range 475, Effect Radius 750
            return Spell::CreateCircle(2, 475, 750, 0, 0.25f);
        }
        
        // Ezreal R - Trueshot Barrage
        inline Spell EzrealR() {
            // Wiki: Range Global (20000), Speed 2000, Width 320 (Radius 160), Delay 1.0
            return Spell::CreateLine(3, 20000, 2000, 320, 1.0f)
                .SetCollision(CollisionYasuoWall);
        }
        
        // Morgana Q - Dark Binding
        inline Spell MorganaQ() {
            return Spell::CreateLine(0, 1175, 1200, 70, 0.25f)
                .SetCollision(CollisionMinions | CollisionChampions | CollisionYasuoWall);
        }
        
        // Lux Q - Light Binding
        inline Spell LuxQ() {
            return Spell::CreateLine(0, 1175, 1200, 70, 0.25f)
                .SetCollision(CollisionChampions | CollisionYasuoWall); // Can hit 2 targets
        }
        
        // Lux E - Lucent Singularity
        inline Spell LuxE() {
            return Spell::CreateCircle(2, 1100, 310, 1200, 0.25f);
        }
        
        // Lux R - Final Spark
        inline Spell LuxR() {
            return Spell::CreateLine(3, 3340, 0, 110, 1.0f); // Speed 0 = instant
        }
        
        // Jinx W - Zap!
        inline Spell JinxW() {
            return Spell::CreateLine(1, 1450, 3300, 60, 0.6f)
                .SetCollision(CollisionMinions | CollisionChampions | CollisionYasuoWall);
        }
        
        // Jinx R - Super Mega Death Rocket
        inline Spell JinxR() {
            return Spell::CreateLine(3, 25000, 1700, 140, 0.6f)
                .SetCollision(CollisionChampions | CollisionYasuoWall);
        }
        
        // Blitzcrank Q - Rocket Grab
        inline Spell BlitzcrankQ() {
            return Spell::CreateLine(0, 1150, 1800, 70, 0.25f)
                .SetCollision(CollisionMinions | CollisionChampions | CollisionYasuoWall);
        }
        
        // Thresh Q - Death Sentence
        inline Spell ThreshQ() {
            return Spell::CreateLine(0, 1100, 1900, 70, 0.5f)
                .SetCollision(CollisionMinions | CollisionChampions | CollisionYasuoWall);
        }
        
        // Ahri E - Charm
        inline Spell AhriE() {
            return Spell::CreateLine(2, 975, 1550, 60, 0.25f)
                .SetCollision(CollisionMinions | CollisionChampions | CollisionYasuoWall);
        }
        
        // Brand W - Pillar of Flame
        inline Spell BrandW() {
            return Spell::CreateCircle(1, 900, 250, 0, 0.85f); // 0.85s delay
        }
        
        // Xerath Q - Arcanopulse (Charged)
        inline Spell XerathQ() {
            return Spell::CreateLine(0, 1450, 0, 95, 0.5f) // Instant after charge
                .SetCharged(1.5f, 700);
        }
        
        // Varus Q - Piercing Arrow (Charged)
        inline Spell VarusQ() {
            return Spell::CreateLine(0, 1625, 1850, 70, 0.0f)
                .SetCharged(1.25f, 925)
                .SetCollision(CollisionYasuoWall);
        }

        // ========================================================================
        // DYNAMIC RETRIEVAL
        // ========================================================================
        inline Spell GetSpell(const std::string& championName, int slot) {
            // Helper macro for common pattern
            #define CHECK_CHAMP(name) if (championName == name)

            CHECK_CHAMP("Ezreal") {
                if (slot == 0) return EzrealQ();
                if (slot == 1) return EzrealW();
                if (slot == 2) return EzrealE();
                if (slot == 3) return EzrealR();
            }
            CHECK_CHAMP("Jinx") {
                if (slot == 1) return JinxW();
                if (slot == 3) return JinxR();
            }
            CHECK_CHAMP("Lux") {
                if (slot == 0) return LuxQ();
                if (slot == 2) return LuxE();
                if (slot == 3) return LuxR();
            }
            CHECK_CHAMP("Morgana") {
                if (slot == 0) return MorganaQ();
            }
            CHECK_CHAMP("Blitzcrank") {
                if (slot == 0) return BlitzcrankQ();
            }
            CHECK_CHAMP("Thresh") {
                if (slot == 0) return ThreshQ();
            }
            CHECK_CHAMP("Ahri") {
                if (slot == 2) return AhriE();
            }
            CHECK_CHAMP("Brand") {
                if (slot == 1) return BrandW();
            }
            CHECK_CHAMP("Xerath") {
                if (slot == 0) return XerathQ();
            }
            CHECK_CHAMP("Varus") {
                if (slot == 0) return VarusQ();
            }
            
            return Spell(); // Return invalid/empty spell
        }
    }
}
