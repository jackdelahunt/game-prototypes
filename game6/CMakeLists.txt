cmake_minimum_required(VERSION 3.15)

project(game)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_SUPPRESS_REGENERATION true)

add_executable(game6
	src/main.cpp
	src/imgui/imgui.cpp
	src/imgui/imgui_demo.cpp
	src/imgui/imgui_draw.cpp
	src/imgui/imgui_impl_glfw.cpp
	src/imgui/imgui_impl_opengl3.cpp
	src/imgui/imgui_tables.cpp
	src/imgui/imgui_widgets.cpp
)

set_property(TARGET game6 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

target_include_directories(game6 PRIVATE 
	src/imgui
	src/glfw
	src/glew/include
)

target_link_libraries(game6
	${CMAKE_SOURCE_DIR}/src/glew/lib/Release/x64/glew32s.lib
	${CMAKE_SOURCE_DIR}/src/glfw/GLFW/glfw3_mt.lib
	User32.lib
	Gdi32.lib
	Shell32.lib
	opengl32.lib
)
