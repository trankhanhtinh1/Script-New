#pragma once

#include "../../IPlugin.h"
#include "../../PluginRegistry.h"
#include "TargetSelectorImpulse.h"

namespace Plugins {

class TargetSelectorImpulsePlugin final : public IPlugin {
public:
    const char* GetName() const override { return "TargetSelectorImpulse"; }
    const char* GetInternalId() const override { return "core.targetselector_impulse"; }
    const char* GetAuthor() const override { return "Impulse"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return true; }

    void OnLoad() override {
        if (implementation_) return;

        DestroyMenu();
        menu_ = new SDK::Menu(GetInternalId(), GetName(), true);
        implementation_ = new TargetSelectorImpulse::TargetSelectorImpulse(menu_);
        menu_->Attach();

        const bool added = SDK::TargetSelector::AddTargetSelector(kImplementationName, implementation_);
        const bool selected = added && SDK::TargetSelector::SetTargetSelector(kImplementationName);
        if (!selected) {
            if (added) SDK::TargetSelector::RemoveTargetSelector(kImplementationName);
            implementation_->Dispose();
            delete implementation_;
            implementation_ = nullptr;
            DestroyMenu();
            return;
        }

        SetSdkTargetSelectorLoaded(false);
    }

    void OnUnload() override {
        if (!implementation_) return;

        SDK::TargetSelector::RemoveTargetSelector(kImplementationName);
        implementation_->Dispose();
        delete implementation_;
        implementation_ = nullptr;
        DestroyMenu();
        SetSdkTargetSelectorLoaded(true);
    }

private:
    static constexpr const char* kImplementationName = "Impulse";

    static void SetSdkTargetSelectorLoaded(bool loaded) {
        const int index = PluginRegistry::FindByInternalId("targetselector");
        if (index >= 0 && PluginRegistry::HasRuntime(index)) {
            if (loaded) PluginRegistry::LoadPlugin(index);
            else PluginRegistry::UnloadPlugin(index);
            return;
        }

        if (auto* sdk = SDK::TargetSelector::GetTargetSelector("SDK")) {
            if (loaded) sdk->Resume();
            else sdk->Suspend();
        }
        if (index >= 0) PluginRegistry::Plugins[index].Loaded = loaded;
    }

    void DestroyMenu() {
        if (!menu_) return;
        SDK::UI::MenuManager::Instance().Remove(menu_);
        delete menu_;
        menu_ = nullptr;
    }

    SDK::Menu* menu_ = nullptr;
    TargetSelectorImpulse::TargetSelectorImpulse* implementation_ = nullptr;
};

} // namespace Plugins
