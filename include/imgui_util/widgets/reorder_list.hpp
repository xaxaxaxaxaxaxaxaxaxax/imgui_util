// reorder_list.hpp - Drag-to-reorder list widget
//
// Usage:
//   std::vector<std::string> items = {"A", "B", "C"};
//   if (imgui_util::reorder_list("##list", items, [](const std::string& s) {
//       ImGui::TextUnformatted(s.c_str());
//   })) {
//       // order changed
//   }
//
// Renders each item with a drag handle grip icon. Full drag & drop reordering
// with visual insertion indicator. Uses std::ranges::rotate for efficient moves.
#pragma once

#include <algorithm>
#include <concepts>
#include <imgui.h>
#include <vector>

#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/drag_drop.hpp"

namespace imgui_util {

    namespace detail {

        // Draw a three-line grip icon (drag handle) at the given position
        inline void draw_grip_icon(ImDrawList* dl, const ImVec2 pos, const float height,
                                   const ImU32 color) noexcept {
            const float cx = pos.x + 8.0f;
            const float cy = pos.y + height * 0.5f;

            for (int i = -1; i <= 1; ++i) {
                constexpr float gap = 4.0f;
                constexpr float w   = 8.0f;
                const float     y   = cy + static_cast<float>(i) * gap;
                dl->AddLine({cx - w * 0.5f, y}, {cx + w * 0.5f, y}, color, 1.5f);
            }
        }

    } // namespace detail

    // Drag-to-reorder list. Returns true if the order changed this frame.
    // item_height <= 0 means auto-detect from first item content height.
    template<typename T, std::invocable<const T&> RenderFn>
    [[nodiscard]]
    bool reorder_list(const char* str_id, std::vector<T>& items, RenderFn render_item,
                      const float item_height = 0.0f) {
        if (items.empty()) return false;

        const id scope{str_id};
        bool     changed = false;

        // Persistent drag state via ImGui storage
        ImGuiStorage* const storage    = ImGui::GetStateStorage();
        const ImGuiID       src_key    = ImGui::GetID("##reorder_src");
        const ImGuiID       active_key = ImGui::GetID("##reorder_active");
        const bool          dragging   = storage->GetBool(active_key, false);

        const float avail_w      = ImGui::GetContentRegionAvail().x;
        const ImU32 grip_color   = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const ImU32 insert_color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);

        const auto count = static_cast<int>(items.size());
        for (int i = 0; i < count; ++i) {
            constexpr float grip_w = 20.0f;
            const id        id_scope{i};

            // Determine item height: if not specified, measure from content
            if (float row_h = item_height; row_h <= 0.0f) {
                // Use a group to measure content height on first pass
                const ImVec2 cursor_before = ImGui::GetCursorPos();
                {
                    const group grp{};

                    // Reserve grip area
                    ImGui::SetCursorPosX(cursor_before.x + grip_w);
                    render_item(items[static_cast<std::size_t>(i)]);
                }
                row_h = ImGui::GetItemRectSize().y;

                // Now draw the grip icon over the reserved area
                const ImVec2 item_min = ImGui::GetItemRectMin();
                auto* const  dl       = ImGui::GetWindowDrawList();
                detail::draw_grip_icon(dl, item_min, row_h, grip_color);

                // Make the whole row a drag source / drop target
                // Use an invisible button overlaid on the group for drag interaction
                ImGui::SetCursorPos(cursor_before);
                ImGui::InvisibleButton("##drag_handle", {avail_w, row_h});
            } else {
                // Fixed height path
                const ImVec2 cursor_before = ImGui::GetCursorPos();
                ImGui::InvisibleButton("##drag_handle", {avail_w, row_h});
                const ImVec2 item_min = ImGui::GetItemRectMin();

                auto* const dl = ImGui::GetWindowDrawList();
                detail::draw_grip_icon(dl, item_min, row_h, grip_color);

                // Render content offset by grip width
                ImGui::SetCursorPos({cursor_before.x + grip_w, cursor_before.y});
                render_item(items[static_cast<std::size_t>(i)]);
                ImGui::SetCursorPos({cursor_before.x, cursor_before.y + row_h});
            }

            // Drag source (uses type-safe helpers from drag_drop.hpp)
            if (const drag_drop_source src{ImGuiDragDropFlags_SourceNoPreviewTooltip}) {
                drag_drop::set_payload("REORDER_ITEM", i);
                storage->SetInt(src_key, i);
                storage->SetBool(active_key, true);
                ImGui::TextUnformatted("Moving...");
            }

            // Drop target
            if (const drag_drop_target tgt{}) {
                // Draw insertion indicator
                const ImVec2 rect_min = ImGui::GetItemRectMin();
                const ImVec2 rect_max = ImGui::GetItemRectMax();
                const float  mouse_y  = ImGui::GetMousePos().y;
                const float  mid_y    = (rect_min.y + rect_max.y) * 0.5f;
                const bool   above    = mouse_y < mid_y;
                const float  line_y   = above ? rect_min.y : rect_max.y;

                auto* const dl = ImGui::GetWindowDrawList();
                dl->AddLine({rect_min.x, line_y}, {rect_max.x, line_y}, insert_color, 2.0f);

                if (const auto from_opt = drag_drop::accept_payload<int>("REORDER_ITEM")) {
                    const int from = *from_opt;
                    int       to   = above ? i : i + 1;

                    if (from != to && from >= 0 && from < count) {
                        // Adjust target index after removal
                        if (from < to) --to;
                        if (to != from && to >= 0 && to <= count - 1) {
                            if (from < to) {
                                std::ranges::rotate(items.begin() + from,
                                                    items.begin() + from + 1,
                                                    items.begin() + to + 1);
                            } else {
                                std::ranges::rotate(items.begin() + to,
                                                    items.begin() + from,
                                                    items.begin() + from + 1);
                            }
                            changed = true;
                        }
                    }
                    storage->SetInt(src_key, -1);
                    storage->SetBool(active_key, false);
                }
            }
        }

        // Clear drag state when not dragging
        if (dragging && !ImGui::IsMouseDragging(0)) {
            storage->SetInt(src_key, -1);
            storage->SetBool(active_key, false);
        }

        return changed;
    }

} // namespace imgui_util
