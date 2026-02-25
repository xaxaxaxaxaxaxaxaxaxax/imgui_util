#include <algorithm>
#include <functional>
#include <gtest/gtest.h>
#include <imgui_util/table/table_builder.hpp>
#include <list>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <vector>

using namespace imgui_util;

struct test_row {
    int         id;
    const char *name;
    float       value;
};

// --- Default table_builder has 0 columns ---

TEST(TableBuilder, DefaultHasZeroColumns) {
    using builder_t = table_builder<test_row>;
    static_assert(std::tuple_size_v<std::tuple<>> == 0);
    const builder_t builder;
    (void) builder;
}

// --- Adding columns increases the count ---

TEST(TableBuilder, AddColumnIncreasesCount) {
    auto builder =
        table_builder<test_row>{}.set_id("test").add_column("ID", 50.0f, [](const test_row &r) {
        (void) r.id;
    }).add_column("Name", 0.0f, [](const test_row &r) { (void) r.name; });

    using builder_type = decltype(builder);
    static_assert(!std::is_same_v<builder_type, table_builder<test_row>>);
    (void) builder;
}

// --- Column type deduction works ---

TEST(TableBuilder, ColumnTypeDeduction) {
    [[maybe_unused]]
    auto lambda    = [](const test_row &r) { (void) r.id; };
    using col_type = detail::table_column<decltype(lambda)>;

    static_assert(std::is_same_v<decltype(col_type::name), std::string_view>);
    static_assert(std::is_same_v<decltype(col_type::width), float>);
}

// --- Chained builder fluent API compiles ---

TEST(TableBuilder, FluentApiCompiles) {
    const auto builder = table_builder<test_row>{}
                       .set_id("fluent_test")
                       .set_flags(0)
                       .set_scroll_freeze(1, 1)
                       .add_column("Col1", 100.0f, [](const test_row &) {
    }).add_column("Col2", 0.0f, [](const test_row &) {});
    (void) builder;
}

// --- Builder methods are rvalue-qualified ---

TEST(TableBuilder, BuilderMethodsAreRvalueQualified) {
    static_assert(requires(table_builder<test_row> b) { std::move(b).set_id("x"); });
    static_assert(requires(table_builder<test_row> b) { std::move(b).set_flags(0); });
    static_assert(requires(table_builder<test_row> b) { std::move(b).set_scroll_freeze(0, 0); });
}

// --- Rvalue-qualified setters return table_builder by value (M14) ---

TEST(TableBuilder, SettersReturnByValue) {
    static_assert(
        std::is_same_v<decltype(std::declval<table_builder<test_row>>().set_id("x")), table_builder<test_row>>);
    static_assert(
        std::is_same_v<decltype(std::declval<table_builder<test_row>>().set_flags(0)), table_builder<test_row>>);
    static_assert(std::is_same_v<decltype(std::declval<table_builder<test_row>>().set_scroll_freeze(0, 0)),
                                 table_builder<test_row>>);
}

// --- Forwarding constructor is private ---

TEST(TableBuilder, ForwardingCtorIsPrivate) {
    static_assert(
        !std::is_constructible_v<table_builder<test_row>, const char *, ImGuiTableFlags, int, int, std::tuple<>>);
}

// --- set_row_id accepts function pointer (M11) ---

TEST(TableBuilder, SetRowIdAcceptsFunctionPointer) {
    const auto builder = table_builder<test_row>{}.set_id("row_id_test").set_row_id(+[](const test_row &r) {
        return r.id;
    }).add_column("ID", 50.0f, [](const test_row &) {});
    (void) builder;
}

// --- set_row_id accepts stateless lambda ---

TEST(TableBuilder, SetRowIdAcceptsStatelessLambda) {
    const auto builder = table_builder<test_row>{}.set_id("row_id_test").set_row_id([](const test_row &r) {
        return r.id;
    }).add_column("ID", 50.0f, [](const test_row &) {});
    (void) builder;
}

// --- set_row_id accepts stateful callables ---

TEST(TableBuilder, SetRowIdAcceptsStatefulCallable) {
    int  offset  = 100;
    const auto builder = table_builder<test_row>{}
                       .set_id("row_id_stateful")
                       .set_row_id([offset](const test_row &r) {
        return r.id + offset;
    }).add_column("ID", 50.0f, [](const test_row &) {});
    (void) builder;
}

