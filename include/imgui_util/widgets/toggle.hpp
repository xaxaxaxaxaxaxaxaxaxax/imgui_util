// toggle.hpp - Pill-shaped animated toggle switch widget
//
// Usage:
//   static bool enabled = false;
//   if (imgui_util::toggle("Enable feature", &enabled)) {
//       // value changed
//   }
//
//   // With custom on-color:
//   if (imgui_util::toggle_colored("Dark mode", &dark, imgui_util::colors::teal)) { ... }
//
// Renders a pill-shaped toggle with smooth animation using ImGui storage
// for the animation state. Uses ImDrawList for custom rendering.
#pragma once

#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>

#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    // Pill-shaped toggle switch with custom on-color. Returns true when the value changes.
    [[nodiscard]]
    inline bool toggle_colored(const char *label, bool *v, const ImVec4 &on_color) {
        ImGuiWindow *win = ImGui::GetCurrentWindow();
        if (win->SkipItems) return false;

        const ImGuiID    id    = win->GetID(label);
        const ImGuiStyle &style = ImGui::GetStyle();

        const float height = ImGui::GetFrameHeight();
        const float width  = height * 1.75f;
        const float radius = height * 0.5f;

        const ImVec2 pos        = ImGui::GetCursorScreenPos();
        const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
        const float  total_w    = width + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f);
        const ImRect bb(pos, {pos.x + total_w, pos.y + height});

        ImGui::ItemSize(bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id)) return false;

        // Interaction
        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) *v = !*v;

        // Animation state stored in ImGui storage (0.0 = off, 1.0 = on)
        float      *anim_t = ImGui::GetStateStorage()->GetFloatRef(id, *v ? 1.0f : 0.0f);
        const float target  = *v ? 1.0f : 0.0f;
        const float speed   = ImGui::GetIO().DeltaTime * 8.0f; // ~125ms transition
        if (*anim_t < target)
            *anim_t = (std::min)(*anim_t + speed, target);
        else if (*anim_t > target)
            *anim_t = (std::max)(*anim_t - speed, target);

        const float t = *anim_t;

        // Colors: off = gray (FrameBg), on = provided on_color
        const ImVec4 bg_off_v4 = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_FrameBg));
        const ImVec4 bg_lerp{
            bg_off_v4.x + (on_color.x - bg_off_v4.x) * t,
            bg_off_v4.y + (on_color.y - bg_off_v4.y) * t,
            bg_off_v4.z + (on_color.z - bg_off_v4.z) * t,
            bg_off_v4.w + (on_color.w - bg_off_v4.w) * t,
        };

        ImDrawList *dl = ImGui::GetWindowDrawList();

        // Pill background
        dl->AddRectFilled(
            {pos.x, pos.y},
            {pos.x + width, pos.y + height},
            ImGui::ColorConvertFloat4ToU32(bg_lerp),
            radius);

        // Hover highlight
        if (hovered) {
            dl->AddRectFilled(
                {pos.x, pos.y},
                {pos.x + width, pos.y + height},
                IM_COL32(255, 255, 255, 20),
                radius);
        }

        // Circle knob
        const float knob_radius = radius - 2.0f;
        const float knob_x      = pos.x + radius + t * (width - height);
        const float knob_y      = pos.y + radius;
        dl->AddCircleFilled({knob_x, knob_y}, knob_radius, IM_COL32(255, 255, 255, 230));

        // Label
        if (label_size.x > 0.0f) {
            const float text_x = pos.x + width + style.ItemInnerSpacing.x;
            const float text_y = pos.y + (height - label_size.y) * 0.5f;
            ImGui::RenderText({text_x, text_y}, label);
        }

        return pressed;
    }

    // Pill-shaped toggle switch using the theme's CheckMark color. Returns true when the value changes.
    [[nodiscard]]
    inline bool toggle(const char *label, bool *v) {
        return toggle_colored(label, v, ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);
    }

} // namespace imgui_util
