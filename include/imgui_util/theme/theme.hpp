// theme.hpp - preset-driven theme definition and constexpr color derivation
//
// Usage:
//   auto theme = theme_config::from_preset(my_preset);   // build from preset (needs ImNodes ctx)
//   theme.apply();                                        // push to ImGui + ImNodes styles
//   auto blended = lerp(theme_a, theme_b, 0.5f);         // smooth transition between themes
//
// For compile-time use (no ImNodes context):
//   constexpr auto core = theme_config::from_preset_core(my_preset, +1.0f);

#pragma once

#include <array>
#include <cstdint>
#include <imgui.h>
#include <imnodes.h>
#include <optional>
#include <string>
#include <string_view>

#include "imgui_util/theme/color_math.hpp"

namespace imgui_util::theme {

    using color::rgb_color;

    // Forward declaration for member pointer table
    struct theme_config;

    // Compile-time descriptor for a serializable field on theme_config.
    // T = field type (float, rgb_color, std::optional<rgb_color>, etc.)
    template<typename T>
    struct theme_field {
        std::string_view name; // JSON/display key
        T theme_config::*ptr;  // member pointer for generic access
    };

    // ============================================================================
    // Data-driven theme preset definition
    // ============================================================================

    struct theme_preset {
        std::string_view name;

        // Background colors (RGB, alpha always 1.0)
        rgb_color bg_dark;
        rgb_color bg_mid;

        // Primary accent (for buttons, headers, etc.)
        rgb_color accent;

        // Secondary accent (for sliders, checkmarks, links)
        rgb_color secondary;

        std::optional<rgb_color> alternate; // optional second secondary for two-tone themes (e.g. plot histogram)

        std::optional<rgb_color> text; // text override (defaults to white for dark, near-black for light)

        // Node editor colors (title bar and links)
        ImU32 node_title_bar;
        ImU32 node_title_bar_hovered;
        ImU32 node_title_bar_selected;
        ImU32 node_link;
        ImU32 node_link_hovered;
        ImU32 node_pin;
        ImU32 node_pin_hovered;
        ImU32 node_grid_bg;

        // Optional: node background override
        std::optional<ImU32> node_background;
        std::optional<ImU32> node_background_hovered;
        std::optional<ImU32> node_background_selected;
        std::optional<ImU32> node_outline;

        // Light-mode overrides -- from_preset_core picks these when offset_dir < 0
        std::optional<rgb_color> light_bg_dark;
        std::optional<rgb_color> light_bg_mid;
        std::optional<rgb_color> light_accent;
        std::optional<rgb_color> light_secondary;
        std::optional<rgb_color> light_text;

        [[nodiscard]]
        constexpr bool has_light() const {
            return light_bg_mid.has_value();
        }
    };

    // Type-safe dark/light mode selector (replaces raw float +1/-1)
    enum class theme_mode : int8_t { dark = 1, light = -1 };

    // ============================================================================
    // Full theme definition - stores all customizable colors
    // ============================================================================

    struct theme_config {
        std::string name;

        // ImGui style values
        float window_rounding    = 0.0f;
        float frame_rounding     = 0.0f;
        float window_border_size = 1.0f;
        float frame_border_size  = 1.0f;
        float tab_rounding       = 0.0f;
        float scrollbar_rounding = 0.0f;
        float grab_rounding      = 0.0f;

        // ImGui colors (all entries)
        std::array<ImVec4, ImGuiCol_COUNT> colors;

        // ImNodes colors
        std::array<ImU32, ImNodesCol_COUNT> node_colors{};

        // ImNodes style values
        float node_corner_rounding = 0.0f;
        float link_thickness       = 2.0f;
        float pin_circle_radius    = 4.5f;

        // Preset base RGB colors (preserved for round-trip serialization)
        rgb_color                preset_bg_dark{};
        rgb_color                preset_bg_mid{};
        rgb_color                preset_accent{};
        rgb_color                preset_accent_hover{};
        rgb_color                preset_secondary{};
        rgb_color                preset_secondary_dim{};
        std::optional<rgb_color> preset_alternate;
        std::optional<rgb_color> preset_text;

