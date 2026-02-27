#include "imgui_util/theme/theme_manager.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <log.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "imgui_util/core/parse.hpp"
#include "imgui_util/core/raii.hpp"
#include "imgui_util/theme/color_math.hpp"

#include "imgui_internal.h"

namespace imgui_util::theme {

    using color::rgb_color;

    // ============================================================================
    // Theme Preset Data - All theme definitions in one place
    // ============================================================================

    static constexpr std::array theme_presets =
        std::to_array<theme_preset>(
            {
                // CrueltySquad - Industrial theme with blue/teal accents (dark + light)
                {
                    .name                     = "CrueltySquad",
                    .bg_dark                  = {{{0.10f, 0.10f, 0.12f}}},
                    .bg_mid                   = {{{0.14f, 0.14f, 0.16f}}},
                    .accent                   = {{{0.45f, 0.55f, 0.90f}}},
                    .secondary                = {{{0.30f, 0.75f, 0.70f}}},
                    .alternate                = std::nullopt,
                    .text                     = std::nullopt,
                    .node_title_bar           = IM_COL32(75, 90, 140, 255),
                    .node_title_bar_hovered   = IM_COL32(95, 115, 170, 255),
                    .node_title_bar_selected  = IM_COL32(115, 140, 230, 255),
                    .node_link                = IM_COL32(75, 190, 180, 220),
                    .node_link_hovered        = IM_COL32(100, 220, 210, 255),
                    .node_pin                 = IM_COL32(75, 190, 180, 255),
                    .node_pin_hovered         = IM_COL32(100, 220, 210, 255),
                    .node_grid_bg             = IM_COL32(22, 22, 26, 255),
                    .node_background          = {},
                    .node_background_hovered  = {},
                    .node_background_selected = {},
                    .node_outline             = {},
                    .light =
                        theme_preset::light_overrides{
                            .bg_dark   = {{{0.85f, 0.85f, 0.88f}}},
                            .bg_mid    = {{{0.92f, 0.92f, 0.94f}}},
                            .accent    = {},
                            .secondary = {},
                            .text      = rgb_color{{{0.10f, 0.10f, 0.12f}}},
                        },
                },

                // Gruvbox - Warm retro palette (dark + light)
                {
                    .name                     = "Gruvbox",
                    .bg_dark                  = {{{0.157f, 0.157f, 0.157f}}},
                    .bg_mid                   = {{{0.235f, 0.220f, 0.212f}}},
                    .accent                   = {{{0.843f, 0.600f, 0.129f}}},
                    .secondary                = {{{0.408f, 0.616f, 0.416f}}},
                    .alternate                = std::nullopt,
                    .text                     = rgb_color{{{0.922f, 0.859f, 0.698f}}},
                    .node_title_bar           = IM_COL32(140, 100, 21, 255),
                    .node_title_bar_hovered   = IM_COL32(172, 122, 26, 255),
                    .node_title_bar_selected  = IM_COL32(215, 153, 33, 255),
                    .node_link                = IM_COL32(104, 157, 106, 220),
                    .node_link_hovered        = IM_COL32(134, 187, 136, 255),
                    .node_pin                 = IM_COL32(104, 157, 106, 255),
                    .node_pin_hovered         = IM_COL32(134, 187, 136, 255),
                    .node_grid_bg             = IM_COL32(30, 30, 30, 255),
                    .node_background          = {},
                    .node_background_hovered  = {},
                    .node_background_selected = {},
                    .node_outline             = {},
                    .light =
                        theme_preset::light_overrides{
                            .bg_dark   = {{{0.922f, 0.859f, 0.698f}}}, // #ebdbb2
                            .bg_mid    = {{{0.984f, 0.945f, 0.780f}}}, // #fbf1c7
                            .accent    = {},
                            .secondary = {},
                            .text      = rgb_color{{{0.235f, 0.220f, 0.212f}}}, // #3c3836
                        },
                },

                // Dracula - Purple/pink palette (dark + light)
                {
                    .name                     = "Dracula",
                    .bg_dark                  = {{{0.157f, 0.165f, 0.212f}}},
                    .bg_mid                   = {{{0.267f, 0.278f, 0.353f}}},
                    .accent                   = {{{0.741f, 0.576f, 0.976f}}},
                    .secondary                = {{{1.000f, 0.475f, 0.776f}}},
                    .alternate                = std::nullopt,
                    .text                     = rgb_color{{{0.973f, 0.973f, 0.949f}}},
                    .node_title_bar           = IM_COL32(123, 96, 162, 255),
                    .node_title_bar_hovered   = IM_COL32(151, 118, 199, 255),
                    .node_title_bar_selected  = IM_COL32(189, 147, 249, 255),
                    .node_link                = IM_COL32(255, 121, 198, 220),
                    .node_link_hovered        = IM_COL32(255, 151, 218, 255),
                    .node_pin                 = IM_COL32(255, 121, 198, 255),
                    .node_pin_hovered         = IM_COL32(255, 151, 218, 255),
                    .node_grid_bg             = IM_COL32(30, 32, 44, 255),
                    .node_background          = {},
                    .node_background_hovered  = {},
                    .node_background_selected = {},
                    .node_outline             = {},
                    .light =
                        theme_preset::light_overrides{
                            .bg_dark   = {{{0.910f, 0.910f, 0.886f}}}, // #e8e8e2
                            .bg_mid    = {{{0.973f, 0.973f, 0.949f}}}, // #f8f8f2
                            .accent    = {},
                            .secondary = {},
                            .text      = rgb_color{{{0.157f, 0.165f, 0.212f}}}, // #282a36
                        },
                },

                // Nord - Arctic frost-blue palette (dark + light)
                {
                    .name                     = "Nord",
                    .bg_dark                  = {{{0.180f, 0.204f, 0.251f}}},
                    .bg_mid                   = {{{0.231f, 0.259f, 0.322f}}},
                    .accent                   = {{{0.533f, 0.753f, 0.816f}}},
                    .secondary                = {{{0.639f, 0.745f, 0.549f}}},
                    .alternate                = std::nullopt,
                    .text                     = rgb_color{{{0.847f, 0.871f, 0.914f}}},
                    .node_title_bar           = IM_COL32(88, 125, 135, 255),
                    .node_title_bar_hovered   = IM_COL32(109, 154, 166, 255),
                    .node_title_bar_selected  = IM_COL32(136, 192, 208, 255),
                    .node_link                = IM_COL32(163, 190, 140, 220),
                    .node_link_hovered        = IM_COL32(183, 210, 170, 255),
                    .node_pin                 = IM_COL32(163, 190, 140, 255),
                    .node_pin_hovered         = IM_COL32(183, 210, 170, 255),
                    .node_grid_bg             = IM_COL32(36, 42, 54, 255),
                    .node_background          = {},
                    .node_background_hovered  = {},
                    .node_background_selected = {},
                    .node_outline             = {},
                    .light =
                        theme_preset::light_overrides{
                            .bg_dark   = {{{0.898f, 0.914f, 0.941f}}},          // #E5E9F0
                            .bg_mid    = {{{0.925f, 0.937f, 0.957f}}},          // #ECEFF4
                            .accent    = rgb_color{{{0.369f, 0.506f, 0.675f}}}, // #5E81AC
                            .secondary = {},
                            .text      = rgb_color{{{0.180f, 0.204f, 0.251f}}}, // #2E3440
                        },
                },

                // Catppuccin - Pastel mauve/blue palette (Mocha dark + Latte light)
                {
                    .name                     = "Catppuccin",
                    .bg_dark                  = {{{0.118f, 0.118f, 0.180f}}},
                    .bg_mid                   = {{{0.192f, 0.196f, 0.267f}}},
                    .accent                   = {{{0.796f, 0.651f, 0.969f}}},
                    .secondary                = {{{0.537f, 0.706f, 0.980f}}},
                    .alternate                = std::nullopt,
                    .text                     = rgb_color{{{0.804f, 0.839f, 0.957f}}},
                    .node_title_bar           = IM_COL32(132, 108, 161, 255),
                    .node_title_bar_hovered   = IM_COL32(162, 133, 198, 255),
                    .node_title_bar_selected  = IM_COL32(203, 166, 247, 255),
                    .node_link                = IM_COL32(137, 180, 250, 220),
                    .node_link_hovered        = IM_COL32(167, 200, 255, 255),
                    .node_pin                 = IM_COL32(137, 180, 250, 255),
                    .node_pin_hovered         = IM_COL32(167, 200, 255, 255),
                    .node_grid_bg             = IM_COL32(22, 22, 36, 255),
                    .node_background          = {},
                    .node_background_hovered  = {},
                    .node_background_selected = {},
                    .node_outline             = {},
                    .light =
                        theme_preset::light_overrides{
                            .bg_dark   = {{{0.902f, 0.914f, 0.937f}}},          // #E6E9EF Mantle
                            .bg_mid    = {{{0.937f, 0.945f, 0.961f}}},          // #EFF1F5 Base
                            .accent    = rgb_color{{{0.533f, 0.224f, 0.937f}}}, // #8839EF Latte Mauve
                            .secondary = rgb_color{{{0.118f, 0.400f, 0.961f}}}, // #1E66F5 Latte Blue
                            .text      = rgb_color{{{0.298f, 0.310f, 0.412f}}}, // #4C4F69 Latte Text
                        },
                },

                // Solarized - Ethan Schoonover's palette (dark + light)
                {
                    .name                     = "Solarized",
                    .bg_dark                  = {{{0.000f, 0.169f, 0.212f}}},
                    .bg_mid                   = {{{0.027f, 0.212f, 0.259f}}},
                    .accent                   = {{{0.149f, 0.545f, 0.824f}}},
                    .secondary                = {{{0.165f, 0.631f, 0.596f}}},
                    .alternate                = std::nullopt,
                    .text                     = rgb_color{{{0.514f, 0.580f, 0.588f}}},
                    .node_title_bar           = IM_COL32(25, 90, 137, 255),
                    .node_title_bar_hovered   = IM_COL32(30, 111, 168, 255),
                    .node_title_bar_selected  = IM_COL32(38, 139, 210, 255),
                    .node_link                = IM_COL32(42, 161, 152, 220),
                    .node_link_hovered        = IM_COL32(72, 191, 182, 255),
                    .node_pin                 = IM_COL32(42, 161, 152, 255),
                    .node_pin_hovered         = IM_COL32(72, 191, 182, 255),
                    .node_grid_bg             = IM_COL32(0, 33, 44, 255),
                    .node_background          = {},
                    .node_background_hovered  = {},
                    .node_background_selected = {},
                    .node_outline             = {},
                    .light =
                        theme_preset::light_overrides{
                            .bg_dark   = {{{0.933f, 0.910f, 0.835f}}}, // #EEE8D5 base2
                            .bg_mid    = {{{0.992f, 0.965f, 0.890f}}}, // #FDF6E3 base3
                            .accent    = {},
                            .secondary = {},
                            .text      = rgb_color{{{0.396f, 0.482f, 0.514f}}}, // #657B83 base00
                        },
                },
            });

