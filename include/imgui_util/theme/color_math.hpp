/// @file color_math.hpp
/// @brief Constexpr RGB math and ImU32/ImVec4 conversions.
///
/// Usage:
/// @code
///   using imgui_util::color::rgb_color;
///   constexpr rgb_color base = {{{0.2f, 0.3f, 0.5f}}};
///   ImVec4 col  = rgb(base);                // rgb_color -> ImVec4
///   ImVec4 dim  = scale(base, 0.8f);        // per-channel multiply
///   ImVec4 lit  = offset(base, 0.1f);       // per-channel add
///   ImU32  packed = float4_to_u32(col);      // ImVec4 -> packed RGBA
///   ImVec4 back   = u32_to_float4(packed);   // packed RGBA -> ImVec4
/// @endcode

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <imgui.h>

namespace imgui_util::color {

    /// @brief Strong type for RGB color values (channels in [0, 1]).
    ///
    /// All arithmetic operators clamp results to [0, 1].
    struct rgb_color {
        std::array<float, 3> channels{};

        constexpr float &operator[](const std::size_t i) {
            return channels[i];
        } // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        constexpr const float &operator[](const std::size_t i) const {
            return channels[i];
        } // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

        [[nodiscard]] constexpr const float *data() const noexcept { return channels.data(); }
        constexpr float                     *data() noexcept { return channels.data(); }
        constexpr bool                       operator==(const rgb_color &) const noexcept = default;

        /**
         * @brief Apply a binary operation to each channel, clamping results to [0, 1].
         * @param c  Source color.
         * @param v  Scalar operand passed to @p op alongside each channel.
         * @param op Binary callable `(float channel, float v) -> float`.
         * @return   New rgb_color with clamped results.
         */
        static constexpr rgb_color map(const rgb_color &c, const float v, auto op) noexcept {
            return {{{std::clamp(op(c.channels[0], v), 0.0f, 1.0f), std::clamp(op(c.channels[1], v), 0.0f, 1.0f),
                      std::clamp(op(c.channels[2], v), 0.0f, 1.0f)}}};
        }

        constexpr friend rgb_color operator+(const rgb_color a, const float d) noexcept {
            return map(a, d, [](const float x, const float y) { return x + y; });
        }

        constexpr friend rgb_color operator*(const rgb_color a, const float f) noexcept {
            return map(a, f, [](const float x, const float y) { return x * y; });
        }

        constexpr friend rgb_color operator-(const rgb_color a, const float d) noexcept {
            return map(a, d, [](const float x, const float y) { return x - y; });
        }

        constexpr rgb_color &operator+=(const float d) noexcept { return *this = *this + d; }
        constexpr rgb_color &operator-=(const float d) noexcept { return *this = *this - d; }
        constexpr rgb_color &operator*=(const float f) noexcept { return *this = *this * f; }
    };

    /**
     * @brief Convert individual RGBA floats to an ImVec4.
     * @param r Red channel.
     * @param g Green channel.
     * @param b Blue channel.
     * @param a Alpha channel (defaults to 1.0).
     */
    [[nodiscard]] constexpr ImVec4 rgb(float r, float g, float b, float a = 1.0f) noexcept {
        return {r, g, b, a};
    }
    /**
     * @brief Convert an rgb_color to an ImVec4.
     * @param c Source color.
     * @param a Alpha channel (defaults to 1.0).
     */
    [[nodiscard]] constexpr ImVec4 rgb(const rgb_color &c, float a = 1.0f) noexcept {
        return {c.channels[0], c.channels[1], c.channels[2],
                a}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }

    /**
     * @brief Multiply each channel of an rgb_color by a factor and return as ImVec4.
     * @param c      Source color.
     * @param factor Multiplication factor.
     * @param a      Alpha channel (defaults to 1.0).
     */
    [[nodiscard]] constexpr ImVec4 scale(const rgb_color &c, const float factor, const float a = 1.0f) noexcept {
        return rgb(c * factor, a);
    }

    /**
     * @brief Add a delta to each channel of an rgb_color and return as ImVec4.
     * @param c     Source color.
     * @param delta Value added to each channel.
     * @param a     Alpha channel (defaults to 1.0).
     */
    [[nodiscard]] constexpr ImVec4 offset(const rgb_color &c, const float delta, const float a = 1.0f) noexcept {
        return rgb(c + delta, a);
    }

    /**
     * @brief Add amount to each RGB channel of an ImVec4, clamping to [0,1] and preserving alpha.
     * @param color  Source color.
     * @param amount Value added to each RGB channel.
     */
    [[nodiscard]] constexpr ImVec4 offset(const ImVec4 &color, const float amount) noexcept {
        return {std::clamp(color.x + amount, 0.0f, 1.0f), std::clamp(color.y + amount, 0.0f, 1.0f),
                std::clamp(color.z + amount, 0.0f, 1.0f), color.w};
    }

    /// @brief Unpack a packed RGBA ImU32 into an ImVec4 with channels in [0, 1].
    [[nodiscard]] constexpr ImVec4 u32_to_float4(const ImU32 c) noexcept {
        constexpr float inv = 1.0f / 255.0f;
        return {static_cast<float>(c >> IM_COL32_R_SHIFT & 0xFF) * inv,
                static_cast<float>(c >> IM_COL32_G_SHIFT & 0xFF) * inv,
                static_cast<float>(c >> IM_COL32_B_SHIFT & 0xFF) * inv,
                static_cast<float>(c >> IM_COL32_A_SHIFT & 0xFF) * inv};
    }

    /// @brief Pack an ImVec4 into a packed RGBA ImU32, clamping each channel to [0, 1].
    [[nodiscard]] constexpr ImU32 float4_to_u32(const ImVec4 &c) noexcept {
        auto sat = [](const float f) constexpr -> ImU32 {
            const auto clamped = std::clamp(f, 0.0f, 1.0f);
            return static_cast<ImU32>(clamped * 255.0f + 0.5f); // NOLINT(bugprone-incorrect-roundings)
        };
        return sat(c.x) << IM_COL32_R_SHIFT | sat(c.y) << IM_COL32_G_SHIFT | sat(c.z) << IM_COL32_B_SHIFT
            | sat(c.w) << IM_COL32_A_SHIFT;
    }

    /**
     * @brief Offset the RGB channels of a packed ImU32 color by an integer delta, replacing alpha.
     *
     * Useful for deriving grid line colors from a grid background.
     * @param color Source packed color.
     * @param delta Integer offset applied to each RGB channel (clamped to [0, 255]).
     * @param alpha Replacement alpha value.
     */
    [[nodiscard]] constexpr ImU32 offset_u32_rgb(const ImU32 color, const int delta, const uint8_t alpha) noexcept {
        const int r = std::clamp(static_cast<int>(color >> IM_COL32_R_SHIFT & 0xFF) + delta, 0, 255);
        const int g = std::clamp(static_cast<int>(color >> IM_COL32_G_SHIFT & 0xFF) + delta, 0, 255);
        const int b = std::clamp(static_cast<int>(color >> IM_COL32_B_SHIFT & 0xFF) + delta, 0, 255);
        return IM_COL32(r, g, b, alpha);
    }

} // namespace imgui_util::color
