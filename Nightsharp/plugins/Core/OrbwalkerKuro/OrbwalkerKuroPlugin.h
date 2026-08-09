#pragma once
// ============================================================================
// OrbwalkerKuroPlugin — nạp orbwalker dev2 (bản copy trong folder này) như một
// Core plugin và override orbwalker SDK:
//   * OnLoad : đăng ký implementation "Kuro" vào SDK::Orbwalker, chọn nó làm
//     implementation hiện hành, rồi suspend bản SDK (unhook event + ẩn menu,
//     row "Orbwalker" trong Plugin Manager chuyển sang unloaded).
//   * OnUnload: gỡ "Kuro", selection tự rơi về "SDK", resume bản SDK.
// Mọi plugin phụ thuộc SDK::Orbwalker (facade static + event OnBeforeAttack/
// OnAttack/OnAfterAttack/...) không cần đổi gì: facade route qua
// implementation đang chọn, còn event bus dùng chung của SDK.
// ============================================================================

#include "../../IPlugin.h"
#include "../../PluginRegistry.h"
#include "OrbwalkerSelector.h"

namespace Plugins {

class OrbwalkerKuroPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "OrbwalkerKuro"; }
    const char* GetInternalId() const override { return "core.orbwalker_kuro"; }
    const char* GetAuthor() const override { return "Kuro"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return true; }

    void OnLoad() override {
        if (m_orbwalker) {
            return;
        }

        // Kuro is the preferred implementation for the KuroEvade bridge.
        // If 7UP was enabled manually or by an older saved config, switch it
        // off before attaching Kuro so both implementations cannot issue
        // competing attack/move orders.
        const int alternateOrbwalker =
            PluginRegistry::FindByInternalId("core.orbwalker_7up");
        if (alternateOrbwalker >= 0 &&
            PluginRegistry::Plugins[alternateOrbwalker].Loaded) {
            PluginRegistry::UnloadPlugin(alternateOrbwalker);
        }

        // Build menu con xong hết rồi mới Attach — ConfigStore áp giá trị đã
        // lưu cho cả subtree đúng lúc Attach (xem ConfigStore::OnMenuAttached).
        DestroyMenu();
        m_menu = new ::SDK::Menu(GetInternalId(), GetName(), true);
        m_orbwalker = new ::OrbwalkerKuro::OrbwalkerSelector(m_menu);
        m_menu->Attach();

        ::SDK::Orbwalker::AddOrbwalker(kImplementationName, m_orbwalker);
        ::SDK::Orbwalker::SetOrbwalker(kImplementationName);

        // Override bản SDK: ngừng chạy + ẩn menu của nó.
        SetSdkOrbwalkerLoaded(false);
    }

    void OnUnload() override {
        if (!m_orbwalker) {
            return;
        }

        // Gỡ "Kuro"; RemoveOrbwalker tự trả selection về "SDK".
        ::SDK::Orbwalker::RemoveOrbwalker(kImplementationName);
        m_orbwalker->Dispose();
        delete m_orbwalker;
        m_orbwalker = nullptr;
        DestroyMenu();

        // Bật lại orbwalker SDK (hook event + hiện menu).
        SetSdkOrbwalkerLoaded(true);
    }

private:
    static constexpr const char* kImplementationName = "Kuro";

    static void SetSdkOrbwalkerLoaded(bool loaded) {
        const int idx = PluginRegistry::FindByInternalId("orbwalker");
        if (idx >= 0 && PluginRegistry::HasRuntime(idx)) {
            if (loaded) {
                PluginRegistry::LoadPlugin(idx);
            } else {
                PluginRegistry::UnloadPlugin(idx);
            }
            return;
        }

        // Không có registry row runtime (SDK wrappers tắt?) — điều khiển trực tiếp.
        if (auto* impl = ::SDK::Orbwalker::GetOrbwalker("SDK")) {
            if (loaded) {
                impl->Resume();
            } else {
                impl->Suspend();
            }
        }
        if (idx >= 0) {
            PluginRegistry::Plugins[idx].Loaded = loaded;
        }
    }

    void DestroyMenu() {
        if (!m_menu) {
            return;
        }
        ::SDK::UI::MenuManager::Instance().Remove(m_menu);
        delete m_menu;
        m_menu = nullptr;
    }

    ::SDK::Menu* m_menu = nullptr;
    ::OrbwalkerKuro::OrbwalkerSelector* m_orbwalker = nullptr;
};

} // namespace Plugins
