// presets.hpp - reusable flag presets and size constants for windows, tables, and columns
//
// Usage:
//   ImGui::Begin("My Window", &open, imgui_util::layout::window::sidebar);
//   if (ImGui::BeginTable("t", 3, imgui_util::layout::table::sortable_list)) { ... }
//   auto flags = imgui_util::layout::with(window::sidebar, ImGuiWindowFlags_MenuBar);
//
// Presets are plain constexpr values -- combine them with with()/without() helpers.
#pragma once

#include <concepts>
#include <imgui.h>
#include <type_traits>

namespace imgui_util::layout {

    // Flag composition helpers (separate template params to handle ImGui's int typedef vs enum mismatch)
    template<typename T, typename U>
        requires(std::integral<T> || std::is_enum_v<T>) && (std::integral<U> || std::is_enum_v<U>)
    [[nodiscard]]
    constexpr T with(T base, U flags) noexcept {
        return static_cast<T>(base | flags); // NOLINT(hicpp-signed-bitwise)
    }

    template<typename T, typename U>
        requires(std::integral<T> || std::is_enum_v<T>) && (std::integral<U> || std::is_enum_v<U>)
    [[nodiscard]]
    constexpr T without(T base, U flags) noexcept {
        return static_cast<T>(base & ~flags); // NOLINT(hicpp-signed-bitwise)
    }

    // Window flag presets -- combine with with()/without() if you need variations
    namespace window {

        inline constexpr ImGuiWindowFlags navbar = // NOLINT(hicpp-signed-bitwise)
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_MenuBar;

        inline constexpr ImGuiWindowFlags settings_panel = // NOLINT(hicpp-signed-bitwise)
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing;

        inline constexpr ImGuiWindowFlags tooltip = // NOLINT(hicpp-signed-bitwise)
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

        inline constexpr ImGuiWindowFlags overlay = // NOLINT(hicpp-signed-bitwise)
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings;

        inline constexpr ImGuiWindowFlags modal_dialog = // NOLINT(hicpp-signed-bitwise)
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        inline constexpr ImGuiWindowFlags dockspace_host = // NOLINT(hicpp-signed-bitwise)
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;

        inline constexpr ImGuiWindowFlags sidebar = // NOLINT(hicpp-signed-bitwise)
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse;

        inline constexpr ImGuiWindowFlags popup = // NOLINT(hicpp-signed-bitwise)
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize;

    } // namespace window

    // Table flag presets
    namespace table {

        inline constexpr ImGuiTableFlags summary =           // NOLINT(hicpp-signed-bitwise)
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg; // static key-value display

        inline constexpr ImGuiTableFlags scroll_list = // NOLINT(hicpp-signed-bitwise)
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp;

        inline constexpr ImGuiTableFlags resizable_list = // NOLINT(hicpp-signed-bitwise)
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;

        inline constexpr ImGuiTableFlags sortable_list =
            resizable_list | ImGuiTableFlags_Sortable; // NOLINT(hicpp-signed-bitwise)

        inline constexpr ImGuiTableFlags property = // NOLINT(hicpp-signed-bitwise)
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;

        inline constexpr ImGuiTableFlags compact =
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody; // NOLINT(hicpp-signed-bitwise)

    } // namespace table

    // Column flag presets
    namespace column {

        inline constexpr ImGuiTableColumnFlags frozen_column = // NOLINT(hicpp-signed-bitwise)
            ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide;

        inline constexpr ImGuiTableColumnFlags default_sort = // NOLINT(hicpp-signed-bitwise)
            ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortAscending;

    } // namespace column

    // Constexpr ImVec2 wrapper that supports named mutations (with_width, with_height)
    struct size_preset {
        float width;
        float height;
        [[nodiscard]]
        constexpr ImVec2 vec() const noexcept {
            return {width, height};
        }
        explicit constexpr operator ImVec2() const noexcept {
            return {width, height};
        } // explicit: use .vec() or static_cast<ImVec2>(preset)
        [[nodiscard]]
        static constexpr size_preset from(const ImVec2 v) noexcept {
            return {.width = v.x, .height = v.y};
        }
        [[nodiscard]]
        constexpr size_preset with_width(const float w) const noexcept {
            return {.width = w, .height = height};
        }
        [[nodiscard]]
        constexpr size_preset with_height(const float h) const noexcept {
            return {.width = width, .height = h};
        }
        [[nodiscard]]
        constexpr size_preset scale(const float f) const noexcept {
            return {.width = width * f, .height = height * f};
        }
        [[nodiscard]]
        constexpr friend size_preset operator+(const size_preset &a, const size_preset &b) noexcept {
            return {.width = a.width + b.width, .height = a.height + b.height};
        }
        [[nodiscard]]
        constexpr friend size_preset operator-(const size_preset &a, const size_preset &b) noexcept {
            return {.width = a.width - b.width, .height = a.height - b.height};
        }
    };

    inline constexpr size_preset dialog_size{.width = 500.0f, .height = 400.0f};
    inline constexpr size_preset editor_size{.width = 500.0f, .height = 600.0f};

    // Common dimension defaults used across widgets
    namespace defaults {

        inline constexpr float       button_width = 120.0f;
        inline constexpr float       label_width  = 180.0f;
        inline constexpr float       list_height  = 150.0f;
        inline constexpr float       input_width  = 400.0f;
        inline constexpr size_preset auto_size{.width = 0.0f, .height = 0.0f}; // (0,0) = auto-fit

    } // namespace defaults

} // namespace imgui_util::layout
