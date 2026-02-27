// range_slider.hpp - Dual-handle range slider for min/max selection
//
// Usage:
//   static float lo = 20.0f, hi = 80.0f;
//   if (imgui_util::range_slider("Range", &lo, &hi, 0.0f, 100.0f)) {
//       // lo or hi changed
//   }
//
//   // With int values:
//   static int min_val = 10, max_val = 90;
//   if (imgui_util::range_slider("Int Range", &min_val, &max_val, 0, 100)) { ... }
//
//   // With format string (std::format syntax):
//   imgui_util::range_slider("Freq", &lo, &hi, 20.0f, 20000.0f, "{:.0f} Hz");
//
// Renders a background track, a colored fill between the two handles, and two
// draggable handles. Uses ImDrawList for custom rendering and ButtonBehavior
// for interaction.
#pragma once

#include <algorithm>
#include <concepts>
#include <imgui.h>
#include <imgui_internal.h>
#include <string_view>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    namespace detail {

        // Map value to 0..1 normalized position on the slider
        template<typename T>
        [[nodiscard]] constexpr float range_normalize(T val, T range_min, T range_max) noexcept {
            if (range_max <= range_min) return 0.0f;
            return static_cast<float>(val - range_min) / static_cast<float>(range_max - range_min);
        }

        // Map 0..1 normalized position back to value
        template<typename T>
        [[nodiscard]] constexpr T range_denormalize(float t, T range_min, T range_max) noexcept {
            if constexpr (std::integral<T>) {
                return static_cast<T>(range_min + static_cast<T>(t * static_cast<float>(range_max - range_min) + 0.5f));
            } else {
                return range_min + static_cast<T>(t) * (range_max - range_min);
            }
        }

        // Format value for overlay text
        template<typename T>
        [[nodiscard]] fmt_buf<> range_format_value(T val, const std::string_view fmt_str) {
            if (!fmt_str.empty()) return fmt_buf<>(std::runtime_format(fmt_str), val);
            if constexpr (std::integral<T>)
                return fmt_buf<>("{}", val);
            else
                return fmt_buf<>("{:.3f}", val);
        }

        // Shared handle drag logic for range slider handles.
        // Returns true if value changed. Updates t and px in-place.
        template<typename T>
        [[nodiscard]] bool handle_drag(const ImGuiID handle_id, const ImRect &grab_bb, T *val, T range_min, T range_max,
                                       const float clamp_lo, const float clamp_hi, const float pos_x,
                                       const float grab_radius, const float usable, float &t, float &px) noexcept {
            bool hovered = false;
            bool held    = false;
            ImGui::ButtonBehavior(grab_bb, handle_id, &hovered, &held, ImGuiButtonFlags_NoKeyModsAllowed);
            if (held) {
                const float new_t =
                    std::clamp((ImGui::GetIO().MousePos.x - pos_x - grab_radius) / usable, clamp_lo, clamp_hi);
                const T new_val = range_denormalize(new_t, range_min, range_max);
                if (new_val != *val) {
                    *val = new_val;
                    t    = new_t;
                    px   = pos_x + grab_radius + t * usable;
                    return true;
                }
            }
            return false;
        }

    } // namespace detail

    // Dual-handle range slider. Returns true when either value changes.
    template<typename T>
        requires std::integral<T> || std::floating_point<T>
    [[nodiscard]] bool range_slider(const char *label, T *v_min, T *v_max, T range_min, T range_max,
                                    std::string_view format = {}) {
        ImGuiWindow *win = ImGui::GetCurrentWindow();
        if (win->SkipItems) return false;

        const auto  &style = ImGui::GetStyle();
        const float  w     = ImGui::CalcItemWidth();
        const float  h     = ImGui::GetFrameHeight();
        const ImVec2 pos   = ImGui::GetCursorScreenPos();

        const ImGuiID id_lo = win->GetID(label);
        const ImGuiID id_hi = [&] {
            const id scope{label};
            return ImGui::GetID("##hi");
        }();

        const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
        const float  total_w    = w + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f);
        const ImRect total_bb(pos, {pos.x + total_w, pos.y + h});

        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id_lo)) return false;

        auto *dl = ImGui::GetWindowDrawList();

        // Track geometry
        const float     track_y      = pos.y + h * 0.5f;
        constexpr float track_height = 4.0f;
        const float     grab_radius  = h * 0.35f;

        // Background track
        dl->AddRectFilled({pos.x, track_y - track_height * 0.5f}, {pos.x + w, track_y + track_height * 0.5f},
                          ImGui::GetColorU32(ImGuiCol_FrameBg), track_height * 0.5f);

        // Normalized positions
        float t_lo = detail::range_normalize(*v_min, range_min, range_max);
        float t_hi = detail::range_normalize(*v_max, range_min, range_max);

        // Handle positions in pixels
        const float usable = w - grab_radius * 2.0f;
        float       px_lo  = pos.x + grab_radius + t_lo * usable;
        float       px_hi  = pos.x + grab_radius + t_hi * usable;

        bool changed = false;

        // Low handle interaction
        {
            const ImRect grab_bb({px_lo - grab_radius, pos.y}, {px_lo + grab_radius, pos.y + h});
            changed |= detail::handle_drag(id_lo, grab_bb, v_min, range_min, range_max, 0.0f, t_hi, pos.x, grab_radius,
                                           usable, t_lo, px_lo);
        }

        // High handle interaction
        {
            const ImRect grab_bb({px_hi - grab_radius, pos.y}, {px_hi + grab_radius, pos.y + h});
            changed |= detail::handle_drag(id_hi, grab_bb, v_max, range_min, range_max, t_lo, 1.0f, pos.x, grab_radius,
                                           usable, t_hi, px_hi);
        }

        // Filled region between handles
        const ImU32 fill_col = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
        dl->AddRectFilled({px_lo, track_y - track_height * 0.5f}, {px_hi, track_y + track_height * 0.5f}, fill_col,
                          track_height * 0.5f);

        // Draw handles
        const ImU32 grab_col     = ImGui::GetColorU32(ImGuiCol_SliderGrab);
        const ImU32 grab_col_act = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);

        const bool lo_active = ImGui::GetActiveID() == id_lo;
        const bool hi_active = ImGui::GetActiveID() == id_hi;

        dl->AddCircleFilled({px_lo, track_y}, grab_radius, lo_active ? grab_col_act : grab_col);
        dl->AddCircleFilled({px_hi, track_y}, grab_radius, hi_active ? grab_col_act : grab_col);

        // Value overlay text
        const auto         lo_text = detail::range_format_value(*v_min, format);
        const auto         hi_text = detail::range_format_value(*v_max, format);
        const fmt_buf<128> overlay("{} - {}", lo_text.sv(), hi_text.sv());

        const ImVec2 text_size = ImGui::CalcTextSize(overlay.c_str());
        const float  text_x    = pos.x + (w - text_size.x) * 0.5f;
        const float  text_y    = pos.y + (h - text_size.y) * 0.5f;
        dl->AddText({text_x, text_y}, ImGui::GetColorU32(ImGuiCol_Text), overlay.c_str(), overlay.end());

        // Label
        if (label_size.x > 0.0f) {
            ImGui::RenderText({pos.x + w + style.ItemInnerSpacing.x, pos.y + (h - label_size.y) * 0.5f}, label);
        }

        return changed;
    }

} // namespace imgui_util