        // Apply theme to ImGui and ImNodes
        void apply() const;

        // Capture current ImGui/ImNodes style into theme
        static theme_config capture_from_current(std::string name);

        // Build theme from preset data (constexpr core + runtime ImNodes defaults).
        // mode: dark or light (controls bg offset direction and light-mode overrides).
        static theme_config from_preset(const theme_preset &preset, theme_mode mode = theme_mode::dark);

        // Constexpr core: build theme from preset without any runtime dependencies.
        // Sets all colors, style values, and derived fields. Does not set name or
        // initialize ImNodes defaults (node_colors entries not explicitly set by the
        // preset will be zero-initialized rather than copied from ImNodes defaults).
        // mode: dark (offsets go brighter) or light (offsets go darker).
        static constexpr theme_config from_preset_core(const theme_preset &preset, theme_mode mode = theme_mode::dark);

        // Legacy overloads accepting raw float offset_dir (+1 dark, -1 light)
        static theme_config from_preset(const theme_preset &preset, float offset_dir) {
            return from_preset(preset, offset_dir > 0 ? theme_mode::dark : theme_mode::light);
        }
        static constexpr theme_config from_preset_core(const theme_preset &preset, float offset_dir) {
            return from_preset_core(preset, offset_dir > 0 ? theme_mode::dark : theme_mode::light);
        }

        // Apply ImNodes default colors for any node_colors entry not set by preset.
        // Requires a live ImNodes context. Called automatically by from_preset().
        static void apply_imnodes_defaults(theme_config &cfg);
    };

    // Compile-time descriptor mapping a theme_config float field to its ImGuiStyle counterpart.
    // Adding a new style float requires changing only this table -- apply() and capture_from_current()
    // both iterate it, eliminating duplication.
    struct style_field_pair {
        std::string_view      name;         // serialization key
        float theme_config::*theme_ptr;     // member in theme_config
        float ImGuiStyle::*  imgui_ptr;     // member in ImGuiStyle
    };

    inline constexpr std::array style_float_map = std::to_array<style_field_pair>({
        {.name = "window_rounding",    .theme_ptr = &theme_config::window_rounding,    .imgui_ptr = &ImGuiStyle::WindowRounding},
        {.name = "frame_rounding",     .theme_ptr = &theme_config::frame_rounding,     .imgui_ptr = &ImGuiStyle::FrameRounding},
        {.name = "window_border_size", .theme_ptr = &theme_config::window_border_size, .imgui_ptr = &ImGuiStyle::WindowBorderSize},
        {.name = "frame_border_size",  .theme_ptr = &theme_config::frame_border_size,  .imgui_ptr = &ImGuiStyle::FrameBorderSize},
        {.name = "tab_rounding",       .theme_ptr = &theme_config::tab_rounding,       .imgui_ptr = &ImGuiStyle::TabRounding},
        {.name = "scrollbar_rounding", .theme_ptr = &theme_config::scrollbar_rounding, .imgui_ptr = &ImGuiStyle::ScrollbarRounding},
        {.name = "grab_rounding",      .theme_ptr = &theme_config::grab_rounding,      .imgui_ptr = &ImGuiStyle::GrabRounding},
    });

    // Compile-time descriptor mapping a theme_config float field to its ImNodesStyle counterpart.
    struct node_style_field_pair {
        std::string_view       name;        // serialization key
        float theme_config::* theme_ptr;    // member in theme_config
        float ImNodesStyle::* imnodes_ptr;  // member in ImNodesStyle
    };

    inline constexpr std::array node_style_float_map = std::to_array<node_style_field_pair>({
        {.name = "node_corner_rounding", .theme_ptr = &theme_config::node_corner_rounding, .imnodes_ptr = &ImNodesStyle::NodeCornerRounding},
        {.name = "link_thickness",       .theme_ptr = &theme_config::link_thickness,       .imnodes_ptr = &ImNodesStyle::LinkThickness},
        {.name = "pin_circle_radius",    .theme_ptr = &theme_config::pin_circle_radius,    .imnodes_ptr = &ImNodesStyle::PinCircleRadius},
    });

