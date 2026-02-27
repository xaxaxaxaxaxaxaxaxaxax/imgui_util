/// @file controls.hpp
/// @brief Styled buttons, combo boxes, and convenience window wrappers.
///
/// Usage:
/// @code
///   if (imgui_util::styled_button("Delete", {0.8f,0.2f,0.2f,1.0f})) { ... }
///   imgui_util::show_window("Settings", {400,300}, &open, [&]{ ... });
///   if (imgui_util::column_combo("Item", idx, names)) { ... }
///   imgui_util::checkbox_action("Enable", &flag, [&]{ apply(); });
/// @endcode
#pragma once

#include <algorithm>
#include <concepts>
#include <imgui.h>
#include <imgui_internal.h>
#include <ranges>
#include <span>

#include "imgui_util/core/raii.hpp"
#include "imgui_util/theme/color_math.hpp"

namespace imgui_util {

    /**
     * @brief Render a button with explicit normal, hover, and active colors.
     * @param label  Button label.
     * @param btn    Normal background color.
     * @param hover  Hovered background color.
     * @param active Active (pressed) background color.
     * @param size   Optional button size (0 = auto-fit).
     * @return True if the button was clicked.
     */
    [[nodiscard]] inline bool styled_button(const char *label, const ImVec4 &btn, const ImVec4 &hover,
                                            const ImVec4 &active, const ImVec2 size = {0, 0}) noexcept {
        const style_colors sc{{ImGuiCol_Button, btn}, {ImGuiCol_ButtonHovered, hover}, {ImGuiCol_ButtonActive, active}};
        return ImGui::Button(label, size);
    }

    /**
     * @brief Render a button with hover/active colors derived from a single base color.
     *
     * Uses additive blending so dark colors still produce visible hover/active shifts.
     * @param label Button label.
     * @param base  Base background color.
     * @param size  Optional button size (0 = auto-fit).
     * @return True if the button was clicked.
     */
    [[nodiscard]] inline bool styled_button(const char *label, const ImVec4 &base,
                                            const ImVec2 size = {0, 0}) noexcept {
        return styled_button(label, base, color::offset(base, 0.1f), color::offset(base, 0.2f), size);
    }

    /**
     * @brief Checkbox that invokes a callback when toggled.
     * @param label     Checkbox label.
     * @param v         Pointer to the boolean state.
     * @param on_change Callback invoked when the checkbox value changes.
     * @return True if the checkbox was toggled this frame.
     */
    template<std::invocable F>
    [[nodiscard]] bool checkbox_action(const char *label, bool *v,
                                       F &&on_change) noexcept(std::is_nothrow_invocable_v<F>) {
        if (ImGui::Checkbox(label, v)) {
            std::forward<F>(on_change)();
            return true;
        }
        return false;
    }

    /**
     * @brief Convenience wrapper that creates a sized ImGui window and renders content via callback.
     * @param title        Window title.
     * @param default_size Initial window size (applied on first use).
     * @param open         Pointer to the open state (nullptr = no close button).
     * @param render_fn    Callback invoked inside the window scope.
     * @param flags        ImGui window flags.
     */
    template<std::invocable F>
    void show_window(const char *title, const ImVec2 default_size, bool *open, F &&render_fn,
                     const ImGuiWindowFlags flags = 0) noexcept(std::is_nothrow_invocable_v<F>) {
        ImGui::SetNextWindowSize(default_size, ImGuiCond_FirstUseEver);
        if (const window win{title, open, flags}) {
            std::forward<F>(render_fn)();
        }
    }

