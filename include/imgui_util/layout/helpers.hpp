// helpers.hpp - Layout utility helpers for horizontal groups and alignment
//
// Usage:
//   imgui_util::layout::center_next(100.0f);  // center next 100px-wide widget
//   ImGui::Button("Centered", {100, 0});
//
//   imgui_util::layout::right_align_next(100.0f);
//   ImGui::Button("Right", {100, 0});
//
//   {
//       imgui_util::layout::horizontal_layout h{8.0f};
//       h.next(); ImGui::Button("A");
//       h.next(); ImGui::Button("B");
//       h.next(); ImGui::Button("C");
//   }
#pragma once

#include <imgui.h>

namespace imgui_util::layout {

    // Set cursor to center the next widget of given width within the available region
    inline void center_next(const float widget_width) noexcept {
        const float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((avail - widget_width) * 0.5f));
    }

    // Set cursor to right-align the next widget of given width within the available region
    inline void right_align_next(const float widget_width) noexcept {
        const float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - widget_width);
    }

    // RAII helper that auto-inserts SameLine between items
    class [[nodiscard]] horizontal_layout {
    public:
        explicit horizontal_layout(const float spacing = -1.0f) noexcept : spacing_(spacing) {}

        // Call before each item. Inserts SameLine after the first item.
        void next() noexcept {
            if (count_ > 0) {
                ImGui::SameLine(0.0f, spacing_);
            }
            ++count_;
        }

    private:
        float spacing_;
        int   count_ = 0;
    };

} // namespace imgui_util::layout
