#include <cmath>
#include <gtest/gtest.h>
#include <imgui_util/theme/theme.hpp>

using namespace imgui_util::theme;
using namespace imgui_util::color;

// --- lerp_vec4 ---

TEST(LerpVec4, ZeroT) {
    constexpr ImVec4 a{0.0f, 0.2f, 0.4f, 1.0f};
    constexpr ImVec4 b{1.0f, 0.8f, 0.6f, 0.0f};
    constexpr ImVec4 r = lerp_vec4(a, b, 0.0f);
    EXPECT_FLOAT_EQ(r.x, 0.0f);
    EXPECT_FLOAT_EQ(r.y, 0.2f);
    EXPECT_FLOAT_EQ(r.z, 0.4f);
    EXPECT_FLOAT_EQ(r.w, 1.0f);
}

TEST(LerpVec4, OneT) {
    constexpr ImVec4 a{0.0f, 0.2f, 0.4f, 1.0f};
    constexpr ImVec4 b{1.0f, 0.8f, 0.6f, 0.0f};
    constexpr ImVec4 r = lerp_vec4(a, b, 1.0f);
    EXPECT_FLOAT_EQ(r.x, 1.0f);
    EXPECT_FLOAT_EQ(r.y, 0.8f);
    EXPECT_FLOAT_EQ(r.z, 0.6f);
    EXPECT_FLOAT_EQ(r.w, 0.0f);
}

TEST(LerpVec4, HalfT) {
    constexpr ImVec4 a{0.0f, 0.0f, 0.0f, 0.0f};
    constexpr ImVec4 b{1.0f, 1.0f, 1.0f, 1.0f};
    constexpr ImVec4 r = lerp_vec4(a, b, 0.5f);
    EXPECT_NEAR(r.x, 0.5f, 1e-5f);
    EXPECT_NEAR(r.y, 0.5f, 1e-5f);
    EXPECT_NEAR(r.z, 0.5f, 1e-5f);
    EXPECT_NEAR(r.w, 0.5f, 1e-5f);
}

TEST(LerpVec4, QuarterT) {
    constexpr ImVec4 a{0.0f, 0.0f, 0.0f, 0.0f};
    constexpr ImVec4 b{1.0f, 1.0f, 1.0f, 1.0f};
    constexpr ImVec4 r = lerp_vec4(a, b, 0.25f);
    EXPECT_NEAR(r.x, 0.25f, 1e-5f);
    EXPECT_NEAR(r.y, 0.25f, 1e-5f);
    EXPECT_NEAR(r.z, 0.25f, 1e-5f);
    EXPECT_NEAR(r.w, 0.25f, 1e-5f);
}

TEST(LerpVec4, SameInputs) {
    constexpr ImVec4 v{0.3f, 0.5f, 0.7f, 0.9f};
    constexpr ImVec4 r = lerp_vec4(v, v, 0.42f);
    EXPECT_FLOAT_EQ(r.x, 0.3f);
    EXPECT_FLOAT_EQ(r.y, 0.5f);
    EXPECT_FLOAT_EQ(r.z, 0.7f);
    EXPECT_FLOAT_EQ(r.w, 0.9f);
}

// Constexpr verification
static_assert(lerp_vec4({0, 0, 0, 0}, {1, 1, 1, 1}, 0.0f).x == 0.0f);
static_assert(lerp_vec4({0, 0, 0, 0}, {1, 1, 1, 1}, 1.0f).x == 1.0f);

// --- from_preset_core: basic validation ---

namespace {
    // Minimal test preset for constexpr validation
    constexpr theme_preset test_preset{
        .name                     = "TestPreset",
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
        .light_bg_dark            = rgb_color{{{0.85f, 0.85f, 0.88f}}},
        .light_bg_mid             = rgb_color{{{0.92f, 0.92f, 0.94f}}},
        .light_accent             = {},
        .light_secondary          = {},
        .light_text               = rgb_color{{{0.10f, 0.10f, 0.12f}}},
    };

