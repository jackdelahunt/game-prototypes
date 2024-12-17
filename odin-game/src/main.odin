package src

// TODO:
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

SCREEN_WIDTH :: 900
SCREEN_HEIGHT :: 700
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
    entities: [MAX_ENTITIES]Entity,
    entity_count: uint,

    // renderer state
    camera_position: Vector2,
    zoom: f32,
    quads: [MAX_ENTITIES]Quad,
    quad_count: uint,
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

///////////////////////////////// @colour
Colour :: distinct Vector4

Red	:: Colour{0.9, 0.05, 0.05, 1}
Green	:: Colour{0.05, 0.9, 0.05, 1}
Blue	:: Colour{0.05, 0.05, 0.9, 1}

///////////////////////////////// @entity
Entity :: struct {
    position: Vector2,
    size: Vector2,
    colour: Colour,
}

///////////////////////////////// @main
main :: proc() {
    { // init state
	state.start_time = time.now()

	state.camera_position = {0, 0}
	state.zoom = 0.5

	create_entity({0, 0}, {2, 1}, Red)
	create_entity({0, 1}, {3, 1}, Green)
	create_entity({-1, -1}, {0.5, 0.5}, Blue)
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

    CAMERA_SPEED :: 0.1

    if state.key_inputs[.A] == .DOWN {
	state.camera_position.x -= CAMERA_SPEED
    }

    if state.key_inputs[.D] == .DOWN {
	state.camera_position.x += CAMERA_SPEED
    }

    if state.key_inputs[.S] == .DOWN {
	state.camera_position.y -= CAMERA_SPEED
    }

    if state.key_inputs[.W] == .DOWN {
	state.camera_position.y += CAMERA_SPEED
    }
}

draw :: proc() {
    for i in 0..<state.entity_count {
	draw_entity(&state.entities[i])
    }
}

create_entity :: proc(position: Vector2, size: Vector2, colour: Colour) {
    state.entities[state.entity_count] = Entity{position = position, size = size, colour = colour}
    state.entity_count += 1
}

draw_entity :: proc(entity: ^Entity) {
    width := sapp.widthf()
    height := sapp.heightf()

    // creating the model view projection matrix
    model_matrix := translate_matrix(entity.position) * scale_matrix(entity.size)
    view_matrix := view_matrix_from_position(state.camera_position) * scale_matrix(state.zoom)
    projection_matrix := linalg.matrix4_perspective_f32(90, width/height, 0, 10)
 
    model_view_projection := projection_matrix * view_matrix * model_matrix

    // get the quad that we are setting the data to
    // REMEMBER: the positions in the quad are in its local space
    // the order that the vertices are draw and that the index buffer
    // is assuming is:
    //	    top left, top right, bottom right, bottom left

    quad := &state.quads[state.quad_count]
    state.quad_count += 1

    // each vert position is * by mvp matrix to convert to screen space, then we just
    // get the xy and why because that is all we care about
    // i dont know why it needs to be vec4 and why the 4th value needs to be 1 *shrug*
    quad[0].position = (model_view_projection * Vector4{-0.5, 0.5, 0, 1}).xyz
    quad[1].position = (model_view_projection * Vector4{0.5, 0.5, 0, 1}).xyz
    quad[2].position = (model_view_projection * Vector4{0.5, -0.5, 0, 1}).xyz
    quad[3].position = (model_view_projection * Vector4{-0.5, -0.5, 0, 1}).xyz

    quad[0].colour = cast(Vector4) entity.colour
    quad[1].colour = cast(Vector4) entity.colour
    quad[2].colour = cast(Vector4) entity.colour
    quad[3].colour = cast(Vector4) entity.colour
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









