/// @file settings_panel.hpp
/// @brief Tree-navigated settings panel.
///
/// Left pane shows a navigable tree of sections. Right pane renders the
/// selected section's content. Remembers selection across frames.
///
/// Usage:
/// @code
///   static imgui_util::settings_panel panel;
///   panel.section("General", [] {
///       ImGui::Text("General settings...");
///   })
///   .section("Appearance", [] {
///       ImGui::ColorEdit3("Accent", &color.x);
///   })
///   .section("Fonts", "Appearance", [] {
///       ImGui::Text("Font settings under Appearance...");
///   });
///   panel.render("##settings", &open);
/// @endcode
#pragma once

#include <algorithm>
#include <functional>
#include <imgui.h>
#include <string>
#include <string_view>
#include <vector>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    /**
     * @brief Tree-navigated settings panel with a left-side section tree and right-side content area.
     *
     * Add sections with section(), optionally nested under a parent, then call render() each frame.
     */
    class settings_panel {
    public:
        /**
         * @brief Add a top-level section.
         * @param name      Section display name (must be unique).
         * @param render_fn Callback that renders the section content.
         * @return Reference to this panel for chaining.
         */
        [[nodiscard]] settings_panel &section(const std::string_view          name,
                                              std::move_only_function<void()> render_fn) noexcept {
            sections_.push_back({.name = std::string(name), .parent = {}, .render_fn = std::move(render_fn)});
            return *this;
        }

        /**
         * @brief Add a section nested under an existing parent.
         * @param name      Section display name (must be unique).
         * @param parent    Name of the parent section.
         * @param render_fn Callback that renders the section content.
         * @return Reference to this panel for chaining.
         */
        [[nodiscard]] settings_panel &section(const std::string_view name, const std::string_view parent,
                                              std::move_only_function<void()> render_fn) noexcept {
            sections_.push_back(
                {.name = std::string(name), .parent = std::string(parent), .render_fn = std::move(render_fn)});
            return *this;
        }

        /**
         * @brief Render the settings panel (tree navigation + content area).
         * @param str_id ImGui ID string for the panel scope.
         * @param open   Optional open state pointer (currently unused, reserved for modal usage).
         */
        void render(const std::string_view str_id, [[maybe_unused]] const bool *open = nullptr) noexcept {
            const id scope{str_id.data()};

            if (sections_.empty()) return;

            if (selected_.empty()) {
                selected_ = sections_[0].name;
            }

            const auto      avail      = ImGui::GetContentRegionAvail();
            constexpr float left_ratio = 0.3f;
            const auto      left_w     = avail.x * left_ratio;
            const auto      right_w    = avail.x - left_w - 8.0f; // 8px gap

            {
                const child left_child{"##settings_nav", ImVec2(left_w, 0), ImGuiChildFlags_Borders};
                render_tree();
            }

            ImGui::SameLine();

            {
                const child right_child{"##settings_content", ImVec2(right_w, 0), ImGuiChildFlags_Borders};
                const auto  it = std::ranges::find_if(
                    sections_, [&](const auto &sec) { return sec.name == selected_ && sec.render_fn; });
                if (it != sections_.end()) {
                    ImGui::TextUnformatted(it->name.c_str(), it->name.c_str() + it->name.size());
                    ImGui::Separator();
                    ImGui::Spacing();
                    it->render_fn();
                }
            }

            sections_.clear();
        }

    private:
        struct section_entry {
            std::string                     name;
            std::string                     parent;
            std::move_only_function<void()> render_fn;
        };

        std::vector<section_entry> sections_;
        std::string                selected_;

        void render_tree() noexcept {
            for (const auto &sec: sections_) {
                if (sec.parent.empty()) {
                    render_tree_node(sec);
                }
            }
        }

        void render_tree_node(const section_entry &entry) noexcept { // NOLINT(misc-no-recursion)
            const bool has_children =
                std::ranges::any_of(sections_, [&](const auto &sec) { return sec.parent == entry.name; });

            if (has_children) {
                constexpr ImGuiTreeNodeFlags base_flags =
                    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
                const ImGuiTreeNodeFlags flags =
                    base_flags | (selected_ == entry.name ? ImGuiTreeNodeFlags_Selected : 0);

                if (const tree_node tn{entry.name.c_str(), flags}) {
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                        selected_ = entry.name;
                    }
                    for (const auto &sec: sections_) {
                        if (sec.parent == entry.name) {
                            render_tree_node(sec);
                        }
                    }
                } else {
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                        selected_ = entry.name;
                    }
                }
            } else {
                if (ImGui::Selectable(entry.name.c_str(), selected_ == entry.name)) {
                    selected_ = entry.name;
                }
            }
        }
    };

} // namespace imgui_util
