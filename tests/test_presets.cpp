// NOLINTBEGIN(hicpp-signed-bitwise)
#include <gtest/gtest.h>
#include <imgui_util/layout/presets.hpp>
#include <type_traits>

using namespace imgui_util::layout;

// --- Flag bit verification ---

TEST(LayoutPresets, TooltipUsesNoDecoration) {
    // NoDecoration = NoTitleBar | NoResize | NoScrollbar | NoCollapse
    EXPECT_TRUE(window::tooltip & ImGuiWindowFlags_NoTitleBar);
    EXPECT_TRUE(window::tooltip & ImGuiWindowFlags_NoResize);
    EXPECT_TRUE(window::tooltip & ImGuiWindowFlags_NoScrollbar);
    EXPECT_TRUE(window::tooltip & ImGuiWindowFlags_NoCollapse);
    EXPECT_TRUE(window::tooltip & ImGuiWindowFlags_NoMove);
    EXPECT_TRUE(window::tooltip & ImGuiWindowFlags_AlwaysAutoResize);
    EXPECT_TRUE(window::tooltip & ImGuiWindowFlags_NoSavedSettings);
    EXPECT_TRUE(window::tooltip & ImGuiWindowFlags_NoDocking);
}

TEST(LayoutPresets, DockspaceHostFlags) {
    EXPECT_TRUE(window::dockspace_host & ImGuiWindowFlags_NoTitleBar);
    EXPECT_TRUE(window::dockspace_host & ImGuiWindowFlags_NoCollapse);
    EXPECT_TRUE(window::dockspace_host & ImGuiWindowFlags_NoResize);
    EXPECT_TRUE(window::dockspace_host & ImGuiWindowFlags_NoMove);
    EXPECT_TRUE(window::dockspace_host & ImGuiWindowFlags_NoBringToFrontOnFocus);
    EXPECT_TRUE(window::dockspace_host & ImGuiWindowFlags_NoNavFocus);
    EXPECT_TRUE(window::dockspace_host & ImGuiWindowFlags_NoBackground);
}

TEST(LayoutPresets, SidebarFlags) {
    EXPECT_TRUE(window::sidebar & ImGuiWindowFlags_NoTitleBar);
    EXPECT_TRUE(window::sidebar & ImGuiWindowFlags_NoMove);
    EXPECT_TRUE(window::sidebar & ImGuiWindowFlags_NoResize);
    EXPECT_TRUE(window::sidebar & ImGuiWindowFlags_NoCollapse);
}

TEST(LayoutPresets, ColumnPresetFlags) {
    EXPECT_TRUE(column::frozen_column & ImGuiTableColumnFlags_NoResize);
    EXPECT_TRUE(column::frozen_column & ImGuiTableColumnFlags_NoReorder);
    EXPECT_TRUE(column::frozen_column & ImGuiTableColumnFlags_NoHide);
    EXPECT_TRUE(column::default_sort & ImGuiTableColumnFlags_DefaultSort);
    EXPECT_TRUE(column::default_sort & ImGuiTableColumnFlags_PreferSortAscending);
}

// --- size_preset ---

static_assert(dialog_size.width == 500.0f);
static_assert(dialog_size.height == 400.0f);
static_assert(editor_size.width == 500.0f);
static_assert(editor_size.height == 600.0f);

TEST(LayoutPresets, SizePresetVec) {
    constexpr auto dv = dialog_size.vec();
    EXPECT_FLOAT_EQ(dv.x, 500.0f);
    EXPECT_FLOAT_EQ(dv.y, 400.0f);

    constexpr auto ev = editor_size.vec();
    EXPECT_FLOAT_EQ(ev.x, 500.0f);
    EXPECT_FLOAT_EQ(ev.y, 600.0f);
}

// --- with()/without() helpers ---

