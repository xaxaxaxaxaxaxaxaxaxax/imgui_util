#include <gtest/gtest.h>
#include <imgui_util/plot/raii.hpp>
#include <type_traits>

using namespace imgui_util;
using namespace imgui_util::implot;

// --- policy values for all plot traits ---

static_assert(plot_trait::policy == end_policy::conditional);
static_assert(subplot_trait::policy == end_policy::conditional);
static_assert(aligned_plots_trait::policy == end_policy::conditional);
static_assert(legend_popup_trait::policy == end_policy::conditional);
static_assert(drag_drop_source_plot_trait::policy == end_policy::conditional);
static_assert(drag_drop_source_axis_trait::policy == end_policy::conditional);
static_assert(drag_drop_source_item_trait::policy == end_policy::conditional);
static_assert(drag_drop_target_plot_trait::policy == end_policy::conditional);
static_assert(drag_drop_target_axis_trait::policy == end_policy::conditional);
static_assert(drag_drop_target_legend_trait::policy == end_policy::conditional);

static_assert(colormap_trait::policy == end_policy::none);
static_assert(plot_style_color_trait::policy == end_policy::none);
static_assert(plot_style_var_trait::policy == end_policy::none);
static_assert(plot_clip_rect_trait::policy == end_policy::none);

// --- raii_scope aliases are non-copyable and non-movable ---

#define ASSERT_NON_COPYABLE_NON_MOVABLE(T)                                                                             \
    static_assert(!std::is_copy_constructible_v<T>, #T " must not be copy-constructible");                             \
    static_assert(!std::is_copy_assignable_v<T>, #T " must not be copy-assignable");                                   \
    static_assert(!std::is_move_constructible_v<T>, #T " must not be move-constructible");                             \
    static_assert(!std::is_move_assignable_v<T>, #T " must not be move-assignable")

ASSERT_NON_COPYABLE_NON_MOVABLE(plot);
ASSERT_NON_COPYABLE_NON_MOVABLE(subplots);
ASSERT_NON_COPYABLE_NON_MOVABLE(aligned_plots);
ASSERT_NON_COPYABLE_NON_MOVABLE(legend_popup);
ASSERT_NON_COPYABLE_NON_MOVABLE(colormap);
ASSERT_NON_COPYABLE_NON_MOVABLE(plot_style_color);
ASSERT_NON_COPYABLE_NON_MOVABLE(plot_style_var);
ASSERT_NON_COPYABLE_NON_MOVABLE(plot_clip_rect);
ASSERT_NON_COPYABLE_NON_MOVABLE(drag_drop_target_plot);
ASSERT_NON_COPYABLE_NON_MOVABLE(drag_drop_target_axis);
ASSERT_NON_COPYABLE_NON_MOVABLE(drag_drop_target_legend);
ASSERT_NON_COPYABLE_NON_MOVABLE(drag_drop_source_plot);
ASSERT_NON_COPYABLE_NON_MOVABLE(drag_drop_source_axis);
ASSERT_NON_COPYABLE_NON_MOVABLE(drag_drop_source_item);

ASSERT_NON_COPYABLE_NON_MOVABLE(plot_style_vars);
ASSERT_NON_COPYABLE_NON_MOVABLE(plot_style_colors);

#undef ASSERT_NON_COPYABLE_NON_MOVABLE

// --- conditional-policy types are bool-convertible ---

static_assert(std::is_constructible_v<bool, plot>, "plot should be convertible to bool");
static_assert(std::is_constructible_v<bool, subplots>, "subplots should be convertible to bool");
static_assert(std::is_constructible_v<bool, aligned_plots>, "aligned_plots should be convertible to bool");
static_assert(std::is_constructible_v<bool, legend_popup>, "legend_popup should be convertible to bool");
static_assert(std::is_constructible_v<bool, drag_drop_source_plot>,
              "drag_drop_source_plot should be convertible to bool");
static_assert(std::is_constructible_v<bool, drag_drop_source_axis>,
              "drag_drop_source_axis should be convertible to bool");
static_assert(std::is_constructible_v<bool, drag_drop_source_item>,
              "drag_drop_source_item should be convertible to bool");
static_assert(std::is_constructible_v<bool, drag_drop_target_plot>,
              "drag_drop_target_plot should be convertible to bool");
static_assert(std::is_constructible_v<bool, drag_drop_target_axis>,
              "drag_drop_target_axis should be convertible to bool");
static_assert(std::is_constructible_v<bool, drag_drop_target_legend>,
              "drag_drop_target_legend should be convertible to bool");

// --- none-policy types are NOT bool-convertible ---

static_assert(!std::is_constructible_v<bool, colormap>, "colormap should not be convertible to bool");
static_assert(!std::is_constructible_v<bool, plot_style_color>, "plot_style_color should not be convertible to bool");
static_assert(!std::is_constructible_v<bool, plot_style_var>, "plot_style_var should not be convertible to bool");
static_assert(!std::is_constructible_v<bool, plot_clip_rect>, "plot_clip_rect should not be convertible to bool");

// --- plot_style_vars::entry supports float, ImVec2, and int ---

TEST(PlotStyleVars, EntryAcceptsFloat) {
    [[maybe_unused]] plot_style_vars::entry e{ImPlotStyleVar_LineWeight, 2.0f};
}

TEST(PlotStyleVars, EntryAcceptsImVec2) {
    [[maybe_unused]] plot_style_vars::entry e{ImPlotStyleVar_MajorTickLen, ImVec2(10.0f, 10.0f)};
}

TEST(PlotStyleVars, EntryAcceptsInt) {
    [[maybe_unused]] plot_style_vars::entry e{ImPlotStyleVar_Marker, 1};
}
