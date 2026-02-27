/// @file undo_stack.hpp
/// @brief Generic undo/redo stack with visual history panel.
///
/// Usage:
/// @code
///   struct my_state { int x; float y; };
///   static imgui_util::undo_stack<my_state> undo{my_state{0, 1.0f}};
///
///   // After user action:
///   undo.push("Changed X", my_state{42, 1.0f});
///
///   // Each frame:
///   if (undo.handle_shortcuts()) { apply(undo.current()); }
///
///   // History panel:
///   static bool history_open = true;
///   undo.render_history_panel("##history", &history_open);
/// @endcode
///
/// Template on any std::copyable State type. Supports configurable max depth,
/// named entries, Ctrl+Z/Y shortcuts, and clickable history panel.
#pragma once

#include <concepts>
#include <cstddef>
#include <imgui.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    /**
     * @brief Generic undo/redo stack with visual history panel.
     * @tparam State Any std::copyable type representing a snapshot of application state.
     */
    template<std::copyable State>
    class undo_stack {
    public:
        /**
         * @brief Construct an undo stack with an initial state.
         * @param initial    The starting state (becomes the first history entry).
         * @param max_depth  Maximum number of entries retained (default 100). Oldest entries
         *                   are discarded when exceeded.
         */
        explicit undo_stack(State initial, const std::size_t max_depth = 100) : max_depth_(max_depth) {
            stack_.push_back({.description = "Initial", .state = std::move(initial)});
        }

        /**
         * @brief Push a new state snapshot, discarding any redo history.
         * @param description  Human-readable label shown in the history panel.
         * @param snapshot     The state to record.
         */
        void push(const std::string_view description, State snapshot) noexcept {
            stack_.resize(current_index_ + 1);
            stack_.push_back({.description = std::string(description), .state = std::move(snapshot)});
            current_index_ = stack_.size() - 1;
            enforce_max_depth();
        }

        /// @brief Step back one entry. Returns true if the position changed.
        [[nodiscard]] bool undo() noexcept {
            if (!can_undo()) return false;
            --current_index_;
            return true;
        }

        /// @brief Step forward one entry. Returns true if the position changed.
        [[nodiscard]] bool redo() noexcept {
            if (!can_redo()) return false;
            ++current_index_;
            return true;
        }

        /// @brief Access the state at the current position.
        [[nodiscard]] const State &current() const noexcept { return stack_[current_index_].state; }
        [[nodiscard]] bool         can_undo() const noexcept { return current_index_ > 0; }
        [[nodiscard]] bool         can_redo() const noexcept { return current_index_ + 1 < stack_.size(); }
        [[nodiscard]] std::size_t  depth() const noexcept { return stack_.size(); }

        /// @brief Handle Ctrl+Z / Ctrl+Y. Returns true if state changed.
        [[nodiscard]] bool handle_shortcuts() noexcept {
            const bool ctrl = ImGui::GetIO().KeyCtrl;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) return undo();
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) return redo();
            return false;
        }

        /// @brief Handle Ctrl+Z / Ctrl+Y and invoke callback with current() on change.
        template<std::invocable<const State &> F>
        bool handle_shortcuts(F &&callback) noexcept {
            if (handle_shortcuts()) {
                std::forward<F>(callback)(current());
                return true;
            }
            return false;
        }

        /**
         * @brief Render a clickable history panel with undo/redo toolbar.
         * @param panel_id  ImGui window ID for the panel.
         * @param open      Optional visibility flag (pass nullptr to always show).
         */
        void render_history_panel(const char *panel_id, bool *open = nullptr) noexcept {
            if (const window win{panel_id, open}) {
                render_toolbar();
                ImGui::Separator();
                render_history_list();
            }
        }

        /// @brief Reset the stack with a new initial state, discarding all history.
        void clear(State initial) noexcept {
            stack_.clear();
            stack_.push_back({.description = "Initial", .state = std::move(initial)});
            current_index_ = 0;
        }

    private:
        struct entry {
            std::string description;
            State       state;
        };

        std::vector<entry> stack_;
        std::size_t        current_index_ = 0;
        std::size_t        max_depth_;

        void enforce_max_depth() noexcept {
            if (stack_.size() <= max_depth_) return;
            const auto excess = stack_.size() - max_depth_;
            stack_.erase(stack_.begin(), stack_.begin() + static_cast<std::ptrdiff_t>(excess));
            current_index_ -= excess;
        }

        void render_toolbar() noexcept {
            {
                const disabled guard{!can_undo()};
                if (ImGui::Button("Undo")) (void) undo();
            }
            ImGui::SameLine();
            {
                const disabled guard{!can_redo()};
                if (ImGui::Button("Redo")) (void) redo();
            }
            ImGui::SameLine();
            const fmt_buf<32> pos("{}/{}", current_index_ + 1, stack_.size());
            dim_text(pos.sv());
        }

        void render_history_list() noexcept {
            if (const child list{"##undo_list"}; !list) return;

            for (std::size_t i = 0; i < stack_.size(); ++i) {
                const auto &[description, state] = stack_[i];
                const bool is_current            = i == current_index_;
                const id   entry_id{static_cast<int>(i)};

                if (i > current_index_) {
                    const style_var alpha{ImGuiStyleVar_Alpha, 0.5f};
                    if (ImGui::Selectable(description.c_str(), is_current)) current_index_ = i;
                } else if (is_current) {
                    const fmt_buf<256> label("> {}", description);
                    ImGui::Selectable(label.c_str(), true);
                } else if (ImGui::Selectable(description.c_str(), false)) {
                    current_index_ = i;
                }
            }
        }
    };

} // namespace imgui_util
