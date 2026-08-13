#pragma once

#include <cstddef>
#include <cstdint>

namespace ruffneckk::remote_stash {

struct WidgetRect {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
};

// The public inter-mod build never judges or rewrites the layout position.
// Any signed x/y is accepted; the mod-owned button only needs a usable size.
constexpr bool IsUsableLayoutOwnedButton(const WidgetRect& rect) noexcept {
    return rect.width > 0 && rect.height > 0;
}

constexpr std::uintptr_t EmbeddedMessageOwner(
    const std::uintptr_t message,
    const std::size_t messageOffset
) noexcept {
    return message >= messageOffset ? message - messageOffset : 0;
}

} // namespace ruffneckk::remote_stash
