// tab_bar_builder.hpp - Declarative tab bar builder
//
// Usage:
//   imgui_util::tab_bar_builder("MyTabs")
//       .tab("General", [&]{ render_general(); })
//       .tab("Advanced", [&]{ render_advanced(); })
//       .tab("Optional", [&]{ render_optional(); }, &show_optional)
//       .render();
//
// Uses existing RAII tab_bar/tab_item wrappers from core/raii.hpp.
#pragma once

#include <functional>
#include <imgui.h>
#include <vector>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    class tab_bar_builder {
    public:
        explicit tab_bar_builder(const char *id, ImGuiTabBarFlags flags = 0)
            : id_(id), flags_(flags) {}

        tab_bar_builder &tab(const char *label, std::function<void()> render_fn,
                             bool *open = nullptr, ImGuiTabItemFlags flags = 0) {
            tabs_.push_back({label, std::move(render_fn), open, flags});
            return *this;
        }

        void render() {
            if (const tab_bar tb{id_, flags_}) {
                for (auto &t : tabs_) {
                    if (const tab_item ti{t.label, t.open, t.flags}) {
                        if (t.render_fn) t.render_fn();
                    }
                }
            }
        }

    private:
        struct tab_entry {
            const char *label;
            std::function<void()> render_fn;
            bool *open;
            ImGuiTabItemFlags flags;
        };

        const char *id_;
        ImGuiTabBarFlags flags_;
        std::vector<tab_entry> tabs_;
    };

} // namespace imgui_util
