// fmt_buf.hpp - Stack-allocated formatted text buffer for ImGui
//
// Usage:
//   imgui_util::fmt_buf label{"{}: {}", key, value};
//   ImGui::TextUnformatted(label.c_str());
//
//   imgui_util::fmt_buf<128> big{"long text: {}", data};  // larger buffer
//
// No heap allocation. Default capacity is 64 chars. Truncates silently on overflow.
#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace imgui_util {

    // N = buffer capacity in bytes (including null terminator). Must be >= 2.
    template<size_t N = 64>
        requires(N >= 2)
    struct fmt_buf {
        std::array<char, N> buf{};   // fixed storage, null-terminated
        uint16_t            len = 0; // number of chars written (excludes null terminator)

        constexpr fmt_buf() noexcept { buf[0] = '\0'; }

        // Format directly into the buffer via std::format_to_n (no heap alloc).
        // NOTE: std::format_to_n is constexpr in C++26 (P2510R3) but not in C++23.
        template<typename... Args>
        constexpr explicit fmt_buf(std::format_string<Args...> fmt, Args &&...args) {
            auto result = std::format_to_n(buf.data(), N - 1, fmt, std::forward<Args>(args)...);
            len         = static_cast<uint16_t>(result.out - buf.data());
            buf[len]    = '\0';
        }

        // Accessors -- pass c_str() to ImGui text functions
        [[nodiscard]] constexpr const char      *c_str() const noexcept { return buf.data(); }
        [[nodiscard]] constexpr const char      *data() const noexcept { return buf.data(); }
        [[nodiscard]] constexpr const char      *begin() const noexcept { return buf.data(); }
        [[nodiscard]] constexpr const char      *end() const noexcept { return buf.data() + len; }
        [[nodiscard]] constexpr std::string_view sv() const noexcept { return {buf.data(), len}; }
        [[nodiscard]] constexpr size_t           size() const noexcept { return len; }
        [[nodiscard]] constexpr bool             empty() const noexcept { return len == 0; }
        explicit constexpr operator std::string_view() const noexcept { return {buf.data(), len}; }

        // Reset to empty, ready for incremental append()
        constexpr void reset() noexcept {
            len    = 0;
            buf[0] = '\0';
        }

        // Append formatted text to the buffer (truncates on overflow)
        template<typename... Args>
        constexpr void append(std::format_string<Args...> fmt, Args &&...args) {
            const auto remaining = static_cast<std::ptrdiff_t>((N - 1) - len);
            if (remaining <= 0) return;
            auto result = std::format_to_n(buf.data() + len, remaining, fmt, std::forward<Args>(args)...);
            len         = static_cast<uint16_t>(result.out - buf.data());
            buf[len]    = '\0';
        }

        // Heap-allocating conversion for when you actually need a std::string
        [[nodiscard]] constexpr std::string str() const { return std::string{sv()}; }

        [[nodiscard]] constexpr bool operator==(const fmt_buf &other) const noexcept { return sv() == other.sv(); }
        [[nodiscard]] constexpr bool operator==(const std::string_view other) const noexcept { return sv() == other; }
        [[nodiscard]] constexpr auto operator<=>(const fmt_buf &o) const noexcept { return sv() <=> o.sv(); }
        [[nodiscard]] constexpr auto operator<=>(const std::string_view o) const noexcept { return sv() <=> o; }
    };

    // Format a count with K/M suffixes (e.g. 1500 -> "1.5K", 2000000 -> "2.0M")
    [[nodiscard]] constexpr fmt_buf<32> format_count(int64_t count) {
        // 999'950 rounds to "1.0M" at 1 decimal, avoiding "1000.0K"
        if (count >= 999'950) {
            return fmt_buf<32>("{:.1f}M", static_cast<double>(count) / 1e6);
        }
        if (count >= 1'000) {
            return fmt_buf<32>("{:.1f}K", static_cast<double>(count) / 1e3);
        }
        return fmt_buf<32>("{}", count);
    }

    // Format byte size with B/KB/MB/GB suffixes (e.g. 1536 -> "1.5 KB")
    [[nodiscard]] constexpr fmt_buf<32> format_bytes(int64_t bytes) { // NOLINT(readability-function-size)
        constexpr int64_t kb = 1024;
        constexpr int64_t mb = kb * 1024;
        constexpr int64_t gb = mb * 1024;
        if (bytes < kb) {
            return fmt_buf<32>("{} B", bytes);
        }
        if (bytes < mb) {
            return fmt_buf<32>("{:.1f} KB", static_cast<double>(bytes) / static_cast<double>(kb));
        }
        if (bytes < gb) {
            return fmt_buf<32>("{:.1f} MB", static_cast<double>(bytes) / static_cast<double>(mb));
        }
        return fmt_buf<32>("{:.2f} GB", static_cast<double>(bytes) / static_cast<double>(gb));
    }

} // namespace imgui_util