    // Node color name table for serialization
    static constexpr std::array<std::string_view, ImNodesCol_COUNT> node_color_names = {
        "NodeBackground",
        "NodeBackgroundHovered",
        "NodeBackgroundSelected",
        "NodeOutline",
        "TitleBar",
        "TitleBarHovered",
        "TitleBarSelected",
        "Link",
        "LinkHovered",
        "LinkSelected",
        "Pin",
        "PinHovered",
        "BoxSelector",
        "BoxSelectorOutline",
        "GridBackground",
        "GridLine",
        "GridLinePrimary",
        "MiniMapBackground",
        "MiniMapBackgroundHovered",
        "MiniMapOutline",
        "MiniMapOutlineHovered",
        "MiniMapNodeBackground",
        "MiniMapNodeBackgroundHovered",
        "MiniMapNodeBackgroundSelected",
        "MiniMapNodeOutline",
        "MiniMapLink",
        "MiniMapLinkSelected",
        "MiniMapCanvas",
        "MiniMapCanvasOutline",
    };
    static_assert(std::size(node_color_names) == ImNodesCol_COUNT);

    // ============================================================================
    // Explicit color index arrays per editor category (no sequential enum assumption)
    // ============================================================================

    static constexpr auto window_color_indices   = std::to_array<int>({
        ImGuiCol_WindowBg,
        ImGuiCol_ChildBg,
        ImGuiCol_PopupBg,
    });
    static constexpr auto title_bar_indices      = std::to_array<int>({
        ImGuiCol_TitleBg,
        ImGuiCol_TitleBgActive,
        ImGuiCol_TitleBgCollapsed,
    });
    static constexpr auto menu_bar_indices       = std::to_array<int>({
        ImGuiCol_MenuBarBg,
    });
    static constexpr auto border_indices         = std::to_array<int>({
        ImGuiCol_Border,
        ImGuiCol_BorderShadow,
    });
    static constexpr auto frame_indices          = std::to_array<int>({
        ImGuiCol_FrameBg,
        ImGuiCol_FrameBgHovered,
        ImGuiCol_FrameBgActive,
    });
    static constexpr auto button_indices         = std::to_array<int>({
        ImGuiCol_Button,
        ImGuiCol_ButtonHovered,
        ImGuiCol_ButtonActive,
    });
    static constexpr auto header_indices         = std::to_array<int>({
        ImGuiCol_Header,
        ImGuiCol_HeaderHovered,
        ImGuiCol_HeaderActive,
    });
    static constexpr auto slider_indices         = std::to_array<int>({
        ImGuiCol_SliderGrab,
        ImGuiCol_SliderGrabActive,
    });
    static constexpr auto checkmark_indices      = std::to_array<int>({
        ImGuiCol_CheckMark,
    });
    static constexpr auto tab_indices            = std::to_array<int>({
        ImGuiCol_Tab,
        ImGuiCol_TabHovered,
        ImGuiCol_TabSelected,
        ImGuiCol_TabSelectedOverline,
        ImGuiCol_TabDimmed,
        ImGuiCol_TabDimmedSelected,
        ImGuiCol_TabDimmedSelectedOverline,
    });
    static constexpr auto docking_indices        = std::to_array<int>({
        ImGuiCol_DockingPreview,
        ImGuiCol_DockingEmptyBg,
    });
    static constexpr auto text_indices           = std::to_array<int>({
        ImGuiCol_Text,
        ImGuiCol_TextDisabled,
        ImGuiCol_TextSelectedBg,
    });
    static constexpr auto plot_lines_indices     = std::to_array<int>({
        ImGuiCol_PlotLines,
        ImGuiCol_PlotLinesHovered,
    });
    static constexpr auto plot_histogram_indices = std::to_array<int>({
        ImGuiCol_PlotHistogram,
        ImGuiCol_PlotHistogramHovered,
    });
    static constexpr auto scrollbar_indices      = std::to_array<int>({
        ImGuiCol_ScrollbarBg,
        ImGuiCol_ScrollbarGrab,
        ImGuiCol_ScrollbarGrabHovered,
        ImGuiCol_ScrollbarGrabActive,
    });
    static constexpr auto resize_grip_indices    = std::to_array<int>({
        ImGuiCol_ResizeGrip,
        ImGuiCol_ResizeGripHovered,
        ImGuiCol_ResizeGripActive,
    });

