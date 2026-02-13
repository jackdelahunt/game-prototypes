@echo off

set msbuild_flags=-m /p:Configuration=Debug /p:Platform=x64

.\premake5.exe vs2022

pushd build
MSBuild.exe %msbuild_flags% -t:game13 .\Engine.sln
popd
