@echo off

set msbuild_flags=-m /p:Configuration=Debug /p:Platform=x64

.\premake5.exe vs2022

pushd build
rem MSBuild.exe %msbuild_flags% -t:game12 .\Engine.sln
MSBuild.exe %msbuild_flags% -t:meta .\Engine.sln
rem MSBuild.exe %msbuild_flags% -t:metaexample .\Engine.sln
popd