    // ============================================================================
    // theme_manager implementation
    // ============================================================================

    theme_manager::theme_manager() :
        current_theme_(theme_config::from_preset(theme_presets.at(0))), editing_theme_(current_theme_) {}

    void theme_manager::set_theme(theme_config theme) {
        current_theme_ = std::move(theme);
        current_theme_.apply();
        Log::info("Theme", "applied '", current_theme_.name, "'");
    }

    const theme_preset *theme_manager::find_preset(const std::string_view name) {
        const auto *it = std::ranges::find(theme_presets, name, &theme_preset::name);
        return it != theme_presets.end() ? it : nullptr;
    }

    std::span<const theme_preset> theme_manager::get_presets() {
        return theme_presets;
    }

    theme_config theme_manager::get_preset(const std::string_view name) {
        if (const theme_preset *const preset = find_preset(name)) {
            return theme_config::from_preset(*preset);
        }
        return theme_config::from_preset(theme_presets.at(0));
    }

    // ============================================================================
    // Persistence
    // ============================================================================

    bool theme_manager::save_to_file(const std::filesystem::path &path) {
        std::ofstream file(path);
        if (!file.is_open()) {
            Log::error("Theme", "save failed: could not open ", path.c_str());
            return false;
        }

        file << "version=" << file_version << "\n";
        file << "name=" << current_theme_.name << "\n";

        // Save style floats via constexpr member-pointer table (lossless round-trip)
        for (const auto &[name, ptr]: theme_float_fields) {
            file << name << "=" << std::format("{:.9g}", current_theme_.*ptr) << "\n";
        }

        // Save preset RGB fields for round-trip fidelity
        for (const auto &[name, ptr]: theme_rgb_fields) {
            const auto &[r, g, b] = (current_theme_.*ptr).channels;
            file << name << "=" << std::format("{:.9g},{:.9g},{:.9g}", r, g, b) << "\n";
        }

        // Save optional preset RGB fields
        for (const auto &[name, ptr]: theme_opt_rgb_fields) {
            if (const auto &opt = current_theme_.*ptr) {
                const auto &[r, g, b] = opt->channels;
                file << name << "=" << std::format("{:.9g},{:.9g},{:.9g}", r, g, b) << "\n";
            }
        }

        // Save ImGui colors using named keys
        for (int i = 0; i < ImGuiCol_COUNT; i++) {
            const ImVec4 &c = current_theme_.colors.at(i);
            file << ImGui::GetStyleColorName(i) << "=" << std::format("{:.9g},{:.9g},{:.9g},{:.9g}", c.x, c.y, c.z, c.w)
                 << "\n";
        }

        // Save ImNodes colors using named keys
        for (int i = 0; i < ImNodesCol_COUNT; i++) {
            file << "node_" << node_color_names.at(i) << "=" << current_theme_.node_colors.at(i) << "\n";
        }

        Log::info("Theme", "saved '", current_theme_.name, "' to ", path.c_str());
        return true;
    }

