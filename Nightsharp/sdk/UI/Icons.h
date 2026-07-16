#pragma once

#include "../../imgui/imgui.h"
#include "../Utils/Logging.h"

#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

namespace SDK::UI::Icons {

struct LoadedTexture {
    ImTextureID Texture = nullptr;
    int Width = 0;
    int Height = 0;
};

struct ImagePixels {
    std::vector<std::uint8_t> Rgba;
    int Width = 0;
    int Height = 0;

    bool IsValid() const {
        return Width > 0 && Height > 0 &&
               Rgba.size() == static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height) * 4u;
    }
};

namespace detail {
    using Microsoft::WRL::ComPtr;
    using IconCache = std::unordered_map<std::string, LoadedTexture>;

    inline ID3D11Device*& Device() { static ID3D11Device* device = nullptr; return device; }
    inline ID3D11DeviceContext*& Context() { static ID3D11DeviceContext* context = nullptr; return context; }
    inline IconCache*& CachePtr() {
        static IconCache* cache = nullptr;
        return cache;
    }
    inline IconCache& Cache() {
        auto*& cache = CachePtr();
        if (!cache) {
            cache = new (std::nothrow) IconCache();
        }
        return *cache;
    }
    inline LoadedTexture& Placeholder() { static LoadedTexture texture; return texture; }
    inline bool& Initialized() { static bool initialized = false; return initialized; }
    inline ComPtr<IWICImagingFactory>& WicFactory() {
        static ComPtr<IWICImagingFactory> factory;
        return factory;
    }

    inline std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    inline std::string FilenameKey(const char* path) {
        std::string value = path ? path : "";
        const auto slash = value.find_last_of("/\\");
        if (slash != std::string::npos) {
            value = value.substr(slash + 1);
        }
        const auto dot = value.find_last_of('.');
        if (dot != std::string::npos) {
            value = value.substr(0, dot);
        }
        return ToLower(value);
    }

    inline std::wstring ToWide(const char* text) {
        if (!text || !text[0]) {
            return {};
        }

        int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        UINT codePage = CP_UTF8;
        if (needed <= 0) {
            needed = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
            codePage = CP_ACP;
        }
        if (needed <= 0) {
            return {};
        }

        std::wstring result(static_cast<std::size_t>(needed), L'\0');
        MultiByteToWideChar(codePage, 0, text, -1, result.data(), needed);
        if (!result.empty() && result.back() == L'\0') {
            result.pop_back();
        }
        return result;
    }

    inline IWICImagingFactory* EnsureWicFactory() {
        auto& factory = WicFactory();
        if (factory) {
            return factory.Get();
        }

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            SDK::Utils::Logging::Write()(LogLevel::Warn, "Icons: CoInitializeEx failed 0x%08X", static_cast<unsigned>(hr));
        }

        hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(factory.GetAddressOf()));
        if (FAILED(hr) || !factory) {
            SDK::Utils::Logging::Write()(LogLevel::Error, "Icons: WIC factory failed 0x%08X", static_cast<unsigned>(hr));
            return nullptr;
        }
        return factory.Get();
    }

    inline LoadedTexture UploadRGBA(const std::uint8_t* data, int width, int height) {
        LoadedTexture result{};
        if (!Device() || !data || width <= 0 || height <= 0) {
            return result;
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = data;
        initData.SysMemPitch = static_cast<UINT>(width * 4);

        ID3D11Texture2D* texture = nullptr;
        HRESULT hr = Device()->CreateTexture2D(&desc, &initData, &texture);
        if (FAILED(hr) || !texture) {
            return result;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        ID3D11ShaderResourceView* srv = nullptr;
        hr = Device()->CreateShaderResourceView(texture, &srvDesc, &srv);
        texture->Release();
        if (FAILED(hr) || !srv) {
            return result;
        }

        result.Texture = reinterpret_cast<ImTextureID>(srv);
        result.Width = width;
        result.Height = height;
        return result;
    }

    inline bool DecodeFrameToPixels(IWICBitmapFrameDecode* frame, ImagePixels& out) {
        out = {};
        if (!frame) {
            return false;
        }

        IWICImagingFactory* factory = EnsureWicFactory();
        if (!factory) {
            return false;
        }

        UINT width = 0;
        UINT height = 0;
        if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0) {
            return false;
        }

        ComPtr<IWICFormatConverter> converter;
        HRESULT hr = factory->CreateFormatConverter(converter.GetAddressOf());
        if (FAILED(hr) || !converter) {
            return false;
        }

        hr = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            return false;
        }

        const UINT stride = width * 4;
        out.Rgba.resize(static_cast<std::size_t>(stride) * height);
        hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(out.Rgba.size()), out.Rgba.data());
        if (FAILED(hr)) {
            out = {};
            return false;
        }

        out.Width = static_cast<int>(width);
        out.Height = static_cast<int>(height);
        return out.IsValid();
    }

    inline bool DecodeFrame(IWICBitmapFrameDecode* frame, LoadedTexture& out) {
        out = {};
        ImagePixels pixels{};
        if (!DecodeFrameToPixels(frame, pixels)) {
            return false;
        }
        out = UploadRGBA(pixels.Rgba.data(), pixels.Width, pixels.Height);
        return out.Texture != nullptr;
    }

    inline LoadedTexture CreatePlaceholder() {
        constexpr int kWidth = 32;
        constexpr int kHeight = 32;
        std::uint8_t pixels[kWidth * kHeight * 4]{};
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const int idx = (y * kWidth + x) * 4;
                const bool bright = ((x + y) / 4) % 2 == 0;
                pixels[idx + 0] = static_cast<std::uint8_t>(bright ? 74 : 42);
                pixels[idx + 1] = static_cast<std::uint8_t>(bright ? 74 : 42);
                pixels[idx + 2] = static_cast<std::uint8_t>(bright ? 82 : 48);
                pixels[idx + 3] = 220;
            }
        }
        return UploadRGBA(pixels, kWidth, kHeight);
    }

    inline void Release(ImTextureID& texture) {
        if (auto* srv = reinterpret_cast<ID3D11ShaderResourceView*>(texture)) {
            srv->Release();
        }
        texture = nullptr;
    }
} // namespace detail

inline void SetDevice(ID3D11Device* device, ID3D11DeviceContext* context) {
    detail::Device() = device;
    detail::Context() = context;
}

inline bool LoadTextureFromFile(const char* file, LoadedTexture& out) {
    out = {};
    if (!detail::Device() || !file || !file[0]) {
        return false;
    }

    IWICImagingFactory* factory = detail::EnsureWicFactory();
    if (!factory) {
        return false;
    }

    const std::wstring wideFile = detail::ToWide(file);
    if (wideFile.empty()) {
        return false;
    }

    detail::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        wideFile.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        decoder.GetAddressOf());
    if (FAILED(hr) || !decoder) {
        return false;
    }

    detail::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    return SUCCEEDED(hr) && detail::DecodeFrame(frame.Get(), out);
}

inline LoadedTexture LoadTextureFromFile(const char* file) {
    LoadedTexture texture{};
    (void)LoadTextureFromFile(file, texture);
    return texture;
}

inline bool LoadPixelsFromFile(const char* file, ImagePixels& out) {
    out = {};
    if (!file || !file[0]) {
        return false;
    }

    IWICImagingFactory* factory = detail::EnsureWicFactory();
    if (!factory) {
        return false;
    }

    const std::wstring wideFile = detail::ToWide(file);
    if (wideFile.empty()) {
        return false;
    }

    detail::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        wideFile.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        decoder.GetAddressOf());
    if (FAILED(hr) || !decoder) {
        return false;
    }

    detail::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    return SUCCEEDED(hr) && detail::DecodeFrameToPixels(frame.Get(), out);
}

