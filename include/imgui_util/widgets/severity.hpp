// severity.hpp - Unified severity enum for toasts, alerts, and status messages
//
// Usage:
//   imgui_util::severity::info
//   imgui_util::severity::success
//   imgui_util::severity::warning
//   imgui_util::severity::error
#pragma once

#include <cstdint>

namespace imgui_util {

    enum class severity : std::uint8_t { info, success, warning, error };

} // namespace imgui_util