    bool theme_manager::load_from_file(const std::filesystem::path &path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            Log::error("Theme", "load failed: could not open ", path.c_str());
            return false;
        }

        // Build O(1) dispatch map for all known field keys.
        enum class field_kind : uint8_t { float_field, rgb_field, opt_rgb_field, imgui_color, node_color };

        struct field_entry {
            field_kind kind;
            int        index; // index into the corresponding constexpr table or color array
        };

        // Full dispatch map including node keys — built once since all key strings are
        // composed from static constexpr names (node_color_names) and ImGui style color names.
        static const auto field_map = [] {
            // Owned storage for "node_" prefixed keys so string_views remain valid.
            static std::array<std::string, ImNodesCol_COUNT> node_keys = [] {
                std::array<std::string, ImNodesCol_COUNT> keys;
                for (int i = 0; i < ImNodesCol_COUNT; i++)
                    keys[i] = std::string("node_") + std::string(node_color_names[i]);
                return keys;
            }();

            std::unordered_map<std::string_view, field_entry> map;
            map.reserve(static_cast<size_t>(ImGuiCol_COUNT) + static_cast<size_t>(ImNodesCol_COUNT)
                        + theme_float_fields.size() + theme_rgb_fields.size() + theme_opt_rgb_fields.size());
            for (int i = 0; std::cmp_less(i, theme_float_fields.size()); i++)
                map.emplace(theme_float_fields[i].name, field_entry{.kind = field_kind::float_field, .index = i});
            for (int i = 0; std::cmp_less(i, theme_rgb_fields.size()); i++)
                map.emplace(theme_rgb_fields[i].name, field_entry{.kind = field_kind::rgb_field, .index = i});
            for (int i = 0; std::cmp_less(i, theme_opt_rgb_fields.size()); i++)
                map.emplace(theme_opt_rgb_fields[i].name, field_entry{.kind = field_kind::opt_rgb_field, .index = i});
            for (int i = 0; i < ImGuiCol_COUNT; i++)
                map.emplace(ImGui::GetStyleColorName(i), field_entry{.kind = field_kind::imgui_color, .index = i});
            for (int i = 0; i < ImNodesCol_COUNT; i++)
                map.emplace(std::string_view(node_keys[i]), field_entry{.kind = field_kind::node_color, .index = i});
            return map;
        }();

