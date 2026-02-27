// tag_input.hpp - Tag/chip input widget with pill rendering and inline editing
//
// Usage:
//   static std::vector<std::string> tags = {"C++", "ImGui"};
//   if (imgui_util::tag_input("Tags", tags)) {
//       // tags changed (added or removed)
//   }
//
//   // With max tag limit:
//   if (imgui_util::tag_input("Labels", tags, 5)) { ... }
//
// Renders each tag as a colored pill with an X button. An InputText at the end
// allows adding new tags on Enter. Uses ImDrawList for pill rendering.
#pragma once

#include <array>
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    [[nodiscard]] inline bool tag_input(const char *label, std::vector<std::string> &tags, const int max_tags = 0) {
        if (const auto *const win = ImGui::GetCurrentWindow(); win->SkipItems) return false;

        const id scope{label};

        const auto &style = ImGui::GetStyle();
        auto *const dl    = ImGui::GetWindowDrawList();

        const float     line_height      = ImGui::GetFrameHeight();
        const float     pill_height      = line_height - 2.0f;
        const float     pill_rounding    = pill_height * 0.5f;
        constexpr float pill_pad_x       = 6.0f;
        const float     cached_x_glyph_w = ImGui::CalcTextSize("x").x;
        const float     x_btn_width      = cached_x_glyph_w + pill_pad_x;

        const auto  region     = ImGui::GetContentRegionAvail();
        const float wrap_width = region.x > 0.0f ? region.x : 200.0f;

        bool changed = false;

        // Layout: wrap pills across lines
        const auto origin   = ImGui::GetCursorScreenPos();
        float      cursor_x = 0.0f;
        float      cursor_y = 0.0f;

        int remove_idx = -1;

        for (int i = 0; std::cmp_less(i, tags.size()); ++i) {
            const auto &tag        = tags[i];
            const auto  text_size  = ImGui::CalcTextSize(tag.c_str());
            const float pill_width = text_size.x + x_btn_width + pill_pad_x * 3.0f;

            // Wrap to next line if needed
            if (cursor_x + pill_width > wrap_width && cursor_x > 0.0f) {
                cursor_x = 0.0f;
                cursor_y += line_height + style.ItemSpacing.y;
            }

            const ImVec2 pill_min{origin.x + cursor_x, origin.y + cursor_y + 1.0f};
            const ImVec2 pill_max{pill_min.x + pill_width, pill_min.y + pill_height};

            // Pill background
            dl->AddRectFilled(pill_min, pill_max, ImGui::GetColorU32(ImGuiCol_FrameBg), pill_rounding);

            // Tag text
            const float text_y = pill_min.y + (pill_height - text_size.y) * 0.5f;
            dl->AddText({pill_min.x + pill_pad_x, text_y}, ImGui::GetColorU32(ImGuiCol_Text), tag.c_str());

            // X button region
            const float  x_start = pill_max.x - x_btn_width - pill_pad_x;
            const ImVec2 x_min{x_start, pill_min.y};
            const ImVec2 x_max{pill_max.x, pill_max.y};

            const fmt_buf<32> x_id{"##tag_x_{}", i};
            ImGui::SetCursorScreenPos(x_min);
            if (ImGui::InvisibleButton(x_id.c_str(), {x_max.x - x_min.x, x_max.y - x_min.y})) {
                remove_idx = i;
            }

            // X glyph (hover turns error-red)
            const auto  x_color   = ImGui::IsItemHovered() ? colors::error : colors::text_secondary;
            const float x_glyph_w = cached_x_glyph_w;
            const float x_text_x  = x_start + (x_btn_width - x_glyph_w) * 0.5f + pill_pad_x * 0.5f;
            const float x_text_y  = pill_min.y + (pill_height - ImGui::GetTextLineHeight()) * 0.5f;
            dl->AddText({x_text_x, x_text_y}, ImGui::ColorConvertFloat4ToU32(x_color), "x");

            cursor_x += pill_width + style.ItemSpacing.x;
        }

        // Remove tag if X was clicked
        if (remove_idx >= 0) {
            tags.erase(tags.begin() + remove_idx);
            changed = true;
        }

        // Input text for new tags
        const bool at_limit = max_tags > 0 && std::cmp_greater_equal(tags.size(), max_tags);
        if (!at_limit) {
            // Wrap input to next line if not enough space
            constexpr float input_min_w = 80.0f;
            if (cursor_x + input_min_w > wrap_width && cursor_x > 0.0f) {
                cursor_x = 0.0f;
                cursor_y += line_height + style.ItemSpacing.y;
            }

            const float input_w = wrap_width - cursor_x;
            ImGui::SetCursorScreenPos({origin.x + cursor_x, origin.y + cursor_y});

            // Per-instance buffer keyed by ImGui ID to avoid sharing across multiple tag_input widgets
            const ImGuiID                                             buf_id = ImGui::GetID("##tag_buf");
            static std::unordered_map<ImGuiID, std::array<char, 128>> input_bufs;
            auto                                                     &input_buf = input_bufs[buf_id];

            const item_width iw{input_w > 0.0f ? input_w : -1.0f};
            if (ImGui::InputTextWithHint("##tag_add", "Add tag...", input_buf.data(), input_buf.size(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (const std::string new_tag(input_buf.data()); !new_tag.empty()) {
                    tags.push_back(new_tag);
                    input_buf[0] = '\0';
                    changed      = true;
                    ImGui::SetKeyboardFocusHere(-1);
                }
            }

            cursor_y += line_height;
        } else {
            cursor_y += line_height;
        }

        // Reserve item space for the whole widget
        ImGui::SetCursorScreenPos(origin);
        ImGui::ItemSize({wrap_width, cursor_y});

        return changed;
    }

} // namespace imgui_util
