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

#include <functional>
#include <imgui.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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
        [[nodiscard]] settings_panel &section(const std::string_view name, std::move_only_function<void()> render_fn) {
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
                                              std::move_only_function<void()> render_fn) {
            sections_.push_back(
                {.name = std::string(name), .parent = std::string(parent), .render_fn = std::move(render_fn)});
            return *this;
        }

        /**
         * @brief Render the settings panel (tree navigation + content area).
         * @param str_id ImGui ID string for the panel scope.
         */
        void render(const char *str_id) noexcept {
            const id scope{str_id};

            if (sections_.empty()) return;

            if (selected_idx_ < 0 || static_cast<std::size_t>(selected_idx_) >= sections_.size()) {
                selected_idx_ = 0;
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
                if (auto &sec = sections_[static_cast<std::size_t>(selected_idx_)]; sec.render_fn) {
                    ImGui::TextUnformatted(sec.name.c_str(), sec.name.c_str() + sec.name.size());
                    ImGui::Separator();
                    ImGui::Spacing();
                    sec.render_fn();
                }
            }
        }

    private:
        struct section_entry {
            std::string                     name;
            std::string                     parent;
            std::move_only_function<void()> render_fn;
        };

        std::vector<section_entry> sections_;
        int                        selected_idx_ = -1;

        void render_tree() noexcept {
            std::unordered_map<std::string_view, std::vector<int>> children_of;
            for (int i = 0; std::cmp_less(i, sections_.size()); ++i) {
                children_of[sections_[static_cast<std::size_t>(i)].parent].push_back(i);
            }

            for (const int i: children_of[""]) {
                render_tree_node(sections_[static_cast<std::size_t>(i)], i, children_of);
            }
        }

        void render_tree_node(const section_entry &entry, const int idx, // NOLINT(misc-no-recursion)
                              const std::unordered_map<std::string_view, std::vector<int>> &children_of) noexcept {
            if (const auto it = children_of.find(entry.name); it != children_of.end() && !it->second.empty()) {
                constexpr ImGuiTreeNodeFlags base_flags =
                    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
                const ImGuiTreeNodeFlags flags = base_flags | (selected_idx_ == idx ? ImGuiTreeNodeFlags_Selected : 0);

                if (const tree_node tn{entry.name.c_str(), flags}) {
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                        selected_idx_ = idx;
                    }
                    for (const int ci: it->second) {
                        render_tree_node(sections_[static_cast<std::size_t>(ci)], ci, children_of);
                    }
                } else {
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                        selected_idx_ = idx;
                    }
                }
            } else {
                if (ImGui::Selectable(entry.name.c_str(), selected_idx_ == idx)) {
                    selected_idx_ = idx;
                }
            }
        }
    };

} // namespace imgui_util