    // Constexpr table of serializable float fields
    inline constexpr std::array theme_float_fields = std::to_array<theme_field<float>>({
        {.name = "window_rounding", .ptr = &theme_config::window_rounding},
        {.name = "frame_rounding", .ptr = &theme_config::frame_rounding},
        {.name = "window_border_size", .ptr = &theme_config::window_border_size},
        {.name = "frame_border_size", .ptr = &theme_config::frame_border_size},
        {.name = "tab_rounding", .ptr = &theme_config::tab_rounding},
        {.name = "scrollbar_rounding", .ptr = &theme_config::scrollbar_rounding},
        {.name = "grab_rounding", .ptr = &theme_config::grab_rounding},
        {.name = "node_corner_rounding", .ptr = &theme_config::node_corner_rounding},
        {.name = "link_thickness", .ptr = &theme_config::link_thickness},
        {.name = "pin_circle_radius", .ptr = &theme_config::pin_circle_radius},
    });

    // Constexpr table of serializable RGB fields
    inline constexpr std::array theme_rgb_fields = std::to_array<theme_field<rgb_color>>({
        {.name = "preset_bg_dark", .ptr = &theme_config::preset_bg_dark},
        {.name = "preset_bg_mid", .ptr = &theme_config::preset_bg_mid},
        {.name = "preset_accent", .ptr = &theme_config::preset_accent},
        {.name = "preset_accent_hover", .ptr = &theme_config::preset_accent_hover},
        {.name = "preset_secondary", .ptr = &theme_config::preset_secondary},
        {.name = "preset_secondary_dim", .ptr = &theme_config::preset_secondary_dim},
    });

    // Constexpr table of serializable optional RGB fields
    inline constexpr std::array theme_opt_rgb_fields = std::to_array<theme_field<std::optional<rgb_color>>>({
        {.name = "preset_alternate", .ptr = &theme_config::preset_alternate},
        {.name = "preset_text", .ptr = &theme_config::preset_text},
    });

    // ============================================================================
    // Constexpr implementations (must be in header for cross-TU compile-time use)
    // ============================================================================

