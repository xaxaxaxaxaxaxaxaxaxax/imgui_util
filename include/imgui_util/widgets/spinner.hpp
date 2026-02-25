// spinner.hpp - Spinning arc indicator and overlay variant
//
// Usage:
//   imgui_util::spinner("loading");                             // default size/color
//   imgui_util::spinner("sync", 12.0f, 3.0f, colors::teal);    // custom size/color
//
//   // Full-area overlay with spinner and optional message:
//   imgui_util::spinner_overlay("Loading data...");
//
// Uses ImDrawList PathArcTo + PathStroke for arc rendering.
// Spins continuously using ImGui's time.
#pragma once

#include <cmath>
#include <imgui.h>
#include <numbers>

#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    // Spinning arc indicator. Renders at the current cursor position.
    // Pass color {0,0,0,0} (default) to use the theme's ButtonActive color.
    inline void spinner(const char *label, float radius = 8.0f, float thickness = 2.0f,
                        const ImVec4 &color = {}) {
        const float  diameter = radius * 2.0f;
        const ImVec2 pos      = ImGui::GetCursorScreenPos();

        // Reserve space via InvisibleButton so layout advances
        ImGui::InvisibleButton(label, {diameter, diameter});

        // Resolve color: if all zeros, use theme's ButtonActive
        const ImVec4 resolved = (color.x == 0.0f && color.y == 0.0f && color.z == 0.0f && color.w == 0.0f)
                                    ? ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]
                                    : color;
        const ImU32 col = ImGui::ColorConvertFloat4ToU32(resolved);

        ImDrawList      *dl     = ImGui::GetWindowDrawList();
        const ImVec2     center = {pos.x + radius, pos.y + radius};
        constexpr float  pi     = std::numbers::pi_v<float>;
        const float      start  = std::fmod(static_cast<float>(ImGui::GetTime()) * 5.0f, pi * 2.0f);
        const float      end    = start + (pi * 1.2f); // ~216 degree arc
        constexpr int    segments = 24;

        dl->PathClear();
        dl->PathArcTo(center, radius, start, end, segments);
        dl->PathStroke(col, 0, thickness);
    }

    // Spinner overlay: fills the available content region with a dimmed background
    // and centers a spinner with an optional label below it.
    inline void spinner_overlay(const char *label, float spinner_radius = 24.0f,
                                const ImVec4 &color = colors::accent) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 pos   = ImGui::GetCursorScreenPos();

        ImDrawList *dl = ImGui::GetWindowDrawList();

        // Dim background
        dl->AddRectFilled(pos, {pos.x + avail.x, pos.y + avail.y}, IM_COL32(0, 0, 0, 100));

        // Center spinner
        const float cx = pos.x + avail.x * 0.5f;
        const float cy = pos.y + avail.y * 0.5f - spinner_radius;

        ImGui::SetCursorScreenPos({cx - spinner_radius, cy - spinner_radius});
        spinner("##overlay_spinner", spinner_radius, 4.0f, color);

        // Optional label
        if (label != nullptr) {
            const ImVec2 text_size = ImGui::CalcTextSize(label);
            ImGui::SetCursorScreenPos({cx - text_size.x * 0.5f, cy + spinner_radius + 8.0f});
            colored_text(label, colors::text_secondary);
        }
    }

    // Determinate progress arc. progress is 0.0..1.0.
    // Renders a background circle and a filled arc proportional to progress.
    inline void spinner_progress(const char *label, float progress, float radius = 8.0f,
                                 float thickness = 2.0f, const ImVec4 &color = {}) {
        const float  diameter = radius * 2.0f;
        const ImVec2 pos      = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton(label, {diameter, diameter});

        const ImVec4 resolved = (color.x == 0.0f && color.y == 0.0f && color.z == 0.0f && color.w == 0.0f)
                                    ? ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]
                                    : color;
        const ImU32 col    = ImGui::ColorConvertFloat4ToU32(resolved);
        const ImU32 bg_col = ImGui::ColorConvertFloat4ToU32(
            {resolved.x, resolved.y, resolved.z, resolved.w * 0.25f});

        ImDrawList      *dl     = ImGui::GetWindowDrawList();
        const ImVec2     center = {pos.x + radius, pos.y + radius};
        constexpr float  pi     = std::numbers::pi_v<float>;
        constexpr int    segments = 24;

        // Background arc (full circle, dimmed)
        dl->PathClear();
        dl->PathArcTo(center, radius, 0.0f, pi * 2.0f, segments);
        dl->PathStroke(bg_col, 0, thickness);

        // Progress arc
        const float clamped   = std::clamp(progress, 0.0f, 1.0f);
        const float end_angle = (-pi * 0.5f) + (clamped * (pi * 2.0f));
        dl->PathClear();
        dl->PathArcTo(center, radius, -pi * 0.5f, end_angle, segments);
        dl->PathStroke(col, 0, thickness);
    }

} // namespace imgui_util