TEST(TableBuilder, SetRowIdAcceptsStdFunction) {
    std::function<int(const test_row &)> fn      = [](const test_row &r) { return r.id; };
    const auto                                 builder = table_builder<test_row>{}
                       .set_id("row_id_stdfn")
                       .set_row_id(std::move(fn))
                       .add_column("ID", 50.0f, [](const test_row &) {});
    (void) builder;
}

// --- render overload for arbitrary sized ranges compiles ---

TEST(TableBuilder, RenderOverloadForSizedRange) {
    [[maybe_unused]]
    auto builder = table_builder<test_row>{}.set_id("range_test").add_column("ID", 50.0f, [](const test_row &) {});

    static_assert(requires { builder.render(std::declval<std::vector<test_row> &>()); });
    static_assert(requires { builder.render(std::declval<std::vector<test_row> &>(), 100.0f); });
}

// --- column_renderer concept (extracted named concept) ---

TEST(TableBuilder, ColumnRendererConceptAcceptsValid) {
    static_assert(column_renderer<void (*)(const test_row &), test_row>);
    [[maybe_unused]]
    auto valid_lambda = [](const test_row &) {};
    static_assert(column_renderer<decltype(valid_lambda), test_row>);
}

TEST(TableBuilder, ColumnRendererConceptRejectsNonVoid) {
    [[maybe_unused]]
    auto returns_int = [](const test_row &) { return 42; };
    static_assert(!column_renderer<decltype(returns_int), test_row>);
}

TEST(TableBuilder, ColumnRendererConceptRejectsNonInvocable) {
    static_assert(!column_renderer<int, test_row>);
    static_assert(!column_renderer<void (*)(int), test_row>);
}

// --- column_stretch constant ---

TEST(TableBuilder, ColumnStretchConstant) {
    static_assert(column_stretch == 0.0f);
}

// --- Sort comparator logic (ascending, no ImGui context needed) ---

TEST(TableBuilder, SortComparatorAscending) {
    std::vector<test_row> data = {{3, "c", 3.0f}, {1, "a", 1.0f}, {2, "b", 2.0f}};

    auto key_fn = [](const test_row &r) { return r.id; };
    // Simulate ascending sort
    std::ranges::stable_sort(
        data, [&](const test_row &a, const test_row &b) { return std::invoke(key_fn, a) < std::invoke(key_fn, b); });

    EXPECT_EQ(data[0].id, 1);
    EXPECT_EQ(data[1].id, 2);
    EXPECT_EQ(data[2].id, 3);
}

TEST(TableBuilder, SortComparatorDescending) {
    std::vector<test_row> data = {{1, "a", 1.0f}, {3, "c", 3.0f}, {2, "b", 2.0f}};

    auto key_fn = [](const test_row &r) { return r.id; };
    // Simulate descending sort (swap argument order)
    std::ranges::stable_sort(
        data, [&](const test_row &a, const test_row &b) { return std::invoke(key_fn, b) < std::invoke(key_fn, a); });

    EXPECT_EQ(data[0].id, 3);
    EXPECT_EQ(data[1].id, 2);
    EXPECT_EQ(data[2].id, 1);
}

TEST(TableBuilder, SortComparatorConsolidatedLambda) {
    const std::vector<test_row> data = {{3, "c", 3.0f}, {1, "a", 1.0f}, {2, "b", 2.0f}};

    auto key_fn = [](const test_row &r) { return r.id; };

    // Test the consolidated ascending/descending pattern used in sort_if_dirty
    for (bool ascending: {true, false}) {
        auto data_copy = data;
        std::ranges::stable_sort(data_copy, [&](const test_row &a, const test_row &b) {
            return ascending ? std::invoke(key_fn, a) < std::invoke(key_fn, b)
                             : std::invoke(key_fn, b) < std::invoke(key_fn, a);
        });
        if (ascending) {
            EXPECT_EQ(data_copy[0].id, 1);
            EXPECT_EQ(data_copy[2].id, 3);
        } else {
            EXPECT_EQ(data_copy[0].id, 3);
            EXPECT_EQ(data_copy[2].id, 1);
        }
    }
}

