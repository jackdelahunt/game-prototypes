@echo off

if not exist build mkdir build

set out_flags=/Fo.\build\ /Fe.\build\game.exe
set compile_flags=/Zi /Od
set link_flags=/DEBUG:FULL /PDB:.\build\
set windows_libs=user32.lib Gdi32.lib

cl %compile_flags% .\src\main.cpp %windows_libs% %out_flags% /link %link_flags%
if %errorlevel% neq 0 exit /b %errorlevel%

.\build\game.exe
