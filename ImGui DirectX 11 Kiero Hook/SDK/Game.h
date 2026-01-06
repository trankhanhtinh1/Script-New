#pragma once
#include "Offsets.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cmath>

namespace SDK
{
    class Game
    {
    public:
        static float GetTime() {
             return *(float*)((uint64_t)GetModuleHandle(NULL) + Offset::oGametime);
        }

        static bool IsFocused() {
             // Use FindWindow or internal flag
             // return GetForegroundWindow() == ...
             return true;
        }

        static Vector3 GetMousePos() {
             uint64_t base = (uint64_t)GetModuleHandle(NULL);
             uint64_t hudInstance = *(uint64_t*)(base + Offset::oHudInstance);
             if (!hudInstance) return Vector3(0,0,0);

             uint64_t input = *(uint64_t*)(hudInstance + Offset::oHudInstanceInput);
             if (!input) return Vector3(0,0,0);

             // Read mouse world position from hudInputInstance + oHudMouseVec3
             Vector3 mousePos;
             mousePos.x = *(float*)(input + Offset::oHudMouseVec3);
             mousePos.y = *(float*)(input + Offset::oHudMouseVec3 + 0x4);
             mousePos.z = *(float*)(input + Offset::oHudMouseVec3 + 0x8);
             
             // Validate: Check if position is reasonable (not garbage/NaN/infinity)
             // League map is roughly -500 to 15000 on X/Z
             if (mousePos.x < -1000.0f || mousePos.x > 20000.0f ||
                 mousePos.z < -1000.0f || mousePos.z > 20000.0f ||
                 mousePos.y < -500.0f || mousePos.y > 500.0f ||
                 isnan(mousePos.x) || isnan(mousePos.y) || isnan(mousePos.z)) {
                 return Vector3(0,0,0); // Invalid position
             }
             
             return mousePos;
        }
    };
}
