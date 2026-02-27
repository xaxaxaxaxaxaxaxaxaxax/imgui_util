// detail/menu_common.hpp - Shared menu entry types and rendering for context_menu / menu_bar_builder
//
// Internal detail header. Used by menu_bar_builder.hpp.
// Not intended for direct use.
#pragma once

#include <functional>
#include <imgui.h>
#include <span>
#include <variant>
#include <vector>

#include "imgui_util/core/raii.hpp"

namespace imgui_util::detail {

    struct menu_item {
        const char                     *label;
        const char                     *shortcut;
        bool                            enabled;
        std::move_only_function<void()> action;
    };

    struct menu_separator {};

    struct menu_checkbox {
        const char *label;
        bool       *value;
    };

    // Forward-declared so menu_submenu can hold a vector of it.
    struct menu_entry;

    struct menu_submenu {
        const char                     *label;
        bool                            enabled;
        std::move_only_function<void()> action;
        std::vector<menu_entry>         children;
    };

    struct menu_entry : std::variant<menu_item, menu_separator, menu_checkbox, menu_submenu> {
        using base_variant = std::variant<menu_item, menu_separator, menu_checkbox, menu_submenu>;
        using base_variant::base_variant;
    };

    inline void render_menu_entries(std::span<menu_entry> entries) { // NOLINT(misc-no-recursion)
        for (auto &entry: entries) {
            std::visit([&]<typename T0>(T0 &e) { // NOLINT(misc-no-recursion)
                using t = std::decay_t<T0>;
                if constexpr (std::is_same_v<t, menu_item>) {
                    if (ImGui::MenuItem(e.label, e.shortcut, false, e.enabled)) {
                        if (e.action) e.action();
                    }
                } else if constexpr (std::is_same_v<t, menu_separator>) {
                    ImGui::Separator();
                } else if constexpr (std::is_same_v<t, menu_checkbox>) {
                    if (e.value != nullptr) ImGui::MenuItem(e.label, nullptr, e.value);
                } else if constexpr (std::is_same_v<t, menu_submenu>) {
                    if (const menu m{e.label, e.enabled}) {
                        if (e.action) e.action();
                        if (!e.children.empty()) render_menu_entries(e.children);
                    }
                }
            }, static_cast<menu_entry::base_variant &>(entry));
        }
    }

} // namespace imgui_util::detail
