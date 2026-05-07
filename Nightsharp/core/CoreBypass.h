#pragma once

#include "CoreRuntime.h"
#include "spoof/spoofcall.h"

#include <Windows.h>
#include <cstdint>

namespace CoreBypass {

    template <typename Type>
    class EncryptedBuffer {
    private:
        BYTE* GetKeyArray() const noexcept {
            if constexpr ((sizeof(Type) % 4) == 0) {
                return (BYTE*)((uintptr_t)this);
            }
            return (BYTE*)((uintptr_t)this + 0x3);
        }

        BYTE GetIndex() const noexcept {
            if constexpr ((sizeof(Type) % 4) == 0) {
                return *(BYTE*)((uintptr_t)this + 0x2B);
            }
            return *(BYTE*)((uintptr_t)this + 0x4);
        }

        Type* GetValuesArray() const noexcept {
            if constexpr ((sizeof(Type) % 4) == 0) {
                return (Type*)((uintptr_t)this + 0x8);
            }
            return (Type*)((uintptr_t)this + 0x5);
        }

    public:
        __forceinline void ZeroCurrentValue() const noexcept {
            auto* values = GetValuesArray();
            values[GetIndex()] = 0x0 ^ ~GetKeyArray()[0];
        }
    };

    inline bool ForEachExecutableSection(bool (*callback)(const uint8_t* start, size_t size, uintptr_t sectionBase, void* user), void* user) {
        const auto base = CoreRuntime::GetContext().moduleBase;
        if (!base || !callback) {
            return false;
        }

        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            return false;
        }

        auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) {
            return false;
        }

        auto* sec = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
            if ((sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) {
                continue;
            }

            const auto sectionBase = base + sec->VirtualAddress;
            const auto sectionSize = static_cast<size_t>(sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData);
            if (!sectionBase || sectionSize < 2) {
                continue;
            }

            if (callback(reinterpret_cast<const uint8_t*>(sectionBase), sectionSize, sectionBase, user)) {
                return true;
            }
        }

        return false;
    }

    struct PatternSearchCtx {
        uintptr_t result = 0;
    };

    inline bool DetectionWatcherPatternCallback(const uint8_t* start, size_t size, uintptr_t sectionBase, void* user) {
        auto* ctx = reinterpret_cast<PatternSearchCtx*>(user);
        if (!ctx) {
            return false;
        }

        for (size_t i = 0; i + 11 < size; ++i) {
            if (start[i] != 0x4C || start[i + 1] != 0x8B || start[i + 2] != 0x3D) {
                continue;
            }
            if (start[i + 7] != 0x4D || start[i + 8] != 0x85 || start[i + 9] != 0xFF || start[i + 10] != 0x0F) {
                continue;
            }

            const auto rel = *reinterpret_cast<const int32_t*>(start + i + 3);
            const auto rip = static_cast<uintptr_t>(sectionBase + i + 7);
            const auto candidate = static_cast<uintptr_t>(static_cast<int64_t>(rip) + rel);
            if (!Globals::IsValidPtr(candidate)) {
                continue;
            }

            ctx->result = candidate;
            return true;
        }

        return false;
    }

    inline bool SpoofTrampolinePatternCallback(const uint8_t* start, size_t size, uintptr_t sectionBase, void* user) {
        auto* ctx = reinterpret_cast<PatternSearchCtx*>(user);
        if (!ctx) {
            return false;
        }

        for (size_t i = 0; i + 1 < size; ++i) {
            if (start[i] == 0xFF && start[i + 1] == 0x23) {
                ctx->result = sectionBase + i;
                return true;
            }
        }

        return false;
    }

    inline uintptr_t ResolveDetectionWatcher2() {
        if (CoreRuntime::GetContext().detectionWatcher2) {
            return CoreRuntime::GetContext().detectionWatcher2;
        }

        PatternSearchCtx search = {};
        if (ForEachExecutableSection(&DetectionWatcherPatternCallback, &search)) {
            CoreRuntime::g_ctx.detectionWatcher2 = search.result;
        }
        return CoreRuntime::GetContext().detectionWatcher2;
    }

    inline uintptr_t ResolveSpoofTrampoline() {
        if (CoreRuntime::GetContext().spoofTrampoline) {
            return CoreRuntime::GetContext().spoofTrampoline;
        }

        PatternSearchCtx search = {};
        if (ForEachExecutableSection(&SpoofTrampolinePatternCallback, &search)) {
            CoreRuntime::g_ctx.spoofTrampoline = search.result;
        }
        return CoreRuntime::GetContext().spoofTrampoline;
    }

    inline bool MainloopCheck() {
        const auto watcher2 = ResolveDetectionWatcher2();
        if (!Globals::IsValidPtr(watcher2)) {
            return false;
        }

        const auto detectionInst = Globals::Read<uintptr_t>(watcher2);
        if (!Globals::IsValidPtr(detectionInst)) {
            return false;
        }

        auto* encryptedFlag = reinterpret_cast<EncryptedBuffer<uint8_t>*>(detectionInst + 0x8);
        __try {
            encryptedFlag->ZeroCurrentValue();
            return true;
        }
        __except (1) {
            return false;
        }
    }

    // Offset::Flag::IssueOrderFlag → Offset::ControlRuntime::IssueOrderFlag
    inline bool PrepareIssueOrder(uint8_t order) {
        const bool ok = MainloopCheck();
        Globals::Write<int>(CoreRuntime::GetContext().moduleBase + Offset::ControlRuntime::IssueOrderFlag, static_cast<int>(order) + 17);
        return ok;
    }

    inline bool FinalizeIssueOrder(uint8_t order) {
        const bool ok = MainloopCheck();
        Globals::Write<int>(CoreRuntime::GetContext().moduleBase + Offset::ControlRuntime::IssueOrderFlag, static_cast<int>(order) + 17);
        return ok;
    }

    // Offset::Flag::CastSpellFlag → Offset::ControlRuntime::CastSpellFlag
    inline bool PrepareCastSpell() {
        const bool ok = MainloopCheck();
        Globals::Write<uint8_t>(CoreRuntime::GetContext().moduleBase + Offset::ControlRuntime::CastSpellFlag, 1);
        return ok;
    }

    inline void ClearCastSpellFlag() {
        Globals::Write<uint8_t>(CoreRuntime::GetContext().moduleBase + Offset::ControlRuntime::CastSpellFlag, 0);
    }

} // namespace CoreBypass
