#include <gtest/gtest.h>
#include <imgui_util/widgets/helpers.hpp>
#include <imgui_util/widgets/search_bar.hpp>
#include <imgui_util/widgets/text.hpp>
#include <string_view>

using namespace imgui_util::search;
using namespace imgui_util;

// --- char_equal_ignore_case ---

TEST(CharEqualIgnoreCase, SameLetterDifferentCase) {
    EXPECT_TRUE(char_equal_ignore_case('a', 'A'));
    EXPECT_TRUE(char_equal_ignore_case('A', 'a'));
    EXPECT_TRUE(char_equal_ignore_case('Z', 'z'));
}

TEST(CharEqualIgnoreCase, SameLetterSameCase) {
    EXPECT_TRUE(char_equal_ignore_case('a', 'a'));
    EXPECT_TRUE(char_equal_ignore_case('A', 'A'));
}

TEST(CharEqualIgnoreCase, DifferentLetters) {
    EXPECT_FALSE(char_equal_ignore_case('a', 'b'));
    EXPECT_FALSE(char_equal_ignore_case('A', 'B'));
}

TEST(CharEqualIgnoreCase, NonAlphaCharacters) {
    EXPECT_TRUE(char_equal_ignore_case('1', '1'));
    EXPECT_FALSE(char_equal_ignore_case('1', '2'));
    EXPECT_TRUE(char_equal_ignore_case(' ', ' '));
}

// --- contains_ignore_case ---

TEST(ContainsIgnoreCase, SubstringMatch) {
    EXPECT_TRUE(contains_ignore_case("Hello World", "hello"));
    EXPECT_TRUE(contains_ignore_case("Hello World", "WORLD"));
    EXPECT_TRUE(contains_ignore_case("Hello World", "lo Wo"));
}

TEST(ContainsIgnoreCase, NoMatch) {
    EXPECT_FALSE(contains_ignore_case("Hello World", "xyz"));
    EXPECT_FALSE(contains_ignore_case("Hello World", "worldx"));
}

TEST(ContainsIgnoreCase, EmptyNeedle) {
    EXPECT_TRUE(contains_ignore_case("Hello World", ""));
}

TEST(ContainsIgnoreCase, EmptyHaystack) {
    EXPECT_FALSE(contains_ignore_case("", "hello"));
}

TEST(ContainsIgnoreCase, ExactMatch) {
    EXPECT_TRUE(contains_ignore_case("hello", "HELLO"));
}

TEST(ContainsIgnoreCase, BothEmpty) {
    EXPECT_TRUE(contains_ignore_case("", ""));
}

TEST(ContainsIgnoreCase, SingleChar) {
    EXPECT_TRUE(contains_ignore_case("A", "a"));
    EXPECT_FALSE(contains_ignore_case("A", "b"));
}

TEST(ContainsIgnoreCase, NeedleLongerThanHaystack) {
    EXPECT_FALSE(contains_ignore_case("hi", "hello world"));
}

TEST(ContainsIgnoreCase, SpecialCharacters) {
    EXPECT_TRUE(contains_ignore_case("hello-world_123", "-world_"));
    EXPECT_TRUE(contains_ignore_case("foo.bar", ".bar"));
}

TEST(ContainsIgnoreCase, RepeatedPattern) {
    EXPECT_TRUE(contains_ignore_case("ababab", "bab"));
}

// --- matches_any ---

TEST(MatchesAny, FirstFieldMatches) {
    EXPECT_TRUE(matches_any("test", std::string_view{"testing"}, std::string_view{"other"}));
}

TEST(MatchesAny, SecondFieldMatches) {
    EXPECT_TRUE(matches_any("other", std::string_view{"testing"}, std::string_view{"another"}));
}

TEST(MatchesAny, EmptyQueryMatchesAll) {
    EXPECT_TRUE(matches_any("", std::string_view{"anything"}));
    EXPECT_TRUE(matches_any("", std::string_view{"a"}, std::string_view{"b"}));
}

TEST(MatchesAny, NoFieldMatches) {
    EXPECT_FALSE(matches_any("xyz", std::string_view{"abc"}, std::string_view{"def"}));
}

TEST(MatchesAny, CaseInsensitive) {
    EXPECT_TRUE(matches_any("TEST", std::string_view{"testing"}));
}

TEST(MatchesAny, SingleField) {
    EXPECT_TRUE(matches_any("hello", std::string_view{"say hello"}));
    EXPECT_FALSE(matches_any("goodbye", std::string_view{"say hello"}));
}

TEST(MatchesAny, AllFieldsEmpty) {
    EXPECT_FALSE(matches_any("query", std::string_view{""}, std::string_view{""}));
}

TEST(MatchesAny, ManyFields) {
    EXPECT_TRUE(matches_any("d", std::string_view{"a"}, std::string_view{"b"}, std::string_view{"c"},
                            std::string_view{"d"}));
    EXPECT_FALSE(matches_any("e", std::string_view{"a"}, std::string_view{"b"}, std::string_view{"c"},
                             std::string_view{"d"}));
}

