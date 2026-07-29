#pragma once

#include <atomic>
#include <cstdint>

namespace SDK::Drawing {

inline constexpr std::uintptr_t HideAllDrawingHotkey = 'L';

namespace detail {
    inline std::atomic_bool HideAllDrawing{ false };
}

inline bool IsAllDrawingHidden() noexcept {
    return detail::HideAllDrawing.load(std::memory_order_acquire);
}

inline void SetAllDrawingHidden(bool hidden) noexcept {
    detail::HideAllDrawing.store(hidden, std::memory_order_release);
}

inline bool ToggleAllDrawingHidden() noexcept {
    bool expected = detail::HideAllDrawing.load(std::memory_order_acquire);
    for (;;) {
        const bool desired = !expected;
        if (detail::HideAllDrawing.compare_exchange_weak(
                expected,
                desired,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return desired;
        }
    }
}

// lParam bit 30 is set for auto-repeat WM_KEYDOWN messages. Ignoring repeats
// prevents a held hotkey from rapidly alternating between hidden and visible.
inline bool IsHideAllDrawingHotkeyPress(
    std::uint32_t msg,
    std::uintptr_t wParam,
    std::intptr_t lParam) noexcept {
    constexpr std::uint32_t WmKeyDown = 0x0100;
    constexpr std::uintptr_t PreviousKeyStateMask = std::uintptr_t{ 1 } << 30;
    return msg == WmKeyDown &&
           wParam == HideAllDrawingHotkey &&
           (static_cast<std::uintptr_t>(lParam) & PreviousKeyStateMask) == 0;
}

} // namespace SDK::Drawing
