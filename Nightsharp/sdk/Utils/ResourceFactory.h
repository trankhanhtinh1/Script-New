#pragma once

#include <Windows.h>

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace SDK::Core::Utils {

class ResourceFactory {
public:
    static void RegisterResource(const std::string& name, const std::vector<std::uint8_t>& bytes) {
        Resources()[name] = bytes;
    }

    static void RegisterResource(const std::string& name, const std::string& text) {
        Resources()[name] = std::vector<std::uint8_t>(text.begin(), text.end());
    }

    static void RegisterResource(const std::string& name, const void* data, std::size_t size) {
        if (!data || size == 0) {
            Resources()[name] = {};
            return;
        }
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        Resources()[name] = std::vector<std::uint8_t>(bytes, bytes + size);
    }

    static std::string StringResource(const std::string& file) {
        const auto bytes = ByteResource(file);
        return std::string(bytes.begin(), bytes.end());
    }

    static std::vector<std::uint8_t> ByteResource(const std::string& file) {
        if (file.empty()) {
            throw std::invalid_argument("file");
        }

        for (const auto& pair : Resources()) {
            if (EndsWith(pair.first, file)) {
                return pair.second;
            }
        }

        std::vector<std::uint8_t> win32Resource;
        if (TryLoadWin32Resource(file, win32Resource)) {
            return win32Resource;
        }

        if (std::filesystem::exists(file)) {
            std::ifstream stream(file, std::ios::binary);
            return std::vector<std::uint8_t>(
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
        }

        throw std::runtime_error("resourceFile Embedded Resource not found");
    }

private:
    static std::unordered_map<std::string, std::vector<std::uint8_t>>& Resources() {
        static std::unordered_map<std::string, std::vector<std::uint8_t>> resources;
        return resources;
    }

    struct EnumContext {
        HMODULE Module = nullptr;
        const std::string* Suffix = nullptr;
        std::vector<std::uint8_t>* Bytes = nullptr;
        bool Found = false;
    };

    static HMODULE CurrentModule() {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(&CurrentModule), &mbi, sizeof(mbi)) == 0) {
            return nullptr;
        }
        return reinterpret_cast<HMODULE>(mbi.AllocationBase);
    }

    static bool EndsWith(const std::string& value, const std::string& suffix) {
        return value.size() >= suffix.size() &&
               value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    static std::string ResourceNameToString(LPCSTR name) {
        if (IS_INTRESOURCE(name)) {
            return "#" + std::to_string(reinterpret_cast<ULONG_PTR>(name));
        }
        return name ? std::string(name) : std::string();
    }

    static bool CopyResourceBytes(HMODULE module,
                                  LPCSTR resourceName,
                                  LPCSTR resourceType,
                                  std::vector<std::uint8_t>& bytes) {
        HRSRC resource = FindResourceA(module, resourceName, resourceType);
        if (!resource) {
            return false;
        }

        const DWORD size = SizeofResource(module, resource);
        HGLOBAL loaded = LoadResource(module, resource);
        const void* data = loaded ? LockResource(loaded) : nullptr;
        if (!data || size == 0) {
            return false;
        }

        const auto* first = static_cast<const std::uint8_t*>(data);
        bytes.assign(first, first + size);
        return true;
    }

    static BOOL CALLBACK EnumResourceNameProc(HMODULE module, LPCSTR type, LPSTR name, LONG_PTR param) {
        auto* ctx = reinterpret_cast<EnumContext*>(param);
        if (!ctx || ctx->Found || !ctx->Suffix || !ctx->Bytes) {
            return TRUE;
        }

        const std::string currentName = ResourceNameToString(name);
        if (EndsWith(currentName, *ctx->Suffix) &&
            CopyResourceBytes(module, name, type, *ctx->Bytes)) {
            ctx->Found = true;
            return FALSE;
        }
        return TRUE;
    }

    static bool TryLoadWin32Resource(const std::string& file, std::vector<std::uint8_t>& bytes) {
        HMODULE module = CurrentModule();
        if (!module) {
            return false;
        }

        if (CopyResourceBytes(module, file.c_str(), RT_RCDATA, bytes)) {
            return true;
        }

        EnumContext context{ module, &file, &bytes, false };
        EnumResourceNamesA(module, RT_RCDATA, &EnumResourceNameProc, reinterpret_cast<LONG_PTR>(&context));
        return context.Found;
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using ResourceFactory = ::SDK::Core::Utils::ResourceFactory;
} // namespace SDK::Utils
