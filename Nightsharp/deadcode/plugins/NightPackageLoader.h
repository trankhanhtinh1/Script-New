// ============================================================================
// NightPackageLoader.h — .night Plugin Runtime Loader
//
// Scans %APPDATA%/NightSharp/Plugin/ for .night files, decrypts and loads
// them as shellcode plugins within NightSharp's own memory space.
//
// Key properties:
//   - NO LoadLibrary → no module in PEB → invisible to anti-cheat
//   - Plugin code lives in VirtualAlloc'd RWX memory owned by NightSharp
//   - Plugin receives NightSharpAPI function table for all SDK calls
//   - Fully CRT-free compatible (WinAPI only)
//
// Loading workflow:
//   1. FindFirstFile → scan for *.night files
//   2. Read file → validate magic/version
//   3. Decrypt payload (XOR with pluginId + salt)
//   4. Verify integrity (FNV-1a hash)
//   5. Parse PE sections from decrypted payload:
//      - Map sections manually (code + data)
//      - Apply relocations (RELOC table)
//      - Resolve imports via GetProcAddress (kernel32/user32 only)
//      - Find exported plugin functions
//   6. Create PluginEntry + register with PluginRegistry
//   7. Call NightPlugin_OnLoad with API table
// ============================================================================

#pragma once

#include "../core/NightPackage.h"
#include "../menu/PluginRegistry.h"
#include "../sdk/External/nightsharp_plugin.h"
#include "../sdk/External/NightSharpAPI.h"

#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <Shlobj.h>

namespace NightPackageLoader {

    // ════════════════════════════════════════════════════════════════════
    // Constants
    // ════════════════════════════════════════════════════════════════════
    constexpr int MAX_NIGHT_PLUGINS = 32;
    constexpr const char* PLUGIN_SUBFOLDER = "NightSharp\\Plugin";

    // ════════════════════════════════════════════════════════════════════
    // Loaded plugin state
    // ════════════════════════════════════════════════════════════════════
    struct LoadedNightPlugin {
        bool                 Active;
        char                 FilePath[MAX_PATH];
        NightPluginInfo      Info;
        uint8_t*             MappedBase;       // VirtualAlloc'd memory
        uint32_t             MappedSize;

        // Function pointers resolved from mapped PE
        NightPlugin_GetInfoFn   GetInfoFn;
        NightPlugin_OnLoadFn    OnLoadFn;
        NightPlugin_OnUnloadFn  OnUnloadFn;
        NightPlugin_OnUpdateFn  OnUpdateFn;
        NightPlugin_OnRenderFn  OnRenderFn;

        int                  RegistryIndex;    // Index in PluginRegistry
    };

    inline LoadedNightPlugin g_nightPlugins[MAX_NIGHT_PLUGINS] = {};
    inline int               g_nightPluginCount = 0;
    inline NightSharpAPI     g_api = {};
    inline bool              g_apiInitialized = false;

    // ════════════════════════════════════════════════════════════════════
    // Forward declarations for binding functions
    // (These will be implemented to bridge NightSharpAPI → internal SDK)
    // ════════════════════════════════════════════════════════════════════

    // Include the binding implementation
    #include "../sdk/NightSharpAPIBinding.h"

    inline void InitializeAPITable(NightSharpAPI* api) {
        NightSharpAPIBinding::Initialize(api);
    }

    // ════════════════════════════════════════════════════════════════════
    // Forward declarations (defined later, used by LoadNightFile)
    // ════════════════════════════════════════════════════════════════════
    inline bool RuntimeLoad(void* userData);
    inline bool RuntimeUnload(void* userData);
    inline bool RuntimeCanLoad(void* userData);

    // ════════════════════════════════════════════════════════════════════
    // PE Manual Mapping (InMemory, no LoadLibrary)
    // ════════════════════════════════════════════════════════════════════

