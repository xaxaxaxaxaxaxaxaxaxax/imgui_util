#include <gtest/gtest.h>
#include <imgui_util/core/raii.hpp>
#include <type_traits>

using namespace imgui_util;

// --- end_policy enum values exist ---

TEST(RaiiEndPolicy, EnumValuesExist) {
    [[maybe_unused]] constexpr auto always_val      = end_policy::always;
    [[maybe_unused]] constexpr auto conditional_val = end_policy::conditional;
    [[maybe_unused]] constexpr auto none_val        = end_policy::none;
}

// --- raii_scope is non-copyable and non-movable ---

static_assert(!std::is_copy_constructible_v<window>, "window must not be copy-constructible");
static_assert(!std::is_copy_assignable_v<window>, "window must not be copy-assignable");
static_assert(!std::is_move_constructible_v<window>, "window must not be move-constructible");
static_assert(!std::is_move_assignable_v<window>, "window must not be move-assignable");

static_assert(!std::is_copy_constructible_v<tab_bar>, "tab_bar must not be copy-constructible");
static_assert(!std::is_copy_assignable_v<tab_bar>, "tab_bar must not be copy-assignable");
static_assert(!std::is_move_constructible_v<tab_bar>, "tab_bar must not be move-constructible");
static_assert(!std::is_move_assignable_v<tab_bar>, "tab_bar must not be move-assignable");

static_assert(!std::is_copy_constructible_v<style_var>, "style_var must not be copy-constructible");
static_assert(!std::is_copy_assignable_v<style_var>, "style_var must not be copy-assignable");
static_assert(!std::is_move_constructible_v<style_var>, "style_var must not be move-constructible");
static_assert(!std::is_move_assignable_v<style_var>, "style_var must not be move-assignable");

// --- policy values for known traits ---

TEST(RaiiTraits, EndPolicyValues) {
    // Always policy traits
    EXPECT_EQ(window_trait::policy, end_policy::always);
    EXPECT_EQ(child_trait::policy, end_policy::always);
    EXPECT_EQ(group_trait::policy, end_policy::always);
    EXPECT_EQ(tooltip_trait::policy, end_policy::always);
    EXPECT_EQ(disabled_trait::policy, end_policy::always);

    // Conditional policy traits
    EXPECT_EQ(menu_bar_trait::policy, end_policy::conditional);
    EXPECT_EQ(main_menu_bar_trait::policy, end_policy::conditional);
    EXPECT_EQ(tab_bar_trait::policy, end_policy::conditional);
    EXPECT_EQ(tab_item_trait::policy, end_policy::conditional);
    EXPECT_EQ(combo_trait::policy, end_policy::conditional);
    EXPECT_EQ(popup_modal_trait::policy, end_policy::conditional);
    EXPECT_EQ(tree_node_trait::policy, end_policy::conditional);
    EXPECT_EQ(popup_trait::policy, end_policy::conditional);
    EXPECT_EQ(menu_trait::policy, end_policy::conditional);
    EXPECT_EQ(table_trait::policy, end_policy::conditional);
    EXPECT_EQ(list_box_trait::policy, end_policy::conditional);

    // None policy traits
    EXPECT_EQ(style_var_trait::policy, end_policy::none);
    EXPECT_EQ(style_color_trait::policy, end_policy::none);
    EXPECT_EQ(id_trait::policy, end_policy::none);
    EXPECT_EQ(item_width_trait::policy, end_policy::none);
    EXPECT_EQ(indent_trait::policy, end_policy::none);
}

// --- kHasState correctness ---
// kHasState is true when kPolicy != end_policy::none, i.e. always and conditional

TEST(RaiiTraits, HasStateForAlwaysPolicy) {
    // Always policy: kHasState = true (policy != none)
    static_assert(window_trait::policy != end_policy::none);
    static_assert(group_trait::policy != end_policy::none);
}

