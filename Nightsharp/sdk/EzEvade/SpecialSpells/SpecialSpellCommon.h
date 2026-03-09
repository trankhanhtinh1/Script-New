#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Core/EvadeHelper.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/EzEvade/Helpers/Situation.h"
#include "sdk/EzEvade/Spells/ObjectTracker.h"
#include "sdk/EzEvade/Spells/SpellDetector.h"
#include "sdk/EzEvade/Utils/DelayAction.h"
#include "sdk/EzEvade/Utils/MathUtils.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace EzEvade {
namespace SpecialSpells {

inline std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

inline bool EqualsI(const std::string& left, const std::string& right) {
    return _stricmp(left.c_str(), right.c_str()) == 0;
}

inline bool SpellNameIs(const std::shared_ptr<SpellData>& spellData, const char* name) {
    return spellData && _stricmp(spellData->spellName.c_str(), name) == 0;
}

inline SDK::GameObject FindHeroByChampion(const std::string& championName, bool checkTeam = true) {
    for (const auto& hero : SDK::GameObjects::AllHeroes) {
        if (!hero.IsValid()) {
            continue;
        }
        if (!EqualsI(hero.GetChampionName(), championName)) {
            continue;
        }
        if (checkTeam && !Situation::CheckTeam(hero)) {
            continue;
        }
        return hero;
    }
    return SDK::GameObject();
}

inline SDK::GameObject FindObjectByNetId(int netId) {
    if (netId <= 0) {
        return SDK::GameObject();
    }

    auto tryList = [netId](const std::vector<SDK::GameObject>& list) -> SDK::GameObject {
        for (const auto& obj : list) {
            if (obj.IsValid() && obj.GetNetId() == netId) {
                return obj;
            }
        }
        return SDK::GameObject();
    };

    SDK::GameObject found;
    found = tryList(SDK::GameObjects::AllHeroes); if (found.IsValid()) return found;
    found = tryList(SDK::GameObjects::AllMinions); if (found.IsValid()) return found;
    found = tryList(SDK::GameObjects::JungleMinions); if (found.IsValid()) return found;
    found = tryList(SDK::GameObjects::AllTurrets); if (found.IsValid()) return found;
    found = tryList(SDK::GameObjects::ParticleEmitters); if (found.IsValid()) return found;
    found = tryList(SDK::GameObjects::AzirSoldiers); if (found.IsValid()) return found;
    return found;
}

inline Vec3 ClipEndByRange(const Vec3& start, const Vec3& end, float range) {
    if (range <= 0.0f || start.Distance2D(end) <= range) {
        return end;
    }
    return start.Extend(end, range);
}

inline float TickNow() {
    return (float)SDK::Game::GetTickCount();
}

} // namespace SpecialSpells
} // namespace EzEvade

