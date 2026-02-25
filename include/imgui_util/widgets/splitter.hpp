// splitter.hpp - Resizable split panels
//
// Usage:
//   static float ratio = 0.5f;
//   imgui_util::splitter("##split", imgui_util::split_direction::horizontal, ratio);
//
//   // Use ratio to size child panels:
//   ImVec2 avail = ImGui::GetContentRegionAvail();
//   imgui_util::child left{"left", {avail.x * ratio, 0}};
//   // ... left panel content ...
//   // Then right panel at (1 - ratio)
//
// Returns true if the ratio changed. Renders an invisible drag handle between panels.
#pragma once

#include <algorithm>
#include <imgui.h>

namespace imgui_util {

    enum class split_direction { horizontal, vertical };

    // Returns true if the ratio changed. ratio is 0.0-1.0 representing the first panel's share.
    [[nodiscard]]
    inline bool splitter(const char *id, split_direction dir, float &ratio,
                         float thickness = 8.0f, float min_ratio = 0.1f, float max_ratio = 0.9f) {
        const bool is_h = (dir == split_direction::horizontal);

        // Use the window's full content region width, not GetContentRegionAvail which
        // returns only the remaining space after the cursor position.
        const ImVec2 content_max = ImGui::GetContentRegionMax();
        const ImVec2 content_min = ImGui::GetWindowContentRegionMin();
        const float total = is_h ? (content_max.x - content_min.x)
                                 : (content_max.y - content_min.y);
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        // Render the invisible button at the current cursor position (no SetCursorPos).
        ImGui::InvisibleButton(id, is_h ? ImVec2(thickness, avail.y) : ImVec2(avail.x, thickness));

        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(is_h ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        }

        bool changed = false;
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            float pos = ratio * total;
            const float delta = is_h ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
            pos += delta;
            const float new_ratio = std::clamp(pos / total, min_ratio, max_ratio);
            if (new_ratio != ratio) {
                ratio = new_ratio;
                changed = true;
            }
        }
        return changed;
    }

} // namespace imgui_util
