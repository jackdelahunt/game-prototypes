package entry

import "core:fmt"
import "core:log"
import "base:runtime"
import "core:math/linalg"

import sg "sokol/gfx"
import sapp "sokol/app"
import slog "sokol/log"
import sglue "sokol/glue"

import shaders "shaders"

SCREEN_WIDTH :: 640
SCREEN_HEIGHT :: 400

Mat4 :: linalg.Matrix4f32
Vector2 :: [2]f32
Vector3 :: [3]f32

State :: struct {
    quad_position: Vector2,
    camera_position: Vector2,
    pipeline: sg.Pipeline,
    bindings: sg.Bindings,
    pass_action: sg.Pass_Action
}

state : State = {}

main :: proc() {
    sapp.run({
	init_cb = init,
	frame_cb = frame,
	cleanup_cb = cleanup,
	width = SCREEN_WIDTH,
	height = SCREEN_HEIGHT,
	window_title = "sokol window",
	icon = { sokol_default = true },
	logger = { func = slog.func },
    })
}

init :: proc "c" () {
    context = runtime.default_context()

    sg.setup({
	environment = sglue.environment(),
	logger = { func = slog.func },
    })

    { // things on screen we see
	state.quad_position = {0, 0}
	state.camera_position = {0, 0}
    }

    vertices := [4 * 3 + 4 * 4]f32 {
	// positions            // colors
	-0.5,  0.5, 0.5,	1.0, 0.0, 0.0, 1.0,
         0.5,  0.5, 0.5,	0.0, 1.0, 0.0, 1.0,
         0.5, -0.5, 0.5,	0.0, 0.0, 1.0, 1.0,
        -0.5, -0.5, 0.5,	1.0, 1.0, 0.0, 1.0,
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

    state.pipeline = sg.make_pipeline({
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

frame :: proc "c" () {
    context = runtime.default_context()

    // matrix math to get the model view projection matrix
    // local space -> world space
    model_matrix := vector_to_matrix(state.quad_position)

    // world space -> view (camera) space
    view_matrix := view_matrix_from_camera_position(state.camera_position)

    state.quad_position.x += 0.1
    state.camera_position.x += 0.1
 
    // view space -> view space with projection
    projection_matrix :: Mat4(1)

    model_view_projection := projection_matrix * view_matrix * model_matrix

    // set the mvp matrix so it can be used in the shader 
    // and passed as a uniform
    vs_params: shaders.Vs_Params
    vs_params.mvp = linalg.matrix_flatten(model_view_projection)

    sg.begin_pass({action = state.pass_action, swapchain = sglue.swapchain()})

    {
	sg.apply_pipeline(state.pipeline)
	sg.apply_bindings(state.bindings)
	sg.apply_uniforms(shaders.UB_vs_params, {ptr = &vs_params, size = size_of(vs_params)})
	sg.draw(0, 6, 1)
    }

    sg.end_pass()
    sg.commit()
}

cleanup :: proc "c" () {
    sg.shutdown()
}

range :: proc(buffer: []$T) -> sg.Range {
    return sg.Range{
	ptr = &buffer[0],
	size = len(buffer) * size_of(T)
    }
}

vector_to_matrix :: proc(position: Vector2) -> Mat4 {
    mat := Mat4(1)
    mat *= linalg.matrix4_translate_f32({position.x, position.y, 0})
    return mat
}

view_matrix_from_camera_position :: proc(position: Vector2) -> Mat4 {
    return linalg.matrix4_look_at_f32({position.x, position.y, 0}, {position.x, position.y, -1}, {0, 1, 0}, false)
}












