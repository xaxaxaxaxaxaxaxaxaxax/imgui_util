#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <imgui_util/core/parse.hpp>
#include <span>

using namespace imgui_util::parse;

// --- parse_float ---

TEST(ParseFloat, ValidFloat) {
    EXPECT_NEAR(parse_float("3.14"), 3.14f, 0.001f);
}

TEST(ParseFloat, ZeroValue) {
    EXPECT_FLOAT_EQ(parse_float("0.0"), 0.0f);
}

TEST(ParseFloat, NegativeValue) {
    EXPECT_NEAR(parse_float("-2.5"), -2.5f, 0.001f);
}

TEST(ParseFloat, InvalidReturnsDefault) {
    EXPECT_FLOAT_EQ(parse_float("invalid", 42.0f), 42.0f);
}

TEST(ParseFloat, EmptyReturnsDefault) {
    EXPECT_FLOAT_EQ(parse_float("", 7.0f), 7.0f);
}

// --- parse_int ---

TEST(ParseInt, ValidInt) {
    EXPECT_EQ(parse_int("42"), 42);
}

TEST(ParseInt, NegativeInt) {
    EXPECT_EQ(parse_int("-10"), -10);
}

TEST(ParseInt, InvalidReturnsDefault) {
    EXPECT_EQ(parse_int("invalid", -1), -1);
}

TEST(ParseInt, EmptyReturnsDefault) {
    EXPECT_EQ(parse_int("", 99), 99);
}

// --- parse_u32 ---

TEST(ParseU32, ValidU32) {
    EXPECT_EQ(parse_u32("255"), 255u);
}

TEST(ParseU32, ZeroValue) {
    EXPECT_EQ(parse_u32("0"), 0u);
}

TEST(ParseU32, InvalidReturnsDefault) {
    EXPECT_EQ(parse_u32("invalid", 100u), 100u);
}

// --- Generic parse_value<T> ---

TEST(ParseValue, Float) {
    EXPECT_NEAR(parse_value<float>("3.14"), 3.14f, 0.001f);
    EXPECT_FLOAT_EQ(parse_value<float>("invalid", 1.0f), 1.0f);
}

TEST(ParseValue, Double) {
    EXPECT_NEAR(parse_value<double>("3.14159265"), 3.14159265, 1e-8);
    EXPECT_DOUBLE_EQ(parse_value<double>("invalid", 2.0), 2.0);
}

TEST(ParseValue, Int) {
    EXPECT_EQ(parse_value<int>("42"), 42);
    EXPECT_EQ(parse_value<int>("invalid", -1), -1);
}

TEST(ParseValue, Int64) {
    EXPECT_EQ(parse_value<std::int64_t>("9999999999"), 9999999999LL);
    EXPECT_EQ(parse_value<std::int64_t>("invalid", -1), -1);
}

TEST(ParseValue, Uint32) {
    EXPECT_EQ(parse_value<std::uint32_t>("4294967295"), 4294967295u);
}

// --- Generic try_parse<T> ---

TEST(TryParse, FloatValid) {
    const auto result = try_parse<float>("3.14");
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(*result, 3.14f, 0.001f);
}

TEST(TryParse, FloatInvalid) {
    EXPECT_FALSE(try_parse<float>("abc").has_value());
    EXPECT_FALSE(try_parse<float>("").has_value());
}

TEST(TryParse, DoubleValid) {
    const auto result = try_parse<double>("2.718281828");
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(*result, 2.718281828, 1e-8);
}

TEST(TryParse, DoubleInvalid) {
    EXPECT_FALSE(try_parse<double>("not_a_number").has_value());
}

TEST(TryParse, IntValid) {
    const auto result = try_parse<int>("123");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(TryParse, IntInvalid) {
    EXPECT_FALSE(try_parse<int>("xyz").has_value());
}

TEST(TryParse, Int64Valid) {
    const auto result = try_parse<std::int64_t>("9999999999");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 9999999999LL);
}

TEST(TryParse, Int64Invalid) {
    EXPECT_FALSE(try_parse<std::int64_t>("").has_value());
}

TEST(TryParse, Uint32Valid) {
    constexpr auto result = try_parse<std::uint32_t>("42");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42u);
}

TEST(TryParse, Uint32Invalid) {
    EXPECT_FALSE(try_parse<std::uint32_t>("negative").has_value());
}

// --- Named try_parse wrappers still work ---

TEST(TryParseNamed, Float) {
    EXPECT_TRUE(try_parse_float("1.5").has_value());
    EXPECT_FALSE(try_parse_float("bad").has_value());
}

TEST(TryParseNamed, Int) {
    EXPECT_TRUE(try_parse_int("10").has_value());
    EXPECT_FALSE(try_parse_int("bad").has_value());
}

