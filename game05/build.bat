@echo off

if not exist build mkdir build

odin build src -debug -show-timings -o:none -extra-linker-flags:"/DEBUG:FULL" -out:build\game5.exe

if %errorlevel% neq 0 exit /b %errorlevel%

if not exist release mkdir release

odin build src -show-timings -o:speed -out:release\game5.exe
if %errorlevel% neq 0 exit /b %errorlevel%
