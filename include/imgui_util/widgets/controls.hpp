// controls.hpp - styled buttons, combo boxes, and convenience window wrappers
//
// Usage:
//   if (imgui_util::styled_button("Delete", {0.8f,0.2f,0.2f,1.0f})) { ... }
//   imgui_util::show_window("Settings", {400,300}, &open, [&]{ ... });
//   if (imgui_util::column_combo("Item", idx, names)) { ... }
//   imgui_util::checkbox_action("Enable", &flag, [&]{ apply(); });
#pragma once

#include <algorithm>
#include <concepts>
#include <imgui.h>
#include <ranges>
#include <span>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    // Constexpr helper: brighten each RGB channel by `amount`, clamped to 1.0
    [[nodiscard]]
    constexpr ImVec4 brighten(const ImVec4 &color, const float amount) noexcept {
        return {std::min(color.x + amount, 1.0f), std::min(color.y + amount, 1.0f), std::min(color.z + amount, 1.0f),
                color.w};
    }

    // Button with custom color scheme (button, hovered, active)
    [[nodiscard]]
    inline bool styled_button(const char *label, const ImVec4 &btn, const ImVec4 &hover, const ImVec4 &active,
                              const ImVec2 size = {0, 0}) {
        const style_color c1(ImGuiCol_Button, btn);
        const style_color c2(ImGuiCol_ButtonHovered, hover);
        const style_color c3(ImGuiCol_ButtonActive, active);
        return ImGui::Button(label, size);
    }

    // Convenience: derive hover/active from a single base color.
    // Uses additive blending so dark colors still produce visible hover/active shifts.
    [[nodiscard]]
    inline bool styled_button(const char *label, const ImVec4 &base, const ImVec2 size = {0, 0}) {
        return styled_button(label, base, brighten(base, 0.1f), brighten(base, 0.2f), size);
    }

    // Checkbox that invokes a callback on change
    template<std::invocable F>
    [[nodiscard]]
    bool checkbox_action(const char *label, bool *v, F &&on_change) {
        if (ImGui::Checkbox(label, v)) {
            std::forward<F>(on_change)();
            return true;
        }
        return false;
    }

    // Convenience: SetNextWindowSize + window scope + render body in one call
    template<std::invocable F>
    void show_window(const char *title, const ImVec2 default_size, bool *open, F &&render_fn,
                     const ImGuiWindowFlags flags = 0) {
        ImGui::SetNextWindowSize(default_size, ImGuiCond_FirstUseEver);
        if (const window win{title, open, flags}) {
            std::forward<F>(render_fn)();
        }
    }

    namespace detail {
        // Concept for a range whose elements provide .c_str() returning const char*
        template<typename R>
        concept c_string_range = std::ranges::sized_range<R> && std::ranges::random_access_range<R> &&
                                 requires(std::ranges::range_value_t<R> v) {
                                     { v.c_str() } -> std::convertible_to<const char *>;
                                 };

        // Unified element accessor: calls .c_str() if available, otherwise passes through
        template<typename T>
        [[nodiscard]]
        constexpr const char *as_c_str(const T &item) {
            if constexpr (requires {
                              { item.c_str() } -> std::convertible_to<const char *>;
                          }) {
                return item.c_str();
            } else {
                return item;
            }
        }

        // Shared combo rendering: renders selectable items and returns true if selection changed.
        // idx uses -1 for "no selection"; preview_none is shown when idx < 0 or out of range.
        template<typename R>
        bool combo_impl(const char *label, int &idx, const R &items, const char *preview_none,
                        const bool show_none_entry) {
            bool        changed = false;
            const auto  sz      = static_cast<int>(std::ranges::size(items));
            const char *preview = (idx >= 0 && idx < sz) ? as_c_str(items[idx]) : preview_none;
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

    // Combo box for selecting from a range of strings. Returns true if selection changed.
    // Works with both std::string ranges (.c_str()) and const char* spans.
    // idx uses int to match ImGui's Combo API; -1 means no selection.
    template<typename R>
        requires detail::c_string_range<R> || std::same_as<R, std::span<const char *const>>
    [[nodiscard]]
    bool column_combo(const char *label, int &idx, const R &items) {
        return detail::combo_impl(label, idx, items, "<none>", false);
    }

    // Combo box with an optional "(none)" entry at index -1. Returns true if selection changed.
    template<typename R>
        requires detail::c_string_range<R> || std::same_as<R, std::span<const char *const>>
    [[nodiscard]]
    bool optional_column_combo(const char *label, int &idx, const R &items) {
        return detail::combo_impl(label, idx, items, "(none)", true);
    }

} // namespace imgui_util
