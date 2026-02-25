cmake_minimum_required(VERSION 3.25)
project(imgui_util LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ON)

# Warning flags are set per-target in src/CMakeLists.txt

option(IMGUI_UTIL_BUILD_TESTS "Build imgui_util tests" OFF)
option(IMGUI_UTIL_BUILD_EXAMPLES "Build imgui_util examples" OFF)

include(cmake/Dependencies.cmake)

add_subdirectory(src)

if(IMGUI_UTIL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(IMGUI_UTIL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
