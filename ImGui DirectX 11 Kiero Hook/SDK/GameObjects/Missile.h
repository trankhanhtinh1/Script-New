#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"
#include "Enums.h"
#include <string>
#include <cstdint>

// ============================================================================
// Missile — Projectile tracking (turret shots, auto attacks, spells)
//
// IDA MCP verified layout (2026-03-08):
//   CastInfo is INLINE at missile+0x2C0 (NOT a pointer!)
//   sub_886AE0 copies CastInfo struct into missile+0x2C0 via sub_845A50
//   sub_90A0E0 reads Position at +0x25C, CasterNetId at +0x358
//
//   missile+0x128 = SpellData ptr (direct)
//   missile+0x25C = Position (Vec3, inherited from GameObject)
//   missile+0x2C0 = CastInfo INLINE base
//     +0x2C0 = SpellData ptr (= CastInfo+0x00)
//     +0x2E0 = SpellName (std::string SSO, CastInfo+0x20)
//     +0x308 = MissileName (std::string SSO, CastInfo+0x48)
//     +0x330 = StartPos (Vec3, CastInfo+0x70)
//     +0x33C = EndPos (Vec3, CastInfo+0x7C)
//     +0x34C = CastEndPos (Vec3, CastInfo+0x8C)
//     +0x358 = CasterNetId (int, CastInfo+0x98)
//     +0x364 = MissileNetId (int, CastInfo+0xA4)
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
        // CastInfo fields — read DIRECTLY (CastInfo is INLINE, no dereference!)
        // ====================================================================

        // Source caster network ID (CastInfo+0x98 → missile+0x358)
        int GetCasterNetId() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::Missile::CasterNetId);
        }

        // Target network ID (CastInfo+0x9C → missile+0x35C)
        int GetTargetNetId() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::Missile::TargetNetId);
        }

        // Missile network ID (CastInfo+0xA4 → missile+0x364)
        int GetNetworkId() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::Missile::MissileNetId);
        }

        // Start position — where spell was cast from (CastInfo+0x70 → missile+0x330)
        Vec3 GetStartPos() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::Missile::StartPos);
        }

        // End position — target destination (CastInfo+0x7C → missile+0x33C)
        Vec3 GetEndPos() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::Missile::EndPos);
        }

        // Cast end position (CastInfo+0x8C → missile+0x34C)
        Vec3 GetCastEndPos() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::Missile::CastEndPos);
        }

        // ====================================================================
        // Spell / Missile name (std::string SSO, read DIRECTLY from missile)
        // ====================================================================

        std::string GetSpellName() const {
            if (!IsValid()) return "";
            char buf[128] = {};
            if (!Globals::ReadGameString(address + Offset::Missile::SpellName, buf, sizeof(buf)))
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

        // ====================================================================
        // SpellData pointer (direct at missile+0x128)
        // ====================================================================

        uintptr_t GetSpellData() const {
            if (!IsValid()) return 0;
            return Globals::Read<uintptr_t>(address + Offset::Missile::SpellDataPtr);
        }

        // SpellData ptr from CastInfo (first QWORD at CastInfo base)
        uintptr_t GetSpellDataFromCastInfo() const {
            if (!IsValid()) return 0;
            return Globals::Read<uintptr_t>(address + Offset::Missile::CI_SpellData);
        }

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
                    return true;
                }
            }
            return false;
        }
    };

} // namespace SDK
