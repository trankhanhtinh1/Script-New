#include <NightSharp.SDK.Package.h>
#include <NightSharp.SDK.Plugin.h>

#include <Windows.h>
#include <commdlg.h>

#include <cstdint>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string inputDll;
    std::string outputNs;
    std::string version = "1.0.0";
    std::string dependencies;
};

void PrintUsage() {
    std::cout
        << "NightSharp.Plugin.Packager\n"
        << "Usage:\n"
        << "  NightSharp.Plugin.Packager.exe <plugin.dll> [output.ns] [--version 1.0.0] [--deps dep1;dep2]\n";
}

std::string DefaultOutputPath(std::string inputDll) {
    const std::size_t slash = inputDll.find_last_of("\\/");
    const std::size_t dot = inputDll.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        inputDll.resize(dot);
    }
    inputDll += ".NS";
    return inputDll;
}

bool SelectInputDll(std::string& outPath) {
    char path[MAX_PATH] = {};

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter =
        "NightSharp plugin DLL (*.dll)\0*.dll\0"
        "All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.lpstrTitle = "Select NightSharp plugin DLL";
    ofn.Flags = OFN_FILEMUSTEXIST |
                OFN_PATHMUSTEXIST |
                OFN_NOCHANGEDIR |
                OFN_HIDEREADONLY;

    if (!GetOpenFileNameA(&ofn)) {
        return false;
    }

    outPath = path;
    return !outPath.empty();
}

bool IsOption(const char* arg) {
    return arg && arg[0] == '-' && arg[1] == '-';
}

bool StartsWith(const std::string& value, const char* prefix) {
    return value.rfind(prefix, 0) == 0;
}

bool ParseArgs(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            return false;
        }

        if (StartsWith(arg, "--out=")) {
            options.outputNs = arg.substr(6);
            continue;
        }
        if (arg == "--out" && i + 1 < argc) {
            options.outputNs = argv[++i];
            continue;
        }

        if (StartsWith(arg, "--version=")) {
            options.version = arg.substr(10);
            continue;
        }
        if (arg == "--version" && i + 1 < argc) {
            options.version = argv[++i];
            continue;
        }

        if (StartsWith(arg, "--deps=")) {
            options.dependencies = arg.substr(7);
            continue;
        }
        if (arg == "--deps" && i + 1 < argc) {
            options.dependencies = argv[++i];
            continue;
        }

        if (IsOption(arg.c_str())) {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }

        if (options.inputDll.empty()) {
            options.inputDll = arg;
        } else if (options.outputNs.empty()) {
            options.outputNs = arg;
        } else {
            std::cerr << "Unexpected positional argument: " << arg << "\n";
            return false;
        }
    }

    if (options.inputDll.empty()) {
        return false;
    }

    if (options.outputNs.empty()) {
        options.outputNs = DefaultOutputPath(options.inputDll);
    }

    return true;
}

bool ReadAllBytes(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open input: " << path << "\n";
        return false;
    }

    const std::streamoff size = file.tellg();
    if (size <= 0) {
        std::cerr << "Input file is empty: " << path << "\n";
        return false;
    }

    out.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(out.data()), size);
    if (!file) {
        std::cerr << "Failed to read input: " << path << "\n";
        return false;
    }
    return true;
}

bool WritePackage(const std::string& path,
                  const NightSharp::Package::Header& header,
                  const std::vector<std::uint8_t>& payload) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Failed to open output: " << path << "\n";
        return false;
    }

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(payload.data()),
               static_cast<std::streamsize>(payload.size()));
    if (!file) {
        std::cerr << "Failed to write output: " << path << "\n";
        return false;
    }
    return true;
}

