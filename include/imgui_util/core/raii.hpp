/// @file raii.hpp
/// @brief RAII scoped wrappers for all ImGui Begin/End and Push/Pop pairs.
///
/// End/Pop is called automatically in the destructor, even on early return.
/// Scopes that track a bool (window, tab_bar, etc.) convert to bool for if-blocks.
///
/// Usage:
/// @code
///   if (imgui_util::window w{"Settings", &open}) { ImGui::Text("..."); }
///   if (imgui_util::tab_bar tb{"Tabs"}) { ... }
///   { imgui_util::style_var sv{ImGuiStyleVar_Alpha, 0.5f}; ... }
///   { imgui_util::id scope{"my_id"}; ... }
/// @endcode
#pragma once

#include <concepts>
#include <imgui.h>
#include <initializer_list>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

namespace imgui_util {

    /**
     * @brief Determines when the end/pop function is called.
     *
     * - always:      end() unconditionally (e.g. Window, Group).
     * - conditional: end() only if begin() returned true (e.g. TabBar, Menu).
     * - none:        always pop, no bool tracking (e.g. PushStyleVar, PushID).
     */
    enum class end_policy { always, conditional, none };

    /**
     * @brief Generic RAII wrapper that calls Trait::begin() on construction and
     *        Trait::end() on destruction according to the trait's end_policy.
     * @tparam Trait A type providing static begin(), end(), policy, and storage.
     */
    template<typename Trait>
    class [[nodiscard]] raii_scope {
        static constexpr end_policy policy    = Trait::policy;              // how/when to call end()
        static constexpr bool       has_state = policy != end_policy::none; // track begin() return?
        static constexpr bool       has_storage =
            !std::is_same_v<typename Trait::storage, std::monostate>; // extra data for end()

        [[no_unique_address]] std::conditional_t<has_state, bool, std::monostate>
                                             state_{};   // begin() result; monostate when unused
        [[no_unique_address]] Trait::storage storage_{}; // extra data passed to end(); monostate when unused

    public:
        template<typename... Args>
            requires requires { Trait::begin(std::declval<Args>()...); }
        explicit raii_scope(Args &&...args) noexcept(noexcept(Trait::begin(std::declval<Args>()...))) {
            static_assert(std::is_void_v<decltype(Trait::begin(std::declval<Args>()...))>
                              || std::is_same_v<decltype(Trait::begin(std::declval<Args>()...)), bool> || has_storage,
                          "Trait::begin() returns a non-void/non-bool value but storage is monostate — the return "
                          "value would be lost");
            if constexpr (requires {
                              { Trait::begin(std::forward<Args>(args)...) } -> std::same_as<bool>;
                          }) {
                // Pattern: bool begin(args...) - e.g., window, tab_bar
                if constexpr (has_state) {
                    state_ = Trait::begin(std::forward<Args>(args)...);
                } else {
                    (void) Trait::begin(std::forward<Args>(args)...);
                }
            } else if constexpr (has_storage) {
                // Pattern: Storage begin(args...) - e.g., indent returns float
                storage_ = Trait::begin(std::forward<Args>(args)...);
            } else {
                // Pattern: void begin(args...) - e.g., group, style_var
                Trait::begin(std::forward<Args>(args)...);
            }
        }

        ~raii_scope() {
            if constexpr (policy == end_policy::always) {
                Trait::end();
            } else if constexpr (policy == end_policy::conditional) {
                if (state_) Trait::end();
            } else {
                // end_policy::none - always call end, may need storage
                if constexpr (has_storage) {
                    Trait::end(storage_);
                } else {
                    Trait::end();
                }
            }
        }

        raii_scope(const raii_scope &)            = delete;
        raii_scope &operator=(const raii_scope &) = delete;
        raii_scope(raii_scope &&)                 = delete;
        raii_scope &operator=(raii_scope &&)      = delete;

        [[nodiscard]] explicit operator bool() const noexcept
            requires has_state
        {
            return state_;
        }

        [[nodiscard]] bool is_active() const noexcept
            requires has_state
        {
            return state_;
        }
    };

    /**
     * @brief Trait helper for ImGui pairs with no extra storage.
     * @tparam P        The end_policy governing when EndFn is called.
     * @tparam BeginFn  Pointer to the ImGui Begin/Push function.
     * @tparam EndFn    Pointer to the ImGui End/Pop function.
     */
    template<end_policy P, auto BeginFn, auto EndFn>
    struct simple_trait {
        static constexpr end_policy policy = P;
        using storage                      = std::monostate;
        template<typename... Args>
            requires std::is_invocable_v<decltype(BeginFn), Args...>
        static auto begin(Args &&...args) noexcept(noexcept(BeginFn(std::forward<Args>(args)...))) {
            return BeginFn(std::forward<Args>(args)...);
        }
        static void end() noexcept(noexcept(EndFn())) { EndFn(); }
    };

