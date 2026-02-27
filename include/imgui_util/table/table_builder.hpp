/// @file table_builder.hpp
/// @brief Compile-time composable table builder with sorting and selection.
///
/// Each add_column() returns a new builder type (columns stored in a tuple).
/// Supports optional row filtering, multi-column sorting, and shift/ctrl selection.
///
/// Usage:
/// @code
///   auto table = imgui_util::table_builder<MyRow>()
///       .set_id("##items")
///       .set_flags(ImGuiTableFlags_Sortable)
///       .add_column("Name", 200.0f, [](const MyRow& r){ ImGui::Text("%s", r.name); })
///       .add_column("Value", imgui_util::column_stretch, [](const MyRow& r){ ImGui::Text("%d", r.val); });
///   table.render(rows);
/// @endcode
#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <imgui.h>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace imgui_util {

    inline constexpr float column_stretch = 0.0f; ///< @brief Pass as width to get a stretch column.

    /// @brief Row highlight callback type: returns a color for custom row background, or nullopt for default.
    template<typename RowT>
    using row_highlight_fn = std::move_only_function<std::optional<ImU32>(const RowT &)>;

    namespace detail {

        template<typename Fn>
        struct table_column {
            std::string_view      name;
            float                 width{}; // 0.0f = stretch, >0 = fixed pixel width
            ImGuiTableColumnFlags col_flags{};
            Fn                    fn;
        };

    } // namespace detail

    /// @brief Non-template configuration state shared across table_builder type morphs.
    struct table_config {
        std::string_view         id;
        ImGuiTableFlags          flags            = ImGuiTableFlags_None;
        int                      freeze_cols      = 0;
        int                      freeze_rows      = 0;
        std::unordered_set<int> *selection        = nullptr;
        int                      last_clicked_row = -1;
    };

    template<typename Fn, typename RowT>
    concept column_renderer =
        std::invocable<Fn, const RowT &> && std::is_void_v<std::invoke_result_t<Fn, const RowT &>>;

    template<typename Fn, typename RowT>
    concept row_predicate = std::predicate<Fn, const RowT &>;

    /**
     * @brief Compile-time composable table builder with sorting, filtering, and selection.
     *
     * Columns are accumulated in a tuple via add_column(), producing a new builder type each time.
     * Supports optional row filtering, multi-column sorting, and shift/ctrl multi-select.
     *
     * @tparam RowT      Row data type.
     * @tparam Cols      Tuple of table_column descriptors (grows with each add_column call).
     * @tparam FilterFn  Row predicate for filtering, or std::monostate if none.
     * @tparam RowIdFn   Function mapping a row to a unique int ID, or std::monostate to use index.
     */
    template<typename RowT, typename Cols = std::tuple<>, typename FilterFn = std::monostate,
             typename RowIdFn = std::monostate>
    class table_builder {
        template<typename, typename, typename, typename>
        friend class table_builder;

        static constexpr bool has_filter = !std::same_as<FilterFn, std::monostate>;
        static constexpr bool has_row_id = !std::same_as<RowIdFn, std::monostate>;

    public:
        using row_highlight_fn_t = row_highlight_fn<RowT>;
        table_builder()          = default;

        /// @brief Set the ImGui table string ID.
        table_builder set_id(const std::string_view id) && {
            cfg_.id = id;
            return std::move(*this);
        }
        /// @brief Set ImGuiTableFlags for the table.
        table_builder set_flags(const ImGuiTableFlags flags) && {
            cfg_.flags = flags;
            return std::move(*this);
        }
        /**
         * @brief Freeze columns/rows so they remain visible when scrolling.
         * @param cols  Number of columns to freeze from the left.
         * @param rows  Number of rows to freeze from the top.
         */
        table_builder set_scroll_freeze(const int cols, const int rows) && {
            cfg_.freeze_cols = cols;
            cfg_.freeze_rows = rows;
            return std::move(*this);
        }

        /**
         * @brief Append a column to the builder (returns a new builder type with the column added).
         * @param name       Column header label.
         * @param width      Pixel width (>0 = fixed, 0 = stretch via column_stretch).
         * @param fn         Callable invoked per-row to render the cell.
         * @param col_flags  Optional ImGuiTableColumnFlags.
         */
        template<typename Fn>
            requires column_renderer<Fn, RowT>
        [[nodiscard]] auto add_column(std::string_view name, float width, Fn &&fn,
                                      ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_None) && {
            auto col         = detail::table_column<std::decay_t<Fn>>{name, width, col_flags, std::forward<Fn>(fn)};
            auto new_cols    = std::tuple_cat(std::move(cols_), std::make_tuple(std::move(col)));
            using new_cols_t = decltype(new_cols);
            return table_builder<RowT, new_cols_t, FilterFn, RowIdFn>{
                cfg_, std::move(new_cols), std::move(filter_), std::move(row_id_fn_), std::move(row_highlight_fn_)};
        }

        /// @brief Set a function that maps each row to a unique int for selection tracking.
        template<std::invocable<const RowT &> Fn>
            requires std::convertible_to<std::invoke_result_t<Fn, const RowT &>, int>
        [[nodiscard]] auto set_row_id(Fn &&fn) && {
            return table_builder<RowT, Cols, FilterFn, std::decay_t<Fn>>{
                cfg_, std::move(cols_), std::move(filter_), std::forward<Fn>(fn), std::move(row_highlight_fn_)};
        }

        /// @brief Point to an external selection set; enables shift/ctrl multi-select.
        table_builder set_selection(std::unordered_set<int> *sel) && {
            cfg_.selection = sel;
            return std::move(*this);
        }

        /// @brief Set a row filter predicate (rows where fn returns false are hidden).
        template<typename Fn>
            requires row_predicate<Fn, RowT>
        [[nodiscard]] auto set_filter(Fn &&fn) && {
            return table_builder<RowT, Cols, std::decay_t<Fn>, RowIdFn>{
                cfg_, std::move(cols_), std::forward<Fn>(fn), std::move(row_id_fn_), std::move(row_highlight_fn_)};
        }

        /// @brief Set a per-row highlight callback for custom row background colors.
        table_builder set_row_highlight(row_highlight_fn_t fn) && {
            row_highlight_fn_ = std::move(fn);
            return std::move(*this);
        }

        /// @brief Mark filtered indices as stale (call when source data changes).
        void invalidate_filter() const noexcept { filter_dirty_ = true; }

        /// @brief Set a callback to render when the table has no data.
        table_builder set_empty_state(std::move_only_function<void()> fn) && {
            empty_state_fn_ = std::move(fn);
            return std::move(*this);
        }

        /// @brief Set a callback invoked when a row is double-clicked.
        table_builder set_row_activate(std::move_only_function<void(const RowT &, int index)> fn) && {
            row_activate_fn_ = std::move(fn);
            return std::move(*this);
        }

        /**
         * @brief Open the table. Call before render_clipped().
         * @param height  Table height in pixels (0 = auto).
         * @return false if the table is clipped away and should be skipped.
         */
        [[nodiscard]] bool begin(const float height = 0.0f) {
            constexpr int ncols = std::tuple_size_v<Cols>;
            if (!ImGui::BeginTable(cfg_.id.data(), ncols, cfg_.flags, ImVec2(0, height))) {
                return false;
            }
            ImGui::TableSetupScrollFreeze(cfg_.freeze_cols, cfg_.freeze_rows);
            setup_columns(std::make_index_sequence<ncols>{});
            ImGui::TableHeadersRow();
            sort_specs_ = ImGui::TableGetSortSpecs();
            return true;
        }

        /// @brief Return the current sort specs (valid after begin()).
        [[nodiscard]] ImGuiTableSortSpecs *get_sort_specs() const noexcept { return sort_specs_; }

        /**
         * @brief Sort data by a single key extractor (uses the first sort spec only).
         * @param data    Mutable span of rows to sort in-place.
         * @param key_fn  Callable returning a totally-ordered key for each row.
         * @param force   Sort even if specs are not dirty.
         */
        template<typename KeyFn>
            requires std::invocable<KeyFn, const RowT &>
            && std::totally_ordered<std::invoke_result_t<KeyFn, const RowT &>>
        void sort_if_dirty(std::span<RowT> data, KeyFn key_fn, const bool force = false) {
            if (sort_specs_ && (sort_specs_->SpecsDirty || force)) {
                if (sort_specs_->SpecsCount > 0) {
                    if (sort_specs_->Specs[0].SortDirection == ImGuiSortDirection_Ascending)
                        std::ranges::stable_sort(data, std::less{}, key_fn);
                    else
                        std::ranges::stable_sort(data, std::greater{}, key_fn);
                }
                sort_specs_->SpecsDirty = false;
            }
        }

        /// @brief Less-than comparator per column for multi-column sorting.
        using comparator_fn = std::move_only_function<bool(const RowT &, const RowT &)>;

        /**
         * @brief Multi-column sort: one comparator per column index, stable_sort in reverse spec order.
         * @param data         Mutable span of rows to sort in-place.
         * @param comparators  Span of comparators, one per column index.
         * @param force        Sort even if specs are not dirty.
         */
        void sort_if_dirty(std::span<RowT> data, std::span<comparator_fn> comparators, const bool force = false) {
            if (sort_specs_ && (sort_specs_->SpecsDirty || force)) {
                for (int s = sort_specs_->SpecsCount - 1; s >= 0; --s) {
                    const auto &spec = sort_specs_->Specs[s];
                    const auto  col  = static_cast<std::size_t>(spec.ColumnIndex);
                    if (col >= comparators.size() || !comparators[col]) continue;

                    const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
                    auto      &cmp       = comparators[col];
                    std::ranges::stable_sort(
                        data, [&](const RowT &a, const RowT &b) { return ascending ? cmp(a, b) : cmp(b, a); });
                }
                sort_specs_->SpecsDirty = false;
            }
        }

        /**
         * @brief Sort by a single comparator, respecting the current sort direction.
         * @param data  Mutable span of rows to sort in-place.
         * @param comp  Less-than comparator for the rows.
         * @param force Force sort even if specs are not dirty.
         */
        void sort_if_dirty(std::span<RowT> data, comparator_fn comp, const bool force = false) {
            if (sort_specs_ && (sort_specs_->SpecsDirty || force)) {
                if (sort_specs_->SpecsCount > 0) {
                    const bool ascending = sort_specs_->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
                    std::ranges::stable_sort(
                        data, [&](const RowT &a, const RowT &b) { return ascending ? comp(a, b) : comp(b, a); });
                }
                sort_specs_->SpecsDirty = false;
            }
        }

        /**
         * @brief Multi-column sort using key extractors: one callable per column returning a totally-ordered key.
         *
         * Example:
         * @code
         *   table.sort_if_dirty_by_key(data,
         *       [](const Row& r) { return r.name; },
         *       [](const Row& r) { return r.age; },
         *       [](const Row& r) { return r.score; });
         * @endcode
         */
        template<typename... KeyFns>
            requires (sizeof...(KeyFns) == std::tuple_size_v<Cols>)
                  && (std::invocable<KeyFns, const RowT &> && ...)
                  && (std::totally_ordered<std::invoke_result_t<KeyFns, const RowT &>> && ...)
        void sort_if_dirty_by_key(std::span<RowT> data, KeyFns &&...key_fns, const bool force = false) {
            if (!sort_specs_ || (!sort_specs_->SpecsDirty && !force)) return;
            auto keys = std::forward_as_tuple(std::forward<KeyFns>(key_fns)...);
            for (int s = sort_specs_->SpecsCount - 1; s >= 0; --s) {
                const auto &spec      = sort_specs_->Specs[s];
                const bool  ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
                sort_by_column_key(data, keys, static_cast<std::size_t>(spec.ColumnIndex), ascending,
                                   std::make_index_sequence<sizeof...(KeyFns)>{});
            }
            sort_specs_->SpecsDirty = false;
        }

        /// @brief Render a single row (no clipper, no selection).
        void render_single_row(const RowT &data) {
            ImGui::TableNextRow();
            render_columns(data, std::make_index_sequence<std::tuple_size_v<Cols>>{});
        }

        template<std::ranges::sized_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, const RowT &>
        void render_clipped(const R &data) {
            if constexpr (has_row_id) {
                if constexpr (std::ranges::random_access_range<R>) {
                    index_to_id_ = [this, &data](const int idx) {
                        return row_id_for(*(std::ranges::begin(data) + idx), idx);
                    };
                } else {
                    index_to_id_ = [this, &data](const int idx) {
                        auto it = std::ranges::begin(data);
                        std::ranges::advance(it, idx);
                        return row_id_for(*it, idx);
                    };
                }
            } else {
                index_to_id_ = nullptr;
            }
            if constexpr (has_filter) {
                rebuild_filter_range(data);
                if constexpr (std::ranges::random_access_range<R>) {
                    clip_and_render(static_cast<int>(filtered_indices_.size()), [&](const int fi) {
                        const int   i   = filtered_indices_[fi];
                        const auto &row = *(std::ranges::begin(data) + i);
                        ImGui::PushID(i);
                        render_row_with_selection(row, i);
                        ImGui::PopID();
                    });
                } else {
                    const int        total = static_cast<int>(filtered_indices_.size());
                    ImGuiListClipper clipper;
                    clipper.Begin(total);
                    while (clipper.Step()) {
                        const int disp_end = std::min(clipper.DisplayEnd, total);
                        auto      it       = std::ranges::begin(data);
                        int       cur_data = 0;
                        for (int fi = clipper.DisplayStart; fi < disp_end; ++fi) {
                            const int i = filtered_indices_[fi];
                            std::ranges::advance(it, i - cur_data);
                            cur_data = i;
                            ImGui::PushID(i);
                            render_row_with_selection(*it, i);
                            ImGui::PopID();
                        }
                    }
                }
            } else {
                const auto count = static_cast<int>(std::ranges::size(data));
                if constexpr (std::ranges::random_access_range<R>) {
                    clip_and_render(count, [&](int i) {
                        const auto &row = *(std::ranges::begin(data) + i);
                        ImGui::PushID(i);
                        render_row_with_selection(row, i);
                        ImGui::PopID();
                    });
                } else {
                    // Non-random-access: advance iterator sequentially per clipper step
                    ImGuiListClipper clipper;
                    clipper.Begin(count);
                    while (clipper.Step()) {
                        const int end = std::min(clipper.DisplayEnd, count);
                        auto      it  = std::ranges::begin(data);
                        std::ranges::advance(it, clipper.DisplayStart);
                        for (int i = clipper.DisplayStart; i < end; ++i, ++it) {
                            ImGui::PushID(i);
                            render_row_with_selection(*it, i);
                            ImGui::PopID();
                        }
                    }
                }
            }
        }

        /**
         * @brief Convenience: begin + render_clipped + end in one call.
         * @param data    Rows to render.
         * @param height  Table height in pixels (0 = auto).
         */
        template<std::ranges::sized_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, const RowT &>
        void render(R &&data, const float height = 0.0f) {
            if (begin(height)) {
                if (std::ranges::empty(data)) {
                    render_empty_state();
                } else {
                    render_clipped(std::forward<R>(data));
                }
                end();
            }
        }

        /// @brief End the table (call after render_clipped).
        static void end() { ImGui::EndTable(); }

        /// @brief Show or hide a column by index.
        static void set_column_visible(const int col_index, const bool visible) {
            ImGui::TableSetColumnEnabled(col_index, visible);
        }

        /// @brief Render a right-click context menu with checkboxes for toggling column visibility.
        void render_column_visibility_menu() {
            if (!ImGui::BeginPopupContextItem("##col_vis")) return;
            render_column_visibility_items(std::make_index_sequence<std::tuple_size_v<Cols>>{});
            ImGui::EndPopup();
        }

        /// @brief Clear all selected rows.
        void clear_selection() const noexcept {
            if (cfg_.selection) cfg_.selection->clear();
        }

        /// @brief Return true if the row with the given ID is currently selected.
        [[nodiscard]] bool is_selected(const int row_id) const noexcept {
            return cfg_.selection && cfg_.selection->contains(row_id);
        }

        /// @brief Return the number of currently selected rows.
        [[nodiscard]] std::size_t selected_count() const noexcept {
            return cfg_.selection ? cfg_.selection->size() : 0;
        }

        /// @brief Return the number of columns.
        [[nodiscard]] static constexpr int column_count() noexcept { return std::tuple_size_v<Cols>; }

        /// @brief Return the number of rows passing the filter (only meaningful after render_clipped).
        [[nodiscard]] std::size_t filtered_count() const noexcept
            requires has_filter
        {
            return filtered_indices_.size();
        }

        /// @brief Invoke fn(id) for each selected row ID.
        template<std::invocable<int> Fn>
        void for_each_selected(const Fn &fn) const {
            if (cfg_.selection)
                for (const int id : *cfg_.selection)
                    fn(id);
        }

        /// @brief Return a vector of all currently selected row IDs.
        [[nodiscard]] std::vector<int> get_selected_ids() const {
            if (!cfg_.selection) return {};
            return {cfg_.selection->begin(), cfg_.selection->end()};
        }

        /// @brief Select all rows in data.
        template<std::ranges::sized_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, const RowT &>
        void select_all(const R &data) {
            if (!cfg_.selection) return;
            int i = 0;
            for (const auto &row: data)
                cfg_.selection->insert(row_id_for(row, i++));
        }

    private:
        template<typename NewCols>
        table_builder(table_config cfg, NewCols &&cols, FilterFn filter, RowIdFn row_id,
                      row_highlight_fn<RowT> highlight = {}) :
            cfg_(std::move(cfg)),
            cols_(std::forward<NewCols>(cols)),
            filter_(std::move(filter)),
            row_id_fn_(std::move(row_id)),
            row_highlight_fn_(std::move(highlight)) {}

        table_config         cfg_;
        Cols                 cols_{};
        ImGuiTableSortSpecs *sort_specs_ = nullptr;

        [[no_unique_address]] FilterFn filter_{};
        [[no_unique_address]] RowIdFn  row_id_fn_{};

        mutable std::vector<int>                               filtered_indices_;
        mutable bool                                           filter_dirty_ = true;
        row_highlight_fn_t                                     row_highlight_fn_;
        std::move_only_function<void()>                        empty_state_fn_;
        std::move_only_function<void(const RowT &, int index)> row_activate_fn_;
        std::move_only_function<int(int)>                      index_to_id_;

        void render_empty_state() {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (empty_state_fn_)
                empty_state_fn_();
            else
                ImGui::TextDisabled("No data");
        }

        int row_id_for(const RowT &row, const int index) const {
            if constexpr (has_row_id)
                return static_cast<int>(row_id_fn_(row));
            else
                return index;
        }

        template<typename PerRow>
        static void clip_and_render(const int count, PerRow per_row) {
            ImGuiListClipper clipper;
            clipper.Begin(count);
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < std::min(clipper.DisplayEnd, count); ++i) {
                    per_row(i);
                }
            }
        }

        template<std::ranges::sized_range R>
        void rebuild_filter_range(const R &data) const {
            if (!filter_dirty_) return;
            filtered_indices_.clear();
            const auto count = static_cast<int>(std::ranges::size(data));
            filtered_indices_.reserve(count);
            int idx = 0;
            for (auto it = std::ranges::begin(data); idx < count; ++it, ++idx) {
                if (filter_(*it)) filtered_indices_.push_back(idx);
            }
            filter_dirty_ = false;
        }

        void apply_row_highlight(const RowT &row) {
            if (row_highlight_fn_) {
                if (auto color = row_highlight_fn_(row)) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, *color);
                }
            }
        }

        void check_row_activate(const RowT &data, const int index) {
            if (row_activate_fn_ && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                row_activate_fn_(data, index);
            }
        }

        void handle_selection_click(const int row_index, const int row_id, const bool new_selected) {
            if (const auto &io = ImGui::GetIO(); io.KeyShift && cfg_.last_clicked_row >= 0) {
                const int lo = std::min(cfg_.last_clicked_row, row_index);
                const int hi = std::max(cfg_.last_clicked_row, row_index);
                for (int r = lo; r <= hi; ++r)
                    cfg_.selection->insert(index_to_id_ ? index_to_id_(r) : r);
            } else if (io.KeyCtrl) {
                if (new_selected)
                    cfg_.selection->insert(row_id);
                else
                    cfg_.selection->erase(row_id);
            } else {
                cfg_.selection->clear();
                cfg_.selection->insert(row_id);
            }
            cfg_.last_clicked_row = row_index;
        }

        void render_row_with_selection(const RowT &data, const int index) {
            const int row_id = row_id_for(data, index);
            ImGui::TableNextRow();
            apply_row_highlight(data);

            if (cfg_.selection) {
                ImGui::TableSetColumnIndex(0);
                const bool was_selected = cfg_.selection->contains(row_id);
                bool       new_selected = was_selected;
                ImGui::Selectable("##sel", &new_selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
                check_row_activate(data, index);
                if (new_selected != was_selected) {
                    handle_selection_click(index, row_id, new_selected);
                }
                ImGui::SameLine();
            }

            render_columns(data, std::make_index_sequence<std::tuple_size_v<Cols>>{});

            if (!cfg_.selection) {
                check_row_activate(data, index);
            }
        }

        template<typename KeyTuple, std::size_t... Is>
        static void sort_by_column_key(std::span<RowT> data, KeyTuple &keys, const std::size_t col,
                                       const bool ascending, std::index_sequence<Is...> /*seq*/) {
            ((Is == col ? (std::ranges::stable_sort(data,
                                                    [&](const RowT &a, const RowT &b) {
                 const auto &key_fn = std::get<Is>(keys);
                 return ascending ? std::less{}(key_fn(a), key_fn(b))
                                  : std::greater{}(key_fn(a), key_fn(b));
             }),
                          true)
                        : false)
             || ...);
        }

        template<size_t... Is>
        void setup_columns(std::index_sequence<Is...> /*seq*/) {
            (([]<typename Col>(const Col &col) {
                auto           flags = col.col_flags;
                constexpr auto mask  = ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_WidthStretch;
                if (!(flags & mask)) {
                    flags |= col.width == column_stretch ? ImGuiTableColumnFlags_WidthStretch
                                                         : ImGuiTableColumnFlags_WidthFixed;
                }
                ImGui::TableSetupColumn(col.name.data(), flags, col.width);
            }(std::get<Is>(cols_))),
             ...);
        }

        template<size_t... Is>
        void render_columns(const RowT &data, std::index_sequence<Is...> /*seq*/) {
            ((ImGui::TableSetColumnIndex(static_cast<int>(Is)), std::get<Is>(cols_).fn(data)), ...);
        }

        template<std::size_t... Is>
        void render_column_visibility_items(std::index_sequence<Is...> /*seq*/) {
            (([this] {
                const auto &col     = std::get<Is>(cols_);
                bool        enabled = ImGui::TableGetColumnFlags(static_cast<int>(Is))
                                   & ImGuiTableColumnFlags_IsEnabled;
                if (ImGui::Checkbox(col.name.data(), &enabled))
                    ImGui::TableSetColumnEnabled(static_cast<int>(Is), enabled);
            }()),
             ...);
        }
    };

} // namespace imgui_util
