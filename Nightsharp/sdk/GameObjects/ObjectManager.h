#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "GameObject.h"
#include "sdk/Utils/Bypass.h"
#include "sdk/Utils/DebugConsole.h"
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
            // Run mainloop guard once at the beginning of object iteration cycle.
            SDK::Bypass::MainloopCheck();

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

static GameObject GetObjectByNetId(int netId)
{
    if (netId <= 0)
        return GameObject(0);

    GameObject fallback(0);

    ForEach([&](GameObject& obj)
    {
        if (!obj.IsValid())
            return;

        if (obj.GetNetId() == netId)
        {
            fallback = obj;
            return;
        }

        if (!fallback.IsValid())
        {
            if ((obj.GetIndex() & 0xFFFF) == (netId & 0xFFFF))
                fallback = obj;
        }
    });

    return fallback;
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
                fnGetFirst getFirstAlt = (fnGetFirst)(Globals::base + Offset::Function::GetFirstObjectAlt);
                fnGetNext getNext   = (fnGetNext)(Globals::base + Offset::Function::GetNextObject);

                auto iterateFrom = [&](fnGetFirst starter) -> int {
                    uintptr_t obj = starter(mgr);
                    int count = 0;
                    while (obj > 0x10000 && obj < 0x7FFFFFFFFFFF && count < maxCount) {
                        out[count++] = obj;
                        obj = getNext(mgr, obj);
                    }
                    return count;
                };

                int count = iterateFrom(getFirst);
                if (count == 0 && getFirstAlt != nullptr &&
                    (uintptr_t)getFirstAlt != (uintptr_t)getFirst) {
                    count = iterateFrom(getFirstAlt);
                }

                if (count == 0) {
                    uintptr_t list = Globals::Read<uintptr_t>(mgr + 0x8);
                    int listCount = Globals::Read<int>(mgr + 0x10);
                    if (Globals::IsValidPtr(list) && listCount > 0 && listCount <= maxCount) {
                        count = Globals::ReadPtrArray(list, listCount, out, maxCount);
                    }
                }

                static ULONGLONG s_lastLog = 0;
                ULONGLONG now = GetTickCount64();
                if (now - s_lastLog > 2000) {
                    s_lastLog = now;
                    DebugConsole::LogTagged("ObjIter", "count=%d (first=%p alt=%p)",
                        count, (void*)getFirst, (void*)getFirstAlt);
                }

                return count;
            } __except(1) { return 0; }
        }
    public:

        // ====================================================================
        // Hero List via HeroManager pointer
        // ====================================================================

        static void FillHeroes(std::vector<GameObject>& result) {
            result.clear();
            uintptr_t mgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::HeroManager);
            if (!Globals::IsValidPtr(mgr)) return;

            uintptr_t list = Globals::Read<uintptr_t>(mgr + 0x8);
            int count = Globals::Read<int>(mgr + 0x10);
            if (!Globals::IsValidPtr(list) || count <= 0 || count > 12) return;
            result.reserve((size_t)count);

            uintptr_t addrs[12] = {};
            int n = Globals::ReadPtrArray(list, count, addrs, 12);
            for (int i = 0; i < n; i++) {
                if (Globals::IsValidPtr(addrs[i]))
                    result.emplace_back(addrs[i]);
            }
        }

        static std::vector<GameObject> GetHeroes() {
            std::vector<GameObject> result;
            FillHeroes(result);
            return result;
        }

        // ====================================================================
        // Minion List via MinionManager pointer
        // ====================================================================

        static void FillMinions(std::vector<GameObject>& result) {
            uintptr_t mgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::MinionManager);
            result.clear();
            if (!Globals::IsValidPtr(mgr)) return;

            uintptr_t list = Globals::Read<uintptr_t>(mgr + 0x8);
            int count = Globals::Read<int>(mgr + 0x10);

            if (!Globals::IsValidPtr(list) || count <= 0 || count > 2000) return;
            result.reserve((size_t)count);

            uintptr_t addrs[2000] = {};
            int n = Globals::ReadPtrArray(list, count, addrs, 2000);
            for (int i = 0; i < n; i++) {
                if (Globals::IsValidPtr(addrs[i]))
                    result.emplace_back(addrs[i]);
            }

            static ULONGLONG s_lastLog = 0;
            ULONGLONG now = GetTickCount64();
            if (now - s_lastLog > 2000) {
                s_lastLog = now;
                DebugConsole::LogTagged("MinionMgr", "ptr result=%llu (mgr=%p list=%p count=%d)",
                    (unsigned long long)result.size(), (void*)mgr, (void*)list, count);
            }
        }

        static std::vector<GameObject> GetMinions() {
            std::vector<GameObject> result;
            FillMinions(result);
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
