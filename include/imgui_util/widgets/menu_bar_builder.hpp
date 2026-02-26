// menu_bar_builder.hpp - Fluent builder for ImGui menu bars
//
// Usage:
//   imgui_util::menu_bar_builder()
//       .menu("File", [](auto& m) {
//           m.item("New", [&]{ new_file(); }, "Ctrl+N")
//            .item("Open", [&]{ open_file(); }, "Ctrl+O")
//            .separator()
//            .item("Exit", [&]{ exit(); });
//       })
//       .menu("Edit", [](auto& m) {
//           m.item("Undo", [&]{ undo(); }, "Ctrl+Z");
//       })
//       .render();       // window menu bar
//
//   imgui_util::menu_bar_builder()
//       .menu("File", [](auto& m) { ... })
//       .render_main();  // main/viewport menu bar
//
// Uses RAII menu_bar / main_menu_bar / menu wrappers from core/raii.hpp.
#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/detail/menu_common.hpp"

namespace imgui_util {

    class menu_bar_builder {
    public:
        menu_bar_builder() noexcept { entries_.reserve(8); }

        template<std::invocable<menu_bar_builder&> Fn>
        [[nodiscard]] menu_bar_builder& menu(const char* label, Fn&& content) {
            menu_bar_builder sub;
            std::forward<Fn>(content)(sub);
            entries_.push_back({detail::menu_entry_type::submenu, label, {}, nullptr, nullptr, true,
                                std::move(sub.entries_)});
            return *this;
        }

        [[nodiscard]] menu_bar_builder& item(const char* label, std::move_only_function<void()> action,
                                             const char* shortcut = nullptr, const bool enabled = true) {
            entries_.push_back(
                {detail::menu_entry_type::item, label, std::move(action), shortcut, nullptr, enabled, {}});
            return *this;
        }

        [[nodiscard]] menu_bar_builder& separator() {
            entries_.push_back({detail::menu_entry_type::separator, nullptr, {}, nullptr, nullptr, true, {}});
            return *this;
        }

        [[nodiscard]] menu_bar_builder& checkbox(const char* label, bool* value) {
            entries_.push_back({detail::menu_entry_type::checkbox, label, {}, nullptr, value, true, {}});
            return *this;
        }

        void render() {
            if (const menu_bar mb{}) {
                detail::render_menu_entries(entries_);
            }
        }

        void render_main() {
            if (const main_menu_bar mb{}) {
                detail::render_menu_entries(entries_);
            }
        }

    private:
        std::vector<detail::menu_entry> entries_;
    };

} // namespace imgui_util