    constexpr theme_config theme_config::from_preset_core(const theme_preset &preset, const theme_mode mode) {
        using color::offset;
        using color::offset_u32_rgb;
        using color::rgb;
        using color::scale;

        theme_config theme{};

        // Direction multiplier: +1 offsets go brighter (dark mode), -1 go darker (light mode)
        const float d        = static_cast<float>(mode);
        const bool  is_light = d < 0.0f;

        // Pick light-mode overrides when available
        const rgb_color bg_dark_c = (is_light && preset.light_bg_dark) ? *preset.light_bg_dark : preset.bg_dark;
        const rgb_color bg_mid_c  = (is_light && preset.light_bg_mid) ? *preset.light_bg_mid : preset.bg_mid;
        const rgb_color accent_c  = (is_light && preset.light_accent) ? *preset.light_accent : preset.accent;
        const rgb_color second_c  = (is_light && preset.light_secondary) ? *preset.light_secondary : preset.secondary;
        const std::optional<rgb_color> text_c = (is_light && preset.light_text) ? preset.light_text : preset.text;

        // Derive accent_hover and secondary_dim from resolved base colors
        const rgb_color accent_hover_c  = accent_c + 0.10f;
        const rgb_color secondary_dim_c = second_c * 0.80f;

        // Preserve preset base colors for round-trip serialization
        theme.preset_bg_dark       = bg_dark_c;
        theme.preset_bg_mid        = bg_mid_c;
        theme.preset_accent        = accent_c;
        theme.preset_accent_hover  = accent_hover_c;
        theme.preset_secondary     = second_c;
        theme.preset_secondary_dim = secondary_dim_c;
        theme.preset_alternate     = preset.alternate;
        theme.preset_text          = text_c;

        // Derive colors from resolved palette
        const ImVec4 bg_dark       = rgb(bg_dark_c);
        const ImVec4 bg_mid        = rgb(bg_mid_c);
        const ImVec4 bg_light      = offset(bg_mid_c, d * 0.04f);
        const ImVec4 accent        = rgb(accent_c);
        const ImVec4 accent_hover  = rgb(accent_hover_c);
        const ImVec4 accent_active = offset(accent_hover_c, 0.10f);
        const ImVec4 secondary     = rgb(second_c);
        const ImVec4 secondary_dim = rgb(secondary_dim_c);

        // Text colors: dark text for light mode, light text for dark mode
        const ImVec4 text_default   = is_light ? ImVec4(0.10f, 0.10f, 0.12f, 1.0f) : ImVec4(0.95f, 0.95f, 0.97f, 1.0f);
        const float  text_dim_scale = is_light ? 1.40f : 0.65f;
        const ImVec4 text_primary   = !text_c ? text_default : rgb(*text_c);
        const ImVec4 text_secondary =
            !text_c ? (is_light ? ImVec4{0.45f, 0.45f, 0.50f, 1.0f} : ImVec4{0.60f, 0.60f, 0.65f, 1.0f})
                    : scale(*text_c, text_dim_scale);

        // Window backgrounds
        theme.colors.at(ImGuiCol_WindowBg) = bg_mid;
        theme.colors.at(ImGuiCol_ChildBg)  = offset(bg_dark_c, d * 0.02f);
        theme.colors.at(ImGuiCol_PopupBg)  = offset(bg_mid_c, d * 0.02f, 0.98f);

        // Title bars
        theme.colors.at(ImGuiCol_TitleBg)          = bg_dark;
        theme.colors.at(ImGuiCol_TitleBgActive)    = offset(bg_mid_c, d * 0.02f);
        theme.colors.at(ImGuiCol_TitleBgCollapsed) = offset(bg_dark_c, d * -0.02f, 0.8f);

        // Menu bar
        theme.colors.at(ImGuiCol_MenuBarBg) = bg_dark;

        // Borders
        theme.colors.at(ImGuiCol_Border)       = offset(bg_mid_c, d * 0.11f, 0.6f);
        theme.colors.at(ImGuiCol_BorderShadow) = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        // Frame backgrounds
        theme.colors.at(ImGuiCol_FrameBg)        = offset(bg_mid_c, d * 0.06f);
        theme.colors.at(ImGuiCol_FrameBgHovered) = offset(bg_mid_c, d * 0.11f);
        theme.colors.at(ImGuiCol_FrameBgActive)  = offset(bg_mid_c, d * 0.16f);

        // Buttons
        theme.colors.at(ImGuiCol_Button)        = offset(bg_mid_c, d * 0.10f);
        theme.colors.at(ImGuiCol_ButtonHovered) = scale(accent_c, 0.85f);
        theme.colors.at(ImGuiCol_ButtonActive)  = accent;

        // Headers
        theme.colors.at(ImGuiCol_Header)        = offset(bg_mid_c, d * 0.10f);
        theme.colors.at(ImGuiCol_HeaderHovered) = scale(accent_c, 0.80f);
        theme.colors.at(ImGuiCol_HeaderActive)  = accent;

        // Tabs
        theme.colors.at(ImGuiCol_Tab)                       = bg_light;
        theme.colors.at(ImGuiCol_TabHovered)                = scale(accent_c, 0.85f);
        theme.colors.at(ImGuiCol_TabSelected)               = scale(accent_c, 0.70f);
        theme.colors.at(ImGuiCol_TabSelectedOverline)       = accent;
        theme.colors.at(ImGuiCol_TabDimmed)                 = bg_mid;
        theme.colors.at(ImGuiCol_TabDimmedSelected)         = offset(bg_mid_c, d * 0.08f);
        theme.colors.at(ImGuiCol_TabDimmedSelectedOverline) = scale(accent_c, 0.80f, 0.5f);

        // Docking
        theme.colors.at(ImGuiCol_DockingPreview) = rgb(accent_c, 0.7f);
        theme.colors.at(ImGuiCol_DockingEmptyBg) = bg_dark;

        // Scrollbar
        theme.colors.at(ImGuiCol_ScrollbarBg)          = rgb(bg_dark_c, 0.6f);
        theme.colors.at(ImGuiCol_ScrollbarGrab)        = offset(bg_mid_c, d * 0.16f);
        theme.colors.at(ImGuiCol_ScrollbarGrabHovered) = offset(bg_mid_c, d * 0.26f);
        theme.colors.at(ImGuiCol_ScrollbarGrabActive)  = offset(bg_mid_c, d * 0.36f);

        // Slider grab - secondary accent
        theme.colors.at(ImGuiCol_SliderGrab)       = secondary_dim;
        theme.colors.at(ImGuiCol_SliderGrabActive) = secondary;

        // Check mark
        theme.colors.at(ImGuiCol_CheckMark) = secondary;

        // Resize grip
        theme.colors.at(ImGuiCol_ResizeGrip)        = offset(bg_mid_c, d * 0.16f, 0.4f);
        theme.colors.at(ImGuiCol_ResizeGripHovered) = accent;
        theme.colors.at(ImGuiCol_ResizeGripActive)  = accent_active;

        // Separator
        theme.colors.at(ImGuiCol_Separator)        = offset(bg_mid_c, d * 0.14f);
        theme.colors.at(ImGuiCol_SeparatorHovered) = accent;
        theme.colors.at(ImGuiCol_SeparatorActive)  = accent_active;

        // Text
        theme.colors.at(ImGuiCol_Text)           = text_primary;
        theme.colors.at(ImGuiCol_TextDisabled)   = text_secondary;
        theme.colors.at(ImGuiCol_TextSelectedBg) = scale(accent_c, 0.80f, 0.4f);

        // Plot colors - use secondary or alternate
        const rgb_color &plot_color                    = preset.alternate.value_or(second_c);
        theme.colors.at(ImGuiCol_PlotLines)            = rgb(second_c);
        theme.colors.at(ImGuiCol_PlotLinesHovered)     = accent_hover;
        theme.colors.at(ImGuiCol_PlotHistogram)        = rgb(plot_color);
        theme.colors.at(ImGuiCol_PlotHistogramHovered) = accent_hover;

        // Nav highlight
        theme.colors.at(ImGuiCol_NavHighlight)          = accent;
        theme.colors.at(ImGuiCol_NavWindowingHighlight) = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
        theme.colors.at(ImGuiCol_NavWindowingDimBg)     = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);