TEST(TryParseNamed, U32) {
    EXPECT_TRUE(try_parse_u32("10").has_value());
    EXPECT_FALSE(try_parse_u32("bad").has_value());
}

// --- parse_float_array ---

TEST(ParseFloatArray, ThreeComponents) {
    std::array<float, 3> arr = {0.0f, 0.0f, 0.0f};
    parse_float_array<3>("1.0,2.0,3.0", std::span<float, 3>{arr});
    EXPECT_NEAR(arr[0], 1.0f, 0.001f);
    EXPECT_NEAR(arr[1], 2.0f, 0.001f);
    EXPECT_NEAR(arr[2], 3.0f, 0.001f);
}

TEST(ParseFloatArray, PartialFill) {
    std::array<float, 3> arr = {10.0f, 20.0f, 30.0f};
    parse_float_array<3>("5.0", std::span<float, 3>{arr});
    EXPECT_NEAR(arr[0], 5.0f, 0.001f);
    // Remaining values stay at their initial values
    EXPECT_NEAR(arr[1], 20.0f, 0.001f);
    EXPECT_NEAR(arr[2], 30.0f, 0.001f);
}

TEST(ParseFloatArray, FourComponents) {
    std::array<float, 4> arr = {0.0f, 0.0f, 0.0f, 0.0f};
    parse_float_array<4>("0.1,0.2,0.3,0.4", std::span<float, 4>{arr});
    EXPECT_NEAR(arr[0], 0.1f, 0.001f);
    EXPECT_NEAR(arr[1], 0.2f, 0.001f);
    EXPECT_NEAR(arr[2], 0.3f, 0.001f);
    EXPECT_NEAR(arr[3], 0.4f, 0.001f);
}

// --- parse_vec4 ---

TEST(ParseVec4, FullParse) {
    const ImVec4 v = parse_vec4("0.5,0.5,0.5,1.0");
    EXPECT_NEAR(v.x, 0.5f, 0.001f);
    EXPECT_NEAR(v.y, 0.5f, 0.001f);
    EXPECT_NEAR(v.z, 0.5f, 0.001f);
    EXPECT_NEAR(v.w, 1.0f, 0.001f);
}

TEST(ParseVec4, DefaultAlpha) {
    const ImVec4 v = parse_vec4("1.0,0.0,0.0");
    EXPECT_NEAR(v.x, 1.0f, 0.001f);
    EXPECT_NEAR(v.y, 0.0f, 0.001f);
    EXPECT_NEAR(v.z, 0.0f, 0.001f);
    // Alpha defaults to 1.0 from the default parameter
    EXPECT_NEAR(v.w, 1.0f, 0.001f);
}

TEST(ParseVec4, InvalidReturnsDefault) {
    constexpr ImVec4 def = {0.1f, 0.2f, 0.3f, 0.4f};
    const ImVec4 v   = parse_vec4("", def);
    EXPECT_NEAR(v.x, 0.1f, 0.001f);
    EXPECT_NEAR(v.y, 0.2f, 0.001f);
    EXPECT_NEAR(v.z, 0.3f, 0.001f);
    EXPECT_NEAR(v.w, 0.4f, 0.001f);
}

// --- parse_im_u32 ---

TEST(ParseImU32, FullParse) {
    const ImU32 color = parse_im_u32("255,128,0,255");
    EXPECT_EQ((color >> 0) & 0xFF, 255u);  // R
    EXPECT_EQ((color >> 8) & 0xFF, 128u);  // G
    EXPECT_EQ((color >> 16) & 0xFF, 0u);   // B
    EXPECT_EQ((color >> 24) & 0xFF, 255u); // A
}

TEST(ParseImU32, DefaultAlpha) {
    const ImU32 color = parse_im_u32("100,200,50");
    EXPECT_EQ((color >> 0) & 0xFF, 100u);  // R
    EXPECT_EQ((color >> 8) & 0xFF, 200u);  // G
    EXPECT_EQ((color >> 16) & 0xFF, 50u);  // B
    EXPECT_EQ((color >> 24) & 0xFF, 255u); // A (default)
}

TEST(ParseImU32, ClampValues) {
    // Values > 255 should be clamped
    const ImU32 color = parse_im_u32("300,0,0,255");
    EXPECT_EQ((color >> 0) & 0xFF, 255u); // R clamped to 255
}

// --- Whitespace-padded CSV ---

TEST(ParseFloatArray, WhitespacePaddedCsv) {
    std::array<float, 3> arr = {0.0f, 0.0f, 0.0f};
    parse_float_array<3>("1.0, 2.0, 3.0", std::span<float, 3>{arr});
    EXPECT_NEAR(arr[0], 1.0f, 0.001f);
    EXPECT_NEAR(arr[1], 2.0f, 0.001f);
    EXPECT_NEAR(arr[2], 3.0f, 0.001f);
}