    /**
     * @brief Generic multi-push RAII wrapper. Pushes N entries on construction, pops all in destructor.
     * @tparam Entry  Struct with .idx and .val (variant) fields.
     * @tparam PushFn Callable that pushes one entry (takes idx + variant alternative).
     * @tparam PopFn  Callable that pops N entries (takes int count).
     */
    template<typename Entry, auto PushFn, auto PopFn>
    class [[nodiscard]] multi_push {
        int count_ = 0;

        static void push_one(const Entry &e) noexcept {
            std::visit([&](const auto &v) { PushFn(e.idx, v); }, e.val);
        }

    public:
        using entry = Entry;

        multi_push(const std::initializer_list<Entry> entries) noexcept : count_{static_cast<int>(entries.size())} {
            for (const auto &e: entries)
                push_one(e);
        }

        template<std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_value_t<R>, Entry>
        explicit multi_push(R &&entries) noexcept(noexcept(push_one(std::declval<const Entry &>()))) {
            for (const auto &e: std::forward<R>(entries)) {
                push_one(e);
                ++count_;
            }
        }

        ~multi_push() { PopFn(count_); }

        multi_push(const multi_push &)            = delete;
        multi_push &operator=(const multi_push &) = delete;
        multi_push(multi_push &&)                 = delete;
        multi_push &operator=(multi_push &&)      = delete;
    };

    using group_trait   = simple_trait<end_policy::always, &ImGui::BeginGroup, &ImGui::EndGroup>;
    using tooltip_trait = simple_trait<end_policy::always, &ImGui::BeginTooltip, &ImGui::EndTooltip>;

