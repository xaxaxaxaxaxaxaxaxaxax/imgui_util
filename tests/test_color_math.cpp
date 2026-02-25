#include <gtest/gtest.h>
#include <imgui_util/theme/color_math.hpp>

using namespace imgui_util::color;

// --- rgb_color construction and indexing ---

TEST(RgbColor, DefaultConstruction) {
    constexpr rgb_color c{};
    EXPECT_FLOAT_EQ(c[0], 0.0f);
    EXPECT_FLOAT_EQ(c[1], 0.0f);
    EXPECT_FLOAT_EQ(c[2], 0.0f);
}

TEST(RgbColor, AggregateConstruction) {
    constexpr rgb_color c{{{0.2f, 0.4f, 0.6f}}};
    EXPECT_FLOAT_EQ(c[0], 0.2f);
    EXPECT_FLOAT_EQ(c[1], 0.4f);
    EXPECT_FLOAT_EQ(c[2], 0.6f);
}

TEST(RgbColor, DataPointer) {
    constexpr rgb_color c{{{0.1f, 0.2f, 0.3f}}};
    const float *p = c.data();
    EXPECT_FLOAT_EQ(p[0], 0.1f);
    EXPECT_FLOAT_EQ(p[1], 0.2f);
    EXPECT_FLOAT_EQ(p[2], 0.3f);
}

TEST(RgbColor, MutableDataPointer) {
    rgb_color c{{{0.0f, 0.0f, 0.0f}}};
    float *p = c.data();
    p[1] = 0.5f;
    EXPECT_FLOAT_EQ(c[1], 0.5f);
}

// --- rgb_color equality ---

