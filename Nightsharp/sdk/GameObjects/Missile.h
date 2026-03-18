#pragma once
#include "core/RuntimeAPI.h"
#include "core/Globals.h"
#include "core/Vector.h"
#include "ObjectManager.h"
#include "MissileClassification.h"
#include <cmath>
#include <string>
#include <cstdint>
#include <unordered_set>
#include <vector>

// ============================================================================
// Missile — Projectile tracking (turret shots, auto attacks, spells)
//
// ALL data reads go through RuntimeAPI (SEH protected).
// No direct Offset:: usage in this file — RuntimeAPI handles all offsets.
//
// RuntimeAPI functions used:
//   GetMissilePosition, GetMissileStartPos, GetMissileEndPos, GetMissileCastEndPos
//   GetMissileCasterNetId, GetMissileTargetNetId, GetMissileDestIndex
//   GetMissileSpellName, GetMissileMissileName, GetMissileSpellData
//   GetMissileSpeed, GetMissileWidth, GetMissileCastRange
//   GetMissileIsAuto, GetMissileSpellSlot
//   ClassifyMissile, IsMissile, GetMissileManager
// ============================================================================

namespace SDK {

    class Missile {
    public:
        uintptr_t address;

        Missile() : address(0) {}
        Missile(uintptr_t addr) : address(addr) {}
        bool IsValid() const { return Globals::IsValidPtr(address); }

        // ====================================================================
        // Position
        // ====================================================================

        Vec3 GetPosition() const {
            if (!IsValid()) return Vec3();
            auto p = RuntimeAPI::GetMissilePosition(address);
            return SanitizeWorldPos(Vec3(p.x, p.y, p.z));
        }

        // ====================================================================
        // CastInfo fields
        // ====================================================================

        int GetCasterNetId() const {
            if (!IsValid()) return 0;
            return RuntimeAPI::GetMissileCasterNetId(address);
        }

        int GetTargetNetId() const {
            if (!IsValid()) return 0;
            return RuntimeAPI::GetMissileTargetNetId(address);
        }

        int GetDestIndex() const {
            if (!IsValid()) return 0;
            return RuntimeAPI::GetMissileDestIndex(address);
        }

        bool IsTargeting(int netId) const {
            if (netId <= 0) return false;
            if (GetTargetNetId() == netId) return true;
            if (GetDestIndex() == netId) return true;
            return false;
        }

        int GetNetworkId() const {
            if (!IsValid()) return 0;
            return RuntimeAPI::GetNetId(address);
        }

        bool GetIsAutoFlag() const {
            if (!IsValid()) return false;
            return RuntimeAPI::GetMissileIsAuto(address);
        }

        int GetSpellSlot() const {
            if (!IsValid()) return -1;
            return RuntimeAPI::GetMissileSpellSlot(address);
        }

        // ====================================================================
        // Positions (start / end / castEnd)
        // ====================================================================

        Vec3 GetStartPos() const {
            if (!IsValid()) return Vec3();
            auto p = RuntimeAPI::GetMissileStartPos(address);
            Vec3 pos(p.x, p.y, p.z);
            if (!pos.IsZero() && IsReasonableWorldPos(pos)) return pos;
            return GetPosition(); // fallback to current position
        }

        Vec3 GetEndPos() const {
            if (!IsValid()) return Vec3();
            auto p = RuntimeAPI::GetMissileEndPos(address);
            return SanitizeWorldPos(Vec3(p.x, p.y, p.z));
        }

        Vec3 GetCastEndPos() const {
            if (!IsValid()) return Vec3();
            auto p = RuntimeAPI::GetMissileCastEndPos(address);
            return SanitizeWorldPos(Vec3(p.x, p.y, p.z));
        }

        // ====================================================================
        // Spell / Missile name
        // ====================================================================

        std::string GetSpellName() const {
            if (!IsValid()) return "";
            const char* name = RuntimeAPI::GetMissileSpellName(address);
            if (name && name[0] != '\0') return std::string(name);
            return "";
        }

        std::string GetMissileName() const {
            if (!IsValid()) return "";
            const char* name = RuntimeAPI::GetMissileMissileName(address);
            if (name && name[0] != '\0') return std::string(name);
            // Fallback to spell name
            const char* spellName = RuntimeAPI::GetMissileSpellName(address);
            if (spellName && spellName[0] != '\0') return std::string(spellName);
            return "";
        }

        // ====================================================================
        // SpellData pointer
        // ====================================================================

        uintptr_t GetSpellData() const {
            if (!IsValid()) return 0;
            return RuntimeAPI::GetMissileSpellData(address);
        }

        // ====================================================================
        // Missile Speed / Width / Range — all via RuntimeAPI
        // ====================================================================

