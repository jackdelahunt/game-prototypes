@echo off

.\bin\sokol-shdc-win.exe -i src\shaders\basic_shader.glsl -o src\shaders\basic_shader.h -l hlsl4

if not exist build mkdir build

pushd build

set compile_flags=/std:c++20 /Zi /Od  -DDEBUG=1 -DWINDOWS=1
set link_flags=/DEBUG:FULL /SUBSYSTEM:WINDOWS
set extra_flags=/W3 /diagnostics:color /diagnostics:caret

cl %compile_flags% %extra_flags% ..\src\game.cpp /Fegame.exe /link %link_flags%
if %errorlevel% neq 0 exit /b %errorlevel%

popd
