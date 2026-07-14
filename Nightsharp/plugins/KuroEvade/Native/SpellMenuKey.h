#pragma once

#include "Database/SpellData.h"

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace Plugins::KuroEvade::SpellMenuKey {

inline std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline std::string Sanitize(std::string value) {
    for (char& c : value) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc)) {
            c = '_';
        }
    }
    if (value.empty() || std::isdigit(static_cast<unsigned char>(value.front()))) {
        value.insert(value.begin(), '_');
    }
    return value;
}

inline std::string DisplayName(const SDK::SpellDatabaseEntry& data) {
    if (!data.SpellName.empty()) {
        return data.SpellName;
    }
    if (!data.MissileSpellName.empty()) {
        return data.MissileSpellName;
    }
    return data.ChampionName.empty() ? std::string("UnknownSpell") : data.ChampionName + "Spell";
}

inline std::string DisplayName(const Database::SpellData& data) {
    return data.DisplayName.empty() ? DisplayName(data.Runtime) : data.DisplayName;
}

inline std::string Key(const SDK::SpellDatabaseEntry& data) {
    return Lower(data.ChampionName + "|" + DisplayName(data) + "|" + data.MissileSpellName);
}

inline std::string Key(const Database::SpellData& data) {
    return Key(data.Runtime);
}

inline const char* SlotName(SDK::SpellSlot slot) {
    switch (slot) {
    case SDK::SpellSlot::Q: return "Q";
    case SDK::SpellSlot::W: return "W";
    case SDK::SpellSlot::E: return "E";
    case SDK::SpellSlot::R: return "R";
    case SDK::SpellSlot::Summoner1: return "D";
    case SDK::SpellSlot::Summoner2: return "F";
    default: return "-";
    }
}

} // namespace Plugins::KuroEvade::SpellMenuKey