        float GetMissileSpeed() const {
            if (!IsValid()) return 0.0f;
            float speed = RuntimeAPI::GetMissileSpeed(address);
            if (std::isfinite(speed) && speed > 0.0f && speed < 100000.0f) {
                return speed;
            }
            return 0.0f;
        }

        float GetLineWidth() const {
            if (!IsValid()) return 0.0f;
            float width = RuntimeAPI::GetMissileWidth(address);
            if (std::isfinite(width) && width >= 0.0f && width < 10000.0f) {
                return width;
            }
            return 0.0f;
        }

        float GetCastRange() const {
            if (!IsValid()) return 0.0f;
            float range = RuntimeAPI::GetMissileCastRange(address);
            if (std::isfinite(range) && range > 0.0f && range < 50000.0f) {
                return range;
            }
            return 0.0f;
        }

        // ====================================================================
        // Missile Classification — via RuntimeAPI::ClassifyMissile
        // ====================================================================

        MissileType GetMissileType() const {
            if (!IsValid()) return MissileType::Unknown;
            return MissileClassifier::ClassifyByAddress(address);
        }

        // ====================================================================
        // Convenience type checks
        // ====================================================================

        bool IsAutoAttackMissile() const {
            return MissileClassifier::IsAnyAutoAttack(GetMissileType());
        }

        bool IsMinionMissile() const {
            return GetMissileType() == MissileType::MinionAutoAttack;
        }

        bool IsTurretMissile() const {
            return GetMissileType() == MissileType::TurretShot;
        }

        bool IsSpellMissile() const {
            return GetMissileType() == MissileType::SpellMissile;
        }

        bool IsHeroAutoAttack() const {
            return GetMissileType() == MissileType::HeroAutoAttack;
        }

        // Legacy compatibility
        bool IsTurretShot() const { return IsTurretMissile(); }
        bool IsAutoAttack() const { return IsAutoAttackMissile(); }

        // ====================================================================
        // Caster / Target object resolution
        // ====================================================================

        uintptr_t GetCasterObject() const {
            return ResolveObjectByNetId(GetCasterNetId());
        }

        uintptr_t GetTargetObject() const {
            int targetNetId = GetTargetNetId();
            if (targetNetId <= 0) return 0;
            return ResolveObjectByNetId(targetNetId);
        }

        // ====================================================================
        // Arrival time calculation
        // ====================================================================

        float GetArrivalTime(const Vec3& targetPos) const {
            float speed = GetMissileSpeed();
            if (speed <= 0.0f) return 0.0f;

            Vec3 missilePos = GetPosition();
            if (missilePos.IsZero()) missilePos = GetStartPos();
            if (missilePos.IsZero()) return 0.0f;

            float dist = missilePos.Distance2D(targetPos);
            return dist / speed;
        }

        float GetRemainingTravelTime() const {
            return GetArrivalTime(GetEndPos());
        }

    private:
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

        static uintptr_t ResolveObjectByNetId(int netId) {
            if (netId <= 0) return 0;
            uintptr_t found = 0;
            ObjectManager::ForEach([&](GameObject& obj) {
                if (found) return;
                if (RuntimeAPI::GetNetId(obj.address) == netId) {
                    found = obj.address;
                }
            });
            return found;
        }
    };

    // ========================================================================
    // MissileManager — Get all active missiles with 3-way classification
    // Uses RuntimeAPI::IsMissile and RuntimeAPI::GetMissileManager
    // ========================================================================
    class MissileManager {
    public:
        static std::vector<Missile> GetMissiles() {
            std::vector<Missile> result;
            std::unordered_set<uintptr_t> seenObjects;
            CollectMissiles(result, seenObjects);
            return result;
        }

        struct ClassifiedMissiles {
            std::vector<Missile> minionMissiles;
            std::vector<Missile> turretMissiles;
            std::vector<Missile> heroAutoMissiles;
            std::vector<Missile> spellMissiles;
            std::vector<Missile> allMissiles;
        };

        static ClassifiedMissiles GetClassifiedMissiles() {
            ClassifiedMissiles result;
            std::unordered_set<uintptr_t> seenObjects;
            CollectMissiles(result.allMissiles, seenObjects);

            for (auto& m : result.allMissiles) {
                MissileType type = m.GetMissileType();
                switch (type) {
                    case MissileType::MinionAutoAttack:
                        result.minionMissiles.push_back(m);
                        break;
                    case MissileType::TurretShot:
                        result.turretMissiles.push_back(m);
                        break;
                    case MissileType::HeroAutoAttack:
                        result.heroAutoMissiles.push_back(m);
                        break;
                    case MissileType::SpellMissile:
                        result.spellMissiles.push_back(m);
                        break;
                    default:
                        break;
                }
            }
            return result;
        }

