@echo off

if not exist build mkdir build

pushd build

set msbuild_flags=-m /p:Configuration=Debug /p:Platform=x64

cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
MSBuild.exe %msbuild_flags% -t:game6 .\game.sln

popd
