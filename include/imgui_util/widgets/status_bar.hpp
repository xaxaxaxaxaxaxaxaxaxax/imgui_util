/// @file status_bar.hpp
/// @brief Bottom-anchored status bar.
///
/// Uses the main viewport to position at the very bottom of the screen.
/// right_section() moves the cursor to a right-aligned zone.
///
/// Usage:
/// @code
///   if (const imgui_util::status_bar sb{}) {
///       ImGui::Text("Ready");
///       sb.right_section();
///       ImGui::Text("Line 42, Col 8");
///   }
/// @endcode
#pragma once

#include <imgui.h>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    namespace detail {

        /// @brief Sets up window position/size for the status bar.
        ///
        /// Must be called immediately before window construction so that
        /// SetNextWindow* calls take effect.
        inline float setup_status_bar(const float height) noexcept {
            const auto *vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - height});
            ImGui::SetNextWindowSize({vp->WorkSize.x, height});
            return vp->WorkSize.x;
        }

    } // namespace detail

    /// @brief Bottom-anchored status bar using the main viewport.
    class [[nodiscard]] status_bar {
        static constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav;

    public:
        explicit status_bar(const float height = 0.0f) noexcept :
            height_{height > 0.0f ? height : ImGui::GetFrameHeightWithSpacing()},
            width_{detail::setup_status_bar(height_)},
            win_{"##status_bar", nullptr, flags} {}

        status_bar(const status_bar &)            = delete;
        status_bar &operator=(const status_bar &) = delete;
        status_bar(status_bar &&)                 = delete;
        status_bar &operator=(status_bar &&)      = delete;

        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(win_); }

        /// @brief Move cursor to a center-aligned section.
        void center_section() const noexcept { ImGui::SameLine(width_ * 0.5f); }

        /**
         * @brief Move cursor to a right-aligned section.
         * @param offset  Distance from the right edge. Zero aligns to the far right;
         *                use increasing values for multiple right-aligned sections.
         */
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
