/// @file tree_view.hpp
/// @brief Reusable tree view widget for hierarchical data.
///
/// Usage:
/// @code
///   imgui_util::tree_view<MyNode> tv{"Scene"};
///   tv.set_children([](const MyNode& n) -> std::span<const MyNode> { return n.children; })
///     .set_label([](const MyNode& n) { return n.name.c_str(); })
///     .set_on_select([&](const MyNode& n) { selected = &n; })
///     .render(root_nodes);
/// @endcode
///
/// Template on node type. Requires children accessor and label accessor.
/// Optional: on_select callback, on_context_menu callback, is_leaf predicate.
#pragma once

#include <functional>
#include <imgui.h>
#include <ranges>
#include <span>
#include <utility>

#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    /**
     * @brief Reusable tree view widget for hierarchical data.
     *
     * Configure with builder-style setters for children, label, selection, and
     * context menu callbacks, then call render() each frame with the root nodes.
     * @tparam NodeT Node type. Must be stable in memory across frames for selection
     *               tracking (see selected_ pointer).
     */
    template<typename NodeT>
    class tree_view {
    public:
        using children_fn     = std::move_only_function<std::span<const NodeT>(const NodeT &)>;
        using label_fn        = std::move_only_function<const char *(const NodeT &)>;
        using select_fn       = std::move_only_function<void(const NodeT &)>;
        using context_menu_fn = std::move_only_function<void(const NodeT &)>;
        using leaf_fn         = std::move_only_function<bool(const NodeT &)>;

        /// @brief Construct a tree view with the given ImGui ID scope.
        explicit tree_view(const char *id) noexcept : id_(id) {}

        /// @brief Set the callback that returns child nodes for a given node.
        tree_view &set_children(children_fn fn) noexcept {
            children_fn_ = std::move(fn);
            return *this;
        }
        /// @brief Set the callback that returns the display label for a node.
        tree_view &set_label(label_fn fn) noexcept {
            label_fn_ = std::move(fn);
            return *this;
        }
        /// @brief Set the callback invoked when a node is selected.
        tree_view &set_on_select(select_fn fn) noexcept {
            select_fn_ = std::move(fn);
            return *this;
        }
        /// @brief Set the callback invoked to render a right-click context menu for a node.
        tree_view &set_on_context_menu(context_menu_fn fn) noexcept {
            context_fn_ = std::move(fn);
            return *this;
        }
        /// @brief Set a predicate that overrides the default leaf detection (empty children).
        tree_view &set_is_leaf(leaf_fn fn) noexcept {
            leaf_fn_ = std::move(fn);
            return *this;
        }

        /// @brief Render the tree, starting from the given root nodes.
        template<std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, const NodeT &>
        void render(const R &roots) {
            const id scope{id_};
            for (const auto &node: roots) {
                render_node(node);
            }
        }

    private:
        const char     *id_;
        children_fn     children_fn_;
        label_fn        label_fn_;
        select_fn       select_fn_;
        context_menu_fn context_fn_;
        leaf_fn         leaf_fn_;
        // WARNING: raw pointer into caller's node storage -- dangles if the container
        // is reallocated (e.g. std::vector resize) between frames. Callers must ensure
        // node addresses remain stable for the lifetime of this tree_view, or call
        // render() each frame so that selection is re-evaluated before use.
        const NodeT    *selected_ = nullptr;

        void render_node(const NodeT &node) { // NOLINT(misc-no-recursion)
            const char *const label = label_fn_ ? label_fn_(node) : "???";

            // Cache children result to avoid calling children_fn_ twice
            const auto children = children_fn_ ? children_fn_(node) : std::span<const NodeT>{};
            const bool is_leaf  = leaf_fn_ ? leaf_fn_(node) : children.empty();
            const bool selected = selected_ == &node;

            ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (is_leaf) node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (selected) node_flags |= ImGuiTreeNodeFlags_Selected;

            if (is_leaf) {
                // Leaf nodes don't push tree scope
                ImGui::TreeNodeEx(label, node_flags);
                handle_interaction(node);
            } else {
                if (const tree_node tn{label, node_flags}) {
                    handle_interaction(node);
                    for (const auto &child: children) {
                        render_node(child);
                    }
                } else {
                    handle_interaction(node);
                }
            }
        }

        void handle_interaction(const NodeT &node) {
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                selected_ = &node;
                if (select_fn_) select_fn_(node);
            }
            if (context_fn_) {
                if (const popup_context_item ctx{nullptr}) {
                    context_fn_(node);
                }
            }
        }
    };

} // namespace imgui_util
