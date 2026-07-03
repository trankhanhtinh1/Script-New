#pragma once

#include "Core/Game.h"
#include "UI/UI.h"

#include <Windows.h>

#include <cstdint>
#include <string>
#include <vector>

#pragma comment(lib, "version.lib")

namespace SDK {
class TargetSelector;
class Orbwalker;
} // namespace SDK

namespace SDK::Variables {

struct Version {
    int Major = 0;
    int Minor = 0;
    int Build = -1;
    int Revision = -1;

    Version() = default;
    Version(int major, int minor, int build = -1, int revision = -1)
        : Major(major), Minor(minor), Build(build), Revision(revision) {}

    std::string ToString() const {
        std::string value = std::to_string(Major) + "." + std::to_string(Minor);
        if (Build >= 0) {
            value += "." + std::to_string(Build);
        }
        if (Revision >= 0) {
            value += "." + std::to_string(Revision);
        }
        return value;
    }
};

namespace Detail {
    inline HMODULE CurrentModule() {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(&CurrentModule), &mbi, sizeof(mbi)) == 0) {
            return nullptr;
        }
        return reinterpret_cast<HMODULE>(mbi.AllocationBase);
    }

    inline Version ResolveKitVersion() {
        HMODULE module = CurrentModule();
        char path[MAX_PATH] = {};
        if (!module || GetModuleFileNameA(module, path, MAX_PATH) == 0) {
            return Version(1, 0, 0, 0);
        }

        DWORD handle = 0;
        const DWORD size = GetFileVersionInfoSizeA(path, &handle);
        if (size == 0) {
            return Version(1, 0, 0, 0);
        }

        std::vector<std::uint8_t> data(size);
        if (!GetFileVersionInfoA(path, 0, size, data.data())) {
            return Version(1, 0, 0, 0);
        }

        VS_FIXEDFILEINFO* info = nullptr;
        UINT infoSize = 0;
        if (!VerQueryValueA(data.data(), "\\", reinterpret_cast<void**>(&info), &infoSize) ||
            !info ||
            infoSize < sizeof(VS_FIXEDFILEINFO)) {
            return Version(1, 0, 0, 0);
        }

        return Version(
            HIWORD(info->dwFileVersionMS),
            LOWORD(info->dwFileVersionMS),
            HIWORD(info->dwFileVersionLS),
            LOWORD(info->dwFileVersionLS));
    }
} // namespace Detail

inline const Version GameVersion{ 9, 7 };
inline const Version KitVersion = Detail::ResolveKitVersion();

inline ::SDK::Menu* EnsoulSharpMenu = nullptr;
inline ::SDK::TargetSelector* TargetSelector = nullptr;
inline ::SDK::Orbwalker* Orbwalker = nullptr;

inline int TickCount() {
    return SDK::Game::TickCount();
}

} // namespace SDK::Variables
