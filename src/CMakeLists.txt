file(GLOB_RECURSE IMGUI_UTIL_HEADERS
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/include/*.hpp
)

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

# Register headers so IDEs and clangd index them
target_sources(imgui_util PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES ${IMGUI_UTIL_HEADERS}
)
