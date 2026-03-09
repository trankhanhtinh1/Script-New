#pragma once
// ============================================================================
// Invulnerable.h — Invulnerable Database (EnsoulSharp SDK Port)
// ============================================================================
// Full port of EnsoulSharp.SDK/Core/Utils/Invulnerable.cs
// Checks if a target hero is invulnerable to a given damage type.
// Supports custom entries with per-champion conditions.
// ============================================================================

#include "core/Vector.h"
#include "Enums.h"
#include "GameObject.h"
#include "BuffManager.h"
#include "GameObjects.h"

#include <string>
#include <vector>
#include <functional>
#include <algorithm>

namespace SDK {

// ============================================================================
// DamageType enum (for invulnerability checks)
// ============================================================================
enum class InvDamageType {
    Physical,
    Magical,
    True_,
    Any         // Matches all types
};

// ============================================================================
// InvulnerableEntry — Single invulnerability entry
// ============================================================================
struct InvulnerableEntry {
    std::string BuffName;
    std::string ChampionName;       // Empty = any champion
    InvDamageType DamageType = InvDamageType::Any;
    bool IsShield = false;
    int MinHealthPercent = 0;

    // Optional check function: (target, damageType) -> bool
    // If set, invulnerability only applies when this returns true
    std::function<bool(const GameObject&, InvDamageType)> CheckFunction;

    InvulnerableEntry() = default;
    InvulnerableEntry(const std::string& buffName) : BuffName(buffName) {}
};

// ============================================================================
// Invulnerable — Static database and check system
// ============================================================================
class Invulnerable {
public:
    // ---- Initialize with default entries ----
    static void Init() {
        if (s_initialized) return;
        s_initialized = true;

        // Alistar R — only when > 1 enemy nearby
        {
            InvulnerableEntry e("FerociousHowl");
            e.ChampionName = "Alistar";
            e.CheckFunction = [](const GameObject& target, InvDamageType) -> bool {
                auto& player = GameObjects::Player;
                if (!player.IsValid()) return false;
                int count = 0;
                for (auto& enemy : GameObjects::EnemyHeroes) {
                    if (enemy.IsValid() && enemy.IsAlive() && player.GetPosition().Distance(enemy.GetPosition()) <= 600.0f)
                        count++;
                }
                return count > 1;
            };
            Register(e);
        }

        // Fiora W
        Register(InvulnerableEntry("FioraW"));
        s_entries.back().ChampionName = "Fiora";

        // Jax E — only blocks physical
        {
            InvulnerableEntry e("JaxCounterStrike");
            e.ChampionName = "Jax";
            e.DamageType = InvDamageType::Physical;
            Register(e);
        }

        // Kayle R — shield (can be ignored if ignoreShields)
        {
            InvulnerableEntry e("KayleR");
            e.IsShield = true;
            Register(e);
        }

        // Kindred R — only when target health <= 10%
        {
            InvulnerableEntry e("KindredRNoDeathBuff");
            e.MinHealthPercent = 10;
            e.CheckFunction = [](const GameObject& target, InvDamageType) -> bool {
                float healthPct = (target.GetHealth() / target.GetMaxHealth()) * 100.0f;
                return healthPct <= 10.0f;
            };
            Register(e);
        }

        // Malzahar passive — magic shield
        {
            InvulnerableEntry e("malzaharpassiveshield");
            e.ChampionName = "Malzahar";
            e.IsShield = true;
            e.DamageType = InvDamageType::Magical;
            Register(e);
        }

        // Master Yi W — only when > 1 enemy nearby
        {
            InvulnerableEntry e("Meditate");
            e.ChampionName = "MasterYi";
            e.CheckFunction = [](const GameObject& target, InvDamageType) -> bool {
                auto& player = GameObjects::Player;
                if (!player.IsValid()) return false;
                int count = 0;
                for (auto& enemy : GameObjects::EnemyHeroes) {
                    if (enemy.IsValid() && enemy.IsAlive() && player.GetPosition().Distance(enemy.GetPosition()) <= 600.0f)
                        count++;
                }
                return count > 1;
            };
            Register(e);
        }

        // Morgana E — magic shield
        {
            InvulnerableEntry e("MorganaE");
            e.IsShield = true;
            e.DamageType = InvDamageType::Magical;
            Register(e);
        }

        // Nocturne W
        Register(InvulnerableEntry("NocturneShroudofDarkness"));
        s_entries.back().ChampionName = "Nocturne";

        // Shen W — physical only
        {
            InvulnerableEntry e("ShenWBuff");
            e.DamageType = InvDamageType::Physical;
            Register(e);
        }

        // Sivir E — spell shield
        {
            InvulnerableEntry e("SivirE");
            e.ChampionName = "Sivir";
            e.IsShield = true;
            Register(e);
        }

        // Taric R
        Register(InvulnerableEntry("TaricR"));

        // Tryndamere R — only when health is low
        {
            InvulnerableEntry e("UndyingRage");
            e.ChampionName = "Tryndamere";
            e.CheckFunction = [](const GameObject& target, InvDamageType) -> bool {
                return target.GetHealth() <= 70.0f;  // Simplified: R prevents dying
            };
            Register(e);
        }

        // Xin Zhao R — ranged physical immunity
        {
            InvulnerableEntry e("XinZhaoRRangedImmunity");
            e.ChampionName = "XinZhao";
            e.DamageType = InvDamageType::Physical;
            Register(e);
        }

        // Banshee's Veil — magic spell shield
        {
            InvulnerableEntry e("bansheesveil");
            e.IsShield = true;
            e.DamageType = InvDamageType::Magical;
            Register(e);
        }

        // Edge of Night — spell shield
        {
            InvulnerableEntry e("itemmagekillerveil");
            e.IsShield = true;
            e.DamageType = InvDamageType::Magical;
            Register(e);
        }

        // ---- Season 2026 additions ----

        // Bard R (Tempered Fate)
        Register(InvulnerableEntry("BardRStasis"));

        // Zhonya's Hourglass
        Register(InvulnerableEntry("ZhonyasRingShield"));
        Register(InvulnerableEntry("zhonyasringshield"));

        // Stopwatch
        Register(InvulnerableEntry("ChronoShift"));

        // Guardian Angel revive
        Register(InvulnerableEntry("intomingGuardianAngel"));

        // Sion Passive (Glory in Death)
        {
            InvulnerableEntry e("SionPassiveZombie");
            e.ChampionName = "Sion";
            Register(e);
        }

        // Karthus Passive (Death Defied)
        {
            InvulnerableEntry e("KarthusDeathDefiedBuff");
            e.ChampionName = "Karthus";
            Register(e);
        }

        // Kog'Maw Passive (Icathian Surprise)
        {
            InvulnerableEntry e("KogMawIcathianSurprise");
            e.ChampionName = "KogMaw";
            Register(e);
        }

        // K'Sante R armor (All Out) — could be treated as damage reduction not invuln
        // Gwen W (Hallowed Mist) — outside-only protection
        // Nilah W — special dodge, not a buff-based invuln
    }

