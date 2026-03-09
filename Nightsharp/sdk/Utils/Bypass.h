#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "sdk/Utils/DebugConsole.h"
#include <Windows.h>
#include <Psapi.h>

#pragma comment(lib, "Psapi.lib")

namespace SDK::Bypass {

    template <typename Type>
    class EncryptedBuffer {
    private:
        BYTE* GetKeyArray() const noexcept {
            if constexpr (sizeof(Type) % 4 == 0) {
                return (BYTE*)((uintptr_t)this);
            }
            return (BYTE*)((uintptr_t)this + 0x3);
        }

        BYTE& GetIndex() const noexcept {
            if constexpr (sizeof(Type) % 4 == 0) {
                return *(BYTE*)((uintptr_t)this + 0x2B);
            }
            return *(BYTE*)((uintptr_t)this + 0x4);
        }

        Type* GetValuesArray() const noexcept {
            if constexpr (sizeof(Type) % 4 == 0) {
                return (Type*)((uintptr_t)this + 0x8);
            }
            return (Type*)((uintptr_t)this + 0x5);
        }

    public:
        __forceinline void ZeroCurrentValue() const noexcept {
            auto array = GetValuesArray();
            array[GetIndex()] = 0x0 ^ ~GetKeyArray()[0];
        }
    };

    inline uintptr_t ResolveDetectionWatcher2() {
        static uintptr_t cached = 0;
        static bool resolved = false;
        if (resolved) {
            return cached;
        }

        resolved = true;

        MODULEINFO modInfo{};
        HMODULE module = GetModuleHandleA(nullptr);
        if (!module || !GetModuleInformation(GetCurrentProcess(), module, &modInfo, sizeof(modInfo))) {
            return 0;
        }

        const auto* base = reinterpret_cast<const uint8_t*>(modInfo.lpBaseOfDll);
        const size_t size = modInfo.SizeOfImage;
        if (!base || size < 11) {
            return 0;
        }

        for (size_t i = 0; i + 11 < size; ++i) {
            if (base[i] != 0x4C || base[i + 1] != 0x8B || base[i + 2] != 0x3D) {
                continue;
            }
            if (base[i + 7] != 0x4D || base[i + 8] != 0x85 || base[i + 9] != 0xFF || base[i + 10] != 0x0F) {
                continue;
            }

            const auto rel = *reinterpret_cast<const int32_t*>(base + i + 3);
            const uintptr_t rip = reinterpret_cast<uintptr_t>(base + i + 7);
            const uintptr_t candidate = rip + rel;
            if (!Globals::IsValidPtr(candidate)) {
                continue;
            }

            cached = candidate;
            DEBUG_LOG_TAG("BYPASS", "DetectionWatcher2 resolved at RVA=0x%llX", (unsigned long long)(cached - Globals::base));
            return cached;
        }

        DEBUG_LOG_TAG("BYPASS", "DetectionWatcher2 signature not found");
        return 0;
    }

    inline bool MainloopCheck() {
        const uintptr_t detectionWatcher2 = ResolveDetectionWatcher2();
        if (!Globals::IsValidPtr(detectionWatcher2)) {
            return false;
        }

        const uintptr_t detectionInst = Globals::Read<uintptr_t>(detectionWatcher2);
        if (!Globals::IsValidPtr(detectionInst)) {
            return false;
        }

        auto* encryptedFlag = reinterpret_cast<EncryptedBuffer<uint8_t>*>(detectionInst + 0x8);
        __try {
            encryptedFlag->ZeroCurrentValue();
            return true;
        } __except (1) {
            return false;
        }
    }

    inline void PrepareIssueOrder(int order) {
        MainloopCheck();
        Globals::Write<int>(Globals::base + Offset::Flag::IssueOrderFlag, order + 17);
    }

    inline void PrepareCastSpell() {
        MainloopCheck();
        Globals::Write<uint8_t>(Globals::base + Offset::Flag::CastSpellFlag, 1);
    }

} // namespace SDK::Bypass
