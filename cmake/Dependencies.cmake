include(FetchContent)

# Dependencies that lack a usable CMakeLists.txt use SOURCE_SUBDIR pointing to
# a non-existent directory so that FetchContent_MakeAvailable downloads the
# source without calling add_subdirectory.  We then create the targets manually.

# =============================================================================
# imgui
# =============================================================================

if(NOT TARGET imgui)
    FetchContent_Declare(
        imgui_src
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.8-docking
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  _unused
    )
    FetchContent_MakeAvailable(imgui_src)

    add_library(imgui STATIC
        ${imgui_src_SOURCE_DIR}/imgui.cpp
        ${imgui_src_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_src_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_src_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_src_SOURCE_DIR}/imgui_widgets.cpp
    )
    target_include_directories(imgui PUBLIC ${imgui_src_SOURCE_DIR})
    target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
    target_compile_options(imgui PRIVATE -w)
    set_target_properties(imgui PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
endif()

# =============================================================================
# imnodes
# =============================================================================

if(NOT TARGET imnodes)
    FetchContent_Declare(
        imnodes_src
        GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
        GIT_TAG        b2ec254
        GIT_SUBMODULES ""
        SOURCE_SUBDIR  _unused
    )
    FetchContent_MakeAvailable(imnodes_src)

    add_library(imnodes STATIC
        ${imnodes_src_SOURCE_DIR}/imnodes.cpp
    )
    target_include_directories(imnodes PUBLIC ${imnodes_src_SOURCE_DIR})
    target_link_libraries(imnodes PUBLIC imgui)
    target_compile_options(imnodes PRIVATE -w)
    set_target_properties(imnodes PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
endif()

# =============================================================================
# implot
# =============================================================================

if(NOT TARGET implot)
    FetchContent_Declare(
        implot_src
        GIT_REPOSITORY https://github.com/epezent/implot.git
        GIT_TAG        v0.16
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  _unused
    )
    FetchContent_MakeAvailable(implot_src)

    add_library(implot STATIC
        ${implot_src_SOURCE_DIR}/implot.cpp
        ${implot_src_SOURCE_DIR}/implot_items.cpp
    )
    target_include_directories(implot PUBLIC ${implot_src_SOURCE_DIR})
    target_link_libraries(implot PUBLIC imgui)
    target_compile_options(implot PRIVATE -w)
    set_target_properties(implot PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
endif()

# =============================================================================
# log.h — zero-allocation structured logging
# =============================================================================

if(NOT TARGET logh)
    FetchContent_Declare(
        logh_src
        GIT_REPOSITORY https://github.com/xaxaxaxaxaxaxaxaxaxax/log.h.git
        GIT_TAG        5309659
        SOURCE_SUBDIR  _unused
    )
    FetchContent_MakeAvailable(logh_src)

    add_library(logh INTERFACE)
    target_include_directories(logh INTERFACE ${logh_src_SOURCE_DIR})
endif()

# =============================================================================
# GoogleTest (only when building tests)
# =============================================================================

if(IMGUI_UTIL_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    # Suppress warnings from googletest sources (global -Wall leaks into fetched targets)
    target_compile_options(gtest PRIVATE -w)
    target_compile_options(gtest_main PRIVATE -w)
    target_compile_options(gmock PRIVATE -w)
    target_compile_options(gmock_main PRIVATE -w)
    set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
endif()
