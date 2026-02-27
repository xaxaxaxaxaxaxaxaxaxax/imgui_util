/// @file helpers.hpp
/// @brief Small reusable widgets: help markers, sections, label-value rows, dialog buttons.
///
/// Usage:
/// @code
///   imgui_util::help_marker("Tooltip text");
///   imgui_util::section_header("General");
///   imgui_util::label_value("FPS:", "60.0");
///   imgui_util::dialog_buttons("OK", "Cancel", on_ok, on_cancel);
///   imgui_util::section_builder("Details").require_valid(ptr, "No data").render([&]{ ... });
/// @endcode
#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <format>
#include <imgui.h>
#include <span>
#include <string>
#include <string_view>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    /// @brief Render an inline "(?) " help marker with a hover tooltip.
    inline void help_marker(const std::string_view tooltip) noexcept {
        ImGui::SameLine();
        colored_text("(?)", colors::inactive);
        if (const item_tooltip tt{}) {
            ImGui::TextUnformatted(tooltip.data(), tooltip.data() + tooltip.size());
        }
    }

    /// @brief Render an accented section header with separator.
    inline void section_header(const std::string_view title) noexcept {
        ImGui::Spacing();
        colored_text(title, colors::accent);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    /**
     * @brief Render a label-value pair in a fixed-width two-column layout.
     * @param label        Left-side label text.
     * @param value        Right-side value text.
     * @param label_width  Column width for the label in pixels.
     */
    inline void label_value(const std::string_view label, const std::string_view value,
                            const float label_width = 120.0f) noexcept {
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::SameLine(label_width);
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
    }

    /**
     * @brief Label-value with compile-time checked std::format string (stack-allocated, no heap alloc).
     * @param label        Left-side label text.
     * @param label_width  Column width for the label in pixels.
     * @param fmt          Compile-time format string.
     * @param args         Format arguments.
     */
    template<typename... Args>
    void label_value_fmt(const std::string_view label, const float label_width, std::format_string<Args...> fmt,
                         Args &&...args) {
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::SameLine(label_width);
        const fmt_buf<256> text(fmt, std::forward<Args>(args)...);
        ImGui::TextUnformatted(text.c_str(), text.end());
    }

    /**
     * @brief Label-value with runtime format string (heap-allocates).
     *
     * Prefer label_value_fmt() when the format string is known at compile time.
     *
     * @param label        Left-side label text.
     * @param label_width  Column width for the label in pixels.
     * @param fmt          Runtime format string.
     * @param args         Format arguments.
     */
    inline void label_value_rt(const std::string_view label, const float label_width, const std::string_view fmt,
                               const std::format_args args) {
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::SameLine(label_width);
        const auto text = std::vformat(fmt, args);
        ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
    }

    /**
     * @brief Label-value pair where the value is rendered in a custom color.
     * @param label        Left-side label text.
     * @param color        Text color for the value.
     * @param value        Right-side value text.
     * @param label_width  Column width for the label in pixels.
     */
    inline void label_value_colored(const std::string_view label, const ImVec4 color, const std::string_view value,
                                    const float label_width = 120.0f) noexcept {
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::SameLine(label_width);
        const style_color guard(ImGuiCol_Text, color);
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
    }

    /// @brief A keyboard shortcut entry for display in shortcut_list().
    struct shortcut {
        std::string_view key;         ///< Key combination, e.g. "Ctrl+S".
        std::string_view description; ///< Human-readable action, e.g. "Save project".
    };

    /// @brief Render a titled list of keyboard shortcuts.
    inline void shortcut_list(const std::string_view title, std::span<const shortcut> shortcuts) noexcept {
        colored_text(title, colors::accent);
        ImGui::Spacing();
        for (const auto &[key, desc]: shortcuts) {
            ImGui::Bullet();
            ImGui::SameLine();
            const fmt_buf<256> line("{} - {}", key, desc);
            ImGui::TextUnformatted(line.c_str(), line.end());
        }
    }

    /// @brief Render a collapsing header. Returns true if the section is open.
    [[nodiscard]] inline bool section(const char              *label,
                                      const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen) noexcept {
        return ImGui::CollapsingHeader(label, flags);
    }

    /// @brief Show a dimmed message and return false if ptr is null.
    [[nodiscard]] inline bool require_valid(const void *ptr, const std::string_view message) noexcept {
        if (ptr == nullptr) {
            inactive_text(message);
            return false;
        }
        return true;
    }

    /// @brief Collapsing section with optional null-pointer guard and deferred body rendering.
    class section_builder {
    public:
        explicit section_builder(const char              *title,
                                 const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen) noexcept :
            open_(ImGui::CollapsingHeader(title, flags)) {}

        /// @brief Guard: if ptr is null, show message and suppress the body.
        [[nodiscard]] section_builder &require_valid(const void *ptr, const std::string_view message) noexcept {
            if (open_ && ptr == nullptr) {
                inactive_text(message);
                open_ = false;
            }
            return *this;
        }

        /// @brief Invoke fn if the section is open and all guards passed.
        template<std::invocable Fn>
        void render(Fn &&fn) noexcept(std::is_nothrow_invocable_v<Fn>) {
            if (open_) std::forward<Fn>(fn)();
        }

        [[nodiscard]] explicit operator bool() const noexcept { return open_; }

    private:
        bool open_;
    };

    /**
     * @brief Render a pair of OK/Cancel buttons.
     * @param ok_label      Text for the confirm button.
     * @param cancel_label  Text for the cancel button.
     * @param on_ok         Callback invoked on confirm.
     * @param on_cancel     Callback invoked on cancel.
     * @param ok_width      Confirm button width in pixels.
     * @param cancel_width  Cancel button width in pixels.
     */
    template<std::invocable OnOk, std::invocable OnCancel>
    void dialog_buttons(const char *ok_label, const char *cancel_label, OnOk &&on_ok, OnCancel &&on_cancel,
                        const float ok_width = 120.0f, const float cancel_width = 120.0f) noexcept {
        if (ImGui::Button(ok_label, ImVec2(ok_width, 0))) std::forward<OnOk>(on_ok)();
        ImGui::SameLine();
        if (ImGui::Button(cancel_label, ImVec2(cancel_width, 0))) std::forward<OnCancel>(on_cancel)();
    }

    /**
     * @brief Safe copy of string_view to fixed-size buffer. Always null-terminates.
     * @param buf  Destination buffer (must have at least one byte for null terminator).
     * @param src  Source string view.
     * @return True if src fit entirely; false if truncated.
     */
    template<std::size_t N>
    [[nodiscard]] constexpr bool copy_to_buffer(std::span<char, N> buf, const std::string_view src) noexcept {
        static_assert(N > 0, "Buffer must have at least one byte for null terminator");
        const std::size_t len = std::min(src.size(), N - 1);
        std::copy_n(src.begin(), len, buf.data());
        buf.data()[len] = '\0'; // len is always < N, safe by construction
        return src.size() < N;
    }

    /// @brief Overload accepting std::array directly.
    template<std::size_t N>
    [[nodiscard]] constexpr bool copy_to_buffer(std::array<char, N> &buf, const std::string_view src) noexcept {
        return copy_to_buffer(std::span<char, N>{buf}, src);
    }

} // namespace imgui_util
