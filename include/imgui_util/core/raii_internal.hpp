// raii_internal.hpp - RAII wrappers that depend on imgui_internal.h
//
// Usage:
//   { imgui_util::font_scale fs{1.5f}; ImGui::Text("big"); }
//
// Separate header so raii.hpp stays free of internal includes.
#pragma once

#include <imgui_internal.h>
#include <optional>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    struct font_scale_trait {
        static constexpr auto policy = end_policy::push_pop;
        using storage                = std::optional<float>; // stores previous FontWindowScale, or nullopt if no window
        static std::optional<float> begin(const float scale) noexcept {
            const auto *w = ImGui::GetCurrentWindow();
            if (w == nullptr) return std::nullopt;
            const float prev = w->FontWindowScale;
            ImGui::SetWindowFontScale(scale);
            return prev;
        }
        static void end(const std::optional<float> prev) noexcept {
            if (!prev.has_value()) return;
            ImGui::SetWindowFontScale(*prev);
        }
    };

    using font_scale = raii_scope<font_scale_trait>;

} // namespace imgui_util
