#pragma once

// ============================================================================
// CoreClassification.h - Object type classification (pure name-based)
// ----------------------------------------------------------------------------
// Ported from old source/core/CoreClassification.h and simplified per TODO.md
// Phase 1.2.1 taxonomy: the game object list is filtered by CharacterName
// prefix/substring patterns so no RuntimeAPI::CompareTypeFlags is needed.
// The lookup is purely name + team + follow-target driven, avoiding any
// dependency on the RuntimeAPI shim that used to wrap type-flag byte reads.
// ============================================================================

#include "Globals.h"
#include "offset.h"

#include <cstdint>
#include <cstring>

namespace CoreClassification {

    enum class ObjectType : int {
        Hero,
        LaneMinion,
        JungleMonster,
        JungleBig,          // Red / Blue buff, Big wolf / raptor / krug, Gromp
        JungleEpic,         // Dragon, Baron, Herald, Voidgrub, Atakhan
        Turret,
        Inhibitor,
        Nexus,
        Ward,               // all wards (Control, Sight, Farsight, Zombie)
        Plant,              // Honeyfruit, Blast Cone, Scryer Bloom
        Pet,                // Tibbers, Daisy, etc. (non-empty followTargetNetId)
        Scuttle,            // Scuttle Crab
        TurretBuildup,      // visual effect on turret (IGNORE for orbwalker)
        Shop,
        Marker,
        Spawn,
        PracticeDummy,      // AatroxWCenterMinion
        Unknown
    };

    // ── String helpers ──

    inline bool StrContains(const char* str, const char* sub) {
        if (!str || !sub) return false;
        return std::strstr(str, sub) != nullptr;
    }

    inline bool StrStartsWith(const char* str, const char* prefix) {
        if (!str || !prefix) return false;
        const size_t len = std::strlen(prefix);
        return std::strncmp(str, prefix, len) == 0;
    }

    // ── Safe field readers ──

    inline bool ReadObjectName(uintptr_t obj, char* out, int maxOut) {
        if (!out || maxOut <= 0) return false;
        out[0] = 0;
        if (!Globals::IsValidPtr(obj)) return false;
        return Globals::ReadGameString(obj + Offset::All::Name, out, maxOut);
    }

    inline int ReadTeam(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return 0;
        return static_cast<int>(Globals::Read<uint8_t>(obj + Offset::All::Team));
    }

    // ── Taxonomy helpers (match TODO.md 1.2.1) ──

    inline bool IsLaneMinionName(const char* name) {
        return StrContains(name, "SRU_ChaosMinion") ||
               StrContains(name, "SRU_OrderMinion") ||
               StrContains(name, "Super") ||
               StrContains(name, "Siege");
    }

    inline bool IsEpicMonsterName(const char* name) {
        return StrContains(name, "Dragon") ||
               StrContains(name, "dragon") ||
               StrContains(name, "Baron") ||
               StrContains(name, "Worm") ||
               StrContains(name, "RiftHerald") ||
               StrContains(name, "Voidgrub") ||
               StrContains(name, "Atakhan") ||
               StrContains(name, "SRU_Horde");
    }

    inline bool IsScuttleName(const char* name) {
        return StrContains(name, "Sru_Crab") ||
               StrContains(name, "SRU_Crab") ||
               StrContains(name, "RiftScuttler") ||
               StrContains(name, "Scuttler");
    }

    inline bool IsJungleBigName(const char* name) {
        if (StrContains(name, "SRU_Red") || StrContains(name, "SRU_Blue") ||
            StrContains(name, "SRU_Gromp")) {
            return true;
        }
        if ((StrContains(name, "SRU_Murkwolf.") ||
             StrContains(name, "SRU_Razorbeak.") ||
             StrContains(name, "SRU_Krug.")) && !StrContains(name, "Mini")) {
            return true;
        }
        return false;
    }

