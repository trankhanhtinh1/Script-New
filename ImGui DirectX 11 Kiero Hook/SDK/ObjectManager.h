#pragma once
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "Offsets.h"
#include "GameObject.h"
#include "DebugLog.h"

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
                uint64_t base = GetModuleBase();
                uint64_t localParams = *(uint64_t*)(base + Offset::oLocalPlayer);
                Debug::LogOffset("GetLocalPlayer", base, Offset::oLocalPlayer, localParams);
                if (!localParams) return nullptr;
                return new GameObject(localParams);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                Debug::Log("!!! GetLocalPlayer EXCEPTION !!!");
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
                uint64_t base = GetModuleBase();
                uint64_t heroManager = *(uint64_t*)(base + Offset::oHerroList);
                Debug::LogOffset("GetHeroes.heroManager", base, Offset::oHerroList, heroManager);
                if (!heroManager) return heroes;

                int size = *(int*)(heroManager + Offset::oListSizeHero);
                Debug::LogInt("GetHeroes.size", size);
                if (size > 200 || size < 0) size = 0;

                uint64_t arrayPtr = *(uint64_t*)(heroManager + 0x08);
                Debug::LogHex("GetHeroes.arrayPtr", arrayPtr);
                if (!arrayPtr) return heroes;

                for (int i = 0; i < size; i++) {
                    __try {
                        uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                        if (objAddr) {
                            heroes.push_back(new GameObject(objAddr));
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        Debug::Log("!!! GetHeroes loop EXCEPTION !!!");
                    }
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                Debug::Log("!!! GetHeroes EXCEPTION !!!");
            }
            return heroes;
        }

        static std::vector<GameObject*> GetMinions() {
            std::vector<GameObject*> minions;
            __try {
                uint64_t base = GetModuleBase();
                uint64_t minionManager = *(uint64_t*)(base + Offset::oMinionList);
                Debug::LogOffset("GetMinions.minionManager", base, Offset::oMinionList, minionManager);
                if (!minionManager) return minions;

                uint64_t laneMinionsArray = *(uint64_t*)(minionManager + Offset::LaneMinionArray);
                int count = *(int*)(minionManager + Offset::LaneMinionCount);
                Debug::LogHex("GetMinions.laneMinionsArray", laneMinionsArray);
                Debug::LogInt("GetMinions.count", count);

                if (count > 500 || count < 0) count = 0;
                if (!laneMinionsArray) return minions;

                for (int i = 0; i < count; i++) {
                    __try {
                        uint64_t objAddr = *(uint64_t*)(laneMinionsArray + (i * 0x8));
                        if (objAddr) {
                            minions.push_back(new GameObject(objAddr));
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        Debug::Log("!!! GetMinions loop EXCEPTION !!!");
                    }
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                Debug::Log("!!! GetMinions EXCEPTION !!!");
            }
            return minions;
        }

        static std::vector<GameObject*> GetAllMinions() {
            std::vector<GameObject*> allMinions;
            __try {
                uint64_t base = GetModuleBase();
                uint64_t minionManager = *(uint64_t*)(base + Offset::oMinionList);
                Debug::LogOffset("GetAllMinions.minionManager", base, Offset::oMinionList, minionManager);
                if (!minionManager) return allMinions;

                uint64_t arrayPtr = *(uint64_t*)(minionManager + 0x08);
                int size = *(int*)(minionManager + 0x10);
                Debug::LogHex("GetAllMinions.arrayPtr", arrayPtr);
                Debug::LogInt("GetAllMinions.size", size);

                if (size > 1000 || size < 0) size = 0;
                if (!arrayPtr) return allMinions;

                for (int i = 0; i < size; i++) {
                    __try {
                        uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                        if (objAddr) {
                            allMinions.push_back(new GameObject(objAddr));
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        Debug::Log("!!! GetAllMinions loop EXCEPTION !!!");
                    }
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                Debug::Log("!!! GetAllMinions EXCEPTION !!!");
            }
            return allMinions;
        }

        static std::vector<GameObject*> GetTurrets() {
            std::vector<GameObject*> turrets;
            __try {
                uint64_t base = GetModuleBase();
                uint64_t turretManager = *(uint64_t*)(base + Offset::oTurretList);
                Debug::LogOffset("GetTurrets.turretManager", base, Offset::oTurretList, turretManager);
                if (!turretManager) return turrets;

                uint64_t arrayPtr = *(uint64_t*)(turretManager + 0x08);
                int size = *(int*)(turretManager + 0x10);
                Debug::LogHex("GetTurrets.arrayPtr", arrayPtr);
                Debug::LogInt("GetTurrets.size", size);

                if (size > 100 || size < 0) size = 0;
                if (!arrayPtr) return turrets;

                for (int i = 0; i < size; i++) {
                    __try {
                        uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                        if (objAddr) {
                            turrets.push_back(new GameObject(objAddr));
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        Debug::Log("!!! GetTurrets loop EXCEPTION !!!");
                    }
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                Debug::Log("!!! GetTurrets EXCEPTION !!!");
            }
            return turrets;
        }

        static std::vector<GameObject*> GetMissiles() {
            std::vector<GameObject*> missiles;
            __try {
                uint64_t base = GetModuleBase();
                uint64_t missileManager = *(uint64_t*)(base + Offset::oMisslilist);
                Debug::LogOffset("GetMissiles.missileManager", base, Offset::oMisslilist, missileManager);
                if (!missileManager) return missiles;

                uint64_t arrayPtr = *(uint64_t*)(missileManager + 0x08);
                int size = *(int*)(missileManager + 0x10);
                Debug::LogHex("GetMissiles.arrayPtr", arrayPtr);
                Debug::LogInt("GetMissiles.size", size);

                if (size > 500 || size < 0) size = 0;
                if (!arrayPtr) return missiles;

                for (int i = 0; i < size; i++) {
                    __try {
                        uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                        if (objAddr) {
                            missiles.push_back(new GameObject(objAddr));
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        Debug::Log("!!! GetMissiles loop EXCEPTION !!!");
                    }
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                Debug::Log("!!! GetMissiles EXCEPTION !!!");
            }
            return missiles;
        }

        // Cleanup helper if we user 'new' (Ideally should use smart pointers or just return objects)
        // For simplicity in this cheat context, we often leak or manage manually.
        // Better: Return std::vector<GameObject> (by value) since it's just a wrapper around a pointer.
    };
}