        std::string line;
        bool        version_found = false;
        try {
            while (std::getline(file, line)) {
                const size_t eq = line.find('=');
                if (eq == std::string::npos) continue;

                const std::string_view key(line.data(), eq);
                const std::string_view value(line.data() + eq + 1, line.size() - eq - 1);

                if (key == "version") {
                    version_found = true;
                    if (const int v = parse::parse_int(value, -1); v != file_version) {
                        Log::warning("Theme", "file version ", v, " differs from expected ", file_version);
                    }
                    continue;
                }

                if (key == "name") {
                    current_theme_.name = std::string(value);
                    continue;
                }

                // O(1) dispatch for all known fields
                if (auto it = field_map.find(key); it != field_map.end()) {
                    switch (const auto &[kind, idx] = it->second; kind) {
                        case field_kind::float_field:
                            current_theme_.*theme_float_fields[idx].ptr =
                                parse::parse_float(value, current_theme_.*theme_float_fields[idx].ptr);
                            break;
                        case field_kind::rgb_field:
                            parse::parse_float_rgb(value,
                                                   std::span{(current_theme_.*theme_rgb_fields[idx].ptr).channels});
                            break;
                        case field_kind::opt_rgb_field: {
                            rgb_color c{};
                            parse::parse_float_rgb(value, std::span{c.channels});
                            current_theme_.*theme_opt_rgb_fields[idx].ptr = c;
                            break;
                        }
                        case field_kind::imgui_color:
                            current_theme_.colors.at(idx) = parse::parse_vec4(value);
                            break;
                        case field_kind::node_color:
                            current_theme_.node_colors.at(idx) = parse::parse_u32(value);
                            break;
                    }
                    continue;
                }

                // Backward compat: positional "color0=..." format
                if (key.starts_with("color") && key.size() > 5 && key[5] >= '0' && key[5] <= '9') {
                    if (const int idx = parse::parse_int(key.substr(5), -1); idx >= 0 && idx < ImGuiCol_COUNT) {
                        current_theme_.colors.at(idx) = parse::parse_vec4(value);
                    }
                }
                // Backward compat: positional "nodeColor0=..." format
                else if (key.starts_with("nodeColor") && key.size() > 9) {
                    if (const int idx = parse::parse_int(key.substr(9), -1); idx >= 0 && idx < ImNodesCol_COUNT) {
                        current_theme_.node_colors.at(idx) = parse::parse_u32(value);
                    }
                }
            }
        } catch (const std::exception &e) {
            Log::error("Theme", "load failed: parse error in ", path.c_str(), ": ", e.what());
            current_theme_ = theme_config::from_preset(theme_presets.at(0));
            return false;
        }

        if (!version_found) {
            Log::warning("Theme", "no version line in ", path.c_str(), "; assuming version ", file_version);
        }