        // Table colors
        theme.colors.at(ImGuiCol_TableHeaderBg)     = bg_light;
        theme.colors.at(ImGuiCol_TableBorderStrong) = offset(bg_mid_c, d * 0.14f);
        theme.colors.at(ImGuiCol_TableBorderLight)  = offset(bg_mid_c, d * 0.08f, 0.8f);
        theme.colors.at(ImGuiCol_TableRowBg)        = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        theme.colors.at(ImGuiCol_TableRowBgAlt)     = rgb(bg_mid_c, 0.4f);

        // Drag/drop
        theme.colors.at(ImGuiCol_DragDropTarget) = accent;

        // Modal
        theme.colors.at(ImGuiCol_ModalWindowDimBg) = ImVec4(0.0f, 0.0f, 0.0f, 0.6f);

        // Text link
        theme.colors.at(ImGuiCol_TextLink) = accent;

        // ====================================================================
        // ImNodes colors — always use dark preset values (node canvas stays dark)
        // ====================================================================

        theme.node_colors.at(ImNodesCol_NodeBackground) = preset.node_background.value_or(IM_COL32(32, 32, 38, 245));
        theme.node_colors.at(ImNodesCol_NodeBackgroundHovered) =
            preset.node_background_hovered.value_or(IM_COL32(42, 42, 48, 255));
        theme.node_colors.at(ImNodesCol_NodeBackgroundSelected) =
            preset.node_background_selected.value_or(IM_COL32(50, 55, 70, 255));
        theme.node_colors.at(ImNodesCol_NodeOutline)      = preset.node_outline.value_or(IM_COL32(60, 60, 68, 255));
        theme.node_colors.at(ImNodesCol_TitleBar)         = preset.node_title_bar;
        theme.node_colors.at(ImNodesCol_TitleBarHovered)  = preset.node_title_bar_hovered;
        theme.node_colors.at(ImNodesCol_TitleBarSelected) = preset.node_title_bar_selected;
        theme.node_colors.at(ImNodesCol_Link)             = preset.node_link;
        theme.node_colors.at(ImNodesCol_LinkHovered)      = preset.node_link_hovered;
        theme.node_colors.at(ImNodesCol_LinkSelected)     = preset.node_title_bar_selected;
        theme.node_colors.at(ImNodesCol_Pin)              = preset.node_pin;
        theme.node_colors.at(ImNodesCol_PinHovered)       = preset.node_pin_hovered;
        theme.node_colors.at(ImNodesCol_BoxSelector)      = (preset.node_title_bar_selected & 0x00FFFFFF) | (40u << 24);

