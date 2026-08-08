#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace SDK::GameObjects {

// Testable implementation behind the source-compatible two-parameter wrapper
// in GameObjects.h. Fill can only report truncation by returning the complete
// span size, so an exact fill is retried with geometrically larger storage.
template <typename T,
          std::size_t (*Fill)(std::span<T>),
          int (*FrameKey)()>
inline const std::vector<T>& FrameSnapshot() {
    static thread_local std::vector<T> buffer;
    static thread_local int completedFrame = 0;
    static thread_local bool hasCompletedFrame = false;

    const int frame = FrameKey();
    if (hasCompletedFrame && completedFrame == frame) {
        return buffer;
    }

    constexpr std::size_t kInitialCapacity = 8;
    constexpr std::size_t kMaximumCapacity = std::size_t{ 1 } << 24;

    if (buffer.capacity() < kInitialCapacity) {
        buffer.reserve(kInitialCapacity);
    }

    for (;;) {
        const std::size_t suppliedCapacity = buffer.capacity();
        buffer.resize(suppliedCapacity);
        const std::size_t count = Fill(std::span<T>(buffer.data(), suppliedCapacity));

        if (count < suppliedCapacity || suppliedCapacity >= kMaximumCapacity) {
            buffer.resize(std::min(count, suppliedCapacity));
            completedFrame = frame;
            hasCompletedFrame = true;
            return buffer;
        }

        const std::size_t doubled = suppliedCapacity <= kMaximumCapacity / 2
            ? suppliedCapacity * 2
            : kMaximumCapacity;
        const std::size_t nextCapacity = std::min(
            kMaximumCapacity,
            std::max(doubled, suppliedCapacity + kInitialCapacity));
        buffer.reserve(nextCapacity);
    }
}

} // namespace SDK::GameObjects
