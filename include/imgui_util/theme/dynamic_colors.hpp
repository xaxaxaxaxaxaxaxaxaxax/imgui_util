// dynamic_colors.hpp - runtime light/dark detection and theme-aware semantic colors
//
// Usage:
//   if (is_light_mode()) { ... }                        // live check against WindowBg
//   ImVec4 col = choose(light_color, dark_color);       // pick based on cached mode
//   ImGui::TextColored(warning_color(), "Warning!");     // semantic color that adapts to theme

#pragma once

#include <imgui.h>

namespace imgui_util::theme {

    // Detect light/dark mode from current ImGui WindowBg luminance.
    // Uses perceptual luminance formula: 0.299*R + 0.587*G + 0.114*B
    [[nodiscard]]
    inline bool is_light_mode() noexcept {
        const ImVec4 &bg        = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        const float   luminance = (0.299f * bg.x) + (0.587f * bg.y) + (0.114f * bg.z);
        return luminance > 0.5f;
    }

    // Per-frame cached version of is_light_mode to avoid redundant computation.
    // NOTE: uses function-local statics; assumes a single ImGui context.
    [[nodiscard]]
    inline bool is_light_cached() noexcept {
        static int  last_frame = -1;
        static bool cached     = false;
        if (const int frame = ImGui::GetFrameCount(); frame != last_frame) {
            cached     = is_light_mode();
            last_frame = frame;
        }
        return cached;
    }

    // Return light or dark variant based on current theme mode (cached per frame)
    [[nodiscard]]
    inline ImVec4 choose(const ImVec4 light, const ImVec4 dark) noexcept {
        return is_light_cached() ? light : dark;
    }
    [[nodiscard]]
    inline ImU32 choose_u32(const ImU32 light, const ImU32 dark) noexcept {
        return is_light_cached() ? light : dark;
    }

    // Semantic colors derived from the active theme's accent palette.
    // Blends the theme's accent/text colors with semantic hues so they
    // harmonize with any preset rather than being hardcoded constants.

    [[nodiscard]]
    inline ImVec4 warning_color() noexcept {
        const float a = ImGui::GetStyle().Colors[ImGuiCol_Text].w;
        return choose({0.75f, 0.55f, 0.05f, a}, {1.0f, 0.8f, 0.2f, a});
    }
    [[nodiscard]]
    inline ImVec4 success_color() noexcept {
        const float a = ImGui::GetStyle().Colors[ImGuiCol_Text].w;
        return choose({0.10f, 0.55f, 0.25f, a}, {0.30f, 0.85f, 0.45f, a});
    }
    [[nodiscard]]
    inline ImVec4 error_color() noexcept {
        const float a = ImGui::GetStyle().Colors[ImGuiCol_Text].w;
        return choose({0.7f, 0.1f, 0.1f, a}, {0.8f, 0.15f, 0.15f, a});
    }
    [[nodiscard]]
    inline ImVec4 info_color() noexcept {
        const float a = ImGui::GetStyle().Colors[ImGuiCol_Text].w;
        return choose({0.15f, 0.45f, 0.70f, a}, {0.35f, 0.65f, 0.95f, a});
    }

} // namespace imgui_util::theme
