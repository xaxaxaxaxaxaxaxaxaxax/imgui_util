// table_builder.hpp - compile-time composable table builder with sorting and selection
//
// Usage:
//   auto table = imgui_util::table_builder<MyRow>()
//       .set_id("##items")
//       .set_flags(ImGuiTableFlags_Sortable)
//       .add_column("Name", 200.0f, [](const MyRow& r){ ImGui::Text("%s", r.name); })
//       .add_column("Value", imgui_util::column_stretch, [](const MyRow& r){ ImGui::Text("%d", r.val); });
//   table.render(rows);
//
// Each add_column() returns a new builder type (columns stored in a tuple).
// Supports optional row filtering, multi-column sorting, and shift/ctrl selection.
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

    inline constexpr float column_stretch = 0.0f; // pass as width to get a stretch column

    // Row highlight callback type: returns a color for custom row background, or nullopt for default
    template<typename RowT>
    using row_highlight_fn = std::move_only_function<std::optional<ImU32>(const RowT &)>;

    namespace detail {

        template<typename Fn>
        struct table_column {
            std::string_view      name;
            float                 width{}; // 0.0f = stretch, >0 = fixed pixel width
            ImGuiTableColumnFlags col_flags{};
            Fn                    fn; // void(const RowT&) renderer for this column
        };

    } // namespace detail

    // Fn must be callable with (const RowT&) and return void
    template<typename Fn, typename RowT>
    concept column_renderer =
        std::invocable<Fn, const RowT &> && std::is_void_v<std::invoke_result_t<Fn, const RowT &>>;

    // Fn must be a predicate on const RowT&
    template<typename Fn, typename RowT>
    concept row_predicate = std::predicate<Fn, const RowT &>;

    // RowT = your data row type, Cols = tuple of table_column<Fn> (built by add_column)
    // FilterFn = optional filter predicate type (std::monostate = no filter)
    // RowIdFn = optional row-id function type (std::monostate = use index)
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

        // All setters use && to enforce move-chain style (builder pattern)
        table_builder set_id(const std::string_view id) && {
            id_ = id;
            return std::move(*this);
        }
        table_builder set_flags(const ImGuiTableFlags flags) && {
            flags_ = flags;
            return std::move(*this);
        }
        table_builder set_scroll_freeze(const int cols, const int rows) && {
            freeze_cols_ = cols;
            freeze_rows_ = rows;
            return std::move(*this);
        }

        // Returns a NEW builder type with the column appended to the tuple
        template<typename Fn>
            requires column_renderer<Fn, RowT>
        [[nodiscard]] auto add_column(std::string_view name, float width, Fn &&fn,
                                      ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_None) && {
            auto col         = detail::table_column<std::decay_t<Fn>>{name, width, col_flags, std::forward<Fn>(fn)};
            auto new_cols    = std::tuple_cat(std::move(cols_), std::make_tuple(std::move(col)));
            using new_cols_t = decltype(new_cols);
            return table_builder<RowT, new_cols_t, FilterFn, RowIdFn>{id_,
                                                                      flags_,
                                                                      freeze_cols_,
                                                                      freeze_rows_,
                                                                      std::move(new_cols),
                                                                      std::move(filter_),
                                                                      selection_,
                                                                      std::move(row_id_fn_),
                                                                      last_clicked_row_,
                                                                      std::move(row_highlight_fn_)};
        }

        // Row ID function: maps row -> unique int for selection tracking
        template<std::invocable<const RowT &> Fn>
            requires std::convertible_to<std::invoke_result_t<Fn, const RowT &>, int>
        [[nodiscard]] auto set_row_id(Fn &&fn) && {
            return table_builder<RowT, Cols, FilterFn, std::decay_t<Fn>>{id_,
                                                                         flags_,
                                                                         freeze_cols_,
                                                                         freeze_rows_,
                                                                         std::move(cols_),
                                                                         std::move(filter_),
                                                                         selection_,
                                                                         std::forward<Fn>(fn),
                                                                         last_clicked_row_,
                                                                         std::move(row_highlight_fn_)};
        }

        // Pointer to external selection set; enables shift/ctrl multi-select
        table_builder set_selection(std::unordered_set<int> *sel) && {
            selection_ = sel;
            return std::move(*this);
        }

        template<typename Fn>
            requires row_predicate<Fn, RowT>
        [[nodiscard]] auto set_filter(Fn &&fn) && {
            auto result          = table_builder<RowT, Cols, std::decay_t<Fn>, RowIdFn>{id_,
                                                                                        flags_,
                                                                                        freeze_cols_,
                                                                                        freeze_rows_,
                                                                                        std::move(cols_),
                                                                                        std::forward<Fn>(fn),
                                                                                        selection_,
                                                                                        std::move(row_id_fn_),
                                                                                        last_clicked_row_,
                                                                                        std::move(row_highlight_fn_)};
            result.filter_dirty_ = true;
            return result;
        }

        // Optional per-row background highlight; return std::nullopt for default
        table_builder set_row_highlight(row_highlight_fn_t fn) && {
            row_highlight_fn_ = std::move(fn);
            return std::move(*this);
        }

        // Mark filtered indices as stale (call when source data changes)
        void invalidate_filter() const { filter_dirty_ = true; }

        // Custom empty-state renderer (replaces default "No data" message)
        table_builder set_empty_state(std::move_only_function<void()> fn) && {
            empty_state_fn_ = std::move(fn);
            return std::move(*this);
        }

        // Callback invoked on double-click of a row
        table_builder set_row_activate(std::move_only_function<void(const RowT &)> fn) && {
            row_activate_fn_ = std::move(fn);
            return std::move(*this);
        }

        // Call begin() before render_clipped(); returns false if table is clipped away
        [[nodiscard]] bool begin(const float height = 0.0f) {
            constexpr int ncols = std::tuple_size_v<Cols>;
            if (!ImGui::BeginTable(id_.data(), ncols, flags_, ImVec2(0, height))) {
                return false;
            }
            ImGui::TableSetupScrollFreeze(freeze_cols_, freeze_rows_);
            setup_columns(std::make_index_sequence<ncols>{});
            ImGui::TableHeadersRow();
            sort_specs_ = ImGui::TableGetSortSpecs();
            return true;
        }

        [[nodiscard]] ImGuiTableSortSpecs *get_sort_specs() const { return sort_specs_; }

        // Sort data by a single key extractor (uses first sort spec only)
        template<typename KeyFn>
            requires std::invocable<KeyFn, const RowT &>
            && std::totally_ordered<std::invoke_result_t<KeyFn, const RowT &>>
        void sort_if_dirty(std::span<RowT> data, KeyFn key_fn, const bool force = false) {
            if (sort_specs_ && (sort_specs_->SpecsDirty || force)) {
                if (sort_specs_->SpecsCount > 0) {
                    const bool ascending = sort_specs_->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
                    std::ranges::stable_sort(data, [&](const RowT &a, const RowT &b) {
                        return ascending ? std::invoke(key_fn, a) < std::invoke(key_fn, b)
                                         : std::invoke(key_fn, b) < std::invoke(key_fn, a);
                    });
                }
                sort_specs_->SpecsDirty = false;
            }
        }

        using comparator_fn =
            std::move_only_function<bool(const RowT &, const RowT &)>; // less-than comparator per column

        // Multi-column sort: one comparator per column index, stable_sort in reverse spec order
        void sort_if_dirty(std::span<RowT> data, std::span<comparator_fn> comparators, const bool force = false) {
            if ((sort_specs_ != nullptr) && (sort_specs_->SpecsDirty || force)) {
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

        void render_single_row(const RowT &data) {
            ImGui::TableNextRow();
            render_columns(data, std::make_index_sequence<std::tuple_size_v<Cols>>{});
        }

        // Render rows using ImGuiListClipper for virtualized scrolling
        void render_clipped(std::span<const RowT> data) {
            if constexpr (has_filter) {
                rebuild_filter_span(data);
                clip_and_render(static_cast<int>(filtered_indices_.size()), [&](const int fi) {
                    const int i   = filtered_indices_[fi];
                    const int rid = row_id_for(data[i], i);
                    ImGui::PushID(rid);
                    render_row_with_selection(data[i], rid);
                    ImGui::PopID();
                });
            } else {
                clip_and_render(static_cast<int>(data.size()), [&](int i) {
                    const int rid = row_id_for(data[i], i);
                    ImGui::PushID(rid);
                    render_row_with_selection(data[i], rid);
                    ImGui::PopID();
                });
            }
        }

        template<std::ranges::sized_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, const RowT &>
        void render_clipped(R &&data) { // NOLINT(cppcoreguidelines-missing-std-forward)
            if constexpr (has_filter) {
                rebuild_filter_range(data);
                clip_and_render(static_cast<int>(filtered_indices_.size()), [&](const int fi) {
                    const int i = filtered_indices_[fi];
                    if constexpr (std::ranges::random_access_range<R>) {
                        const auto &row = *(std::ranges::begin(data) + i);
                        ImGui::PushID(i);
                        render_row_with_selection(row, i);
                        ImGui::PopID();
                    } else {
                        auto it = std::ranges::begin(data);
                        std::ranges::advance(it, i);
                        ImGui::PushID(i);
                        render_row_with_selection(*it, i);
                        ImGui::PopID();
                    }
                });
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

        // Convenience: begin + render_clipped + end in one call
        void render(std::span<const RowT> data, const float height = 0.0f) {
            if (begin(height)) {
                if (data.empty()) {
                    render_empty_state();
                } else {
                    render_clipped(data);
                }
                end();
            }
        }

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

        static void end() { ImGui::EndTable(); }

        static void set_column_visible(const int col_index, const bool visible) {
            ImGui::TableSetColumnEnabled(col_index, visible);
        }

    private:
        template<typename NewCols>
        table_builder(const std::string_view id, const ImGuiTableFlags flags, const int fc, const int fr,
                      NewCols &&cols, FilterFn filter, std::unordered_set<int> *sel, RowIdFn row_id,
                      const int last_clicked, row_highlight_fn<RowT> highlight = {}) :
            id_(id),
            flags_(flags),
            freeze_cols_(fc),
            freeze_rows_(fr),
            cols_(std::forward<NewCols>(cols)),
            selection_(sel),
            filter_(std::move(filter)),
            row_id_fn_(std::move(row_id)),
            last_clicked_row_(last_clicked),
            row_highlight_fn_(std::move(highlight)) {}

        std::string_view         id_;
        ImGuiTableFlags          flags_       = ImGuiTableFlags_None;
        int                      freeze_cols_ = 0; // TableSetupScrollFreeze columns
        int                      freeze_rows_ = 0; // TableSetupScrollFreeze rows
        Cols                     cols_{};          // tuple of table_column<Fn>
        ImGuiTableSortSpecs     *sort_specs_ = nullptr;
        std::unordered_set<int> *selection_  = nullptr; // external selection state

        [[no_unique_address]] FilterFn filter_{};    // optional row visibility predicate
        [[no_unique_address]] RowIdFn  row_id_fn_{}; // optional: maps row -> unique id

        int                                         last_clicked_row_ = -1; // for shift-select range
        mutable std::vector<int>                    filtered_indices_;      // cached filtered row indices
        mutable bool                                filter_dirty_ = true;   // rebuild filtered_indices_ when true
        row_highlight_fn_t                          row_highlight_fn_;      // optional per-row highlight
        std::move_only_function<void()>             empty_state_fn_;        // custom empty-state renderer
        std::move_only_function<void(const RowT &)> row_activate_fn_;       // double-click/Enter callback

        void render_empty_state() {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (empty_state_fn_)
                empty_state_fn_();
            else
                ImGui::TextDisabled("No data");
        }

        // Compute row id: uses row_id_fn_ if available, otherwise falls back to index
        int row_id_for(const RowT &row, const int index) const {
            if constexpr (has_row_id)
                return static_cast<int>(row_id_fn_(row));
            else
                return index;
        }

        // Clipper helper: runs ImGuiListClipper and calls per_row(visible_index) for each visible row
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

        // Rebuild filtered indices from a span
        void rebuild_filter_span(std::span<const RowT> data) const {
            if (!filter_dirty_) return;
            filtered_indices_.clear();
            filtered_indices_.reserve(data.size());
            for (int i = 0; i < static_cast<int>(data.size()); ++i) {
                if (filter_(data[i])) filtered_indices_.push_back(i);
            }
            filter_dirty_ = false;
        }

        // Rebuild filtered indices from a sized range
        template<std::ranges::sized_range R>
        void rebuild_filter_range(R &&data) const { // NOLINT(cppcoreguidelines-missing-std-forward)
            if (!filter_dirty_) return;
            filtered_indices_.clear();
            const auto count = static_cast<int>(std::ranges::size(data));
            filtered_indices_.reserve(count);
            if constexpr (std::ranges::random_access_range<R>) {
                auto it = std::ranges::begin(data);
                for (int i = 0; i < count; ++i) {
                    if (filter_(*(it + i))) filtered_indices_.push_back(i);
                }
            } else {
                int idx = 0;
                for (auto it = std::ranges::begin(data); idx < count; ++it, ++idx) {
                    if (filter_(*it)) filtered_indices_.push_back(idx);
                }
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

        void check_row_activate(const RowT &data) {
            if (row_activate_fn_ && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                row_activate_fn_(data);
            }
        }

        void render_row_with_selection(const RowT &data, const int rid) {
            ImGui::TableNextRow();
            apply_row_highlight(data);

            if (selection_ != nullptr) {
                ImGui::TableSetColumnIndex(0);
                const bool was_selected = selection_->contains(rid);
                bool       is_selected  = was_selected;
                ImGui::Selectable("##sel", &is_selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
                check_row_activate(data);
                if (is_selected != was_selected) {
                    if (const auto &io = ImGui::GetIO(); io.KeyShift && last_clicked_row_ >= 0) {
                        const int lo = std::min(last_clicked_row_, rid);
                        const int hi = std::max(last_clicked_row_, rid);
                        for (int r = lo; r <= hi; ++r)
                            selection_->insert(r);
                    } else if (io.KeyCtrl) {
                        if (is_selected)
                            selection_->insert(rid);
                        else
                            selection_->erase(rid);
                    } else {
                        selection_->clear();
                        selection_->insert(rid);
                    }
                    last_clicked_row_ = rid;
                }
                ImGui::SameLine();
            }

            render_columns(data, std::make_index_sequence<std::tuple_size_v<Cols>>{});

            if (selection_ == nullptr) {
                check_row_activate(data);
            }
        }

        template<size_t... Is>
        void setup_columns(std::index_sequence<Is...> /*seq*/) {
            (([]<typename Col>(const Col &col) {
                auto           flags = col.col_flags;
                constexpr auto mask  = ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_WidthStretch;
                if (!(flags & mask)) {
                    flags |= (col.width == column_stretch) ? ImGuiTableColumnFlags_WidthStretch
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
    };

} // namespace imgui_util
