include(FetchContent)

# Wrap FetchContent_Populate calls to suppress CMP0169 deprecation warning.
# We use Populate (not MakeAvailable) because these deps lack usable CMakeLists.txt.
macro(fetch_populate name)
    cmake_policy(SET CMP0169 OLD)
    FetchContent_Populate(${name})
    cmake_policy(SET CMP0169 NEW)
endmacro()

# Use FetchContent_Populate (not MakeAvailable) for all three because none
# ship a usable CMakeLists.txt — we create the targets ourselves.  The _src
# suffix on FetchContent names avoids clashing with the target names.

# =============================================================================
# imgui
# =============================================================================

if(NOT TARGET imgui)
    FetchContent_Declare(
        imgui_src
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.8-docking
        GIT_SHALLOW    TRUE
    )
    fetch_populate(imgui_src)

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
    )
    fetch_populate(imnodes_src)

    add_library(imnodes STATIC
        ${imnodes_src_SOURCE_DIR}/imnodes.cpp
    )
    target_include_directories(imnodes PUBLIC ${imnodes_src_SOURCE_DIR})
    target_link_libraries(imnodes PUBLIC imgui)
    target_compile_options(imnodes PRIVATE -w)
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
    )
    fetch_populate(implot_src)

    add_library(implot STATIC
        ${implot_src_SOURCE_DIR}/implot.cpp
        ${implot_src_SOURCE_DIR}/implot_items.cpp
    )
    target_include_directories(implot PUBLIC ${implot_src_SOURCE_DIR})
    target_link_libraries(implot PUBLIC imgui)
    target_compile_options(implot PRIVATE -w)
endif()

# =============================================================================
# log.h — zero-allocation structured logging
# =============================================================================

if(NOT TARGET logh)
    FetchContent_Declare(
        logh_src
        GIT_REPOSITORY https://github.com/xaxaxaxaxaxaxaxaxaxax/log.h.git
        GIT_TAG        a620bdd4d1ccbf89977d2dec72f1175b7d6f96db
    )
    fetch_populate(logh_src)

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
endif()
