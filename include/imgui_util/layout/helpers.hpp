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

#include <algorithm>
#include <imgui.h>

namespace imgui_util::layout {

    inline void center_next(const float widget_width) noexcept {
        const float offset = (ImGui::GetContentRegionAvail().x - widget_width) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(offset, 0.0f));
    }

    inline void right_align_next(const float widget_width) noexcept {
        const float offset = ImGui::GetContentRegionAvail().x - widget_width;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(offset, 0.0f));
    }

    inline void left_align_next(const float indent = 0.0f) noexcept {
        ImGui::SetCursorPosX(ImGui::GetCursorStartPos().x + std::max(indent, 0.0f));
    }

    inline void vertical_pad(const float pixels) noexcept {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + pixels);
    }

    inline void label_left(const char *label, const float widget_width) noexcept {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(widget_width);
    }

    [[nodiscard]] inline float remaining_width() noexcept { return ImGui::GetContentRegionAvail().x; }

    [[nodiscard]] inline float remaining_height() noexcept { return ImGui::GetContentRegionAvail().y; }

    class [[nodiscard]] horizontal_layout {
    public:
        explicit horizontal_layout(const float spacing = -1.0f) noexcept : spacing_(spacing) {}

        void next() noexcept {
            if (started_) ImGui::SameLine(0.0f, spacing_);
            started_ = true;
        }

        decltype(auto) item(auto &&callable) noexcept(noexcept(callable())) {
            next();
            return callable();
        }

    private:
        float spacing_;
        bool  started_ = false;
    };

} // namespace imgui_util::layout
