#include <algorithm>
#include <gtest/gtest.h>
#include <imgui_util/widgets/controls.hpp>
#include <imgui_util/widgets/text.hpp>

using namespace imgui_util;

// --- brighten ---

TEST(Brighten, BasicBrighten) {
    constexpr ImVec4 base{0.2f, 0.3f, 0.4f, 0.8f};
    constexpr ImVec4 result = brighten(base, 0.1f);
    EXPECT_NEAR(result.x, 0.3f, 1e-5f);
    EXPECT_NEAR(result.y, 0.4f, 1e-5f);
    EXPECT_NEAR(result.z, 0.5f, 1e-5f);
    EXPECT_FLOAT_EQ(result.w, 0.8f); // alpha unchanged
}

TEST(Brighten, ClampToOne) {
    constexpr ImVec4 base{0.9f, 0.95f, 1.0f, 1.0f};
    constexpr ImVec4 result = brighten(base, 0.2f);
    EXPECT_FLOAT_EQ(result.x, 1.0f);
    EXPECT_FLOAT_EQ(result.y, 1.0f);
    EXPECT_FLOAT_EQ(result.z, 1.0f);
}

TEST(Brighten, ZeroAmount) {
    constexpr ImVec4 base{0.5f, 0.6f, 0.7f, 1.0f};
    constexpr ImVec4 result = brighten(base, 0.0f);
    EXPECT_FLOAT_EQ(result.x, 0.5f);
    EXPECT_FLOAT_EQ(result.y, 0.6f);
    EXPECT_FLOAT_EQ(result.z, 0.7f);
}

TEST(Brighten, AlphaPreserved) {
    constexpr ImVec4 base{0.1f, 0.2f, 0.3f, 0.42f};
    constexpr ImVec4 result = brighten(base, 0.5f);
    EXPECT_FLOAT_EQ(result.w, 0.42f);
}

// Constexpr verification
static_assert(brighten({0.5f, 0.5f, 0.5f, 1.0f}, 0.1f).x == std::min(0.6f, 1.0f));
static_assert(brighten({1.0f, 1.0f, 1.0f, 0.5f}, 0.1f).x == 1.0f);
static_assert(brighten({0.0f, 0.0f, 0.0f, 0.7f}, 0.0f).w == 0.7f);

// --- linear_fade_alpha ---

TEST(LinearFadeAlpha, ZeroElapsed) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(0.0f, 1.0f), 1.0f);
}

TEST(LinearFadeAlpha, FullDuration) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(1.0f, 1.0f), 0.0f);
}

TEST(LinearFadeAlpha, HalfDuration) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(0.5f, 1.0f), 0.5f);
}

TEST(LinearFadeAlpha, QuarterDuration) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(0.25f, 1.0f), 0.75f);
}

TEST(LinearFadeAlpha, CustomDuration) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(1.0f, 2.0f), 0.5f);
    EXPECT_FLOAT_EQ(linear_fade_alpha(1.5f, 3.0f), 0.5f);
}

TEST(LinearFadeAlpha, ThreeQuarters) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(3.0f, 4.0f), 0.25f);
}

// Constexpr verification
static_assert(linear_fade_alpha(0.0f, 1.0f) == 1.0f);
static_assert(linear_fade_alpha(1.0f, 1.0f) == 0.0f);
static_assert(linear_fade_alpha(0.5f, 1.0f) == 0.5f);
static_assert(linear_fade_alpha(1.0f, 2.0f) == 0.5f);

// --- colors struct: semantic palette values are valid ---

TEST(Colors, AccentValues) {
    // Verify accent colors have full alpha and reasonable RGB
    EXPECT_FLOAT_EQ(colors::accent.w, 1.0f);
    EXPECT_FLOAT_EQ(colors::accent_hover.w, 1.0f);
    EXPECT_GT(colors::accent.z, colors::accent.x); // blue-ish
}

TEST(Colors, StatusColorsAlpha) {
    EXPECT_FLOAT_EQ(colors::success.w, 1.0f);
    EXPECT_FLOAT_EQ(colors::warning.w, 1.0f);
    EXPECT_FLOAT_EQ(colors::error.w, 1.0f);
    EXPECT_FLOAT_EQ(colors::error_dark.w, 1.0f);
}

TEST(Colors, TextHierarchyOrder) {
    // Text colors should get progressively dimmer
    EXPECT_GT(colors::text_primary.x, colors::text_secondary.x);
    EXPECT_GT(colors::text_secondary.x, colors::text_dim.x);
    EXPECT_GT(colors::text_dim.x, colors::text_very_dim.x);
    EXPECT_GT(colors::text_very_dim.x, colors::text_disabled.x);
}

// Constexpr verification of color palette
static_assert(colors::accent.w == 1.0f);
static_assert(colors::error.x == 1.0f);
static_assert(colors::success.w == 1.0f);

// --- truncated_text (no ImGui context needed for basic construction) ---

TEST(TruncatedText, NonTruncatedViewPreservesOriginal) {
    const truncated_text t{std::string_view{"hello world"}};
    EXPECT_EQ(t.view(), "hello world");
    EXPECT_FALSE(t.was_truncated());
}

TEST(TruncatedText, TruncatedFromOwnedString) {
    const truncated_text t{std::string{"hell..."}};
    EXPECT_EQ(t.view(), "hell...");
    EXPECT_TRUE(t.was_truncated());
}
