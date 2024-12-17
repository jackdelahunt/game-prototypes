run:
	./bin/sokol-shdc-macos-arm -i src/shaders/quad_shader.glsl -o src/shaders/quad_shader.odin -l metal_macos -f sokol_odin
	odin build src -debug -o:none -out:build/entry.exe
	./build/entry.exe

clean:
	rm -rf build
	mkdir build
	rm -f src/shaders/*.odin
