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

renderer_init :: proc "c" () {
    context = runtime.default_context()

    sg.setup({
	environment = sglue.environment(),
	logger = { func = slog.func },
    })

    vertices := [4 * 3 + 4 * 4]f32 {
	// positions            // colors
	-0.5,  0.5, 0,		1.0, 0.0, 0.0, 1.0,
         0.5,  0.5, 0,		0.0, 1.0, 0.0, 1.0,
         0.5, -0.5, 0,		0.0, 0.0, 1.0, 1.0,
        -0.5, -0.5, 0,		1.0, 1.0, 0.0, 1.0,
    }

    state.bindings.vertex_buffers[0] = sg.make_buffer({
	type = .VERTEXBUFFER,
	data = range(vertices[0:]),
	label = "quad-vertices"
    })

    indices := [3 * 2]u16 {
	0, 1, 2,
	0, 2, 3
    }

    state.bindings.index_buffer = sg.make_buffer({
	type = .INDEXBUFFER,
	data = range(indices[0:]),
	label = "quad-indices"
    })

    shader := sg.make_shader(shaders.quad_shader_desc(sg.query_backend()))

    state.render_pipeline = sg.make_pipeline({
	shader = shader,
	index_type = .UINT16,
	layout = {
	    attrs = {
		shaders.ATTR_quad_position = { format = .FLOAT3 },
		shaders.ATTR_quad_color0 = { format = .FLOAT4 }
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

    sg.begin_pass({action = state.pass_action, swapchain = sglue.swapchain()})

    sg.apply_pipeline(state.render_pipeline)
    sg.apply_bindings(state.bindings)

    frame() 

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
    return linalg.matrix4_look_at_f32({position.x, position.y, -1}, {position.x, position.y, 0}, {0, 1, 0})
}
