// text.hpp - colored text, alignment helpers, truncation, and semantic color palette
//
// Usage:
//   imgui_util::colored_text("hello", imgui_util::colors::accent);
//   imgui_util::error_text("failed!");
//   imgui_util::fmt_text("FPS: {:.1f}", fps);
//   imgui_util::right_aligned_text(x, w, y, "100%", colors::success);
//   auto t = imgui_util::truncate_to_width(long_text, 200.0f);
//
// colors:: provides a consistent semantic palette (accent, warning, error, etc.).
// All text helpers use ImGui::TextUnformatted internally for performance.
#pragma once

#include <cstdint>
#include <format>
#include <imgui.h>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    // Semantic color palette for UI elements
    struct colors {
        // Primary accent - soft blue (matches theme accent)
        static constexpr ImVec4 accent{0.45f, 0.55f, 0.90f, 1.0f};
        static constexpr ImVec4 accent_hover{0.55f, 0.65f, 1.0f, 1.0f};

        // Secondary accent - teal (for highlights, node names, links)
        static constexpr ImVec4 teal{0.30f, 0.75f, 0.70f, 1.0f};
        static constexpr ImVec4 teal_dim{0.25f, 0.60f, 0.55f, 1.0f};

        // Text hierarchy
        static constexpr ImVec4 text_primary{0.95f, 0.95f, 0.97f, 1.0f};
        static constexpr ImVec4 text_secondary{0.60f, 0.60f, 0.65f, 1.0f};
        static constexpr ImVec4 text_dim{0.50f, 0.50f, 0.48f, 1.0f};
        static constexpr ImVec4 text_very_dim{0.40f, 0.40f, 0.38f, 1.0f};
        static constexpr ImVec4 text_disabled{0.35f, 0.35f, 0.33f, 1.0f};

        // Status colors
        static constexpr ImVec4 success{0.30f, 0.75f, 0.45f, 1.0f};
        static constexpr ImVec4 warning{0.90f, 0.70f, 0.25f, 1.0f};
        static constexpr ImVec4 error{1.0f, 0.3f, 0.3f, 1.0f};
        static constexpr ImVec4 error_dark{0.85f, 0.35f, 0.35f, 1.0f};

        // UI element colors
        static constexpr ImVec4 inactive{0.6f, 0.6f, 0.6f, 1.0f};
    };

    // Colored text helpers
    inline void colored_text(const std::string_view text, const ImVec4 &color) noexcept {
        const style_color guard(ImGuiCol_Text, color);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
    }

    inline void colored_text(const std::string_view text, ImU32 color) noexcept {
        const style_color guard(ImGuiCol_Text, color);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
    }

    // Semantic text variants
    inline void inactive_text(const std::string_view text) noexcept {
        colored_text(text, colors::inactive);
    }
    inline void dim_text(const std::string_view text) noexcept {
        colored_text(text, colors::text_dim);
    }
    inline void secondary_text(const std::string_view text) noexcept {
        colored_text(text, colors::text_secondary);
    }
    inline void error_text(const std::string_view text) noexcept {
        colored_text(text, colors::error);
    }

    // Centering and alignment
    [[nodiscard]]
    inline float center_text_y(const float row_y, const float row_height) noexcept {
        return row_y + ((row_height - ImGui::GetTextLineHeight()) * 0.5f);
    }

    inline void right_aligned_text(const float base_x, const float col_width, float text_y, const std::string_view text,
                                   const ImVec4 &color, const float padding = 3.0f) {
        const float tw = ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
        ImGui::SetCursorScreenPos({base_x + col_width - tw - padding, text_y});
        colored_text(text, color);
    }

    inline void centered_text(const float base_x, const float col_width, float text_y, const std::string_view text,
                              const ImVec4 &color) {
        const float tw = ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
        ImGui::SetCursorScreenPos({base_x + ((col_width - tw) * 0.5f), text_y});
        colored_text(text, color);
    }

    // Result of truncate_to_width: holds original string_view when no truncation needed,
    // or an owned string with "..." suffix when truncated (avoids allocation in the common case)
    class truncated_text {
    public:
        explicit truncated_text(std::string_view original) : data_(original) {}
        explicit truncated_text(std::string truncated) : data_(std::move(truncated)) {}

        [[nodiscard]]
        std::string_view view() const noexcept {
            if (const auto *sv = std::get_if<std::string_view>(&data_)) return *sv;
            return std::get<std::string>(data_);
        }

        [[nodiscard]]
        bool was_truncated() const noexcept {
            return std::holds_alternative<std::string>(data_);
        }

    private:
        std::variant<std::string_view, std::string> data_;
    };

    // Truncate text to fit within max_width pixels (binary search on character count)
    [[nodiscard]]
    inline truncated_text truncate_to_width(const std::string_view text, const float max_width) {
        if (ImGui::CalcTextSize(text.data(), text.data() + text.size()).x <= max_width) {
            return truncated_text{text};
        }
        constexpr std::string_view ellipsis = "...";
        const float ellipsis_w              = ImGui::CalcTextSize(ellipsis.data(), ellipsis.data() + ellipsis.size()).x;
        size_t      lo                      = 0;
        size_t      hi                      = text.size();
        while (lo < hi) {
            const size_t mid = lo + ((hi - lo + 1) / 2);
            // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage) -- two-pointer overload, size-bounded
            const float  w   = ImGui::CalcTextSize(text.data(), text.data() + mid).x + ellipsis_w;
            if (w <= max_width) {
                lo = mid;
            } else {
                hi = mid - 1;
            }
        }
        std::string result(text.data(), lo); // NOLINT(bugprone-suspicious-stringview-data-usage) -- size-bounded
        result += ellipsis;
        return truncated_text{std::move(result)};
    }

    // Grid lines
    inline void draw_vertical_grid_lines(ImDrawList *dl, float y, const float h, const std::span<const float> xs,
                                         const ImU32 color) {
        for (const float x: xs) {
            dl->AddLine({x, y}, {x, y + h}, color);
        }
    }

    // Linear fade
    [[nodiscard]]
    constexpr float linear_fade_alpha(const float elapsed, const float duration) noexcept {
        return 1.0f - (elapsed / duration);
    }

    // fmt_text: format + render in one call (stack-allocated, no heap alloc for small strings)
    template<size_t N = 64, typename... Args>
    void fmt_text(std::format_string<Args...> fmt, Args &&...args) {
        const fmt_buf<N> buf(fmt, std::forward<Args>(args)...);
        ImGui::TextUnformatted(buf.c_str(), buf.end());
    }

    // Severity level for status_message display
    enum class message_severity : std::uint8_t {
        normal = 0,
        error  = 1,
    };

    // Status message: colored error or plain text, no-op when empty
    inline void status_message(const std::string_view text, const message_severity severity) {
        if (text.empty()) return;
        if (severity == message_severity::error)
            colored_text(text, colors::error);
        else
            ImGui::TextUnformatted(text.data(), text.data() + text.size());
    }

    // ImGui InputText resize callback for std::string (pass as UserData)
    inline int input_text_callback_std_string(ImGuiInputTextCallbackData *data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            auto *str = static_cast<std::string *>(data->UserData);
            str->resize(data->BufTextLen);
            data->Buf = str->data();
        }
        return 0;
    }

    // Summary table helpers (assume a 2-column table is active)
    inline void stat_row(const char *label, const fmt_buf<> &value) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(value.c_str(), value.end());
    }

    inline void separator_row() {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Separator();
        ImGui::TableSetColumnIndex(1);
        ImGui::Separator();
    }

} // namespace imgui_util
