#pragma once

#include "EvadeSpellData.h"
#include "EvadeSpellDatabase.h"
#include "EvadeHelper.h"
#include "SpellDetector.h"
#include "../../SDK/SDK.h"

#include <algorithm>
#include <vector>

namespace ZDEvade {

class EvadeSpellManager {
public:
    static inline std::vector<EvadeSpellData> evadeSpells;
    static inline bool initialized = false;
    static inline int lastEvadeSpellCastTick = 0;
    static inline SDK::SpellSlot lastEvadeSpellSlot = SDK::SpellSlot::Unknown;
    static constexpr int kEvadeSpellCastInterval = 250;

    static void Initialize() {
        if (initialized) return;
        initialized = true;
        EvadeSpellDatabase::Initialize();
        LoadPlayerEvadeSpells();
    }

    static void LoadPlayerEvadeSpells() {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return;

        const std::string charName = player.CharacterName();

        evadeSpells.clear();
        for (const auto& spell : EvadeSpellDatabase::Spells) {
            if (spell.charName == charName || spell.isSummonerSpell) {
                evadeSpells.push_back(spell);
            }
        }
    }

    // ── UseEvadeSpell: try to use dash/blink to dodge ──
    static bool UseEvadeSpell(const Vec2& heroPos, float boundingRadius,
                              const std::vector<TrackedSpell>& skillshots,
                              bool evadeSpellsEnabled) {
        if (!evadeSpellsEnabled || evadeSpells.empty()) return false;

        const int now = SDK::Variables::TickCount();
        if (lastEvadeSpellCastTick > 0 && now - lastEvadeSpellCastTick < kEvadeSpellCastInterval) return false;

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return false;

        const TrackedSpell* mostDangerous = nullptr;
        int maxDanger = 0;
        for (const auto& s : skillshots) {
            if (!EvadeHelper::InSkillShot(s, heroPos, boundingRadius)) continue;
            int danger = std::max(1, s.DangerValue());
            if (danger > maxDanger) { maxDanger = danger; mostDangerous = &s; }
        }

        if (!mostDangerous || maxDanger < 3) return false;

        // Try evade spells
        for (const auto& evadeSpell : evadeSpells) {
            if (!IsEvadeSpellReady(player, evadeSpell)) continue;

            if (evadeSpell.evadeType == EvadeType::Dash || evadeSpell.evadeType == EvadeType::Blink) {
                Vec2 dodgePos = FindEvadeSpellPosition(player, heroPos, evadeSpell, skillshots, boundingRadius);
                if (!dodgePos.IsZero()) {
                    if (CastEvadeSpell(player, evadeSpell, dodgePos)) return true;
                }
            }
        }

        return false;
    }

    static bool PreferEvadeSpell() {
        // Check if player has dash/blink evade spells available
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return false;

        for (const auto& spell : evadeSpells) {
            if ((spell.evadeType == EvadeType::Dash || spell.evadeType == EvadeType::Blink) &&
                IsEvadeSpellReady(player, spell)) {
                return true;
            }
        }
        return false;
    }

private:
    static bool IsEvadeSpellReady(const SDK::AIHeroClient& player, const EvadeSpellData& spell) {
        const auto slot = ToSDKSlot(spell.spellKey);
        if (slot == SDK::SpellSlot::Unknown) return false;
        return SDK::Extensions::IsReady(slot, 0);
    }

    static SDK::SpellSlot ToSDKSlot(EvadeSpellSlot slot) {
        switch (slot) {
        case EvadeSpellSlot::Q: return SDK::SpellSlot::Q;
        case EvadeSpellSlot::W: return SDK::SpellSlot::W;
        case EvadeSpellSlot::E: return SDK::SpellSlot::E;
        case EvadeSpellSlot::R: return SDK::SpellSlot::R;
        default: return SDK::SpellSlot::Unknown;
        }
    }

    static Vec2 FindEvadeSpellPosition(const SDK::AIHeroClient& player,
                                       const Vec2& heroPos,
                                       const EvadeSpellData& spell,
                                       const std::vector<TrackedSpell>& skillshots,
                                       float boundingRadius) {
        const float range = spell.range > 0 ? spell.range : 400.0f;
        Vec2 dodgePos;
        if (EvadeHelper::GetBestEvadeSpellPosition(player, heroPos, boundingRadius, skillshots,
                                                   range, spell.speed, spell.spellDelay,
                                                   spell.fixedRange, dodgePos)) {
            return dodgePos;
        }
        return {};
    }

    static bool CastEvadeSpell(const SDK::AIHeroClient& player,
                               const EvadeSpellData& spell,
                               const Vec2& pos) {
        const auto slot = ToSDKSlot(spell.spellKey);
        if (slot == SDK::SpellSlot::Unknown) return false;

        const int now = SDK::Variables::TickCount();
        if (lastEvadeSpellCastTick > 0 && slot == lastEvadeSpellSlot &&
            now - lastEvadeSpellCastTick < kEvadeSpellCastInterval) return false;

        const float planeY = player.ServerPosition().y;
        const Vec3 worldPos = Vec3::From2D(pos, planeY);
        player.Spellbook().CastSpell(slot, worldPos, false);
        lastEvadeSpellCastTick = now;
        lastEvadeSpellSlot = slot;
        return true;
    }
};

} // namespace ZDEvade
