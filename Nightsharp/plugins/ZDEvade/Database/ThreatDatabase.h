#pragma once
#include "SpellData.h"
#include "SpellDatabase.h"
#include <algorithm>
#include <cstring>

namespace ZDEvade {

class ThreatDatabase {
public:
    static void Initialize() {
        SpellDatabase::Initialize();
    }

    static std::size_t Count() {
        return SpellDatabase::Spells.size();
    }

    static const SpellData* FindAny(const char* name, const char* championName) {
        if (!name || !name[0]) return nullptr;
        for (const auto& s : SpellDatabase::Spells) {
            if (s.charName != championName && s.charName != "AllChampions")
                continue;
            if (MatchName(s.spellName, name)) return &s;
            if (!s.missileName.empty() && MatchName(s.missileName, name)) return &s;
            for (const auto& es : s.extraSpellNames)
                if (MatchName(es, name)) return &s;
            for (const auto& em : s.extraMissileNames)
                if (MatchName(em, name)) return &s;
        }
        return nullptr;
    }

    static const SpellData* FindMissile(const char* name, const char* championName) {
        if (!name || !name[0]) return nullptr;
        for (const auto& s : SpellDatabase::Spells) {
            if (s.charName != championName && s.charName != "AllChampions")
                continue;
            if (!s.missileName.empty() && MatchName(s.missileName, name)) return &s;
            for (const auto& em : s.extraMissileNames)
                if (MatchName(em, name)) return &s;
        }
        return nullptr;
    }

    static const SpellData* FindCast(const char* spellSlotName, const char* championName) {
        if (!spellSlotName || !spellSlotName[0]) return nullptr;
        for (const auto& s : SpellDatabase::Spells) {
            if (s.charName != championName && s.charName != "AllChampions")
                continue;
            if (MatchName(s.spellName, spellSlotName)) return &s;
            for (const auto& es : s.extraSpellNames)
                if (MatchName(es, spellSlotName)) return &s;
        }
        return nullptr;
    }

private:
    static bool MatchName(const std::string& a, const char* b) {
        if (a.empty() || !b || !b[0]) return false;
#ifdef _WIN32
        return _stricmp(a.c_str(), b) == 0;
#else
        return strcasecmp(a.c_str(), b) == 0;
#endif
    }
};

} // namespace ZDEvade
