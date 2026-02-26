find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

# GLFW + OpenGL3 backend (not part of the core library)
add_library(imgui_backend STATIC
    ${imgui_src_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_src_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_backend PUBLIC ${imgui_src_SOURCE_DIR}/backends)
target_link_libraries(imgui_backend PUBLIC imgui glfw OpenGL::GL)
target_compile_options(imgui_backend PRIVATE -w)
set_target_properties(imgui_backend PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

add_executable(imgui_util_demo demo.cpp)
target_link_libraries(imgui_util_demo PRIVATE imgui_util imgui_backend)
