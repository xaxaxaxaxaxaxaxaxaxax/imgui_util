// progress_bar.hpp - Themed progress bar with label and color support
//
// Usage:
//   imgui_util::progress_bar(0.75f);
//   imgui_util::progress_bar_pct("Loading", 0.42f);
//   imgui_util::progress_bar_colored(0.9f, imgui_util::colors::error);
//
// Wraps ImGui::ProgressBar with semantic coloring and automatic percentage labels.
#pragma once

#include <algorithm>
#include <imgui.h>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    // Simple themed progress bar wrapping ImGui::ProgressBar with semantic coloring
    inline void progress_bar(float fraction, const ImVec2 &size = {-1.0f, 0.0f},
                             const char *overlay = nullptr) {
        ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), size, overlay);
    }

    // Progress bar with automatic percentage label
    inline void progress_bar_pct(const char *label, float fraction, float label_width = 180.0f,
                                 const ImVec2 &size = {-1.0f, 0.0f}) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(label_width);
        ImGui::SetNextItemWidth(-1.0f);
        const fmt_buf<16> overlay("{}%", static_cast<int>(std::clamp(fraction, 0.0f, 1.0f) * 100.0f));
        ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), size, overlay.c_str());
    }

    // Colored progress bar (custom fill color via style override)
    inline void progress_bar_colored(float fraction, const ImVec4 &fill_color,
                                     const ImVec2 &size = {-1.0f, 0.0f},
                                     const char *overlay = nullptr) {
        const style_color col(ImGuiCol_PlotHistogram, fill_color);
        ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), size, overlay);
    }

} // namespace imgui_util
