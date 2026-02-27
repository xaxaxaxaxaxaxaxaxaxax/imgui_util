// raii_internal.hpp - RAII wrappers that depend on imgui_internal.h
//
// Usage:
//   { imgui_util::font_scale fs{1.5f}; ImGui::Text("big"); }
//
// Separate header so raii.hpp stays free of internal includes.
#pragma once

#include <imgui_internal.h>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    struct font_scale_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = float; // stores previous FontWindowScale
        static float begin(const float scale) noexcept {
            const auto *w = ImGui::GetCurrentWindow();
            if (w == nullptr) return -1.0f; // sentinel: no current window
            const float prev = w->FontWindowScale;
            ImGui::SetWindowFontScale(scale);
            return prev;
        }
        static void end(const float prev) noexcept {
            if (prev < 0.0f) return; // begin was a no-op
            ImGui::SetWindowFontScale(prev);
        }
    };

    using font_scale = raii_scope<font_scale_trait>;

} // namespace imgui_util
