// parse.hpp - Safe noexcept string-to-number parsing for ImGui config/input values
//
// Usage:
//   int n = imgui_util::parse::parse_int("42");          // 42
//   float f = imgui_util::parse::parse_float("bad", -1); // -1 (default on error)
//   auto opt = imgui_util::parse::try_parse_int("123");  // std::optional<int>{123}
//   ImVec4 c = imgui_util::parse::parse_vec4("1.0, 0.5, 0.0, 1.0");
//
// All parse functions are noexcept. Bad input returns the default or nullopt.
#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <imgui.h>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

namespace imgui_util::parse {

    // Arithmetic types that we can parse (excludes bool and all char-like types)
    template<typename T>
    concept parseable_arithmetic = std::is_arithmetic_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char> &&
                                   !std::is_same_v<T, wchar_t> && !std::is_same_v<T, char8_t> &&
                                   !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>;

    // Parse any arithmetic type (except bool/char) from a string_view.
    // Returns default_val if parsing fails. Uses std::from_chars internally.
    // NOTE: constexpr for integral types. Float/double from_chars is not constexpr
    // in C++20/23, so those instantiations evaluate at runtime only.
    template<parseable_arithmetic T>
    [[nodiscard]]
    constexpr T parse_value(const std::string_view sv, const T default_val = T{}) noexcept {
        T result = default_val;
        std::from_chars(sv.data(), sv.data() + sv.size(), result);
        return result;
    }

    // Like parse_value but returns nullopt on failure instead of a default.
    template<parseable_arithmetic T>
    [[nodiscard]]
    constexpr std::optional<T> try_parse(const std::string_view sv) noexcept {
        T result{};
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result);
        if (ec == std::errc{})
            return result; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange) -- zero is the standard "no error"
                           // value
        return std::nullopt;
    }

    // Convenience wrappers for common types
    [[nodiscard]]
    constexpr float parse_float(const std::string_view sv, const float default_val = 0.0f) noexcept {
        return parse_value<float>(sv, default_val);
    }

    [[nodiscard]]
    constexpr double parse_double(const std::string_view sv, const double default_val = 0.0) noexcept {
        return parse_value<double>(sv, default_val);
    }

    [[nodiscard]]
    constexpr int parse_int(const std::string_view sv, const int default_val = 0) noexcept {
        return parse_value<int>(sv, default_val);
    }

    [[nodiscard]]
    constexpr std::int64_t parse_i64(const std::string_view sv, const std::int64_t default_val = 0) noexcept {
        return parse_value<std::int64_t>(sv, default_val);
    }

    [[nodiscard]]
    constexpr std::uint32_t parse_u32(const std::string_view sv, const std::uint32_t default_val = 0) noexcept {
        return parse_value<std::uint32_t>(sv, default_val);
    }

    [[nodiscard]]
    constexpr std::optional<float> try_parse_float(const std::string_view sv) noexcept {
        return try_parse<float>(sv);
    }

    [[nodiscard]]
    constexpr std::optional<int> try_parse_int(const std::string_view sv) noexcept {
        return try_parse<int>(sv);
    }

    [[nodiscard]]
    constexpr std::optional<std::uint32_t> try_parse_u32(const std::string_view sv) noexcept {
        return try_parse<std::uint32_t>(sv);
    }

    [[nodiscard]]
    constexpr std::optional<double> try_parse_double(const std::string_view sv) noexcept {
        return try_parse<double>(sv);
    }

    [[nodiscard]]
    constexpr std::optional<std::int64_t> try_parse_i64(const std::string_view sv) noexcept {
        return try_parse<std::int64_t>(sv);
    }

    // Iterate comma-separated tokens in a string_view, invoking fn(index, token) for each.
    // Whitespace around each token is trimmed. Stops after max_count tokens.
    template<typename Fn>
        requires std::invocable<Fn, size_t, std::string_view>
    constexpr void for_each_csv_token(std::string_view sv, const size_t max_count, Fn fn) noexcept {
        size_t pos = 0;
        for (size_t i = 0; i < max_count && pos < sv.size(); ++i) {
            const size_t     comma = sv.find(',', pos);
            std::string_view part  = (comma != std::string_view::npos) ? sv.substr(pos, comma - pos) : sv.substr(pos);
            if (const auto start = part.find_first_not_of(" \t\r\n"); start != std::string_view::npos) {
                part = part.substr(start, part.find_last_not_of(" \t\r\n") - start + 1);
            } else {
                part = {};
            }
            fn(i, part);
            pos = (comma != std::string_view::npos) ? comma + 1 : sv.size();
        }
    }

    // Parse N comma-separated float components. Returns true if at least one parsed.
    // Unparsed slots keep their initial value.
    template<size_t N>
    constexpr bool parse_float_components(const std::string_view sv, const std::span<float, N> out) noexcept {
        bool parsed = false;
        for_each_csv_token(sv, N, [&](size_t i, const std::string_view part) {
            if (auto v = try_parse<float>(part)) {
                out[i] = *v;
                parsed = true;
            }
        });
        return parsed;
    }

    // Parse "r, g, b" floats. Returns true if at least one component parsed.
    constexpr bool parse_float_rgb(const std::string_view sv, const std::span<float, 3> out) noexcept {
        return parse_float_components<3>(sv, out);
    }

    // Parse "r, g, b, a" floats. Returns true if at least one component parsed.
    constexpr bool parse_float_rgba(const std::string_view sv, const std::span<float, 4> out) noexcept {
        return parse_float_components<4>(sv, out);
    }

    // Parse "r, g, b, a" floats into ImVec4 (alpha defaults to 1.0)
    [[nodiscard]]
    constexpr ImVec4 parse_vec4(const std::string_view sv,
                                const ImVec4           default_val = {0.0f, 0.0f, 0.0f, 1.0f}) noexcept {
        std::array components = {default_val.x, default_val.y, default_val.z, default_val.w};
        parse_float_components<4>(sv, std::span{components});
        return {components[0], components[1], components[2], components[3]};
    }

    // Parse "r, g, b, a" integers (0-255) into packed ImU32 color. Clamped to [0,255].
    // Assumes IM_COL32 RGBA layout: R at bits 0-7 (IM_COL32_R_SHIFT=0), G at 8-15, B at 16-23, A at 24-31.
    static_assert(IM_COL32_R_SHIFT == 0 && IM_COL32_G_SHIFT == 8 && IM_COL32_B_SHIFT == 16 && IM_COL32_A_SHIFT == 24,
                  "parse_im_u32 assumes standard IM_COL32 RGBA byte order");
    [[nodiscard]]
    constexpr ImU32 parse_im_u32(const std::string_view sv, const ImU32 default_val = IM_COL32(0, 0, 0, 255)) noexcept {
        std::array components = {
            static_cast<int>((default_val >> 0) & 0xFF),  // R
            static_cast<int>((default_val >> 8) & 0xFF),  // G
            static_cast<int>((default_val >> 16) & 0xFF), // B
            static_cast<int>((default_val >> 24) & 0xFF), // A
        };
        for_each_csv_token(sv, 4, [&](const size_t i, const std::string_view part) {
            components[i] = std::clamp(parse_int(part, components[i]), 0, 255);
        });
        return IM_COL32(components[0], components[1], components[2], components[3]);
    }

} // namespace imgui_util::parse
