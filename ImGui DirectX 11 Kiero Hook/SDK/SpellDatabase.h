#pragma once
#include <string>
#include <unordered_map>

namespace SpellDatabase
{
    enum class SpellType
    {
        Linear,
        Circular,
        Cone,
        Targeted
    };

    struct SpellInfo
    {
        std::string name;
        float speed;
        float radius;
        float width;
        float range;
        SpellType type;
        float castTime;

        SpellInfo() : speed(0), radius(0), width(0), range(0), type(SpellType::Linear), castTime(0) {}
        SpellInfo(const std::string& n, float s, float r, float w, float rg, SpellType t, float ct = 0.25f)
            : name(n), speed(s), radius(r), width(w), range(rg), type(t), castTime(ct) {}
    };

    inline std::unordered_map<std::string, SpellInfo>& GetSpellsMap()
    {
        static std::unordered_map<std::string, SpellInfo> spells = {
            {"EzrealMysticShot", SpellInfo("EzrealMysticShot", 2000.0f, 60.0f, 60.0f, 1200.0f, SpellType::Linear, 0.25f)},
            {"EzrealQ", SpellInfo("EzrealQ", 2000.0f, 60.0f, 60.0f, 1200.0f, SpellType::Linear, 0.25f)},
            {"EzrealEssenceFlux", SpellInfo("EzrealEssenceFlux", 1700.0f, 80.0f, 80.0f, 1150.0f, SpellType::Linear, 0.25f)},
            {"EzrealW", SpellInfo("EzrealW", 1700.0f, 80.0f, 80.0f, 1150.0f, SpellType::Linear, 0.25f)},
            {"EzrealTrueshotBarrage", SpellInfo("EzrealTrueshotBarrage", 2000.0f, 160.0f, 160.0f, 25000.0f, SpellType::Linear, 1.0f)},
            {"EzrealR", SpellInfo("EzrealR", 2000.0f, 160.0f, 160.0f, 25000.0f, SpellType::Linear, 1.0f)},
            {"JinxW", SpellInfo("JinxW", 3300.0f, 60.0f, 60.0f, 1500.0f, SpellType::Linear, 0.6f)},
            {"JinxWMissile", SpellInfo("JinxWMissile", 3300.0f, 60.0f, 60.0f, 1500.0f, SpellType::Linear, 0.6f)},
            {"JinxR", SpellInfo("JinxR", 1700.0f, 140.0f, 140.0f, 25000.0f, SpellType::Linear, 0.6f)},
            {"LuxQ", SpellInfo("LuxQ", 1200.0f, 70.0f, 70.0f, 1175.0f, SpellType::Linear, 0.25f)},
            {"LuxLightBinding", SpellInfo("LuxLightBinding", 1200.0f, 70.0f, 70.0f, 1175.0f, SpellType::Linear, 0.25f)},
            {"LuxE", SpellInfo("LuxE", 1200.0f, 310.0f, 310.0f, 1100.0f, SpellType::Circular, 0.25f)},
            {"AhriE", SpellInfo("AhriE", 1550.0f, 60.0f, 60.0f, 1000.0f, SpellType::Linear, 0.25f)},
            {"AhriSeduce", SpellInfo("AhriSeduce", 1550.0f, 60.0f, 60.0f, 1000.0f, SpellType::Linear, 0.25f)},
            {"SmolderW", SpellInfo("SmolderW", 1200.0f, 125.0f, 250.0f, 1500.0f, SpellType::Linear, 0.35f)},
        };
        return spells;
    }

    inline const SpellInfo* GetSpellInfo(const std::string& spellName)
    {
        auto& spells = GetSpellsMap();
        auto it = spells.find(spellName);
        if (it != spells.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const SpellInfo* GetSpellInfo(const char* spellName);

    inline bool HasSpell(const std::string& spellName)
    {
        return GetSpellsMap().find(spellName) != GetSpellsMap().end();
    }

    bool HasSpell(const char* spellName);
}