        // Grid - derive line colors from background
        theme.node_colors.at(ImNodesCol_GridBackground)  = preset.node_grid_bg;
        theme.node_colors.at(ImNodesCol_GridLine)        = offset_u32_rgb(preset.node_grid_bg, 16, 120);
        theme.node_colors.at(ImNodesCol_GridLinePrimary) = offset_u32_rgb(preset.node_grid_bg, 26, 180);

        // Minimap
        theme.node_colors.at(ImNodesCol_MiniMapBackground)     = offset_u32_rgb(preset.node_grid_bg, -4, 220);
        theme.node_colors.at(ImNodesCol_MiniMapNodeBackground) = preset.node_title_bar;
        theme.node_colors.at(ImNodesCol_MiniMapNodeOutline)    = IM_COL32(40, 40, 48, 255);
        theme.node_colors.at(ImNodesCol_MiniMapLink)           = (preset.node_link & 0x00FFFFFFu) | (180u << 24);

        return theme;
    }

    // Component-wise lerp for ImVec4 (t=0 returns a, t=1 returns b)
    constexpr ImVec4 lerp_vec4(const ImVec4 a, const ImVec4 b, const float t) noexcept {
        return {a.x + ((b.x - a.x) * t), a.y + ((b.y - a.y) * t), a.z + ((b.z - a.z) * t), a.w + ((b.w - a.w) * t)};
    }

    // Interpolate between two theme configs element-wise
    [[nodiscard]]
    constexpr theme_config lerp(const theme_config &a, const theme_config &b, const float t) {
        using color::float4_to_u32;
        using color::u32_to_float4;

        theme_config result{};
        result.name = a.name;

        // Lerp ImGui colors
        for (int i = 0; i < ImGuiCol_COUNT; i++) {
            result.colors.at(i) = lerp_vec4(a.colors.at(i), b.colors.at(i), t);
        }

        // Lerp ImNodes colors (component-wise on packed U32)
        for (int i = 0; i < ImNodesCol_COUNT; i++) {
            result.node_colors.at(i) =
                float4_to_u32(lerp_vec4(u32_to_float4(a.node_colors.at(i)), u32_to_float4(b.node_colors.at(i)), t));
        }

        // Lerp style floats via constexpr table
        const float s = 1.0f - t;
        for (const auto &[name, ptr]: theme_float_fields) {
            result.*ptr = ((a.*ptr) * s) + ((b.*ptr) * t);
        }

        // Lerp preset RGB fields via constexpr table
        for (const auto &[name, ptr]: theme_rgb_fields) {
            for (int j = 0; j < 3; j++) {
                (result.*ptr).channels.at(j) = ((a.*ptr).channels.at(j) * s) + ((b.*ptr).channels.at(j) * t);
            }
        }

        return result;
    }

} // namespace imgui_util::theme
