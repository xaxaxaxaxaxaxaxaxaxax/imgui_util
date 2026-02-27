/// @file spinner.hpp
/// @brief Spinning arc indicator and overlay variant.
///
/// Uses ImDrawList PathArcTo + PathStroke for arc rendering.
/// Spins continuously using ImGui's time.
///
/// Usage:
/// @code
///   imgui_util::spinner("loading");                             // default size/color
///   imgui_util::spinner("sync", 12.0f, 3.0f, colors::teal);    // custom size/color
///
///   // Full-area overlay with spinner and optional message:
///   imgui_util::spinner_overlay("Loading data...");
/// @endcode
#pragma once

#include <cmath>
#include <imgui.h>
#include <numbers>
#include <string_view>

#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    namespace detail {

        constexpr float spinner_pi       = std::numbers::pi_v<float>;
        constexpr int   spinner_segments = 24;

        // Resolve transparent-alpha sentinel to theme's ButtonActive color
        [[nodiscard]] inline ImVec4 resolve_spinner_color(const ImVec4 &color) noexcept {
            if (color.w == 0.0f) return ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
            return color;
        }

        // Common setup shared by spinner and spinner_progress: reserves layout space,
        // resolves color, and returns the draw list + center point.
        struct spinner_setup {
            ImDrawList *dl{};
            ImVec2      center;
            ImVec4      col{};
        };

        [[nodiscard]] inline spinner_setup spinner_begin(const char *label, const float radius,
                                                         const ImVec4 &color) noexcept {
            const float  diameter = radius * 2.0f;
            const ImVec2 pos      = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(label, {diameter, diameter});
            return {
                .dl     = ImGui::GetWindowDrawList(),
                .center = {pos.x + radius, pos.y + radius},
                .col    = resolve_spinner_color(color),
            };
        }

    } // namespace detail

    /**
     * @brief Spinning arc indicator rendered at the current cursor position.
     *
     * Pass color {0,0,0,0} (default) to use the theme's ButtonActive color.
     * @param label     ImGui ID label.
     * @param radius    Radius of the arc in pixels.
     * @param thickness Stroke thickness in pixels.
     * @param color     Arc color ({0,0,0,0} = theme default).
     */
    inline void spinner(const char *label, const float radius = 8.0f, const float thickness = 2.0f,
                        const ImVec4 &color = {}) noexcept {
        const auto [dl, center, col] = detail::spinner_begin(label, radius, color);
        const ImU32 col32 = ImGui::ColorConvertFloat4ToU32(col);

        const float start = std::fmod(static_cast<float>(ImGui::GetTime()) * 5.0f, detail::spinner_pi * 2.0f);
        const float end   = start + detail::spinner_pi * 1.2f; // ~216 degree arc

        dl->PathClear();
        dl->PathArcTo(center, radius, start, end, detail::spinner_segments);
        dl->PathStroke(col32, 0, thickness);
    }

    /**
     * @brief Spinner overlay that fills the available content region with a dimmed background
     *        and centers a spinner with an optional label below it.
     * @param label          Optional text shown below the spinner.
     * @param spinner_radius Spinner radius in pixels.
     * @param color          Spinner color.
     */
    inline void spinner_overlay(const std::string_view label = {}, const float spinner_radius = 24.0f,
                                const ImVec4 &color = colors::accent) noexcept {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 pos   = ImGui::GetCursorScreenPos();

        auto *dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(pos, {pos.x + avail.x, pos.y + avail.y}, IM_COL32(0, 0, 0, 100));

        const float cx = pos.x + avail.x * 0.5f;
        const float cy = pos.y + avail.y * 0.5f - spinner_radius;

        ImGui::SetCursorScreenPos({cx - spinner_radius, cy - spinner_radius});
        spinner("##overlay_spinner", spinner_radius, 4.0f, color);

        if (!label.empty()) {
            const ImVec2 text_size = ImGui::CalcTextSize(label.data(), label.data() + label.size());
            ImGui::SetCursorScreenPos({cx - text_size.x * 0.5f, cy + spinner_radius + 8.0f});
            colored_text(label, colors::text_secondary);
        }
    }

    /**
     * @brief Determinate progress arc (0.0 to 1.0).
     *
     * Renders a background circle and a filled arc proportional to progress.
     * @param label     ImGui ID label.
     * @param progress  Progress value, clamped to [0.0, 1.0].
     * @param radius    Radius of the arc in pixels.
     * @param thickness Stroke thickness in pixels.
     * @param color     Arc color ({0,0,0,0} = theme default).
     */
    inline void spinner_progress(const char *label, const float progress, const float radius = 8.0f,
                                 const float thickness = 2.0f, const ImVec4 &color = {}) noexcept {
        const auto [dl, center, col] = detail::spinner_begin(label, radius, color);
        const ImU32 col32 = ImGui::ColorConvertFloat4ToU32(col);

        const ImU32  bg_col = ImGui::ColorConvertFloat4ToU32({col.x, col.y, col.z, col.w * 0.25f});

        dl->PathClear();
        dl->PathArcTo(center, radius, 0.0f, detail::spinner_pi * 2.0f, detail::spinner_segments);
        dl->PathStroke(bg_col, 0, thickness);

        const float clamped   = std::clamp(progress, 0.0f, 1.0f);
        const float end_angle = -detail::spinner_pi * 0.5f + clamped * (detail::spinner_pi * 2.0f);
        dl->PathClear();
        dl->PathArcTo(center, radius, -detail::spinner_pi * 0.5f, end_angle, detail::spinner_segments);
        dl->PathStroke(col32, 0, thickness);
    }

} // namespace imgui_util
