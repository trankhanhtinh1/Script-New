#pragma once

// ============================================================================
// CoreClassification.h — Object type classification system
// Ported from Offset/ObjectManager.h (freshly reversed, name-based)
// Replaces RuntimeAPI bitmask classification which missed many object types
// and caused orbwalker to get stuck on non-attackable objects.
// ============================================================================

#include "Globals.h"
#include "Offsets.h"
#include "RuntimeAPI.h"

#include <cstdint>
#include <cstring>

namespace CoreClassification {

    enum class ObjectType : int {
        Hero,
        LaneMinion,
        JungleMonster,
        JungleBig,          // Red/Blue buff, Big wolf/raptor/krug, Gromp
        JungleEpic,         // Dragon, Baron, Herald, Voidgrub
        Turret,
        Inhibitor,
        Nexus,
        Ward,               // all wards (Control, Sight, Farsight, Zombie)
        Plant,              // Honeyfruit, Blast Cone, Scryer's Bloom
        Pet,                // Tibbers, Daisy, etc.
        Scuttle,            // Scuttle Crab
        TurretBuildup,      // visual effect on turret (IGNORE)
        Shop,
        Marker,
        Spawn,
        PracticeDummy,      // AatroxWCenterMinion (practice tool target)
        Unknown
    };

    // ── String helpers ──

    inline bool StrContains(const char* str, const char* sub) {
        if (!str || !sub) return false;
        return strstr(str, sub) != nullptr;
    }

    // ── Safe name reader ──

    inline bool ReadObjectName(uintptr_t obj, char* out, int maxOut) {
        if (!out || maxOut <= 0) return false;
        out[0] = 0;
        if (!Globals::IsValidPtr(obj)) return false;
        return Globals::ReadGameString(obj + Offset::All::Name, out, maxOut);
    }

    inline int ReadTeam(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return 0;
        return static_cast<int>(Globals::Read<uint8_t>(obj + Offset::GameObject::TeamAlt));
    }

    // ── Core classification ──

    inline ObjectType Classify(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return ObjectType::Unknown;

        char name[128] = {};
        if (!ReadObjectName(obj, name, sizeof(name)) || !name[0])
            return ObjectType::Unknown;

        // === NON-ATTACKABLE (filter out first) ===
        if (name[0] == '_' && name[1] == '_') return ObjectType::Spawn;
        if (name[0] == 'l' && name[1] == 'a' && name[2] == 'n') return ObjectType::Marker;
        if (name[0] == 'S' && name[1] == 'h' && name[2] == 'o') return ObjectType::Shop;
        if (StrContains(name, "TurretBuildup")) return ObjectType::TurretBuildup;
        if (StrContains(name, "AatroxWCenter")) return ObjectType::PracticeDummy;
        if (StrContains(name, "Corpse") || StrContains(name, "corpse")) return ObjectType::Unknown;
        if (StrContains(name, "Seed")) return ObjectType::Unknown;
        if (StrContains(name, "Marker") || StrContains(name, "marker")) return ObjectType::Marker;
        if (StrContains(name, "Respawn")) return ObjectType::Unknown;
        if (StrContains(name, "PreSeason")) return ObjectType::Unknown;
        if (StrContains(name, "GenericUnit")) return ObjectType::Unknown;
        if (StrContains(name, "HiddenMinion")) return ObjectType::Unknown;
        if (StrContains(name, "PlantMasterMinion")) return ObjectType::Unknown;
        if (StrContains(name, "FaeLightWardPad")) return ObjectType::Unknown;  // visual pad, NOT a ward
        if (StrContains(name, "Minion_Order") || StrContains(name, "Minion_Chaos")) return ObjectType::Unknown;

        // === STRUCTURES ===
        if (name[0] == 'T' && name[1] == 'u' && name[2] == 'r') return ObjectType::Turret;
        if (name[0] == 'I' && name[1] == 'n' && name[2] == 'h') return ObjectType::Inhibitor;
        if (name[0] == 'B' && name[1] == 'a' && name[2] == 'r') return ObjectType::Inhibitor;
        if (name[0] == 'N' && name[1] == 'e' && name[2] == 'x') return ObjectType::Nexus;

        // === PLANTS ===
        if (StrContains(name, "SRU_Plant")) return ObjectType::Plant;
        if (StrContains(name, "PlantSatchel")) return ObjectType::Plant;   // Blast Cone
        if (StrContains(name, "PlantVision")) return ObjectType::Plant;    // Scryer's Bloom
        if (StrContains(name, "PlantHealth")) return ObjectType::Plant;    // Honeyfruit
        if (StrContains(name, "PlantStealthy")) return ObjectType::Plant;  // Stealthward plant

        // === WARDS ===
        if (StrContains(name, "Ward") || StrContains(name, "JammerDevice") ||
            StrContains(name, "BlueTrinket") || StrContains(name, "YellowTrinket") ||
            StrContains(name, "GhostWard") || StrContains(name, "ZombieWard"))
            return ObjectType::Ward;

        // === JUNGLE EPIC ===
        if (StrContains(name, "dragon") || StrContains(name, "Dragon")) return ObjectType::JungleEpic;
        if (StrContains(name, "Baron") || StrContains(name, "Worm")) return ObjectType::JungleEpic;
        if (StrContains(name, "RiftHerald")) return ObjectType::JungleEpic;
        if (StrContains(name, "SRU_Horde")) return ObjectType::JungleEpic;

        // === SCUTTLE ===
        if (StrContains(name, "Sru_Crab") || StrContains(name, "Crab")) return ObjectType::Scuttle;

        // === LANE MINIONS ===
        if (StrContains(name, "SRU_Order") || StrContains(name, "SRU_Chaos") ||
            StrContains(name, "Super") || StrContains(name, "Siege"))
            return ObjectType::LaneMinion;

        // === JUNGLE (team 3 = neutral) ===
        const int team = ReadTeam(obj);
        if (team == 3) {
            if (StrContains(name, "SRU_Red") || StrContains(name, "SRU_Blue") ||
                StrContains(name, "SRU_Gromp"))
                return ObjectType::JungleBig;
            if ((StrContains(name, "SRU_Murkwolf.") || StrContains(name, "SRU_Razorbeak.") ||
                 StrContains(name, "SRU_Krug.")) && !StrContains(name, "Mini"))
                return ObjectType::JungleBig;
            return ObjectType::JungleMonster;
        }

        // === PET (has follow target) ===
        const auto followId = Globals::Read<uint32_t>(obj + Offset::MinionFieldsLayout::FollowTargetNetId);
        if (followId != 0 && (team == 1 || team == 2)) return ObjectType::Pet;

        // === HERO (team 1/2, none of above) ===
        // Double check with RuntimeAPI flag for accuracy
        if (team == 1 || team == 2) {
            if (RuntimeAPI::CompareTypeFlags(obj, Offset::TypeFlags::Hero))
                return ObjectType::Hero;
            // Fallback: if has hero flag, it's a hero; otherwise minion-like
            if (RuntimeAPI::CompareTypeFlags(obj, Offset::TypeFlags::Minion))
                return ObjectType::LaneMinion;
            return ObjectType::Hero;
        }

        return ObjectType::Unknown;
    }