    // ---- Check if hero is invulnerable ----
    static bool Check(
        const GameObject& hero,
        InvDamageType damageType = InvDamageType::Any,
        bool ignoreShields = true,
        float damage = -1.0f)
    {
        if (!hero.IsValid() || !hero.IsAlive()) return false;

        // Check game-level invulnerability flag
        if (hero.IsInvulnerable()) {
            return true;
        }

        BuffManager buffMgr(hero.address);

        // Check each registered entry
        for (auto& entry : s_entries) {
            // Champion filter
            if (!entry.ChampionName.empty()) {
                std::string charName = hero.GetChampionName();
                if (charName != entry.ChampionName)
                    continue;
            }

            // Damage type filter
            if (entry.DamageType != InvDamageType::Any && entry.DamageType != damageType)
                continue;

            // Check if hero has this buff
            if (!buffMgr.HasBuff(entry.BuffName.c_str()))
                continue;

            // Shield filter
            if (ignoreShields && entry.IsShield)
                continue;

            // Custom check function
            if (entry.CheckFunction) {
                try {
                    if (!entry.CheckFunction(hero, damageType))
                        continue;
                }
                catch (...) {
                    continue;
                }
            }

            // Min health percent check
            if (damage > 0 && entry.MinHealthPercent > 0) {
                float afterDmgPct = (hero.GetHealth() - damage) / hero.GetMaxHealth() * 100.0f;
                if (afterDmgPct >= entry.MinHealthPercent)
                    continue;
            }

            return true;
        }

        return false;
    }

    // ---- Register / Deregister custom entries ----
    static void Register(const InvulnerableEntry& entry) {
        if (entry.BuffName.empty()) return;
        // Don't add duplicates
        for (auto& existing : s_entries) {
            if (existing.BuffName == entry.BuffName) return;
        }
        s_entries.push_back(entry);
    }

    static void Deregister(const std::string& buffName) {
        s_entries.erase(
            std::remove_if(s_entries.begin(), s_entries.end(),
                [&](const InvulnerableEntry& e) { return e.BuffName == buffName; }),
            s_entries.end()
        );
    }

    // ---- Access entries ----
    static const std::vector<InvulnerableEntry>& GetEntries() { return s_entries; }

    // ---- Quick helpers ----
    static bool IsInvulnerable(const GameObject& target) {
        return Check(target, InvDamageType::Any, false);
    }

    static bool IsPhysicalInvulnerable(const GameObject& target) {
        return Check(target, InvDamageType::Physical, true);
    }

    static bool IsMagicalInvulnerable(const GameObject& target) {
        return Check(target, InvDamageType::Magical, true);
    }

private:
    static inline std::vector<InvulnerableEntry> s_entries;
    static inline bool s_initialized = false;
};

} // namespace SDK
