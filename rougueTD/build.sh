set -e

./bin/sokol-shdc-macos-arm -i src/shaders/basic_shader.glsl -o src/shaders/basic_shader.odin -l metal_macos -f sokol_odin
odin build src -debug -o:none -out:build/entry.exe
./build/entry.exe
