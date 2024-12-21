.\bin\sokol-shdc-win.exe -i src\shaders\basic_shader.glsl -o src\shaders\basic_shader.odin -l hlsl4 -f sokol_odin

odin build src -debug -o:none -out:build\entry.exe
if %errorlevel% neq 0 exit /b %errorlevel%

.\build\entry.exe
