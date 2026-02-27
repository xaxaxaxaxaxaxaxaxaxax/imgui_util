/// @file presets.hpp
/// @brief Reusable flag presets and size constants for windows, tables, and columns.
///
/// Presets are plain constexpr values -- combine them with with()/without() helpers.
///
/// Usage:
/// @code
///   ImGui::Begin("My Window", &open, imgui_util::layout::window::sidebar);
///   if (ImGui::BeginTable("t", 3, imgui_util::layout::table::sortable_list)) { ... }
///   auto flags = imgui_util::layout::with(window::sidebar, ImGuiWindowFlags_MenuBar);
/// @endcode
#pragma once

#include <concepts>
#include <imgui.h>
#include <type_traits>

namespace imgui_util::layout {

    /**
     * @brief Add flags to a base flag set.
     *
     * Handles ImGui's int-typedef / enum mismatch via separate template params.
     * @param base   Base flag value.
     * @param flags  Flags to add.
     * @return Combined flag value.
     */
    template<typename T, typename U>
        requires(std::integral<T> || std::is_enum_v<T>) && (std::integral<U> || std::is_enum_v<U>)
    [[nodiscard]] constexpr T with(T base, U flags) noexcept {
        return static_cast<T>(base | flags);
    }

    /**
     * @brief Remove flags from a base flag set.
     * @param base   Base flag value.
     * @param flags  Flags to remove.
     * @return Resulting flag value.
     */
    template<typename T, typename U>
        requires(std::integral<T> || std::is_enum_v<T>) && (std::integral<U> || std::is_enum_v<U>)
    [[nodiscard]] constexpr T without(T base, U flags) noexcept {
        return static_cast<T>(base & ~flags);
    }

    namespace window {

        constexpr ImGuiWindowFlags navbar = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;
        constexpr ImGuiWindowFlags settings_panel = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoFocusOnAppearing;
        constexpr ImGuiWindowFlags tooltip = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
        constexpr ImGuiWindowFlags overlay = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;
        constexpr ImGuiWindowFlags modal_dialog =
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
        constexpr ImGuiWindowFlags dockspace_host = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        constexpr ImGuiWindowFlags sidebar = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        constexpr ImGuiWindowFlags popup = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;

    } // namespace window

    namespace table {

        constexpr ImGuiTableFlags summary     = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
        constexpr ImGuiTableFlags scroll_list = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
        constexpr ImGuiTableFlags resizable_list =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
        constexpr ImGuiTableFlags sortable_list = resizable_list | ImGuiTableFlags_Sortable;
        constexpr ImGuiTableFlags property =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
        constexpr ImGuiTableFlags compact = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody;

    } // namespace table

    namespace column {

        constexpr ImGuiTableColumnFlags frozen_column =
            ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide;

        constexpr ImGuiTableColumnFlags default_sort =
            ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortAscending;

    } // namespace column

    /// @brief Compile-time width/height pair with ImVec2 conversion and builder-style overrides.
    struct size_preset {
        float width;
        float height;

        /// @brief Convert to ImVec2.
        [[nodiscard]] constexpr ImVec2 vec() const noexcept { return {width, height}; }
        /// @brief Explicit conversion to ImVec2 -- use .vec() or static_cast.
        explicit constexpr             operator ImVec2() const noexcept {
            return {width, height};
        }
        /// @brief Return a copy with a different width.
        [[nodiscard]] constexpr size_preset with_width(const float w) const noexcept {
            return {.width = w, .height = height};
        }
        /// @brief Return a copy with a different height.
        [[nodiscard]] constexpr size_preset with_height(const float h) const noexcept {
            return {.width = width, .height = h};
        }
    };

    constexpr size_preset dialog_size{.width = 500.0f, .height = 400.0f};
    constexpr size_preset editor_size{.width = 500.0f, .height = 600.0f};

    namespace defaults {

        constexpr float       button_width = 120.0f;
        constexpr float       label_width  = 180.0f;
        constexpr float       list_height  = 150.0f;
        constexpr float       input_width  = 400.0f;
        constexpr size_preset auto_size{.width = 0.0f, .height = 0.0f}; // (0,0) = auto-fit

    } // namespace defaults

} // namespace imgui_util::layout
