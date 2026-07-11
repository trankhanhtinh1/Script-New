#pragma once

#include "EvadeSpellData.h"
#include "EvadeSpellDatabase.h"
#include "../Evade/EvadePlanner.h"
#include "../../../Core/CoreEvadeState.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace ZDEvade {

struct EvadeSpellCastResult {
    bool casted = false;
    CandidateEvaluation destination;
    int holdUntilTick = 0;
    SDK::SpellSlot slot = SDK::SpellSlot::Unknown;
};

class EvadeSpellEngine {
public:
    void Reset() {
        loadedChampion.clear();
        loaded = false;
        spells.clear();
        lastCastTick = 0;
        lastCastSlot = SDK::SpellSlot::Unknown;
    }

    EvadeSpellCastResult TryUse(const SDK::AIHeroClient& player,
                                const std::vector<Threat>& threats,
                                const EvadeSettings& settings,
                                int minimumDanger) {
        EvadeSpellCastResult result;
        if (!player.IsValid() || threats.empty()) return result;
        EnsureLoaded(player);
        const int now = SDK::Variables::TickCount();
        if (lastCastTick > 0 && now - lastCastTick < 275) return result;
        const int threatDanger = HighestThreatDanger(player, threats, settings, now);
        if (threatDanger < std::max(1, minimumDanger)) return result;

        for (const auto& entry : spells) {
            if (entry.data.dangerlevel > threatDanger) continue;
            if (!SDK::Extensions::IsReady(entry.slot, 0)) continue;
            const bool blink = entry.data.evadeType == EvadeType::Blink;
            CandidateEvaluation destination = EvadePlanner::FindBestSpellPosition(
                player,
                threats,
                settings,
                entry.data.range,
                entry.data.speed,
                entry.data.spellDelay +
                    static_cast<float>(std::max(0, SDK::Game::Ping())) * 0.5f,
                entry.data.fixedRange && !entry.data.isSummonerSpell,
                blink);
            if (!destination.strictSafe) continue;

            Vec2 castPosition = destination.position;
            if (entry.data.isReversed) {
                const Vec2 heroPos = player.ServerPosition().To2D();
                castPosition = heroPos - (destination.position - heroPos);
            }
            CoreEvadeState::SpellCastBypassScope bypass;
            const bool casted = player.Spellbook().CastSpell(
                entry.slot,
                Vec3::From2D(castPosition, player.ServerPosition().y),
                false);
            if (!casted) continue;

            lastCastTick = now;
            lastCastSlot = entry.slot;
            result.casted = true;
            result.destination = destination;
            result.slot = entry.slot;
            const float travelMs = blink || entry.data.speed <= 1.0f
                ? 0.0f
                : 1000.0f * player.ServerPosition().To2D().Distance(destination.position) /
                    std::max(50.0f, entry.data.speed);
            result.holdUntilTick = now + static_cast<int>(std::ceil(
                entry.data.spellDelay + travelMs +
                static_cast<float>(std::max(0, SDK::Game::Ping())) * 0.5f + 60.0f));
            return result;
        }
        return result;
    }

private:
    struct RuntimeSpell {
        EvadeSpellData data;
        SDK::SpellSlot slot = SDK::SpellSlot::Unknown;
    };

    std::string loadedChampion;
    bool loaded = false;
    std::vector<RuntimeSpell> spells;
    int lastCastTick = 0;
    SDK::SpellSlot lastCastSlot = SDK::SpellSlot::Unknown;

    static bool EqualsNoCase(const std::string& left, const std::string& right) {
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (std::tolower(static_cast<unsigned char>(left[index])) !=
                std::tolower(static_cast<unsigned char>(right[index]))) return false;
        }
        return true;
    }

    static SDK::SpellSlot BaseSlot(EvadeSpellSlot slot) {
        switch (slot) {
        case EvadeSpellSlot::Q: return SDK::SpellSlot::Q;
        case EvadeSpellSlot::W: return SDK::SpellSlot::W;
        case EvadeSpellSlot::E: return SDK::SpellSlot::E;
        case EvadeSpellSlot::R: return SDK::SpellSlot::R;
        default: return SDK::SpellSlot::Unknown;
        }
    }

    static SDK::SpellSlot ResolveSlot(const SDK::AIHeroClient& player,
                                      const EvadeSpellData& data) {
        if (data.isSummonerSpell) {
            for (SDK::SpellSlot slot : {SDK::SpellSlot::Summoner1, SDK::SpellSlot::Summoner2}) {
                const auto spell = player.Spellbook().GetSpell(slot);
                if (spell.IsValid() && EqualsNoCase(spell.Name(), data.spellName)) return slot;
            }
            return SDK::SpellSlot::Unknown;
        }
        const SDK::SpellSlot slot = BaseSlot(data.spellKey);
        if (slot == SDK::SpellSlot::Unknown) return slot;
        if (data.checkSpellName) {
            const auto spell = player.Spellbook().GetSpell(slot);
            if (!spell.IsValid() || !EqualsNoCase(spell.Name(), data.spellName)) {
                return SDK::SpellSlot::Unknown;
            }
        }
        return slot;
    }

    void EnsureLoaded(const SDK::AIHeroClient& player) {
        const std::string champion = player.CharacterName();
        if (!champion.empty() && champion == loadedChampion && loaded) return;
        EvadeSpellDatabase::Initialize();
        spells.clear();
        loadedChampion = champion;
        loaded = true;
        for (const auto& data : EvadeSpellDatabase::Spells) {
            const bool playerSpell = !champion.empty() && data.charName == champion;
            if (!playerSpell && !data.isSummonerSpell) continue;
            if (data.isItem || data.isSpecial || data.untargetable) continue;
            if (data.castType != EvadeCastType::Position) continue;
            if (data.evadeType != EvadeType::Dash && data.evadeType != EvadeType::Blink) continue;
            const SDK::SpellSlot slot = ResolveSlot(player, data);
            if (slot == SDK::SpellSlot::Unknown) continue;
            spells.push_back({data, slot});
        }
        std::sort(spells.begin(), spells.end(), [](const RuntimeSpell& left, const RuntimeSpell& right) {
            if (left.data.isSummonerSpell != right.data.isSummonerSpell)
                return !left.data.isSummonerSpell;
            if (left.data.dangerlevel != right.data.dangerlevel)
                return left.data.dangerlevel < right.data.dangerlevel;
            return left.data.spellDelay < right.data.spellDelay;
        });
    }

    static int HighestThreatDanger(const SDK::AIHeroClient& player,
                                   const std::vector<Threat>& threats,
                                   const EvadeSettings& settings,
                                   int now) {
        const Vec2 heroPos = player.ServerPosition().To2D();
        const float radius = std::max(10.0f, player.BoundingRadius());
        int danger = 0;
        for (const auto& threat : threats) {
            if (threat.IsExpiredAt(now)) continue;
            const int impact = EvadeGeometry::ImpactTickAt(threat, heroPos);
            if (impact - now > static_cast<int>(settings.maxThreatHorizonMs)) continue;
            if (EvadeGeometry::ContainsAt(threat, heroPos, radius, settings.pathBuffer, now) ||
                EvadeGeometry::ContainsAt(threat, heroPos, radius, settings.pathBuffer, impact)) {
                danger = std::max(danger, threat.Danger());
            }
        }
        return danger;
    }
};

}