    // Build theme at compile time
    constexpr auto dark_theme  = theme_config::from_preset_core(test_preset, 1.0f);
    constexpr auto light_theme = theme_config::from_preset_core(test_preset, -1.0f);
} // namespace

TEST(FromPresetCore, PresetColorsPreserved) {
    EXPECT_TRUE(dark_theme.preset_bg_dark == test_preset.bg_dark);
    EXPECT_TRUE(dark_theme.preset_bg_mid == test_preset.bg_mid);
    EXPECT_TRUE(dark_theme.preset_accent == test_preset.accent);
    EXPECT_TRUE(dark_theme.preset_secondary == test_preset.secondary);
}

TEST(FromPresetCore, AccentHoverDerived) {
    // accent_hover = accent + 0.10f
    constexpr rgb_color expected = test_preset.accent + 0.10f;
    EXPECT_TRUE(dark_theme.preset_accent_hover == expected);
}

TEST(FromPresetCore, SecondaryDimDerived) {
    // secondary_dim = secondary * 0.80f
    constexpr rgb_color expected = test_preset.secondary * 0.80f;
    EXPECT_TRUE(dark_theme.preset_secondary_dim == expected);
}

TEST(FromPresetCore, DarkModeWindowBg) {
    // WindowBg should be rgb(bg_mid_c) for dark mode
    constexpr ImVec4 expected = rgb(test_preset.bg_mid);
    EXPECT_FLOAT_EQ(dark_theme.colors.at(ImGuiCol_WindowBg).x, expected.x);
    EXPECT_FLOAT_EQ(dark_theme.colors.at(ImGuiCol_WindowBg).y, expected.y);
    EXPECT_FLOAT_EQ(dark_theme.colors.at(ImGuiCol_WindowBg).z, expected.z);
    EXPECT_FLOAT_EQ(dark_theme.colors.at(ImGuiCol_WindowBg).w, expected.w);
}

TEST(FromPresetCore, DarkModeTextColor) {
    // Dark mode with no text override: text_primary should be near-white
    const ImVec4 &text = dark_theme.colors.at(ImGuiCol_Text);
    EXPECT_NEAR(text.x, 0.95f, 0.01f);
    EXPECT_NEAR(text.y, 0.95f, 0.01f);
    EXPECT_NEAR(text.z, 0.97f, 0.01f);
    EXPECT_FLOAT_EQ(text.w, 1.0f);
}

TEST(FromPresetCore, LightModeUsesOverrides) {
    // Light mode should use light_bg_mid override
    EXPECT_TRUE(light_theme.preset_bg_mid == *test_preset.light_bg_mid);
    EXPECT_TRUE(light_theme.preset_bg_dark == *test_preset.light_bg_dark);
}

TEST(FromPresetCore, LightModeTextColor) {
    // Light mode with text override: should use the override
    const ImVec4 &text = light_theme.colors.at(ImGuiCol_Text);
    EXPECT_NEAR(text.x, 0.10f, 0.01f);
    EXPECT_NEAR(text.y, 0.10f, 0.01f);
    EXPECT_NEAR(text.z, 0.12f, 0.01f);
}

TEST(FromPresetCore, NodeColorsSetFromPreset) {
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_TitleBar), test_preset.node_title_bar);
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_TitleBarHovered), test_preset.node_title_bar_hovered);
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_TitleBarSelected), test_preset.node_title_bar_selected);
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_Link), test_preset.node_link);
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_LinkHovered), test_preset.node_link_hovered);
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_Pin), test_preset.node_pin);
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_PinHovered), test_preset.node_pin_hovered);
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_GridBackground), test_preset.node_grid_bg);
}

TEST(FromPresetCore, NodeGridLinesDerivedFromBg) {
    constexpr ImU32 expected_line = offset_u32_rgb(test_preset.node_grid_bg, 16, 120);
    constexpr ImU32 expected_primary = offset_u32_rgb(test_preset.node_grid_bg, 26, 180);
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_GridLine), expected_line);
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_GridLinePrimary), expected_primary);
}

