// error.hpp - UI error types with std::expected integration
//
// Usage:
//   auto result = imgui_util::validate_path(p);
//   if (!result) log(result.error().message().sv());
//
//   imgui_util::ui_expected<int> parse_config(std::string_view s);
//   return imgui_util::make_ui_error(ui_error_code::file_open_failed, "not found");
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

    enum class ui_error_code : std::uint8_t {
        path_empty,
        path_too_long,
        path_invalid_chars,
        path_invalid,
        file_open_failed,
        file_write_failed,
        file_malformed
    };

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

    struct ui_error {
        ui_error_code code;
        std::string   detail;

        explicit constexpr ui_error(const ui_error_code c) noexcept : code{c} {}
        explicit constexpr ui_error(const ui_error_code c, std::string d) : code{c}, detail{std::move(d)} {}

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

    // Type aliases for std::expected
    template<typename T>
    using ui_expected      = std::expected<T, ui_error>;
    using ui_expected_void = std::expected<void, ui_error>;

    // Convenience factory for creating unexpected errors
    [[nodiscard]] constexpr std::unexpected<ui_error> make_ui_error(const ui_error_code code) {
        return std::unexpected{ui_error{code}};
    }

    // Not constexpr: ui_error two-arg ctor takes std::string by value (heap alloc at runtime).
    [[nodiscard]] inline std::unexpected<ui_error> make_ui_error(const ui_error_code code, std::string detail) {
        return std::unexpected{ui_error{code, std::move(detail)}};
    }

    // Path validation
    constexpr size_t max_path_length = 4096;

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
        if (err.detail.empty()) return std::format_to(ctx.out(), "{}", imgui_util::to_string(err.code));
        return std::format_to(ctx.out(), "{}: {}", imgui_util::to_string(err.code), err.detail);
    }
};
