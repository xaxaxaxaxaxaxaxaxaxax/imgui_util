file(GLOB TEST_SOURCES CONFIGURE_DEPENDS test_*.cpp)

foreach(src ${TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE imgui_util GTest::gtest_main)
    add_test(NAME ${name} COMMAND ${name})
endforeach()
