cmake_minimum_required(VERSION 3.17)

project(css30 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

include_directories(./imgui)
include_directories(./imgui/backends)
include_directories(./implot)

add_executable(css30)

target_sources(css30 PRIVATE Criptocoin_Consumer_UI.cpp ./implot/implot.cpp ./implot/implot_demo.cpp ./implot/implot_items.cpp ./implot/implot_internal.h ./imgui/imgui.cpp ./imgui/imgui_demo.cpp ./imgui/imgui_draw.cpp ./imgui/backends/imgui_impl_glfw.cpp ./imgui/backends/imgui_impl_opengl3.cpp ./imgui/imgui_internal.h ./imgui/imgui_tables.cpp ./imgui/imgui_widgets.cpp ./imgui/imstb_rectpack.h ./imgui/imstb_textedit.h ./imgui/imstb_truetype.h ./imgui/misc/cpp/imgui_stdlib.cpp ./imgui/misc/cpp/imgui_stdlib.h)

target_link_libraries(css30 PRIVATE glfw rdkafka++ GLEW::GLEW ${OPENGL_LIBRARIES})