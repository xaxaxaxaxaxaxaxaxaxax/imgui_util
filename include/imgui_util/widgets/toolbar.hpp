// toolbar.hpp - Toolbar builder with buttons, toggles, and separators
//
// Usage:
//   imgui_util::toolbar()
//       .button("New", [&]{ new_file(); }, "Create new file")
//       .button("Open", [&]{ open_file(); })
//       .separator()
//       .toggle("Grid", &show_grid, "Toggle grid overlay")
//       .render();
//
//   // Vertical toolbar:
//   imgui_util::toolbar(imgui_util::toolbar::direction::vertical)
//       .button("A", [&]{ action_a(); })
//       .button("B", [&]{ action_b(); })
//       .render();
//
// Renders a row (or column) of buttons with optional tooltips and toggle state.
#pragma once

#include <cstddef>
#include <functional>
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    class toolbar {
    public:
        enum class direction { horizontal, vertical };

        explicit toolbar(direction dir = direction::horizontal) : dir_(dir) {}

        toolbar &button(const char *label, std::function<void()> action, const char *tooltip = nullptr) {
            entries_.push_back({entry_type::button, label, std::move(action), tooltip, nullptr});
            return *this;
        }

        toolbar &toggle(const char *label, bool *value, const char *tooltip = nullptr) {
            entries_.push_back({entry_type::toggle, label, {}, tooltip, value});
            return *this;
        }

        toolbar &separator() {
            entries_.push_back({entry_type::separator, nullptr, {}, nullptr, nullptr});
            return *this;
        }

        void render() {
            for (std::size_t i = 0; i < entries_.size(); ++i) {
                auto &e = entries_[i];
                switch (e.type) {
                    case entry_type::button:
                        if (ImGui::Button(e.label)) {
                            if (e.action) e.action();
                        }
                        break;
                    case entry_type::toggle: {
                        if (e.value != nullptr) {
                            const bool active = *e.value;
                            if (active) {
                                const style_color col(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                                if (ImGui::Button(e.label)) *e.value = !*e.value;
                            } else {
                                if (ImGui::Button(e.label)) *e.value = !*e.value;
                            }
                        }
                        break;
                    }
                    case entry_type::separator:
                        if (dir_ == direction::horizontal) {
                            ImGui::SameLine();
                            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                        } else {
                            ImGui::Separator();
                        }
                        break;
                }
                if (e.tooltip != nullptr && ImGui::IsItemHovered()) {
                    if (ImGui::BeginTooltip()) {
                        ImGui::TextUnformatted(e.tooltip);
                        ImGui::EndTooltip();
                    }
                }
                if (dir_ == direction::horizontal && i + 1 < entries_.size() && entries_[i + 1].type != entry_type::separator) {
                    ImGui::SameLine();
                }
            }
        }

    private:
        enum class entry_type { button, toggle, separator };
        struct toolbar_entry {
            entry_type type;
            const char *label;
            std::function<void()> action;
            const char *tooltip;
            bool *value;
        };

        direction dir_;
        std::vector<toolbar_entry> entries_;
    };

} // namespace imgui_util
