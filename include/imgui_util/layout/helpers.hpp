/// @file helpers.hpp
/// @brief Layout utility helpers for horizontal groups and alignment.
///
/// Usage:
/// @code
///   imgui_util::layout::center_next(100.0f);  // center next 100px-wide widget
///   ImGui::Button("Centered", {100, 0});
///
///   imgui_util::layout::right_align_next(100.0f);
///   ImGui::Button("Right", {100, 0});
///
///   {
///       imgui_util::layout::horizontal_layout h{8.0f};
///       h.next(); ImGui::Button("A");
///       h.next(); ImGui::Button("B");
///       h.next(); ImGui::Button("C");
///   }
/// @endcode
#pragma once

#include <imgui.h>

namespace imgui_util::layout {

    /**
     * @brief Offset the cursor so the next widget of the given width is horizontally centered
     *        within the available content region.
     * @param widget_width  Width of the widget to center (pixels).
     */
    inline void center_next(const float widget_width) noexcept {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float offset = (avail - widget_width) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (offset > 0.0f ? offset : 0.0f));
    }

    /**
     * @brief Offset the cursor so the next widget of the given width is right-aligned
     *        within the available content region.
     * @param widget_width  Width of the widget to right-align (pixels).
     */
    inline void right_align_next(const float widget_width) noexcept {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float offset = avail - widget_width;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (offset > 0.0f ? offset : 0.0f));
    }

    /// @brief RAII helper that auto-inserts SameLine between items.
    class [[nodiscard]] horizontal_layout {
    public:
        /// @brief Construct a horizontal layout group.
        /// @param spacing  Pixel spacing between items (-1.0f = use default ImGui spacing).
        explicit horizontal_layout(const float spacing = -1.0f) noexcept : spacing_(spacing) {}

        /// @brief Call before each item. Inserts SameLine after the first item.
        void next() noexcept {
            if (started_) {
                ImGui::SameLine(0.0f, spacing_);
            }
            started_ = true;
        }

        /// @brief Call next() then invoke a callable for the item.
        decltype(auto) item(auto &&callable) noexcept(noexcept(callable())) {
            next();
            return callable();
        }

    private:
        float spacing_;
        bool                 started_ = false;
    };

} // namespace imgui_util::layout
