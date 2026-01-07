#pragma once
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "Offsets.h"
#include "GameObject.h"

namespace SDK
{
    class ObjectManager
    {
    public:
        static uint64_t GetModuleBase() {
            return (uint64_t)GetModuleHandle(NULL);
        }

        static GameObject* GetLocalPlayer() {
            __try {
                uint64_t localParams = *(uint64_t*)(GetModuleBase() + Offset::oLocalPlayer);
                if (!localParams) return nullptr;
                return new GameObject(localParams);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return nullptr;
            }
        }

        // Helper to read a standard array of pointers ? 
        // Or is it a list of structs?
        // Based on "MinionList" having "LaneMinionArray" at 0x68, it's a specific manager struct.
        // For Heroes, usually it's just an array of pointers.
        
        static std::vector<GameObject*> GetHeroes() {
            std::vector<GameObject*> heroes;
            __try {
                uint64_t heroManager = *(uint64_t*)(GetModuleBase() + Offset::oHerroList);
                if (!heroManager) return heroes;

                int size = *(int*)(heroManager + Offset::oListSizeHero);
                if (size > 200 || size < 0) size = 0;

                uint64_t arrayPtr = *(uint64_t*)(heroManager + 0x08);
                if (!arrayPtr) return heroes;

                for (int i = 0; i < size; i++) {
                    __try {
                        uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                        if (objAddr) {
                            heroes.push_back(new GameObject(objAddr));
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return heroes;
        }

        static std::vector<GameObject*> GetMinions() {
            std::vector<GameObject*> minions;
            __try {
                uint64_t minionManager = *(uint64_t*)(GetModuleBase() + Offset::oMinionList);
                if (!minionManager) return minions;

                uint64_t laneMinionsArray = *(uint64_t*)(minionManager + Offset::LaneMinionArray);
                int count = *(int*)(minionManager + Offset::LaneMinionCount);

                if (count > 500 || count < 0) count = 0;
                if (!laneMinionsArray) return minions;

                for (int i = 0; i < count; i++) {
                    __try {
                        uint64_t objAddr = *(uint64_t*)(laneMinionsArray + (i * 0x8));
                        if (objAddr) {
                            minions.push_back(new GameObject(objAddr));
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return minions;
        }

        static std::vector<GameObject*> GetAllMinions() {
            std::vector<GameObject*> allMinions;
            __try {
                uint64_t minionManager = *(uint64_t*)(GetModuleBase() + Offset::oMinionList);
                if (!minionManager) return allMinions;

                uint64_t arrayPtr = *(uint64_t*)(minionManager + 0x08);
                int size = *(int*)(minionManager + 0x10);

                if (size > 1000 || size < 0) size = 0;
                if (!arrayPtr) return allMinions;

                for (int i = 0; i < size; i++) {
                    __try {
                        uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                        if (objAddr) {
                            allMinions.push_back(new GameObject(objAddr));
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return allMinions;
        }

        static std::vector<GameObject*> GetTurrets() {
            std::vector<GameObject*> turrets;
            __try {
                uint64_t turretManager = *(uint64_t*)(GetModuleBase() + Offset::oTurretList);
                if (!turretManager) return turrets;

                uint64_t arrayPtr = *(uint64_t*)(turretManager + 0x08);
                int size = *(int*)(turretManager + 0x10);

                if (size > 100 || size < 0) size = 0;
                if (!arrayPtr) return turrets;

                for (int i = 0; i < size; i++) {
                    __try {
                        uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                        if (objAddr) {
                            turrets.push_back(new GameObject(objAddr));
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return turrets;
        }

        static std::vector<GameObject*> GetMissiles() {
            std::vector<GameObject*> missiles;
            __try {
                uint64_t missileManager = *(uint64_t*)(GetModuleBase() + Offset::oMisslilist);
                if (!missileManager) return missiles;

                uint64_t arrayPtr = *(uint64_t*)(missileManager + 0x08);
                int size = *(int*)(missileManager + 0x10);

                if (size > 500 || size < 0) size = 0;
                if (!arrayPtr) return missiles;

                for (int i = 0; i < size; i++) {
                    __try {
                        uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                        if (objAddr) {
                            missiles.push_back(new GameObject(objAddr));
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return missiles;
        }

        // Cleanup helper if we user 'new' (Ideally should use smart pointers or just return objects)
        // For simplicity in this cheat context, we often leak or manage manually.
        // Better: Return std::vector<GameObject> (by value) since it's just a wrapper around a pointer.
    };
}
