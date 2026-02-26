cmake_minimum_required(VERSION 3.30)
project(imgui_util LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

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