TEST(LayoutPresets, WithAddsFlags) {
    constexpr auto result = with(window::modal_dialog, ImGuiWindowFlags_NoTitleBar);
    EXPECT_TRUE(result & ImGuiWindowFlags_NoTitleBar);
    EXPECT_TRUE(result & ImGuiWindowFlags_NoResize);
    EXPECT_TRUE(result & ImGuiWindowFlags_NoMove);
}

TEST(LayoutPresets, WithoutRemovesFlags) {
    constexpr auto result = without(window::tooltip, ImGuiWindowFlags_NoDocking);
    EXPECT_FALSE(result & ImGuiWindowFlags_NoDocking);
    // Other flags should remain
    EXPECT_TRUE(result & ImGuiWindowFlags_NoTitleBar);
    EXPECT_TRUE(result & ImGuiWindowFlags_NoMove);
}

TEST(LayoutPresets, WithWithoutRoundTrip) {
    constexpr auto base    = window::overlay;
    constexpr auto added   = with(base, ImGuiWindowFlags_MenuBar);
    constexpr auto removed = without(added, ImGuiWindowFlags_MenuBar);
    // Removing the added flag should yield the original
    EXPECT_EQ(removed, base);
}

// --- Exact flag composition checks ---

static_assert(window::modal_dialog == (ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse));
static_assert(window::sidebar == (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse));
static_assert(window::overlay == (ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings));
static_assert(window::popup == (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize));
static_assert(window::navbar == (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar));
static_assert(window::settings_panel == (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing));
static_assert(window::dockspace_host == (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground));

static_assert(table::summary == (ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg));
static_assert(table::scroll_list == (ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp));
static_assert(table::resizable_list == (ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable));
static_assert(table::sortable_list == (table::resizable_list | ImGuiTableFlags_Sortable));
static_assert(table::property == (ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit));
static_assert(table::compact == (ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody));

static_assert(column::frozen_column == (ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide));
static_assert(column::default_sort == (ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortAscending));

// --- New preset tests ---

static_assert(defaults::auto_size.width == 0.0f);
static_assert(defaults::auto_size.height == 0.0f);

TEST(LayoutPresets, PopupFlags) {
    EXPECT_TRUE(window::popup & ImGuiWindowFlags_NoTitleBar);
    EXPECT_TRUE(window::popup & ImGuiWindowFlags_NoResize);
    EXPECT_TRUE(window::popup & ImGuiWindowFlags_NoMove);
    EXPECT_TRUE(window::popup & ImGuiWindowFlags_AlwaysAutoResize);
}

TEST(LayoutPresets, CompactTableFlags) {
    EXPECT_TRUE(table::compact & ImGuiTableFlags_SizingFixedFit);
    EXPECT_TRUE(table::compact & ImGuiTableFlags_NoBordersInBody);
}

TEST(LayoutPresets, AutoSize) {
    constexpr auto v = defaults::auto_size.vec();
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
}

// --- size_preset builder methods and conversions ---

TEST(LayoutPresets, SizePresetWithWidth) {
    constexpr auto sp = dialog_size.with_width(800.0f);
    EXPECT_FLOAT_EQ(sp.width, 800.0f);
    EXPECT_FLOAT_EQ(sp.height, 400.0f);
}

TEST(LayoutPresets, SizePresetWithHeight) {
    constexpr auto sp = dialog_size.with_height(900.0f);
    EXPECT_FLOAT_EQ(sp.width, 500.0f);
    EXPECT_FLOAT_EQ(sp.height, 900.0f);
}

TEST(LayoutPresets, SizePresetExplicitConversion) {
    constexpr ImVec2 v = dialog_size.vec();
    EXPECT_FLOAT_EQ(v.x, 500.0f);
    EXPECT_FLOAT_EQ(v.y, 400.0f);
}

TEST(LayoutPresets, SizePresetVecRoundTrip) {
    constexpr ImVec2 v = editor_size.vec();
    EXPECT_FLOAT_EQ(v.x, editor_size.width);
    EXPECT_FLOAT_EQ(v.y, editor_size.height);
}
// NOLINTEND(hicpp-signed-bitwise)
