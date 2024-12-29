@echo off

del /F /S /Q .\build\
if not exist build mkdir build

set out_flags=/Fo.\build\ /Fe.\build\
set compile_flags=/Zi /Od /std:c++20 -DDEBUG=1 -DWINDOWS=1
set link_flags=/DEBUG:FULL /PDB:.\build\ /SUBSYSTEM:WINDOWS
set windows_libs=


.\bin\sokol-shdc-win.exe -i src\shaders\basic_shader.glsl -o src\shaders\basic_shader.h -l hlsl4

cl %compile_flags% .\src\main_win32.cpp %windows_libs% %out_flags% /link %link_flags%
if %errorlevel% neq 0 exit /b %errorlevel%

cl %compile_flags% .\src\game.cpp %windows_libs% %out_flags% /LD /link %link_flags%
if %errorlevel% neq 0 exit /b %errorlevel%

.\build\main_win32.exe
