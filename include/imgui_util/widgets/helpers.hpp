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
#include <format>
#include <imgui.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    // Help marker: "(?) " with tooltip on hover
    inline void help_marker(const char *tooltip) {
        ImGui::SameLine();
        colored_text("(?)", colors::inactive);
        if (ImGui::IsItemHovered()) {
            if (ImGui::BeginTooltip()) {
                ImGui::TextUnformatted(tooltip);
                ImGui::EndTooltip();
            }
        }
    }

    // Section header with accent color and separator
    inline void section_header(const char *title) {
        ImGui::Spacing();
        colored_text(title, colors::accent);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // Two-column label-value layout
    inline void label_value(const char *label, const char *value, const float label_width = 120.0f) {
        ImGui::TextUnformatted(label);
        ImGui::SameLine(label_width);
        ImGui::TextUnformatted(value);
    }

    // Label-value with compile-time checked std::format string (stack-allocated, no heap alloc)
    template<typename... Args>
    void label_value_fmt(const char *label, const float label_width, std::format_string<Args...> fmt, Args &&...args) {
        ImGui::TextUnformatted(label);
        ImGui::SameLine(label_width);
        const fmt_buf<256> text(fmt, std::forward<Args>(args)...);
        ImGui::TextUnformatted(text.c_str(), text.end());
    }

    // Label-value with runtime format string (for callers that cannot use compile-time format checking)
    inline void label_value_rt(const char *label, const float label_width, std::string_view fmt,
                               std::format_args args) {
        ImGui::TextUnformatted(label);
        ImGui::SameLine(label_width);
        const auto text = std::vformat(fmt, args);
        ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
    }

    // Label-value with colored value
    inline void label_value_colored(const char *label, const ImVec4 color, const char *value,
                                    const float label_width = 120.0f) {
        ImGui::TextUnformatted(label);
        ImGui::SameLine(label_width);
        const style_color guard(ImGuiCol_Text, color);
        ImGui::TextUnformatted(value);
    }

    // Shortcut description entry (for help/keybinding panels)
    struct shortcut {
        std::string_view key;         // e.g. "Ctrl+S"
        std::string_view description; // e.g. "Save project"
    };

    inline void shortcut_list(const char *title, std::span<const shortcut> shortcuts) {
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
    inline bool section(const char *label, const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen) {
        return ImGui::CollapsingHeader(label, flags);
    }

    // Null-pointer validation with inactive text fallback
    [[nodiscard]]
    inline bool require_valid(const void *ptr, const char *message) {
        if (ptr == nullptr) {
            inactive_text(message);
            return false;
        }
        return true;
    }

    // Fluent API for sections with pointer validation before rendering
    class section_builder {
    public:
        explicit section_builder(const char *title, const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen) :
            open_(ImGui::CollapsingHeader(title, flags)) {}

        section_builder &require_valid(const void *ptr, const char *message) {
            if (open_ && (ptr == nullptr)) {
                inactive_text(message);
                open_ = false;
            }
            return *this;
        }

        template<std::invocable Fn>
        void render(Fn &&fn) {
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
                        const float ok_width = 120.0f, const float cancel_width = 120.0f) {
        if (ImGui::Button(ok_label, ImVec2(ok_width, 0))) std::forward<OnOk>(on_ok)();
        ImGui::SameLine();
        if (ImGui::Button(cancel_label, ImVec2(cancel_width, 0))) std::forward<OnCancel>(on_cancel)();
    }

    // Safe copy of string_view to fixed-size buffer. Always null-terminates.
    // Returns true if src fit entirely; false if truncated.
    template<size_t N>
    [[nodiscard]]
    constexpr bool copy_to_buffer(std::span<char, N> buf, std::string_view src) noexcept {
        static_assert(N > 0, "Buffer must have at least one byte for null terminator");
        const size_t len = std::min(src.size(), N - 1);
        std::copy_n(src.begin(), len, buf.data());
        buf.data()[len] = '\0'; // len is always < N, safe by construction
        return src.size() < N;
    }

    template<size_t N>
    [[nodiscard]]
    constexpr bool copy_to_buffer(std::array<char, N> &buf, std::string_view src) noexcept {
        return copy_to_buffer(std::span<char, N>{buf}, src);
    }


    // Type-safe mutable bool reference (avoids raw bool* in widget APIs)
    class toggle_ref {
    public:
        constexpr explicit toggle_ref(bool &ref) noexcept : ref_(&ref) {}

        constexpr void toggle() const noexcept { *ref_ = !*ref_; }
        constexpr void set(const bool value) const noexcept { *ref_ = value; }

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept {
            return *ref_;
        }

    private:
        bool *ref_;
    };

    using optional_toggle = std::optional<toggle_ref>; // nullopt = no toggle, has_value = toggleable

} // namespace imgui_util