inline bool LoadTextureFromMemory(const std::uint8_t* data, int size, LoadedTexture& out) {
    out = {};
    if (!detail::Device() || !data || size <= 0) {
        return false;
    }

    IWICImagingFactory* factory = detail::EnsureWicFactory();
    if (!factory) {
        return false;
    }

    detail::ComPtr<IWICStream> stream;
    HRESULT hr = factory->CreateStream(stream.GetAddressOf());
    if (FAILED(hr) || !stream) {
        return false;
    }

    hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(data)),
        static_cast<DWORD>(size));
    if (FAILED(hr)) {
        return false;
    }

    detail::ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(hr) || !decoder) {
        return false;
    }

    detail::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    return SUCCEEDED(hr) && detail::DecodeFrame(frame.Get(), out);
}

inline LoadedTexture LoadTextureFromMemory(const std::uint8_t* data, int size) {
    LoadedTexture texture{};
    (void)LoadTextureFromMemory(data, size, texture);
    return texture;
}

inline bool LoadPixelsFromMemory(const std::uint8_t* data, int size, ImagePixels& out) {
    out = {};
    if (!data || size <= 0) {
        return false;
    }

    IWICImagingFactory* factory = detail::EnsureWicFactory();
    if (!factory) {
        return false;
    }

    detail::ComPtr<IWICStream> stream;
    HRESULT hr = factory->CreateStream(stream.GetAddressOf());
    if (FAILED(hr) || !stream) {
        return false;
    }

    hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(data)),
        static_cast<DWORD>(size));
    if (FAILED(hr)) {
        return false;
    }

    detail::ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(hr) || !decoder) {
        return false;
    }

    detail::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    return SUCCEEDED(hr) && detail::DecodeFrameToPixels(frame.Get(), out);
}

inline LoadedTexture CreateTextureFromRgba(const std::uint8_t* data, int width, int height) {
    return detail::UploadRGBA(data, width, height);
}

inline LoadedTexture CreateTextureFromPixels(const ImagePixels& pixels) {
    return pixels.IsValid()
        ? CreateTextureFromRgba(pixels.Rgba.data(), pixels.Width, pixels.Height)
        : LoadedTexture{};
}

inline void ReleaseTexture(LoadedTexture& texture) {
    detail::Release(texture.Texture);
    texture.Width = 0;
    texture.Height = 0;
}

inline void Initialize() {
    if (detail::Initialized()) {
        return;
    }
    detail::Placeholder() = detail::CreatePlaceholder();
    detail::Initialized() = true;
    SDK::Utils::Logging::Write()(LogLevel::Info, "Icons: initialized");
}

inline ImTextureID GetPlaceholder() {
    if (!detail::Initialized()) {
        Initialize();
    }
    return detail::Placeholder().Texture;
}

inline void LoadIconsFromDirectory(const char* directory) {
    if (!detail::Initialized()) {
        Initialize();
    }
    if (!directory || !directory[0]) {
        return;
    }

    char searchPath[MAX_PATH]{};
    _snprintf_s(searchPath, sizeof(searchPath), _TRUNCATE, "%s\\*", directory);

    WIN32_FIND_DATAA fd{};
    HANDLE handle = FindFirstFileA(searchPath, &fd);
    if (handle == INVALID_HANDLE_VALUE) {
        SDK::Utils::Logging::Write()(LogLevel::Warn, "Icons: directory missing or empty: %s", directory);
        return;
    }

    int loaded = 0;
    int failed = 0;
    do {
        const std::string name = fd.cFileName;
        if (name == "." || name == "..") {
            continue;
        }

        char full[MAX_PATH]{};
        _snprintf_s(full, sizeof(full), _TRUNCATE, "%s\\%s", directory, fd.cFileName);
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            LoadIconsFromDirectory(full);
            continue;
        }

        if (name.size() < 4) {
            continue;
        }
        const std::string ext = detail::ToLower(name.substr(name.size() - 4));
        if (ext != ".png" && ext != ".jpg" && ext != "jpeg") {
            continue;
        }

        LoadedTexture texture{};
        if (!LoadTextureFromFile(full, texture)) {
            ++failed;
            continue;
        }

        const std::string key = detail::FilenameKey(fd.cFileName);
        auto& cache = detail::Cache();
        auto it = cache.find(key);
        if (it != cache.end()) {
            ReleaseTexture(it->second);
        }
        cache[key] = texture;
        ++loaded;
    } while (FindNextFileA(handle, &fd));
    FindClose(handle);

    SDK::Utils::Logging::Write()(LogLevel::Info, "Icons: loaded %d (%d failed) from '%s'", loaded, failed, directory);
}

