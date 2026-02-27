// detail/menu_common.hpp - Shared menu entry types and rendering for context_menu / menu_bar_builder
//
// Internal detail header. Used by menu_bar_builder.hpp.
// Not intended for direct use.
#pragma once

#include <cstdint>
#include <functional>
#include <imgui.h>
#include <span>
#include <vector>

#include "imgui_util/core/raii.hpp"

namespace imgui_util::detail {

    enum class menu_entry_type : std::uint8_t { item, separator, checkbox, submenu };

    struct menu_entry {
        menu_entry_type                 type;
        const char                     *label;
        std::move_only_function<void()> action;
        const char                     *shortcut;
        bool                           *checkbox_value;
        bool                            enabled;
        std::vector<menu_entry>         children;
    };

    inline void render_menu_entries(std::span<menu_entry> entries) {
        for (auto &[type, label, action, shortcut, checkbox_value, enabled, children]: entries) {
            switch (type) {
                case menu_entry_type::item:
                    if (ImGui::MenuItem(label, shortcut, false, enabled)) {
                        if (action) action();
                    }
                    break;
                case menu_entry_type::separator:
                    ImGui::Separator();
                    break;
                case menu_entry_type::checkbox:
                    if (checkbox_value != nullptr) ImGui::MenuItem(label, nullptr, checkbox_value);
                    break;
                case menu_entry_type::submenu:
                    if (const menu m{label, enabled}) {
                        if (action) action();
                        if (!children.empty()) render_menu_entries(children);
                    }
                    break;
            }
        }
    }

} // namespace imgui_util::detail
