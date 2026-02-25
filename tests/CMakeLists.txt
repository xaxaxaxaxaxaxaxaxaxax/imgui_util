# test_raii: compile-time property tests (no ImGui context needed)
add_executable(test_raii test_raii.cpp)
target_link_libraries(test_raii PRIVATE imgui_util GTest::gtest_main)
add_test(NAME test_raii COMMAND test_raii)

# test_fmt_buf: formatting and buffer tests
add_executable(test_fmt_buf test_fmt_buf.cpp)
target_link_libraries(test_fmt_buf PRIVATE imgui_util GTest::gtest_main)
add_test(NAME test_fmt_buf COMMAND test_fmt_buf)

# test_parse: parsing utility tests
add_executable(test_parse test_parse.cpp)
target_link_libraries(test_parse PRIVATE imgui_util GTest::gtest_main)
add_test(NAME test_parse COMMAND test_parse)

# test_table_builder: compile-time table builder property tests
add_executable(test_table_builder test_table_builder.cpp)
target_link_libraries(test_table_builder PRIVATE imgui_util GTest::gtest_main)
add_test(NAME test_table_builder COMMAND test_table_builder)

# test_search: search bar and string matching tests
add_executable(test_search test_search.cpp)
target_link_libraries(test_search PRIVATE imgui_util GTest::gtest_main)
add_test(NAME test_search COMMAND test_search)

# test_presets: layout preset flags and plot multi-push type tests
add_executable(test_presets test_presets.cpp)
target_link_libraries(test_presets PRIVATE imgui_util GTest::gtest_main)
add_test(NAME test_presets COMMAND test_presets)

# test_plot_raii: compile-time property tests for plot RAII wrappers
add_executable(test_plot_raii test_plot_raii.cpp)
target_link_libraries(test_plot_raii PRIVATE imgui_util GTest::gtest_main)
add_test(NAME test_plot_raii COMMAND test_plot_raii)

# test_color_math: rgb_color arithmetic, conversions, round-trips
add_executable(test_color_math test_color_math.cpp)
target_link_libraries(test_color_math PRIVATE imgui_util GTest::gtest_main)
add_test(NAME test_color_math COMMAND test_color_math)

# test_theme_constexpr: lerp_vec4, from_preset_core validation
add_executable(test_theme_constexpr test_theme_constexpr.cpp)
target_link_libraries(test_theme_constexpr PRIVATE imgui_util GTest::gtest_main)
add_test(NAME test_theme_constexpr COMMAND test_theme_constexpr)

# test_text: brighten, linear_fade_alpha, color palette, truncated_text
add_executable(test_text test_text.cpp)
target_link_libraries(test_text PRIVATE imgui_util GTest::gtest_main)
add_test(NAME test_text COMMAND test_text)
