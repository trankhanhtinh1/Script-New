#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"
#include "Enums.h"
#include <string>
#include <cstdint>

// ============================================================================
// Missile — Projectile tracking (turret shots, auto attacks, spells)
// Reference: Script-New-main/SDK/Missile.h
// ============================================================================

namespace SDK {

    class Missile {
    public:
        uintptr_t address;

        Missile() : address(0) {}
        Missile(uintptr_t addr) : address(addr) {}
        bool IsValid() const { return Globals::IsValidPtr(address); }

        // ====================================================================
        // Position (inherited from GameObject)
        // ====================================================================

        Vec3 GetPosition() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::Missile::Position);
        }

        // ====================================================================
        // Cast Info (via CastInfoBase)
        // ====================================================================

        uintptr_t GetCastInfo() const {
            if (!IsValid()) return 0;
            return Globals::Read<uintptr_t>(address + Offset::Missile::CastInfoBase);
        }

        // Source caster network ID
        int GetCasterNetId() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::Missile::CasterNetId);
        }

        // Missile network ID
        int GetNetworkId() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::Missile::NetworkId);
        }

        // Start position
        Vec3 GetStartPos() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::Missile::StartPos);
        }

        // End position (target)
        Vec3 GetEndPos() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::Missile::EndPos);
        }

        // Cast end position
        Vec3 GetCastEndPos() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::Missile::CastEndPos);
        }

        // ====================================================================
        // Spell name via CastInfo → SpellData
        // ====================================================================

        std::string GetSpellName() const {
            if (!IsValid()) return "";
            char buf[128] = {};
            if (!ReadSpellNameRaw(address, buf, sizeof(buf)))
                return "";
            return std::string(buf);
        }

        std::string GetMissileName() const {
            if (!IsValid()) return "";
            char buf[128] = {};
            if (!Globals::ReadGameString(address + Offset::Missile::MissileName, buf, sizeof(buf)))
                return "";
            return std::string(buf);
        }

    private:
        // SEH-safe: read spell name via SpellDataInst → name (no C++ objects)
        static bool ReadSpellNameRaw(uintptr_t addr, char* out, int maxLen) {
            __try {
                uintptr_t sdi = *(uintptr_t*)(addr + Offset::Missile::SpellDataInst);
                if (sdi < 0x10000 || sdi > 0x7FFFFFFFFFFF) return false;
                uintptr_t nameAddr = sdi + 0x80;
                return Globals::ReadGameString(nameAddr, out, maxLen);
            } __except(1) { out[0] = 0; return false; }
        }
    public:

        // ====================================================================
        // Type checks
        // ====================================================================

        bool IsTurretShot() const {
            std::string name = GetSpellName();
            if (name.empty()) name = GetMissileName();
            return (name.find("TurretAttack") != std::string::npos ||
                    name.find("Turret") != std::string::npos ||
                    name.find("Obelisk") != std::string::npos);
        }

        bool IsAutoAttack() const {
            std::string name = GetSpellName();
            return (name.find("BasicAttack") != std::string::npos ||
                    name.find("CritAttack") != std::string::npos);
        }
    };

    // ========================================================================
    // MissileManager — Get all active missiles
    // ========================================================================
    class MissileManager {
    public:
        static std::vector<Missile> GetMissiles() {
            std::vector<Missile> result;
            uintptr_t mgr = Globals::Read<uintptr_t>(
                Globals::base + Offset::Global::MissileManager);
            if (!Globals::IsValidPtr(mgr)) return result;

            uintptr_t list = Globals::Read<uintptr_t>(mgr + Offset::ManagerList::Items);
            int count = Globals::Read<int>(mgr + Offset::ManagerList::Size);
            if (!Globals::IsValidPtr(list) || count <= 0 || count > 500)
                return result;

            uintptr_t addrs[500] = {};
            int n = Globals::ReadPtrArray(list, count, addrs, 500);
            for (int i = 0; i < n; i++) {
                if (Globals::IsValidPtr(addrs[i]))
                    result.emplace_back(addrs[i]);
            }
            return result;
        }

        // Check if a target has turret aggro
        static bool HasTurretAggro(int targetNetId) {
            auto missiles = GetMissiles();
            for (auto& m : missiles) {
                if (m.IsTurretShot() && m.GetCasterNetId() != targetNetId) {
                    // Check if missile is heading toward target area
                    // Simplified: check by caster NetId (TODO: proper target check)
                    return true;
                }
            }
            return false;
        }
    };

} // namespace SDK
