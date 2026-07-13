#include "MenuSDKBridge.h"
#include "../../../menu/MenuConfig.h"

#include <algorithm>
#include <cstring>

namespace NightSharpMenu {

MenuSDKBridge& MenuSDKBridge::Instance() {
    static MenuSDKBridge bridge;
    return bridge;
}

MenuSDKBridge::MenuSDKBridge()
    : renderer_(registry_.Model(), theme_) {
    EnsureShell();
}

void MenuSDKBridge::EnsureShell() {
    auto core = registry_.Root("core", "Core");
    core.Section("language", "Language");
    auto plugins = core.Section("plugins", "Plugins");
    plugins.Section("core", "Core Plugins");
    plugins.Section("utility", "Utility");
    plugins.Section("champions", "Champions");
    plugins.Section("misc", "Misc");
    core.Section("menu", "Menu");
    core.Section("debug", "Debug");
    EnsureDefaultMenus();
}

void MenuSDKBridge::EnsureDefaultMenus() {
    auto change = registry_.Root("change", "Change");
    auto visual = registry_.Root("visual", "Visual");

    if (!defaultMenus_.skinEnabled) {
        defaultMenus_.skinEnabled = change.Checkbox(
            "skinChanger", "Skin Changer", Config::SkinChanger::enabled);
        defaultMenus_.skinId = change.Slider(
            "skinId", "Skin ID", Config::SkinChanger::skinId, 0, 100);
        defaultMenus_.zoomEnabled = visual.Checkbox(
            "zoomHack", "Zoom Hack", Config::ZoomHack::enabled);
        defaultMenus_.bypassObs = visual.Checkbox(
            "bypassObs", "Bypass OBS", Config::StreamProtection::bypassObs);

        defaultMenus_.skinEnabled->onChanged = [](NightSharp::Menu::MenuItem& item) {
            Config::SkinChanger::enabled = item.value;
        };
        defaultMenus_.skinId->onChanged = [](NightSharp::Menu::MenuItem& item) {
            Config::SkinChanger::skinId = item.integer;
        };
        defaultMenus_.zoomEnabled->onChanged = [](NightSharp::Menu::MenuItem& item) {
            Config::ZoomHack::enabled = item.value;
        };
        defaultMenus_.bypassObs->onChanged = [](NightSharp::Menu::MenuItem& item) {
            Config::StreamProtection::bypassObs = item.value;
        };
    }

    RefreshDefaultMenus();
}

void MenuSDKBridge::RefreshDefaultMenus() {
    if (defaultMenus_.skinEnabled) {
        defaultMenus_.skinEnabled->value = Config::SkinChanger::enabled;
    }
    if (defaultMenus_.skinId) {
        defaultMenus_.skinId->integer = std::clamp(
            Config::SkinChanger::skinId,
            defaultMenus_.skinId->minimum,
            defaultMenus_.skinId->maximum);
        defaultMenus_.skinId->enabled = Config::SkinChanger::enabled;
    }
    if (defaultMenus_.zoomEnabled) {
        defaultMenus_.zoomEnabled->value = Config::ZoomHack::enabled;
    }
    if (defaultMenus_.bypassObs) {
        defaultMenus_.bypassObs->value = Config::StreamProtection::bypassObs;
    }
}

bool MenuSDKBridge::Initialize() {
    EnsureShell();
    if (initialized_) {
        return true;
    }
    if (!ImGui::GetCurrentContext()) {
        return false;
    }

    const float dpiScale = std::max(
        ImGui::GetIO().DisplayFramebufferScale.x,
        1.0f);
    initialized_ = renderer_.Initialize(
        ImGui::GetIO().Fonts,
        dpiScale);
    if (initialized_) {
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        renderer_.SetPosition(ImVec2(
            std::max(24.0f, displaySize.x - 720.0f),
            40.0f));
    }
    return initialized_;
}

void MenuSDKBridge::Render() {
    if (backend_ == MenuBackend::Legacy || !Initialize()) {
        return;
    }

    RefreshDefaultMenus();
    for (auto& entry : portedPlugins_) {
        RefreshPluginState(entry.second);
    }
    renderer_.Render();
}

void MenuSDKBridge::Shutdown() {
    renderer_.Shutdown();
    renderer_.Permashow().Clear();
    NightSharp::Menu::VisualSDK::Instance().Reset();
    registry_.Model().roots.clear();
    portedPlugins_.clear();
    runtimeMenus_.clear();
    defaultMenus_ = {};
    initialized_ = false;
}

NightSharp::Menu::MenuBuilder MenuSDKBridge::RegisterMenu(
    const char* internalId,
    const char* displayName) {
    if (!internalId || !internalId[0]) {
        return {};
    }

    auto menu = registry_.Root(
        internalId,
        displayName ? displayName : internalId);
    const std::string id(internalId);
    runtimeMenus_[id] = menu.Node();
    auto plugin = portedPlugins_.find(id);
    if (plugin != portedPlugins_.end()) {
        plugin->second.configurationNode = menu.Node();
    }
    return menu;
}

const char* MenuSDKBridge::CategoryLabel(const char* categoryId) {
    if (!categoryId || !categoryId[0] || std::strcmp(categoryId, "core") == 0) {
        return "Core Plugins";
    }
    if (std::strcmp(categoryId, "utility") == 0) {
        return "Utility";
    }
    if (std::strcmp(categoryId, "champions") == 0) {
        return "Champions";
    }
    if (std::strcmp(categoryId, "misc") == 0) {
        return "Misc";
    }
    return categoryId;
}

NightSharp::Menu::MenuBuilder MenuSDKBridge::RegisterPlugin(
    const char* internalId,
    const char* displayName,
    const char* author,
    int registryIndex,
    const char* categoryId) {
    if (!internalId || !internalId[0]) {
        return {};
    }

    EnsureShell();
    const std::string id(internalId);
    auto core = registry_.Root("core", "Core");
    auto plugins = core.Section("plugins", "Plugins");
    auto parent = plugins.Section(
        categoryId && categoryId[0] ? categoryId : "core",
        CategoryLabel(categoryId));
    const std::string label = displayName ? displayName : id;
    auto management = parent.Section(id, label);
    management.Node()->secondaryLabel = author ? author : "NightSharp";

    PluginState state;
    state.displayName = label;
    state.author = author ? author : "NightSharp";
    state.registryIndex = registryIndex;
    state.parent = parent.Node();
    state.managementNode = management.Node();
    state.loadButton = management.Button("load", "Load", "Load");
    state.autoLoad = management.Checkbox("auto_load", "Auto load", false);
    portedPlugins_[id] = std::move(state);

    auto& stored = portedPlugins_.at(id);
    stored.loadButton->onChanged = [this, id](NightSharp::Menu::MenuItem&) {
        auto found = portedPlugins_.find(id);
        if (found == portedPlugins_.end()) {
            return;
        }
        PluginState& current = found->second;
        if (current.registryIndex < 0 ||
            current.registryIndex >= PluginRegistry::PluginCount) {
            return;
        }
        auto& plugin = PluginRegistry::Plugins[current.registryIndex];
        if (plugin.Loaded) {
            PluginRegistry::UnloadPlugin(current.registryIndex);
        } else if (PluginRegistry::CanPluginLoad(current.registryIndex)) {
            PluginRegistry::LoadPlugin(current.registryIndex);
        }
        RefreshPluginState(current);
    };
    stored.autoLoad->onChanged = [this, id](NightSharp::Menu::MenuItem& item) {
        auto found = portedPlugins_.find(id);
        if (found == portedPlugins_.end()) {
            return;
        }
        const int index = found->second.registryIndex;
        if (index >= 0 && index < PluginRegistry::PluginCount) {
            PluginRegistry::SetAlwaysLoad(index, item.value);
        }
    };
    RefreshPluginState(stored);
    return management;
}

void MenuSDKBridge::RefreshPluginState(PluginState& state) {
    if (state.registryIndex < 0 ||
        state.registryIndex >= PluginRegistry::PluginCount) {
        return;
    }
    auto& plugin = PluginRegistry::Plugins[state.registryIndex];
    const bool canLoad = PluginRegistry::CanPluginLoad(state.registryIndex);
    state.loadButton->actionLabel = plugin.Loaded ? "Unload" : "Load";
    state.loadButton->enabled = plugin.Loaded || canLoad;
    state.autoLoad->value = plugin.AlwaysLoad;
    state.autoLoad->enabled = true;
}

bool MenuSDKBridge::UnregisterMenu(const char* internalId) {
    if (!internalId || !internalId[0]) {
        return false;
    }
    const std::string id(internalId);
    const auto found = runtimeMenus_.find(id);
    if (found == runtimeMenus_.end() || !found->second) {
        return false;
    }
    const bool removed = registry_.Model().RemoveRoot(id);
    runtimeMenus_.erase(found);
    auto plugin = portedPlugins_.find(id);
    if (plugin != portedPlugins_.end()) {
        plugin->second.configurationNode.reset();
    }
    return removed;
}

bool MenuSDKBridge::UnregisterPlugin(const char* internalId) {
    if (!internalId || !internalId[0]) {
        return false;
    }
    const std::string id(internalId);
    UnregisterMenu(internalId);
    const auto found = portedPlugins_.find(id);
    if (found == portedPlugins_.end() || !found->second.parent) {
        return false;
    }
    const bool removed = found->second.parent->RemoveChild(id);
    portedPlugins_.erase(found);
    return removed;
}

bool MenuSDKBridge::IsPorted(const char* internalId) const {
    if (!internalId) {
        return false;
    }
    const auto found = portedPlugins_.find(internalId);
    return found != portedPlugins_.end() &&
        static_cast<bool>(found->second.configurationNode);
}

bool MenuSDKBridge::IsPointInside(const ImVec2& point) const {
    return renderer_.IsPointInside(point);
}

NightSharp::Menu::PermashowRegistry& MenuSDKBridge::Permashow() {
    return renderer_.Permashow();
}

NightSharp::Menu::Renderer& MenuSDKBridge::Renderer() {
    return renderer_;
}

NightSharp::Menu::VisualSDK& MenuSDKBridge::Visuals() {
    return NightSharp::Menu::VisualSDK::Instance();
}

MenuBackend MenuSDKBridge::Backend() const {
    return backend_;
}

void MenuSDKBridge::SetBackend(MenuBackend backend) {
    if (backend_ == backend) {
        return;
    }
    backend_ = backend;
    renderer_.SetVisible(backend_ != MenuBackend::Legacy);
}

}
