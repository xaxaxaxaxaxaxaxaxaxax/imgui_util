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

    template<typename T>
    concept flag_type = std::integral<T> || std::is_enum_v<T>;

    template<flag_type T, std::convertible_to<T>... Us>
        requires(sizeof...(Us) >= 1)
    [[nodiscard]] constexpr T with(T base, Us... flags) noexcept {
        return static_cast<T>((base | ... | static_cast<T>(flags)));
    }

    template<flag_type T, std::convertible_to<T>... Us>
        requires(sizeof...(Us) >= 1)
    [[nodiscard]] constexpr T without(T base, Us... flags) noexcept {
        return static_cast<T>((base & ... & ~static_cast<T>(flags)));
    }

    template<flag_type T, std::convertible_to<T> U = T>
    [[nodiscard]] constexpr bool has_all(T base, U flags) noexcept {
        return (base & static_cast<T>(flags)) == static_cast<T>(flags);
    }

    template<flag_type T, std::convertible_to<T> U = T>
    [[nodiscard]] constexpr bool has_any(T base, U flags) noexcept {
        return (base & static_cast<T>(flags)) != static_cast<T>(0);
    }

    template<flag_type T, std::convertible_to<T> U = T>
    [[nodiscard]] constexpr T toggle(T base, U flags) noexcept {
        return static_cast<T>(base ^ static_cast<T>(flags));
    }

    namespace window {

        constexpr ImGuiWindowFlags navbar = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;
        constexpr ImGuiWindowFlags settings_panel = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoFocusOnAppearing;
        constexpr ImGuiWindowFlags tooltip = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoFocusOnAppearing;
        constexpr ImGuiWindowFlags overlay = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;
        constexpr ImGuiWindowFlags modal_dialog = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
        constexpr ImGuiWindowFlags dockspace_host = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        constexpr ImGuiWindowFlags sidebar = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        constexpr ImGuiWindowFlags popup = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

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
        constexpr ImGuiTableFlags compact     = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody;
        constexpr ImGuiTableFlags equal_width = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchSame;

    } // namespace table

    namespace column {

        constexpr ImGuiTableColumnFlags frozen_column =
            ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide;

        constexpr ImGuiTableColumnFlags default_sort =
            ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortAscending;

        constexpr ImGuiTableColumnFlags frozen_default_sort = frozen_column | default_sort;
        constexpr ImGuiTableColumnFlags stretch_fill        = ImGuiTableColumnFlags_WidthStretch;

    } // namespace column

    struct size_preset {
        float width;
        float height;

        explicit constexpr             operator ImVec2() const noexcept { return {width, height}; }
        [[nodiscard]] constexpr ImVec2 vec2() const noexcept { return {width, height}; }
        [[nodiscard]] constexpr size_preset with_width(const float w) const noexcept {
            return {.width = w, .height = height};
        }
        [[nodiscard]] constexpr size_preset with_height(const float h) const noexcept {
            return {.width = width, .height = h};
        }
        [[nodiscard]] constexpr size_preset scaled(const float s) const noexcept {
            return {.width = width * s, .height = height * s};
        }
        [[nodiscard]] constexpr size_preset operator+(const size_preset &rhs) const noexcept {
            return {.width = width + rhs.width, .height = height + rhs.height};
        }
        friend constexpr bool operator==(const size_preset &, const size_preset &) noexcept = default;
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
