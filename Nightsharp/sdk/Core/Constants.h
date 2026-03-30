#pragma once

#include <Windows.h>

#include <string>

namespace SDK::Constants {

    inline const char* Patch() {
        return "26.6";
    }

    inline std::string BaseDirectory() {
        char buffer[MAX_PATH] = {};
        const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) {
            return {};
        }

        std::string path(buffer, buffer + len);
        const auto slash = path.find_last_of("\\/");
        return slash == std::string::npos ? std::string() : path.substr(0, slash);
    }

    inline std::string PublicLogDirectory() {
        return "C:\\Users\\Public";
    }

} // namespace SDK::Constants
