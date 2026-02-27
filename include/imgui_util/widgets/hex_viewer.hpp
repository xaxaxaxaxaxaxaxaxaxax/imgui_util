/// @file hex_viewer.hpp
/// @brief Memory/hex byte viewer with address gutter, ASCII column, and editing.
///
/// Uses ImGui::ListClipper for large data sets. Configurable bytes per row (8, 16, 32).
///
/// Usage:
/// @code
///   static imgui_util::hex_viewer hex{16};
///   hex.add_highlight(0x10, 8, IM_COL32(255, 200, 50, 80));
///   hex.render("##hex", data_span);
///
///   // Editable mode:
///   if (hex.render_editable("##hex_edit", mutable_span, 0x1000)) { /* data changed */ }
/// @endcode
#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <imgui.h>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    /// @brief A colored byte range to highlight in the hex viewer.
    struct highlight_range {
        std::size_t offset; ///< Byte offset into the data.
        std::size_t length; ///< Number of bytes to highlight.
        ImU32       color;  ///< Background highlight color.
    };

    /**
     * @brief Memory/hex byte viewer with address gutter, ASCII column, and optional editing.
     *
     * Uses ImGuiListClipper for efficient rendering of large data sets.
     * Configurable bytes per row (8, 16, 32). Supports colored highlight ranges,
     * click-to-select, double-click-to-edit, and scroll-to-offset.
     */
    class hex_viewer {
    public:
        explicit hex_viewer(const std::size_t bytes_per_row = 16) noexcept : bytes_per_row_(bytes_per_row) {}

        /**
         * @brief Render a read-only hex view.
         * @param id           ImGui child window ID.
         * @param data         Byte data to display.
         * @param base_address Address shown in the gutter for the first byte.
         */
        void render(const std::string_view id, const std::span<const std::byte> data,
                    const std::size_t base_address = 0) {
            render_impl(id, data.data(), data.size(), base_address, false, nullptr);
        }

        /**
         * @brief Render a read-only hex view from a uint8_t span.
         * @param id           ImGui child window ID.
         * @param data         Byte data to display.
         * @param base_address Address shown in the gutter for the first byte.
         */
        void render(const std::string_view id, const std::span<const uint8_t> data,
                    const std::size_t base_address = 0) {
            render(id, std::as_bytes(data), base_address);
        }

        /**
         * @brief Render an editable hex view. Double-click a byte to edit it.
         * @param id           ImGui child window ID.
         * @param data         Mutable byte data to display and edit.
         * @param base_address Address shown in the gutter for the first byte.
         * @return True if any byte was modified this frame.
         */
        [[nodiscard]] bool render_editable(const std::string_view id, const std::span<std::byte> data,
                                           const std::size_t base_address = 0) {
            modified_ = false;
            render_impl(id, data.data(), data.size(), base_address, true, data.data());
            return modified_;
        }

        /// @brief Add a colored highlight range to the viewer (chainable).
        hex_viewer &add_highlight(const std::size_t offset, const std::size_t length, const ImU32 color) {
            highlights_.push_back({.offset = offset, .length = length, .color = color});
            return *this;
        }

        /// @brief Remove all highlight ranges (chainable).
        hex_viewer &clear_highlights() noexcept {
            highlights_.clear();
            return *this;
        }

        /// @brief Set the number of bytes displayed per row (chainable).
        hex_viewer &set_bytes_per_row(const std::size_t n) noexcept {
            bytes_per_row_ = n;
            return *this;
        }

        /// @brief Return the byte offset of the currently selected byte, if any.
        [[nodiscard]] std::optional<std::size_t> selected_offset() const noexcept { return selected_; }

        /// @brief Scroll the view so that @p offset is visible on the next frame.
        void scroll_to(const std::size_t offset) noexcept { scroll_target_ = offset; }

    private:
        struct row_layout {
            float char_w;
            float space_w;
            float addr_w;
            float byte_w;
            float group_gap;
        };

        std::size_t                  bytes_per_row_;
        std::vector<highlight_range> highlights_;
        std::optional<std::size_t>   selected_;
        std::optional<std::size_t>   scroll_target_;
        // editing state
        std::optional<std::size_t>   editing_offset_;
        std::array<char, 3>          edit_buf_{};
        bool                         modified_        = false;
        float                        cached_char_w_   = 0.0f;
        float                        cached_space_w_  = 0.0f;
        ImFont                      *cached_font_     = nullptr;

        void draw_row_backgrounds(const ImVec2 row_pos, const float row_h, const std::size_t row_offset,
                                  const std::size_t row_bytes, const row_layout &lay) const {
            auto *dl = ImGui::GetWindowDrawList();

            if (selected_.has_value() && *selected_ >= row_offset && *selected_ < row_offset + bytes_per_row_) {
                dl->AddRectFilled(row_pos, {row_pos.x + ImGui::GetContentRegionAvail().x, row_pos.y + row_h},
                                  IM_COL32(255, 255, 255, 20));
            }

            const auto group_gaps_before = [&](const std::size_t col) -> float {
                if (bytes_per_row_ <= 8) return 0.0f;
                // Integer division is intentional: count complete 8-byte groups.
                return static_cast<float>(col / 8) * lay.group_gap; // NOLINT(bugprone-integer-division)
            };

            for (const auto &[hl_offset, hl_length, hl_color]: highlights_) {
                if (hl_offset + hl_length <= row_offset || hl_offset >= row_offset + row_bytes) continue;
                const std::size_t hl_start = hl_offset > row_offset ? hl_offset - row_offset : 0;
                const std::size_t hl_end   = std::min(row_bytes, hl_offset + hl_length - row_offset);

                const float x0 =
                    row_pos.x + lay.addr_w + static_cast<float>(hl_start) * lay.byte_w + group_gaps_before(hl_start);
                const float x1 = row_pos.x + lay.addr_w + static_cast<float>(hl_end) * lay.byte_w
                    + group_gaps_before(hl_end) - lay.space_w;
                dl->AddRectFilled({x0, row_pos.y}, {x1, row_pos.y + row_h}, hl_color);
            }
        }

        void render_byte_editor(const std::size_t byte_offset, const float char_w, std::byte *mutable_data) {
            ImGui::SetNextItemWidth(char_w * 2.0f + 4.0f);
            const id byte_id{static_cast<int>(byte_offset)};
            if (!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();

            if (ImGui::InputText("##edit", edit_buf_.data(), edit_buf_.size(),
                                 ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue
                                     | ImGuiInputTextFlags_AutoSelectAll)) {
                unsigned int parsed  = 0;
                const auto  *buf_end = edit_buf_.data() + std::strlen(edit_buf_.data());
                if (auto [ptr, ec] = std::from_chars(edit_buf_.data(), buf_end, parsed, 16);
                    ec != std::errc::invalid_argument && ec != std::errc::result_out_of_range) {
                    mutable_data[byte_offset] = static_cast<std::byte>(parsed & 0xFFu);
                    modified_                 = true;
                }
                editing_offset_.reset();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) editing_offset_.reset();
        }

        void render_byte_display(const std::size_t byte_offset, const std::uint8_t byte_val, const bool editable) {
            const fmt_buf<4> hex_str("{:02X}", byte_val);
            ImGui::TextUnformatted(hex_str.c_str(), hex_str.end());

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) selected_ = byte_offset;

            if (editable && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                editing_offset_ = byte_offset;
                const fmt_buf<4> tmp("{:02X}", byte_val);
                std::copy_n(tmp.c_str(), std::min(tmp.size() + 1, edit_buf_.size()), edit_buf_.data());
            }
        }

        static void render_ascii_column(const std::byte *data, const std::size_t row_offset,
                                        const std::size_t row_bytes, const float space_w) {
            ImGui::SameLine(0.0f, space_w);
            ImGui::TextUnformatted("|");
            ImGui::SameLine(0.0f, space_w);

            std::array<char, 64> ascii_buf{};
            for (std::size_t col = 0; col < row_bytes && col < ascii_buf.size(); ++col) {
                const auto c   = static_cast<char>(data[row_offset + col]);
                ascii_buf[col] = c >= 32 && c < 127 ? c : '.';
            }
            ImGui::TextUnformatted(ascii_buf.data(), ascii_buf.data() + row_bytes);
        }

        void render_row(const int row_i, const std::byte *data, const std::size_t data_size,
                        const std::size_t base_address, const bool editable, std::byte *mutable_data,
                        const row_layout &lay) {
            const auto        row        = static_cast<std::size_t>(row_i);
            const std::size_t row_offset = row * bytes_per_row_;
            const std::size_t row_bytes  = std::min(bytes_per_row_, data_size - row_offset);
            const std::size_t addr       = base_address + row_offset;

            const id     row_id{row_i};
            const ImVec2 row_pos = ImGui::GetCursorScreenPos();
            const float  row_h   = ImGui::GetTextLineHeightWithSpacing();

            draw_row_backgrounds(row_pos, row_h, row_offset, row_bytes, lay);

            const fmt_buf<16> addr_buf("0x{:08X}: ", addr);
            ImGui::TextUnformatted(addr_buf.c_str(), addr_buf.end());
            ImGui::SameLine(0.0f, 0.0f);

            for (std::size_t col = 0; col < bytes_per_row_; ++col) {
                if (col > 0 && col % 8 == 0)
                    ImGui::SameLine(0.0f, lay.group_gap);
                else if (col > 0)
                    ImGui::SameLine(0.0f, lay.space_w);

                if (col < row_bytes) {
                    const std::size_t byte_offset = row_offset + col;
                    const auto        byte_val    = static_cast<std::uint8_t>(data[byte_offset]);

                    if (editable && editing_offset_.has_value() && *editing_offset_ == byte_offset)
                        render_byte_editor(byte_offset, lay.char_w, mutable_data);
                    else
                        render_byte_display(byte_offset, byte_val, editable);
                } else {
                    ImGui::TextUnformatted("  ");
                }
            }

            render_ascii_column(data, row_offset, row_bytes, lay.space_w);

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) selected_ = row_offset;
        }

        void render_impl(const std::string_view id, const std::byte *data, const std::size_t data_size,
                         const std::size_t base_address, const bool editable, std::byte *mutable_data) {
            if (bytes_per_row_ == 0) bytes_per_row_ = 16;

            const std::size_t total_rows = (data_size + bytes_per_row_ - 1) / bytes_per_row_;

            if (ImFont *const font = ImGui::GetFont(); font != cached_font_) {
                cached_char_w_  = ImGui::CalcTextSize("F").x;
                cached_space_w_ = ImGui::CalcTextSize(" ").x;
                cached_font_    = font;
            }
            const float      char_w  = cached_char_w_;
            const float      space_w = cached_space_w_;
            const row_layout lay{
                .char_w    = char_w,
                .space_w   = space_w,
                .addr_w    = char_w * 12.0f + space_w,
                .byte_w    = char_w * 2.0f + space_w,
                .group_gap = space_w * 2.0f,
            };

            const fmt_buf<32> child_id("{}", id);
            if (const child child_scope{child_id.c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders}) {
                if (scroll_target_.has_value()) {
                    const std::size_t target_row = *scroll_target_ / bytes_per_row_;
                    const float       target_y = static_cast<float>(target_row) * ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetScrollY(target_y);
                    scroll_target_.reset();
                }

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(total_rows));

                while (clipper.Step()) {
                    for (int row_i = clipper.DisplayStart; row_i < clipper.DisplayEnd; ++row_i) {
                        render_row(row_i, data, data_size, base_address, editable, mutable_data, lay);
                    }
                }
            }
        }
    };

} // namespace imgui_util
