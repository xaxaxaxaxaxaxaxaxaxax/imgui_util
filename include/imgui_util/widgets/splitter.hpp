/// @file splitter.hpp
/// @brief Resizable split panels.
///
/// Returns true if the ratio changed. Renders an invisible drag handle between panels.
///
/// Usage:
/// @code
///   static float ratio = 0.5f;
///   imgui_util::splitter("##split", imgui_util::direction::horizontal, ratio);
///
///   // Use ratio to size child panels:
///   ImVec2 avail = ImGui::GetContentRegionAvail();
///   imgui_util::child left{"left", {avail.x * ratio, 0}};
///   // ... left panel content ...
///   // Then right panel at (1 - ratio)
/// @endcode
#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <imgui.h>
#include <tuple>

#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/helpers.hpp"

namespace imgui_util {

    using split_direction = direction;

    /**
     * @brief Render a draggable splitter handle and update the split ratio.
     * @param id         ImGui ID for the invisible drag button.
     * @param dir        Split direction (horizontal or vertical).
     * @param ratio      Current split ratio, updated on drag.
     * @param thickness  Thickness of the drag handle in pixels.
     * @param min_ratio  Minimum allowed ratio.
     * @param max_ratio  Maximum allowed ratio.
     * @return True if the ratio changed this frame.
     */
    [[nodiscard]] inline bool splitter(const char *id, const split_direction dir, float &ratio, float thickness = 8.0f,
                                       const float min_ratio = 0.1f, const float max_ratio = 0.9f) noexcept {
        assert(min_ratio < max_ratio && "min_ratio must be less than max_ratio");
        thickness       = std::max(thickness, 1.0f);
        ratio           = std::clamp(ratio, min_ratio, max_ratio);
        const bool is_h = dir == direction::horizontal;

        // Use the window's full content region width, not GetContentRegionAvail which
        // returns only the remaining space after the cursor position.
        const auto  content_max = ImGui::GetContentRegionMax();
        const auto  content_min = ImGui::GetWindowContentRegionMin();
        const float total       = is_h ? content_max.x - content_min.x : content_max.y - content_min.y;
        const auto  avail       = ImGui::GetContentRegionAvail();

        ImGui::InvisibleButton(id, is_h ? ImVec2(thickness, avail.y) : ImVec2(avail.x, thickness));

        const bool hovered_or_active = ImGui::IsItemHovered() || ImGui::IsItemActive();
        if (hovered_or_active) {
            ImGui::SetMouseCursor(is_h ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        }

        {
            const auto  rmin = ImGui::GetItemRectMin();
            const auto  rmax = ImGui::GetItemRectMax();
            const ImU32 col  = ImGui::GetColorU32(hovered_or_active ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);

            auto *const dl = ImGui::GetWindowDrawList();
            if (is_h) {
                const float cx = (rmin.x + rmax.x) * 0.5f;
                dl->AddLine({cx, rmin.y}, {cx, rmax.y}, col, 2.0f);
            } else {
                const float cy = (rmin.y + rmax.y) * 0.5f;
                dl->AddLine({rmin.x, cy}, {rmax.x, cy}, col, 2.0f);
            }
        }

        bool changed = false;
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            float       pos   = ratio * total;
            const float delta = is_h ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
            pos += delta;
            if (const float new_ratio = std::clamp(pos / total, min_ratio, max_ratio); new_ratio != ratio) {
                ratio   = new_ratio;
                changed = true;
            }
        }
        return changed;
    }

    /**
     * @brief Render two child panels separated by a draggable splitter.
     * @param id         ImGui ID for the splitter handle.
     * @param dir        Split direction (horizontal or vertical).
     * @param ratio      Current split ratio, updated on drag.
     * @param left       Callable rendering the first (left/top) panel content.
     * @param right      Callable rendering the second (right/bottom) panel content.
     * @param min_ratio  Minimum allowed ratio.
     * @param max_ratio  Maximum allowed ratio.
     */
    template<std::invocable Left, std::invocable Right>
    void split_panel(const char *id, const direction dir, float &ratio, Left &&left, Right &&right,
                     const float min_ratio = 0.1f, const float max_ratio = 0.9f) noexcept {
        const bool is_h  = dir == direction::horizontal;
        const auto avail = ImGui::GetContentRegionAvail();

        const float first_size  = is_h ? avail.x * ratio : avail.y * ratio;
        const float second_size = is_h ? avail.x * (1.0f - ratio) : avail.y * (1.0f - ratio);

        {
            const ImVec2 first_child_size = is_h ? ImVec2(first_size, avail.y) : ImVec2(avail.x, first_size);
            if (const child first{"##split_first", first_child_size, ImGuiChildFlags_None}) {
                std::forward<Left>(left)();
            }
        }

        if (is_h) ImGui::SameLine();

        std::ignore = splitter(id, dir, ratio, 8.0f, min_ratio, max_ratio);

        if (is_h) ImGui::SameLine();

        {
            const ImVec2 second_child_size = is_h ? ImVec2(second_size, avail.y) : ImVec2(avail.x, second_size);
            if (const child second{"##split_second", second_child_size, ImGuiChildFlags_None}) {
                std::forward<Right>(right)();
            }
        }
    }

} // namespace imgui_util