TEST(RgbColor, Equality) {
    constexpr rgb_color a{{{0.1f, 0.2f, 0.3f}}};
    constexpr rgb_color b{{{0.1f, 0.2f, 0.3f}}};
    constexpr rgb_color c{{{0.1f, 0.2f, 0.4f}}};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

static_assert(rgb_color{{{0.5f, 0.5f, 0.5f}}} == rgb_color{{{0.5f, 0.5f, 0.5f}}});
static_assert(!(rgb_color{{{0.5f, 0.5f, 0.5f}}} == rgb_color{{{0.5f, 0.5f, 0.6f}}}));

// --- operator+ (per-channel offset) ---

TEST(RgbColor, AddOffset) {
    constexpr rgb_color base{{{0.2f, 0.3f, 0.5f}}};
    constexpr rgb_color result = base + 0.1f;
    EXPECT_NEAR(result[0], 0.3f, 1e-5f);
    EXPECT_NEAR(result[1], 0.4f, 1e-5f);
    EXPECT_NEAR(result[2], 0.6f, 1e-5f);
}

TEST(RgbColor, AddOffsetClampHigh) {
    constexpr rgb_color base{{{0.9f, 0.95f, 1.0f}}};
    constexpr rgb_color result = base + 0.2f;
    EXPECT_FLOAT_EQ(result[0], 1.0f);
    EXPECT_FLOAT_EQ(result[1], 1.0f);
    EXPECT_FLOAT_EQ(result[2], 1.0f);
}

static_assert((rgb_color{{{0.9f, 0.9f, 0.9f}}} + 0.2f) == rgb_color{{{1.0f, 1.0f, 1.0f}}});

// --- operator- (per-channel subtract) ---

TEST(RgbColor, SubtractOffset) {
    constexpr rgb_color base{{{0.5f, 0.6f, 0.7f}}};
    constexpr rgb_color result = base - 0.1f;
    EXPECT_NEAR(result[0], 0.4f, 1e-5f);
    EXPECT_NEAR(result[1], 0.5f, 1e-5f);
    EXPECT_NEAR(result[2], 0.6f, 1e-5f);
}

TEST(RgbColor, SubtractOffsetClampLow) {
    constexpr rgb_color base{{{0.05f, 0.0f, 0.1f}}};
    constexpr rgb_color result = base - 0.2f;
    EXPECT_FLOAT_EQ(result[0], 0.0f);
    EXPECT_FLOAT_EQ(result[1], 0.0f);
    EXPECT_FLOAT_EQ(result[2], 0.0f);
}

static_assert((rgb_color{{{0.05f, 0.0f, 0.1f}}} - 0.2f) == rgb_color{{{0.0f, 0.0f, 0.0f}}});

// --- operator* (per-channel scale) ---

TEST(RgbColor, Scale) {
    constexpr rgb_color base{{{0.4f, 0.5f, 0.6f}}};
    constexpr rgb_color result = base * 0.5f;
    EXPECT_NEAR(result[0], 0.2f, 1e-5f);
    EXPECT_NEAR(result[1], 0.25f, 1e-5f);
    EXPECT_NEAR(result[2], 0.3f, 1e-5f);
}

TEST(RgbColor, ScaleClampHigh) {
    constexpr rgb_color base{{{0.6f, 0.7f, 0.8f}}};
    constexpr rgb_color result = base * 2.0f;
    EXPECT_FLOAT_EQ(result[0], 1.0f);
    EXPECT_FLOAT_EQ(result[1], 1.0f);
    EXPECT_FLOAT_EQ(result[2], 1.0f);
}

TEST(RgbColor, ScaleByZero) {
    constexpr rgb_color base{{{0.5f, 0.5f, 0.5f}}};
    constexpr rgb_color result = base * 0.0f;
    EXPECT_FLOAT_EQ(result[0], 0.0f);
    EXPECT_FLOAT_EQ(result[1], 0.0f);
    EXPECT_FLOAT_EQ(result[2], 0.0f);
}

// --- compound assignment operators ---

TEST(RgbColor, PlusEquals) {
    rgb_color c{{{0.3f, 0.4f, 0.5f}}};
    c += 0.1f;
    EXPECT_NEAR(c[0], 0.4f, 1e-5f);
    EXPECT_NEAR(c[1], 0.5f, 1e-5f);
    EXPECT_NEAR(c[2], 0.6f, 1e-5f);
}

TEST(RgbColor, MinusEquals) {
    rgb_color c{{{0.3f, 0.4f, 0.5f}}};
    c -= 0.1f;
    EXPECT_NEAR(c[0], 0.2f, 1e-5f);
    EXPECT_NEAR(c[1], 0.3f, 1e-5f);
    EXPECT_NEAR(c[2], 0.4f, 1e-5f);
}

TEST(RgbColor, TimesEquals) {
    rgb_color c{{{0.4f, 0.5f, 0.6f}}};
    c *= 2.0f;
    EXPECT_NEAR(c[0], 0.8f, 1e-5f);
    EXPECT_FLOAT_EQ(c[1], 1.0f);
    EXPECT_FLOAT_EQ(c[2], 1.0f);
}

// --- rgb() conversion to ImVec4 ---

TEST(RgbConversion, FromChannels) {
    constexpr ImVec4 v = rgb(0.1f, 0.2f, 0.3f);
    EXPECT_FLOAT_EQ(v.x, 0.1f);
    EXPECT_FLOAT_EQ(v.y, 0.2f);
    EXPECT_FLOAT_EQ(v.z, 0.3f);
    EXPECT_FLOAT_EQ(v.w, 1.0f); // default alpha
}

TEST(RgbConversion, FromChannelsWithAlpha) {
    constexpr ImVec4 v = rgb(0.1f, 0.2f, 0.3f, 0.5f);
    EXPECT_FLOAT_EQ(v.w, 0.5f);
}

TEST(RgbConversion, FromRgbColor) {
    constexpr rgb_color c{{{0.2f, 0.4f, 0.6f}}};
    constexpr ImVec4 v = rgb(c);
    EXPECT_FLOAT_EQ(v.x, 0.2f);
    EXPECT_FLOAT_EQ(v.y, 0.4f);
    EXPECT_FLOAT_EQ(v.z, 0.6f);
    EXPECT_FLOAT_EQ(v.w, 1.0f);
}

TEST(RgbConversion, FromRgbColorWithAlpha) {
    constexpr rgb_color c{{{0.2f, 0.4f, 0.6f}}};
    constexpr ImVec4 v = rgb(c, 0.7f);
    EXPECT_FLOAT_EQ(v.w, 0.7f);
}

// --- scale() ---

TEST(Scale, BasicScale) {
    constexpr rgb_color c{{{0.4f, 0.5f, 0.6f}}};
    constexpr ImVec4 v = scale(c, 0.5f);
    EXPECT_NEAR(v.x, 0.2f, 1e-5f);
    EXPECT_NEAR(v.y, 0.25f, 1e-5f);
    EXPECT_NEAR(v.z, 0.3f, 1e-5f);
    EXPECT_FLOAT_EQ(v.w, 1.0f);
}

TEST(Scale, ClampToOne) {
    constexpr rgb_color c{{{0.8f, 0.9f, 1.0f}}};
    constexpr ImVec4 v = scale(c, 2.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 1.0f);
    EXPECT_FLOAT_EQ(v.z, 1.0f);
}

TEST(Scale, CustomAlpha) {
    constexpr rgb_color c{{{0.5f, 0.5f, 0.5f}}};
    constexpr ImVec4 v = scale(c, 1.0f, 0.3f);
    EXPECT_FLOAT_EQ(v.w, 0.3f);
}

// --- offset() ---

TEST(Offset, BasicOffset) {
    constexpr rgb_color c{{{0.2f, 0.3f, 0.4f}}};
    constexpr ImVec4 v = offset(c, 0.1f);
    EXPECT_NEAR(v.x, 0.3f, 1e-5f);
    EXPECT_NEAR(v.y, 0.4f, 1e-5f);
    EXPECT_NEAR(v.z, 0.5f, 1e-5f);
    EXPECT_FLOAT_EQ(v.w, 1.0f);
}

TEST(Offset, NegativeOffset) {
    constexpr rgb_color c{{{0.5f, 0.5f, 0.5f}}};
    constexpr ImVec4 v = offset(c, -0.3f);
    EXPECT_NEAR(v.x, 0.2f, 1e-5f);
    EXPECT_NEAR(v.y, 0.2f, 1e-5f);
    EXPECT_NEAR(v.z, 0.2f, 1e-5f);
}

TEST(Offset, ClampToZero) {
    constexpr rgb_color c{{{0.1f, 0.0f, 0.05f}}};
    constexpr ImVec4 v = offset(c, -0.5f);
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Offset, CustomAlpha) {
    constexpr rgb_color c{{{0.5f, 0.5f, 0.5f}}};
    constexpr ImVec4 v = offset(c, 0.0f, 0.8f);
    EXPECT_FLOAT_EQ(v.w, 0.8f);
}

// --- float4_to_u32 and u32_to_float4 ---

TEST(PackedConversion, OpaqueWhite) {
    constexpr ImVec4 white{1.0f, 1.0f, 1.0f, 1.0f};
    constexpr ImU32 packed = float4_to_u32(white);
    EXPECT_EQ((packed >> IM_COL32_R_SHIFT) & 0xFF, 255u);
    EXPECT_EQ((packed >> IM_COL32_G_SHIFT) & 0xFF, 255u);
    EXPECT_EQ((packed >> IM_COL32_B_SHIFT) & 0xFF, 255u);
    EXPECT_EQ((packed >> IM_COL32_A_SHIFT) & 0xFF, 255u);
}

TEST(PackedConversion, OpaqueBlack) {
    constexpr ImVec4 black{0.0f, 0.0f, 0.0f, 1.0f};
    constexpr ImU32 packed = float4_to_u32(black);
    EXPECT_EQ((packed >> IM_COL32_R_SHIFT) & 0xFF, 0u);
    EXPECT_EQ((packed >> IM_COL32_G_SHIFT) & 0xFF, 0u);
    EXPECT_EQ((packed >> IM_COL32_B_SHIFT) & 0xFF, 0u);
    EXPECT_EQ((packed >> IM_COL32_A_SHIFT) & 0xFF, 255u);
}

TEST(PackedConversion, RoundTripFloat4ToU32) {
    constexpr ImVec4 original{1.0f, 0.0f, 0.0f, 1.0f}; // pure red
    constexpr ImU32 packed = float4_to_u32(original);
    constexpr ImVec4 back = u32_to_float4(packed);
    EXPECT_NEAR(back.x, 1.0f, 1.0f / 255.0f);
    EXPECT_NEAR(back.y, 0.0f, 1.0f / 255.0f);
    EXPECT_NEAR(back.z, 0.0f, 1.0f / 255.0f);
    EXPECT_NEAR(back.w, 1.0f, 1.0f / 255.0f);
}

TEST(PackedConversion, RoundTripU32ToFloat4) {
    constexpr ImU32 original = IM_COL32(100, 150, 200, 255);
    constexpr ImVec4 unpacked = u32_to_float4(original);
    constexpr ImU32 repacked = float4_to_u32(unpacked);
    EXPECT_EQ((repacked >> IM_COL32_R_SHIFT) & 0xFF, 100u);
    EXPECT_EQ((repacked >> IM_COL32_G_SHIFT) & 0xFF, 150u);
    EXPECT_EQ((repacked >> IM_COL32_B_SHIFT) & 0xFF, 200u);
    EXPECT_EQ((repacked >> IM_COL32_A_SHIFT) & 0xFF, 255u);
}

TEST(PackedConversion, MidGray) {
    constexpr ImVec4 gray{0.5f, 0.5f, 0.5f, 0.5f};
    constexpr ImU32 packed = float4_to_u32(gray);
    // 0.5 * 255 + 0.5 = 128
    EXPECT_EQ((packed >> IM_COL32_R_SHIFT) & 0xFF, 128u);
    EXPECT_EQ((packed >> IM_COL32_G_SHIFT) & 0xFF, 128u);
    EXPECT_EQ((packed >> IM_COL32_B_SHIFT) & 0xFF, 128u);
    EXPECT_EQ((packed >> IM_COL32_A_SHIFT) & 0xFF, 128u);
}

// Constexpr round-trip verification
static_assert(float4_to_u32(u32_to_float4(IM_COL32(0, 0, 0, 255))) == IM_COL32(0, 0, 0, 255));
static_assert(float4_to_u32(u32_to_float4(IM_COL32(255, 255, 255, 255))) == IM_COL32(255, 255, 255, 255));
static_assert(float4_to_u32(u32_to_float4(IM_COL32(100, 150, 200, 128))) == IM_COL32(100, 150, 200, 128));

// --- offset_u32_rgb ---

TEST(OffsetU32Rgb, PositiveDelta) {
    constexpr ImU32 base = IM_COL32(100, 100, 100, 255);
    constexpr ImU32 result = offset_u32_rgb(base, 16, 120);
    EXPECT_EQ((result >> IM_COL32_R_SHIFT) & 0xFF, 116u);
    EXPECT_EQ((result >> IM_COL32_G_SHIFT) & 0xFF, 116u);
    EXPECT_EQ((result >> IM_COL32_B_SHIFT) & 0xFF, 116u);
    EXPECT_EQ((result >> IM_COL32_A_SHIFT) & 0xFF, 120u); // alpha replaced
}

TEST(OffsetU32Rgb, NegativeDelta) {
    constexpr ImU32 base = IM_COL32(50, 100, 150, 255);
    constexpr ImU32 result = offset_u32_rgb(base, -60, 200);
    EXPECT_EQ((result >> IM_COL32_R_SHIFT) & 0xFF, 0u);   // clamped to 0
    EXPECT_EQ((result >> IM_COL32_G_SHIFT) & 0xFF, 40u);
    EXPECT_EQ((result >> IM_COL32_B_SHIFT) & 0xFF, 90u);
    EXPECT_EQ((result >> IM_COL32_A_SHIFT) & 0xFF, 200u);
}

TEST(OffsetU32Rgb, ClampHigh) {
    constexpr ImU32 base = IM_COL32(250, 240, 200, 255);
    constexpr ImU32 result = offset_u32_rgb(base, 20, 255);
    EXPECT_EQ((result >> IM_COL32_R_SHIFT) & 0xFF, 255u); // clamped
    EXPECT_EQ((result >> IM_COL32_G_SHIFT) & 0xFF, 255u); // clamped
    EXPECT_EQ((result >> IM_COL32_B_SHIFT) & 0xFF, 220u);
}

TEST(OffsetU32Rgb, ZeroDelta) {
    constexpr ImU32 base = IM_COL32(42, 84, 126, 255);
    constexpr ImU32 result = offset_u32_rgb(base, 0, 100);
    EXPECT_EQ((result >> IM_COL32_R_SHIFT) & 0xFF, 42u);
    EXPECT_EQ((result >> IM_COL32_G_SHIFT) & 0xFF, 84u);
    EXPECT_EQ((result >> IM_COL32_B_SHIFT) & 0xFF, 126u);
    EXPECT_EQ((result >> IM_COL32_A_SHIFT) & 0xFF, 100u);
}
