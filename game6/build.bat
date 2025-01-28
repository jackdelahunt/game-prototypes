@echo off

if not exist build mkdir build

pushd build

set compile_flags=/std:c++20 /MT /Zi /Od /diagnostics:color /diagnostics:caret
set link_flags=/DEBUG:FULL /SUBSYSTEM:CONSOLE
set libs=..\src\glfw\glfw3_mt.lib User32.lib Gdi32.lib Shell32.lib

cl %compile_flags% ..\src\main.cpp %libs% /Fegame6.exe /link %link_flags%
if %errorlevel% neq 0 exit /b %errorlevel%

popd
