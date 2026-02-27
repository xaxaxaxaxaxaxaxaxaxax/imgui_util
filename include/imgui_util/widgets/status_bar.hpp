// status_bar.hpp - Bottom-anchored status bar
//
// Usage:
//   if (const imgui_util::status_bar sb{}) {
//       ImGui::Text("Ready");
//       sb.right_section();
//       ImGui::Text("Line 42, Col 8");
//   }
//
// Uses the main viewport to position at the very bottom of the screen.
// right_section() moves the cursor to a right-aligned zone.
#pragma once

#include <imgui.h>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    namespace detail {

        // Sets up window position/size for the status bar and returns the viewport width.
        // Must be called immediately before window construction so that SetNextWindow* calls take effect.
        inline float setup_status_bar(const float height) noexcept {
            const auto *vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - height});
            ImGui::SetNextWindowSize({vp->WorkSize.x, height});
            return vp->WorkSize.x;
        }

    } // namespace detail

    class [[nodiscard]] status_bar {
        static constexpr ImGuiWindowFlags flags_ = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav;

    public:
        explicit status_bar(const float height = 0.0f) noexcept :
            height_{height > 0.0f ? height : ImGui::GetFrameHeightWithSpacing()},
            width_{detail::setup_status_bar(height_)},
            win_{"##status_bar", nullptr, flags_} {}

        status_bar(const status_bar &)            = delete;
        status_bar &operator=(const status_bar &) = delete;
        status_bar(status_bar &&)                 = delete;
        status_bar &operator=(status_bar &&)      = delete;

        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(win_); }

        // Move cursor to a centered section.
        void center_section() const noexcept { ImGui::SameLine(width_ * 0.5f); }

        // Move cursor to a right-aligned section at the given offset from the right edge.
        // With no argument, aligns to the far right. Use increasing offsets for multiple sections.
        void right_section(const float offset = 0.0f) const noexcept {
            const float padding = ImGui::GetStyle().WindowPadding.x;
            ImGui::SameLine(width_ - padding * 2.0f - offset);
        }

    private:
        float  height_;
        float  width_;
        window win_;
    };

} // namespace imgui_util