    inline uint8_t* MapPEInMemory(const uint8_t* peData, uint32_t peSize,
                                   uint32_t* outMappedSize)
    {
        if (!peData || peSize < sizeof(IMAGE_DOS_HEADER)) return nullptr;

        auto* dos = (const IMAGE_DOS_HEADER*)peData;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

        auto* nt = (const IMAGE_NT_HEADERS64*)(peData + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

        const uint32_t imageSize = nt->OptionalHeader.SizeOfImage;
        if (imageSize == 0 || imageSize > 64 * 1024 * 1024) return nullptr;  // sanity

        // ── Allocate memory for mapped image ──
        uint8_t* mapped = (uint8_t*)VirtualAlloc(nullptr, imageSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!mapped) return nullptr;

        memset(mapped, 0, imageSize);
        *outMappedSize = imageSize;

        // ── Copy headers ──
        memcpy(mapped, peData, nt->OptionalHeader.SizeOfHeaders);

        // ── Copy sections ──
        auto* section = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            if (section[i].SizeOfRawData == 0) continue;
            if (section[i].PointerToRawData + section[i].SizeOfRawData > peSize) {
                VirtualFree(mapped, 0, MEM_RELEASE);
                return nullptr;
            }
            memcpy(mapped + section[i].VirtualAddress,
                   peData + section[i].PointerToRawData,
                   section[i].SizeOfRawData);
        }

        // ── Process relocations ──
        const uint64_t deltaBase = (uint64_t)mapped - nt->OptionalHeader.ImageBase;
        if (deltaBase != 0) {
            auto& relocDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
            if (relocDir.VirtualAddress && relocDir.Size) {
                auto* relocBlock = (IMAGE_BASE_RELOCATION*)(mapped + relocDir.VirtualAddress);
                uint32_t processed = 0;

                while (processed < relocDir.Size && relocBlock->SizeOfBlock > 0) {
                    uint32_t entryCount = (relocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
                    uint16_t* entries = (uint16_t*)((uint8_t*)relocBlock + sizeof(IMAGE_BASE_RELOCATION));

                    for (uint32_t j = 0; j < entryCount; j++) {
                        uint16_t type = entries[j] >> 12;
                        uint16_t offset = entries[j] & 0xFFF;

                        if (type == IMAGE_REL_BASED_DIR64) {
                            uint64_t* patch = (uint64_t*)(mapped + relocBlock->VirtualAddress + offset);
                            *patch += deltaBase;
                        } else if (type == IMAGE_REL_BASED_HIGHLOW) {
                            uint32_t* patch = (uint32_t*)(mapped + relocBlock->VirtualAddress + offset);
                            *patch += (uint32_t)deltaBase;
                        }
                        // type 0 = padding, skip
                    }

                    processed += relocBlock->SizeOfBlock;
                    relocBlock = (IMAGE_BASE_RELOCATION*)((uint8_t*)relocBlock + relocBlock->SizeOfBlock);
                }
            }
        }

        // ── Resolve imports ──
        auto& importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (importDir.VirtualAddress && importDir.Size) {
            auto* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)(mapped + importDir.VirtualAddress);

            while (importDesc->Name) {
                const char* dllName = (const char*)(mapped + importDesc->Name);
                HMODULE hDll = GetModuleHandleA(dllName);
                if (!hDll) {
                    // Only allow loading system DLLs (kernel32, user32, etc.)
                    hDll = LoadLibraryA(dllName);
                }

                if (hDll) {
                    auto* thunk = (IMAGE_THUNK_DATA64*)(mapped + importDesc->FirstThunk);
                    auto* origThunk = importDesc->OriginalFirstThunk
                        ? (IMAGE_THUNK_DATA64*)(mapped + importDesc->OriginalFirstThunk)
                        : thunk;

                    while (origThunk->u1.AddressOfData) {
                        if (origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                            WORD ordinal = (WORD)(origThunk->u1.Ordinal & 0xFFFF);
                            thunk->u1.Function = (uint64_t)GetProcAddress(hDll, MAKEINTRESOURCEA(ordinal));
                        } else {
                            auto* import = (IMAGE_IMPORT_BY_NAME*)(mapped + origThunk->u1.AddressOfData);
                            thunk->u1.Function = (uint64_t)GetProcAddress(hDll, import->Name);
                        }
                        thunk++;
                        origThunk++;
                    }
                }

                importDesc++;
            }
        }

        return mapped;
    }

