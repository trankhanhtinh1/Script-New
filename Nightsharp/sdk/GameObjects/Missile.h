#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"
#include "ObjectManager.h"
#include "SpellBook.h"
#include "Enums.h"
#include <cmath>
#include <string>
#include <cstdint>
#include <unordered_set>
#include <vector>

// ============================================================================
// Missile — Projectile tracking (turret shots, auto attacks, spells)
//
// IDA MCP verified layout (2026-03-10):
//   CastInfo is INLINE at missile+0x2C0 (NOT a pointer!)
//   sub_886BB0 copies CastInfo struct into missile+0x2C0 via sub_845B20
//
//   missile+0x128 = SpellData ptr (direct)
//   missile+0x25C = Position (Vec3, inherited from GameObject)
//   missile+0x2C0 = CastInfo INLINE base
//     +0x2C0 = SpellData ptr (= CastInfo+0x00)
//     +0x2E0 = SpellName (std::string SSO, CastInfo+0x20)
//     +0x308 = MissileName (std::string SSO, CastInfo+0x48)
//     +0x388 = StartPos (Vec3, CastInfo+0xC8)  [brute confirmed]
//     +0x394 = EndPos (Vec3, CastInfo+0xD4)    [brute confirmed]
//     +0x3A4 = CastEndPos (Vec3, CastInfo+0xE4) [brute confirmed]
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
            return SanitizeWorldPos(Globals::Read<Vec3>(address + Offset::Missile::Position));
        }

        // ====================================================================
        // CastInfo fields — read DIRECTLY (CastInfo is INLINE, no dereference!)
        // ====================================================================

        // Source caster network ID (CastInfo+0x98 → missile+0x358) [confirmed]
        int GetCasterNetId() const {
            if (!IsValid()) return 0;
            int netId = Globals::Read<int>(address + Offset::Missile::CasterNetId);
            return netId;
        }

        // Target network ID (CastInfo+0x9C → missile+0x35C)
        int GetTargetNetId() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::Missile::TargetNetId);
        }

        // Target destination index (CastInfo+0x108 → missile+0x3C8)
        // This is a pointer to the target dest index — dereference to get the value.
        // Used by EzEvade to identify which unit a targeted missile is aimed at.
        int GetDestIndex() const {
            if (!IsValid()) return 0;
            uintptr_t destPtr = Globals::Read<uintptr_t>(address + Offset::Missile::DestIndex);
            if (!Globals::IsValidPtr(destPtr)) return 0;
            return Globals::Read<int>(destPtr);
        }

        // Check if this missile is targeting a specific network ID
        bool IsTargeting(int netId) const {
            if (netId <= 0) return false;
            // Check primary target net ID first
            int targetNet = GetTargetNetId();
            if (targetNet == netId) return true;
            // Fallback: check dest index (for some targeted spells)
            int destIdx = GetDestIndex();
            if (destIdx == netId) return true;
            return false;
        }

        // Missile network ID (CastInfo+0xA4 → missile+0x364)
        int GetNetworkId() const {
            if (!IsValid()) return 0;
            int netId = Globals::Read<int>(address + Offset::Missile::MissileNetId);
            if (netId != 0) {
                return netId;
            }
            return Globals::Read<int>(address + Offset::GameObject::NetId);
        }

        // Start position — where spell was cast from
        // Primary: CastInfo+0xC8 (missile+0x388) [brute confirmed]
        // Fallback: CastInfo+0x70 (missile+0x330) [old offset, has data earlier]
        Vec3 GetStartPos() const {
            if (!IsValid()) return Vec3();
            // Try confirmed offset first
            Vec3 pos = SanitizeWorldPos(Globals::Read<Vec3>(address + Offset::Missile::StartPos));
            if (!pos.IsZero()) return pos;
            // Fallback: old offset (CastInfo may populate this area first)
            pos = SanitizeWorldPos(Globals::Read<Vec3>(address + 0x330));
            if (!pos.IsZero()) return pos;
            // Fallback: MissileClient
            pos = ReadMissileClientVec(kMissileClientStartPos);
            if (!pos.IsZero()) return pos;
            // Last resort: current position
            return GetPosition();
        }

        // End position — target destination
        // Primary: CastInfo+0xD4 (missile+0x394) [brute confirmed]
        // Fallback: CastInfo+0x7C (missile+0x33C) [old offset]
        Vec3 GetEndPos() const {
            if (!IsValid()) return Vec3();
            Vec3 pos = SanitizeWorldPos(Globals::Read<Vec3>(address + Offset::Missile::EndPos));
            if (!pos.IsZero()) return pos;
            pos = SanitizeWorldPos(Globals::Read<Vec3>(address + 0x33C));
            if (!pos.IsZero()) return pos;
            return ReadMissileClientVec(kMissileClientEndPos);
        }

        // Cast end position
        // Primary: CastInfo+0xE4 (missile+0x3A4) [brute confirmed]
        // Fallback: CastInfo+0x8C (missile+0x34C) [old offset]
        Vec3 GetCastEndPos() const {
            if (!IsValid()) return Vec3();
            Vec3 pos = SanitizeWorldPos(Globals::Read<Vec3>(address + Offset::Missile::CastEndPos));
            if (!pos.IsZero()) return pos;
            pos = SanitizeWorldPos(Globals::Read<Vec3>(address + 0x34C));
            if (!pos.IsZero()) return pos;
            return ReadMissileClientVec(kMissileClientCastPos);
        }

        // ====================================================================
        // Spell / Missile name (std::string SSO, read DIRECTLY from missile)
        // ====================================================================

        std::string GetSpellName() const {
            if (!IsValid()) return "";
            std::string name = ReadStringField(address + Offset::Missile::SpellName);
            if (!name.empty()) {
                return name;
            }

            SpellData spellData(GetSpellData());
            name = spellData.GetName();
            if (!name.empty()) {
                return name;
            }

            SpellData castInfoData(GetSpellDataFromCastInfo());
            return castInfoData.GetName();
        }

        std::string GetMissileName() const {
            if (!IsValid()) return "";
            std::string name = ReadStringField(address + Offset::Missile::MissileName);
            if (!name.empty()) {
                return name;
            }

            uintptr_t missileClient = GetMissileClientPtr();
            if (Globals::IsValidPtr(missileClient)) {
                SpellData missileClientData(Globals::Read<uintptr_t>(missileClient + kMissileClientSpellInfo));
                name = missileClientData.GetName();
                if (!name.empty()) {
                    return name;
                }
            }

            SpellData spellData(GetSpellData());
            return spellData.GetName();
        }

        // ====================================================================
        // SpellData pointer (direct at missile+0x128)
        // ====================================================================

        uintptr_t GetSpellData() const {
            if (!IsValid()) return 0;
            uintptr_t ptr = Globals::Read<uintptr_t>(address + Offset::Missile::SpellDataPtr);
            if (Globals::IsValidPtr(ptr)) {
                return ptr;
            }

            uintptr_t missileClient = GetMissileClientPtr();
            if (Globals::IsValidPtr(missileClient)) {
                ptr = Globals::Read<uintptr_t>(missileClient + kMissileClientSpellInfo);
                if (Globals::IsValidPtr(ptr)) {
                    return ptr;
                }
            }

            return 0;
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

    private:
        static constexpr uintptr_t kMissileClientSpellInfo = 0x8;
        static constexpr uintptr_t kMissileClientStartPos  = 0xAC;
        static constexpr uintptr_t kMissileClientEndPos    = 0xB8;
        static constexpr uintptr_t kMissileClientCastPos   = 0xC4;
        static constexpr float kWorldBound = 50000.0f;

        static bool IsReasonableWorldPos(const Vec3& pos) {
            return std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z) &&
                   std::fabs(pos.x) < kWorldBound &&
                   std::fabs(pos.y) < kWorldBound &&
                   std::fabs(pos.z) < kWorldBound;
        }

        static Vec3 SanitizeWorldPos(const Vec3& pos) {
            return IsReasonableWorldPos(pos) ? pos : Vec3();
        }

        uintptr_t GetMissileClientPtr() const {
            if (!IsValid()) {
                return 0;
            }
            return Globals::Read<uintptr_t>(address + Offset::GameObject::MissileClient);
        }

        static std::string ReadStringField(uintptr_t addr) {
            char buf[128] = {};
            if (!Globals::ReadGameString(addr, buf, sizeof(buf))) {
                return "";
            }
            return std::string(buf);
        }

        Vec3 ReadMissileClientVec(uintptr_t offset) const {
            uintptr_t missileClient = GetMissileClientPtr();
            if (!Globals::IsValidPtr(missileClient)) {
                return Vec3();
            }
            return SanitizeWorldPos(Globals::Read<Vec3>(missileClient + offset));
        }
    };

    // ========================================================================
    // MissileManager — Get all active missiles
    // ========================================================================
    class MissileManager {
    public:
        static std::vector<Missile> GetMissiles() {
            std::vector<Missile> result;
            std::unordered_set<uintptr_t> seenObjects;
            uintptr_t mgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::MissileManager);
            if (Globals::IsValidPtr(mgr)) {
                TraverseMissileTree(mgr, result, seenObjects);
            }

            // Fallback: if manager list is empty/misaligned, scan all objects and pick
            // entries that look like active missiles by CastInfo + trajectory data.
            if (result.empty()) {
                ObjectManager::ForEach([&](GameObject& obj) {
                    uintptr_t addr = obj.address;
                    if (!Globals::IsValidPtr(addr) || !seenObjects.insert(addr).second) {
                        return;
                    }

                    uintptr_t missileClient = Globals::Read<uintptr_t>(obj.address + Offset::GameObject::MissileClient);
                    if (!Globals::IsValidPtr(missileClient)) {
                        return;
                    }

                    Missile m(addr);
                    if (!m.IsValid()) {
                        return;
                    }

                    uintptr_t sd1 = m.GetSpellData();
                    uintptr_t sd2 = m.GetSpellDataFromCastInfo();
                    Vec3 start = m.GetStartPos();
                    Vec3 end = m.GetEndPos();
                    Vec3 castEnd = m.GetCastEndPos();
                    Vec3 pos = m.GetPosition();
                    int caster = m.GetCasterNetId();
                    std::string spell = m.GetSpellName();
                    std::string missile = m.GetMissileName();

                    // Basic sanity to avoid non-missile objects.
                    bool hasMissileClient = Globals::IsValidPtr(missileClient);
                    bool hasSpellData = Globals::IsValidPtr(sd1) || Globals::IsValidPtr(sd2);
                    bool hasNames = !spell.empty() || !missile.empty();
                    if (!hasMissileClient && !hasSpellData && !hasNames) {
                        return;
                    }
                    bool hasTrajectory = !(start.IsZero() && end.IsZero() && castEnd.IsZero());
                    if (!hasTrajectory) {
                        return;
                    }
                    if (pos.IsZero() && start.IsZero() && end.IsZero()) {
                        return;
                    }
                    if (!start.IsZero() && !end.IsZero() && start.Distance2D(end) < 3.0f) {
                        return;
                    }
                    if (caster <= 0 && !hasSpellData && missile.empty()) {
                        return;
                    }

                    result.emplace_back(addr);
                });
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

    private:
        static constexpr uintptr_t kTreeHeadPtr = 0x8;
        static constexpr uintptr_t kHeadRootPtr = 0x8;
        static constexpr uintptr_t kNodeLeft = 0x0;
        static constexpr uintptr_t kNodeRight = 0x10;
        static constexpr uintptr_t kNodeIsNil = 0x19;
        static constexpr uintptr_t kNodeValue = 0x28;

        static void TraverseMissileTree(uintptr_t mgr,
                                        std::vector<Missile>& out,
                                        std::unordered_set<uintptr_t>& seenObjects) {
            uintptr_t head = Globals::Read<uintptr_t>(mgr + kTreeHeadPtr);
            if (!Globals::IsValidPtr(head)) {
                return;
            }

            uintptr_t root = Globals::Read<uintptr_t>(head + kHeadRootPtr);
            if (!Globals::IsValidPtr(root)) {
                return;
            }

            std::unordered_set<uintptr_t> visitedNodes;
            std::vector<uintptr_t> stack;
            stack.push_back(root);

            while (!stack.empty() && out.size() < 512) {
                uintptr_t node = stack.back();
                stack.pop_back();

                if (!Globals::IsValidPtr(node) || !visitedNodes.insert(node).second) {
                    continue;
                }

                if (Globals::Read<uint8_t>(node + kNodeIsNil) != 0) {
                    continue;
                }

                uintptr_t value = Globals::Read<uintptr_t>(node + kNodeValue);
                if (Globals::IsValidPtr(value) && seenObjects.insert(value).second) {
                    Missile m(value);
                    if (m.IsValid()) {
                        out.emplace_back(value);
                    }
                }

                uintptr_t left = Globals::Read<uintptr_t>(node + kNodeLeft);
                uintptr_t right = Globals::Read<uintptr_t>(node + kNodeRight);
                if (Globals::IsValidPtr(left)) {
                    stack.push_back(left);
                }
                if (Globals::IsValidPtr(right)) {
                    stack.push_back(right);
                }
            }
        }
    };

} // namespace SDK