        current_theme_.apply();
        editing_theme_ = current_theme_;
        Log::info("Theme", "loaded '", current_theme_.name, "' from ", path.c_str());
        return true;
    }

    void theme_manager::apply_preset(const std::string_view name) {
        const auto *preset = find_preset(name);
        if (!preset) {
            Log::warning("Theme", "preset '", name, "' not found");
            return;
        }
        set_theme(theme_config::from_preset(*preset));
    }

    // ============================================================================
    // Theme Editor UI
    // ============================================================================

    void theme_manager::render_theme_editor(bool *open) {
        ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
        if (const window w{"Theme Editor", open}; !w) return;

        // Detect light/dark mode from current WindowBg luminance
        const ImVec4 &win_bg = editing_theme_.colors.at(ImGuiCol_WindowBg);
        const auto    dir    = color::luminance(win_bg) > 0.5f ? theme_mode::light : theme_mode::dark;

        // ImGui builtin style presets (set all colors directly)
        auto apply_builtin = [&](const char *name, void (*fn)(ImGuiStyle *)) {
            ImGuiStyle tmp;
            fn(&tmp);
            editing_theme_.name = name;
            std::ranges::copy(std::span(tmp.Colors, ImGuiCol_COUNT), editing_theme_.colors.begin());
            if (live_preview_) editing_theme_.apply();
        };

        ImGui::TextUnformatted("Presets:");
        ImGui::SameLine();
        if (ImGui::Button("Dark")) apply_builtin("Dark", ImGui::StyleColorsDark);
        ImGui::SameLine();
        if (ImGui::Button("Light")) apply_builtin("Light", ImGui::StyleColorsLight);
        ImGui::SameLine();
        if (ImGui::Button("Classic")) apply_builtin("Classic", ImGui::StyleColorsClassic);
        ImGui::SameLine();
        ImGui::TextUnformatted("|");
        ImGui::SameLine();
        for (size_t i = 0; i < theme_presets.size(); i++) {
            if (i > 0) ImGui::SameLine();
            const auto &preset     = theme_presets.at(i);
            const auto  preset_dir = preset.has_light() ? dir : theme_mode::dark;
            if (ImGui::Button(preset.name.data())) { // NOLINT(bugprone-not-null-terminated-result) — string literal
                editing_theme_ = theme_config::from_preset(preset, preset_dir);
                if (live_preview_) editing_theme_.apply();
            }
        }

        ImGui::Separator();
        ImGui::Checkbox("Live Preview", &live_preview_);
        ImGui::SameLine();
        if (ImGui::Button("Apply")) {
            current_theme_ = editing_theme_;
            current_theme_.apply();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            editing_theme_ = current_theme_;
            if (live_preview_) editing_theme_.apply();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy as C++ Preset")) {
            ImGui::SetClipboardText(generate_preset_code(editing_theme_).c_str());
        }

        ImGui::Separator();

        if (const tab_bar tb{"ThemeCategories"}) {
            if (const tab_item ti{"Sizes"}) {
                render_sizes_tab();
            }
            if (const tab_item ti{"Colors"}) {
                color_filter_.Draw("Filter Colors##filter", -FLT_MIN);
                ImGui::Spacing();
                render_color_category("Window Colors", window_color_indices);
                render_color_category("Title Bar", title_bar_indices);
                render_color_category("Menu Bar", menu_bar_indices);
                render_color_category("Borders", border_indices);
                render_color_category("Frame", frame_indices);
                render_color_category("Buttons", button_indices);
                render_color_category("Headers", header_indices);
                render_color_category("Sliders", slider_indices);
                render_color_category("Check Mark", checkmark_indices);
                render_color_category("Tabs", tab_indices);
                render_color_category("Docking", docking_indices);
                render_color_category("Text", text_indices);
                render_color_category("Plot Lines", plot_lines_indices);
                render_color_category("Plot Histogram", plot_histogram_indices);
                render_color_category("Scrollbar", scrollbar_indices);
                render_color_category("Resize Grip", resize_grip_indices);
            }
            if (const tab_item ti{"Fonts"}) {
                render_fonts_tab();
            }
            if (const tab_item ti{"Rendering"}) {
                render_rendering_tab();
            }
            if (const tab_item ti{"Node Editor"}) {
                render_node_colors();
            }
        }
    }

    void theme_manager::render_color_category(const char *name, const std::span<const int> indices) {
        // If a filter is active, check if any color in this category passes
        if (color_filter_.IsActive()) {
            bool any_visible = false;
            for (const int idx: indices) {
                if (idx < ImGuiCol_COUNT && color_filter_.PassFilter(ImGui::GetStyleColorName(idx))) {
                    any_visible = true;
                    break;
                }
            }
            if (!any_visible) return;
        }

        if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const int idx: indices) {
                if (idx < ImGuiCol_COUNT) {
                    const char *const color_name = ImGui::GetStyleColorName(idx);
                    if (color_filter_.IsActive() && !color_filter_.PassFilter(color_name)) continue;
                    if (ImGui::ColorEdit4(color_name, &editing_theme_.colors.at(idx).x, ImGuiColorEditFlags_AlphaBar)) {
                        if (live_preview_) {
                            ImGui::GetStyle().Colors[idx] = editing_theme_.colors.at(idx);
                        }
                    }
                }
            }
        }
    }

    void theme_manager::render_node_colors() {
        ImGui::SliderFloat("Node Corner Rounding", &editing_theme_.node_corner_rounding, 0.0f, 12.0f);
        ImGui::SliderFloat("Link Thickness", &editing_theme_.link_thickness, 1.0f, 5.0f);
        ImGui::SliderFloat("Pin Circle Radius", &editing_theme_.pin_circle_radius, 2.0f, 8.0f);

        if (live_preview_) {
            ImNodesStyle &node_style      = ImNodes::GetStyle();
            node_style.NodeCornerRounding = editing_theme_.node_corner_rounding;
            node_style.LinkThickness      = editing_theme_.link_thickness;
            node_style.PinCircleRadius    = editing_theme_.pin_circle_radius;
        }

        ImGui::Separator();

        for (int i = 0; i < ImNodesCol_COUNT; i++) {
            ImVec4 col = ImGui::ColorConvertU32ToFloat4(editing_theme_.node_colors.at(i));
            if (ImGui::ColorEdit4(
                    node_color_names.at(i).data(), &col.x,
                    ImGuiColorEditFlags_AlphaBar)) { // NOLINT(bugprone-not-null-terminated-result) — string literal
                editing_theme_.node_colors.at(i) = ImGui::ColorConvertFloat4ToU32(col);
                if (live_preview_) {
                    ImNodes::GetStyle().Colors[i] = editing_theme_.node_colors.at(i);
                }
            }
        }
    }

    void theme_manager::render_sizes_tab() {
        ImGuiStyle &style = ImGui::GetStyle();

        if (ImGui::CollapsingHeader("Main", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Alpha", &style.Alpha, 0.20f, 1.0f, "%.2f");
            ImGui::SliderFloat("DisabledAlpha", &style.DisabledAlpha, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat2("WindowPadding", &style.WindowPadding.x, 0.5f, 0.0f, 40.0f, "%.0f");
            ImGui::DragFloat2("WindowMinSize", &style.WindowMinSize.x, 1.0f, 1.0f, 500.0f, "%.0f");
            ImGui::DragFloat2("FramePadding", &style.FramePadding.x, 0.5f, 0.0f, 40.0f, "%.0f");
            ImGui::DragFloat2("ItemSpacing", &style.ItemSpacing.x, 0.5f, 0.0f, 40.0f, "%.0f");
            ImGui::DragFloat2("ItemInnerSpacing", &style.ItemInnerSpacing.x, 0.5f, 0.0f, 40.0f, "%.0f");
            ImGui::SliderFloat("IndentSpacing", &style.IndentSpacing, 0.0f, 50.0f, "%.0f");
            ImGui::SliderFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 30.0f, "%.0f");
            ImGui::SliderFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 30.0f, "%.0f");
        }

        if (ImGui::CollapsingHeader("Borders")) {
            ImGui::SliderFloat("WindowBorderSize", &style.WindowBorderSize, 0.0f, 2.0f, "%.1f");
            ImGui::SliderFloat("ChildBorderSize", &style.ChildBorderSize, 0.0f, 2.0f, "%.1f");
            ImGui::SliderFloat("PopupBorderSize", &style.PopupBorderSize, 0.0f, 2.0f, "%.1f");
            ImGui::SliderFloat("FrameBorderSize", &style.FrameBorderSize, 0.0f, 2.0f, "%.1f");
            ImGui::SliderFloat("TabBorderSize", &style.TabBorderSize, 0.0f, 2.0f, "%.1f");
            ImGui::SliderFloat("TabBarBorderSize", &style.TabBarBorderSize, 0.0f, 2.0f, "%.1f");
            ImGui::SliderFloat("TabBarOverlineSize", &style.TabBarOverlineSize, 0.0f, 3.0f, "%.1f");
            ImGui::SliderFloat("SeparatorTextBorderSize", &style.SeparatorTextBorderSize, 0.0f, 10.0f, "%.0f");
        }

        if (ImGui::CollapsingHeader("Rounding")) {
            ImGui::SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 14.0f, "%.0f");
            ImGui::SliderFloat("ChildRounding", &style.ChildRounding, 0.0f, 14.0f, "%.0f");
            ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 14.0f, "%.0f");
            ImGui::SliderFloat("PopupRounding", &style.PopupRounding, 0.0f, 14.0f, "%.0f");
            ImGui::SliderFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.0f, 14.0f, "%.0f");
            ImGui::SliderFloat("GrabRounding", &style.GrabRounding, 0.0f, 14.0f, "%.0f");
            ImGui::SliderFloat("TabRounding", &style.TabRounding, 0.0f, 14.0f, "%.0f");
        }

        if (ImGui::CollapsingHeader("Tables")) {
            ImGui::DragFloat2("CellPadding", &style.CellPadding.x, 0.5f, 0.0f, 40.0f, "%.0f");
            ImGui::SliderFloat("TableAngledHeadersAngle", &style.TableAngledHeadersAngle, -50.0f, 50.0f, "%.0f deg");
            ImGui::DragFloat2("TableAngledHeadersTextAlign", &style.TableAngledHeadersTextAlign.x, 0.01f, 0.0f, 1.0f,
                              "%.2f");
        }

        if (ImGui::CollapsingHeader("Widgets")) {
            ImGui::DragFloat2("WindowTitleAlign", &style.WindowTitleAlign.x, 0.01f, 0.0f, 1.0f, "%.2f");
            {
                int dir = style.ColorButtonPosition;
                if (ImGui::Combo("ColorButtonPosition", &dir, "Left\0Right\0")) {
                    style.ColorButtonPosition = static_cast<ImGuiDir>(dir);
                }
            }
            ImGui::DragFloat2("ButtonTextAlign", &style.ButtonTextAlign.x, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat2("SelectableTextAlign", &style.SelectableTextAlign.x, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat2("SeparatorTextAlign", &style.SeparatorTextAlign.x, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat2("SeparatorTextPadding", &style.SeparatorTextPadding.x, 1.0f, 0.0f, 100.0f, "%.0f");
            ImGui::SliderFloat("LogSliderDeadzone", &style.LogSliderDeadzone, 0.0f, 12.0f, "%.0f");
            ImGui::SliderFloat("DockingSeparatorSize", &style.DockingSeparatorSize, 0.0f, 12.0f, "%.1f");
        }

        if (ImGui::CollapsingHeader("Safe Area")) {
            ImGui::DragFloat2("DisplayWindowPadding", &style.DisplayWindowPadding.x, 1.0f, 0.0f, 100.0f, "%.0f");
            ImGui::DragFloat2("DisplaySafeAreaPadding", &style.DisplaySafeAreaPadding.x, 1.0f, 0.0f, 100.0f, "%.0f");
        }
    }

    void theme_manager::render_fonts_tab() {
        ImGui::ShowFontSelector("Fonts##Selector");
        ImGui::Separator();
        ImGui::ShowFontAtlas(ImGui::GetIO().Fonts);
    }

    void theme_manager::render_rendering_tab() {
        ImGuiStyle &style = ImGui::GetStyle();

        ImGui::Checkbox("AntiAliasedLines", &style.AntiAliasedLines);
        ImGui::Checkbox("AntiAliasedLinesUseTex", &style.AntiAliasedLinesUseTex);
        ImGui::Checkbox("AntiAliasedFill", &style.AntiAliasedFill);

        ImGui::Separator();

        ImGui::SliderFloat("CurveTessellationTol", &style.CurveTessellationTol, 0.10f, 10.0f, "%.2f");
        ImGui::SliderFloat("CircleTessellationMaxError", &style.CircleTessellationMaxError, 0.10f, 5.0f, "%.2f");

        ImGui::Separator();

        ImGui::SliderFloat("HoverStationaryDelay", &style.HoverStationaryDelay, 0.0f, 2.0f, "%.2f sec");
        ImGui::SliderFloat("HoverDelayShort", &style.HoverDelayShort, 0.0f, 2.0f, "%.2f sec");
        ImGui::SliderFloat("HoverDelayNormal", &style.HoverDelayNormal, 0.0f, 2.0f, "%.2f sec");
    }

    // ============================================================================
    // Code generation: produce a valid C++ theme_preset{...} initializer string
    // ============================================================================

    std::string generate_preset_code(const theme_config &cfg) {
        auto fmt_rgb = [](const rgb_color &c) {
            return std::format("{{{{{{{:.3f}f, {:.3f}f, {:.3f}f}}}}}}", c[0], c[1], c[2]);
        };
        auto fmt_u32 = [](const ImU32 c) {
            return std::format("IM_COL32({}, {}, {}, {})", c >> IM_COL32_R_SHIFT & 0xFF, c >> IM_COL32_G_SHIFT & 0xFF,
                               c >> IM_COL32_B_SHIFT & 0xFF, c >> IM_COL32_A_SHIFT & 0xFF);
        };
        auto fmt_opt_rgb = [&](const std::optional<rgb_color> &opt) -> std::string {
            if (!opt.has_value()) return "std::nullopt";
            return std::format("rgb_color{}", fmt_rgb(*opt));
        };

        // Reconstruct node colors from the config's node_colors array
        std::string result;
        result.reserve(2048);
        const auto out = std::back_inserter(result);
        std::format_to(out, "theme_preset{{\n");
        std::format_to(out, "    .name                     = \"{}\",\n", cfg.name);
        std::format_to(out, "    .bg_dark                  = {},\n", fmt_rgb(cfg.preset_bg_dark));
        std::format_to(out, "    .bg_mid                   = {},\n", fmt_rgb(cfg.preset_bg_mid));
        std::format_to(out, "    .accent                   = {},\n", fmt_rgb(cfg.preset_accent));
        std::format_to(out, "    .secondary                = {},\n", fmt_rgb(cfg.preset_secondary));
        std::format_to(out, "    .alternate                = {},\n", fmt_opt_rgb(cfg.preset_alternate));
        std::format_to(out, "    .text                     = {},\n", fmt_opt_rgb(cfg.preset_text));
        std::format_to(out, "    .node_title_bar           = {},\n", fmt_u32(cfg.node_colors.at(ImNodesCol_TitleBar)));
        std::format_to(out, "    .node_title_bar_hovered   = {},\n",
                       fmt_u32(cfg.node_colors.at(ImNodesCol_TitleBarHovered)));
        std::format_to(out, "    .node_title_bar_selected  = {},\n",
                       fmt_u32(cfg.node_colors.at(ImNodesCol_TitleBarSelected)));
        std::format_to(out, "    .node_link                = {},\n", fmt_u32(cfg.node_colors.at(ImNodesCol_Link)));
        std::format_to(out, "    .node_link_hovered        = {},\n",
                       fmt_u32(cfg.node_colors.at(ImNodesCol_LinkHovered)));
        std::format_to(out, "    .node_pin                 = {},\n", fmt_u32(cfg.node_colors.at(ImNodesCol_Pin)));
        std::format_to(out, "    .node_pin_hovered         = {},\n",
                       fmt_u32(cfg.node_colors.at(ImNodesCol_PinHovered)));
        std::format_to(out, "    .node_grid_bg             = {},\n",
                       fmt_u32(cfg.node_colors.at(ImNodesCol_GridBackground)));
        // Emit std::nullopt for optional node fields that match from_preset_core defaults,
        // so generated presets don't hardcode resolved defaults as explicit overrides.
        auto fmt_opt_u32 = [&](const ImU32 c, const ImU32 default_val) -> std::string {
            if (c == default_val) return "{}";
            return fmt_u32(c);
        };
        std::format_to(out, "    .node_background          = {},\n",
                       fmt_opt_u32(cfg.node_colors.at(ImNodesCol_NodeBackground), default_node_background));
        std::format_to(
            out, "    .node_background_hovered  = {},\n",
            fmt_opt_u32(cfg.node_colors.at(ImNodesCol_NodeBackgroundHovered), default_node_background_hovered));
        std::format_to(
            out, "    .node_background_selected = {},\n",
            fmt_opt_u32(cfg.node_colors.at(ImNodesCol_NodeBackgroundSelected), default_node_background_selected));
        std::format_to(out, "    .node_outline             = {},\n",
                       fmt_opt_u32(cfg.node_colors.at(ImNodesCol_NodeOutline), default_node_outline));
        std::format_to(out, "}}");
        return result;
    }

    // ============================================================================
    // Validation: check a theme_config for common mistakes
    // ============================================================================

    std::vector<std::string> validate(const theme_config &cfg) {
        std::vector<std::string> errors;

        // Check alpha values on all ImGui colors
        for (int i = 0; i < ImGuiCol_COUNT; i++) {
            if (const float a = cfg.colors.at(i).w; a < 0.0f || a > 1.0f) {
                errors.push_back(std::format("{}: alpha {:.3f} out of [0,1] range", ImGui::GetStyleColorName(i), a));
            }
        }

        // Fully transparent window background is almost certainly a mistake
        if (cfg.colors.at(ImGuiCol_WindowBg).w < 0.01f) {
            errors.emplace_back("WindowBg is fully transparent");
        }

        // Validate rounding/style fields via table
        struct range_check {
            float theme_config::*ptr;
            std::string_view     name;
            float                min;
            float                max;
        };
        static constexpr auto rounding_checks = std::to_array<range_check>({
            {.ptr = &theme_config::window_rounding, .name = "window_rounding", .min = 0.0f, .max = 50.0f},
            {.ptr = &theme_config::frame_rounding, .name = "frame_rounding", .min = 0.0f, .max = 50.0f},
            {.ptr = &theme_config::tab_rounding, .name = "tab_rounding", .min = 0.0f, .max = 50.0f},
            {.ptr = &theme_config::scrollbar_rounding, .name = "scrollbar_rounding", .min = 0.0f, .max = 50.0f},
            {.ptr = &theme_config::grab_rounding, .name = "grab_rounding", .min = 0.0f, .max = 50.0f},
        });
        for (const auto &[ptr, name, min_val, max_val]: rounding_checks) {
            if (const float v = cfg.*ptr; v < min_val || v > max_val) {
                errors.push_back(
                    std::format("{} ({:.1f}) is out of reasonable range [{},{}]", name, v, min_val, max_val));
            }
        }

        return errors;
    }

} // namespace imgui_util::theme
