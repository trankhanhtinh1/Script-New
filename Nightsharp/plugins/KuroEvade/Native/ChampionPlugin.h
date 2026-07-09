#pragma once

#include "SpellData.h"

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace Plugins::KuroEvade {

struct ChampionPlugin {
    virtual ~ChampionPlugin() = default;
    virtual void LoadSpecialSpell(const Generated::SpellDataEntry& /*spellData*/) {}

    static bool MissileCasterCheck(const SDK::MissileClient& missile,
                                   const SDK::AIBaseClient& caster) {
        return missile.IsValid() &&
               _stricmp(missile.MissileName().c_str(), "AzirSoldierMissile") == 0 &&
               caster.IsValid() &&
               caster.Type() == ::Core::Objects::ObjectType::AIMinionClient;
    }

    static bool ProcessDetectSpellName(const char* spellName) {
        if (!spellName || !spellName[0]) {
            return false;
        }
        std::string lower(spellName);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lower.find("karthuslaywastea") != std::string::npos ||
               lower.find("karthuslaywastedeada") != std::string::npos;
    }
};

} // namespace Plugins::KuroEvade