    struct tab_bar_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *str_id, const ImGuiTabBarFlags flags = 0) noexcept {
            return ImGui::BeginTabBar(str_id, flags);
        }
        static void end() noexcept { ImGui::EndTabBar(); }
    };
    using menu_trait     = simple_trait<end_policy::conditional, &ImGui::BeginMenu, &ImGui::EndMenu>;
    using list_box_trait = simple_trait<end_policy::conditional, &ImGui::BeginListBox, &ImGui::EndListBox>;

    using item_width_trait = simple_trait<end_policy::none, &ImGui::PushItemWidth, &ImGui::PopItemWidth>;

    struct window_trait {
        static constexpr auto policy = end_policy::always;
        using storage                = std::monostate;
        static bool begin(const char *name, bool *open = nullptr, const ImGuiWindowFlags flags = 0) noexcept {
            return ImGui::Begin(name, open, flags);
        }
        static void end() noexcept { ImGui::End(); }
    };

    struct child_trait {
        static constexpr auto policy = end_policy::always;
        using storage                = std::monostate;
        static bool begin(const char *id, const ImVec2 &size = ImVec2(0, 0), const ImGuiChildFlags child_flags = 0,
                          const ImGuiWindowFlags window_flags = 0) noexcept {
            return ImGui::BeginChild(id, size, child_flags, window_flags);
        }
        static bool begin(const ImGuiID id, const ImVec2 &size = ImVec2(0, 0), const ImGuiChildFlags child_flags = 0,
                          const ImGuiWindowFlags window_flags = 0) noexcept {
            return ImGui::BeginChild(id, size, child_flags, window_flags);
        }
        static void end() noexcept { ImGui::EndChild(); }
    };

    using disabled_trait      = simple_trait<end_policy::always, &ImGui::BeginDisabled, &ImGui::EndDisabled>;
    using menu_bar_trait      = simple_trait<end_policy::conditional, &ImGui::BeginMenuBar, &ImGui::EndMenuBar>;
    using main_menu_bar_trait = simple_trait<end_policy::conditional, &ImGui::BeginMainMenuBar, &ImGui::EndMainMenuBar>;
    using item_tooltip_trait  = simple_trait<end_policy::conditional, &ImGui::BeginItemTooltip, &ImGui::EndTooltip>;

    struct tab_item_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *label, bool *open = nullptr, const ImGuiTabItemFlags flags = 0) noexcept {
            return ImGui::BeginTabItem(label, open, flags);
        }
        static void end() noexcept { ImGui::EndTabItem(); }
    };

    struct combo_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *label, const char *preview_value, const ImGuiComboFlags flags = 0) noexcept {
            return ImGui::BeginCombo(label, preview_value, flags);
        }
        static void end() noexcept { ImGui::EndCombo(); }
    };

    struct popup_modal_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *name, bool *open = nullptr, const ImGuiWindowFlags flags = 0) noexcept {
            return ImGui::BeginPopupModal(name, open, flags);
        }
        static void end() noexcept { ImGui::EndPopup(); }
    };

    struct tree_node_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *label, const ImGuiTreeNodeFlags flags = 0) noexcept {
            return ImGui::TreeNodeEx(label, flags);
        }
        static void end() noexcept { ImGui::TreePop(); }
    };

    struct popup_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *id, const ImGuiPopupFlags flags = 0) noexcept {
            return ImGui::BeginPopup(id, flags);
        }
        static void end() noexcept { ImGui::EndPopup(); }
    };

    template<auto BeginFn>
    struct popup_context_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char           *str_id      = nullptr,
                          const ImGuiPopupFlags popup_flags = ImGuiPopupFlags_MouseButtonRight) noexcept {
            return BeginFn(str_id, popup_flags);
        }
        static void end() noexcept { ImGui::EndPopup(); }
    };

    using popup_context_item_trait   = popup_context_trait<&ImGui::BeginPopupContextItem>;
    using popup_context_window_trait = popup_context_trait<&ImGui::BeginPopupContextWindow>;
    using popup_context_void_trait   = popup_context_trait<&ImGui::BeginPopupContextVoid>;

    struct table_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *id, const int columns, const ImGuiTableFlags flags = 0,
                          const ImVec2 &outer_size = ImVec2(0, 0), const float inner_width = 0.0f) noexcept {
            return ImGui::BeginTable(id, columns, flags, outer_size, inner_width);
        }
        static void end() noexcept { ImGui::EndTable(); }
    };

    struct style_var_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin(const ImGuiStyleVar idx, const float val) noexcept { ImGui::PushStyleVar(idx, val); }
        static void begin(const ImGuiStyleVar idx, const ImVec2 &val) noexcept { ImGui::PushStyleVar(idx, val); }
        static void end() noexcept { ImGui::PopStyleVar(); }
    };

    struct style_color_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin(const ImGuiCol idx, const ImU32 col) noexcept { ImGui::PushStyleColor(idx, col); }
        static void begin(const ImGuiCol idx, const ImVec4 &col) noexcept { ImGui::PushStyleColor(idx, col); }
        static void end() noexcept { ImGui::PopStyleColor(); }
    };

    struct id_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin(const char *str_id) noexcept { ImGui::PushID(str_id); }
        static void begin(const int int_id) noexcept { ImGui::PushID(int_id); }
        static void begin(const void *ptr_id) noexcept { ImGui::PushID(ptr_id); }
        static void end() noexcept { ImGui::PopID(); }
    };

    struct indent_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = float;
        static float begin(const float width = 0.0f) noexcept {
            ImGui::Indent(width);
            return width;
        }
        static void end(const float storage) noexcept { ImGui::Unindent(storage); }
    };

    struct font_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin(ImFont *font) noexcept { ImGui::PushFont(font); }
        static void end() noexcept { ImGui::PopFont(); }
    };

    struct clip_rect_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin(const ImVec2 &min, const ImVec2 &max, const bool intersect = true) noexcept {
            ImGui::PushClipRect(min, max, intersect);
        }
        static void end() noexcept { ImGui::PopClipRect(); }
    };

    struct text_wrap_pos_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin(const float wrap_local_pos_x = 0.0f) noexcept { ImGui::PushTextWrapPos(wrap_local_pos_x); }
        static void end() noexcept { ImGui::PopTextWrapPos(); }
    };

    struct item_flag_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin(const ImGuiItemFlags option, const bool enabled) noexcept {
            ImGui::PushItemFlag(option, enabled);
        }
        static void end() noexcept { ImGui::PopItemFlag(); }
    };

    using button_repeat_trait = simple_trait<end_policy::none, &ImGui::PushButtonRepeat, &ImGui::PopButtonRepeat>;
    using tab_stop_trait      = simple_trait<end_policy::none, &ImGui::PushTabStop, &ImGui::PopTabStop>;
    using drag_drop_target_trait =
        simple_trait<end_policy::conditional, &ImGui::BeginDragDropTarget, &ImGui::EndDragDropTarget>;

    struct drag_drop_source_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const ImGuiDragDropFlags flags = 0) noexcept { return ImGui::BeginDragDropSource(flags); }
        static void end() noexcept { ImGui::EndDragDropSource(); }
    };

    struct style_var_entry {
        ImGuiStyleVar               idx;
        std::variant<float, ImVec2> val;
    };

    struct style_color_entry {
        ImGuiCol                    idx;
        std::variant<ImVec4, ImU32> val;

        style_color_entry(const ImGuiCol i, const ImVec4 &c) noexcept : idx{i}, val{c} {}
        style_color_entry(const ImGuiCol i, ImU32 c) noexcept : idx{i}, val{c} {}
    };

    constexpr auto push_style_var_fn   = [](ImGuiStyleVar idx, const auto &v) { ImGui::PushStyleVar(idx, v); };
    constexpr auto pop_style_var_fn    = [](const int count) { ImGui::PopStyleVar(count); };
    constexpr auto push_style_color_fn = [](ImGuiCol idx, const auto &v) { ImGui::PushStyleColor(idx, v); };
    constexpr auto pop_style_color_fn  = [](const int count) { ImGui::PopStyleColor(count); };

    /// @brief Push multiple style vars in one shot; pops all in destructor.
    using style_vars = multi_push<style_var_entry, push_style_var_fn, pop_style_var_fn>;
    /// @brief Push multiple style colors in one shot; pops all in destructor.
    using style_colors = multi_push<style_color_entry, push_style_color_fn, pop_style_color_fn>;

    using window               = raii_scope<window_trait>;
    using child                = raii_scope<child_trait>;
    using tab_bar              = raii_scope<tab_bar_trait>;
    using tab_item             = raii_scope<tab_item_trait>;
    using combo                = raii_scope<combo_trait>;
    using popup_modal          = raii_scope<popup_modal_trait>;
    using tree_node            = raii_scope<tree_node_trait>;
    using popup                = raii_scope<popup_trait>;
    using popup_context_item   = raii_scope<popup_context_item_trait>;
    using popup_context_window = raii_scope<popup_context_window_trait>;
    using popup_context_void   = raii_scope<popup_context_void_trait>;
    using menu                 = raii_scope<menu_trait>;
    using menu_bar             = raii_scope<menu_bar_trait>;
    using main_menu_bar        = raii_scope<main_menu_bar_trait>;
    using table                = raii_scope<table_trait>;
    using list_box             = raii_scope<list_box_trait>;
    using style_var            = raii_scope<style_var_trait>;
    using style_color          = raii_scope<style_color_trait>;
    using item_width           = raii_scope<item_width_trait>;
    using id                   = raii_scope<id_trait>;
    using indent               = raii_scope<indent_trait>;
    using disabled             = raii_scope<disabled_trait>;
    using group                = raii_scope<group_trait>;
    using tooltip              = raii_scope<tooltip_trait>;
    using item_tooltip         = raii_scope<item_tooltip_trait>;
    using font                 = raii_scope<font_trait>;
    using clip_rect            = raii_scope<clip_rect_trait>;
    using text_wrap_pos        = raii_scope<text_wrap_pos_trait>;
    using button_repeat        = raii_scope<button_repeat_trait>;
    using tab_stop             = raii_scope<tab_stop_trait>;
    using item_flag            = raii_scope<item_flag_trait>;
    using drag_drop_source     = raii_scope<drag_drop_source_trait>;
    using drag_drop_target     = raii_scope<drag_drop_target_trait>;

    /**
     * @brief RAII wrapper for ImGui::BeginMultiSelect / EndMultiSelect.
     *
     * Uses a dedicated class because both begin and end return ImGuiMultiSelectIO*,
     * which raii_scope cannot expose.
     */
    class [[nodiscard]] multi_select {
        ImGuiMultiSelectIO *begin_io_;
        bool                ended_ = false;

    public:
        explicit multi_select(const ImGuiMultiSelectFlags flags, const int selection_size = -1,
                              const int items_count = -1) noexcept :
            begin_io_(ImGui::BeginMultiSelect(flags, selection_size, items_count)) {}

        ~multi_select() noexcept {
            if (!ended_) ImGui::EndMultiSelect();
        }

        multi_select(const multi_select &)            = delete;
        multi_select &operator=(const multi_select &) = delete;
        multi_select(multi_select &&)                 = delete;
        multi_select &operator=(multi_select &&)      = delete;

        /// @brief Get the IO pointer returned by BeginMultiSelect().
        [[nodiscard]] ImGuiMultiSelectIO *begin_io() const noexcept { return begin_io_; }

        /**
         * @brief End the scope early and retrieve EndMultiSelect's IO*.
         *
         * After this call the destructor becomes a no-op.
         * @return The IO pointer from EndMultiSelect, or nullptr if already ended.
         */
        ImGuiMultiSelectIO *end() noexcept {
            if (ended_) return nullptr;
            ended_ = true;
            return ImGui::EndMultiSelect();
        }
    };

} // namespace imgui_util
