/// @file parse.hpp
/// @brief Safe noexcept string-to-number parsing for ImGui config/input values.
///
/// All parse functions are noexcept. Bad input returns the default or nullopt.
///
/// Usage:
/// @code
///   int n = imgui_util::parse::parse_int("42");          // 42
///   float f = imgui_util::parse::parse_float("bad", -1); // -1 (default on error)
///   auto opt = imgui_util::parse::try_parse_int("123");  // std::optional<int>{123}
///   ImVec4 c = imgui_util::parse::parse_vec4("1.0, 0.5, 0.0, 1.0");
/// @endcode
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

    /// @brief Constrains to numeric types suitable for std::from_chars (excludes bool and character types).
    template<typename T>
    concept parseable_arithmetic =
        std::is_arithmetic_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char> && !std::is_same_v<T, wchar_t>
        && !std::is_same_v<T, char8_t> && !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>;

    /**
     * @brief Parse any arithmetic type from a string via std::from_chars.
     * @tparam T     Arithmetic type to parse (constexpr for integral types only).
     * @param sv          Input string.
     * @param default_val Value returned on parse failure.
     * @return Parsed value, or @p default_val on error.
     */
    template<parseable_arithmetic T>
    [[nodiscard]] constexpr T parse_value(const std::string_view sv, const T default_val = T{}) noexcept {
        T                 result = default_val;
        const auto *const end    = sv.data() + sv.size();
        auto [ptr, ec]           = std::from_chars(sv.data(), end, result);
        if (ec != std::errc() || ptr != end) return default_val;
        return result;
    }

    /**
     * @brief Try to parse an arithmetic type, returning nullopt on failure.
     * @tparam T Arithmetic type to parse.
     * @param sv Input string.
     * @return The parsed value, or std::nullopt on error.
     */
    template<parseable_arithmetic T>
    [[nodiscard]] constexpr std::optional<T> try_parse(const std::string_view sv) noexcept {
        T                 result{};
        const auto *const end = sv.data() + sv.size();
        auto [ptr, ec]        = std::from_chars(sv.data(), end, result);
        if (ec == std::errc() && ptr == end) return result;
        return std::nullopt;
    }

    [[nodiscard]] constexpr float parse_float(const std::string_view sv, const float default_val = 0.0f) noexcept {
        return parse_value<float>(sv, default_val);
    }

    [[nodiscard]] constexpr double parse_double(const std::string_view sv, const double default_val = 0.0) noexcept {
        return parse_value<double>(sv, default_val);
    }

    [[nodiscard]] constexpr int parse_int(const std::string_view sv, const int default_val = 0) noexcept {
        return parse_value<int>(sv, default_val);
    }

    [[nodiscard]] constexpr std::int64_t parse_i64(const std::string_view sv,
                                                   const std::int64_t     default_val = 0) noexcept {
        return parse_value<std::int64_t>(sv, default_val);
    }

    [[nodiscard]] constexpr std::uint32_t parse_u32(const std::string_view sv,
                                                    const std::uint32_t    default_val = 0) noexcept {
        return parse_value<std::uint32_t>(sv, default_val);
    }

    [[nodiscard]] constexpr std::optional<float> try_parse_float(const std::string_view sv) noexcept {
        return try_parse<float>(sv);
    }

    [[nodiscard]] constexpr std::optional<int> try_parse_int(const std::string_view sv) noexcept {
        return try_parse<int>(sv);
    }

    [[nodiscard]] constexpr std::optional<std::uint32_t> try_parse_u32(const std::string_view sv) noexcept {
        return try_parse<std::uint32_t>(sv);
    }

    [[nodiscard]] constexpr std::optional<double> try_parse_double(const std::string_view sv) noexcept {
        return try_parse<double>(sv);
    }

    [[nodiscard]] constexpr std::optional<std::int64_t> try_parse_i64(const std::string_view sv) noexcept {
        return try_parse<std::int64_t>(sv);
    }

    [[nodiscard]] constexpr std::uint64_t parse_u64(const std::string_view sv,
                                                    const std::uint64_t    default_val = 0) noexcept {
        return parse_value<std::uint64_t>(sv, default_val);
    }

    [[nodiscard]] constexpr std::optional<std::uint64_t> try_parse_u64(const std::string_view sv) noexcept {
        return try_parse<std::uint64_t>(sv);
    }

    /// @brief Parse "true"/"false"/"1"/"0" into a bool, returning nullopt on unrecognized input.
    [[nodiscard]] constexpr std::optional<bool> parse_bool(const std::string_view sv) noexcept {
        if (sv == "true" || sv == "1") return true;
        if (sv == "false" || sv == "0") return false;
        return std::nullopt;
    }

    /**
     * @brief Iterate comma-separated tokens, invoking fn(index, token) for each.
     *
     * Whitespace around each token is trimmed. Stops after @p max_count tokens.
     * @param sv        Input string containing comma-separated values.
     * @param max_count Maximum number of tokens to process.
     * @param fn        Callback invoked as fn(size_t index, std::string_view token).
     */
    template<typename Fn>
        requires std::invocable<Fn, size_t, std::string_view>
    constexpr size_t for_each_csv_token(std::string_view sv, const size_t max_count, Fn fn) noexcept {
        size_t i = 0;
        for (size_t pos = 0; i < max_count && pos < sv.size(); ++i) {
            const size_t     comma = sv.find(',', pos);
            std::string_view part  = comma != std::string_view::npos ? sv.substr(pos, comma - pos) : sv.substr(pos);
            if (const auto start = part.find_first_not_of(" \t\r\n"); start != std::string_view::npos) {
                part = part.substr(start, part.find_last_not_of(" \t\r\n") - start + 1);
            } else {
                part = {};
            }
            fn(i, part);
            pos = comma != std::string_view::npos ? comma + 1 : sv.size();
        }
        return i;
    }

    /**
     * @brief Parse N comma-separated float components into a fixed-size span.
     *
     * Unparsed slots keep their initial value.
     * @tparam N Number of components to parse.
     * @param sv  Comma-separated float string.
     * @param out Output span receiving parsed values.
     * @return True if at least one component was successfully parsed.
     */
    template<size_t N>
    [[nodiscard]] constexpr bool parse_float_components(const std::string_view sv, std::span<float, N> out) noexcept {
        bool parsed = false;
        for_each_csv_token(sv, N, [&](size_t i, const std::string_view part) {
            if (auto v = try_parse<float>(part)) {
                out[i] = *v;
                parsed = true;
            }
        });
        return parsed;
    }

    /// @brief Parse 3 comma-separated floats as RGB into a span.
    [[nodiscard]] constexpr bool parse_float_rgb(const std::string_view sv, const std::span<float, 3> out) noexcept {
        return parse_float_components<3>(sv, out);
    }

    /// @brief Parse 4 comma-separated floats as RGBA into a span.
    [[nodiscard]] constexpr bool parse_float_rgba(const std::string_view sv, const std::span<float, 4> out) noexcept {
        return parse_float_components<4>(sv, out);
    }

    /// @brief Parse "x, y" comma-separated floats into an ImVec2.
    [[nodiscard]] constexpr ImVec2 parse_vec2(const std::string_view sv,
                                              const ImVec2           default_val = {0.0f, 0.0f}) noexcept {
        std::array components = {default_val.x, default_val.y};
        (void) parse_float_components<2>(sv, std::span{components});
        return {components[0], components[1]};
    }

    /// @brief Parse "x, y, z, w" comma-separated floats into an ImVec4.
    [[nodiscard]] constexpr ImVec4 parse_vec4(const std::string_view sv,
                                              const ImVec4           default_val = {0.0f, 0.0f, 0.0f, 1.0f}) noexcept {
        std::array components = {default_val.x, default_val.y, default_val.z, default_val.w};
        (void) parse_float_components<4>(sv, std::span{components});
        return {components[0], components[1], components[2], components[3]};
    }

    /// @brief Parse "r, g, b, a" integers (0-255) into a packed ImU32 color, clamped to [0,255].
    /// Also accepts #RRGGBB, #RRGGBBAA, 0xRRGGBB, 0xRRGGBBAA hex formats.
    static_assert(IM_COL32_R_SHIFT == 0 && IM_COL32_G_SHIFT == 8 && IM_COL32_B_SHIFT == 16 && IM_COL32_A_SHIFT == 24,
                  "parse_im_u32 assumes standard IM_COL32 RGBA byte order");
    [[nodiscard]] constexpr ImU32 parse_im_u32(const std::string_view sv,
                                               const ImU32            default_val = IM_COL32(0, 0, 0, 255)) noexcept {
        std::string_view hex_digits;
        if (sv.starts_with('#')) {
            hex_digits = sv.substr(1);
        } else if (sv.starts_with("0x") || sv.starts_with("0X")) {
            hex_digits = sv.substr(2);
        }

        if (!hex_digits.empty() && (hex_digits.size() == 6 || hex_digits.size() == 8)) {
            std::uint32_t packed = 0;
            if (auto [ptr, ec] = std::from_chars(hex_digits.data(), hex_digits.data() + hex_digits.size(), packed, 16);
                ec == std::errc() && ptr == hex_digits.data() + hex_digits.size()) {
                int r = 0;
                int g = 0;
                int b = 0;
                int a = 0;
                if (hex_digits.size() == 8) {
                    // RRGGBBAA
                    r = static_cast<int>((packed >> 24) & 0xFF);
                    g = static_cast<int>((packed >> 16) & 0xFF);
                    b = static_cast<int>((packed >> 8) & 0xFF);
                    a = static_cast<int>((packed >> 0) & 0xFF);
                } else {
                    // RRGGBB
                    r = static_cast<int>((packed >> 16) & 0xFF);
                    g = static_cast<int>((packed >> 8) & 0xFF);
                    b = static_cast<int>((packed >> 0) & 0xFF);
                    a = 255;
                }
                return IM_COL32(r, g, b, a);
            }
        }

        std::array components = {
            static_cast<int>(default_val >> IM_COL32_R_SHIFT & 0xFF), // R
            static_cast<int>(default_val >> IM_COL32_G_SHIFT & 0xFF), // G
            static_cast<int>(default_val >> IM_COL32_B_SHIFT & 0xFF), // B
            static_cast<int>(default_val >> IM_COL32_A_SHIFT & 0xFF), // A
        };
        for_each_csv_token(sv, 4, [&](const size_t i, const std::string_view part) {
            components[i] = std::clamp(parse_int(part, components[i]), 0, 255);
        });
        return IM_COL32(components[0], components[1], components[2], components[3]);
    }

} // namespace imgui_util::parse
