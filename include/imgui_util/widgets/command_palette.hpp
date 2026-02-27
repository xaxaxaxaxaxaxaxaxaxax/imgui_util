/// @file command_palette.hpp
/// @brief Fuzzy-search command popup (like VS Code Ctrl+P).
///
/// Enter/click invokes the selected command and closes the palette.
/// Escape closes without invoking.
///
/// Usage:
/// @code
///   static imgui_util::command_palette palette;
///   palette.add("Open File", [&]{ open_dialog(); });
///   palette.add("Save", [&]{ save(); });
///   if (ImGui::IsKeyPressed(ImGuiKey_P) && ImGui::GetIO().KeyCtrl)
///       palette.open();
///   palette.render();
/// @endcode
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <imgui.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    /// @brief Fuzzy-search command popup (similar to VS Code Ctrl+P).
    class command_palette {
    public:
        /**
         * @brief Register a named command.
         * @param name      Display name shown in the results list.
         * @param callback  Action invoked when the command is selected.
         */
        void add(std::string name, std::move_only_function<void()> callback) {
            commands_.push_back({.name = std::move(name), .callback = std::move(callback)});
        }

        /// @brief Remove all registered commands.
        void clear() noexcept { commands_.clear(); }

        /// @brief Open the palette popup (resets filter and selection).
        void open() noexcept {
            should_open_ = true;
            filter_.fill('\0');
            prev_filter_.fill('\0');
            selected_ = 0;
        }

        /// @brief Render the command palette. Call once per frame.
        void render() {
            if (should_open_) {
                ImGui::OpenPopup("##cmd_palette");
                should_open_ = false;
            }

            ImGui::SetNextWindowSize({400.0f, 0.0f}, ImGuiCond_Always);

            constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;

            if (const popup_modal pm{"##cmd_palette", nullptr, flags}) {
                // Center on screen
                const auto *vp = ImGui::GetMainViewport();
                ImGui::SetWindowPos({
                    vp->WorkPos.x + (vp->WorkSize.x - 400.0f) * 0.5f,
                    vp->WorkPos.y + vp->WorkSize.y * 0.25f,
                });

                // Filter input
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##input", "Type a command...", filter_.data(), filter_.size());

                update_scored_results();
                handle_keyboard();
                render_results_list();
            }
        }

    private:
        struct command_entry {
            std::string                     name;
            std::move_only_function<void()> callback;
        };

        struct scored_entry {
            int idx;
            int score;
        };

        void update_scored_results() {
            const std::string_view query{filter_.data()};
            if (filter_ != prev_filter_) {
                prev_filter_ = filter_;
                scored_.clear();
                for (int i = 0; std::cmp_less(i, commands_.size()); ++i) {
                    if (int score = 0; query.empty() || fuzzy_match(query, commands_[i].name, score)) {
                        scored_.push_back({.idx = i, .score = score});
                    }
                }
                std::ranges::sort(scored_, [](const auto &a, const auto &b) { return a.score > b.score; });
            }
        }

        void handle_keyboard() {
            const int max_idx = scored_.empty() ? 0 : static_cast<int>(scored_.size()) - 1;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selected_ < max_idx) ++selected_;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && selected_ > 0) --selected_;
            selected_ = std::clamp(selected_, 0, max_idx);

            // Enter to invoke selected
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !scored_.empty()) {
                commands_[scored_[selected_].idx].callback();
                ImGui::CloseCurrentPopup();
            }

            // Escape to close
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                ImGui::CloseCurrentPopup();
            }
        }

        void render_results_list() {
            ImGui::Separator();
            const int max_visible = std::min(static_cast<int>(scored_.size()), 10);
            for (int i = 0; i < max_visible; ++i) {
                auto &[name, callback] = commands_[scored_[i].idx];
                const bool sel         = i == selected_;

                if (ImGui::Selectable(name.c_str(), sel)) {
                    callback();
                    ImGui::CloseCurrentPopup();
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }

            if (scored_.empty()) {
                ImGui::TextDisabled("No matching commands");
            }
        }

        // Fuzzy match: all query chars must appear in order in candidate.
        // Score prefers consecutive matches and matches near the start.
        [[nodiscard]] static bool fuzzy_match(const std::string_view query, const std::string_view candidate,
                                              int &score) noexcept {
            score                  = 0;
            std::size_t ci         = 0;
            int         last_match = -1;

            for (const char qc: query) {
                const char qlower = static_cast<char>(std::tolower(static_cast<unsigned char>(qc)));
                bool       found  = false;
                while (ci < candidate.size()) {
                    if (const char clower = static_cast<char>(std::tolower(static_cast<unsigned char>(candidate[ci])));
                        qlower == clower) {
                        // Bonus for consecutive matches
                        if (last_match >= 0 && static_cast<int>(ci) == last_match + 1) score += 5;
                        // Bonus for early matches
                        score += std::max(0, 10 - static_cast<int>(ci));
                        last_match = static_cast<int>(ci);
                        ++ci;
                        found = true;
                        break;
                    }
                    ++ci;
                }
                if (!found) return false;
            }
            return true;
        }

        std::vector<command_entry> commands_;
        std::vector<scored_entry>  scored_; // transient per-frame
        std::array<char, 128>      filter_{};
        std::array<char, 128>      prev_filter_{};
        int                        selected_    = 0;
        bool                       should_open_ = false;
    };

} // namespace imgui_util