    [[nodiscard]] inline bool toggle_switch(const char *label, bool *v) noexcept {
        ImGuiWindow *win = ImGui::GetCurrentWindow();
        if (win->SkipItems) return false;

        const auto   &style  = ImGui::GetStyle();
        const float   height = ImGui::GetFrameHeight();
        const float   width  = height * 1.7f;
        const float   radius = height * 0.5f;
        const ImVec2  pos    = ImGui::GetCursorScreenPos();
        const ImGuiID id     = win->GetID(label);

        const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
        const float  total_w    = width + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f);
        const ImRect total_bb(pos, {pos.x + total_w, pos.y + height});

        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id)) return false;

        bool hovered = false;
        bool held    = false;
        if (ImGui::ButtonBehavior(total_bb, id, &hovered, &held)) {
            *v = !*v;
        }

        auto *dl = ImGui::GetWindowDrawList();

        const float t_anim = *v ? 1.0f : 0.0f;
        const ImU32 bg_col = *v ? ImGui::GetColorU32(ImGuiCol_CheckMark) : ImGui::GetColorU32(ImGuiCol_FrameBg);

        dl->AddRectFilled(pos, {pos.x + width, pos.y + height}, bg_col, radius);
        if (hovered)
            dl->AddRect(pos, {pos.x + width, pos.y + height}, ImGui::GetColorU32(ImGuiCol_BorderShadow), radius);

        const float knob_x = pos.x + radius + t_anim * (width - height);
        const float knob_y = pos.y + radius;
        dl->AddCircleFilled({knob_x, knob_y}, radius - 2.0f, IM_COL32(255, 255, 255, 255));

        if (label_size.x > 0.0f) {
            ImGui::RenderText({pos.x + width + style.ItemInnerSpacing.x, pos.y + (height - label_size.y) * 0.5f}, label);
        }

        return held;
    }

    namespace detail {
        template<typename R>
        concept c_string_range = std::ranges::sized_range<R> && std::ranges::random_access_range<R>
            && requires(std::ranges::range_value_t<R> v) {
                   { v.c_str() } -> std::convertible_to<const char *>;
               };

        template<typename R>
        concept combo_range = c_string_range<R> || std::same_as<R, std::span<const char *const>>;

        template<typename T>
            requires requires(const T &t) {
                { t.c_str() } -> std::convertible_to<const char *>;
            } || std::convertible_to<T, const char *>
        [[nodiscard]] constexpr const char *as_c_str(const T &item) noexcept {
            if constexpr (requires {
                              { item.c_str() } -> std::convertible_to<const char *>;
                          }) {
                return item.c_str();
            } else {
                return item;
            }
        }

        template<typename R>
        [[nodiscard]] bool combo_impl(const char *label, int &idx, const R &items, const char *preview_none,
                                      const bool show_none_entry) noexcept {
            bool              changed = false;
            const auto        sz      = static_cast<int>(std::ranges::size(items));
            const char *const preview = idx >= 0 && idx < sz ? as_c_str(items[idx]) : preview_none;
            if (const combo c{label, preview}) {
                if (show_none_entry) {
                    if (ImGui::Selectable(preview_none, idx < 0)) {
                        idx     = -1;
                        changed = true;
                    }
                }
                for (int i = 0; i < sz; ++i) {
                    if (ImGui::Selectable(as_c_str(items[i]), i == idx)) {
                        idx     = i;
                        changed = true;
                    }
                }
            }
            return changed;
        }
    } // namespace detail

    /**
     * @brief Combo box backed by a range of string-like items.
     *
     * idx uses int to match ImGui's Combo API; -1 means no selection.
     * @param label Combo label.
     * @param idx   Current selection index (in/out).
     * @param items Range of items with .c_str() or const char* elements.
     * @return True if the selection changed.
     */
    template<detail::combo_range R>
    [[nodiscard]] bool column_combo(const char *label, int &idx, const R &items) noexcept {
        return detail::combo_impl(label, idx, items, "<none>", false);
    }

    /**
     * @brief Combo box with a "(none)" entry that allows clearing the selection (idx = -1).
     * @param label Combo label.
     * @param idx   Current selection index (in/out, -1 = none).
     * @param items Range of items with .c_str() or const char* elements.
     * @return True if the selection changed.
     */
    template<detail::combo_range R>
    [[nodiscard]] bool optional_column_combo(const char *label, int &idx, const R &items) noexcept {
        return detail::combo_impl(label, idx, items, "(none)", true);
    }

    [[nodiscard]] inline bool colored_progress_bar(const float fraction, const ImVec4 &bar_color,
                                                      const ImVec2 size = {-1, 0},
                                                      const char *overlay = nullptr) noexcept {
        const style_color col(ImGuiCol_PlotHistogram, bar_color);
        ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), size, overlay);
        return fraction >= 1.0f;
    }

} // namespace imgui_util