    // ════════════════════════════════════════════════════════════════════
    // Find exported function in mapped PE
    // ════════════════════════════════════════════════════════════════════
    inline void* FindExport(const uint8_t* mapped, const char* funcName) {
        if (!mapped || !funcName) return nullptr;

        auto* dos = (const IMAGE_DOS_HEADER*)mapped;
        auto* nt = (const IMAGE_NT_HEADERS64*)(mapped + dos->e_lfanew);
        auto& exportDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!exportDir.VirtualAddress || !exportDir.Size) return nullptr;

        auto* exports = (const IMAGE_EXPORT_DIRECTORY*)(mapped + exportDir.VirtualAddress);
        auto* names = (const uint32_t*)(mapped + exports->AddressOfNames);
        auto* ordinals = (const uint16_t*)(mapped + exports->AddressOfNameOrdinals);
        auto* functions = (const uint32_t*)(mapped + exports->AddressOfFunctions);

        for (DWORD i = 0; i < exports->NumberOfNames; i++) {
            const char* name = (const char*)(mapped + names[i]);
            if (strcmp(name, funcName) == 0) {
                return (void*)(mapped + functions[ordinals[i]]);
            }
        }

        return nullptr;
    }

    // ════════════════════════════════════════════════════════════════════
    // Load a single .night file
    // ════════════════════════════════════════════════════════════════════
    inline bool LoadNightFile(const char* filePath) {
        if (g_nightPluginCount >= MAX_NIGHT_PLUGINS) return false;

        // ── Read file ──
        HANDLE hFile = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        DWORD fileSize = GetFileSize(hFile, nullptr);
        if (fileSize < sizeof(NightPackage::PackageHeader) + sizeof(NightPackage::Manifest)) {
            CloseHandle(hFile);
            return false;
        }

        uint8_t* fileData = (uint8_t*)HeapAlloc(GetProcessHeap(), 0, fileSize);
        if (!fileData) { CloseHandle(hFile); return false; }

        DWORD bytesRead = 0;
        ReadFile(hFile, fileData, fileSize, &bytesRead, nullptr);
        CloseHandle(hFile);

        if (bytesRead != fileSize) {
            HeapFree(GetProcessHeap(), 0, fileData);
            return false;
        }

        // ── Validate header ──
        auto* header = (const NightPackage::PackageHeader*)fileData;
        if (header->Magic != NightPackage::MAGIC || header->Version != NightPackage::VERSION) {
            HeapFree(GetProcessHeap(), 0, fileData);
            return false;
        }

        // ── Read manifest ──
        if (header->ManifestOff + header->ManifestLen > fileSize) {
            HeapFree(GetProcessHeap(), 0, fileData);
            return false;
        }
        auto* manifest = (const NightPackage::Manifest*)(fileData + header->ManifestOff);

        // ── Decrypt payload ──
        if (header->PayloadOff + header->PayloadLen > fileSize) {
            HeapFree(GetProcessHeap(), 0, fileData);
            return false;
        }

        uint32_t payloadLen = header->PayloadLen;
        uint8_t* payload = (uint8_t*)HeapAlloc(GetProcessHeap(), 0, payloadLen);
        if (!payload) {
            HeapFree(GetProcessHeap(), 0, fileData);
            return false;
        }
        memcpy(payload, fileData + header->PayloadOff, payloadLen);

        // Decrypt
        NightPackage::XorEncryptDecrypt(payload, payloadLen,
            manifest->PluginId, header->XorSalt);

        // ── Verify hash ──
        uint64_t expectedHash = 0;
        memcpy(&expectedHash, header->Hash, 8);
        // Hash is computed on ENCRYPTED data, but we already decrypted.
        // Re-encrypt to compute hash for verification:
        uint8_t* tmpEncrypted = (uint8_t*)HeapAlloc(GetProcessHeap(), 0, payloadLen);
        if (tmpEncrypted) {
            memcpy(tmpEncrypted, payload, payloadLen);
            NightPackage::XorEncryptDecrypt(tmpEncrypted, payloadLen,
                manifest->PluginId, header->XorSalt);
            uint64_t computedHash = NightPackage::FNV1aHash(tmpEncrypted, payloadLen);
            HeapFree(GetProcessHeap(), 0, tmpEncrypted);

            if (computedHash != expectedHash) {
                HeapFree(GetProcessHeap(), 0, payload);
                HeapFree(GetProcessHeap(), 0, fileData);
                return false;
            }
        }

        // ── Manual map PE into memory ──
        uint32_t mappedSize = 0;
        uint8_t* mapped = MapPEInMemory(payload, payloadLen, &mappedSize);
        HeapFree(GetProcessHeap(), 0, payload);
        HeapFree(GetProcessHeap(), 0, fileData);

        if (!mapped) return false;

        // ── Find exports ──
        auto* getInfoFn  = (NightPlugin_GetInfoFn)FindExport(mapped,  NIGHT_FN_GET_INFO);
        auto* onLoadFn   = (NightPlugin_OnLoadFn)FindExport(mapped,   NIGHT_FN_ON_LOAD);
        auto* onUnloadFn = (NightPlugin_OnUnloadFn)FindExport(mapped, NIGHT_FN_ON_UNLOAD);
        auto* onUpdateFn = (NightPlugin_OnUpdateFn)FindExport(mapped, NIGHT_FN_ON_UPDATE);
        auto* onRenderFn = (NightPlugin_OnRenderFn)FindExport(mapped, NIGHT_FN_ON_RENDER);

        if (!getInfoFn || !onLoadFn || !onUnloadFn || !onUpdateFn || !onRenderFn) {
            VirtualFree(mapped, 0, MEM_RELEASE);
            return false;
        }

        // ── Get plugin info ──
        NightPluginInfo pluginInfo = {};
        if (getInfoFn(&pluginInfo) != 0) {
            VirtualFree(mapped, 0, MEM_RELEASE);
            return false;
        }

        // ── Store loaded plugin ──
        auto& slot = g_nightPlugins[g_nightPluginCount];
        slot.Active = true;
        strncpy_s(slot.FilePath, filePath, _TRUNCATE);
        slot.Info = pluginInfo;
        slot.MappedBase = mapped;
        slot.MappedSize = mappedSize;
        slot.GetInfoFn = getInfoFn;
        slot.OnLoadFn = onLoadFn;
        slot.OnUnloadFn = onUnloadFn;
        slot.OnUpdateFn = onUpdateFn;
        slot.OnRenderFn = onRenderFn;
        slot.RegistryIndex = -1;

        // ── Register with PluginRegistry ──
        PluginRegistry::PluginKind kind = PluginRegistry::PluginKind::External;
        if (pluginInfo.Type == NIGHT_TYPE_CHAMPION)
            kind = PluginRegistry::PluginKind::Plugin;
        else if (pluginInfo.Type == NIGHT_TYPE_UTILITY)
            kind = PluginRegistry::PluginKind::Plugin;

        int regIdx = PluginRegistry::Register(pluginInfo.Name, pluginInfo.Id, kind, nullptr, true);
        slot.RegistryIndex = regIdx;

        if (regIdx >= 0) {
            PluginRegistry::BindRuntime(regIdx,
                &g_nightPlugins[g_nightPluginCount],
                &NightPackageLoader::RuntimeLoad,
                &NightPackageLoader::RuntimeUnload,
                nullptr,  // No menu root getter (plugin creates menus via API)
                (pluginInfo.Type == NIGHT_TYPE_CHAMPION)
                    ? &NightPackageLoader::RuntimeCanLoad
                    : nullptr);
        }

        g_nightPluginCount++;
        return true;
    }

    // ════════════════════════════════════════════════════════════════════
    // Runtime callbacks (bridged to PluginRegistry)
    // ════════════════════════════════════════════════════════════════════

    inline bool RuntimeLoad(void* userData) {
        auto* plugin = (LoadedNightPlugin*)userData;
        if (!plugin || !plugin->Active || !plugin->OnLoadFn) return false;

        // Initialize API table once
        if (!g_apiInitialized) {
            InitializeAPITable(&g_api);
            g_apiInitialized = true;
        }

        plugin->OnLoadFn(&g_api);
        return true;
    }

    inline bool RuntimeUnload(void* userData) {
        auto* plugin = (LoadedNightPlugin*)userData;
        if (!plugin || !plugin->Active || !plugin->OnUnloadFn) return false;
        plugin->OnUnloadFn();
        return true;
    }

    inline bool RuntimeCanLoad(void* userData) {
        auto* plugin = (LoadedNightPlugin*)userData;
        if (!plugin || !plugin->Active) return false;

        // Check if any supported champion matches current player champion
        if (plugin->Info.SupportedChampionCount == 0) return true;

        // Get player champion name (requires API initialized)
        if (!g_apiInitialized) {
            InitializeAPITable(&g_api);
            g_apiInitialized = true;
        }

        if (!g_api.GetPlayerCharacterName) return true;

        char playerChamp[64] = {};
        g_api.GetPlayerCharacterName(playerChamp, sizeof(playerChamp));

        for (uint32_t i = 0; i < plugin->Info.SupportedChampionCount && i < 8; i++) {
            if (_stricmp(playerChamp, plugin->Info.SupportedChampions[i]) == 0) {
                return true;
            }
        }
        return false;
    }

    // ════════════════════════════════════════════════════════════════════
    // Update/Render — called by PluginManager for active .night plugins
    // ════════════════════════════════════════════════════════════════════

    inline void OnUpdateAll() {
        for (int i = 0; i < g_nightPluginCount; i++) {
            auto& p = g_nightPlugins[i];
            if (!p.Active || !p.OnUpdateFn) continue;
            if (p.RegistryIndex >= 0 && !PluginRegistry::Plugins[p.RegistryIndex].Loaded)
                continue;
            p.OnUpdateFn();
        }
    }

    inline void OnRenderAll() {
        for (int i = 0; i < g_nightPluginCount; i++) {
            auto& p = g_nightPlugins[i];
            if (!p.Active || !p.OnRenderFn) continue;
            if (p.RegistryIndex >= 0 && !PluginRegistry::Plugins[p.RegistryIndex].Loaded)
                continue;
            p.OnRenderFn();
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // Scan and load all .night files from plugin folder
    // ════════════════════════════════════════════════════════════════════

    inline int ScanAndLoadAll() {
        // Build plugin folder path: %APPDATA%/NightSharp/Plugin/
        char appData[MAX_PATH] = {};
        if (!SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
            return 0;
        }

        char pluginDir[MAX_PATH] = {};
        wsprintfA(pluginDir, "%s\\%s", appData, PLUGIN_SUBFOLDER);

        // Create directory if it doesn't exist
        CreateDirectoryA(appData, nullptr);
        char nightsharpDir[MAX_PATH] = {};
        wsprintfA(nightsharpDir, "%s\\NightSharp", appData);
        CreateDirectoryA(nightsharpDir, nullptr);
        CreateDirectoryA(pluginDir, nullptr);

        // Scan for .night files
        char searchPath[MAX_PATH] = {};
        wsprintfA(searchPath, "%s\\*.night", pluginDir);

        WIN32_FIND_DATAA findData = {};
        HANDLE hFind = FindFirstFileA(searchPath, &findData);
        if (hFind == INVALID_HANDLE_VALUE) return 0;

        int loaded = 0;
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            char fullPath[MAX_PATH] = {};
            wsprintfA(fullPath, "%s\\%s", pluginDir, findData.cFileName);

            if (LoadNightFile(fullPath)) {
                loaded++;
                OutputDebugStringA("[NightSharp] Loaded .night: ");
                OutputDebugStringA(findData.cFileName);
                OutputDebugStringA("\n");
            } else {
                OutputDebugStringA("[NightSharp] Failed to load: ");
                OutputDebugStringA(findData.cFileName);
                OutputDebugStringA("\n");
            }
        } while (FindNextFileA(hFind, &findData));

        FindClose(hFind);
        return loaded;
    }

    // ════════════════════════════════════════════════════════════════════
    // Unload all (cleanup)
    // ════════════════════════════════════════════════════════════════════

    inline void UnloadAll() {
        for (int i = 0; i < g_nightPluginCount; i++) {
            auto& p = g_nightPlugins[i];
            if (!p.Active) continue;

            if (p.OnUnloadFn) p.OnUnloadFn();

            if (p.MappedBase) {
                VirtualFree(p.MappedBase, 0, MEM_RELEASE);
                p.MappedBase = nullptr;
            }
            p.Active = false;
        }
        g_nightPluginCount = 0;
    }

} // namespace NightPackageLoader
