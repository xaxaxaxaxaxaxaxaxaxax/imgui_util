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
//   imgui_util::toolbar(imgui_util::direction::vertical)
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
#include "imgui_util/widgets/helpers.hpp"

namespace imgui_util {

    class toolbar {
    public:
        // toolbar_direction is an alias for the shared direction enum for backward compat
        using toolbar_direction          = direction;
        // Also expose as nested for callers that used toolbar::direction
        static constexpr auto horizontal = direction::horizontal;
        static constexpr auto vertical   = direction::vertical;

        explicit toolbar(const direction dir = direction::horizontal) noexcept : dir_(dir) { entries_.reserve(8); }

        [[nodiscard]] toolbar &button(const char *label, std::move_only_function<void()> action,
                                      const char *tooltip = nullptr, const bool enabled = true) {
            entries_.push_back({.type    = entry_type::button,
                                .label   = label,
                                .action  = std::move(action),
                                .tooltip = tooltip,
                                .enabled = enabled});
            return *this;
        }

        [[nodiscard]] toolbar &icon_button(const char *str_id, const ImTextureID texture, const ImVec2 &size,
                                           std::move_only_function<void()> action, const char *tooltip = nullptr,
                                           const bool enabled = true) {
            entries_.push_back({.type    = entry_type::icon_button,
                                .label   = str_id,
                                .action  = std::move(action),
                                .tooltip = tooltip,
                                .icon    = {.texture = texture, .size = size},
                                .enabled = enabled});
            return *this;
        }

        [[nodiscard]] toolbar &toggle(const char *label, bool *value, const char *tooltip = nullptr) {
            entries_.push_back({.type = entry_type::toggle, .label = label, .tooltip = tooltip, .value = value});
            return *this;
        }

        [[nodiscard]] toolbar &separator() {
            entries_.push_back({.type = entry_type::separator});
            return *this;
        }

        [[nodiscard]] toolbar &label(const char *text) {
            entries_.push_back({.type = entry_type::label, .label = text});
            return *this;
        }

        void render() {
            for (std::size_t i = 0; i < entries_.size(); ++i) {
                auto &[type, lbl, action, tip, icon, value, enabled] = entries_[i]; // NOLINT(misc-const-correctness)
                render_entry(type, lbl, action, tip, icon, value, enabled);

                if (dir_ == direction::horizontal && i + 1 < entries_.size()
                    && entries_[i + 1].type != entry_type::separator) {
                    ImGui::SameLine();
                }
            }
        }

    private:
        enum class entry_type : std::uint8_t { button, icon_button, toggle, separator, label };

        struct icon_data {
            ImTextureID texture = {};
            ImVec2      size    = {0, 0};
        };

        struct toolbar_entry {
            entry_type                      type;
            const char                     *label   = nullptr;
            std::move_only_function<void()> action  = {};
            const char                     *tooltip = nullptr;
            icon_data                       icon    = {};
            bool                           *value   = nullptr;
            bool                            enabled = true;
        };

        void render_entry(const entry_type type, const char *lbl, std::move_only_function<void()> &action,
                          const char *tip, const icon_data &icon, bool *value, const bool enabled) const {
            switch (type) {
                case entry_type::button: {
                    const disabled guard{!enabled};
                    if (ImGui::Button(lbl) && action) action();
                    break;
                }
                case entry_type::icon_button: {
                    const disabled guard{!enabled};
                    if (ImGui::ImageButton(lbl, icon.texture, icon.size) && action) action();
                    break;
                }
                case entry_type::toggle: {
                    if (value) {
                        const auto       &colors = ImGui::GetStyle().Colors;
                        const style_color col(ImGuiCol_Button,
                                              *value ? colors[ImGuiCol_ButtonActive] : colors[ImGuiCol_Button]);
                        if (ImGui::Button(lbl)) *value = !*value;
                    }
                    break;
                }
                case entry_type::label: {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(lbl);
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

            if (tip) {
                if (const item_tooltip tt{}) {
                    ImGui::TextUnformatted(tip);
                }
            }
        }

        direction                  dir_;
        std::vector<toolbar_entry> entries_;
    };

} // namespace imgui_util
