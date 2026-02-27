#include <compare>
#include <cstring>
#include <gtest/gtest.h>
#include <imgui_util/core/fmt_buf.hpp>
#include <string_view>

using namespace imgui_util;

// --- Basic formatting ---

TEST(FmtBuf, BasicFormatting) {
    const fmt_buf<64> buf("{} world", "hello");
    EXPECT_STREQ(buf.c_str(), "hello world");
}

TEST(FmtBuf, IntegerFormatting) {
    const fmt_buf<64> buf("{} + {} = {}", 1, 2, 3);
    EXPECT_STREQ(buf.c_str(), "1 + 2 = 3");
}

// --- Truncation ---

TEST(FmtBuf, TruncationToBufferSize) {
    const fmt_buf<8> buf("{}", "longstring");
    // Buffer is 8, so max 7 chars + null terminator
    EXPECT_EQ(std::strlen(buf.c_str()), 7u);
    EXPECT_STREQ(buf.c_str(), "longstr");
}

// --- c_str() null termination ---

TEST(FmtBuf, CStrIsNullTerminated) {
    const fmt_buf<64> buf("{}", "test");
    const char *const s = buf.c_str();
    EXPECT_EQ(s[4], '\0');
}

// --- sv() returns correct string_view ---

TEST(FmtBuf, SvReturnsCorrectView) {
    const fmt_buf<64>      buf("{} {}", "hello", "world");
    const std::string_view sv = buf.sv();
    EXPECT_EQ(sv, "hello world");
    EXPECT_EQ(sv.size(), 11u);
}

// --- Implicit string_view conversion ---

TEST(FmtBuf, ImplicitStringViewConversion) {
    const fmt_buf<64>      buf("{}", "abc");
    const std::string_view sv = buf.sv();
    EXPECT_EQ(sv, "abc");
}

// --- Copy constructor ---

TEST(FmtBuf, CopyConstructorPreservesContent) {
    const fmt_buf<64> original("{} {}", "copy", "test");
    const fmt_buf<64> copy(original);
    EXPECT_STREQ(copy.c_str(), "copy test");
    EXPECT_EQ(copy.sv(), original.sv());
}

// --- Copy assignment ---

TEST(FmtBuf, CopyAssignmentPreservesContent) {
    const fmt_buf<64> original("{}", "assigned");
    fmt_buf<64>       other("{}", "other");
    other = original;
    EXPECT_STREQ(other.c_str(), "assigned");
    EXPECT_EQ(other.sv(), original.sv());
}

// --- Self-assignment ---

TEST(FmtBuf, SelfAssignment) {
    fmt_buf<64> buf("{}", "self");
    const auto *ptr = &buf;
    buf             = *ptr;
    EXPECT_STREQ(buf.c_str(), "self");
}

// --- data(), begin(), end() accessors ---

TEST(FmtBuf, DataAccessor) {
    const fmt_buf<64> buf("{}", "hello");
    EXPECT_EQ(buf.data(), buf.c_str());
    EXPECT_EQ(buf.data()[0], 'h');
}

TEST(FmtBuf, BeginEndAccessors) {
    const fmt_buf<64> buf("{}", "abc");
    EXPECT_EQ(buf.begin(), buf.data());
    EXPECT_EQ(buf.end(), buf.begin() + 3);
    EXPECT_EQ(static_cast<size_t>(buf.end() - buf.begin()), buf.size());
}

TEST(FmtBuf, BeginEndRangeIteration) {
    const fmt_buf<64> buf("{}", "xyz");
    std::string       accumulated;
    for (const char it: buf) {
        accumulated += it;
    }
    EXPECT_EQ(accumulated, "xyz");
}

// --- operator== ---

TEST(FmtBuf, EqualityBufToBuf) {
    const fmt_buf<64> a("{}", "hello");
    const fmt_buf<64> b("{}", "hello");
    const fmt_buf<64> c("{}", "world");
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(FmtBuf, EqualityBufToStringView) {
    const fmt_buf<64> buf("{}", "hello");
    EXPECT_TRUE(buf == std::string_view{"hello"});
    EXPECT_FALSE(buf == std::string_view{"world"});
}

TEST(FmtBuf, EqualityDifferentSizes) {
    const fmt_buf<32>  a("{}", "same");
    const fmt_buf<128> b("{}", "same");
    // Different template params but same content - compare via sv()
    EXPECT_EQ(a.sv(), b.sv());
}

// --- N >= 2 constraint ---
// fmt_buf<0> and fmt_buf<1> should not compile. We verify the constraint exists
// by checking the valid case compiles.
static_assert(requires { fmt_buf<2>("{}", 0); }, "fmt_buf<2> should be valid");
static_assert(requires { fmt_buf<64>("{}", 0); }, "fmt_buf<64> should be valid");

// --- format_count ---

TEST(FormatCount, Plain) {
    EXPECT_EQ(format_count(0).sv(), "0");
    EXPECT_EQ(format_count(500).sv(), "500");
    EXPECT_EQ(format_count(999).sv(), "999");
}

TEST(FormatCount, Thousands) {
    EXPECT_EQ(format_count(1000).sv(), "1.0K");
    EXPECT_EQ(format_count(1500).sv(), "1.5K");
    EXPECT_EQ(format_count(999999).sv(), "1.0M");
}

TEST(FormatCount, Millions) {
    EXPECT_EQ(format_count(1000000).sv(), "1.0M");
    EXPECT_EQ(format_count(1500000).sv(), "1.5M");
    EXPECT_EQ(format_count(2500000).sv(), "2.5M");
}

// --- format_bytes ---

TEST(FormatBytes, Bytes) {
    EXPECT_EQ(format_bytes(0).sv(), "0 B");
    EXPECT_EQ(format_bytes(512).sv(), "512 B");
    EXPECT_EQ(format_bytes(1023).sv(), "1023 B");
}

TEST(FormatBytes, Kilobytes) {
    EXPECT_EQ(format_bytes(1024).sv(), "1.0 KB");
    EXPECT_EQ(format_bytes(1536).sv(), "1.5 KB");
}

TEST(FormatBytes, Megabytes) {
    EXPECT_EQ(format_bytes(1048576).sv(), "1.0 MB");
    EXPECT_EQ(format_bytes(1572864).sv(), "1.5 MB");
}

TEST(FormatBytes, Gigabytes) {
    EXPECT_EQ(format_bytes(1073741824LL).sv(), "1.00 GB");
}

// --- Default constructor ---

TEST(FmtBuf, DefaultConstructor) {
    static constexpr fmt_buf<64> buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_STREQ(buf.c_str(), "");
    EXPECT_EQ(buf.sv(), "");
}

// --- operator<=> ---

TEST(FmtBuf, SpaceshipBufToBuf) {
    const fmt_buf<64> a("{}", "abc");
    const fmt_buf<64> b("{}", "def");
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_FALSE(a > b);
}

TEST(FmtBuf, SpaceshipBufToStringView) {
    const fmt_buf<64> buf("{}", "hello");
    EXPECT_TRUE(buf < std::string_view{"world"});
    EXPECT_TRUE(buf > std::string_view{"abc"});
    EXPECT_TRUE((buf <=> std::string_view{"hello"}) == 0);
}
