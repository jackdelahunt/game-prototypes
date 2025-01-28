@echo off

if not exist build mkdir build

pushd build

set compile_flags=/std:c++20 /MT /Zi /Od /diagnostics:color /diagnostics:caret
set link_flags=/DEBUG:FULL /SUBSYSTEM:CONSOLE

set windows_libs=User32.lib Gdi32.lib Shell32.lib

set includes=/I..\src\imgui /I..\src\glfw

set libs=..\src\glfw\GLFW\glfw3_mt.lib ..\src\imgui\backends\imgui_impl_glfw.cpp ..\src\imgui\backends\imgui_impl_opengl3.cpp ..\src\imgui\imgui*.cpp 


cl %compile_flags% %includes% ..\src\main.cpp %libs% %windows_libs% /Fegame6.exe /link %link_flags%
if %errorlevel% neq 0 exit /b %errorlevel%

popd
