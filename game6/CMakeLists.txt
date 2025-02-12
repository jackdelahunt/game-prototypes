cmake_minimum_required(VERSION 3.15)

project(game)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_SUPPRESS_REGENERATION true)  # remove weird ZERO_CHECK target
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(game6
	src/main.cpp
	libs/imgui/imgui.cpp
	libs/imgui/imgui_demo.cpp
	libs/imgui/imgui_draw.cpp
	libs/imgui/imgui_impl_glfw.cpp
	libs/imgui/imgui_impl_opengl3.cpp
	libs/imgui/imgui_tables.cpp
	libs/imgui/imgui_widgets.cpp
)

set_property(TARGET game6 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}") # set working dir in VS

target_include_directories(game6 PRIVATE
	libs
	libs/glew/include
)

target_link_libraries(game6
	${CMAKE_SOURCE_DIR}/libs/glew/lib/Release/x64/glew32s.lib
	${CMAKE_SOURCE_DIR}/libs/glfw/glfw3_mt.lib
	User32.lib
	Gdi32.lib
	Shell32.lib
	opengl32.lib
)
