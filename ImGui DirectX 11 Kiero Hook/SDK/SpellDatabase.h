#pragma once
#include <string>
#include <unordered_map>

// ============================================================================
// Spell Database for Evade System
// ============================================================================
// 💡 MỤC ĐÍCH: Lưu thông tin spell (Speed, Radius, Width, Type) để vẽ đường bay
// 💡 Khi detect được SpellInfo name, tự động lookup từ database này
// 💡 Thông tin từ League of Legends Wiki hoặc game data
// ============================================================================

namespace SpellDatabase
{
    enum class SpellType
    {
        Linear,         // Skillshot đi thẳng (EzrealQ, JinxW, etc.)
        Circular,       // Skillshot hình tròn (LuxE, etc.)
        Cone,           // Skillshot hình nón (AnnieW, etc.)
        Targeted        // Targeted spell (không phải skillshot)
    };

    struct SpellInfo
    {
        std::string name;           // Spell name từ SpellInfo (ví dụ: "EzrealQ", "MysticShot")
        float speed;                // Missile speed (units/s)
        float radius;               // Collision radius
        float width;                 // Skillshot width (Edge range)
        float range;                // Target range (Centered range)
        SpellType type;              // Spell type
        float castTime;              // Cast time (seconds)
        
        SpellInfo() : speed(0), radius(0), width(0), range(0), type(SpellType::Linear), castTime(0) {}
        SpellInfo(const std::string& n, float s, float r, float w, float rg, SpellType t, float ct = 0.25f)
            : name(n), speed(s), radius(r), width(w), range(rg), type(t), castTime(ct) {}
    };

    // Spell Database Map: SpellName -> SpellInfo
    // 💡 NOTE: Spell name phải match với SpellInfo name từ game (không phải wiki name)
    // 💡 Ví dụ: Wiki name "Mystic Shot" nhưng game name là "EzrealQ"
    inline std::unordered_map<std::string, SpellInfo> spells = {
        // Ezreal Q - Mystic Shot
        {"EzrealMysticShot", SpellInfo("EzrealMysticShot", 2000.0f, 60.0f, 60.0f, 1200.0f, SpellType::Linear, 0.25f)},
        {"EzrealQ", SpellInfo("EzrealQ", 2000.0f, 60.0f, 60.0f, 1200.0f, SpellType::Linear, 0.25f)}, // Alias
        
        // Ezreal W - Essence Flux
        {"EzrealEssenceFlux", SpellInfo("EzrealEssenceFlux", 1700.0f, 80.0f, 80.0f, 1150.0f, SpellType::Linear, 0.25f)},
        {"EzrealW", SpellInfo("EzrealW", 1700.0f, 80.0f, 80.0f, 1150.0f, SpellType::Linear, 0.25f)}, // Alias
        
        // Ezreal R - Trueshot Barrage
        {"EzrealTrueshotBarrage", SpellInfo("EzrealTrueshotBarrage", 2000.0f, 160.0f, 160.0f, 25000.0f, SpellType::Linear, 1.0f)},
        {"EzrealR", SpellInfo("EzrealR", 2000.0f, 160.0f, 160.0f, 25000.0f, SpellType::Linear, 1.0f)}, // Alias
        
        // Jinx W - Zap!
        {"JinxW", SpellInfo("JinxW", 3300.0f, 60.0f, 60.0f, 1500.0f, SpellType::Linear, 0.6f)},
        {"JinxWMissile", SpellInfo("JinxWMissile", 3300.0f, 60.0f, 60.0f, 1500.0f, SpellType::Linear, 0.6f)},
        {"JinxR", SpellInfo("JinxR", 1700.0f, 140.0f, 140.0f, 25000.0f, SpellType::Linear, 0.6f)},

        // Lux Q - Light Binding
        {"LuxQ", SpellInfo("LuxQ", 1200.0f, 70.0f, 70.0f, 1175.0f, SpellType::Linear, 0.25f)},
        {"LuxLightBinding", SpellInfo("LuxLightBinding", 1200.0f, 70.0f, 70.0f, 1175.0f, SpellType::Linear, 0.25f)},
        {"LuxE", SpellInfo("LuxE", 1200.0f, 310.0f, 310.0f, 1100.0f, SpellType::Circular, 0.25f)},

        // Ahri E - Charm
        {"AhriE", SpellInfo("AhriE", 1550.0f, 60.0f, 60.0f, 1000.0f, SpellType::Linear, 0.25f)},
        {"AhriSeduce", SpellInfo("AhriSeduce", 1550.0f, 60.0f, 60.0f, 1000.0f, SpellType::Linear, 0.25f)},

        // Smolder W - Achoo!
        {"SmolderW", SpellInfo("SmolderW", 1200.0f, 125.0f, 250.0f, 1500.0f, SpellType::Linear, 0.35f)},

        // Add more spells here...
    };

    // Lookup spell info by name
    inline const SpellInfo* GetSpellInfo(const std::string& spellName)
    {
        auto it = spells.find(spellName);
        if (it != spells.end()) {
            return &it->second;
        }
        return nullptr;
    }

    // Lookup spell info by name (const char* version to avoid C2712 in SEH functions)
    // 💡 Implementation in SpellDatabase.cpp
    const SpellInfo* GetSpellInfo(const char* spellName);

    // Check if spell exists in database
    inline bool HasSpell(const std::string& spellName)
    {
        return spells.find(spellName) != spells.end();
    }

    // Check if spell exists in database (const char* version)
    // 💡 Implementation in SpellDatabase.cpp
    bool HasSpell(const char* spellName);
}

