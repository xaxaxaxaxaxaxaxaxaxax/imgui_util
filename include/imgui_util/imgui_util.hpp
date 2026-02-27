// imgui_util.hpp - umbrella header: pulls in every module except plot (optional implot dep)
//
// Usage:
//   #include <imgui_util/imgui_util.hpp>
//
// ImPlot wrappers are excluded — include "imgui_util/plot/raii.hpp" separately if needed.
// NOLINTBEGIN(misc-include-cleaner)
#pragma once
#include "imgui_util/core.hpp"
#include "imgui_util/layout/helpers.hpp"
#include "imgui_util/layout/presets.hpp"
#include "imgui_util/table/table_builder.hpp"
#include "imgui_util/theme/color_math.hpp"
#include "imgui_util/theme/dynamic_colors.hpp"
#include "imgui_util/theme/theme.hpp"
#include "imgui_util/theme/theme_manager.hpp"
#include "imgui_util/widgets.hpp"
// Note: plot/raii.hpp is intentionally excluded — ImPlot is an optional dependency.
// Include "imgui_util/plot/raii.hpp" directly if ImPlot is available in your build.
// NOLINTEND(misc-include-cleaner)
