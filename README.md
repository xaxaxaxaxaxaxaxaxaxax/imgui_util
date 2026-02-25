# imgui_util

c++26 utility library for dear imgui. theme engine, fluent table builder, raii wrappers, widgets. single static lib, fetches all deps via cmake.

needs c++26 (clang 19+ / gcc 15+) and cmake 3.25+.

## demo

![demo](demo.png)

theme editor with community presets (gruvbox, dracula, nord, catppuccin, solarized) that auto-adapt to light/dark mode. process table from /proc with multi-column sort, ctrl/shift selection, virtual clipping.

## build

```
cmake -B build -DIMGUI_UTIL_BUILD_EXAMPLES=ON
cmake --build build
./build/examples/imgui_util_demo
```

## test

```
cmake -B build -DIMGUI_UTIL_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## use

```cmake
include(FetchContent)
FetchContent_Declare(imgui_util
    GIT_REPOSITORY https://github.com/xaxaxaxaxaxaxaxaxaxax/imgui_util.git
    GIT_TAG        main)
FetchContent_MakeAvailable(imgui_util)
target_link_libraries(your_app PRIVATE imgui_util)
```

## modules

- **theme** — full style editor, preset derivation model, dark/light mode, lerp transitions, save/load
- **table** — `table_builder<RowT>` with compile-time columns, multi-sort, selection, filtering, clipping
- **core** — raii scope guards for imgui/implot begin/end, `fmt_buf`, parsing
- **widgets** — property editor, log viewer, search bar, controls
- **layout** — docking layout presets
- **plot** — raii wrappers for implot

## deps

all fetched automatically. imgui v1.91.8-docking, imnodes, implot v0.16, log.h. tests use googletest.