    inline bool IsPlantName(const char* name) {
        return StrContains(name, "SRU_Plant") ||
               StrContains(name, "PlantSatchel") ||
               StrContains(name, "PlantVision") ||
               StrContains(name, "PlantHealth") ||
               StrContains(name, "PlantStealthy");
    }

    inline bool IsWardName(const char* name) {
        return StrContains(name, "Ward") ||
               StrContains(name, "JammerDevice") ||
               StrContains(name, "BlueTrinket") ||
               StrContains(name, "YellowTrinket") ||
               StrContains(name, "GhostWard") ||
               StrContains(name, "ZombieWard");
    }

    // ── Core classification (pure name-based) ──

    inline ObjectType Classify(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) return ObjectType::Unknown;

        char name[128] = {};
        if (!ReadObjectName(obj, name, sizeof(name)) || !name[0]) {
            return ObjectType::Unknown;
        }

        // === NON-ATTACKABLE filters (highest priority) ===
        if (name[0] == '_' && name[1] == '_') return ObjectType::Spawn;
        if (name[0] == 'l' && name[1] == 'a' && name[2] == 'n') return ObjectType::Marker;
        if (name[0] == 'S' && name[1] == 'h' && name[2] == 'o') return ObjectType::Shop;
        if (StrContains(name, "TurretBuildup"))     return ObjectType::TurretBuildup;
        if (StrContains(name, "AatroxWCenter"))     return ObjectType::PracticeDummy;
        if (StrContains(name, "Corpse") || StrContains(name, "corpse")) return ObjectType::Unknown;
        if (StrContains(name, "Seed"))              return ObjectType::Unknown;
        if (StrContains(name, "Marker") || StrContains(name, "marker")) return ObjectType::Marker;
        if (StrContains(name, "Respawn"))           return ObjectType::Unknown;
        if (StrContains(name, "PreSeason"))         return ObjectType::Unknown;
        if (StrContains(name, "GenericUnit"))       return ObjectType::Unknown;
        if (StrContains(name, "HiddenMinion"))      return ObjectType::Unknown;
        if (StrContains(name, "PlantMasterMinion")) return ObjectType::Unknown;
        if (StrContains(name, "FaeLightWardPad"))   return ObjectType::Unknown;  // visual pad, NOT a ward
        if (StrContains(name, "Minion_Order") || StrContains(name, "Minion_Chaos")) return ObjectType::Unknown;

        // === STRUCTURES ===
        // Nexus turrets start with "SRUAP_" too. Check "Turret" substring first so
        // "SRUAP_OrderTurretShrine" / "ChaosTurretShrine" / "OrderTurretDragon" /
        // "ChaosTurretNormal*" all classify as Turret regardless of prefix.
        if (StrContains(name, "Turret"))            return ObjectType::Turret;
        if (StrStartsWith(name, "Barracks"))        return ObjectType::Inhibitor;
        if (StrStartsWith(name, "SR_Inhib"))        return ObjectType::Inhibitor;
        if (StrStartsWith(name, "OrderInhibitor"))  return ObjectType::Inhibitor;
        if (StrStartsWith(name, "ChaosInhibitor"))  return ObjectType::Inhibitor;
        if (name[0] == 'I' && name[1] == 'n' && name[2] == 'h') return ObjectType::Inhibitor;
        if (StrStartsWith(name, "HQ"))              return ObjectType::Nexus;
        if (StrStartsWith(name, "SR_Nexus"))        return ObjectType::Nexus;
        if (StrStartsWith(name, "OrderNexus"))      return ObjectType::Nexus;
        if (StrStartsWith(name, "ChaosNexus"))      return ObjectType::Nexus;
        if (name[0] == 'N' && name[1] == 'e' && name[2] == 'x') return ObjectType::Nexus;

        // === PLANTS ===
        if (IsPlantName(name))                      return ObjectType::Plant;

        // === WARDS ===
        if (IsWardName(name))                       return ObjectType::Ward;

        // === JUNGLE EPIC (Dragon / Baron / Herald / Voidgrub / Atakhan) ===
        if (IsEpicMonsterName(name))                return ObjectType::JungleEpic;

