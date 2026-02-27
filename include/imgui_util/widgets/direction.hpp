/// @file direction.hpp
/// @brief Shared direction enum used by splitter, toolbar, and other layout widgets.
#pragma once

#include <cstdint>

namespace imgui_util {

    /// @brief Shared direction enum used by splitter, toolbar, and other layout widgets.
    enum class direction : std::uint8_t { horizontal, vertical };

} // namespace imgui_util
