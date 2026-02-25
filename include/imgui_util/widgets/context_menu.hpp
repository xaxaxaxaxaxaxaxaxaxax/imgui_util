// context_menu.hpp - Declarative context menu builder
//
// Usage:
//   imgui_util::context_menu()
//       .item("Copy", [&]{ copy(); })
//       .separator()
//       .checkbox("Show details", &show)
//       .item("Delete", [&]{ delete_it(); }, can_delete)
//       .render();
//
//   // Window-level context menu:
//   imgui_util::context_menu("bg_ctx")
//       .item("Paste", [&]{ paste(); })
//       .render_window();
//
// Uses existing RAII popup_context_item/popup_context_window wrappers.
#pragma once

#include <functional>
#include <imgui.h>
#include <vector>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    class context_menu {
    public:
        explicit context_menu(const char *id = nullptr, ImGuiPopupFlags flags = ImGuiPopupFlags_MouseButtonRight)
            : id_(id), flags_(flags) {}

        context_menu &item(const char *label, std::function<void()> action, bool enabled = true) {
            entries_.push_back({entry_type::item, label, std::move(action), enabled, nullptr});
            return *this;
        }

        context_menu &separator() {
            entries_.push_back({entry_type::separator, "", {}, true, nullptr});
            return *this;
        }

        context_menu &checkbox(const char *label, bool *value) {
            entries_.push_back({entry_type::checkbox, label, {}, true, value});
            return *this;
        }

        void render() {
            if (const popup_context_item ctx{id_, flags_}) {
                render_entries();
            }
        }

        void render_window() {
            if (const popup_context_window ctx{id_, flags_}) {
                render_entries();
            }
        }

    private:
        enum class entry_type { item, separator, checkbox };
        struct menu_entry {
            entry_type type;
            const char *label;
            std::function<void()> action;
            bool enabled;
            bool *checkbox_value;
        };

        void render_entries() {
            for (auto &e : entries_) {
                switch (e.type) {
                    case entry_type::item:
                        if (ImGui::MenuItem(e.label, nullptr, false, e.enabled)) {
                            if (e.action) e.action();
                        }
                        break;
                    case entry_type::separator:
                        ImGui::Separator();
                        break;
                    case entry_type::checkbox:
                        if (e.checkbox_value != nullptr) ImGui::MenuItem(e.label, nullptr, e.checkbox_value);
                        break;
                }
            }
        }

        const char *id_;
        ImGuiPopupFlags flags_;
        std::vector<menu_entry> entries_;
    };

} // namespace imgui_util
