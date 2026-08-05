#pragma once

#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"

#include <cstdint>
#include <cstring>

namespace Plugins::KuroActivator {

// ── Helper chung cho các component ──────────────────────────────────────────

inline bool ContainsIgnoreCase(const char* haystack,
                               const char* needle) noexcept {
    if (!haystack || !needle || !needle[0]) return false;
    const std::size_t n = std::strlen(needle);
    const std::size_t h = std::strlen(haystack);
    if (h < n) return false;
    for (std::size_t i = 0; i + n <= h; ++i) {
        if (_strnicmp(haystack + i, needle, n) == 0) return true;
    }
    return false;
}

// Tìm summoner spell chứa token (case-insensitive) ở slot Summoner1/2.
// Trả về -1 nếu không có.
inline int FindSummonerSlot(const SDK::AIHeroClient& player,
                            const char* token) noexcept {
    if (!token || !token[0]) return -1;
    const SDK::SpellSlot slots[] = {
        SDK::SpellSlot::Summoner1,
        SDK::SpellSlot::Summoner2,
    };
    for (SDK::SpellSlot slot : slots) {
        const auto spell = player.Spellbook().GetSpell(slot);
        if (!spell.IsValid()) continue;
        if (ContainsIgnoreCase(spell.Name().c_str(), token)) {
            return static_cast<int>(slot);
        }
    }
    return -1;
}

// ============================================================================
// KuroActivatorComponent — một "dạng item" (QSS, Smite, ...) tự quản:
//   - Tự tạo menu của riêng nó (submenu dưới root của plugin)
//   - Tự register/unregister event nó cần
//   - Có vòng update riêng
// Plugin chỉ giữ danh sách component để load/unload và forward OnUpdate.
// ============================================================================
class KuroActivatorComponent {
public:
    virtual ~KuroActivatorComponent() = default;

    virtual const char* GetName() const = 0;

    // root = menu gốc "KuroActivator" do plugin tạo. Component tự thêm submenu.
    virtual void OnLoad(SDK::Menu* root) = 0;
    virtual void OnUnload() = 0;
    virtual void OnUpdate() = 0;

    bool IsLoaded() const noexcept { return loaded_; }

protected:
    void SetLoaded(bool value) noexcept { loaded_ = value; }

private:
    bool loaded_ = false;
};

} // namespace Plugins::KuroActivator