TEST(FromPresetCore, NodeBackgroundDefaults) {
    // No override in preset, should get default values
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_NodeBackground), IM_COL32(32, 32, 38, 245));
    EXPECT_EQ(dark_theme.node_colors.at(ImNodesCol_NodeOutline), IM_COL32(60, 60, 68, 255));
}

TEST(FromPresetCore, HasLightMode) {
    EXPECT_TRUE(test_preset.has_light());
}

TEST(FromPresetCore, ThemeModeEnum) {
    static constexpr auto dark_via_enum = theme_config::from_preset_core(test_preset, theme_mode::dark);
    static constexpr auto light_via_enum = theme_config::from_preset_core(test_preset, theme_mode::light);
    // Should produce identical results as float overload
    EXPECT_TRUE(dark_via_enum.preset_bg_mid == dark_theme.preset_bg_mid);
    EXPECT_TRUE(light_via_enum.preset_bg_mid == light_theme.preset_bg_mid);
}

// Verify from_preset_core is truly constexpr
static_assert(theme_config::from_preset_core(test_preset, 1.0f).preset_accent == test_preset.accent);
static_assert(theme_config::from_preset_core(test_preset, theme_mode::dark).preset_accent == test_preset.accent);

// --- Preset without light overrides ---

namespace {
    constexpr theme_preset dark_only_preset{
        .name                     = "DarkOnly",
        .bg_dark                  = {{{0.08f, 0.08f, 0.10f}}},
        .bg_mid                   = {{{0.12f, 0.12f, 0.14f}}},
        .accent                   = {{{0.50f, 0.30f, 0.80f}}},
        .secondary                = {{{0.80f, 0.40f, 0.20f}}},
        .alternate                = rgb_color{{{0.90f, 0.60f, 0.10f}}},
        .text                     = std::nullopt,
        .node_title_bar           = IM_COL32(80, 50, 130, 255),
        .node_title_bar_hovered   = IM_COL32(100, 65, 160, 255),
        .node_title_bar_selected  = IM_COL32(120, 80, 200, 255),
        .node_link                = IM_COL32(200, 100, 50, 220),
        .node_link_hovered        = IM_COL32(230, 130, 70, 255),
        .node_pin                 = IM_COL32(200, 100, 50, 255),
        .node_pin_hovered         = IM_COL32(230, 130, 70, 255),
        .node_grid_bg             = IM_COL32(18, 18, 22, 255),
    };
} // namespace

TEST(FromPresetCore, NoLightOverrides) {
    EXPECT_FALSE(dark_only_preset.has_light());
    // Even in "light" mode, falls back to dark values since no overrides exist
    static constexpr auto theme = theme_config::from_preset_core(dark_only_preset, -1.0f);
    EXPECT_TRUE(theme.preset_bg_mid == dark_only_preset.bg_mid);
    EXPECT_TRUE(theme.preset_accent == dark_only_preset.accent);
}

TEST(FromPresetCore, AlternateUsedForPlotHistogram) {
    static constexpr auto theme = theme_config::from_preset_core(dark_only_preset, 1.0f);
    // PlotHistogram should use the alternate color
    const ImVec4 &plot_hist = theme.colors.at(ImGuiCol_PlotHistogram);
    constexpr ImVec4 expected = rgb(*dark_only_preset.alternate);
    EXPECT_FLOAT_EQ(plot_hist.x, expected.x);
    EXPECT_FLOAT_EQ(plot_hist.y, expected.y);
    EXPECT_FLOAT_EQ(plot_hist.z, expected.z);
}

TEST(FromPresetCore, PresetAlternatePreserved) {
    static constexpr auto theme = theme_config::from_preset_core(dark_only_preset, 1.0f);
    ASSERT_TRUE(theme.preset_alternate.has_value());
    EXPECT_TRUE(*theme.preset_alternate == *dark_only_preset.alternate);
}
