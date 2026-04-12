@echo off

set msbuild_flags=-m /p:Configuration=Debug /p:Platform=x64

.\premake5.exe vs2026

pushd build
MSBuild.exe %msbuild_flags% -t:meta .\Engine.sln
popd

.\build\bin\debug\meta.exe .\src\main.cpp .\src\type_info.h

pushd build
MSBuild.exe %msbuild_flags% -t:game12 .\Engine.sln
popd
