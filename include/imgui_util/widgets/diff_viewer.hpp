/// @file diff_viewer.hpp
/// @brief Side-by-side diff viewer with synchronized scrolling.
///
/// Color-coded backgrounds for added/removed/changed lines. Line numbers in gutter.
/// Uses ListClipper for efficient rendering of large diffs.
///
/// Usage:
/// @code
///   static imgui_util::diff_viewer dv;
///   std::vector<imgui_util::diff_line> left = { {imgui_util::diff_kind::removed, "old line"} };
///   std::vector<imgui_util::diff_line> right = { {imgui_util::diff_kind::added, "new line"} };
///   dv.render("##diff", left, right);
/// @endcode
#pragma once

#include <cstdint>
#include <imgui.h>
#include <span>
#include <string_view>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    /// @brief Classification of a diff line.
    enum class diff_kind : std::uint8_t { same, added, removed, changed };

    /// @brief A single line in a diff, tagged with its kind.
    struct diff_line {
        diff_kind        kind;
        std::string_view text;
    };

    /// @brief Side-by-side diff viewer with synchronized scrolling and line numbers.
    class diff_viewer {
    public:
        /**
         * @brief Render the diff viewer.
         * @param str_id       ImGui string ID.
         * @param left         Lines for the left pane.
         * @param right        Lines for the right pane.
         * @param left_label   Header label for the left pane.
         * @param right_label  Header label for the right pane.
         */
        void render(std::string_view str_id, const std::span<const diff_line> left,
                    const std::span<const diff_line> right, const std::string_view left_label = "Before",
                    const std::string_view right_label = "After") const {
            const fmt_buf id_str{"{}", str_id};
            const id      scope{id_str.c_str()};

            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float  half  = avail.x * 0.5f - 4.0f;

            // Header labels
            {
                const group g;
                ImGui::TextUnformatted(left_label.data(), left_label.data() + left_label.size());
            }
            ImGui::SameLine(half + 8.0f);
            ImGui::TextUnformatted(right_label.data(), right_label.data() + right_label.size());

            // Synchronize scroll positions
            auto      *storage     = ImGui::GetStateStorage();
            const auto scroll_key  = ImGui::GetID("##diff_scroll_y");
            float      scroll_sync = storage->GetFloat(scroll_key, 0.0f);

            // Left pane
            float left_scroll = scroll_sync;
            {
                const child left_child{"##diff_left", ImVec2(half, 0), ImGuiChildFlags_Borders};
                render_pane(left, left_scroll);
            }

            // Track scroll change from left pane
            if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                scroll_sync = left_scroll;
            }

            ImGui::SameLine();

            // Right pane
            float right_scroll = scroll_sync;
            {
                const child right_child{"##diff_right", ImVec2(half, 0), ImGuiChildFlags_Borders};
                render_pane(right, right_scroll);
            }

            if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                scroll_sync = right_scroll;
            }

            storage->SetFloat(scroll_key, scroll_sync);
        }

        /// @brief Toggle line number gutter visibility.
        [[nodiscard]] diff_viewer &set_line_numbers(const bool show) noexcept {
            show_line_numbers_ = show;
            return *this;
        }

    private:
        bool show_line_numbers_ = true;

        [[nodiscard]] static ImU32 bg_color(const diff_kind kind) noexcept {
            switch (kind) {
                case diff_kind::added:
                    return IM_COL32(26, 58, 26, 255);
                case diff_kind::removed:
                    return IM_COL32(58, 26, 26, 255);
                case diff_kind::changed:
                    return IM_COL32(58, 58, 26, 255);
                case diff_kind::same:
                    return IM_COL32(0, 0, 0, 0);
            }
            return IM_COL32(0, 0, 0, 0);
        }

        void render_pane(std::span<const diff_line> lines, float &scroll_y) const {
            const auto count = static_cast<int>(lines.size());
            if (count == 0) return;

            // Set scroll position to synchronized value
            if (ImGui::GetScrollY() != scroll_y) {
                ImGui::SetScrollY(scroll_y);
            }

            const float line_h   = ImGui::GetTextLineHeightWithSpacing();
            const float gutter_w = show_line_numbers_ ? ImGui::CalcTextSize("99999 ").x : 0.0f;

            ImGuiListClipper clipper;
            clipper.Begin(count, line_h);
            auto *dl = ImGui::GetWindowDrawList();

            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto &[kind, text] = lines[static_cast<std::size_t>(i)];

                    const ImVec2 cursor_screen = ImGui::GetCursorScreenPos();
                    const float  row_w         = ImGui::GetContentRegionAvail().x;

                    // Background color
                    if (const ImU32 bg = bg_color(kind); bg != IM_COL32(0, 0, 0, 0)) {
                        dl->AddRectFilled(cursor_screen, {cursor_screen.x + row_w, cursor_screen.y + line_h}, bg);
                    }

                    // Line number gutter
                    if (show_line_numbers_) {
                        const fmt_buf<16> num{"{} ", i + 1};
                        constexpr ImVec4  dim{0.5f, 0.5f, 0.5f, 1.0f};
                        const style_color col_guard(ImGuiCol_Text, dim);
                        ImGui::TextUnformatted(num.c_str(), num.end());
                        ImGui::SameLine(gutter_w);
                    }

                    // Line text
                    ImGui::TextUnformatted(text.data(), text.data() + text.size());
                }
            }

            // Update scroll position for synchronization
            scroll_y = ImGui::GetScrollY();
        }
    };

} // namespace imgui_util
