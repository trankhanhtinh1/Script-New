#pragma once

#include "../Core/MenuRegistry.h"
#include "../Rendering/Renderer.h"
#include "../Styling/Theme.h"
#include "../Visual/VisualSDK.h"
#include "../../../plugins/PluginRegistry.h"

#include <string>
#include <unordered_map>

namespace NightSharpMenu {

enum class MenuBackend {
    Legacy,
    Hybrid,
    MenuSDK,
};

class MenuSDKBridge {
public:
    static MenuSDKBridge& Instance();

    void Render();
    bool Initialize();
    void Shutdown();

    NightSharp::Menu::MenuBuilder RegisterMenu(
        const char* internalId,
        const char* displayName);
    NightSharp::Menu::MenuBuilder RegisterPlugin(
        const char* internalId,
        const char* displayName,
        const char* author,
        int registryIndex,
        const char* categoryId = "core");
    bool UnregisterMenu(const char* internalId);
    bool UnregisterPlugin(const char* internalId);
    bool IsPorted(const char* internalId) const;
    bool IsPointInside(const ImVec2& point) const;

    NightSharp::Menu::PermashowRegistry& Permashow();
    NightSharp::Menu::Renderer& Renderer();
    NightSharp::Menu::VisualSDK& Visuals();
    MenuBackend Backend() const;
    void SetBackend(MenuBackend backend);

private:
    struct PluginState {
        std::string displayName;
        std::string author;
        int registryIndex = -1;
        NightSharp::Menu::MenuNodeHandle parent;
        NightSharp::Menu::MenuNodeHandle managementNode;
        NightSharp::Menu::MenuNodeHandle configurationNode;
        NightSharp::Menu::MenuItemHandle loadButton;
        NightSharp::Menu::MenuItemHandle autoLoad;
    };

    struct DefaultMenuState {
        NightSharp::Menu::MenuItemHandle skinEnabled;
        NightSharp::Menu::MenuItemHandle skinId;
        NightSharp::Menu::MenuItemHandle zoomEnabled;
        NightSharp::Menu::MenuItemHandle bypassObs;
    };

    MenuSDKBridge();
    void EnsureShell();
    void EnsureDefaultMenus();
    void RefreshDefaultMenus();
    void RefreshPluginState(PluginState& state);
    static const char* CategoryLabel(const char* categoryId);

    NightSharp::Menu::MenuRegistry registry_;
    NightSharp::Menu::Theme theme_;
    NightSharp::Menu::Renderer renderer_;
    std::unordered_map<std::string, PluginState> portedPlugins_;
    std::unordered_map<std::string, NightSharp::Menu::MenuNodeHandle> runtimeMenus_;
    DefaultMenuState defaultMenus_;
    MenuBackend backend_ = MenuBackend::Hybrid;
    bool initialized_ = false;
};

}
