@echo off

if not exist build mkdir build

pushd build

set compile_flags=/std:c++20 /MP /MT /Zi /Od /diagnostics:color /diagnostics:caret
set link_flags=/DEBUG:FULL /SUBSYSTEM:CONSOLE /INCREMENTAL

set windows_libs=User32.lib Gdi32.lib Shell32.lib opengl32.lib
set libs=..\src\libs\glfw\glfw3_mt.lib ..\src\libs\glew\lib\Release\x64\glew32s.lib ..\src\libs\imgui\imgui*.cpp ..\src\libs\miniaudio\miniaudio.c
set includes=/I..\src\libs\imgui /I..\src\libs /I..\src\libs\glew\include

cl %compile_flags% %includes% ..\src\main.cpp %libs% %windows_libs% /Fegame6.exe /link %link_flags%
if %errorlevel% neq 0 exit /b %errorlevel%

popd
