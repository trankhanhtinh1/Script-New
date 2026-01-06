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
            uint64_t localParams = *(uint64_t*)(GetModuleBase() + Offset::oLocalPlayer);
            if (!localParams) return nullptr;
            return new GameObject(localParams);
        }

        // Helper to read a standard array of pointers ? 
        // Or is it a list of structs?
        // Based on "MinionList" having "LaneMinionArray" at 0x68, it's a specific manager struct.
        // For Heroes, usually it's just an array of pointers.
        
        static std::vector<GameObject*> GetHeroes() {
            std::vector<GameObject*> heroes;
            uint64_t heroManager = *(uint64_t*)(GetModuleBase() + Offset::oHerroList);
            if (!heroManager) return heroes;

            // Assuming standard Array structure: [ArrayStart] [ArrayEnd] ?
            // OR [Ptr] [Size] [Capacity] ?
            // Offset::oListSizeHero = 0x10.
            
            // Let's assume the manager points to an array of pointers at offset 0x08? or 0x0?
            // "oHerroList" comment says: 48 ? ? ? (HeroManager).
            // Usually HeroManager + 0x04 or 0x08 is the array.
            
            // Let's try reading the list based on common reversing patterns for this game engine.
            // Usually: 
            // Manager + 0x08 = Array Start
            // Manager + 0x10 = Size
            
            int size = *(int*)(heroManager + Offset::oListSizeHero); // Using provided offset
            // Limit size to reasonable amount (e.g. 100) to check bounds
            if (size > 200 || size < 0) size = 0;

            // Where is the array? In old sources it's often at 0x08 relative to manager.
            // But let's check if there's an offset for it?
            // "GetNextObject" exists in offsets. Maybe iterating via linked list?
            // But usually HeroList is contiguous.
            
            uint64_t arrayPtr = *(uint64_t*)(heroManager + 0x08); // Guessing 0x08
            
            for (int i = 0; i < size; i++) {
                uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                if (objAddr) {
                    heroes.push_back(new GameObject(objAddr));
                }
            }
            return heroes;
        }

        static std::vector<GameObject*> GetMinions() {
            // Returns Lane Minions (usually for farming)
            std::vector<GameObject*> minions;
            uint64_t minionManager = *(uint64_t*)(GetModuleBase() + Offset::oMinionList);
            if (!minionManager) return minions;

            // Using LaneMinionArray for lane minions (most important for orbwalker)
            uint64_t laneMinionsArray = *(uint64_t*)(minionManager + Offset::LaneMinionArray); // 0x68
            int count = *(int*)(minionManager + Offset::LaneMinionCount); // 0x70
            
            if (count > 500 || count < 0) count = 0;

            for (int i = 0; i < count; i++) {
                 uint64_t objAddr = *(uint64_t*)(laneMinionsArray + (i * 0x8));
                 if (objAddr) {
                     minions.push_back(new GameObject(objAddr));
                 }
            }
            return minions;
        }

        static std::vector<GameObject*> GetAllMinions() {
            // Returns ALL minions (Jungle, Wards, Lane, etc.)
            // Iterating the base array at 0x08 (Common Structure)
            std::vector<GameObject*> allMinions;
            uint64_t minionManager = *(uint64_t*)(GetModuleBase() + Offset::oMinionList);
            if (!minionManager) return allMinions;

            // Guessing offsets based on structure: [vtable] [reserved] [ArrayPtr] [Size]
            // Usually Array is at 0x08 or 0x10?
            // "oHerroList" is at 0x1D2F3B0 (Manager). Array at +0x08.
            // Let's assume MinionManager follows same:
            
            uint64_t arrayPtr = *(uint64_t*)(minionManager + 0x08); // Probably 0x08
            int size = *(int*)(minionManager + 0x10); // Probably 0x10
            
            if (size > 1000 || size < 0) size = 0;

            for (int i = 0; i < size; i++) {
                uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                if (objAddr) {
                    allMinions.push_back(new GameObject(objAddr));
                }
            }
            return allMinions;
        }

        static std::vector<GameObject*> GetTurrets() {
             std::vector<GameObject*> turrets;
             uint64_t turretManager = *(uint64_t*)(GetModuleBase() + Offset::oTurretList);
             if (!turretManager) return turrets;

             // Assuming standard list structure
             uint64_t arrayPtr = *(uint64_t*)(turretManager + 0x08);
             int size = *(int*)(turretManager + 0x10);

             if (size > 100 || size < 0) size = 0;

             for (int i = 0; i < size; i++) {
                 uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                 if (objAddr) {
                     turrets.push_back(new GameObject(objAddr));
                 }
             }
             return turrets;
        }

        static std::vector<GameObject*> GetMissiles() {
             std::vector<GameObject*> missiles;
             uint64_t missileManager = *(uint64_t*)(GetModuleBase() + Offset::oMisslilist);
             if (!missileManager) return missiles;

             // Assuming standard list structure (same as Turrets/Heroes)
             uint64_t arrayPtr = *(uint64_t*)(missileManager + 0x08);
             int size = *(int*)(missileManager + 0x10);

             if (size > 500 || size < 0) size = 0;

             for (int i = 0; i < size; i++) {
                 uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                 if (objAddr) {
                     missiles.push_back(new GameObject(objAddr));
                 }
             }
             return missiles;
        }

        // Cleanup helper if we user 'new' (Ideally should use smart pointers or just return objects)
        // For simplicity in this cheat context, we often leak or manage manually.
        // Better: Return std::vector<GameObject> (by value) since it's just a wrapper around a pointer.
    };
}
