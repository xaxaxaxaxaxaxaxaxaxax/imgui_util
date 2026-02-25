add_library(imgui_util STATIC
    theme.cpp
    theme_manager.cpp
)

target_include_directories(imgui_util PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(imgui_util PUBLIC
    imgui
    imnodes
    implot
    logh
)

target_compile_features(imgui_util PUBLIC cxx_std_26)
target_compile_options(imgui_util PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)

# Register headers so IDEs and clangd index them
target_sources(imgui_util PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
        ${PROJECT_SOURCE_DIR}/include/imgui_util/core/error.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/core/fmt_buf.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/core/parse.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/core/raii.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/core/raii_internal.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/plot/raii.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/layout/presets.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/table/table_builder.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/theme/color_math.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/theme/dynamic_colors.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/theme/theme.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/theme/theme_manager.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets/controls.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets/helpers.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets/log_viewer.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets/modal_builder.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets/property_editor.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets/search_bar.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets/spinner.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets/text.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets/toast.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets/toggle.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/core.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/widgets.hpp
        ${PROJECT_SOURCE_DIR}/include/imgui_util/imgui_util.hpp
)