    // ── Orbwalker filter: is this target attackable? ──

    inline bool IsAttackable(uintptr_t obj, int myTeam) {
        if (!Globals::IsValidPtr(obj)) return false;

        // Quick dead/invisible check
        const float hp = Globals::Read<float>(obj + Offset::Health::HP);
        if (hp <= 0.0f) return false;
        const bool visible = Globals::Read<bool>(obj + Offset::All::Visible);
        if (!visible) return false;

        const auto type = Classify(obj);
        const int team = ReadTeam(obj);

        switch (type) {
            case ObjectType::Hero:          return team != myTeam && hp > 0.0f;
            case ObjectType::LaneMinion:    return hp > 0.0f;
            case ObjectType::JungleMonster: return hp > 0.0f;
            case ObjectType::JungleBig:     return hp > 0.0f;
            case ObjectType::JungleEpic:    return hp > 0.0f;
            case ObjectType::Scuttle:       return hp > 0.0f;
            case ObjectType::Ward:          return hp > 0.0f;
            case ObjectType::Plant:         return hp > 0.0f;
            case ObjectType::Turret:        return team != myTeam && hp > 0.0f;
            case ObjectType::Inhibitor:     return team != myTeam && hp > 0.0f;
            case ObjectType::Nexus:         return team != myTeam && hp > 0.0f;
            case ObjectType::PracticeDummy: return hp > 0.0f;
            case ObjectType::Pet:           return team != myTeam && hp > 0.0f;
            // NOT attackable:
            case ObjectType::TurretBuildup: return false;
            case ObjectType::Shop:          return false;
            case ObjectType::Marker:        return false;
            case ObjectType::Spawn:         return false;
            default: return false;
        }
    }

    // ── Should orbwalker completely ignore this object? ──

    inline bool ShouldIgnore(uintptr_t obj) {
        const auto type = Classify(obj);
        return type == ObjectType::TurretBuildup ||
               type == ObjectType::Shop ||
               type == ObjectType::Marker ||
               type == ObjectType::Spawn ||
               type == ObjectType::Unknown;
    }

    // ── Is this a structure (Turret, Inhibitor, Nexus)? ──

    inline bool IsStructure(uintptr_t obj) {
        const auto type = Classify(obj);
        return type == ObjectType::Turret ||
               type == ObjectType::Inhibitor ||
               type == ObjectType::Nexus;
    }

    // ── Is this an inhibitor or nexus? ──

    inline bool IsInhibitorOrNexus(uintptr_t obj) {
        const auto type = Classify(obj);
        return type == ObjectType::Inhibitor || type == ObjectType::Nexus;
    }

    // ── Type name for diagnostics ──

    inline const char* TypeName(ObjectType type) {
        switch (type) {
            case ObjectType::Hero: return "Hero";
            case ObjectType::LaneMinion: return "LaneMinion";
            case ObjectType::JungleMonster: return "Jungle";
            case ObjectType::JungleBig: return "JungleBig";
            case ObjectType::JungleEpic: return "JungleEpic";
            case ObjectType::Turret: return "Turret";
            case ObjectType::Inhibitor: return "Inhibitor";
            case ObjectType::Nexus: return "Nexus";
            case ObjectType::Ward: return "Ward";
            case ObjectType::Plant: return "Plant";
            case ObjectType::Pet: return "Pet";
            case ObjectType::Scuttle: return "Scuttle";
            case ObjectType::TurretBuildup: return "TurretBuildup";
            case ObjectType::Shop: return "Shop";
            case ObjectType::Marker: return "Marker";
            case ObjectType::Spawn: return "Spawn";
            case ObjectType::PracticeDummy: return "PracticeDummy";
            default: return "Unknown";
        }
    }

} // namespace CoreClassification