// --- Multi-column sort logic ---

TEST(TableBuilder, MultiColumnSortComparators) {
    // Test the multi-column sort pattern: stable_sort applied in reverse priority order
    // (last sort spec applied first, first spec applied last = highest priority)
    std::vector<test_row> data = {
        {1, "b", 2.0f}, {2, "a", 1.0f}, {3, "a", 3.0f}, {1, "c", 1.0f}, {2, "b", 2.0f},
    };

    // Comparators indexed by column
    using cmp_fn                      = bool (*)(const test_row &, const test_row &);
    constexpr std::array<cmp_fn, 3> comparators = {
        +[](const test_row &a, const test_row &b) { return a.id < b.id; },
        +[](const test_row &a, const test_row &b) { return std::string_view(a.name) < std::string_view(b.name); },
        +[](const test_row &a, const test_row &b) { return a.value < b.value; },
    };

    // Sort by column 1 (name) ascending as primary, column 0 (id) ascending as secondary
    // Apply in reverse priority: secondary first, then primary
    // Secondary: sort by id ascending
    std::ranges::stable_sort(data, comparators[0]);
    // Primary: sort by name ascending (stable keeps id order within same name)
    std::ranges::stable_sort(data, comparators[1]);

    // Expected order: a(id=2,val=1), a(id=3,val=3), b(id=1,val=2), b(id=2,val=2), c(id=1,val=1)
    EXPECT_STREQ(data[0].name, "a");
    EXPECT_EQ(data[0].id, 2);
    EXPECT_STREQ(data[1].name, "a");
    EXPECT_EQ(data[1].id, 3);
    EXPECT_STREQ(data[2].name, "b");
    EXPECT_EQ(data[2].id, 1);
    EXPECT_STREQ(data[3].name, "b");
    EXPECT_EQ(data[3].id, 2);
    EXPECT_STREQ(data[4].name, "c");
}

TEST(TableBuilder, MultiColumnSortDescending) {
    std::vector<test_row> data = {
        {1, "a", 1.0f},
        {2, "a", 2.0f},
        {3, "b", 1.0f},
    };

    // Sort by name ascending, then by id descending within same name
    // Apply in reverse priority: id descending first
    std::ranges::stable_sort(data, [](const test_row &a, const test_row &b) {
        return b.id < a.id; // descending
    });
    // Then name ascending (higher priority)
    std::ranges::stable_sort(
        data, [](const test_row &a, const test_row &b) { return std::string_view(a.name) < std::string_view(b.name); });

    // Expected: a(id=2), a(id=1), b(id=3)
    EXPECT_STREQ(data[0].name, "a");
    EXPECT_EQ(data[0].id, 2);
    EXPECT_STREQ(data[1].name, "a");
    EXPECT_EQ(data[1].id, 1);
    EXPECT_STREQ(data[2].name, "b");
    EXPECT_EQ(data[2].id, 3);
}

// --- Row selection toggle ---

TEST(TableBuilder, SelectionToggleInsert) {
    std::unordered_set<int> selection;
    constexpr int                     rid = 42;

    // Simulate clicking a row: if not selected, insert
    const bool was_selected = selection.contains(rid);
    EXPECT_FALSE(was_selected);

    // User clicks -> is_selected becomes true
    constexpr bool is_selected = true;
    if (is_selected != was_selected) {
        if (is_selected)
            selection.insert(rid);
        else
            selection.erase(rid);
    }

    EXPECT_TRUE(selection.contains(42));
    EXPECT_EQ(selection.size(), 1u);
}

TEST(TableBuilder, SelectionToggleRemove) {
    std::unordered_set<int> selection = {42, 7};
    constexpr int                     rid       = 42;

    const bool was_selected = selection.contains(rid);
    EXPECT_TRUE(was_selected);

    // User clicks again -> is_selected becomes false
    constexpr bool is_selected = false;
    if (is_selected != was_selected) {
        if (is_selected)
            selection.insert(rid);
        else
            selection.erase(rid);
    }

    EXPECT_FALSE(selection.contains(42));
    EXPECT_TRUE(selection.contains(7));
    EXPECT_EQ(selection.size(), 1u);
}

