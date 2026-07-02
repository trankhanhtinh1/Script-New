#pragma once

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace SDK {

class Constants {
public:
    static const std::filesystem::path& EnsoulSharpAppData() {
        static const std::filesystem::path path = ResolveAppDataRoot() / ("LS" + UserNameHashHex());
        return path;
    }

    static const std::filesystem::path& LogDirectory() {
        static const std::filesystem::path path = EnsoulSharpAppData() / "Logs" / "SDK";
        return path;
    }

    static const std::string& LogFileName() {
        static const std::string name = ResolveLogFileName();
        return name;
    }

    static const char* EnsoulSharpFontName() {
        return "Calibri";
    }

    static int EnsoulSharpFontSize() {
        return 16;
    }

private:
    static std::filesystem::path ResolveAppDataRoot() {
        char appData[MAX_PATH] = {};
        const DWORD len = GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            return std::filesystem::path(appData);
        }
        return std::filesystem::path("C:\\Users\\Public");
    }

    static std::string ResolveUserName() {
        char name[256] = {};
        DWORD size = static_cast<DWORD>(sizeof(name));
        if (GetUserNameA(name, &size) && name[0]) {
            return std::string(name);
        }
        return "User";
    }

    static std::uint32_t DotNetStringHash(const std::string& value) {
        std::uint32_t hash1 = 5381u;
        std::uint32_t hash2 = hash1;

        for (std::size_t i = 0; i < value.size(); i += 2) {
            hash1 = ((hash1 << 5) + hash1) ^ static_cast<std::uint8_t>(value[i]);
            if (i + 1 >= value.size()) {
                break;
            }
            hash2 = ((hash2 << 5) + hash2) ^ static_cast<std::uint8_t>(value[i + 1]);
        }

        return hash1 + (hash2 * 1566083941u);
    }

    static std::string UserNameHashHex() {
        char buffer[16] = {};
        _snprintf_s(
            buffer,
            sizeof(buffer),
            _TRUNCATE,
            "%08X",
            static_cast<unsigned>(DotNetStringHash(ResolveUserName())));
        return buffer;
    }

    static std::string ResolveLogFileName() {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        char buffer[32] = {};
        _snprintf_s(
            buffer,
            sizeof(buffer),
            _TRUNCATE,
            "%u-%u-%04u.log",
            static_cast<unsigned>(st.wMonth),
            static_cast<unsigned>(st.wDay),
            static_cast<unsigned>(st.wYear));
        return buffer;
    }
};

} // namespace SDK
