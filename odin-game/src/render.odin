package src

import "core:fmt"
import "core:log"
import "base:runtime"
import "core:math/linalg"

import sg "sokol/gfx"
import sapp "sokol/app"
import slog "sokol/log"
import sglue "sokol/glue"

import shaders "shaders"

Mat4 :: linalg.Matrix4f32
Vector2 :: [2]f32
Vector3 :: [3]f32
Vector4 :: [4]f32

Vertex :: struct {
    position: Vector3,
    colour: Vector4
}

Quad :: [4]Vertex

renderer_init :: proc "c" () {
    context = runtime.default_context()

    sg.setup({
	environment = sglue.environment(),
	logger = { func = slog.func },
    })

    // create vertext buffer
    state.bindings.vertex_buffers[0] = sg.make_buffer({
	usage = .DYNAMIC,
	size = size_of(Quad) * len(state.quads),
	label = "quad-vertices"
    })

    // create index buffer
    index_buffer: [len(state.quads) * 6]u16
    i := 0;
    for i < len(index_buffer) {
	// vertex offset pattern to draw a quad
	// { 0, 1, 2,  0, 2, 3 }
	index_buffer[i + 0] = auto_cast ((i/6)*4 + 0)
	index_buffer[i + 1] = auto_cast ((i/6)*4 + 1)
	index_buffer[i + 2] = auto_cast ((i/6)*4 + 2)
	index_buffer[i + 3] = auto_cast ((i/6)*4 + 0)
	index_buffer[i + 4] = auto_cast ((i/6)*4 + 2)
	index_buffer[i + 5] = auto_cast ((i/6)*4 + 3)
	i += 6;
    }

    state.bindings.index_buffer = sg.make_buffer({
	type = .INDEXBUFFER,
	data = { ptr = &index_buffer, size = size_of(index_buffer) },
	label = "quad-indices"
    })

    shader := sg.make_shader(shaders.basic_shader_desc(sg.query_backend()))

    state.render_pipeline = sg.make_pipeline({
	shader = shader,
	index_type = .UINT16,
	layout = {
	    attrs = {
		shaders.ATTR_basic_position = { format = .FLOAT3 },
		shaders.ATTR_basic_color0 = { format = .FLOAT4 }
	    },
	},
	label = "quad-pipeline"
    })

    state.pass_action = {
        colors = {
	    0 = { load_action = .CLEAR, clear_value = { 0.5, 0.5, 0.5, 1 } },
        }
    }
}

renderer_frame :: proc "c" () {
    context = runtime.default_context()

    // reset quad data for this frame
    runtime.mem_zero(&state.quads, size_of(Quad) * len(state.quads))
    state.quad_count = 0

    // let the game create and set any quads it wants for this frame
    frame()

    // update vertex buffer with new quad data for this frame
    sg.update_buffer(
	state.bindings.vertex_buffers[0],
	{ ptr = &state.quads[0], size = size_of(Quad) * state.quad_count }
    )

    sg.begin_pass({action = state.pass_action, swapchain = sglue.swapchain()})

    sg.apply_pipeline(state.render_pipeline)
    sg.apply_bindings(state.bindings)

    sg.draw(0, 6 * state.quad_count, 1)
    
    sg.end_pass()
    sg.commit()
}

renderer_cleanup :: proc "c" () {
    sg.shutdown()
}

range :: proc(buffer: []$T) -> sg.Range {
    return sg.Range{
	ptr = &buffer[0],
	size = len(buffer) * size_of(T)
    }
}

translate_matrix :: proc(position: Vector2) -> Mat4 {
    return linalg.matrix4_translate_f32({position.x, position.y, 0})
}

scale_matrix :: proc(scale: Vector2) -> Mat4 {
	return linalg.matrix4_scale_f32(Vector3{scale.x, scale.y, 1});
}

rotate_matrix :: proc(radians: f32) -> Mat4 {
	return linalg.matrix4_rotate_f32(radians, Vector3{0, 0, 1});
}

view_matrix_from_position :: proc(position: Vector2) -> Mat4 {
    return linalg.matrix4_look_at_f32({position.x, position.y, 1}, {position.x, position.y, 0}, {0, 1, 0})
}