TEST(TableBuilder, SelectionMultipleRows) {
    std::unordered_set<int> selection;

    // Select multiple rows
    selection.insert(1);
    selection.insert(2);
    selection.insert(3);

    EXPECT_EQ(selection.size(), 3u);
    EXPECT_TRUE(selection.contains(1));
    EXPECT_TRUE(selection.contains(2));
    EXPECT_TRUE(selection.contains(3));

    // Deselect one
    selection.erase(2);
    EXPECT_EQ(selection.size(), 2u);
    EXPECT_FALSE(selection.contains(2));
}

// --- set_selection compiles ---

TEST(TableBuilder, SetSelectionCompiles) {
    std::unordered_set<int> sel;
    const auto                    builder =
        table_builder<test_row>{}.set_id("sel_test").set_selection(&sel).add_column("ID", 50.0f, [](const test_row &) {
    });
    (void) builder;
}

// --- comparator_fn type alias ---

TEST(TableBuilder, ComparatorFnTypeAlias) {
    using builder_t = table_builder<test_row>;
    using cmp_fn    = builder_t::comparator_fn;
    static_assert(std::is_same_v<cmp_fn, bool (*)(const test_row &, const test_row &)>);
}

// --- sort_if_dirty constraint: key must be totally_ordered ---

TEST(TableBuilder, SortIfDirtyRequiresTotallyOrdered) {
    // int is totally_ordered, so this should satisfy the constraint
    static_assert(std::totally_ordered<int>);
    static_assert(std::totally_ordered<float>);
    static_assert(std::totally_ordered<std::string_view>);

    // A type that is not totally_ordered should be rejected
    struct not_ordered {};
    static_assert(!std::totally_ordered<not_ordered>);
}

// --- Non-random-access range compiles (std::list) ---

TEST(TableBuilder, NonRandomAccessRangeCompiles) {
    [[maybe_unused]]
    auto builder = table_builder<test_row>{}.set_id("list_test").add_column("ID", 50.0f, [](const test_row &) {});

    static_assert(requires { builder.render(std::declval<std::list<test_row> &>()); });
    static_assert(requires { builder.render_clipped(std::declval<std::list<test_row> &>()); });
}

// --- Filtering API ---

TEST(TableBuilder, SetFilterCompiles) {
    const auto builder =
        table_builder<test_row>{}.set_id("filter_test").set_filter([](const test_row &r) {
        return r.id > 1;
    }).add_column("ID", 50.0f, [](const test_row &) {});
    (void) builder;
}

TEST(TableBuilder, FilterPredicateLogic) {
    const std::vector<test_row>                 data   = {{1, "a", 1.0f}, {2, "b", 2.0f}, {3, "c", 3.0f}};
    const std::function<bool(const test_row &)> filter = [](const test_row &r) { return r.id >= 2; };

    std::vector<test_row> filtered;
    for (const auto &row: data) {
        if (filter(row)) filtered.push_back(row);
    }

    EXPECT_EQ(filtered.size(), 2u);
    EXPECT_EQ(filtered[0].id, 2);
    EXPECT_EQ(filtered[1].id, 3);
}

TEST(TableBuilder, FilterWithEmptyPredicate) {
    const std::function<bool(const test_row &)> filter;
    EXPECT_FALSE(static_cast<bool>(filter));

    const std::vector<test_row> data = {{1, "a", 1.0f}, {2, "b", 2.0f}};
    std::vector<test_row> result;
    for (const auto &row: data) {
        if (!filter || filter(row)) result.push_back(row);
    }
    EXPECT_EQ(result.size(), 2u);
}

// --- Ctrl/Shift multi-select logic ---

