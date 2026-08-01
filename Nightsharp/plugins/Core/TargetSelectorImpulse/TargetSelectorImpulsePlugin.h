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
        // Registration is not activation.  Keep Impulse quiescent until the
        // selector registry decides whether it is the restored/current
        // implementation; this prevents duplicate WndProc/Drawing hooks.
        implementation_->Suspend();
        menu_->Attach();

        const bool added = SDK::TargetSelector::AddTargetSelector(kImplementationName, implementation_);
        if (!added) {
            implementation_->Dispose();
            delete implementation_;
            implementation_ = nullptr;
            DestroyMenu();
            return;
        }
    }

    void OnUnload() override {
        if (!implementation_) return;

        SDK::TargetSelector::RemoveTargetSelector(kImplementationName);
        implementation_->Dispose();
        delete implementation_;
        implementation_ = nullptr;
        DestroyMenu();
    }

private:
    static constexpr const char* kImplementationName = "Impulse";

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