void FillNonce(NightSharp::Package::Header& header) {
    LARGE_INTEGER qpc = {};
    QueryPerformanceCounter(&qpc);

    const std::uint64_t seeds[2] = {
        NightSharp::Package::Mix64(static_cast<std::uint64_t>(std::time(nullptr)) ^
                                   static_cast<std::uint64_t>(GetCurrentProcessId()) ^
                                   static_cast<std::uint64_t>(qpc.QuadPart)),
        NightSharp::Package::Mix64(header.PlainHash ^
                                   static_cast<std::uint64_t>(GetTickCount64()) ^
                                   header.PayloadSize)
    };

    for (int i = 0; i < 2; ++i) {
        std::uint64_t value = seeds[i];
        for (int b = 0; b < 8; ++b) {
            header.Nonce[i * 8 + b] = static_cast<std::uint8_t>(value & 0xFFu);
            value >>= 8u;
        }
    }
}

std::string CategoryName(NightSharp::Plugin::Category category) {
    switch (category) {
    case NightSharp::Plugin::Category::Core: return "Core";
    case NightSharp::Plugin::Category::Champion: return "Champion";
    case NightSharp::Plugin::Category::Utility: return "Utility";
    case NightSharp::Plugin::Category::Misc: return "Misc";
    default: return "Unknown";
    }
}

bool ValidateExports(const NightSharp::Plugin::Exports* exports) {
    if (!exports) {
        std::cerr << "NightSharpGetPluginExports returned null\n";
        return false;
    }
    if (exports->Size < sizeof(NightSharp::Plugin::Exports)) {
        std::cerr << "Exports size mismatch\n";
        return false;
    }
    if (exports->Descriptor.Size < sizeof(NightSharp::Plugin::Descriptor)) {
        std::cerr << "Descriptor size mismatch\n";
        return false;
    }
    if (exports->Descriptor.AbiRevision != NIGHTSHARP_SDK_ABI_REVISION) {
        std::cerr << "ABI revision mismatch: plugin="
                  << exports->Descriptor.AbiRevision
                  << " expected=" << NIGHTSHARP_SDK_ABI_REVISION << "\n";
        return false;
    }
    if (!exports->Descriptor.SdkAbiId ||
        std::string(exports->Descriptor.SdkAbiId) != NIGHTSHARP_SDK_ABI_ID) {
        std::cerr << "SDK ABI id mismatch: plugin="
                  << (exports->Descriptor.SdkAbiId ? exports->Descriptor.SdkAbiId : "<null>")
                  << " expected=" << NIGHTSHARP_SDK_ABI_ID << "\n";
        return false;
    }
    if (!exports->Descriptor.Name || !exports->Descriptor.Name[0]) {
        std::cerr << "Plugin descriptor missing name\n";
        return false;
    }
    if (!exports->Descriptor.InternalId || !exports->Descriptor.InternalId[0]) {
        std::cerr << "Plugin descriptor missing internal id\n";
        return false;
    }
    return true;
}

