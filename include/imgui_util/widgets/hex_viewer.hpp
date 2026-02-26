// hex_viewer.hpp - Memory/hex byte viewer with address gutter, ASCII column, and editing
//
// Usage:
//   static imgui_util::hex_viewer hex{16};
//   hex.add_highlight(0x10, 8, IM_COL32(255, 200, 50, 80));
//   hex.render("##hex", data_span);
//
//   // Editable mode:
//   if (hex.render_editable("##hex_edit", mutable_span, 0x1000)) { /* data changed */ }
//
// Uses ImGui::ListClipper for large data sets. Configurable bytes per row (8, 16, 32).
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

    struct highlight_range {
        std::size_t offset;
        std::size_t length;
        ImU32       color;
    };

    class hex_viewer {
    public:
        explicit hex_viewer(const std::size_t bytes_per_row = 16) noexcept :
            bytes_per_row_(bytes_per_row) {}

        void render(const std::string_view id, const std::span<const std::byte> data,
                    const std::size_t base_address = 0) {
            render_impl(id, data.data(), data.size(), base_address, false, nullptr);
        }

        [[nodiscard]]
        bool render_editable(const std::string_view id, const std::span<std::byte> data,
                             const std::size_t base_address = 0) {
            modified_ = false;
            render_impl(id, data.data(), data.size(), base_address, true, data.data());
            return modified_;
        }

        hex_viewer &add_highlight(const std::size_t offset, const std::size_t length, const ImU32 color) {
            highlights_.push_back({.offset=offset, .length=length, .color=color});
            return *this;
        }

        hex_viewer &clear_highlights() noexcept {
            highlights_.clear();
            return *this;
        }

        hex_viewer &set_bytes_per_row(const std::size_t n) noexcept {
            bytes_per_row_ = n;
            return *this;
        }

        [[nodiscard]]
        std::optional<std::size_t> selected_offset() const noexcept {
            return selected_;
        }

        void scroll_to(const std::size_t offset) noexcept {
            scroll_target_ = offset;
        }

    private:
        std::size_t                  bytes_per_row_;
        std::vector<highlight_range> highlights_;
        std::optional<std::size_t>   selected_;
        std::optional<std::size_t>   scroll_target_;
        // editing state
        std::optional<std::size_t> editing_offset_;
        std::array<char, 3>        edit_buf_{};
        bool                       modified_ = false;

        void render_impl(const std::string_view id, const std::byte *data, const std::size_t data_size,
                         const std::size_t base_address, const bool editable,
                         std::byte *mutable_data) {
            if (bytes_per_row_ == 0) bytes_per_row_ = 16;

            const std::size_t total_rows = (data_size + bytes_per_row_ - 1) / bytes_per_row_;

            // Pre-calculate column widths using monospace character size
            const float char_w    = ImGui::CalcTextSize("F").x;
            const float space_w   = ImGui::CalcTextSize(" ").x;
            const float addr_w    = (char_w * 12.0f) + space_w;   // "0x" + 8 hex digits + ": "
            const float byte_w    = (char_w * 2.0f) + space_w;    // each byte: 2 hex chars + 1 space
            const float group_gap = space_w * 2.0f;              // extra gap every 8 bytes

            const fmt_buf<32> child_id("{}",  id);
            if (const child child_scope{child_id.c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders}) {
                // Handle scroll-to target
                if (scroll_target_.has_value()) {
                    const std::size_t target_row = *scroll_target_ / bytes_per_row_;
                    const float target_y = static_cast<float>(target_row) * ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetScrollY(target_y);
                    scroll_target_.reset();
                }

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(total_rows));

                while (clipper.Step()) {
                    for (int row_i = clipper.DisplayStart; row_i < clipper.DisplayEnd; ++row_i) {
                        const auto row = static_cast<std::size_t>(row_i);
                        const std::size_t row_offset = row * bytes_per_row_;
                        const std::size_t row_bytes  = std::min(bytes_per_row_, data_size - row_offset);
                        const std::size_t addr       = base_address + row_offset;

                        const imgui_util::id row_id{row_i};

                        const ImVec2 row_pos = ImGui::GetCursorScreenPos();
                        const float  row_h   = ImGui::GetTextLineHeightWithSpacing();
                        auto        *dl      = ImGui::GetWindowDrawList();

                        // Row selection highlight
                        if (selected_.has_value() &&
                            *selected_ >= row_offset && *selected_ < row_offset + bytes_per_row_) {
                            dl->AddRectFilled(
                                row_pos,
                                {row_pos.x + ImGui::GetContentRegionAvail().x, row_pos.y + row_h},
                                IM_COL32(255, 255, 255, 20));
                        }

                        // Draw highlight ranges for this row
                        for (const auto &[hl_offset, hl_length, hl_color] : highlights_) {
                            if (hl_offset + hl_length <= row_offset || hl_offset >= row_offset + row_bytes) continue;
                            const std::size_t hl_start = hl_offset > row_offset ? hl_offset - row_offset : 0;
                            const std::size_t hl_end   = std::min(row_bytes, hl_offset + hl_length - row_offset);

                            const auto group_gaps_before = [&](const std::size_t col) -> float {
                                if (bytes_per_row_ <= 8) return 0.0f;
                                return static_cast<float>(col / 8) * group_gap;
                            };

                            const float x0 = row_pos.x + addr_w +
                                             static_cast<float>(hl_start) * byte_w + group_gaps_before(hl_start);
                            const float x1 = row_pos.x + addr_w +
                                             static_cast<float>(hl_end) * byte_w + group_gaps_before(hl_end) - space_w;
                            dl->AddRectFilled({x0, row_pos.y}, {x1, row_pos.y + row_h}, hl_color);
                        }

                        // Address gutter
                        const fmt_buf<16> addr_buf("0x{:08X}: ", addr);
                        ImGui::TextUnformatted(addr_buf.c_str(), addr_buf.end());
                        ImGui::SameLine(0.0f, 0.0f);

                        // Hex bytes
                        for (std::size_t col = 0; col < bytes_per_row_; ++col) {
                            if (col > 0 && col % 8 == 0) {
                                ImGui::SameLine(0.0f, group_gap);
                            } else if (col > 0) {
                                ImGui::SameLine(0.0f, space_w);
                            }

                            if (col < row_bytes) {
                                const std::size_t byte_offset = row_offset + col;
                                const auto byte_val = static_cast<std::uint8_t>(data[byte_offset]);

                                // Editing mode
                                if (editable && editing_offset_.has_value() && *editing_offset_ == byte_offset) {
                                    ImGui::SetNextItemWidth(char_w * 2.0f + 4.0f);
                                    const imgui_util::id byte_id{static_cast<int>(col)};
                                    // Auto-focus the input on the first frame
                                    if (!ImGui::IsAnyItemActive()) {
                                        ImGui::SetKeyboardFocusHere();
                                    }
                                    if (ImGui::InputText("##edit", edit_buf_.data(), edit_buf_.size(),
                                                         ImGuiInputTextFlags_CharsHexadecimal |
                                                         ImGuiInputTextFlags_EnterReturnsTrue |
                                                         ImGuiInputTextFlags_AutoSelectAll)) {
                                        // Parse hex and write back
                                        unsigned int parsed = 0;
                                        const auto *buf_end = edit_buf_.data() + std::strlen(edit_buf_.data());
                                        if (auto [ptr, ec] = std::from_chars(edit_buf_.data(), buf_end, parsed, 16);
                                            ec == std::errc{} && mutable_data) {
                                            mutable_data[byte_offset] = static_cast<std::byte>(parsed & 0xFFu);
                                            modified_ = true;
                                        }
                                        editing_offset_.reset();
                                    }
                                    // Cancel on Escape
                                    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                                        editing_offset_.reset();
                                    }
                                } else {
                                    const fmt_buf<4> hex_str("{:02X}", byte_val);
                                    ImGui::TextUnformatted(hex_str.c_str(), hex_str.end());

                                    // Click to select
                                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
                                        selected_ = byte_offset;
                                    }
                                    // Double-click to edit
                                    if (editable && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                                        editing_offset_ = byte_offset;
                                        const fmt_buf<4> tmp("{:02X}", byte_val);
                                        std::copy_n(tmp.c_str(), std::min(tmp.size() + 1, edit_buf_.size()), edit_buf_.data());
                                    }
                                }
                            } else {
                                // Padding for incomplete rows
                                ImGui::TextUnformatted("  ");
                            }
                        }

                        // ASCII separator and column
                        ImGui::SameLine(0.0f, space_w);
                        ImGui::TextUnformatted("|");
                        ImGui::SameLine(0.0f, space_w);

                        // Build ASCII string for this row
                        std::array<char, 64> ascii_buf{};
                        for (std::size_t col = 0; col < row_bytes && col < ascii_buf.size(); ++col) {
                            const auto c = static_cast<char>(data[row_offset + col]);
                            ascii_buf[col] = c >= 32 && c < 127 ? c : '.';
                        }
                        ImGui::TextUnformatted(ascii_buf.data(), ascii_buf.data() + row_bytes);

                        // Row click selection (on the whole row area)
                        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
                            selected_ = row_offset;
                        }
                    }
                }
            }
        }
    };

} // namespace imgui_util