// --- search_bar default state ---

TEST(SearchBar, DefaultStateIsEmpty) {
    constexpr search_bar<128> bar;
    EXPECT_TRUE(bar.empty());
    EXPECT_EQ(bar.query(), "");
    EXPECT_TRUE(bar.query().empty());
}

TEST(SearchBar, ClearResetsState) {
    search_bar<128> bar;
    // Verify clear works from default state
    bar.clear();
    EXPECT_TRUE(bar.empty());
    EXPECT_EQ(bar.query(), "");
    EXPECT_EQ(bar.query().size(), 0u);
}

TEST(SearchBar, ResetClearsAndRequestsFocus) {
    search_bar<128> bar;
    bar.reset();
    EXPECT_TRUE(bar.empty());
    EXPECT_EQ(bar.query(), "");
}

TEST(SearchBar, MatchesWithEmptyQuery) {
    constexpr search_bar<128> bar;
    // Empty query matches everything
    EXPECT_TRUE(bar.matches(std::string_view{"anything"}));
}

// --- copy_to_buffer ---

TEST(CopyToBuffer, FitsInBuffer) {
    std::array<char, 16> buf{};
    EXPECT_TRUE(copy_to_buffer(buf, "hello"));
    EXPECT_STREQ(buf.data(), "hello");
}

TEST(CopyToBuffer, ExactFit) {
    std::array<char, 6> buf{}; // 5 chars + null
    EXPECT_TRUE(copy_to_buffer(buf, "hello"));
    EXPECT_STREQ(buf.data(), "hello");
}

TEST(CopyToBuffer, TruncatesWhenTooLong) {
    std::array<char, 4> buf{}; // 3 chars + null
    EXPECT_FALSE(copy_to_buffer(buf, "hello"));
    EXPECT_STREQ(buf.data(), "hel");
}

TEST(CopyToBuffer, EmptySource) {
    std::array<char, 8> buf{};
    EXPECT_TRUE(copy_to_buffer(buf, ""));
    EXPECT_STREQ(buf.data(), "");
}

TEST(CopyToBuffer, SpanOverload) {
    std::array<char, 8> buf{};
    const std::span<char, 8> s{buf};
    EXPECT_TRUE(copy_to_buffer(s, "test"));
    EXPECT_STREQ(buf.data(), "test");
}

TEST(CopyToBuffer, SpanOverloadTruncation) {
    std::array<char, 3> buf{};
    const std::span<char, 3> s{buf};
    EXPECT_FALSE(copy_to_buffer(s, "abcdef"));
    EXPECT_STREQ(buf.data(), "ab");
}

TEST(CopyToBuffer, NullTerminated) {
    std::array<char, 2> buf{};
    buf.fill('X');
    (void)copy_to_buffer(buf, "A");
    EXPECT_EQ(buf[0], 'A');
    EXPECT_EQ(buf[1], '\0');
}

// --- toggle_ref ---

TEST(ToggleRef, InitialValue) {
    bool value = true;
    const toggle_ref ref(value);
    EXPECT_TRUE(static_cast<bool>(ref));
}

TEST(ToggleRef, Toggle) {
    bool value = false;
    const toggle_ref ref(value);
    EXPECT_FALSE(static_cast<bool>(ref));
    ref.toggle();
    EXPECT_TRUE(static_cast<bool>(ref));
    EXPECT_TRUE(value);
    ref.toggle();
    EXPECT_FALSE(static_cast<bool>(ref));
    EXPECT_FALSE(value);
}

TEST(ToggleRef, Set) {
    bool value = false;
    const toggle_ref ref(value);
    ref.set(true);
    EXPECT_TRUE(value);
    ref.set(false);
    EXPECT_FALSE(value);
}

TEST(ToggleRef, Constexpr) {
    // Verify constexpr construction and operations compile
    static bool storage = false;
    constexpr toggle_ref ref(storage);
    ref.set(true);
    EXPECT_TRUE(storage);
    storage = false;
}

// --- linear_fade_alpha ---

TEST(LinearFadeAlpha, ZeroElapsed) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(0.0, 1.0), 1.0f);
}

TEST(LinearFadeAlpha, FullDuration) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(1.0, 1.0), 0.0f);
}

TEST(LinearFadeAlpha, HalfDuration) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(0.5, 1.0), 0.5f);
}

TEST(LinearFadeAlpha, QuarterDuration) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(0.25, 1.0), 0.75f);
}

TEST(LinearFadeAlpha, CustomDuration) {
    EXPECT_FLOAT_EQ(linear_fade_alpha(1.0, 2.0), 0.5f);
}

TEST(LinearFadeAlpha, Constexpr) {
    static_assert(linear_fade_alpha(0.0, 1.0) == 1.0f);
    static_assert(linear_fade_alpha(1.0, 1.0) == 0.0f);
}
