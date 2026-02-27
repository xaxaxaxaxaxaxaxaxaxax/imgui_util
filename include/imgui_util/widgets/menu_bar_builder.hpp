/// @file menu_bar_builder.hpp
/// @brief Fluent builder for ImGui menu bars.
///
/// Uses RAII menu_bar / main_menu_bar / menu wrappers from core/raii.hpp.
///
/// Usage:
/// @code
///   imgui_util::menu_bar_builder()
///       .menu("File", [](auto& m) {
///           m.item("New", [&]{ new_file(); }, "Ctrl+N")
///            .item("Open", [&]{ open_file(); }, "Ctrl+O")
///            .separator()
///            .item("Exit", [&]{ exit(); });
///       })
///       .menu("Edit", [](auto& m) {
///           m.item("Undo", [&]{ undo(); }, "Ctrl+Z");
///       })
///       .render();       // window menu bar
///
///   imgui_util::menu_bar_builder()
///       .menu("File", [](auto& m) { ... })
///       .render_main();  // main/viewport menu bar
/// @endcode
#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/detail/menu_common.hpp"

namespace imgui_util {

    /// @brief Fluent builder for constructing ImGui menu bars declaratively.
    class menu_bar_builder {
    public:
        menu_bar_builder() noexcept { entries_.reserve(8); }

        /**
         * @brief Add a top-level menu or submenu.
         * @param label    Menu label.
         * @param content  Callable that populates the menu via the builder reference.
         */
        template<std::invocable<menu_bar_builder &> Fn>
        [[nodiscard]] menu_bar_builder &menu(const char *label, Fn &&content) {
            menu_bar_builder sub;
            std::forward<Fn>(content)(sub);
            entries_.push_back({.type           = detail::menu_entry_type::submenu,
                                .label          = label,
                                .action         = {},
                                .shortcut       = nullptr,
                                .checkbox_value = nullptr,
                                .enabled        = true,
                                .children       = std::move(sub.entries_)});
            return *this;
        }

        /**
         * @brief Add a clickable menu item.
         * @param label     Item label.
         * @param action    Callback invoked when the item is clicked.
         * @param shortcut  Optional shortcut text displayed beside the item.
         * @param enabled   Whether the item is enabled.
         */
        [[nodiscard]] menu_bar_builder &item(const char *label, std::move_only_function<void()> action,
                                             const char *shortcut = nullptr, const bool enabled = true) {
            entries_.push_back({.type           = detail::menu_entry_type::item,
                                .label          = label,
                                .action         = std::move(action),
                                .shortcut       = shortcut,
                                .checkbox_value = nullptr,
                                .enabled        = enabled,
                                .children       = {}});
            return *this;
        }

        /// @brief Add a visual separator line.
        [[nodiscard]] menu_bar_builder &separator() {
            entries_.push_back({.type           = detail::menu_entry_type::separator,
                                .label          = nullptr,
                                .action         = {},
                                .shortcut       = nullptr,
                                .checkbox_value = nullptr,
                                .enabled        = true,
                                .children       = {}});
            return *this;
        }

        /**
         * @brief Add a checkbox menu item.
         * @param label  Item label.
         * @param value  Pointer to the boolean toggled by the checkbox.
         */
        [[nodiscard]] menu_bar_builder &checkbox(const char *label, bool *value) { // NOLINT(*-non-const-parameter)
            entries_.push_back({.type           = detail::menu_entry_type::checkbox,
                                .label          = label,
                                .action         = {},
                                .shortcut       = nullptr,
                                .checkbox_value = value,
                                .enabled        = true,
                                .children       = {}});
            return *this;
        }

        /// @brief Render as a window-level menu bar.
        void render() {
            if (const menu_bar mb{}) {
                detail::render_menu_entries(entries_);
            }
        }

        /// @brief Render as a main/viewport menu bar.
        void render_main() {
            if (const main_menu_bar mb{}) {
                detail::render_menu_entries(entries_);
            }
        }

    private:
        std::vector<detail::menu_entry> entries_;
    };

} // namespace imgui_util
