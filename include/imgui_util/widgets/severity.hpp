/// @file severity.hpp
/// @brief Unified severity enum for toasts, alerts, and status messages.
///
/// Usage:
/// @code
///   imgui_util::severity::info
///   imgui_util::severity::success
///   imgui_util::severity::warning
///   imgui_util::severity::error
/// @endcode
#pragma once

#include <cstdint>

namespace imgui_util {

    /// @brief Severity level for toasts, alerts, and status messages.
    enum class severity : std::uint8_t { info, success, warning, error };

} // namespace imgui_util
