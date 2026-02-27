/// @file error.hpp
/// @brief UI error types with std::expected integration.
///
/// Usage:
/// @code
///   auto result = imgui_util::validate_path(p);
///   if (!result) log(result.error().message().sv());
///
///   imgui_util::ui_expected<int> parse_config(std::string_view s);
///   return imgui_util::make_ui_error(ui_error_code::file_open_failed, "not found");
/// @endcode
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <log.h>
#include <string>
#include <string_view>
#include <utility>

#include "imgui_util/core/fmt_buf.hpp"

namespace imgui_util {

    /// @brief Error codes for UI operations (path validation, file I/O).
    enum class ui_error_code : std::uint8_t {
        path_empty,
        path_too_long,
        path_invalid_chars,
        path_invalid,
        file_open_failed,
        file_write_failed,
        file_malformed
    };

    /// @brief Convert a ui_error_code to a human-readable string.
    [[nodiscard]] constexpr std::string_view to_string(const ui_error_code code) noexcept {
        using enum ui_error_code;
        switch (code) {
            case path_empty:
                return "Path cannot be empty";
            case path_too_long:
                return "Path exceeds maximum length";
            case path_invalid_chars:
                return "Path contains invalid characters";
            case path_invalid:
                return "Invalid path";
            case file_open_failed:
                return "Could not open file";
            case file_write_failed:
                return "Failed to write file";
            case file_malformed:
                return "File contains invalid data";
        }
        return "Unknown UI error";
    }

    /// @brief UI error carrying an error code and optional detail string.
    struct ui_error {
        ui_error_code code;
        std::string   detail;

        explicit constexpr ui_error(const ui_error_code c) noexcept : code{c} {}
        explicit constexpr ui_error(const ui_error_code c, std::string d) : code{c}, detail{std::move(d)} {}

        /// @brief Format the error as "code_name" or "code_name: detail".
        // NOTE: effectively constexpr in C++26 only (std::format_to_n, P2510R3).
        [[nodiscard]] constexpr fmt_buf<256> message() const {
            const auto base = to_string(code);
            if (detail.empty()) return fmt_buf<256>("{}", base);
            return fmt_buf<256>("{}: {}", base, detail);
        }

        [[nodiscard]] constexpr std::string_view code_name() const noexcept { return to_string(code); }

        [[nodiscard]] constexpr bool operator==(const ui_error &other) const noexcept {
            return code == other.code && detail == other.detail;
        }

        friend std::ostream &operator<<(std::ostream &os, const ui_error &err) { return os << err.message().sv(); }
    };

    inline std::ostream &operator<<(std::ostream &os, const ui_error_code code) {
        return os << to_string(code);
    }

    /// @brief Alias for std::expected<T, ui_error>.
    template<typename T>
    using ui_expected      = std::expected<T, ui_error>;
    /// @brief Alias for std::expected<void, ui_error>.
    using ui_expected_void = std::expected<void, ui_error>;

    /**
     * @brief Create an unexpected ui_error, optionally with a detail message.
     * @param code   The error code.
     * @param detail Additional context (empty by default).
     */
    [[nodiscard]] inline std::unexpected<ui_error> make_ui_error(const ui_error_code code, std::string detail = {}) {
        return std::unexpected{ui_error{code, std::move(detail)}};
    }

    /// @brief Maximum allowed filesystem path length for validate_path().
    constexpr size_t max_path_length = 4096;

    /**
     * @brief Validate and canonicalize a filesystem path.
     *
     * Rejects empty paths, paths exceeding max_path_length, and paths with
     * embedded null bytes. Returns a canonical path on success.
     * @param p The path to validate.
     * @return The canonical path, or a ui_error on failure.
     */
    [[nodiscard]] inline ui_expected<std::filesystem::path> validate_path(const std::filesystem::path &p) noexcept {
        if (p.empty()) return make_ui_error(ui_error_code::path_empty);
        if (p.native().size() > max_path_length) return make_ui_error(ui_error_code::path_too_long);

        // Security: check for embedded nulls (potential injection)
        if (const auto &native = p.native(); std::ranges::find(native, '\0') != native.end())
            return make_ui_error(ui_error_code::path_invalid_chars);

        try {
            return std::filesystem::exists(p) ? std::filesystem::canonical(p) : std::filesystem::weakly_canonical(p);
        } catch (const std::exception &e) {
            Log::warning("Path", "validation failed for '", p.c_str(), "': ", e.what());
            return make_ui_error(ui_error_code::path_invalid, e.what());
        } catch (...) {
            Log::warning("Path", "validation failed for '", p.c_str(), "': unknown exception");
            return make_ui_error(ui_error_code::path_invalid, "unknown exception");
        }
    }

} // namespace imgui_util

template<>
struct std::hash<imgui_util::ui_error_code> {
    constexpr std::size_t operator()(const imgui_util::ui_error_code c) const noexcept { return std::to_underlying(c); }
};

template<>
struct std::formatter<imgui_util::ui_error_code> {
    static constexpr auto parse(const std::format_parse_context &ctx) { return ctx.begin(); }

    static constexpr auto format(const imgui_util::ui_error_code code, std::format_context &ctx) {
        return std::format_to(ctx.out(), "{}", imgui_util::to_string(code));
    }
};

template<>
struct std::formatter<imgui_util::ui_error> {
    static constexpr auto parse(const std::format_parse_context &ctx) { return ctx.begin(); }

    static constexpr auto format(const imgui_util::ui_error &err, std::format_context &ctx) {
        return std::format_to(ctx.out(), "{}", err.message().sv());
    }
};