        static std::vector<Missile> GetMinionMissiles() {
            auto all = GetMissiles();
            std::vector<Missile> result;
            for (auto& m : all) {
                if (m.GetMissileType() == MissileType::MinionAutoAttack)
                    result.push_back(m);
            }
            return result;
        }

        static std::vector<Missile> GetTurretMissiles() {
            auto all = GetMissiles();
            std::vector<Missile> result;
            for (auto& m : all) {
                if (m.GetMissileType() == MissileType::TurretShot)
                    result.push_back(m);
            }
            return result;
        }

        static std::vector<Missile> GetSpellMissiles() {
            auto all = GetMissiles();
            std::vector<Missile> result;
            for (auto& m : all) {
                if (m.GetMissileType() == MissileType::SpellMissile)
                    result.push_back(m);
            }
            return result;
        }

        static bool HasTurretAggro(int targetNetId) {
            for (auto& m : GetMissiles()) {
                if (m.IsTurretMissile() && m.IsTargeting(targetNetId))
                    return true;
            }
            return false;
        }

        static Missile GetTurretMissileOnTarget(int targetNetId) {
            for (auto& m : GetMissiles()) {
                if (m.IsTurretMissile() && m.IsTargeting(targetNetId))
                    return m;
            }
            return Missile(0);
        }

        static int CountIncomingAutoAttacks(int targetNetId) {
            int count = 0;
            for (auto& m : GetMissiles()) {
                if (m.IsAutoAttackMissile() && m.IsTargeting(targetNetId))
                    count++;
            }
            return count;
        }

    private:
        // Tree node layout constants (std::set internal structure)
        static constexpr uintptr_t kTreeHeadPtr = 0x8;
        static constexpr uintptr_t kHeadRootPtr = 0x8;
        static constexpr uintptr_t kNodeLeft    = 0x0;
        static constexpr uintptr_t kNodeRight   = 0x10;
        static constexpr uintptr_t kNodeIsNil   = 0x19;
        static constexpr uintptr_t kNodeValue   = 0x28;

        static void CollectMissiles(std::vector<Missile>& out, std::unordered_set<uintptr_t>& seenObjects) {
            // Primary: MissileManager tree traversal via RuntimeAPI
            uintptr_t mgr = RuntimeAPI::GetMissileManager();
            if (Globals::IsValidPtr(mgr)) {
                TraverseMissileTree(mgr, out, seenObjects);
            }

            // Fallback: scan all objects for missiles if tree is empty
            if (out.empty()) {
                ObjectManager::ForEach([&](GameObject& obj) {
                    uintptr_t addr = obj.address;
                    if (!Globals::IsValidPtr(addr) || !seenObjects.insert(addr).second)
                        return;

                    if (!RuntimeAPI::IsMissile(addr)) return;

                    Missile m(addr);
                    if (!m.IsValid()) return;

                    Vec3 start = m.GetStartPos();
                    Vec3 end   = m.GetEndPos();
                    Vec3 pos   = m.GetPosition();

                    if (start.IsZero() && end.IsZero()) return;
                    if (pos.IsZero() && start.IsZero() && end.IsZero()) return;
                    if (!start.IsZero() && !end.IsZero() && start.Distance2D(end) < 3.0f) return;

                    out.emplace_back(addr);
                });
            }
        }

        static void TraverseMissileTree(uintptr_t mgr,
                                        std::vector<Missile>& out,
                                        std::unordered_set<uintptr_t>& seenObjects) {
            uintptr_t head = Globals::Read<uintptr_t>(mgr + kTreeHeadPtr);
            if (!Globals::IsValidPtr(head)) return;

            uintptr_t root = Globals::Read<uintptr_t>(head + kHeadRootPtr);
            if (!Globals::IsValidPtr(root)) return;

            std::unordered_set<uintptr_t> visitedNodes;
            std::vector<uintptr_t> stack;
            stack.push_back(root);

            while (!stack.empty() && out.size() < 512) {
                uintptr_t node = stack.back();
                stack.pop_back();

                if (!Globals::IsValidPtr(node) || !visitedNodes.insert(node).second)
                    continue;

                if (Globals::Read<uint8_t>(node + kNodeIsNil) != 0)
                    continue;

                uintptr_t value = Globals::Read<uintptr_t>(node + kNodeValue);
                if (Globals::IsValidPtr(value) && seenObjects.insert(value).second) {
                    Missile m(value);
                    if (m.IsValid())
                        out.emplace_back(value);
                }

                uintptr_t left  = Globals::Read<uintptr_t>(node + kNodeLeft);
                uintptr_t right = Globals::Read<uintptr_t>(node + kNodeRight);
                if (Globals::IsValidPtr(left))  stack.push_back(left);
                if (Globals::IsValidPtr(right)) stack.push_back(right);
            }
        }
    };

} // namespace SDK
