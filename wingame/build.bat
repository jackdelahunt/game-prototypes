@echo off

rem del /F /S /Q .\build\
if not exist build mkdir build

pushd build

set compile_flags=/Zi /Od /std:c++20 -DDEBUG=1 -DWINDOWS=1
set link_flags=/DEBUG:FULL /SUBSYSTEM:WINDOWS 

.\bin\sokol-shdc-win.exe -i src\shaders\basic_shader.glsl -o src\shaders\basic_shader.h -l hlsl4

cl %compile_flags% ..\src\game.cpp /Fe.\game_preload.dll /LD /link %link_flags%
if %errorlevel% neq 0 exit /b %errorlevel%

cl %compile_flags% ..\src\main_win32.cpp /link %link_flags%
if %errorlevel% neq 0 exit /b %errorlevel%

popd