bool LoadDescriptor(const std::string& path, NightSharp::Plugin::Exports const*& exports, HMODULE& module) {
    module = LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) {
        std::cerr << "LoadLibraryExA failed, gle=" << GetLastError() << ": " << path << "\n";
        return false;
    }

    auto* proc = reinterpret_cast<NightSharp::Plugin::GetExportsFn>(
        GetProcAddress(module, "NightSharpGetPluginExports"));
    if (!proc) {
        std::cerr << "Missing export NightSharpGetPluginExports\n";
        FreeLibrary(module);
        module = nullptr;
        return false;
    }

    __try {
        exports = proc();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cerr << "NightSharpGetPluginExports crashed\n";
        FreeLibrary(module);
        module = nullptr;
        return false;
    }

    if (!ValidateExports(exports)) {
        FreeLibrary(module);
        module = nullptr;
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    const bool guiMode = argc <= 1;

    if (guiMode) {
        if (!SelectInputDll(options.inputDll)) {
            return 0;
        }
        options.outputNs = DefaultOutputPath(options.inputDll);
    } else if (!ParseArgs(argc, argv, options)) {
        PrintUsage();
        return 2;
    }

    auto fail = [&](const std::string& message, int code) {
        if (guiMode) {
            MessageBoxA(nullptr,
                        message.c_str(),
                        "NightSharp Plugin Packager",
                        MB_OK | MB_ICONERROR);
        }
        return code;
    };

    std::vector<std::uint8_t> plain;
    if (!ReadAllBytes(options.inputDll, plain)) {
        return fail("Failed to read input DLL:\n" + options.inputDll, 1);
    }

    HMODULE module = nullptr;
    const NightSharp::Plugin::Exports* exports = nullptr;
    if (!LoadDescriptor(options.inputDll, exports, module)) {
        if (module) {
            FreeLibrary(module);
        }
        return fail("Failed to load or validate NightSharp plugin DLL:\n" + options.inputDll, 1);
    }

    NightSharp::Package::Header header = {};
    header.Magic = NightSharp::Package::kMagic;
    header.HeaderSize = sizeof(NightSharp::Package::Header);
    header.FormatVersion = NightSharp::Package::kFormatVersion;
    header.Flags = NightSharp::Package::kFlagEncrypted;
    header.AbiRevision = exports->Descriptor.AbiRevision;
    header.Category = static_cast<std::uint32_t>(exports->Descriptor.PluginCategory);
    header.AutoLoad = exports->Descriptor.AutoLoad ? 1u : 0u;
    header.OriginalSize = plain.size();
    header.PayloadSize = plain.size();
    header.PlainHash = NightSharp::Package::Fnv1a64(plain.data(), plain.size());
    header.CreatedUnix = static_cast<std::uint64_t>(std::time(nullptr));

    NightSharp::Package::CopyFixed(header.SdkAbiId, sizeof(header.SdkAbiId), exports->Descriptor.SdkAbiId);
    NightSharp::Package::CopyFixed(header.Name, sizeof(header.Name), exports->Descriptor.Name);
    NightSharp::Package::CopyFixed(header.InternalId, sizeof(header.InternalId), exports->Descriptor.InternalId);
    NightSharp::Package::CopyFixed(header.Author, sizeof(header.Author), exports->Descriptor.Author);
    NightSharp::Package::CopyFixed(header.ChampionName, sizeof(header.ChampionName), exports->Descriptor.ChampionName);
    NightSharp::Package::CopyFixed(header.PluginVersion, sizeof(header.PluginVersion), options.version.c_str());
    NightSharp::Package::CopyFixed(header.Dependencies, sizeof(header.Dependencies), options.dependencies.c_str());
    FillNonce(header);

    std::vector<std::uint8_t> payload = plain;
    NightSharp::Package::XorCrypt(header, payload.data(), payload.size());
    header.CipherHash = NightSharp::Package::Fnv1a64(payload.data(), payload.size());
    NightSharp::Package::MakeSignature(header,
                                       payload.data(),
                                       payload.size(),
                                       header.Signature0,
                                       header.Signature1);

    const std::string category = CategoryName(exports->Descriptor.PluginCategory);
    const bool ok = WritePackage(options.outputNs, header, payload);
    FreeLibrary(module);

    if (!ok) {
        return fail("Failed to write .NS package:\n" + options.outputNs, 1);
    }

    if (guiMode) {
        std::string message =
            "Packed .NS plugin successfully.\n\n"
            "Input:\n" + options.inputDll +
            "\n\nOutput:\n" + options.outputNs +
            "\n\nName: " + header.Name +
            "\nID: " + header.InternalId +
            "\nCategory: " + category +
            "\nABI: " + header.SdkAbiId;
        MessageBoxA(nullptr,
                    message.c_str(),
                    "NightSharp Plugin Packager",
                    MB_OK | MB_ICONINFORMATION);
    } else {
        std::cout << "Packed .NS plugin\n"
                  << "  input:    " << options.inputDll << "\n"
                  << "  output:   " << options.outputNs << "\n"
                  << "  name:     " << header.Name << "\n"
                  << "  id:       " << header.InternalId << "\n"
                  << "  category: " << category << "\n"
                  << "  abi:      " << header.SdkAbiId << "\n"
                  << "  bytes:    " << payload.size() << "\n";
    }

    return 0;
}
