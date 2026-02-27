/// @file reorder_list.hpp
/// @brief Drag-to-reorder list widget.
///
/// Renders each item with a drag handle grip icon. Full drag & drop reordering
/// with visual insertion indicator. Uses std::ranges::rotate for efficient moves.
///
/// Usage:
/// @code
///   std::vector<std::string> items = {"A", "B", "C"};
///   if (imgui_util::reorder_list("##list", items, [](const std::string& s) {
///       ImGui::TextUnformatted(s.c_str());
///   })) {
///       // order changed
///   }
/// @endcode
#pragma once

#include <algorithm>
#include <concepts>
#include <imgui.h>
#include <vector>

#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/drag_drop.hpp"

namespace imgui_util {

    namespace detail {

        inline void draw_grip_icon(ImDrawList *dl, const ImVec2 pos, const float height, const ImU32 color) noexcept {
            const float cx = pos.x + 8.0f;
            const float cy = pos.y + height * 0.5f;

            for (int i = -1; i <= 1; ++i) {
                constexpr float gap = 4.0f;
                constexpr float w   = 8.0f;
                const float     y   = cy + static_cast<float>(i) * gap;
                dl->AddLine({cx - w * 0.5f, y}, {cx + w * 0.5f, y}, color, 1.5f);
            }
        }

        template<typename T, typename RenderFn>
        void render_item_auto_height(const T &item, RenderFn &render_item, const float grip_w, const float avail_w,
                                     const ImU32 grip_color) {
            const ImVec2 cursor_before = ImGui::GetCursorPos();
            {
                const group grp{};
                ImGui::SetCursorPosX(cursor_before.x + grip_w);
                render_item(item);
            }
            const float row_h = ImGui::GetItemRectSize().y;

            const ImVec2 item_min = ImGui::GetItemRectMin();
            auto *const  dl       = ImGui::GetWindowDrawList();
            draw_grip_icon(dl, item_min, row_h, grip_color);

            ImGui::SetCursorPos(cursor_before);
            ImGui::InvisibleButton("##drag_handle", {avail_w, row_h});
        }

        template<typename T, typename RenderFn>
        void render_item_fixed_height(const T &item, RenderFn &render_item, const float grip_w, const float avail_w,
                                      const float row_h, const ImU32 grip_color) {
            const ImVec2 cursor_before = ImGui::GetCursorPos();
            ImGui::InvisibleButton("##drag_handle", {avail_w, row_h});
            const ImVec2 item_min = ImGui::GetItemRectMin();

            auto *const dl = ImGui::GetWindowDrawList();
            draw_grip_icon(dl, item_min, row_h, grip_color);

            ImGui::SetCursorPos({cursor_before.x + grip_w, cursor_before.y});
            render_item(item);
            ImGui::SetCursorPos({cursor_before.x, cursor_before.y + row_h});
        }

        template<typename T>
        bool apply_reorder(std::vector<T> &items, const int from, int to) {
            const int count = static_cast<int>(items.size());
            if (from == to || from < 0 || from >= count) return false;
            if (from < to) --to;
            if (to == from || to < 0 || to > count - 1) return false;

            if (from < to) {
                std::ranges::rotate(items.begin() + from, items.begin() + from + 1, items.begin() + to + 1);
            } else {
                std::ranges::rotate(items.begin() + to, items.begin() + from, items.begin() + from + 1);
            }
            return true;
        }

        inline void draw_insertion_indicator(const ImU32 insert_color, bool &above) {
            const ImVec2 rect_min = ImGui::GetItemRectMin();
            const ImVec2 rect_max = ImGui::GetItemRectMax();
            const float  mouse_y  = ImGui::GetMousePos().y;
            const float  mid_y    = (rect_min.y + rect_max.y) * 0.5f;
            above                 = mouse_y < mid_y;
            const float line_y    = above ? rect_min.y : rect_max.y;

            auto *const dl = ImGui::GetWindowDrawList();
            dl->AddLine({rect_min.x, line_y}, {rect_max.x, line_y}, insert_color, 2.0f);
        }

    } // namespace detail

    /**
     * @brief Drag-to-reorder list with grip handles and visual insertion indicator.
     * @tparam T         Element type.
     * @tparam RenderFn  Callable taking `const T&` to render each item.
     * @param str_id       ImGui string ID.
     * @param items        Vector of items (mutated in-place on reorder).
     * @param render_item  Callback to render a single item.
     * @param item_height  Fixed row height, or <= 0 for auto-detect from content.
     * @return True if the order changed this frame.
     */
    template<typename T, std::invocable<const T &> RenderFn>
    [[nodiscard]] bool reorder_list(const char *str_id, std::vector<T> &items, const RenderFn &render_item,
                                    const float item_height = 0.0f) {
        if (items.empty()) return false;

        const id scope{str_id};
        bool     changed = false;

        // Persistent drag state via ImGui storage
        ImGuiStorage *const storage    = ImGui::GetStateStorage();
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

            if (item_height <= 0.0f) {
                detail::render_item_auto_height(items[static_cast<std::size_t>(i)], render_item, grip_w, avail_w,
                                                grip_color);
            } else {
                detail::render_item_fixed_height(items[static_cast<std::size_t>(i)], render_item, grip_w, avail_w,
                                                 item_height, grip_color);
            }

            // Drag source
            if (const drag_drop_source src{ImGuiDragDropFlags_SourceNoPreviewTooltip}) {
                drag_drop::set_payload("REORDER_ITEM", i);
                storage->SetInt(src_key, i);
                storage->SetBool(active_key, true);
                ImGui::TextUnformatted("Moving...");
            }

            // Drop target
            if (const drag_drop_target tgt{}) {
                bool above = false;
                detail::draw_insertion_indicator(insert_color, above);

                if (const auto from_opt = drag_drop::accept_payload<int>("REORDER_ITEM")) {
                    const int to = above ? i : i + 1;
                    changed      = detail::apply_reorder(items, *from_opt, to) || changed;
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
