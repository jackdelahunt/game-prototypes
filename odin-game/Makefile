run:
	./sokol-tools-bin/bin/osx_arm64/sokol-shdc -i shaders/quad_shader.glsl -o shaders/quad_shader.odin -l metal_macos -f sokol_odin
	odin build main.odin -file -debug -o:none -out:bin/entry
	./bin/entry

clean:
	rm -rf bin
	mkdir bin
	rm -f shaders/*.odin
