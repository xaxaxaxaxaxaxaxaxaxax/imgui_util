/// @file dynamic_colors.hpp
/// @brief Runtime light/dark detection and theme-aware semantic colors.
///
/// Usage:
/// @code
///   if (is_light_mode()) { ... }                        // live check against WindowBg
///   ImVec4 col = choose(light_color, dark_color);       // pick based on cached mode
///   ImGui::TextColored(warning_color(), "Warning!");     // semantic color that adapts to theme
/// @endcode

#pragma once

#include <imgui.h>

#include "imgui_util/theme/color_math.hpp"

namespace imgui_util::theme {

    using color::rgb_color;

    /**
     * @brief Detect light/dark mode from current ImGui WindowBg luminance.
     *
     * Uses perceptual luminance formula: 0.299*R + 0.587*G + 0.114*B.
     * @return True if the current theme appears to be light mode.
     */
    [[nodiscard]] inline bool is_light_mode() noexcept {
        const ImVec4 &bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        return color::luminance(bg) > 0.5f;
    }

    /**
     * @brief Per-frame cached version of is_light_mode() to avoid redundant computation.
     *
     * Uses function-local statics; assumes a single ImGui context.
     * @return Cached light-mode result for the current frame.
     */
    [[nodiscard]] inline bool is_light_cached() noexcept {
        static int  last_frame = -1;
        static bool cached     = false;
        if (const int frame = ImGui::GetFrameCount(); frame != last_frame) {
            cached     = is_light_mode();
            last_frame = frame;
        }
        return cached;
    }

    /**
     * @brief Pick a color based on cached light/dark mode detection.
     * @param light Color to use in light mode.
     * @param dark  Color to use in dark mode.
     */
    [[nodiscard]] inline ImVec4 choose(const ImVec4 &light, const ImVec4 &dark) noexcept {
        return is_light_cached() ? light : dark;
    }
    /**
     * @brief Pick an rgb_color based on cached light/dark mode detection.
     * @param light Color to use in light mode.
     * @param dark  Color to use in dark mode.
     */
    [[nodiscard]] inline rgb_color choose(const rgb_color &light, const rgb_color &dark) noexcept {
        return is_light_cached() ? light : dark;
    }
    /**
     * @brief Pick a packed color based on cached light/dark mode detection.
     * @param light Packed color to use in light mode.
     * @param dark  Packed color to use in dark mode.
     */
    [[nodiscard]] inline ImU32 choose_u32(const ImU32 light, const ImU32 dark) noexcept {
        return is_light_cached() ? light : dark;
    }

    /// @brief Semantic colors that adapt to the active theme's light/dark mode.
    ///
    /// Each returns a light-mode or dark-mode variant chosen via is_light_cached(),
    /// using the current text alpha for consistency.
    /// @{

    /// @brief Amber warning color.
    [[nodiscard]] inline ImVec4 warning_color() noexcept {
        const float a = ImGui::GetStyle().Colors[ImGuiCol_Text].w;
        return choose({0.75f, 0.55f, 0.05f, a}, {1.0f, 0.8f, 0.2f, a});
    }
    /// @brief Green success color.
    [[nodiscard]] inline ImVec4 success_color() noexcept {
        const float a = ImGui::GetStyle().Colors[ImGuiCol_Text].w;
        return choose({0.10f, 0.55f, 0.25f, a}, {0.30f, 0.85f, 0.45f, a});
    }
    /// @brief Red error color.
    [[nodiscard]] inline ImVec4 error_color() noexcept {
        const float a = ImGui::GetStyle().Colors[ImGuiCol_Text].w;
        return choose({0.7f, 0.1f, 0.1f, a}, {0.8f, 0.15f, 0.15f, a});
    }
    /// @brief Blue informational color.
    [[nodiscard]] inline ImVec4 info_color() noexcept {
        const float a = ImGui::GetStyle().Colors[ImGuiCol_Text].w;
        return choose({0.15f, 0.45f, 0.70f, a}, {0.35f, 0.65f, 0.95f, a});
    }

    /// @}

} // namespace imgui_util::theme
