@echo off

if not exist build mkdir build

.\bin\sokol-shdc-win.exe -i src\shaders\basic_shader.glsl -o src\shaders\basic_shader.odin -l hlsl4 -f sokol_odin

odin build src -debug -show-timings -o:none -extra-linker-flags:"/DEBUG:FULL" -out:build\game4.exe
if %errorlevel% neq 0 exit /b %errorlevel%
