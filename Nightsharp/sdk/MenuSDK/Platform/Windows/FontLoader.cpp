#include "FontLoader.h"

#include <Windows.h>

#include <array>
#include <climits>
#include <cstddef>
#include <string>
#include <utility>

namespace NightSharp::Menu {
namespace {

std::string WindowsFontsPath(const char* filename) {
    char windowsDirectory[MAX_PATH] = {};
    const UINT length = GetWindowsDirectoryA(
        windowsDirectory,
        static_cast<UINT>(sizeof(windowsDirectory)));
    if (length == 0 || length >= sizeof(windowsDirectory)) {
        return {};
    }

    std::string path(windowsDirectory);
    path += "\\Fonts\\";
    path += filename;
    return path;
}

void* LoadFontData(const char* path, int& size) {
    size = 0;
    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return nullptr;
    }

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(file, &fileSize) ||
        fileSize.QuadPart <= 0 ||
        fileSize.QuadPart > INT_MAX) {
        CloseHandle(file);
        return nullptr;
    }

    size = static_cast<int>(fileSize.QuadPart);
    void* data = ImGui::MemAlloc(static_cast<std::size_t>(size));
    if (!data) {
        CloseHandle(file);
        size = 0;
        return nullptr;
    }

    DWORD bytesRead = 0;
    const BOOL readOk = ReadFile(
        file,
        data,
        static_cast<DWORD>(size),
        &bytesRead,
        nullptr);
    CloseHandle(file);
    if (!readOk || bytesRead != static_cast<DWORD>(size)) {
        ImGui::MemFree(data);
        size = 0;
        return nullptr;
    }
    return data;
}

}

FontLoadResult LoadWindowsFont(ImFontAtlas* atlas, float pixelSize) {
    FontLoadResult result{};
    if (!atlas) {
        return result;
    }

    const std::array<std::pair<const char*, const char*>, 3> candidates = {{
        { "segoeui.ttf", "Segoe UI" },
        { "tahoma.ttf", "Tahoma" },
        { "arial.ttf", "Arial" },
    }};
    for (const auto& candidate : candidates) {
        const std::string path = WindowsFontsPath(candidate.first);
        if (path.empty() ||
            GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            continue;
        }

        int fontSize = 0;
        void* fontData = LoadFontData(path.c_str(), fontSize);
        if (!fontData) {
            continue;
        }

        ImFontConfig config{};
        config.OversampleH = 2;
        config.OversampleV = 1;
        config.FontDataOwnedByAtlas = true;
        ImFont* font = atlas->AddFontFromMemoryTTF(
            fontData,
            fontSize,
            pixelSize,
            &config,
            atlas->GetGlyphRangesVietnamese());
        if (font) {
            result.font = font;
            result.family = candidate.second;
            result.fromWindows = true;
            return result;
        }
        ImGui::MemFree(fontData);
    }

    result.font = atlas->AddFontDefault();
    result.family = "ImGui Default";
    return result;
}

}
