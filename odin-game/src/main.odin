package src

// TODO:
// - input system
// - drawing entities of any colour
// - input -> movement working
// - drawing text

import "core:fmt"
import "core:log"
import "base:runtime"
import "core:math/linalg"
import "core:time"

import sg "sokol/gfx"
import sapp "sokol/app"
import slog "sokol/log"
import sglue "sokol/glue"

import shaders "shaders"

SCREEN_WIDTH :: 800
SCREEN_HEIGHT :: 600
MAX_ENTITIES :: 32

///////////////////////////////// @state
state : State = {}

State :: struct {
    // timing
    start_time: time.Time,

    // inputs
    key_inputs: #sparse [Key]InputState,
    mouse_button_inputs: [3]InputState,
    mouse_screen_position: Vector2,

    // game state
    camera: struct {
	view: Mat4,
	projection: Mat4
    },
    entities: [MAX_ENTITIES]Entity,
    entity_count: uint,

    // renderer state
    render_pipeline: sg.Pipeline,
    bindings: sg.Bindings,
    pass_action: sg.Pass_Action
}

///////////////////////////////// @input
InputState :: enum {
    UP,
    DOWN,
}

Key :: sapp.Keycode
MouseButton :: sapp.Mousebutton

///////////////////////////////// @entity
Entity :: struct {
    position: Vector2,
    size: Vector2,
    rotation: f32
}

///////////////////////////////// @main
main :: proc() {
    { // init state
	state.start_time = time.now()

	state.camera.view = view_matrix_from_position({0, 0})
	create_entity({0, 0}, {2, 1})
	create_entity({0, 1}, {3, 0.5})
    }

    sapp.run({
	init_cb = renderer_init,
	frame_cb = renderer_frame,
	cleanup_cb = renderer_cleanup,
	event_cb = window_event_callback,
	width = SCREEN_WIDTH,
	height = SCREEN_HEIGHT,
	window_title = "sokol window",
	icon = { sokol_default = true },
	logger = { func = slog.func },
    })
}

frame :: proc() {
    update()
    draw() 
}

update :: proc() {
    if state.key_inputs[.ESCAPE] == .DOWN {
	sapp.quit()
    }
}

draw :: proc() {
    for i in 0..<state.entity_count {
	draw_entity(&state.entities[i])
    }
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

window_event_callback :: proc "c" (event: ^sapp.Event) {
    context = runtime.default_context()
    
    if event == nil {
	return
    }

    switch event.type {
    case .MOUSE_MOVE:
	state.mouse_screen_position = {event.mouse_x, event.mouse_y}
    case .MOUSE_UP: 
	fallthrough
    case .MOUSE_DOWN:
	if event.mouse_button == .INVALID {
	    log.warn("got invalid mouse button input")
	    return
	}

	state.mouse_button_inputs[event.mouse_button] = .DOWN if event.type == .MOUSE_DOWN else .UP
    case .KEY_UP: 
	fallthrough
    case .KEY_DOWN:
	if event.key_code == .INVALID {
	    log.warn("got invalid key input")
	    return
	}

	state.key_inputs[event.key_code] = .DOWN if event.type == .KEY_DOWN else .UP
    case .QUIT_REQUESTED:
	sapp.quit()
    case .INVALID, 
	 .CHAR, 
	 .MOUSE_SCROLL, .MOUSE_ENTER, .MOUSE_LEAVE, 
	 .TOUCHES_BEGAN, .TOUCHES_ENDED, .TOUCHES_MOVED, .TOUCHES_CANCELLED, 
	 .RESIZED, .ICONIFIED, .RESTORED, .FOCUSED, .UNFOCUSED, .SUSPENDED, .RESUMED, 
	 .CLIPBOARD_PASTED, .FILES_DROPPED:
    }
}









