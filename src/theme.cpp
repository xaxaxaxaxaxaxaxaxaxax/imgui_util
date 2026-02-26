#include "imgui_util/theme/theme.hpp"

#include <algorithm>
#include <log.h>
#include <utility>

namespace imgui_util::theme {

    // ============================================================================
    // theme_config::from_preset - constexpr core + runtime ImNodes defaults
    // ============================================================================

    theme_config theme_config::from_preset(const theme_preset &preset, const theme_mode mode) {
        theme_config theme = from_preset_core(preset, mode);
        theme.name         = std::string(preset.name);
        if (mode == theme_mode::light) theme.name += " Light";
        apply_imnodes_defaults(theme);
        return theme;
    }

    // ============================================================================
    // theme_config::apply_imnodes_defaults - fill unset node colors from ImNodes
    // ============================================================================

    void theme_config::apply_imnodes_defaults(theme_config &cfg) {
        if (ImNodes::GetCurrentContext() == nullptr) return;

        const ImNodesStyle &defaults = ImNodes::GetStyle();
        for (int i = 0; i < ImNodesCol_COUNT; i++) {
            // Only fill entries not explicitly set by the preset (tracked via bitset).
            if (!cfg.node_colors_set.test(static_cast<std::size_t>(i))) {
                cfg.node_colors[i] = defaults.Colors[i]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }
    }

    // ============================================================================
    // theme_config::apply
    // ============================================================================

    void theme_config::apply() const {
        Log::debug("Theme", "applying theme '", name, "'");
        ImGuiStyle &style = ImGui::GetStyle();

        // Style values via shared field map
        for (const auto &[t_name, tptr, sptr]: style_float_map)
            style.*sptr = this->*tptr;

        // Copy all colors
        std::ranges::copy(colors, style.Colors);

        // Apply ImNodes style via shared field map
        ImNodesStyle &node_style = ImNodes::GetStyle();
        for (const auto &[t_name, tptr, nptr]: node_style_float_map)
            node_style.*nptr = this->*tptr;
        std::ranges::copy(node_colors, node_style.Colors);
    }

    // ============================================================================
    // theme_config::capture_from_current
    // ============================================================================

    theme_config theme_config::capture_from_current(std::string name) {
        theme_config theme;
        theme.name = std::move(name);

        // Capture ImGui style floats via shared field map
        const ImGuiStyle &style = ImGui::GetStyle();
        for (const auto &[t_name, tptr, sptr]: style_float_map)
            theme.*tptr = style.*sptr;

        std::ranges::copy(std::span(style.Colors, ImGuiCol_COUNT), theme.colors.begin());

        // Capture ImNodes style floats via shared field map
        const ImNodesStyle &node_style = ImNodes::GetStyle();
        for (const auto &[t_name, tptr, nptr]: node_style_float_map)
            theme.*tptr = node_style.*nptr;
        std::ranges::copy(std::span(node_style.Colors, ImNodesCol_COUNT), theme.node_colors.begin());

        return theme;
    }

} // namespace imgui_util::theme