TEST(RaiiTraits, HasStateForConditionalPolicy) {
    // Conditional policy: kHasState = true (policy != none)
    static_assert(tab_bar_trait::policy != end_policy::none);
    static_assert(combo_trait::policy != end_policy::none);
}

TEST(RaiiTraits, NoStateForNonePolicy) {
    // None policy: kHasState = false (policy == none)
    static_assert(style_var_trait::policy == end_policy::none);
    static_assert(style_color_trait::policy == end_policy::none);
    static_assert(id_trait::policy == end_policy::none);
    static_assert(indent_trait::policy == end_policy::none);
}

// --- storage type correctness ---

TEST(RaiiTraits, storageTypes) {
    static_assert(std::is_same_v<window_trait::storage, std::monostate>);
    static_assert(std::is_same_v<tab_bar_trait::storage, std::monostate>);
    static_assert(std::is_same_v<style_var_trait::storage, std::monostate>);
    // indent_trait stores a float for unindent
    static_assert(std::is_same_v<indent_trait::storage, float>);
}

// --- bool conversion only available for stateful scopes ---

static_assert(std::is_constructible_v<bool, window>, "window should be convertible to bool");
static_assert(std::is_constructible_v<bool, tab_bar>, "tab_bar should be convertible to bool");
// menu_bar should also be convertible to bool (conditional policy)
static_assert(std::is_constructible_v<bool, menu_bar>, "menu_bar should be convertible to bool");
// style_var (None policy) should NOT be convertible to bool
static_assert(!std::is_constructible_v<bool, style_var>, "style_var should not be convertible to bool");

// --- style_vars entry supports both float and ImVec2 ---

TEST(StyleVars, EntryAcceptsFloat) {
    // Compile-time check: entry can be constructed with float
    [[maybe_unused]] const style_vars::entry e{ImGuiStyleVar_Alpha, 0.5f};
}

TEST(StyleVars, EntryAcceptsImVec2) {
    // Compile-time check: entry can be constructed with ImVec2
    [[maybe_unused]] const style_vars::entry e{ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f)};
}

// --- Mock trait behavioral tests ---

namespace {
    struct mock_counter {
        static inline int begin_count = 0;
        static inline int end_count   = 0;
        static void       reset() { begin_count = end_count = 0; }
    };

    struct mock_always_trait {
        static constexpr auto policy = end_policy::always;
        using storage                = std::monostate;
        static bool begin() noexcept {
            ++mock_counter::begin_count;
            return true;
        }
        static void end() noexcept { ++mock_counter::end_count; }
    };

    struct mock_conditional_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(bool ret) noexcept {
            ++mock_counter::begin_count;
            return ret;
        }
        static void end() noexcept { ++mock_counter::end_count; }
    };

    struct mock_none_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin() noexcept { ++mock_counter::begin_count; }
        static void end() noexcept { ++mock_counter::end_count; }
    };
} // namespace

TEST(RaiiMock, EndCalledOnceOnDestruction) {
    mock_counter::reset();
    { const raii_scope<mock_always_trait> scope; }
    EXPECT_EQ(mock_counter::begin_count, 1);
    EXPECT_EQ(mock_counter::end_count, 1);
}

TEST(RaiiMock, ConditionalEndCalledWhenTrue) {
    mock_counter::reset();
    { const raii_scope<mock_conditional_trait> scope(true); }
    EXPECT_EQ(mock_counter::begin_count, 1);
    EXPECT_EQ(mock_counter::end_count, 1);
}

TEST(RaiiMock, ConditionalEndNotCalledWhenFalse) {
    mock_counter::reset();
    { const raii_scope<mock_conditional_trait> scope(false); }
    EXPECT_EQ(mock_counter::begin_count, 1);
    EXPECT_EQ(mock_counter::end_count, 0);
}

TEST(RaiiMock, NoneEndAlwaysCalled) {
    mock_counter::reset();
    { const raii_scope<mock_none_trait> scope; }
    EXPECT_EQ(mock_counter::begin_count, 1);
    EXPECT_EQ(mock_counter::end_count, 1);
}
