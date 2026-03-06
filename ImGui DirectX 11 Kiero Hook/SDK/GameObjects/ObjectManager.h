#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "GameObject.h"
#include <vector>
#include <functional>

// ============================================================================
// ObjectManager — Enumerate game objects
// Reference: EnsoulSharp GameObjects.cs + Script-New-main/SDK/ObjectManager.h
// ============================================================================

namespace SDK {

    class ObjectManager {
    public:
        // Get local player
        static GameObject GetLocalPlayer() {
            uintptr_t ptr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::LocalPlayer);
            return GameObject(ptr);
        }

        // Get module base
        static uintptr_t GetModuleBase() {
            return Globals::base;
        }

        // ====================================================================
        // Object Iteration via GetFirstObject/GetNextObject
        // ====================================================================

        // Iterate all objects using GetFirstObject/GetNextObject
        static void ForEach(const std::function<void(GameObject&)>& callback) {
            // Collect addresses with SEH, iterate outside __try
            uintptr_t addrs[2000] = {};
            int count = IterateObjectsSafe(addrs, 2000);
            for (int i = 0; i < count; i++) {
                GameObject go(addrs[i]);
                if (go.IsValid()) {
                    callback(go);
                }
            }
        }

    private:
        // SEH-safe: iterate objects into raw array (no C++ objects)
        static int IterateObjectsSafe(uintptr_t* out, int maxCount) {
            __try {
                uintptr_t mgr = *(uintptr_t*)(Globals::base + Offset::Global::ObjectManager);
                if (mgr < 0x10000 || mgr > 0x7FFFFFFFFFFF) return 0;

                typedef uintptr_t(__cdecl* fnGetFirst)(uintptr_t);
                typedef uintptr_t(__cdecl* fnGetNext)(uintptr_t, uintptr_t);

                fnGetFirst getFirst = (fnGetFirst)(Globals::base + Offset::Function::GetFirstObject);
                fnGetNext getNext   = (fnGetNext)(Globals::base + Offset::Function::GetNextObject);

                uintptr_t obj = getFirst(mgr);
                int count = 0;
                while (obj > 0x10000 && obj < 0x7FFFFFFFFFFF && count < maxCount) {
                    out[count++] = obj;
                    obj = getNext(mgr, obj);
                }
                return count;
            } __except(1) { return 0; }
        }
    public:

        // ====================================================================
        // Hero List via HeroManager pointer
        // ====================================================================

        static std::vector<GameObject> GetHeroes() {
            std::vector<GameObject> result;
            uintptr_t mgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::HeroManager);
            if (!Globals::IsValidPtr(mgr)) return result;

            uintptr_t list = Globals::Read<uintptr_t>(mgr + 0x8);
            int count = Globals::Read<int>(mgr + 0x10);
            if (!Globals::IsValidPtr(list) || count <= 0 || count > 12) return result;

            uintptr_t addrs[12] = {};
            int n = Globals::ReadPtrArray(list, count, addrs, 12);
            for (int i = 0; i < n; i++) {
                if (Globals::IsValidPtr(addrs[i]))
                    result.emplace_back(addrs[i]);
            }
            return result;
        }

        // ====================================================================
        // Minion List via MinionManager pointer
        // ====================================================================

        static std::vector<GameObject> GetMinions() {
            std::vector<GameObject> result;
            uintptr_t mgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::MinionManager);
            if (!Globals::IsValidPtr(mgr)) return result;

            uintptr_t list = Globals::Read<uintptr_t>(mgr + 0x8);
            int count = Globals::Read<int>(mgr + 0x10);
            if (!Globals::IsValidPtr(list) || count <= 0 || count > 500) return result;

            uintptr_t addrs[500] = {};
            int n = Globals::ReadPtrArray(list, count, addrs, 500);
            for (int i = 0; i < n; i++) {
                if (Globals::IsValidPtr(addrs[i]))
                    result.emplace_back(addrs[i]);
            }
            return result;
        }

        // ====================================================================
        // Missile List via MissileManager pointer
        // ====================================================================

        static std::vector<GameObject> GetMissiles() {
            std::vector<GameObject> result;
            uintptr_t mgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::MissileManager);
            if (!Globals::IsValidPtr(mgr)) return result;

            uintptr_t list = Globals::Read<uintptr_t>(mgr + 0x8);
            int count = Globals::Read<int>(mgr + 0x10);
            if (!Globals::IsValidPtr(list) || count <= 0 || count > 500) return result;

            uintptr_t addrs[500] = {};
            int n = Globals::ReadPtrArray(list, count, addrs, 500);
            for (int i = 0; i < n; i++) {
                if (Globals::IsValidPtr(addrs[i]))
                    result.emplace_back(addrs[i]);
            }
            return result;
        }
    };

} // namespace SDK
