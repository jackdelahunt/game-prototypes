run:
	./bin/sokol-shdc-macos-arm -i shaders/quad_shader.glsl -o shaders/quad_shader.odin -l metal_macos -f sokol_odin
	odin build main.odin -file -debug -o:none -out:build/entry
	./build/entry

clean:
	rm -rf build
	mkdir build
	rm -f shaders/*.odin
