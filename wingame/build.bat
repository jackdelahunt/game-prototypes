@echo off

if not exist build mkdir build

set out_flags=/Fo.\build\ /Fe.\build\game.exe
set compile_flags=/Zi /Od /std:c++20
set link_flags=/DEBUG:FULL /PDB:.\build\
set windows_libs=

cl %compile_flags% .\src\main_win32.cpp .\src\game.cpp %windows_libs% %out_flags% /link %link_flags%
if %errorlevel% neq 0 exit /b %errorlevel%

.\build\game.exe
