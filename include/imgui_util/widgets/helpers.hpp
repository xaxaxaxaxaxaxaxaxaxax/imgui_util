// helpers.hpp - small reusable widgets: help markers, sections, label-value rows, dialog buttons
//
// Usage:
//   imgui_util::help_marker("Tooltip text");
//   imgui_util::section_header("General");
//   imgui_util::label_value("FPS:", "60.0");
//   imgui_util::dialog_buttons("OK", "Cancel", on_ok, on_cancel);
//   imgui_util::section_builder("Details").require_valid(ptr, "No data").render([&]{ ... });
#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <format>
#include <imgui.h>
#include <span>
#include <string>
#include <string_view>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    // Shared direction enum used by splitter, toolbar, and other layout widgets
    enum class direction : uint8_t { horizontal, vertical };

    // Help marker: "(?) " with tooltip on hover
    inline void help_marker(const std::string_view tooltip) noexcept {
        ImGui::SameLine();
        colored_text("(?)", colors::inactive);
        if (const item_tooltip tt{}) {
            ImGui::TextUnformatted(tooltip.data(), tooltip.data() + tooltip.size());
        }
    }

    // Section header with accent color and separator
    inline void section_header(const std::string_view title) noexcept {
        ImGui::Spacing();
        colored_text(title, colors::accent);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // Two-column label-value layout
    inline void label_value(const std::string_view label, const std::string_view value,
                            const float label_width = 120.0f) noexcept {
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::SameLine(label_width);
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
    }

    // Label-value with compile-time checked std::format string (stack-allocated, no heap alloc)
    template<typename... Args>
    void label_value_fmt(const std::string_view label, const float label_width, std::format_string<Args...> fmt,
                         Args &&...args) {
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::SameLine(label_width);
        const fmt_buf<256> text(fmt, std::forward<Args>(args)...);
        ImGui::TextUnformatted(text.c_str(), text.end());
    }

    // Label-value with runtime format string (for callers that cannot use compile-time format checking)
    // Note: prefer label_value_fmt() when format string is known at compile time (avoids heap allocation)
    inline void label_value_rt(const std::string_view label, const float label_width, const std::string_view fmt,
                               const std::format_args args) {
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::SameLine(label_width);
        const auto text = std::vformat(fmt, args);
        ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
    }

    // Label-value with colored value
    inline void label_value_colored(const std::string_view label, const ImVec4 color, const std::string_view value,
                                    const float label_width = 120.0f) noexcept {
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::SameLine(label_width);
        const style_color guard(ImGuiCol_Text, color);
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
    }

    // Shortcut description entry (for help/keybinding panels)
    struct shortcut {
        std::string_view key;         // e.g. "Ctrl+S"
        std::string_view description; // e.g. "Save project"
    };

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

    // Collapsing section helper
    [[nodiscard]]
    inline bool section(const char *label, const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen) noexcept {
        return ImGui::CollapsingHeader(label, flags);
    }

    // Null-pointer validation with inactive text fallback
    [[nodiscard]]
    inline bool require_valid(const void *ptr, const std::string_view message) noexcept {
        if (ptr == nullptr) {
            inactive_text(message);
            return false;
        }
        return true;
    }

    // Fluent API for sections with pointer validation before rendering
    class section_builder {
    public:
        explicit section_builder(const char              *title,
                                 const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen) noexcept :
            open_(ImGui::CollapsingHeader(title, flags)) {}

        [[nodiscard]]
        section_builder &require_valid(const void *ptr, const std::string_view message) noexcept {
            if (open_ && ptr == nullptr) {
                inactive_text(message);
                open_ = false;
            }
            return *this;
        }

        template<std::invocable Fn>
        void render(Fn &&fn) noexcept(std::is_nothrow_invocable_v<Fn>) {
            if (open_) std::forward<Fn>(fn)();
        }

        [[nodiscard]]
        explicit operator bool() const noexcept {
            return open_;
        }

    private:
        bool open_;
    };

    // Standard OK/Cancel button bar for modal dialogs
    template<std::invocable OnOk, std::invocable OnCancel>
    void dialog_buttons(const char *ok_label, const char *cancel_label, OnOk &&on_ok, OnCancel &&on_cancel,
                        const float ok_width = 120.0f, const float cancel_width = 120.0f) noexcept {
        if (ImGui::Button(ok_label, ImVec2(ok_width, 0))) std::forward<OnOk>(on_ok)();
        ImGui::SameLine();
        if (ImGui::Button(cancel_label, ImVec2(cancel_width, 0))) std::forward<OnCancel>(on_cancel)();
    }

    // Safe copy of string_view to fixed-size buffer. Always null-terminates.
    // Returns true if src fit entirely; false if truncated.
    template<size_t N>
    [[nodiscard]]
    constexpr bool copy_to_buffer(std::span<char, N> buf, const std::string_view src) noexcept {
        static_assert(N > 0, "Buffer must have at least one byte for null terminator");
        const size_t len = std::min(src.size(), N - 1);
        std::copy_n(src.begin(), len, buf.data());
        buf.data()[len] = '\0'; // len is always < N, safe by construction
        return src.size() < N;
    }

    template<size_t N>
    [[nodiscard]]
    constexpr bool copy_to_buffer(std::array<char, N> &buf, const std::string_view src) noexcept {
        return copy_to_buffer(std::span<char, N>{buf}, src);
    }

} // namespace imgui_util
