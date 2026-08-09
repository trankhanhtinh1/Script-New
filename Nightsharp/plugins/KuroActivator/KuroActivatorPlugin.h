#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"

#include "KuroActivatorComponent.h"
#include "KuroQssActivator.h"
#include "KuroSmiteActivator.h"
#include "KuroAfterAttackActivator.h"

#include <array>
#include <cstddef>

namespace Plugins::KuroActivator {

// ============================================================================
// KuroActivatorPlugin — chỉ là nơi đăng ký/quản lý các component (dạng item).
// Mỗi component (QSS, Smite, ...) tự định nghĩa menu, tự register event và có
// vòng update riêng. Plugin chỉ tạo menu gốc, load/unload component và forward
// OnUpdate.
// ============================================================================
class KuroActivatorPlugin final : public Plugins::IPlugin {
public:
    static constexpr std::size_t kMaxComponents = 8;

    KuroActivatorPlugin() = default;

    const char* GetName() const override { return "KuroActivator"; }
    const char* GetInternalId() const override { return "utility.kuro_activator"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    Plugins::PluginCategory GetCategory() const override { return Plugins::PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return true; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        if (!CanLoad()) {
            loaded_ = false;
            return;
        }
        loaded_ = true;

        root_ = new SDK::Menu("utility.kuro_activator", "KuroActivator", true);
        root_->Attach();

        componentCount_ = 0;
        Register(&qssActivator_);
        Register(&smiteActivator_);
        Register(&afterAttackActivator_);

        NightSharpDebug::Logf("[KuroActivator] loaded components=%d/%d",
                              ComponentCount(), kMaxComponents);
    }

    void OnUpdate() override {
        if (!loaded_) return;
        for (std::size_t i = 0; i < componentCount_; ++i) {
            KuroActivatorComponent* component = components_[i];
            if (component && component->IsLoaded()) {
                component->OnUpdate();
            }
        }
    }

    void OnUnload() override {
        for (std::size_t i = componentCount_; i > 0; --i) {
            components_[i - 1]->OnUnload();
        }
        componentCount_ = 0;
        if (root_) {
            SDK::MenuManager::Instance().Remove(root_);
            delete root_;
        }
        root_ = nullptr;
        loaded_ = false;
        NightSharpDebug::Logf("[KuroActivator] unloaded");
    }

private:
    void Register(KuroActivatorComponent* component) noexcept {
        if (!component || componentCount_ >= kMaxComponents) return;
        component->OnLoad(root_);
        if (component->IsLoaded()) {
            components_[componentCount_++] = component;
            NightSharpDebug::Logf("[KuroActivator] component loaded: %s",
                                  component->GetName());
        } else {
            NightSharpDebug::Logf("[KuroActivator] component failed to load: %s",
                                  component->GetName());
        }
    }

    std::size_t ComponentCount() const noexcept { return componentCount_; }

    SDK::Menu* root_ = nullptr;
    bool loaded_ = false;
    std::array<KuroActivatorComponent*, kMaxComponents> components_{};
    std::size_t componentCount_ = 0;

    KuroQssActivator qssActivator_;
    KuroSmiteActivator smiteActivator_;
    KuroAfterAttackActivator afterAttackActivator_;
};

} // namespace Plugins::KuroActivator