        // === SCUTTLE ===
        if (IsScuttleName(name))                    return ObjectType::Scuttle;

        // === LANE MINIONS ===
        if (IsLaneMinionName(name))                 return ObjectType::LaneMinion;

        // === JUNGLE (team 3 = neutral) ===
        const int team = ReadTeam(obj);
        if (team == 3) {
            if (IsJungleBigName(name)) return ObjectType::JungleBig;
            return ObjectType::JungleMonster;
        }

        // === PET (has non-zero followTargetNetId on teams 1 / 2) ===
        const auto followId = Globals::Read<uint32_t>(obj + Offset::MinionClassRuntime::FollowTargetNetId);
        if (followId != 0 && (team == 1 || team == 2)) return ObjectType::Pet;

        // === HERO (team 1 or 2, anything else) ===
        // All structural / minion / ward / plant / jungle / pet cases ruled out
        // above by explicit name taxonomy, so remaining team 1 / 2 objects are
        // always champions. RuntimeAPI::CompareTypeFlags is no longer needed.
        if (team == 1 || team == 2) {
            return ObjectType::Hero;
        }

        return ObjectType::Unknown;
    }

    // ── Orbwalker filter: is this target attackable? ──

    inline bool IsAttackable(uintptr_t obj, int myTeam) {
        if (!Globals::IsValidPtr(obj)) return false;

        // Quick dead / invisible check
        const float hp = Globals::Read<float>(obj + Offset::AttackableUnit::HP);
        if (hp <= 0.0f) return false;
        const bool visible = Globals::Read<bool>(obj + Offset::All::Visible);
        if (!visible) return false;

        const auto type = Classify(obj);
        const int team = ReadTeam(obj);

        switch (type) {
            case ObjectType::Hero:          return team != myTeam;
            case ObjectType::LaneMinion:    return true;
            case ObjectType::JungleMonster: return true;
            case ObjectType::JungleBig:     return true;
            case ObjectType::JungleEpic:    return true;
            case ObjectType::Scuttle:       return true;
            case ObjectType::Ward:          return true;
            case ObjectType::Plant:         return true;
            case ObjectType::Turret:        return team != myTeam;
            case ObjectType::Inhibitor:     return team != myTeam;
            case ObjectType::Nexus:         return team != myTeam;
            case ObjectType::PracticeDummy: return true;
            case ObjectType::Pet:           return team != myTeam;
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

    inline bool IsStructure(uintptr_t obj) {
        const auto type = Classify(obj);
        return type == ObjectType::Turret ||
               type == ObjectType::Inhibitor ||
               type == ObjectType::Nexus;
    }

    inline bool IsInhibitorOrNexus(uintptr_t obj) {
        const auto type = Classify(obj);
        return type == ObjectType::Inhibitor || type == ObjectType::Nexus;
    }

    // ── Type name for diagnostics ──

    inline const char* TypeName(ObjectType type) {
        switch (type) {
            case ObjectType::Hero:          return "Hero";
            case ObjectType::LaneMinion:    return "LaneMinion";
            case ObjectType::JungleMonster: return "Jungle";
            case ObjectType::JungleBig:     return "JungleBig";
            case ObjectType::JungleEpic:    return "JungleEpic";
            case ObjectType::Turret:        return "Turret";
            case ObjectType::Inhibitor:     return "Inhibitor";
            case ObjectType::Nexus:         return "Nexus";
            case ObjectType::Ward:          return "Ward";
            case ObjectType::Plant:         return "Plant";
            case ObjectType::Pet:           return "Pet";
            case ObjectType::Scuttle:       return "Scuttle";
            case ObjectType::TurretBuildup: return "TurretBuildup";
            case ObjectType::Shop:          return "Shop";
            case ObjectType::Marker:        return "Marker";
            case ObjectType::Spawn:         return "Spawn";
            case ObjectType::PracticeDummy: return "PracticeDummy";
            default:                        return "Unknown";
        }
    }

} // namespace CoreClassification
