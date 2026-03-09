#pragma once
#include "sdk/EzEvade/Spells/SpellDatabase.h"
#include <string>
#include <vector>

namespace EzEvade {

class SpellWindupDatabase {
public:
    static inline std::vector<SpellData> Spells = Build();

private:
    static std::vector<SpellData> Build() {
        std::vector<SpellData> db;
        db.reserve(GetSpellDatabase().size());

        // Original C# project keeps a dedicated list for windup delay
        // correction. This C++ port derives it from the live spell database
        // so it stays patch-aligned while preserving the same usage pattern.
        for (const auto& s : GetSpellDatabase()) {
            if (s.spellName.empty()) continue;
            if (s.charName.empty()) continue;
            if (s.spellDelay <= 0.0f) continue;

            SpellData copy = s;
            copy.radius = 0.0f;
            copy.range = 0.0f;
            copy.projectileSpeed = 0.0f;
            copy.collisionObjects.clear();
            db.push_back(std::move(copy));
        }

        return db;
    }
};

} // namespace EzEvade

