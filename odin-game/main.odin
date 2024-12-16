package entry

import "core:fmt"
import "core:log"
import "base:runtime"
import "core:c"

import sg "sokol/gfx"
import sapp "sokol/app"
import slog "sokol/log"
import sglue "sokol/glue"

import shaders "shaders"

SCREEN_WIDTH :: 640
SCREEN_HEIGHT :: 400

State :: struct {
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

range :: proc(buffer: []$T) -> sg.Range {
    return sg.Range{
	ptr = &buffer[0],
	size = len(buffer) * size_of(T)
    }
}

init :: proc "c" () {
    context = runtime.default_context()
    sg.setup({
	environment = sglue.environment(),
	logger = { func = slog.func },
    })

    vertices := [3 * 3 + 3 * 4]f32 {
	// positions            // colors
        0.0,  0.5, 0.5,		1.0, 0.0, 0.0, 1.0,
        0.5, -0.5, 0.5,		0.0, 1.0, 0.0, 1.0,
        -0.5, -0.5, 0.5,	0.0, 0.0, 1.0, 1.0
    }

    state.bindings.vertex_buffers[0] = sg.make_buffer({
	data = range(vertices[0:]),
	label = "triangle-vertices"
    })

    shader := sg.make_shader(shaders.triangle_shader_desc(sg.query_backend()))

    state.pipeline = sg.make_pipeline({
	shader = shader,
	layout = {
	    attrs = {
		shaders.ATTR_triangle_position = { format = .FLOAT3 },
		shaders.ATTR_triangle_color0 = { format = .FLOAT4 }
	    },
	},
	label = "triangle-pipeline"
    })

    state.pass_action = {
        colors = {
	    0 = { load_action = .CLEAR, clear_value = { 0.5, 0.5, 0.5, 1 } },
        }
    }
}

frame :: proc "c" () {
    sg.begin_pass({action = state.pass_action, swapchain = sglue.swapchain()})

    {
	sg.apply_pipeline(state.pipeline)
	sg.apply_bindings(state.bindings)
	sg.draw(0, 3, 1)
    }

    sg.end_pass()
    sg.commit()
}

cleanup :: proc "c" () {
    sg.shutdown()
}