inline ImTextureID GetIcon(const std::string& key) {
    if (!detail::Initialized()) {
        Initialize();
    }
    if (key.empty()) {
        return detail::Placeholder().Texture;
    }

    const auto it = detail::Cache().find(detail::ToLower(key));
    return it != detail::Cache().end() ? it->second.Texture : detail::Placeholder().Texture;
}

inline bool HasIcon(const std::string& key) {
    if (!detail::Initialized() || key.empty()) {
        return false;
    }
    return detail::Cache().find(detail::ToLower(key)) != detail::Cache().end();
}

inline ImTextureID GetChampionSquare(const std::string& championName) {
    if (!detail::Initialized()) {
        Initialize();
    }
    if (championName.empty()) {
        return detail::Placeholder().Texture;
    }

    const std::string lower = detail::ToLower(championName);
    const std::string candidates[] = {
        lower + "_square",
        lower + "_square_0",
        lower,
    };
    for (const auto& candidate : candidates) {
        const auto it = detail::Cache().find(candidate);
        if (it != detail::Cache().end()) {
            return it->second.Texture;
        }
    }

    static const std::pair<const char*, const char*> aliases[] = {
        { "wukong", "monkeyking" },
        { "monkeyking", "monkeyking" },
        { "renataglasc", "renata" },
        { "fiddlesticks", "fiddlesticks" },
    };
    for (const auto& [alias, canonical] : aliases) {
        if (lower != alias) {
            continue;
        }
        const std::string canonicalName = canonical;
        const std::string aliasCandidates[] = {
            canonicalName + "_square",
            canonicalName + "_square_0",
            canonicalName,
        };
        for (const auto& candidate : aliasCandidates) {
            const auto it = detail::Cache().find(candidate);
            if (it != detail::Cache().end()) {
                return it->second.Texture;
            }
        }
    }

    return detail::Placeholder().Texture;
}

inline ImTextureID GetSpellIcon(const std::string& iconStem) {
    return GetIcon(iconStem);
}

inline ImTextureID GetSummonerSpellIcon(const std::string& iconStem) {
    return GetIcon(iconStem);
}

inline bool LoadIconFromBytes(const std::string& key, const std::uint8_t* png, int size) {
    if (!detail::Initialized()) {
        Initialize();
    }
    if (key.empty()) {
        return false;
    }

    LoadedTexture texture{};
    if (!LoadTextureFromMemory(png, size, texture)) {
        return false;
    }

    const std::string lower = detail::ToLower(key);
    auto& cache = detail::Cache();
    auto it = cache.find(lower);
    if (it != cache.end()) {
        ReleaseTexture(it->second);
    }
    cache[lower] = texture;
    return true;
}

inline bool LoadIconFromRgba(const std::string& key,
                             const std::uint8_t* rgba,
                             int width,
                             int height) {
    if (!detail::Initialized()) {
        Initialize();
    }
    if (key.empty() || !rgba || width <= 0 || height <= 0) {
        return false;
    }

    LoadedTexture texture = CreateTextureFromRgba(rgba, width, height);
    if (!texture.Texture) {
        return false;
    }

    const std::string lower = detail::ToLower(key);
    auto& cache = detail::Cache();
    auto it = cache.find(lower);
    if (it != cache.end()) {
        ReleaseTexture(it->second);
    }
    cache[lower] = texture;
    return true;
}

inline void Reset() {
    auto*& cache = detail::CachePtr();
    if (cache) {
        for (auto& pair : *cache) {
            ReleaseTexture(pair.second);
        }
        cache->clear();
        delete cache;
        cache = nullptr;
    }
    ReleaseTexture(detail::Placeholder());
    detail::Device() = nullptr;
    detail::Context() = nullptr;
    detail::Initialized() = false;
}

} // namespace SDK::UI::Icons
