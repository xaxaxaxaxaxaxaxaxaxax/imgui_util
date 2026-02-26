// raii.hpp - RAII scoped wrappers for ImPlot Begin/End and Push/Pop pairs
//
// Usage:
//   if (imgui_util::implot::plot p{"My Plot"}) { ImPlot::PlotLine("sin", xs, ys, n); }
//   { imgui_util::implot::colormap cm{ImPlotColormap_Viridis}; ... }
//   { imgui_util::implot::plot_style_var sv{ImPlotStyleVar_LineWeight, 2.0f}; ... }
//
// End/Pop is called automatically in the destructor.
// Mirrors core/raii.hpp conventions for ImPlot instead of ImGui.
#pragma once
#include <implot.h>
#include <variant>

#include "imgui_util/core/raii.hpp"

namespace imgui_util::implot {

    // Each *_trait struct adapts an ImPlot scope for use with raii_scope<T> from core/raii.hpp.
    // policy::conditional = end() only if begin() returned true; policy::none = always end().

    struct plot_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *title_id, const ImVec2 &size = ImVec2(-1, 0),
                          const ImPlotFlags flags = 0) noexcept {
            return ImPlot::BeginPlot(title_id, size, flags);
        }
        static void end() noexcept { ImPlot::EndPlot(); }
    };

    struct colormap_trait {
        static constexpr auto policy = end_policy::none; // push/pop always paired
        using storage                = std::monostate;
        static void begin(const char *name) noexcept { ImPlot::PushColormap(name); }
        static void begin(const ImPlotColormap cmap) noexcept { ImPlot::PushColormap(cmap); }
        static void end() noexcept { ImPlot::PopColormap(); }
    };

    struct plot_style_color_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin(const ImPlotCol idx, const ImVec4 &col) noexcept { ImPlot::PushStyleColor(idx, col); }
        static void begin(const ImPlotCol idx, const ImU32 col) noexcept { ImPlot::PushStyleColor(idx, col); }
        static void end() noexcept { ImPlot::PopStyleColor(); }
    };

    struct plot_style_var_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin(const ImPlotStyleVar idx, const float val) noexcept { ImPlot::PushStyleVar(idx, val); }
        static void begin(const ImPlotStyleVar idx, const ImVec2 &val) noexcept { ImPlot::PushStyleVar(idx, val); }
        static void begin(const ImPlotStyleVar idx, const int val) noexcept { ImPlot::PushStyleVar(idx, val); }
        static void end() noexcept { ImPlot::PopStyleVar(); }
    };

    struct subplot_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *title_id, const int rows, const int cols, const ImVec2 &size = ImVec2(-1, 0),
                          const ImPlotSubplotFlags flags = 0, float *row_ratios = nullptr,
                          float *col_ratios = nullptr) noexcept {
            return ImPlot::BeginSubplots(title_id, rows, cols, size, flags, row_ratios, col_ratios);
        }
        static void end() noexcept { ImPlot::EndSubplots(); }
    };

    // Manual traits — default parameters prevent clean NTTP function pointer resolution
    struct aligned_plots_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *group_id, const bool vertical = true) noexcept {
            return ImPlot::BeginAlignedPlots(group_id, vertical);
        }
        static void end() noexcept { ImPlot::EndAlignedPlots(); }
    };

    struct legend_popup_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *label_id, const ImGuiMouseButton mouse_button = ImGuiMouseButton_Right) noexcept {
            return ImPlot::BeginLegendPopup(label_id, mouse_button);
        }
        static void end() noexcept { ImPlot::EndLegendPopup(); }
    };

    struct plot_clip_rect_trait {
        static constexpr auto policy = end_policy::none;
        using storage                = std::monostate;
        static void begin(const float expand = 0) noexcept { ImPlot::PushPlotClipRect(expand); }
        static void end() noexcept { ImPlot::PopPlotClipRect(); }
    };

    // Drag-and-drop target traits
    using drag_drop_target_plot_trait =
        simple_trait<end_policy::conditional, &ImPlot::BeginDragDropTargetPlot, &ImPlot::EndDragDropTarget>;
    using drag_drop_target_axis_trait =
        simple_trait<end_policy::conditional, &ImPlot::BeginDragDropTargetAxis, &ImPlot::EndDragDropTarget>;
    using drag_drop_target_legend_trait =
        simple_trait<end_policy::conditional, &ImPlot::BeginDragDropTargetLegend, &ImPlot::EndDragDropTarget>;

    // Drag-and-drop source traits — default parameters prevent clean NTTP function pointer resolution
    struct drag_drop_source_plot_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const ImGuiDragDropFlags flags = 0) noexcept {
            return ImPlot::BeginDragDropSourcePlot(flags);
        }
        static void end() noexcept { ImPlot::EndDragDropSource(); }
    };

    struct drag_drop_source_axis_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const ImAxis axis, const ImGuiDragDropFlags flags = 0) noexcept {
            return ImPlot::BeginDragDropSourceAxis(axis, flags);
        }
        static void end() noexcept { ImPlot::EndDragDropSource(); }
    };

    struct drag_drop_source_item_trait {
        static constexpr auto policy = end_policy::conditional;
        using storage                = std::monostate;
        static bool begin(const char *label_id, const ImGuiDragDropFlags flags = 0) noexcept {
            return ImPlot::BeginDragDropSourceItem(label_id, flags);
        }
        static void end() noexcept { ImPlot::EndDragDropSource(); }
    };

    // Multi-push entry types
    struct plot_style_var_entry {
        ImPlotStyleVar                   idx{};
        std::variant<float, ImVec2, int> val;
    };

    struct plot_style_color_entry {
        ImPlotCol idx{};
        ImVec4    val;
    };

    // Helper lambdas for multi_push template instantiation
    constexpr auto push_plot_style_var_fn   = [](ImPlotStyleVar idx, const auto &v) { ImPlot::PushStyleVar(idx, v); };
    constexpr auto pop_plot_style_var_fn    = [](const int count) { ImPlot::PopStyleVar(count); };
    constexpr auto push_plot_style_color_fn = [](ImPlotCol idx, const auto &v) { ImPlot::PushStyleColor(idx, v); };
    constexpr auto pop_plot_style_color_fn  = [](const int count) { ImPlot::PopStyleColor(count); };

    // Type aliases for direct use: if (implot::plot p{"Title"}) { ... }
    using plot             = raii_scope<plot_trait>;
    using colormap         = raii_scope<colormap_trait>;
    using plot_style_color = raii_scope<plot_style_color_trait>;
    using plot_style_var   = raii_scope<plot_style_var_trait>;
    using subplots         = raii_scope<subplot_trait>;
    using aligned_plots    = raii_scope<aligned_plots_trait>;
    using legend_popup     = raii_scope<legend_popup_trait>;
    using plot_clip_rect   = raii_scope<plot_clip_rect_trait>;

    using drag_drop_target_plot   = raii_scope<drag_drop_target_plot_trait>;
    using drag_drop_target_axis   = raii_scope<drag_drop_target_axis_trait>;
    using drag_drop_target_legend = raii_scope<drag_drop_target_legend_trait>;
    using drag_drop_source_plot   = raii_scope<drag_drop_source_plot_trait>;
    using drag_drop_source_axis   = raii_scope<drag_drop_source_axis_trait>;
    using drag_drop_source_item   = raii_scope<drag_drop_source_item_trait>;

    // Push multiple plot style vars/colors in one shot; pops all in destructor.
    using plot_style_vars   = multi_push<plot_style_var_entry, push_plot_style_var_fn, pop_plot_style_var_fn>;
    using plot_style_colors = multi_push<plot_style_color_entry, push_plot_style_color_fn, pop_plot_style_color_fn>;

    // RAII wrapper for ImPlot context lifetime
    class [[nodiscard]] context {
    public:
        context() noexcept { ImPlot::CreateContext(); }
        ~context() noexcept { ImPlot::DestroyContext(); }
        context(const context &)            = delete;
        context &operator=(const context &) = delete;
        context(context &&)                 = delete;
        context &operator=(context &&)      = delete;
    };

} // namespace imgui_util::implot