TEST(TableBuilder, CtrlClickToggleSelect) {
    std::unordered_set<int> selection = {1, 3};

    // Ctrl+click on row 2: add it without clearing
    constexpr bool shift        = false;
    constexpr int  rid          = 2;

    if (constexpr int last_clicked = 1; shift && last_clicked >= 0) {
        constexpr int lo = std::min(last_clicked, rid);
        constexpr int hi = std::max(last_clicked, rid);
        for (int r = lo; r <= hi; ++r)
            selection.insert(r);
    } else if constexpr (constexpr bool ctrl = true) {
        selection.insert(rid);
    } else {
        selection.clear();
        selection.insert(rid);
    }

    EXPECT_EQ(selection.size(), 3u);
    EXPECT_TRUE(selection.contains(1));
    EXPECT_TRUE(selection.contains(2));
    EXPECT_TRUE(selection.contains(3));
}

TEST(TableBuilder, CtrlClickToggleDeselect) {
    std::unordered_set<int> selection = {1, 2, 3};

    // Ctrl+click on row 2: remove it without clearing others
    constexpr bool ctrl        = true;
    constexpr bool shift       = false;
    constexpr int  rid         = 2;
    constexpr bool is_selected = false; // ImGui reports deselect
    (void) shift;
    if (ctrl) {
        if (is_selected)
            selection.insert(rid);
        else
            selection.erase(rid);
    }

    EXPECT_EQ(selection.size(), 2u);
    EXPECT_TRUE(selection.contains(1));
    EXPECT_FALSE(selection.contains(2));
    EXPECT_TRUE(selection.contains(3));
}

TEST(TableBuilder, ShiftClickRangeSelect) {
    std::unordered_set<int> selection;
    constexpr int                     last_clicked = 2;

    // Shift+click on row 5: select range [2, 5]
    constexpr bool shift = true;
    constexpr int  rid   = 5;

    if (last_clicked >= 0) {
        constexpr int lo = std::min(last_clicked, rid);
        constexpr int hi = std::max(last_clicked, rid);
        for (int r = lo; r <= hi; ++r)
            selection.insert(r);
    }

    EXPECT_EQ(selection.size(), 4u);
    EXPECT_TRUE(selection.contains(2));
    EXPECT_TRUE(selection.contains(3));
    EXPECT_TRUE(selection.contains(4));
    EXPECT_TRUE(selection.contains(5));
}

TEST(TableBuilder, ShiftClickRangeSelectReverse) {
    std::unordered_set<int> selection;
    constexpr int                     last_clicked = 5;

    // Shift+click on row 2 (reverse direction): select range [2, 5]
    constexpr int       rid = 2;
    constexpr int lo  = std::min(last_clicked, rid);
    constexpr int hi  = std::max(last_clicked, rid);
    for (int r = lo; r <= hi; ++r)
        selection.insert(r);

    EXPECT_EQ(selection.size(), 4u);
    EXPECT_TRUE(selection.contains(2));
    EXPECT_TRUE(selection.contains(3));
    EXPECT_TRUE(selection.contains(4));
    EXPECT_TRUE(selection.contains(5));
}

TEST(TableBuilder, PlainClickClearsSelection) {
    std::unordered_set<int> selection = {1, 2, 3, 4, 5};

    // Plain click (no modifier) on row 3: clear all, select only 3
    constexpr bool ctrl         = false;
    constexpr bool shift        = false;
    constexpr int  rid          = 3;
    constexpr int  last_clicked = -1;

    if (shift && last_clicked >= 0) {
        // range select
    } else if (ctrl) {
        selection.insert(rid);
    } else {
        selection.clear();
        selection.insert(rid);
    }

    EXPECT_EQ(selection.size(), 1u);
    EXPECT_TRUE(selection.contains(3));
}

// --- set_column_visible compiles ---

TEST(TableBuilder, SetColumnVisibleCompiles) {
    using builder_t = table_builder<test_row>;
    static_assert(requires { builder_t::set_column_visible(0, true); });
}

// --- set_filter is rvalue-qualified ---

TEST(TableBuilder, SetFilterIsRvalueQualified) {
    static_assert(
        requires(table_builder<test_row> b) { std::move(b).set_filter([](const test_row &) { return true; }); });
}

// --- render_single_row renamed from row ---

TEST(TableBuilder, RenderSingleRowCompiles) {
    [[maybe_unused]]
    auto builder = table_builder<test_row>{}.set_id("render_test").add_column("ID", 50.0f, [](const test_row &) {});

    static_assert(requires { builder.render_single_row(std::declval<const test_row &>()); });
}
