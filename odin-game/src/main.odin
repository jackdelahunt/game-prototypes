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

SCREEN_WIDTH :: 800
SCREEN_HEIGHT :: 600
MAX_ENTITIES :: 32

Mat4 :: linalg.Matrix4f32
Vector2 :: [2]f32
Vector3 :: [3]f32

state : State = {}

State :: struct {
    // game
    camera: struct {
	view: Mat4,
	projection: Mat4
    },
    entities: [MAX_ENTITIES]Entity,
    entity_count: uint,

    // render
    pipeline: sg.Pipeline,
    bindings: sg.Bindings,
    pass_action: sg.Pass_Action
}

Entity :: struct {
    position: Vector2,
    size: Vector2,
    rotation: f32
}


main :: proc() {
    { // init state
	state.camera.view = view_matrix_from_position({0, 0})
	create_entity({0, 0}, {2, 1})
	create_entity({0, 1}, {3, 0.5})
    }

    sapp.run({
	init_cb = init_sokol,
	frame_cb = frame,
	cleanup_cb = cleanup,
	width = SCREEN_WIDTH,
	height = SCREEN_HEIGHT,
	window_title = "sokol window",
	icon = { sokol_default = true },
	logger = { func = slog.func },
    })
}

init_sokol :: proc "c" () {
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

    sg.begin_pass({action = state.pass_action, swapchain = sglue.swapchain()})

    sg.apply_pipeline(state.pipeline)
    sg.apply_bindings(state.bindings)
    
    for i in 0..<state.entity_count {
	draw_entity(&state.entities[i])
    }

    sg.end_pass()
    sg.commit()
}

cleanup :: proc "c" () {
    sg.shutdown()
}

create_entity :: proc(position: Vector2, size: Vector2, rotation: f32 = 0) {
    state.entities[state.entity_count] = Entity{position = position, size = size, rotation = rotation}
    state.entity_count += 1
}

draw_entity :: proc(entity: ^Entity) {
    width := sapp.widthf()
    height := sapp.heightf()

    model_matrix := translate_matrix(entity.position) * rotate_matrix(entity.rotation) * scale_matrix(entity.size)
    view_matrix := view_matrix_from_position({0, 0})
    projection_matrix := linalg.matrix4_perspective_f32(90, width/height, 0, 2)
 
    model_view_projection := projection_matrix * view_matrix * model_matrix
 
    // set the mvp matrix so it can be used in the shader 
    // and passed as a uniform
    vs_params: shaders.Vs_Params
    vs_params.mvp = linalg.matrix_flatten(model_view_projection)

    sg.apply_uniforms(shaders.UB_vs_params, {ptr = &vs_params, size = size_of(vs_params)})
    sg.draw(0, 6, 1)
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









